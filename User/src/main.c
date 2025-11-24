#include "stm32f4xx.h"
#include "st7789.h"
#include "BSP_Cortex_M4_Delay.h"

int main(void)
{
    // 1. 硬件初始化
    ST7789_Init();

    // 2. 刷全屏黑色
    TFT_full(BLACK);
    BSP_Cortex_M4_Delay_ms(500);

    // 3. 画一个红色的矩形 (10,10) 到 (100,100)
    TFT_Fill_Rect(10, 10, 100, 100, RED);

    // 4. 画一个自定义 RGB 颜色的矩形 (10, 120) 到 (100, 200)
    // 天依蓝 RGB(102, 204, 255)
    TFT_Fill_Rect(10, 120, 100, 200, TFT_RGB(102, 204, 255));

    // 5. 动画演示
    while (1)
    {
        TFT_Fill_Rect(0, 0, 10, 10, WHITE);
        BSP_Cortex_M4_Delay_ms(500);
        TFT_Fill_Rect(0, 0, 10, 10, BLACK);
        BSP_Cortex_M4_Delay_ms(500);
    }
}