#ifndef _BH1750_H_
#define _BH1750_H_
#include "stm32f10x.h"

// 器件地址
#define    BH1750_Addr_GND_REG    0x23
#define    BH1750_Addr_VCC_REG    0x5C

// 指令集
#define    BH1750_Power_OFF_REG       0x00
#define    BH1750_Power_ON_REG        0x01
#define    BH1750_MODULE_RESET_REG    0x07

// 工作模式
#define    BH1750_CONTINUE_H_MODE     0x10
#define    BH1750_CONTINUE_H_MODE2    0x11
#define    BH1750_CONTINUE_L_MODE     0x13
#define    BH1750_ONE_TIME_H_MODE     0x20
#define    BH1750_ONE_TIME_H_MODE2    0x21
#define    BH1750_ONE_TIME_L_MODE     0x23

void BH1750_Write_Byte(uint8_t addr);
uint16_t BH1750_Read_Data(void);
void BH1750_Power_ON(void);
void BH1750_Power_OFF(void);
void BH1750_Moudle_RESET(void);
void BH1750_init(void);
float Get_Light(void);

#endif

