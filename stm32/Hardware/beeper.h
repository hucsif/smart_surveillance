#ifndef __BEEPER_H
#define __BEEPER_H

#include "stm32f10x.h"  // 根据你的STM32型号可能需要调整
#include <stdbool.h>

// 蜂鸣器引脚定义
#define BEEP_PORT       GPIOB
#define BEEP_PIN        GPIO_Pin_12
#define BEEP_PORT_CLOCK RCC_APB2Periph_GPIOB

// 蜂鸣器控制宏（根据你的电路：低电平鸣叫，高电平停止）
#define BEEP_ON()   GPIO_ResetBits(BEEP_PORT, BEEP_PIN)   // 低电平，蜂鸣器鸣叫
#define BEEP_OFF()  GPIO_SetBits(BEEP_PORT, BEEP_PIN)     // 高电平，蜂鸣器停止

// 函数声明
void Beeper_Init(void);           // 初始化蜂鸣器
void Beeper_On(void);             // 开启蜂鸣器
void Beeper_Off(void);            // 关闭蜂鸣器
void Beeper_Toggle(void);         // 切换蜂鸣器状态
void Beeper_SetState(bool state); // 设置蜂鸣器状态（true=开，false=关）
bool Beeper_GetState(void);       // 获取蜂鸣器状态
void Beeper_Once(uint16_t ms);    // 蜂鸣器短鸣一次（可指定时长）
void Beeper_Alert(void);          // 报警模式（鸣叫100ms，停止100ms，鸣叫100ms，停止700ms）

#endif /* __BEEPER_H */
