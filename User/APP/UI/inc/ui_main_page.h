/**
 * @file    ui_main_page.h
 * @brief   主界面 (Main Page) 的应用层接口
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 * @note    此接口将底层 UI 驱动和高层应用逻辑解耦，只负责主界面元素的初始化和数据刷新。
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __UI_MAIN_PAGE_H
#define __UI_MAIN_PAGE_H

#include "app_data.h" // 需要知道天气数据结构
#include "bsp_rtc.h"  // 需要知道 RTC 日期结构体
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  初始化主界面布局
 * @note   负责在系统启动时绘制静态背景、色块和UI容器。
 */
void APP_UI_MainPage_Init(void);

/**
 * @brief  更新主界面天气数据
 * @param  data: 待刷新的天气数据结构体指针 (e.g., 温度、天气现象、温差)
 */
void APP_UI_UpdateWeather(const APP_Weather_Data_t* data);

/**
 * @brief  更新当前日期信息
 * @param  current_calendar: 日期结构体 (通常来自 RTC 读取)
 */
void APP_UI_UpdateCalendar(BSP_RTC_Calendar_t current_calendar);

/**
 * @brief  单独更新 WiFi 连接状态及 SSID 显示
 * @param  is_connected: 连接状态 (true=显示正常图标, false=显示断开图标)
 * @param  ssid:         WiFi 名称字符串 (如果为 NULL，则不刷新名称文字)
 */
void APP_UI_Update_WiFi(bool is_connected, const char* ssid);

/**
 * @brief  更新室内温湿度 UI 显示
 * @note   此函数为线程安全 (Thread-Safe)，内部集成了递归互斥锁。
 * 支持特殊错误码显示：
 * - 若 temp <= -900.0，屏幕将显示红色 "--.-" 报警，提示传感器故障。
 * - 正常范围内，显示白色数值 (温度保留1位小数，湿度取整)。
 * @param  temp: 温度值 (单位: ℃)
 * @param  humi: 湿度值 (单位: %)
 * @retval None
 */
void APP_UI_UpdateSensor(float temp, float humi);

#endif /* __UI_MAIN_PAGE_H */