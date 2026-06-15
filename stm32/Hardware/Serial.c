#include "stm32f10x.h"                  // Device header
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "beeper.h"
#include "Delay.h"

// 接收缓冲区（用于单字节指令）
static volatile uint8_t rx_cmd = 0;
static volatile uint8_t rx_cmd_ready = 0;

// LED闪烁标志
static volatile uint8_t led_blink_flag = 0;

void Serial_Init(void)
{
    // 1. 使能 USART1 和 GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. 配置 TX (PA9) 为复用推挽输出
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 RX (PA10) 为上拉输入（接收数据）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置 USART1 参数（全双工模式）
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;           // 与 ESP32 一致
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;        // 全双工

    USART_Init(USART1, &USART_InitStructure);
	
	// 5. 开启接收中断
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 6. 配置NVIC中断优先级
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    // 7. 使能 USART1
    USART_Cmd(USART1, ENABLE);
}

// USART1 中断服务函数
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        uint8_t data = USART_ReceiveData(USART1);
        
		// 收到数据时，设置LED闪烁标志（主循环中处理）
        led_blink_flag = 1;
        
        // 单字节指令解析
        if (data == 0x01) {
            rx_cmd = 1;      // 蜂鸣器开启指令
            rx_cmd_ready = 1;
        } else if (data == 0x00) {
            rx_cmd = 0;      // 蜂鸣器关闭指令
            rx_cmd_ready = 1;
        }
        
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

// 处理LED闪烁（在主循环中调用）
void Process_LED_Blink(void)
{
    static uint32_t last_blink_time = 0;
    static uint8_t led_state = 0;
    
    if (led_blink_flag) {
        led_blink_flag = 0;
        // LED快速闪烁一下（约50ms）
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        last_blink_time = 0;  // 记录开始时间
        led_state = 1;
    }
    
    if (led_state) {
        last_blink_time++;
        if (last_blink_time >= 50) {  // 约50ms后关闭
            GPIO_SetBits(GPIOC, GPIO_Pin_13);
            led_state = 0;
        }
    }
}

// 处理串口指令（在主循环中调用）
void Process_UART_Commands(void)
{
    if (rx_cmd_ready) {
        rx_cmd_ready = 0;
        if (rx_cmd == 1) {
            Beeper_On();
        } else {
            Beeper_Off();
        }
    }
}


// 保持不变....
void Serial_SendByte(uint8_t Byte)
{
	USART_SendData(USART1, Byte);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

void Serial_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i ++)
	{
		Serial_SendByte(Array[i]);
	}
}

void Serial_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)
	{
		Serial_SendByte(String[i]);
	}
}

uint32_t Serial_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y --)
	{
		Result *= X;
	}
	return Result;
}

void Serial_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)
	{
		Serial_SendByte(Number / Serial_Pow(10, Length - i - 1) % 10 + '0');
	}
}

int fputc(int ch, FILE *f)
{
	Serial_SendByte(ch);
	return ch;
}

void Serial_Printf(char *format, ...)
{
	char String[100];
	va_list arg;
	va_start(arg, format);
	vsprintf(String, format, arg);
	va_end(arg);
	Serial_SendString(String);
}
