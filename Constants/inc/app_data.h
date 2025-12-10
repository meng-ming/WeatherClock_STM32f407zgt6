/**
 * @file    app_data.h
 * @brief   全局应用数据定义与配置
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 * @note    此文件包含 WiFi 凭证、天气 API 密钥及核心数据结构定义。
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

/* ==================================================================
 * 网络配置 (Network Configuration)
 * ================================================================== */
#define WIFI_SSID "7041"    ///< 目标 WiFi 名称
#define WIFI_PWD "auto7041" ///< 目标 WiFi 密码

/* ==================================================================
 * 天气服务配置 (Weather Service Configuration)
 * ================================================================== */
/** @name Yiketianqi API Config */
#define WEATHER_HOST "v1.yiketianqi.com" ///< 天气 API 域名
#define WEATHER_PORT 80                  ///< HTTP 端口
#define WEATHER_APPID "91768283"         ///< 易客云应用 ID
#define WEATHER_APPSECRET "b68BdGrM"     ///< 易客云应用密钥

/* ==================================================================
 * 地区配置 (Location Configuration)
 * ================================================================== */
#define CITY_NAME "南京" ///< 城市名称 (用于接收天气信息)

/* ==================================================================
 * 数据结构定义 (Data Structures)
 * ================================================================== */

/**
 * @brief  统一天气数据结构体
 * @note   用于在 Weather Task 和 UI Task 之间传递解析后的天气信息。
 * 字符串长度已包含 '\0' 结束符，请确保解析时不要溢出。
 */
typedef struct
{
    // --- 基础信息 ---
    char city[16];        ///< 城市名称 (e.g., "南京")
    char weather[32];     ///< 天气现象 (e.g., "晴", "雷阵雨")
    char temp[16];        ///< 当前温度 (e.g., "19.5")
    char update_time[16]; ///< 数据更新时间 (e.g., "16:22")

    // --- 扩展信息 ---
    char temp_range[32]; ///< 今日温差范围 (e.g., "6~17℃")
    char wind[32];       ///< 风向风力描述 (e.g., "东南风 2级")
    char air[8];         ///< 空气质量指数 AQI (e.g., "95")
    char humidity[8];    ///< 相对湿度 (e.g., "44%")
    char pressure[8];    ///< 大气压强 (e.g., "1013")
} APP_Weather_Data_t;

/* ==================================================================
 * 外部变量声明 (Extern Variables)
 * ================================================================== */

/**
 * @brief 星期字符串映射表
 * @note  索引 0 对应非法/未知，1~7 对应 周一~周日 (符合 RTC 格式)
 */
extern const char* WEEK_STR[];

#endif /* __APP_DATA_H */