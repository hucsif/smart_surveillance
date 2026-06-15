#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "my_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include "ws_server.h"
#include "cJSON.h"
#include "esp_http_server.h"

#include "hal/uart_ll.h"
#include "esp_rom_sys.h"
#include "ai_detection/image_processing.h"

#define TAG     "stm32_uart"

#define USER_UART_NUM   UART_NUM_1
#define UART_BUFFER_SIZE 1024

uint8_t uart_buffer[UART_BUFFER_SIZE];
//创建串口事件队列
QueueHandle_t uart_queue;

//用于在任务间共享的传感器数据
static sensor_data_t g_latest_sensor_data = {0};

void my_uart_init(void)
{
    esp_err_t ret;
    //1.配置UART参数
    uart_config_t uart_cfg={
        .baud_rate=115200,                          //波特率115200
        .data_bits=UART_DATA_8_BITS,                //8位数据位
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,        //无硬件流控制
        .parity=UART_PARITY_DISABLE,                //无校验位
        .stop_bits=UART_STOP_BITS_1                 //1位停止位
    };
    ret = uart_param_config(USER_UART_NUM, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART参数配置失败: %d", ret);
        return;
    }

    // 2. 先配置GPIO模式，启用上拉
    gpio_reset_pin(GPIO_NUM_18);  // RX引脚
    gpio_set_pull_mode(GPIO_NUM_18, GPIO_PULLUP_ONLY);  // 启用内部上拉
    
    gpio_reset_pin(GPIO_NUM_17);  // TX引脚
    gpio_set_pull_mode(GPIO_NUM_17, GPIO_PULLUP_ONLY);

    //2.设置引脚
    ret = uart_set_pin(USER_UART_NUM,GPIO_NUM_17,GPIO_NUM_18,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE); //设置引脚,tx为17,rx为18
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART引脚设置失败: %d", ret);
        return;
    }

    //3.安装驱动
    ret = uart_driver_install(USER_UART_NUM,UART_BUFFER_SIZE,UART_BUFFER_SIZE,20,&uart_queue,0);   
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART驱动安装失败: %d", ret);
    } 
    
    ESP_LOGI(TAG, "UART初始化成功！");
}

// 解析STM32数据帧
int parse_stm32_data(const char* data, int len, sensor_data_t* sensor_data)
{
    if (len < 8 || data[0] != 'T') {  // 起始符为'T'
        ESP_LOGE(TAG, "Invalid data header: %c", data[0]);
        return -1; // 无效数据
    }

    // 查找各个字段的分隔符
    const char* t_pos = data;  // T的位置就是数据起始
    const char* h_pos = strchr(data, 'H');
    const char* l_pos = strchr(data, 'L');
    const char* s_pos = strchr(data, 'S');  
    const char* end_pos = strchr(data, '#');

    if (!h_pos || !l_pos || !s_pos || !end_pos) {
        ESP_LOGE(TAG, "Invalid data format - missing separators");
        ESP_LOGE(TAG, "Data: %s", data);
        return -1;
    }

    // 提取温度 (AX)
    char temp_str[16] = {0};
    int temp_len = h_pos - t_pos - 1;  // T和H之间的数据
    if (temp_len > 0 && temp_len < (int)sizeof(temp_str)) {
        strncpy(temp_str, t_pos + 1, temp_len);
        temp_str[temp_len] = '\0';
        sensor_data->temperature = atoi(temp_str);
    } else {
        ESP_LOGE(TAG, "Invalid temperature field");
        return -1;
    }

    // 提取湿度 (AY)
    char humi_str[16] = {0};
    int humi_len = l_pos - h_pos - 1;  // H和L之间的数据
    if (humi_len > 0 && humi_len < (int)sizeof(humi_str)) {
        strncpy(humi_str, h_pos + 1, humi_len);
        humi_str[humi_len] = '\0';
        sensor_data->humidity = atoi(humi_str);
    } else {
        ESP_LOGE(TAG, "Invalid humidity field");
        return -1;
    }

    // 提取光照强度 (AZ)
    char light_str[16] = {0};
    int light_len = s_pos - l_pos - 1;  // L和S之间的数据
    if (light_len > 0 && light_len < (int)sizeof(light_str)) {
        strncpy(light_str, l_pos + 1, light_len);
        light_str[light_len] = '\0';
        sensor_data->light_intensity = atoi(light_str);
    } else {
        ESP_LOGE(TAG, "Invalid light field");
        return -1;
    }

    // 提取蜂鸣器状态 (GX)
    char beep_str[16] = {0};
    int beep_len = end_pos - s_pos - 1;  // S和#之间的数据
    if (beep_len > 0 && beep_len < (int)sizeof(beep_str)) {
        strncpy(beep_str, s_pos + 1, beep_len);
        beep_str[beep_len] = '\0';
        sensor_data->beep_status = atoi(beep_str);
        ESP_LOGI(TAG, "蜂鸣器状态: %d (%s)", sensor_data->beep_status, 
                sensor_data->beep_status == 1 ? "开启" : "关闭");
    } else {
        ESP_LOGE(TAG, "Invalid beep field");
        return -1;
    }

    g_latest_sensor_data.temperature = sensor_data->temperature;
    g_latest_sensor_data.humidity = sensor_data->humidity;
    g_latest_sensor_data.light_intensity = sensor_data->light_intensity;
    g_latest_sensor_data.beep_status = sensor_data->beep_status;

    return 0;
}

