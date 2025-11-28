/**
 * @file    main.c
 * @brief   天气时钟项目主入口 (架构重构版)
 * @note    负责系统初始化和任务调度，具体业务逻辑下沉到 App 层。
 */

#include "BSP_Tick_Delay.h"
#include "bsp_rtc.h"
#include "uart_handle_variable.h"
#include "uart_driver.h"

// === 引入应用层模块 ===
#include "app_ui.h"
#include "app_weather.h"
#include "sys_log.h"
#include "ui_main_page.h"
#include <stdint.h>
#include <stdio.h>

/**
 * @brief  主程序入口
 */
int main(void)
{
    static uint64_t last_ms_tick_update = 0; // 用于更新时间，不会阻塞 天气任务

    // ====================================================
    // 1. BSP 层初始化 (硬件基础)
    // ====================================================
    // 设置中断优先级分组 (FreeRTOS 或 高级应用必备) NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // 初始化滴答定时器 (用于 BSP_Delay_ms)
    BSP_SysTick_Init();

    // 初始化调试串口 (USART1 -> 电脑)
    // 注意：ESP32 的串口 (USART2) 不需要在这里显式初始化
    // 因为它属于 Weather 模块的私有资源，应该由 App_Weather_Init 内部去调驱动初始化
    UART_Init(&g_debug_uart_handler);

    LOG_I("=============================================");
    LOG_I("    STM32 Weather Clock (Architecture V2.0)  ");
    LOG_I("    Build Date: %s %s", __DATE__, __TIME__);
    LOG_I("=============================================");

    BSP_RTC_Status_e rtc_status = BSP_RTC_Init();
    if (rtc_status != BSP_RTC_OK && rtc_status != BSP_RTC_ALREADY_INIT)
    {
        LOG_E("[Main] RTC Init failed!");
    }

    // ====================================================
    // 2. App 层初始化 (应用业务)
    // ====================================================

    // A. 初始化 UI 系统
    // 这一步会让屏幕亮起，绘制出背景框、标题栏和静态标签
    LOG_I("[Main] Init UI System...");

    App_UI_Init();
    // 此时屏幕应该显示框架，底部可能有默认状态或空

    // B. 初始化天气服务系统
    // 关键点：【依赖注入】
    // 我们告诉 Weather 模块："你有新数据了就调 App_UI_Update，有状态了就调 App_UI_ShowStatus"
    // 这样 Weather 模块不需要知道 UI 模块的具体实现，只要符合函数签名即可
    LOG_I("[Main] Init Weather Service...");
    // App_Weather_Init(App_UI_Update, App_UI_ShowStatus);

    // ====================================================
    // 3. 超级循环 (Super Loop) / 任务调度
    // ====================================================
    LOG_I("[Main] System Running...");

    BSP_RTC_Calendar_t current_calendar;

    // 3. 调用 时间更新函数

    while (1)
    {
        // 调度天气服务任务

        App_Weather_Task();
        if (BSP_GetTick_ms() - last_ms_tick_update > 1000)
        {
            last_ms_tick_update = BSP_GetTick_ms();
            BSP_RTC_GetCalendar(&current_calendar);
            APP_UI_UpdateCalendar(current_calendar);
        }

        // 可以在这里添加其他非阻塞任务，例如：
        // Key_Scan();      // 按键扫描
        // LED_Blink();     // 运行指示灯

        // 稍微让出 CPU，降低功耗 (在裸机系统中非必须，但推荐)
        // 如果将来上 FreeRTOS，这里就是 vTaskDelay
        BSP_Delay_ms(5);
    }
}