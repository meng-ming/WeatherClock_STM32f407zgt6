#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "st7789.h"
#include "FreeRTOS.h"
#include "task.h"

// =======================================================
// 任务函数 - LCD红蓝交替闪烁
// =======================================================
void LCD_Task(void* pvParameters)
{
    (void) pvParameters; // 避免 unused parameter 警告

    // 屏幕命令初始化 (放在任务里确保所有硬件初始化完成后再执行)
    ST7789_Init();

    while (1)
    {
        // 任务1：显示红色
        TFT_full(RED);
        // FreeRTOS 延时：500ms
        vTaskDelay(pdMS_TO_TICKS(500));

        // 任务2：显示蓝色
        TFT_full(BLUE);
        // FreeRTOS 延时：500ms
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    // 1. 配置中断优先级分组为 4 (FreeRTOS 要求)
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    // 2. 硬件初始化 (背光、SPI) - 放在这里只初始化一次
    ST7789_Hardware_Init();

    // 3. 创建任务
    xTaskCreate(LCD_Task,             // 任务函数
                "LCD",                // 任务名称
                150,                  // 任务堆栈大小 (单位为 word，即 4字节)
                NULL,                 // 任务参数
                tskIDLE_PRIORITY + 1, // 任务优先级
                NULL);                // 任务句柄

    // 4. 启动调度器
    vTaskStartScheduler();

    // 永远不应该执行到这里
    while (1)
    {
        // TFT_full(RED);

        // TFT_full(GREEN);
    };
}