// 打印传感器数据
void print_sensor_data(const sensor_data_t* data)
{
    ESP_LOGI(TAG,"=== STM32 Sensor Data ===");
    ESP_LOGI(TAG, "温度(AX): %d", data->temperature);
    ESP_LOGI(TAG, "湿度(AY): %d", data->humidity);
    ESP_LOGI(TAG, "光照(AZ): %d", data->light_intensity);
    ESP_LOGI(TAG, "蜂鸣器状态(GX): %d (%s)", data->beep_status, data->beep_status == 1 ? "开启" : "关闭");
    ESP_LOGI(TAG,"===========================\n");
}

//任务主循环
void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    char received_data[256];
    int data_index = 0;
    ESP_LOGI(TAG, "UART 任务启动，运行在核心 %d", xPortGetCoreID());
    memset(received_data, 0, sizeof(received_data));

    while (1) {
        //1.等待UART事件
        if (xQueueReceive(uart_queue, &event, pdMS_TO_TICKS(100))) {
            //2.处理不同类型的事件
            switch (event.type) {
            case UART_DATA:     //有数据到达
                
                ESP_LOGI(TAG, "接收到的数据大小: %d", event.size);
                
                /*
                    3.读取实际数据:
                    参数说明：USER_UART_NUM -- 要读取的UART端口号
                             uart_buffer -- 接收缓冲区（用于存放从UART硬件读取的原始数据）
                             event.size -- 可读取的数据大小
                             portMAX_DELAY -- 最大等待时间
                    数据流向：UART硬件 --> uart_buffer --> 再处理到received_data
                */
                int len = uart_read_bytes(USER_UART_NUM, uart_buffer, event.size, pdMS_TO_TICKS(100));
                if (len > 0) {
                    // 处理接收到的数据
                    for (int i = 0; i < len; i++) {
                        // 防止缓冲区溢出
                        if (data_index < (int)sizeof(received_data) - 1) {
                            received_data[data_index++] = uart_buffer[i];
                        } else {
                            ESP_LOGW(TAG, "缓冲区已满，重置");
                            data_index = 0;
                            memset(received_data, 0, sizeof(received_data));
                            break;
                        }

                        // 如果收到结束符#或者缓冲区快满了，开始解析
                        if (uart_buffer[i] == '#') {
                            received_data[data_index] = '\0'; // 确保字符串结束
                            
                            ESP_LOGI(TAG, "完整数据帧: %s", received_data);
                            
                            // 解析传感器数据
                            sensor_data_t sensor_data;
                            if (parse_stm32_data(received_data, data_index, &sensor_data) == 0) {  //接收处理好的数据无异常
                                
                                
                                print_sensor_data(&sensor_data);
                                
                                //将采集到的传感器数据放入队列中，让发送任务处理
                                ws_queue_sensor_data(&sensor_data);

                                // 综合判断蜂鸣器：火焰检测到 或 光照>2000lux 或 湿度>70%
                                bool has_fire = get_last_fire_detected();
                                bool should_beep = has_fire || (sensor_data.light_intensity > LIGHT_ALARM_THRESHOLD) || (sensor_data.humidity > HUMIDITY_ALARM_THRESHOLD);

                                static bool last_beep_state = false;
                                if (should_beep != last_beep_state) {
                                    if (should_beep) {
                                        uart_send_beep_on();
                                        ESP_LOGI(TAG, "蜂鸣器开启 - 火焰:%d 光照:%d lux 湿度:%d%%",
                                                 has_fire, sensor_data.light_intensity, sensor_data.humidity);
                                    } else {
                                        uart_send_beep_off();
                                        ESP_LOGI(TAG, "蜂鸣器关闭 - 所有条件已恢复正常");
                                    }
                                    last_beep_state = should_beep;
                                }


                            } else {
                                ESP_LOGW(TAG, "解析数据失败: %s", received_data);
                            }
                            
                            // 重置缓冲区
                            data_index = 0;
                            memset(received_data, 0, sizeof(received_data));
                        }
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(10));
                break;

            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "HW FIFO overflow");
                uart_flush_input(USER_UART_NUM);
                xQueueReset(uart_queue);
                // 重置缓冲区
                data_index = 0;
                memset(received_data, 0, sizeof(received_data));
                break;

            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "Ring buffer full");
                uart_flush_input(USER_UART_NUM);
                xQueueReset(uart_queue);
                // 重置缓冲区
                data_index = 0;
                memset(received_data, 0, sizeof(received_data));
                break;

            case UART_BREAK:
                ESP_LOGI(TAG, "UART RX break");
                break;

            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "UART parity error");
                break;

            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "UART frame error");
                break;

            default:
                ESP_LOGI(TAG, "UART event type: %d", event.type);
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));       //让出CPU
    }
    vTaskDelete(NULL);
}

