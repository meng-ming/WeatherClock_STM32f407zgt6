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
#include "font_variable.h"
#include "st7789.h"
#include "app_ui_config.h"
#include "sys_log.h"
#include "font.h"

// 引入新做好的页面模块
#include "ui_main_page.h"
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"

// 引入全局变量
extern SemaphoreHandle_t g_mutex_lcd;

// 全局静态变量
static UI_Label_t s_status = {
    35, BOX_STATUS_Y + 5, BOX_STATUS_X + BOX_STATUS_W - 35, &font_16, 0, UI_STATUS_BG, {0, 0}};

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
    s_status.fg_color = color;
    UI_Print_Label(&s_status, status);
}

Cursor_Pos_t UI_Print_Label(UI_Label_t* label, const char* str)
{
    // 1. 获取递归锁 (保障 "绘制+擦除" 操作的原子性，防止中间被打断导致闪烁)
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);
    }

    // 2. 调用底层绘图，获取当前的结束坐标
    //    注意：传入 label->limit_width 以启用底层的自动换行计算
    Cursor_Pos_t curr_pos = TFT_Show_String(
        label->x, label->y, label->limit_width, str, label->font, label->fg_color, label->bg_color);

    // 3. 计算行高 (优先使用汉字高度，覆盖范围更全)
    uint16_t line_h = (label->font->cn_h > 0) ? label->font->cn_h : label->font->ascii_h;

    // 4. 计算当前控件的物理右边界 (用于行末擦除)
    uint16_t right_bound =
        (label->limit_width == 0) ? TFT_COLUMN_NUMBER : (label->x + label->limit_width);
    if (right_bound > TFT_COLUMN_NUMBER)
        right_bound = TFT_COLUMN_NUMBER;

    // ============================================================
    // 智能擦除逻辑 (Diff Algorithm)
    // 对比 curr_pos (新) 与 label->last_pos (旧)
    // ============================================================

    // 场景 A: 行数减少了 (例如从 3 行变成了 1 行)
    // 此时不仅要擦除当前行剩余部分，还要把下方多出来的旧行全部填平
    if (curr_pos.end_y < label->last_pos.end_y)
    {
        // 1. 擦除当前行剩余部分 (从 end_x 到 右边界)
        uint16_t clear_w_row = right_bound - curr_pos.end_x;
        if (clear_w_row > 0)
        {
            TFT_Fill_Rect_DMA(curr_pos.end_x, curr_pos.end_y, clear_w_row, line_h, label->bg_color);
        }

        // 2. 擦除下方所有旧行 (从 下一行顶 到 旧结束行底)
        // 垂直高度 = 旧结束Y - 新结束Y
        uint16_t clear_h_block = label->last_pos.end_y - curr_pos.end_y;

        // 填充整块矩形区域 (从下一行起始处开始)
        // 注意：这里简单处理为擦除整行宽度，确保残留彻底清除
        uint16_t block_w =
            (label->limit_width == 0) ? (TFT_COLUMN_NUMBER - label->x) : label->limit_width;

        TFT_Fill_Rect_DMA(
            label->x, curr_pos.end_y + line_h, block_w, clear_h_block, label->bg_color);
    }
    // 场景 B: 行数没变，但最后一行变短了
    else if (curr_pos.end_y == label->last_pos.end_y)
    {
        if (curr_pos.end_x < label->last_pos.end_x)
        {
            // 只需擦除尾巴
            uint16_t clear_w = label->last_pos.end_x - curr_pos.end_x;
            TFT_Fill_Rect_DMA(curr_pos.end_x, curr_pos.end_y, clear_w, line_h, label->bg_color);
        }
    }
    // 场景 C: 变长或行数增加
    // 不需要任何擦除操作，新内容会自动覆盖旧内容 (背景色填充由 TFT_Show_String 完成)

    // 5. 更新状态 (记账)
    label->last_pos = curr_pos;

    // 6. 释放锁
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }

    return curr_pos;
}