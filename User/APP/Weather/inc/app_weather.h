/**
 * @file    app_weather.h
 * @brief   天气业务服务模块对外接口
 * @author  meng-ming
 * @note    负责管理网络连接、数据获取、协议解析等核心业务逻辑。
 * 通过回调函数机制与 UI 层解耦。
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __APP_WEATHER_H
#define __APP_WEATHER_H

#include "app_data.h" // 引用通用数据结构
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 天气数据更新回调函数类型定义
 * @note  当 Weather Service 成功解析到新天气数据时，会调用此类型的函数通知上层。
 * @param data: 指向最新天气数据的指针。
 */
typedef void (*Weather_DataCallback_t)(const APP_Weather_Data_t* data);

/**
 * @brief 系统状态更新回调函数类型定义
 * @note  当 Weather Service 内部状态发生变化（如开始连接 WiFi、获取失败等）时调用。
 * @param status: 状态描述字符串。
 * @param color:  建议显示的颜色（用于 UI 区分正常/错误状态）。
 */
typedef void (*Weather_StatusCallback_t)(const char* status, uint16_t color);

/**
 * @brief  初始化天气服务模块
 * @note   配置回调函数，初始化底层 ESP 模块，并将状态机复位到初始状态。
 * @param  data_cb:   数据更新时的回调函数指针 (通常传入 App_UI_Update)。
 * @param  status_cb: 状态更新时的回调函数指针 (通常传入 App_UI_ShowStatus)。
 * @retval None
 */
void APP_Weather_Init(Weather_DataCallback_t data_cb, Weather_StatusCallback_t status_cb);

/**
 * @brief  获取最新天气数据 (线程安全/消费者接口)
 * @note   该接口封装了底层 FreeRTOS 队列操作，实现了业务逻辑与 OS 的解耦。
 * @param  out_data: [Out] 指向接收数据的结构体缓冲区 (调用者需分配内存)。
 * @param  wait_ms:  [In]  等待超时时间 (单位: ms)。
 * - 0: 非阻塞模式，若队列为空立即返回 false。
 * - >0: 阻塞等待指定时间。
 * - portMAX_DELAY:一直阻塞直到有数据。
 * * @retval true:  成功获取到最新数据。
 * @retval false: 获取失败 (队列为空或超时)。
 */
bool APP_Weather_GetData(APP_Weather_Data_t* out_data, uint32_t wait_ms);

/**
 * @brief  天气服务主任务循环
 * @note   核心业务逻辑的状态机调度器。
 * ***必须在 main 函数的 while(1) 循环中周期性调用***。
 * 函数内部非阻塞（或仅有微小延时），不会卡死系统。
 * @retval None
 */
void APP_Weather_Task(void);

/**
 * @brief  强制启动更新 (同步时间 + 获取天气)
 * @note   当检测到 RTC 时间丢失 (2000年) 时调用此函数救急
 */
void APP_Weather_Force_Update(void);

#endif