//-------------------------------STM32蜂鸣器控制部分---------------------------------

/**
 * @brief 发送单字节指令到STM32
 * @param cmd 指令字节
 */
void uart_send_cmd(uint8_t cmd)
{
    uart_write_bytes(USER_UART_NUM, &cmd, 1);
    ESP_LOGI(TAG, "发送指令: 0x%02X", cmd);
}

/**
 * @brief 发送蜂鸣器开启指令（0x01）
 */
void uart_send_beep_on(void)
{
    uart_send_cmd(0x01);
    ESP_LOGI(TAG, "发送蜂鸣器开启指令");
}

/**
 * @brief 发送蜂鸣器关闭指令（0x00）
 */
void uart_send_beep_off(void)
{
    uart_send_cmd(0x00);
    ESP_LOGI(TAG, "发送蜂鸣器关闭指令");
}

// 获取最新温度
int get_latest_temperature(void)
{
    return g_latest_sensor_data.temperature;
}

// 获取最新湿度
int get_latest_humidity(void)
{
    return g_latest_sensor_data.humidity;
}

// 获取最新光照强度
int get_latest_light_intensity(void)
{
    return g_latest_sensor_data.light_intensity;
}



// -------------------------UART中断的启用与禁用---------------------------
void uart_temp_disable_rx_intr(void)
{
    uart_dev_t *hw = UART_LL_GET_HW(USER_UART_NUM);
    uart_ll_disable_intr_mask(hw, UART_INTR_RXFIFO_FULL);
    ESP_LOGI(TAG, "UART接收中断已临时禁用");
}

void uart_temp_enable_rx_intr(void)
{
    uart_dev_t *hw = UART_LL_GET_HW(USER_UART_NUM);
    uart_ll_ena_intr_mask(hw, UART_INTR_RXFIFO_FULL);
    ESP_LOGI(TAG, "UART接收中断已恢复");
}

/*

esp_err_t uart_send_data(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid send data parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    int bytes_written = uart_write_bytes(USER_UART_NUM, (const char*)data, len);
    
    if (bytes_written == len) {
        ESP_LOGI(TAG, "Successfully sent %d bytes", bytes_written);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to send data, expected: %d, actual: %d", len, bytes_written);
        return ESP_FAIL;
    }
}


esp_err_t uart_send_string(const char *str)
{
    if (str == NULL) {
        ESP_LOGE(TAG, "Invalid string parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    return uart_send_data((const uint8_t*)str, strlen(str));
}

*/


