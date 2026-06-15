#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"
#include "Serial.h"
#include "my_dht11.h"
#include "bh1750.h"
#include "beeper.h"
#include "IIC.h"

// 电机状态（默认关）
int beep_status = 0;

// 温湿度
uint8_t T,H;

// 光照强度
int L = 0;

// 串口发送标志
int sendflag = 0;

void send_data(void)
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
	beep_status = Beeper_GetState() ? 1 : 0;	//获取蜂鸣器状态
	Serial_Printf("T%dH%dL%dS%d#",T,H,L,beep_status);
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

int main(void)
{
	// 各设备初始化
	OLED_Init();
	Serial_Init();		//串口
	Timer_Init();		//定时器
	BH1750_init();
	Beeper_Init();
	
	//Beeper_Alert();
	
	OLED_ShowString(1, 1, "T:");  // 温度标签
    OLED_ShowString(2, 1, "H:");  // 湿度标签
    OLED_ShowString(3, 1, "L:");  // 光照标签
	OLED_ShowString(4, 1, "B:");  // 蜂鸣器状态标签
	
	while (1)
	{
		// 采集传感器数据
		DHT11_GetData(&T,&H);
		L = (int)(Get_Light() + 0.5f);
		// 显示数据
		OLED_ShowNum(1, 3, T, 2);
		OLED_ShowNum(2, 3, H, 2);
		OLED_ShowNum(3, 3, L, 5);
		
		// 蜂鸣器状态显示
		if (Beeper_GetState()) {
            OLED_ShowString(4, 3, "ON ");   // 开启状态
        } else {
            OLED_ShowString(4, 3, "OFF");   // 关闭状态
        }
		
		//处理串口指令以及LED闪烁
		Process_UART_Commands();
		Process_LED_Blink();
		
		if (sendflag) 
		{
			sendflag = 0;
			send_data();
		}
		Delay_ms(500);
	}
}


void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		sendflag = 1;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}

