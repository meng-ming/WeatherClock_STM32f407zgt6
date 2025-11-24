#include "led.h"
static void Delay(volatile uint32_t count)
{
    while (count--)
        ;
}

int main(void)
{
    LED_Init(); // 初始化 LED
    while (1)
    {
        LED_ON();
        Delay(500000);
        LED_OFF();
        Delay(500000);
    }
}