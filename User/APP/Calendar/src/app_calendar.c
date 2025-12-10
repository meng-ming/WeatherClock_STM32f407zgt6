/**
 * @file    app_calendar.c
 * @brief   日历应用模块。轮询RTC获取时间，优化秒级UI刷新，避免高频无效更新。
 * @author  meng-ming
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "app_calendar.h"
#include "BSP_Tick_Delay.h"
#include "bsp_rtc.h"
#include "ui_main_page.h"
#include <stdint.h>

// 记录上一次显示的秒数 (初始化为无效值 60，确保第一次一定刷新)
static uint8_t  s_last_sec  = 60;
static uint32_t s_poll_tick = 0; // 用 uint32_t，跟系统滴答匹配

void APP_Calendar_Task(void)
{
    // 策略优化：高频轮询 (每 50ms 检查一次)
    // 目的：一旦 RTC 硬件跳秒，屏幕能立刻跟上，减少视觉延迟
    if (BSP_GetTick_ms() - s_poll_tick > 50)
    {
        s_poll_tick = BSP_GetTick_ms();

        BSP_RTC_Calendar_t now;
        BSP_RTC_GetCalendar(&now);

        // 只有当秒数发生变化时，才刷屏幕
        if (now.sec != s_last_sec)
        {
            s_last_sec = now.sec;
            APP_UI_UpdateCalendar(now);
        }
    }
}