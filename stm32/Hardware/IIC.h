#ifndef _IIC_H_
#define _IIC_H_
#include "stm32f10x.h"

#define BH1750_PORT     GPIOB
#define BH1750_SDA_PIN  GPIO_Pin_11
#define BH1750_SCL_PIN  GPIO_Pin_10

#define IIC_SDA(x)  GPIO_WriteBit(BH1750_PORT,BH1750_SDA_PIN,(BitAction)(x))
#define IIC_SCL(x)  GPIO_WriteBit(BH1750_PORT,BH1750_SCL_PIN,(BitAction)(x))
#define Read_SDA    GPIO_ReadInputDataBit(BH1750_PORT,BH1750_SDA_PIN);

void IIC_SDA_IN(void);
void IIC_SDA_OUT(void);
//初始化IIC引脚配置
void IIC_init(void);
//起始信号（ST）
void IIC_start(void);
//终止信号（SP）
void IIC_stop(void);
//等待应答信号
uint8_t IIC_Wait_Ack(void);
//发送应答信号给从机
void IIC_Ack(void);
void IIC_NAck(void);
//发送一个字节
void IIC_Send_Byte(uint8_t Txd);
//接收一个字节
uint8_t IIC_Read_Byte(uint8_t Ack);


#endif
