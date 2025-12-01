#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

// ==============================
// WiFi 配置
// ==============================
#define WIFI_SSID "7041"
#define WIFI_PWD "auto7041"

// ==============================
// 天气 API 配置 (易客云 v1 免费版)
// ==============================
#define WEATHER_HOST "v1.yiketianqi.com"
#define WEATHER_PORT 80
// #define WEATHER_APPID "91768283"
// #define WEATHER_APPSECRET "b68BdGrM"

#define WEATHER_APPID "42919643"
#define WEATHER_APPSECRET "aIg1oiyO"

#define CITY_NAME "南京"

// ==============================
// 统一的天气数据结构体
// ==============================
typedef struct
{
    char city[16];        // 城市 (南京)
    char weather[32];     // 天气 (晴)
    char temp[16];        // 当前温度 (9.7)
    char update_time[16]; // 更新时间 (20:17)

    char temp_range[32]; // 温差 (6~17℃)
    char wind[32];       // 风向风力 (东南风 2级)
    char air[8];         // 空气质量 (95)
    char humidity[8];    // 湿度 (44%)
    char pressure[8];    // 气压 (1013)
} APP_Weather_Data_t;

// 日期对应的星期
extern const char* WEEK_STR[];

#endif