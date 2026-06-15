#include "image_processing.h"
#include "fire_detector.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_dec.h"
#include "iot/ws_server.h"
#include <cstring>
extern "C" {
    #include "hal/my_uart.h"
}

#define TAG "IMAGE_PROC"

// 火焰检测结果
static bool last_fire_detected = false;
// 检测结果置信度
static float last_fire_confidence = 0.0f;

// 目标输出尺寸（模型输入尺寸）
#define MODEL_WIDTH 224
#define MODEL_HEIGHT 224

static uint8_t* rgb_buffer = NULL;
static int last_width = 0;      
static int last_height = 0;  

#ifdef __cplusplus
extern "C" {
#endif

bool process_and_detect_fire(uint8_t* jpeg_data, size_t jpeg_len) {
    ESP_LOGI(TAG, "🔥 process_and_detect_fire 被调用，JPEG大小: %d", jpeg_len);
    
    if (!jpeg_data || jpeg_len == 0) {
        ESP_LOGE(TAG, "无效的JPEG数据");
        return false;
    }

    // 1. 分配最终RGB缓冲区（使用 jpeg_calloc_align 确保16字节对齐）
    if (!rgb_buffer) {
        rgb_buffer = (uint8_t*)jpeg_calloc_align(MODEL_WIDTH * MODEL_HEIGHT * 3, 16);
        if (!rgb_buffer) {
            ESP_LOGE(TAG, "RGB缓冲区分配失败");
            return false;
        }
        ESP_LOGI(TAG, "RGB缓冲区分配成功: %d KB", (MODEL_WIDTH*MODEL_HEIGHT*3)/1024);
    }

    // 2. 配置 JPEG 解码器 - 直接缩放到 224x224
    jpeg_dec_config_t dec_config = {
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
        .scale = {
            .width = MODEL_WIDTH,
            .height = MODEL_HEIGHT
        },
        .clipper = {.width = 0, .height = 0},
        .rotate = JPEG_ROTATE_0D,
        .block_enable = false
    };

    jpeg_dec_handle_t dec_handle = NULL;
    jpeg_error_t ret = jpeg_dec_open(&dec_config, &dec_handle);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG解码器打开失败: %d", ret);
        return false;
    }

    // 3. 解析 JPEG 头部获取原始尺寸
    jpeg_dec_io_t io = {
        .inbuf = jpeg_data,
        .inbuf_len = (int)jpeg_len,
        .inbuf_remain = 0,
        .outbuf = NULL,
        .out_size = 0
    };

    jpeg_dec_header_info_t header_info;
    ret = jpeg_dec_parse_header(dec_handle, &io, &header_info);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG头部解析失败: %d", ret);
        jpeg_dec_close(dec_handle);
        return false;
    }

    ESP_LOGI(TAG, "原始JPEG图像: %dx%d, 硬件缩放到: %dx%d", 
             header_info.width, header_info.height, MODEL_WIDTH, MODEL_HEIGHT);
    last_width = header_info.width;
    last_height = header_info.height;

    // 4. 解码并直接缩放到目标尺寸
    io.outbuf = rgb_buffer;
    ret = jpeg_dec_process(dec_handle, &io);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG解码/缩放失败: %d", ret);
        jpeg_dec_close(dec_handle);
        return false;
    }

    jpeg_dec_close(dec_handle);
    ESP_LOGI(TAG, "解码+缩放成功: %dx%d", MODEL_WIDTH, MODEL_HEIGHT);
    
    //获取最新温度值(动态调整置信阈值)
    int current_temperature = get_latest_temperature();

    // 5. 运行火焰检测
    fire_detection_result_t result;
    if (fire_detector_run(rgb_buffer, MODEL_WIDTH, MODEL_HEIGHT, &result, current_temperature)) {    //成功运行推理
        
        // 保存检测结果
        last_fire_detected = result.detected;
        last_fire_confidence = result.confidence;

        // =============发送火焰检测结果到WebSocket==============
        ws_send_fire_alert(result.detected,result.confidence);
        // ====================================================
        
        if (result.detected) {
            ESP_LOGI(TAG, "🔥 检测到火焰! 置信度: %.2f", result.confidence);
            return true;
        } else {
            ESP_LOGI(TAG, "❌ 未检测到火焰 (无火概率: %.2f, 有火概率: %.2f)", 
                     1.0f - result.confidence, result.confidence);
        }
    }

    return false;
}

int get_last_image_width(void) {
    return last_width;
}

int get_last_image_height(void) {
    return last_height;
}

bool get_last_fire_detected(void) {
    return last_fire_detected;
}

float get_last_fire_confidence(void) {
    return last_fire_confidence;
}

#ifdef __cplusplus
}
#endif