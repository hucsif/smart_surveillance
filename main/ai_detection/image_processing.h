#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 处理JPEG图像并运行火焰检测
 * 
 * @param jpeg_data JPEG图像数据
 * @param jpeg_len JPEG数据长度
 * @return true 检测到火焰
 * @return false 未检测到火焰或处理失败
 */
bool process_and_detect_fire(uint8_t* jpeg_data, size_t jpeg_len);

/**
 * @brief 获取最近处理的图像宽度
 * 
 * @return int 图像宽度
 */
int get_last_image_width(void);

/**
 * @brief 获取最近处理的图像高度
 * 
 * @return int 图像高度
 */
int get_last_image_height(void);

// 获取火焰检测结果
bool get_last_fire_detected(void);

// 获取结果置信度
float get_last_fire_confidence(void);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_PROCESSING_H