#ifndef _GAT_IMAGE_H_
#define _GAT_IMAGE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // 定义 size_t

// 初始化摄像头HTTP客户端
void http_camera_init(void);

// 从ESP32-CAM获取最新帧并保存到内存
bool http_camera_fetch_frame(void);

// 获取当前帧数据指针和大小
uint8_t* http_camera_get_frame(size_t *len);

// 获取帧的时间戳
uint32_t http_camera_get_timestamp(void);

// 帧图像获取任务
void camera_fetch_task(void *pvParameters);


#endif
