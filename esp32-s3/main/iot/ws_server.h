#ifndef _WS_SERVER_H_
#define _WS_SERVER_H_
#include "esp_err.h"
#include "esp_http_server.h"
#include "freertos/queue.h"
#include "sensor_data.h"

//ws接收到的处理回调函数
typedef void(*ws_receive_cb)(uint8_t* payload,int len);


#define SENSOR_QUEUE_SIZE 10
// 声明队列句柄
extern QueueHandle_t sensor_queue;


/** 启动ws
 * @param cfg ws一些配置,请看ws_cfg_t定义
 * @return  ESP_OK or ESP_FAIL
*/
esp_err_t   start_webserver(void);

esp_err_t   web_ws_stop(void);

esp_err_t   web_ws_send(uint8_t* data, int len);

void ws_queue_sensor_data(sensor_data_t *data);

void ws_send_task(void *pvParameters);

#ifdef __cplusplus
extern "C" {
#endif

// 发送火焰告警
void ws_send_fire_alert(bool detected, float confidence);

#ifdef __cplusplus
}
#endif

#endif
