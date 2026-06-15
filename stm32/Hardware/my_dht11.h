#ifndef __MY_DHT11_H
#define __MY_DHT11_H
#include "stm32f10x.h"

#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_Pin_6

//验证数据校验位
uint8_t DHT11_check(void);


//获取一次完整的数据
void DHT11_GetData(uint8_t* temp,uint8_t* humi);


#endif

