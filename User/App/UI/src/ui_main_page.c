#include "ui_main_page.h"
#include "app_data.h"
#include "app_ui_config.h"
#include "lcd_image.h"
#include "st7789.h"
#include "lcd_font.h"
#include "font_variable.h" // font_16
#include <stdio.h>
#include <string.h>

void UI_MainPage_Init(void)
{
    // 1. 全屏米白色 (形成缝隙)
    TFT_full(UI_BG_COLOR);

    // 2. 绘制 5 个色块区域
    // --- 状态栏 ---
    TFT_Fill_Rect(BOX_STATUS_X, BOX_STATUS_Y, BOX_STATUS_W, BOX_STATUS_H, UI_STATUS_BG);
    LCD_Show_Image(BOX_STATUS_X, BOX_STATUS_Y, 25, 25, gImage_WIFI);
    LCD_Show_String(45, 9, "WIFI:", &font_16, WHITE, UI_STATUS_BG);
    LCD_Show_String(120, 9, "更新时间:01:58", &font_16, WHITE, UI_STATUS_BG);

    // --- 时间 ---
    TFT_Fill_Rect(BOX_TIME_X, BOX_TIME_Y, BOX_TIME_W, BOX_TIME_H, UI_TIME_BG);

    // --- 当前天气 ---
    TFT_Fill_Rect(BOX_ICON_X, BOX_ICON_Y, BOX_ICON_W, BOX_ICON_H, UI_ICON_BG);
    LCD_Show_Image(25, 135, 60, 60, gImage_weather_qingtian);
    LCD_Show_String(37, 200, "12℃", &font_time_20, TFT_RGB(255, 180, 0), UI_ICON_BG);

    // --- 室内模块 ---
    TFT_Fill_Rect(BOX_INDOOR_X, BOX_INDOOR_Y, BOX_INDOOR_W, BOX_INDOOR_H, UI_INDOOR_BG);

    LCD_Show_Image(30, 230, 15, 15, gImage_weather_shinei);
    LCD_Show_String(50, 229, "室内", &font_16, WHITE, UI_INDOOR_BG);
    TFT_Fill_Rect(20, 250, 70, 3, WHITE);

    LCD_Show_Image(20, 260, 20, 20, gImage_weather_shineiwendu);
    LCD_Show_String(55, 262, "26.5", &font_16, UI_TEXT_WHITE, UI_INDOOR_BG);

    LCD_Show_Image(20, 290, 20, 20, gImage_weather_shineishidu);
    LCD_Show_String(55, 292, "45%", &font_16, UI_TEXT_WHITE, UI_INDOOR_BG);

    // --- 天气参数模块 ---
    TFT_Fill_Rect(BOX_LIST_X, BOX_LIST_Y, BOX_LIST_W, BOX_LIST_H, UI_LIST_BG);

    LCD_Show_String(152, 135, "北京", &font_time_20, UI_TEXT_BLACK, UI_LIST_BG);

    LCD_Show_Image(115, 170, 20, 20, gImage_weather_wencha);
    LCD_Show_String(140, 172, "温差", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    LCD_Show_String(177, 172, "-2~-7℃", &font_16, UI_TEXT_BLACK, UI_LIST_BG);

    LCD_Show_Image(115, 200, 20, 20, gImage_weather_fengxiang);
    LCD_Show_String(140, 202, "风向", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    LCD_Show_String(177, 202, "东南偏西北风", &font_16, UI_TEXT_BLACK, UI_LIST_BG);

    LCD_Show_Image(115, 235, 20, 16, gImage_weather_kongqizhiliang);
    LCD_Show_String(140, 234, "空气", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    LCD_Show_String(177, 234, "41", &font_16, UI_TEXT_BLACK, UI_LIST_BG);

    LCD_Show_Image(115, 260, 20, 20, gImage_weather_shidu);
    LCD_Show_String(140, 262, "湿度", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    LCD_Show_String(177, 262, "29%", &font_16, UI_TEXT_BLACK, UI_LIST_BG);

    LCD_Show_Image(115, 290, 20, 20, gImage_weather_qiya);
    LCD_Show_String(140, 292, "气压", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
    LCD_Show_String(177, 292, "1016", &font_16, UI_TEXT_BLACK, UI_LIST_BG);
}

void UI_MainPage_Update(const App_Weather_Data_t* data)
{
    if (!data)
        return;
    char buf[64];

    // === 1. 更新状态栏 (深色背景，白色字) ===
    // 先擦除旧字 (用背景色画矩形覆盖)
    TFT_Fill_Rect(BOX_STATUS_X,
                  BOX_STATUS_Y,
                  BOX_STATUS_X + BOX_STATUS_W - 1,
                  BOX_STATUS_Y + BOX_STATUS_H - 1,
                  UI_STATUS_BG);
    snprintf(buf, sizeof(buf), "Upd: %s", data->update_time);
    LCD_Show_String(BOX_STATUS_X + 5, BOX_STATUS_Y + 4, buf, &font_16, UI_TEXT_WHITE, UI_STATUS_BG);

    // === 2. 更新时间栏 (橙色背景，白色字) ===
    // 显示城市
    LCD_Show_String(
        BOX_TIME_X + 10, BOX_TIME_Y + 10, data->city, &font_16, UI_TEXT_WHITE, UI_TIME_BG);

    // === 3. 更新列表栏 (蓝色背景，白色字) ===
    // 显示天气和温度
    LCD_Show_String(
        BOX_LIST_X + 10, BOX_LIST_Y + 10, data->weather, &font_16, UI_TEXT_WHITE, UI_LIST_BG);
    LCD_Show_String(
        BOX_LIST_X + 10, BOX_LIST_Y + 40, data->temp, &font_16, UI_TEXT_WHITE, UI_LIST_BG);
}

void APP_UI_UpdateCalendar(BSP_RTC_Calendar_t cal)
{
    char time_buf[8];
    char date_buf[32];

    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", cal.hour, cal.min);
    LCD_Show_String(30, 35, time_buf, &font_time_30x60, UI_TEXT_WHITE, UI_TIME_BG);

    snprintf(time_buf, sizeof(time_buf), "%02d", cal.sec);
    LCD_Show_String(182, 68, time_buf, &font_time_20, UI_TEXT_WHITE, UI_TIME_BG);

    snprintf(date_buf,
             sizeof(date_buf),
             "%04d-%02d-%02d %s",
             cal.year,
             cal.month,
             cal.date,
             WEEK_STR[cal.week]);
    LCD_Show_String(35, 95, date_buf, &font_time_20, UI_TEXT_WHITE, UI_TIME_BG);
}