/**
 * @file    app_ui.c
 * @brief   UI应用模块。管理开机界面、LCD硬件初始化、互斥更新天气数据、状态文字显示与擦除。
 * @author  meng-ming
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "app_ui.h"
#include "BSP_Tick_Delay.h"
#include "st7789.h"
#include "app_ui_config.h"
#include "sys_log.h"
#include "font.h"

// 引入新做好的页面模块
#include "ui_main_page.h"
#include <string.h>

#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"

// 引入全局变量
extern SemaphoreHandle_t g_mutex_lcd;

static uint8_t s_last_status_len = 0; // 上个状态文字的长度，用于覆盖刷新

/**
 * @brief  系统开机界面
 * @note   负责初始化 LCD 硬件驱动，清屏，展示整个系统的开机界面。
 * @retval None
 */
static void APP_Start_UP(void)
{
    // 1. ST7789 底层初始化
    ST7789_Init();

    // 2. 显示开机界面
    TFT_ShowImage_DMA(0, 0, 240, 320, gImage_Startup_Screen);

    // 3. 延时2s
    BSP_Delay_ms(2000);
}

void APP_UI_Init(void)
{
    // 1. 启动开机界面
    APP_Start_UP();

    // 2. 调用主页面初始化
    APP_UI_MainPage_Init();
}

void APP_UI_Update(const APP_Weather_Data_t* data)
{
    // 上锁：防止被日历更新任务打断，引起花屏
    if (g_mutex_lcd != NULL)
    {
        // 尝试拿锁，如果长时间拿不到（比如死锁了），打印错误
        if (xSemaphoreTakeRecursive(g_mutex_lcd, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            APP_UI_UpdateWeather(data);
            xSemaphoreGiveRecursive(g_mutex_lcd);
        }
        else
        {
            LOG_E("[UI] Take LCD Mutex Timeout!");
        }
    }
}

void APP_UI_ShowStatus(const char* status, uint16_t color)
{
    LOG_I("[APP] %s", status); // 调试时可打开

    // 1. 直接绘制新状态
    TFT_Show_String(35, BOX_STATUS_Y + 5, status, &font_16, color, UI_STATUS_BG);

    // 2. 如果新文字比旧文字短，多出来的旧文字需要擦除
    //    方法：在屁股后面画空格，空格会自动填充背景色
    size_t new_len = strlen(status);

    if (new_len < s_last_status_len)
    {
        uint16_t x_offset = 35 + new_len * 8; // 计算空格开始的 x 坐标

        for (size_t i = new_len; i < s_last_status_len; i++)
        {
            // 画一个背景色的空格，就把旧字盖掉了
            TFT_Show_String(x_offset, BOX_STATUS_Y + 5, " ", &font_16, UI_STATUS_BG, UI_STATUS_BG);
            x_offset += 8;
        }
    }

    // 3. 更新历史长度
    s_last_status_len = new_len;
}
