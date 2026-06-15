#include <stdio.h>
#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "my_uart.h"
#include "ap_wifi.h"
#include "ws_server.h"
#include "get_image.h"
#include "ai_detection/fire_detector.h"
#include "ai_detection/image_processing.h"


#define TAG  "main"

// SSID和密码
//#define DEFAULT_WIFI_SSID       "ESP32-Camera"
//#define DEFAULT_WIFI_PASSWORD   "12345678"

#define DEFAULT_WIFI_SSID       "OPPO Reno7 5G"
#define DEFAULT_WIFI_PASSWORD   "g86h3i9e"

//事件组
static EventGroupHandle_t wifi_ev = NULL;
//事件标志位
#define WIFI_CONNECT_BIT    BIT0

// wifi回调函数
void wifi_state_handle(WIFI_STATE state) 
{
    if (state == WIFI_STATE_CONNECTED) 
    {
        ESP_LOGI(TAG,"wifi connect success");
        xEventGroupSetBits(wifi_ev, WIFI_CONNECT_BIT);
    }
    if (state == WIFI_STATE_DISCONNECTED) 
    {
        ESP_LOGI(TAG,"wifi disconnect");
    }   
}

//程序主入口
void app_main(void)
{
    nvs_flash_init();

    //创建事件组
    wifi_ev = xEventGroupCreate();

    //初始化相关外设
    my_uart_init();

    //ap配网模式的核心初始化函数
    ap_wifi_init(wifi_state_handle);

    //wifi事件位
    EventBits_t ev;

    
    //快速连接wifi
    //wifi_manager_init(wifi_state_handle);
    //wifi_manager_connect(DEFAULT_WIFI_SSID,DEFAULT_WIFI_PASSWORD);

    // 查看当前可用堆内存
    printf("Free heap: %u bytes\n", (unsigned int)esp_get_free_heap_size());
    // 查看开发板型号
    printf("Chip model: %s\n", CONFIG_IDF_TARGET);

    //主循环
    while(1)
    {
        //等待wifi连接成功事件
        ev = xEventGroupWaitBits(wifi_ev,WIFI_CONNECT_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(5 * 1000));
        if (ev & WIFI_CONNECT_BIT) 
        {
            vTaskDelay(pdMS_TO_TICKS(500));    //等待0.5s
            //创建传感器数据队列
            sensor_queue = xQueueCreate(5, sizeof(sensor_data_t));
            
            //启动 WebSocket 发送任务（高优先级）
            xTaskCreatePinnedToCore(ws_send_task, "ws_send", 4096, NULL, 5, NULL, 0);
            
            //启动 UART 任务（中优先级）
            xTaskCreatePinnedToCore(uart_event_task, "uart", 4096, NULL, 4, NULL, 1);
            
            //启动 web 服务器
            start_webserver();

            //启动 帧图像获取 任务
            xTaskCreatePinnedToCore(camera_fetch_task, "camera_fetch", 4096, NULL, 5, NULL, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
