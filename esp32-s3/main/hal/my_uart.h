#ifndef MY_UART_H
#define MY_UART_H

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "common/sensor_data.h"

#define USER_UART_NUM   UART_NUM_1
#define UART_BUFFER_SIZE 1024

// 只在头文件中声明extern，不定义变量
extern uint8_t uart_buffer[UART_BUFFER_SIZE];
extern QueueHandle_t uart_queue;


// 函数声明
int parse_stm32_data(const char* data, int len, sensor_data_t* sensor_data);
void print_sensor_data(const sensor_data_t* data);
void uart_event_task(void *pvParameters);
void my_uart_init(void);
//--------------蜂鸣器控制部分---------------------
void uart_send_cmd(uint8_t cmd);
void uart_send_beep_on(void);
void uart_send_beep_off(void);
// 传感器数据获取
int get_latest_temperature(void);
int get_latest_humidity(void);
int get_latest_light_intensity(void);
// UART中断禁用
void uart_temp_disable_rx_intr(void);
void uart_temp_enable_rx_intr(void);

// esp_err_t uart_send_data(const uint8_t *data, size_t len);
// esp_err_t uart_send_string(const char *str);

#endif
