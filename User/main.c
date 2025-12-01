#include "BSP_Tick_Delay.h"
#include "bsp_rtc.h"
#include "uart_handle_variable.h"
#include "uart_driver.h"
#include "sys_log.h"

// === 引入应用层模块 ===
#include "app_ui.h"
#include "app_weather.h"
#include "app_calendar.h" 
#include "ui_main_page.h"

int main(void)
{
    // 1. BSP 初始化
    BSP_SysTick_Init();
    UART_Init(&g_debug_uart_handler);

    BSP_RTC_Status_e rtc_status = BSP_RTC_Init();
    if (rtc_status != BSP_RTC_OK && rtc_status != BSP_RTC_ALREADY_INIT)
    {
        LOG_E("[Main] RTC Error: %d", rtc_status);
    }

    LOG_I("System Start...");

    // 2. APP 初始化
    APP_UI_Init();

    // 初始化天气 (传入UI回调)
    APP_Weather_Init(APP_UI_UpdateWeather, APP_UI_ShowStatus); // 注意这里名字要对上

    // 3. 超级循环
    while (1)
    {
        // 任务 A: 天气子系统 (负责联网、状态机、数据解析)
        APP_Weather_Task();

        // 任务 B: 日历子系统
        APP_Calendar_Task();

        // 稍微让出 CPU
        BSP_Delay_ms(5);
    }
}