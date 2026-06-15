#ifndef FIRE_MODEL_H
#define FIRE_MODEL_H

#include "esp_attr.h"

#ifdef __cplusplus
extern "C" {
#endif

// 模型数据数组（在 .cc 文件中定义）
extern const unsigned char mobilenetv2_fire_int8_tflite[];
extern const unsigned int mobilenetv2_fire_int8_tflite_len;

#ifdef __cplusplus
}
#endif

#endif // FIRE_MODEL_H