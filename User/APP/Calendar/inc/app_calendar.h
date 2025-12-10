/**
 * @file    app_calendar.h
 * @brief   日历应用任务接口 (实时时间更新与 UI 同步)
 * @note    提供非阻塞日历任务，基于 RTC 轮询实现秒级时间刷新。
 *          仅处理时间变化检测与 UI 回调，上层 UI 负责渲染。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __APP_CALENDAR_H
#define __APP_CALENDAR_H

#include <stdint.h>

/* ==================================================================
 * 1. 函数接口声明 (Function Declarations)
 * ================================================================== */

/**
 * @brief  日历应用任务 (主循环中调用)
 * @note   非阻塞轮询 RTC，每 50ms 检查时间变化（秒/分/日期），变化时回调 UI 更新。
 *         首次运行强制刷新，确保启动即显示当前时间。
 *         依赖 BSP_SysTick_Init() 和 BSP_RTC_Init() 已调用。
 * @retval None
 */
void APP_Calendar_Task(void);

#endif /* __APP_CALENDAR_H */