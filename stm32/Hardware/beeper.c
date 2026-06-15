#include "beeper.h"
#include "Delay.h"  // 你的延时函数头文件

static bool beeper_state = false;  // 静态变量，记录蜂鸣器状态

/**
 * @brief 蜂鸣器初始化
 */
void Beeper_Init(void)
{
    // 开启GPIO时钟
    RCC_APB2PeriphClockCmd(BEEP_PORT_CLOCK, ENABLE);
    
    // GPIO初始化结构体
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出模式
    GPIO_InitStructure.GPIO_Pin = BEEP_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_Init(BEEP_PORT, &GPIO_InitStructure);
    
    // 初始状态为关闭
    Beeper_Off();
}

/**
 * @brief 开启蜂鸣器
 */
void Beeper_On(void)
{
    BEEP_ON();
    beeper_state = true;
}

/**
 * @brief 关闭蜂鸣器
 */
void Beeper_Off(void)
{
    BEEP_OFF();
    beeper_state = false;
}

/**
 * @brief 切换蜂鸣器状态（开变关，关变开）
 */
void Beeper_Toggle(void)
{
    if(beeper_state) {
        Beeper_Off();
    } else {
        Beeper_On();
    }
}

/**
 * @brief 设置蜂鸣器状态
 * @param state true=开启，false=关闭
 */
void Beeper_SetState(bool state)
{
    if(state) {
        Beeper_On();
    } else {
        Beeper_Off();
    }
}

/**
 * @brief 获取蜂鸣器状态
 * @return true=开启，false=关闭
 */
bool Beeper_GetState(void)
{
    return beeper_state;
}

/**
 * @brief 蜂鸣器短鸣一次
 * @param ms 鸣叫时长（毫秒）
 */
void Beeper_Once(uint16_t ms)
{
    Beeper_On();
    Delay_ms(ms);
    Beeper_Off();
}

/**
 * @brief 报警模式（你提供的原始模式）
 * 鸣叫100ms -> 停止100ms -> 鸣叫100ms -> 停止700ms
 */
void Beeper_Alert(void)
{
    Beeper_On();
    Delay_ms(100);
    Beeper_Off();
    Delay_ms(100);
    Beeper_On();
    Delay_ms(100);
    Beeper_Off();
    Delay_ms(700);
}

/**
 * @brief 连续报警模式（鸣叫一段时间，停止一段时间，循环）
 * @param on_ms 鸣叫时长（毫秒）
 * @param off_ms 停止时长（毫秒）
 * @param times 循环次数（0表示无限循环）
 * @note 这个函数会阻塞，慎用
 */
void Beeper_AlertLoop(uint16_t on_ms, uint16_t off_ms, uint16_t times)
{
    uint16_t count = 0;
    
    while((times == 0) || (count < times)) {
        Beeper_On();
        Delay_ms(on_ms);
        Beeper_Off();
        Delay_ms(off_ms);
        count++;
    }
}
