#ifndef __UI_MAIN_PAGE_H
#define __UI_MAIN_PAGE_H

#include "app_data.h" // 需要知道天气数据结构
#include <stdint.h>
#include "bsp_rtc.h"

/**
 * @brief  初始化主界面布局 (画背景和色块)
 */
void UI_MainPage_Init(void);

/**
 * @brief  更新主界面数据
 * @param  data: 天气数据指针
 */
void UI_MainPage_Update(const App_Weather_Data_t* data);

/**
 * @brief  更新当前日期
 * @param  current_calendar: 日期结构体
 * @retval None
 */
void APP_UI_UpdateCalendar(BSP_RTC_Calendar_t current_calendar);

#endif