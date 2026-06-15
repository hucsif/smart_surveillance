#include "Hardware_I2C.h"

//=============================================================================
// I2C2 - 用于 BH1750 (PB10=SCL, PB11=SDA)
//=============================================================================

void I2C2_BH1750_Init(void)
{
    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    
    // GPIO配置 (PB10=SCL, PB11=SDA)
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;      // 复用开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // I2C配置
    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;           // 100kHz
    I2C_Init(I2C2, &I2C_InitStructure);
    
    I2C_Cmd(I2C2, ENABLE);
}

void I2C2_WriteByte(uint8_t devAddr, uint8_t data)
{
    // 产生起始条件
    I2C_GenerateSTART(I2C2, ENABLE);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));
    
    // 发送设备地址（写）
    I2C_Send7bitAddress(I2C2, devAddr, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    
    // 发送数据
    I2C_SendData(I2C2, data);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    
    // 产生停止条件
    I2C_GenerateSTOP(I2C2, ENABLE);
}

uint16_t I2C2_ReadTwoBytes(uint8_t devAddr)
{
    uint8_t buf[2];
    uint8_t i;
    
    // 产生起始条件
    I2C_GenerateSTART(I2C2, ENABLE);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));
    
    // 发送设备地址（读）
    I2C_Send7bitAddress(I2C2, devAddr, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));
    
    // 读取两个字节
    for(i = 0; i < 2; i++)
    {
        // 如果是最后一个字节，发送NACK
        if(i == 1)
        {
            I2C_AcknowledgeConfig(I2C2, DISABLE);
        }
        else
        {
            I2C_AcknowledgeConfig(I2C2, ENABLE);
        }
        
        // 等待数据接收完成
        while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED));
        
        // 读取数据
        buf[i] = I2C_ReceiveData(I2C2);
    }
    
    // 产生停止条件
    I2C_GenerateSTOP(I2C2, ENABLE);
    
    return ((uint16_t)buf[0] << 8) | buf[1];
}
