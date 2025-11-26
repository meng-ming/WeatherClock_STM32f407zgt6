/**
 * @file    esp32_weather.h
 * @brief   ESP32-C3 AT模块天气数据获取驱动
 * @note    基于环形缓冲 + AT+HTTPCLIENT + cJSON
 */

#ifndef __ESP32_WEATHER_H
#define __ESP32_WEATHER_H

#include "uart_driver.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    char city[32];        // 城市名
    char weather[32];     // 天气现象
    char temp[12];        // 温度（带单位）
    char update_time[16]; // 更新时间 HH:MM
} Weather_Info_t;

/**
 * @brief  初始化ESP32模块（复位 + AT检测 + 设置Station模式）
 * @param  handle: ESP32串口句柄
 * @return true: 成功  false: 失败
 */
bool ESP32_Weather_Init(UART_Handle_t* handle);

/**
 * @brief  连接WiFi
 * @param  handle: ESP32串口句柄
 * @param  ssid:   WiFi名称
 * @param  pwd:    WiFi密码
 * @return true: 成功  false: 失败
 */
bool ESP32_WiFi_Connect(UART_Handle_t* handle, const char* ssid, const char* pwd);

/**
 * @brief  获取天气数据（阻塞式，一次性完成）
 * @param  handle:     ESP32串口句柄
 * @param  url:        完整天气API URL（含key）
 * @param  info:       输出天气信息结构体
 * @param  timeout_ms: 总超时时间（建议15000）
 * @return true: 成功  false: 失败
 */
bool ESP32_Get_Weather(UART_Handle_t*  handle,
                       const char*     url,
                       Weather_Info_t* info,
                       uint32_t        timeout_ms);

/**
 * @brief  打印天气信息（带艺术框框）
 * @param  info: 天气信息结构体
 */
void Weather_Print_Info(const Weather_Info_t* info);

#endif