/**
 * @file    ui_main_page.c
 * @brief   UI主页面模块。初始化布局、更新天气/日历/传感器/WiFi状态，使用互斥锁保护LCD刷新。
 * @author  meng-ming
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "ui_main_page.h"
#include "app_data.h"
#include "app_ui.h"
#include "app_ui_config.h"
#include "st7789.h"
#include "font_variable.h"
#include <stdio.h>
#include <string.h>
#include "font.h"
#include "sys_log.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"

// 引用外部互斥锁
extern SemaphoreHandle_t g_mutex_lcd;

// 表示传感器故障
#define SENSOR_ERROR_VAL -999.0f

// 定义数值显示的 X 坐标 (对齐位置)
#define WEATHER_LIST_VAL_X 177
#define WEATHER_LIST_TITLE_X 140
#define ITEM_W (BOX_LIST_X + BOX_LIST_W - WEATHER_LIST_VAL_X) // 预留给天气列表信息数值的宽度

// 可能会造成显示残留现象，使用智能打印

// 1. 温差 (Y=172)
static UI_Label_t s_list_range = {.x           = WEATHER_LIST_VAL_X,
                                  .y           = 172,
                                  .limit_width = ITEM_W,
                                  .font        = &font_16,
                                  .fg_color    = UI_TEXT_BLACK,
                                  .bg_color    = UI_LIST_BG,
                                  .last_pos    = {0}};

// 2. 风向 (Y=202)
static UI_Label_t s_list_wind = {.x           = WEATHER_LIST_VAL_X,
                                 .y           = 202,
                                 .limit_width = ITEM_W,
                                 .font        = &font_16,
                                 .fg_color    = UI_TEXT_BLACK,
                                 .bg_color    = UI_LIST_BG,
                                 .last_pos    = {0}};

// 3. 空气 (Y=234)
static UI_Label_t s_list_air = {.x           = WEATHER_LIST_VAL_X,
                                .y           = 234,
                                .limit_width = ITEM_W,
                                .font        = &font_16,
                                .fg_color    = UI_TEXT_BLACK,
                                .bg_color    = UI_LIST_BG,
                                .last_pos    = {0}};

// 4. 湿度 (Y=262)
static UI_Label_t s_list_humi = {.x           = WEATHER_LIST_VAL_X,
                                 .y           = 262,
                                 .limit_width = ITEM_W,
                                 .font        = &font_16,
                                 .fg_color    = UI_TEXT_BLACK,
                                 .bg_color    = UI_LIST_BG,
                                 .last_pos    = {0}};

// 5. 气压 (Y=292)
static UI_Label_t s_list_pres = {.x           = WEATHER_LIST_VAL_X,
                                 .y           = 292,
                                 .limit_width = ITEM_W,
                                 .font        = &font_16,
                                 .fg_color    = UI_TEXT_BLACK,
                                 .bg_color    = UI_LIST_BG,
                                 .last_pos    = {0}};
// 6. 当前温度
static UI_Label_t s_current_temp = {.x           = 25,
                                    .y           = 200,
                                    .limit_width = BOX_ICON_X + BOX_ICON_W - 25,
                                    .font        = &font_time_20,
                                    .fg_color    = TFT_RGB(255, 180, 0),
                                    .bg_color    = UI_ICON_BG,
                                    .last_pos    = {0}};

// 7. 当前城市
static UI_Label_t s_current_city = {.x           = 152,
                                    .y           = 135,
                                    .limit_width = BOX_LIST_X + BOX_LIST_W - 152,
                                    .font        = &font_time_20,
                                    .fg_color    = UI_TEXT_BLACK,
                                    .bg_color    = UI_LIST_BG,
                                    .last_pos    = {0}};

// 优先级表：顺序很重要！先匹配特殊/长词，后匹配通用/短词
// 比如 "雨夹雪" 必须在 "雨" 之前，否则会被 "雨" 截胡
static const Weather_Map_t s_weather_map[] = {{"雨夹雪", gImage_weather_yujiaxue}, // 优先级高
                                              {"雷阵雨", gImage_weather_leizhenyu},
                                              {"暴雨", gImage_weather_dayu},
                                              {"大雨", gImage_weather_dayu},
                                              {"中雨", gImage_weather_zhongyu},
                                              {"小雨", gImage_weather_xiaoyu}, // 通用匹配
                                              {"雪", gImage_weather_xiaxue},
                                              {"雾", gImage_weather_wumai},
                                              {"霾", gImage_weather_wumai},
                                              {"沙", gImage_weather_wumai},
                                              {"尘", gImage_weather_wumai},
                                              {"阴", gImage_weather_yintian},
                                              {"多云", gImage_weather_duoyun},
                                              {"风", gImage_weather_youfeng},
                                              {"晴", gImage_weather_qingtian},

                                              // 哨兵：表尾 (虽然用不到，但在动态遍历时是个好习惯)
                                              {NULL, NULL}};

#define WEATHER_MAP_SIZE (sizeof(s_weather_map) / sizeof(s_weather_map[0]))

// === 内部查找函数 ===
static const unsigned char* Get_Weather_Icon(const char* weather_str)
{
    // 遍历查表
    for (int i = 0; i < WEATHER_MAP_SIZE - 1; i++) // -1 是减去最后的 NULL 哨兵
    {
        // 只要 weather_str 里包含关键字，就命中
        if (strstr(weather_str, s_weather_map[i].keyword) != NULL)
        {
            return s_weather_map[i].img_ptr;
        }
    }
    // 兜底：如果啥都没匹配到，默认给个晴天
    return gImage_weather_qingtian;
}

void APP_UI_MainPage_Init(void)
{
    // 1. 全屏米白色 (形成缝隙)
    TFT_Full_DMA(UI_BG_COLOR);

    // 2. 绘制 5 个色块区域
    // --- 状态栏 ---
    TFT_Fill_Rect_DMA(BOX_STATUS_X, BOX_STATUS_Y, BOX_STATUS_W, BOX_STATUS_H, UI_STATUS_BG);

    // --- 时间 ---
    TFT_Fill_Rect_DMA(BOX_TIME_X, BOX_TIME_Y, BOX_TIME_W, BOX_TIME_H, UI_TIME_BG);

    // --- 当前天气 ---
    TFT_Fill_Rect_DMA(BOX_ICON_X, BOX_ICON_Y, BOX_ICON_W, BOX_ICON_H, UI_ICON_BG);

    // --- 室内模块 ---
    TFT_Fill_Rect_DMA(BOX_INDOOR_X, BOX_INDOOR_Y, BOX_INDOOR_W, BOX_INDOOR_H, UI_INDOOR_BG);

    TFT_ShowImage_DMA(30, 230, 15, 15, gImage_weather_shinei);
    TFT_Show_String(
        50, 229, BOX_INDOOR_X + BOX_INDOOR_W - 50, "室内", &font_16, WHITE, UI_INDOOR_BG);
    TFT_Fill_Rect_DMA(20, 250, 70, 3, WHITE);

    TFT_ShowImage_DMA(20, 260, 20, 20, gImage_weather_shineiwendu);

    TFT_ShowImage_DMA(20, 290, 20, 20, gImage_weather_shineishidu);

    // --- 天气参数模块 ---
    TFT_Fill_Rect_DMA(BOX_LIST_X, BOX_LIST_Y, BOX_LIST_W, BOX_LIST_H, UI_LIST_BG);

    TFT_ShowImage_DMA(115, 170, 20, 20, gImage_weather_wencha);
    TFT_Show_String(WEATHER_LIST_TITLE_X, 172, 0, "温差", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    TFT_ShowImage_DMA(115, 200, 20, 20, gImage_weather_fengxiang);
    TFT_Show_String(WEATHER_LIST_TITLE_X, 202, 0, "风向", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    TFT_ShowImage_DMA(115, 235, 20, 16, gImage_weather_kongqizhiliang);
    TFT_Show_String(WEATHER_LIST_TITLE_X, 234, 0, "空气", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    TFT_ShowImage_DMA(115, 260, 20, 20, gImage_weather_shidu);
    TFT_Show_String(WEATHER_LIST_TITLE_X, 262, 0, "湿度", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    TFT_ShowImage_DMA(115, 290, 20, 20, gImage_weather_qiya);
    TFT_Show_String(WEATHER_LIST_TITLE_X, 292, 0, "气压", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
}

void APP_UI_UpdateWeather(const APP_Weather_Data_t* data)
{
    if (!data)
        return;
    char buf[64];

    if (xSemaphoreTakeRecursive(g_mutex_lcd, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // === 1. 更新状态栏  ===
        snprintf(buf, sizeof(buf), "更新时间 %s", data->update_time);
        TFT_Show_String(
            120, 9, BOX_STATUS_X + BOX_STATUS_W - 120, buf, &font_16, UI_TEXT_WHITE, UI_STATUS_BG);

        // === 2.. 更新当前天气栏  ===
        const unsigned char* p_weather_img = Get_Weather_Icon(data->weather);
        TFT_ShowImage_DMA(25, 135, 60, 60, p_weather_img);
        UI_Print_Label(&s_current_temp, data->temp);

        // === 3. 更新列表栏 ===
        // 显示当前城市
        UI_Print_Label(&s_current_city, data->city);

        // 显示各种天气参数
        UI_Print_Label(&s_list_range, data->temp_range);
        UI_Print_Label(&s_list_wind, data->wind);
        UI_Print_Label(&s_list_air, data->air);
        UI_Print_Label(&s_list_humi, data->humidity);
        UI_Print_Label(&s_list_pres, data->pressure);
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }
    else
    {
        LOG_W("[UI] Weather Update Locked!");
    }
}

void APP_UI_UpdateCalendar(BSP_RTC_Calendar_t cal)
{
    char time_buf[8];
    char date_buf[32];

    if (xSemaphoreTakeRecursive(g_mutex_lcd, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        // 显示 时分
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", cal.hour, cal.min);
        TFT_Show_String(30, 35, 0, time_buf, &font_time_30x60, UI_TEXT_WHITE, UI_TIME_BG);

        // 显示 秒
        snprintf(time_buf, sizeof(time_buf), "%02d", cal.sec);
        TFT_Show_String(182, 68, 0, time_buf, &font_time_20, UI_TEXT_WHITE, UI_TIME_BG);

        snprintf(date_buf,
                 sizeof(date_buf),
                 "%04d-%02d-%02d %s",
                 cal.year,
                 cal.month,
                 cal.date,
                 WEEK_STR[cal.week]);
        TFT_Show_String(35, 95, 0, date_buf, &font_time_20, UI_TEXT_WHITE, UI_TIME_BG);

        xSemaphoreGiveRecursive(g_mutex_lcd);
    }
}

void APP_UI_Update_WiFi(bool is_connected, const char* ssid)
{
    if (xSemaphoreTakeRecursive(g_mutex_lcd, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        // 1. 更新图标
        if (is_connected)
        {
            TFT_ShowImage_DMA(BOX_STATUS_X, BOX_STATUS_Y, 25, 25, gImage_WIFI);
        }
        else
        {
            TFT_ShowImage_DMA(BOX_STATUS_X, BOX_STATUS_Y, 25, 25, gImage_WIFI_Disconnected);
        }
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }
}

void APP_UI_UpdateSensor(float temp, float humi)
{
    // 1. 定义缓冲区
    static char buf[32];

    // 2. 申请屏幕锁
    if (xSemaphoreTake(g_mutex_lcd, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        // === 检查是否为错误状态 ===
        if (temp <= -900.0f) // 只要小于 -900 都算错误
        {
            // 显示 "--" 提示用户设备故障
            TFT_Show_String(47,
                            262,
                            BOX_INDOOR_X + BOX_INDOOR_W - 47,
                            "--.- C",
                            &font_16,
                            RED,
                            UI_INDOOR_BG); // 用红色醒目提示
            TFT_Show_String(
                57, 292, BOX_INDOOR_X + BOX_INDOOR_W - 57, "-- %", &font_16, RED, UI_INDOOR_BG);
        }
        else
        {
            // === 显示温度 ===
            snprintf(buf, sizeof(buf), "%.1f℃", temp);
            TFT_Show_String(47,
                            262,
                            BOX_INDOOR_X + BOX_INDOOR_W - 47,
                            buf,
                            &font_16,
                            UI_TEXT_WHITE,
                            UI_INDOOR_BG);

            // === 显示湿度 ===
            snprintf(buf, sizeof(buf), "%.1f%%", humi);
            TFT_Show_String(50,
                            292,
                            BOX_INDOOR_X + BOX_INDOOR_W - 57,
                            buf,
                            &font_16,
                            UI_TEXT_WHITE,
                            UI_INDOOR_BG);
        }
        // 3. 释放锁
        xSemaphoreGive(g_mutex_lcd);
    }
}