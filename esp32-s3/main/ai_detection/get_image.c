//ESP32-CAM端获取图像
#include "get_image.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "ai_detection/image_processing.h"
#include "ai_detection/fire_detector.h"
#include "iot/ws_server.h"
#include "hal/my_uart.h"

#define TAG  "http_camera"

// 静态缓冲区存储最新帧
static uint8_t *frame_buffer = NULL;
static size_t frame_len = 0;
static uint32_t frame_timestamp = 0;
static SemaphoreHandle_t frame_mutex = NULL;

#define CAMERA_URL "http://192.168.87.56/ai_capture"

void http_camera_init(void)
{
    frame_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "HTTP Camera initialized");
}

bool http_camera_fetch_frame(void)
{
    for (int retry = 0; retry < 3; retry++) {
        esp_http_client_config_t config = {
            .url = CAMERA_URL,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 5000,
            .buffer_size = 2048,
        };
        
        esp_http_client_handle_t client = esp_http_client_init(&config);
        
        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "连接失败 (尝试 %d/3): %s", retry + 1, esp_err_to_name(err));
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        int content_length = esp_http_client_fetch_headers(client);
        if (content_length <= 0) {
            ESP_LOGW(TAG, "无效内容长度: %d (尝试 %d/3)", content_length, retry + 1);
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // 获取分辨率信息（通过获取header的值）
        char *resolution = NULL;
        esp_err_t header_err = esp_http_client_get_header(client, "X-Resolution", &resolution);
        if (header_err == ESP_OK && resolution) {
            ESP_LOGI(TAG, "图像分辨率: %s", resolution);
        } else {
            ESP_LOGI(TAG, "图像大小: %d bytes (无分辨率信息)", content_length);
        }
        
        // 动态分配，不设上限（但添加合理性检查）
        if (content_length > 200 * 1024) { // 如果超过200KB，可能是未压缩的大图
            ESP_LOGW(TAG, "图像过大: %d KB, 可能是未压缩的大图", content_length/1024);
            // 不退出，继续尝试，但记录警告
        }
        
        uint8_t *buf = malloc(content_length);
        if (!buf) {
            ESP_LOGE(TAG, "内存分配失败 %d bytes", content_length);
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        int read_len = esp_http_client_read_response(client, (char*)buf, content_length);
        
        if (read_len != content_length) {
            ESP_LOGW(TAG, "读取不完整: %d/%d bytes (尝试 %d/3)", read_len, content_length, retry + 1);
            free(buf);
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // 验证JPEG头 (0xFF 0xD8)
        if (read_len >= 2 && buf[0] == 0xFF && buf[1] == 0xD8) {
            ESP_LOGI(TAG, "JPEG头验证通过");
        } else {
            ESP_LOGW(TAG, "可能不是有效的JPEG图像: 0x%02X 0x%02X", 
                     read_len > 0 ? buf[0] : 0, 
                     read_len > 1 ? buf[1] : 0);
        }
        
        // 成功获取帧
        xSemaphoreTake(frame_mutex, portMAX_DELAY);
        
        if (frame_buffer) {
            free(frame_buffer);
        }
        
        frame_buffer = buf;
        frame_len = read_len;
        frame_timestamp = esp_log_timestamp();
        
        xSemaphoreGive(frame_mutex);
        
        esp_http_client_cleanup(client);
        ESP_LOGI(TAG, "帧获取成功: %d bytes (尝试 %d)", read_len, retry + 1);
        return true;
    }
    
    ESP_LOGE(TAG, "连接ESP32-CAM失败，已重试3次");
    return false;
}


uint8_t* http_camera_get_frame(size_t *len)
{
    uint8_t *ptr = NULL;
    
    xSemaphoreTake(frame_mutex, portMAX_DELAY);
    if (frame_buffer && frame_len > 0) {
        ptr = frame_buffer;
        *len = frame_len;
        ESP_LOGI(TAG, "获取帧: %d bytes", frame_len);  // 添加日志
    } else {
        ESP_LOGW(TAG, "帧缓冲区为空");
        *len = 0;
    }
    xSemaphoreGive(frame_mutex);
    
    return ptr;
}

uint32_t http_camera_get_timestamp(void)
{
    uint32_t ts = 0;
    
    xSemaphoreTake(frame_mutex, portMAX_DELAY);
    ts = frame_timestamp;
    xSemaphoreGive(frame_mutex);
    
    return ts;
}

// 帧图像获取并执行推理
void camera_fetch_task(void *pvParameters)
{
    // 帧图像获取http客户端初始化
    http_camera_init();
    
    // 初始化火焰检测器
    bool detector_ok = fire_detector_init();
    if (!detector_ok) {
        ESP_LOGW(TAG, "火焰检测器初始化失败，将继续获取图像但不检测");
    }

    // 统计变量
    uint32_t frame_count = 0;
    uint32_t fire_count = 0;
    //避免重复发送告警
    bool last_alert_sent = false;
    
    while (1) {
        // 每两秒检测一次
        if (http_camera_fetch_frame()) {
            frame_count++;
            size_t frame_len;
            uint8_t* frame_data = http_camera_get_frame(&frame_len);
            
            if (frame_data && frame_len > 0 && detector_ok) {
                bool has_fire = process_and_detect_fire(frame_data, frame_len);
                
                if (has_fire) {
                    fire_count++;
                    ESP_LOGI(TAG, "🔥 检测到火焰! (第 %lu 帧, 累计 %lu 次)", frame_count, fire_count);
                    
                    // =======发送WebSocket告警=======
                    if (!last_alert_sent) {
                        ws_send_fire_alert(true,get_last_fire_confidence());
                        last_alert_sent = true;
                    } 
                } else {        //为检测到火焰时，重置标志
                    last_alert_sent = false;
                }

                // 综合判断蜂鸣器：火焰检测到 或 光照>2000lux 或 湿度>70%
                int humidity = get_latest_humidity();
                int light = get_latest_light_intensity();
                bool should_beep = has_fire || (light > LIGHT_ALARM_THRESHOLD) || (humidity > HUMIDITY_ALARM_THRESHOLD);

                if (should_beep) {
                    uart_send_beep_on();
                    ESP_LOGI(TAG, "蜂鸣器开启 - 火焰:%d 光照:%d lux 湿度:%d%%", has_fire, light, humidity);
                } else {
                    uart_send_beep_off();
                    ESP_LOGI(TAG, "蜂鸣器关闭 - 所有条件已恢复正常");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
