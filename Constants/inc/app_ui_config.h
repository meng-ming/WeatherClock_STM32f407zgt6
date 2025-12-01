/**
 * @file    APP_ui_conf.h
 * @brief   UI 界面布局与配色配置文件
 * @note    所有界面的坐标、尺寸、颜色定义均在此处管理，方便统一调整。
 */

#ifndef __APP_UI_CONF_H
#define __APP_UI_CONF_H

#include <stdint.h>
#include <sys/_types.h>

// ==============================================================================
// 1. 全局布局参数 (Global Layout)
// ==============================================================================
#define UI_SCREEN_W 240
#define UI_SCREEN_H 320
#define UI_GAP 5 // 模块间的缝隙宽度

// ==============================================================================
// 2. 模块坐标定义 (V17 Pixel-Perfect Map)
// ==============================================================================

// --- 模块 1: 顶部状态栏 (Status Bar) ---
#define BOX_STATUS_X 5
#define BOX_STATUS_Y 5
#define BOX_STATUS_W 230
#define BOX_STATUS_H 25

// --- 模块 2: 时间栏 (Time Box) ---
#define BOX_TIME_X 5
#define BOX_TIME_Y 35
#define BOX_TIME_W 230
#define BOX_TIME_H 90

// --- 模块 3: 天气图标 (Icon Box - Top Left) ---
#define BOX_ICON_X 5
#define BOX_ICON_Y 130
#define BOX_ICON_W 100
#define BOX_ICON_H 90

// --- 模块 4: 室内数据 (Indoor Box - Bottom Left) ---
#define BOX_INDOOR_X 5
#define BOX_INDOOR_Y 225
#define BOX_INDOOR_W 100
#define BOX_INDOOR_H 90

// --- 模块 5: 详细列表 (Details List - Right) ---
#define BOX_LIST_X 110
#define BOX_LIST_Y 130
#define BOX_LIST_W 125
#define BOX_LIST_H 185

// ==============================================================================
// 3. 配色方案 (Color Scheme)
// ==============================================================================

// --- 主题色 ---
#define UI_BG_COLOR TFT_RGB(253, 245, 230) // 米白色 (#fdf5e6) -> 缝隙填充色

#define UI_STATUS_BG TFT_RGB(44, 61, 79)
#define UI_TIME_BG TFT_RGB(229, 142, 38)
#define UI_ICON_BG TFT_RGB(255, 241, 118)
#define UI_INDOOR_BG TFT_RGB(162, 155, 254)
#define UI_LIST_BG TFT_RGB(64, 224, 208)

// --- 字体色 ---
#define UI_TEXT_BLACK BLACK                   // 浅色背景用的深色字
#define UI_TEXT_WHITE WHITE                   // 深色背景用的白字
#define UI_TEXT_CURRENT_TEM TFT(189, 41, 144) // 当前温度

// --- 使用的图片取模 ---

// --- 启动界面 ---
extern const unsigned char gImage_Startup_Screen[];

// --- 天气图标 ---
extern const unsigned char gImage_weather_xiaoyu[];
extern const unsigned char gImage_weather_zhongyu[];
extern const unsigned char gImage_weather_dayu[];
extern const unsigned char gImage_weather_duoyun[];
extern const unsigned char gImage_weather_leizhenyu[];
extern const unsigned char gImage_weather_qingtian[];
extern const unsigned char gImage_weather_wumai[];
extern const unsigned char gImage_weather_xiaxue[];
extern const unsigned char gImage_weather_yintian[];
extern const unsigned char gImage_weather_youfeng[];
extern const unsigned char gImage_weather_yujiaxue[];

// === 天气图标映射表 ===
typedef struct
{
    const char*          keyword; // 匹配关键字 (如 "雷")
    const unsigned char* img_ptr; // 对应的图片指针
} Weather_Map_t;

// --- 天气参数图标 ---
extern const unsigned char gImage_weather_shinei[];
extern const unsigned char gImage_weather_shineiwendu[];
extern const unsigned char gImage_weather_shineishidu[];
extern const unsigned char gImage_weather_wencha[];
extern const unsigned char gImage_weather_fengxiang[];
extern const unsigned char gImage_weather_kongqizhiliang[];
extern const unsigned char gImage_weather_shidu[];
extern const unsigned char gImage_weather_qiya[];

// --- WIFI 图标 ---
extern const unsigned char gImage_WIFI[];
extern const unsigned char gImage_WIFI_Disconnected[];
#endif /* __APP_UI_CONF_H */