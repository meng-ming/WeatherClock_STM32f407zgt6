#ifndef __UI_MAIN_PAGE_H
#define __UI_MAIN_PAGE_H

#include "app_data.h" // 需要知道天气数据结构
#include <stdint.h>
#include "bsp_rtc.h"
#include <stdbool.h>

/**
 * @brief  初始化主界面布局 (画背景和色块)
 */
void APP_UI_MainPage_Init(void);

/**
 * @brief  更新主界面数据
 * @param  data: 天气数据指针
 */
void APP_UI_UpdateWeather(const APP_Weather_Data_t* data);

/**
 * @brief  更新当前日期
 * @param  current_calendar: 日期结构体
 * @retval None
 */
void APP_UI_UpdateCalendar(BSP_RTC_Calendar_t current_calendar);

// 单独更新 WiFi 状态和名称
// is_connected: true=显示正常图标, false=显示断开图标
// ssid: WiFi 名称 (如果是 NULL 则不更新文字)
void APP_UI_Update_WiFi(bool is_connected, const char* ssid);

#endif