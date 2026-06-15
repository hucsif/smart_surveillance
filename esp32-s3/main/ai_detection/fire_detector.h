#ifndef FIRE_DETECTOR_H
#define FIRE_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 火焰检测结果结构体
 */
typedef struct {
    bool detected;           // 是否检测到火焰
    float confidence;        // 置信度 (0-1)
    float x1;                // 检测框左上角x坐标 (归一化 0-1) - 分类模型可能用不到
    float y1;                // 检测框左上角y坐标 (归一化 0-1)
    float x2;                // 检测框右下角x坐标 (归一化 0-1)
    float y2;                // 检测框右下角y坐标 (归一化 0-1)
    int class_id;            // 类别ID (0:无火, 1:有火)
} fire_detection_result_t;

/**
 * @brief 初始化火焰分类器
 * 
 * @return true 成功, false 失败
 */
bool fire_detector_init(void);

/**
 * @brief 执行火焰分类
 * 
 * @param rgb_data RGB888图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param result 分类结果输出指针
 * @return true 推理成功, false 推理失败
 */
bool fire_detector_run(uint8_t* rgb_data, int width, int height, fire_detection_result_t* result, int temperature);

/**
 * @brief 获取模型期望的输入宽度
 * 
 * @return int 输入宽度 (64)
 */
int fire_detector_get_input_width(void);

/**
 * @brief 获取模型期望的输入高度
 * 
 * @return int 输入高度 (64)
 */
int fire_detector_get_input_height(void);

/**
 * @brief 获取模型输入数据类型
 * 
 * @return const char* "uint8"
 */
const char* fire_detector_get_input_type(void);

/**
 * @brief 释放火焰分类器占用的资源
 */
void fire_detector_deinit(void);

#ifdef __cplusplus
}
#endif

#endif 