#include "led.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 使能 GPIOF 时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    // 配置 PF9 和 PF10 为输出（野火/正点原子都支持）
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOF, &GPIO_InitStructure);
}

void LED_ON(void)
{
    GPIO_SetBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);
}

void LED_OFF(void)
{
    GPIO_ResetBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);
}

void LED_Toggle(void)
{
    GPIO_ToggleBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);
}