#ifndef __HARDWARE_I2C_H
#define __HARDWARE_I2C_H

#include "stm32f10x.h"

void I2C2_BH1750_Init(void);
void I2C2_WriteByte(uint8_t devAddr, uint8_t data);
uint16_t I2C2_ReadTwoBytes(uint8_t devAddr);

#endif
