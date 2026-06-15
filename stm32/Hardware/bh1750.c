#include "bh1750.h"
#include "Hardware_I2C.h"
#include "Delay.h"

#define BH1750_ADDR  0x46  // 0x23 << 1 (7位地址左移1位)

void BH1750_Write_Byte(uint8_t cmd)
{
    I2C2_WriteByte(BH1750_ADDR, cmd);
}

uint16_t BH1750_Read_Data(void)
{
    return I2C2_ReadTwoBytes(BH1750_ADDR);
}

void BH1750_Power_ON(void)
{
    BH1750_Write_Byte(BH1750_Power_ON_REG);
}

void BH1750_Power_OFF(void)
{
    BH1750_Write_Byte(BH1750_Power_OFF_REG);
}

void BH1750_Moudle_RESET(void)
{
    BH1750_Write_Byte(BH1750_MODULE_RESET_REG);
}

void BH1750_init(void)
{
    I2C2_BH1750_Init();                     // 初始化硬件I2C2
    Delay_ms(200);
    BH1750_Write_Byte(BH1750_Power_ON_REG); // 上电
    Delay_ms(50);
    BH1750_Write_Byte(BH1750_MODULE_RESET_REG); // 复位
    Delay_ms(50);
    BH1750_Write_Byte(BH1750_CONTINUE_H_MODE2); // 连续高分辨率模式2
    Delay_ms(200);                          // 等待第一次测量完成
}

float Get_Light(void)
{
    return (float)BH1750_Read_Data() / 1.2f;
}
