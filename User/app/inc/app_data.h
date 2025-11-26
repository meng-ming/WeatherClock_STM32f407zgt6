/**
 * @file    app_data.h
 * @brief   应用层通用数据结构定义
 * @note    这是 UI 层和 业务层 通用的“普通话”，不依赖任何硬件驱动
 */

#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

// 统一的天气数据结构体
typedef struct
{
    char city[32];        // 城市 (如: Beijing)
    char weather[32];     // 天气现象 (如: Cloudy)
    char temp[16];        // 温度 (如: 6 C)
    char update_time[16]; // 更新时间 (如: 12:50)
} App_Weather_Data_t;

#endif