#include "ui_main_page.h"
#include "app_data.h"
#include "app_ui_config.h"
#include "st7789.h"
#include "font_variable.h"
#include <stdio.h>
#include <string.h>

// 辅助宏：局部刷新一行文字 (防止重叠)
#define SHOW_LIST_ITEM(y, title, string)                                                           \
    do                                                                                             \
    {                                                                                              \
        TFT_Show_String(140, y, title, &font_16, UI_TEXT_BLACK, UI_LIST_BG);                       \
        TFT_Show_String(177, y, string, &font_16, UI_TEXT_BLACK, UI_LIST_BG);                      \
    } while (0)

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
    TFT_Show_String(50, 229, "室内", &font_16, WHITE, UI_INDOOR_BG);
    TFT_Fill_Rect_DMA(20, 250, 70, 3, WHITE);

    TFT_ShowImage_DMA(20, 260, 20, 20, gImage_weather_shineiwendu);

    TFT_ShowImage_DMA(20, 290, 20, 20, gImage_weather_shineishidu);

    // --- 天气参数模块 ---
    TFT_Fill_Rect_DMA(BOX_LIST_X, BOX_LIST_Y, BOX_LIST_W, BOX_LIST_H, UI_LIST_BG);

    TFT_ShowImage_DMA(115, 170, 20, 20, gImage_weather_wencha);

    TFT_ShowImage_DMA(115, 200, 20, 20, gImage_weather_fengxiang);

    TFT_ShowImage_DMA(115, 235, 20, 16, gImage_weather_kongqizhiliang);

    TFT_ShowImage_DMA(115, 260, 20, 20, gImage_weather_shidu);

    TFT_ShowImage_DMA(115, 290, 20, 20, gImage_weather_qiya);
}

void APP_UI_UpdateWeather(const APP_Weather_Data_t* data)
{
    if (!data)
        return;
    char buf[64];

    // === 1. 更新状态栏  ===
    snprintf(buf, sizeof(buf), "更新时间 %s", data->update_time);
    TFT_Show_String(120, 9, buf, &font_16, UI_TEXT_WHITE, UI_STATUS_BG);

    // === 2.. 更新当前天气栏  ===
    const unsigned char* p_weather_img = Get_Weather_Icon(data->weather);
    TFT_ShowImage_DMA(25, 135, 60, 60, p_weather_img);
    TFT_Show_String(25, 200, data->temp, &font_time_20, TFT_RGB(255, 180, 0), UI_ICON_BG);

    // === 3. 更新列表栏 ===
    // 显示当前城市
    TFT_Show_String(152, 135, data->city, &font_time_20, UI_TEXT_BLACK, UI_LIST_BG);

    // 显示各种天气参数
    SHOW_LIST_ITEM(172, "温差", data->temp_range);

    SHOW_LIST_ITEM(202, "风向", data->wind);

    SHOW_LIST_ITEM(234, "空气", data->air);

    SHOW_LIST_ITEM(262, "湿度", data->humidity);

    SHOW_LIST_ITEM(292, "气压", data->pressure);

    // === 3. 更新室内模块 ===
    TFT_Show_String(55, 262, "26.5", &font_16, UI_TEXT_WHITE, UI_INDOOR_BG);
    TFT_Show_String(55, 292, "45%", &font_16, UI_TEXT_WHITE, UI_INDOOR_BG);
}

void APP_UI_UpdateCalendar(BSP_RTC_Calendar_t cal)
{
    char time_buf[8];
    char date_buf[32];

    // 显示 时分
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", cal.hour, cal.min);
    TFT_Show_String(30, 35, time_buf, &font_time_30x60, UI_TEXT_WHITE, UI_TIME_BG);

    // 显示 秒
    snprintf(time_buf, sizeof(time_buf), "%02d", cal.sec);
    TFT_Show_String(182, 68, time_buf, &font_time_20, UI_TEXT_WHITE, UI_TIME_BG);

    snprintf(date_buf,
             sizeof(date_buf),
             "%04d-%02d-%02d %s",
             cal.year,
             cal.month,
             cal.date,
             WEEK_STR[cal.week]);
    TFT_Show_String(35, 95, date_buf, &font_time_20, UI_TEXT_WHITE, UI_TIME_BG);
}

void APP_UI_Update_WiFi(bool is_connected, const char* ssid)
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
}