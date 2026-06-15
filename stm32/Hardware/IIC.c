#include "IIC.h"
#include "Delay.h"

/*

    软件模拟IIC
    只有当SCL被拉低时，SDA的电平才可改变
    当SCL被拉高时，读取SDA的电平

*/

void IIC_SDA_IN(void)
{
	//设置PB7为SDA,首先清空PB7配置
    GPIOB->CRL &= 0x0FFFFFFF;
    //设置为上拉输入模式
    GPIOB->CRL |= (u32)8 << 28;  //8->1000 低二位00表示为输入模式 高二位10表示为上拉/下拉输入
}

void IIC_SDA_OUT(void)
{
    GPIOB->CRL &= 0x0FFFFFFF;
    //设置为推挽输出模式
    GPIOB->CRL |= (u32)3 << 28;  //3->0011 低二位11表示为输出模式，输出最大速度为50MHz，高二位00表示为推挽输出
}

//初始化IIC引脚配置
void IIC_init(void)
{
    //开启对应引脚外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
    
    GPIO_InitTypeDef gpio_initstruct;
    gpio_initstruct.GPIO_Pin = BH1750_SCL_PIN | BH1750_SDA_PIN;
    gpio_initstruct.GPIO_Mode = GPIO_Mode_Out_OD;   //开漏输出
    gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(BH1750_PORT,&gpio_initstruct);

    //初始时输出高电平
    GPIO_SetBits(BH1750_PORT,BH1750_SCL_PIN|BH1750_SDA_PIN);
}

//起始信号（ST）
void IIC_start(void)
{
    //设置SDA为输出模式
    IIC_SDA_OUT();
    
    IIC_SDA(1);
    IIC_SCL(1);
    Delay_us(5);

    IIC_SDA(0);
    Delay_us(5);

    IIC_SCL(0);     //拉低SCL，等待发送或接收
}

//终止信号（SP）
void IIC_stop(void)
{
    IIC_SDA_OUT();
    IIC_SCL(0);
    IIC_SDA(0);
    Delay_us(5);

    IIC_SCL(1);
    IIC_SDA(1);
    Delay_us(5);
}

//等待应答信号
uint8_t IIC_Wait_Ack(void)
{
    uint16_t Errortime = 0;
    
    IIC_SDA_IN();
    IIC_SDA(1);     //释放SDA
    Delay_us(5);
    IIC_SCL(1);
    Delay_us(5);
    
    //读取SDA电平，查看从机是否响应
    uint8_t bit = Read_SDA;
    while(bit) {
        Errortime++;

        if(Errortime > 250) {
            IIC_stop();
            return 0;
        }
    }
    IIC_SCL(0);     //拉低SCL，等待发送或响应
    return 1;
}

//发送应答信号给从机
void IIC_Ack(void)
{
    IIC_SDA_OUT();

    IIC_SCL(0);
    IIC_SDA(0);
    Delay_us(5);

    IIC_SCL(1);
    Delay_us(5);

    IIC_SCL(0);     //拉低SCL，等待发送或响应
}

void IIC_NAck(void)
{
    IIC_SDA_OUT();

    IIC_SCL(0);
    IIC_SDA(1);
    Delay_us(5);

    IIC_SCL(1);
    Delay_us(5);

    IIC_SCL(0);     //拉低SCL，等待发送或响应
}

//发送一个字节
void IIC_Send_Byte(uint8_t Txd)
{
    uint8_t T;

    IIC_SDA_OUT();
    IIC_SCL(0);

    for(T = 0; T < 8;T++) {
        IIC_SDA((Txd & 0x80) >> 7);
        Delay_us(5);
        IIC_SCL(1);
        Delay_us(5);

        IIC_SCL(0);
        Delay_us(5);
    }
}

//接收一个字节
uint8_t IIC_Read_Byte(uint8_t Ack)
{
    uint8_t T;
    uint8_t receive;

    IIC_SDA_IN();

    for(T = 0; T < 8;T++) {
        IIC_SCL(0);
        Delay_us(5);       //延时等待从机发送数据
        IIC_SCL(1);
        receive <<= 1;
        uint8_t bit = Read_SDA;
        if(bit) {
            receive |= 1;
        }
        Delay_us(5);
    }

    //接收完一个字节后发送响应
    if(!Ack) {
        IIC_NAck();
    } else {
        IIC_Ack();
    }

    return receive;
}

