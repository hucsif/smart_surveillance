#include "my_dht11.h"
#include "Delay.h"

//DHT11数据缓冲区
uint8_t DHT11_buffer[5];  //包含湿度整数，小数，温度整数，小数以及校验位

//设置GPIO口为输出模式
void DHT11_OUTPUT(void)
{
    //开启对应时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);

    //配置GPIO结构体
    GPIO_InitTypeDef gpio_initstruct;
    gpio_initstruct.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_initstruct.GPIO_Pin = DHT11_PIN;
    gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(DHT11_PORT,&gpio_initstruct);
}

//设置GPIO口为输入模式
void DHT11_INPUT(void)
{
    //开启对应时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);

    //配置GPIO结构体
    GPIO_InitTypeDef gpio_initstruct;
    gpio_initstruct.GPIO_Mode = GPIO_Mode_IPU;
    gpio_initstruct.GPIO_Pin = DHT11_PIN;
    gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(DHT11_PORT,&gpio_initstruct);
}


//主机发送起始信号
void DHT11_start(void) 
{
    DHT11_OUTPUT();
    //拉低电平20ms
    GPIO_ResetBits(DHT11_PORT,DHT11_PIN);
    Delay_ms(20);

    //释放总线30us,确保传感器能检测到高电平
    GPIO_SetBits(DHT11_PORT,DHT11_PIN);
    Delay_us(30);

    //再设置主机GPIO为输入模式
    DHT11_INPUT();

    //等待DHT11响应
    Delay_us(160);
}

//主机开始接收传感器数据
uint8_t DHT11_read(void)
{
    uint8_t i,j;
    
    for(i = 0; i < 5; i++) {
        DHT11_buffer[i] = 0;
        for(j = 0; j < 8; j++) {
            //超时时间
            uint32_t timeout = 1000;
            //等待低电平结束
            while(GPIO_ReadInputDataBit(DHT11_PORT,DHT11_PIN) == 0 && timeout--);
            if(timeout == 0) return 0;
            //高电平到达，延时40us
            Delay_us(40);

            //如果还是高电平--> 1 低电平--> 0
            if(GPIO_ReadInputDataBit(DHT11_PORT,DHT11_PIN) == 1) {
                DHT11_buffer[i] |= (1<<(7-j)); 
            }

            timeout = 1000;
            //等待变回为低电平，即下一位开始发送
            while(GPIO_ReadInputDataBit(DHT11_PORT,DHT11_PIN) == 1 && timeout--);
            if(timeout == 0) return 0;
        }
    }
    return 1;
}

//验证数据校验位
uint8_t DHT11_check(void)
{
    uint8_t sum = 0;
    for(uint8_t i = 0;i < 4;i++) {
        sum += DHT11_buffer[i];
    }
    return (sum == DHT11_buffer[4]);
}

//获取一次完整的数据
void DHT11_GetData(uint8_t* temp,uint8_t* humi)
{
    DHT11_start();
    if(DHT11_read() && DHT11_check()) {
        *temp = DHT11_buffer[2];
        *humi = DHT11_buffer[0];
    } else {
        *temp = 0;
        *humi = 0;
    }
}
