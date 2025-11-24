#include "stm32f4xx.h"
#include "st7789.h"
#include "BSP_Cortex_M4_Delay.h"
#include "FreeRTOS.h"
#include "task.h"

// =======================================================
// 任务句柄与堆栈定义
// =======================================================
TaskHandle_t xGUITaskHandle = NULL;

// =======================================================
// GUI 任务函数
// =======================================================
void GUI_Task(void* pvParameters)
{
    // 任务内的初始化（如果需要在任务启动后才做）
    // 这里我们假设 ST7789_Init 已经在 main 中调用过了，或者你也可以放在这里

    // 1. 初始刷屏
    TFT_full(BLACK);
    vTaskDelay(pdMS_TO_TICKS(500)); // 使用 OS 延时，释放 CPU 权

    // 2. 画静态 UI 框架
    // 比如画一个标题栏
    TFT_Fill_Rect(0, 0, 239, 30, BLUE);

    // 3. 任务主循环
    while (1)
    {
        // 模拟显示一些动态内容

        // 区域 1：闪烁方块 (心跳)
        TFT_Fill_Rect(10, 50, 60, 100, RED);

        // 区域 2：天依蓝
        TFT_Fill_Rect(80, 50, 130, 100, TFT_RGB(102, 204, 255));

        // 延时 500ms (此时 OS 会切换去运行空闲任务或其他任务)
        vTaskDelay(pdMS_TO_TICKS(500));

        // 切换颜色
        TFT_Fill_Rect(10, 50, 60, 100, GREEN);
        TFT_Fill_Rect(80, 50, 130, 100, YELLOW);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void GUI_Task1(void* pvParameters)
{
    TFT_full(BLACK);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 1. 顶部标题 (8x16 小字)
    TFT_Fill_Rect(0, 0, 240, 30, BLUE);

    while (1)
    {
        // 2. 显示大号时间 (16x32 大字)
        // 模拟时间变化：12:34:56
        // 背景设为黑色，这样刷新时会自动覆盖旧字，不用清屏
        // TFT_ShowChar_1632(50, 80,'A', GREEN, BLACK);
        TFT_ShowString_1632(10, 80, "YeXiangMeng", GREEN, BLUE);

        // 3. 显示大号温度
        TFT_ShowString_1632(50, 120, "Temp: 25C", YELLOW, BLACK);

        // 心跳动画
        TFT_Fill_Rect(220, 5, 230, 15, RED);
        vTaskDelay(pdMS_TO_TICKS(500));
        TFT_Fill_Rect(220, 5, 230, 15, BLUE);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// =======================================================
// 主函数
// =======================================================
int main(void)
{
    // 1. 基础硬件初始化 (时钟、中断分组等)
    // SystemInit() 已经在启动文件中被调用

    // 2. 初始化 DWT 延时 (用于驱动内部的微秒级延时)
    BSP_Cortex_M4_Delay_Init();

    // 3. 初始化屏幕驱动 (包含 GPIO 和 SPI 初始化)
    // 建议在调度器启动前完成硬件初始化，避免多任务竞争硬件资源
    ST7789_Init();

    // 简单测试一下屏幕是否活着，防止任务没跑起来一脸懵
    TFT_clear();

    // 4. 创建任务
    xTaskCreate(GUI_Task1,        // 任务函数
                "GUI_Task1",      // 任务名字
                1024,             // 堆栈大小 (字数，不是字节) -> 4KB
                NULL,             // 传递参数
                2,                // 优先级 (1-configMAX_PRIORITIES)
                &xGUITaskHandle); // 句柄

    // 5. 启动调度器 (从此开始，main 函数结束，OS 接管)
    vTaskStartScheduler();

    // 正常情况下不会执行到这里
    while (1)
    {
    }
}