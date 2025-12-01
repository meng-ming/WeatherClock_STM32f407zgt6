/**
 * @file    app_weather.h
 * @brief   天气业务服务模块对外接口
 * @note    负责管理网络连接、数据获取、协议解析等核心业务逻辑。
 * 通过回调函数机制与 UI 层解耦。
 */

#ifndef __APP_WEATHER_H
#define __APP_WEATHER_H

#include "app_data.h" // 引用通用数据结构
#include <stdint.h>

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
 * @brief  天气服务主任务循环
 * @note   核心业务逻辑的状态机调度器。
 * ***必须在 main 函数的 while(1) 循环中周期性调用***。
 * 函数内部非阻塞（或仅有微小延时），不会卡死系统。
 * @retval None
 */
void APP_Weather_Task(void);

#endif