#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include <stdint.h>

// ==============================
// WiFi 配置
// ==============================
#define WIFI_SSID "7041"
#define WIFI_PWD "auto7041"

// ==============================
// 天气 API 配置 (心知天气)
// ==============================
#define WEATHER_HOST "api.seniverse.com"
#define WEATHER_PORT 80
#define API_KEY "SyLymtirx8EY1K_Dz" // 你的私钥
#define CITY_LOC "beijing"
#define WEATHER_LANG "en" // en, zh-Hans

// ==============================
// 系统参数
// ==============================
#define WEATHER_UPDATE_INTERVAL_MS (60 * 1000) // 天气更新周期
#define NET_CONNECT_TIMEOUT_MS 15000           // 网络连接超时
#define UART_BAUDRATE_DEBUG 115200
#define UART_BAUDRATE_ESP32 115200

// ==============================
// 统一的天气数据结构体
// ==============================
typedef struct
{
    char city[32];        // 城市 (如: Beijing)
    char weather[32];     // 天气现象 (如: Cloudy)
    char temp[16];        // 温度 (如: 6 C)
    char update_time[16]; // 更新时间 (如: 12:50)
} App_Weather_Data_t;

#endif