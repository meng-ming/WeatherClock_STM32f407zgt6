/**
 * @file    app_ui.h
 * @brief   UI 界面模块对外接口
 * @note    负责所有屏幕显示逻辑，依赖 app_data.h 定义的数据结构。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __APP_UI_H
#define __APP_UI_H

#include "app_data.h" // 引用通用数据结构
#include <stdint.h>
#include "font_variable.h"

/**
 * @brief UI 智能标签控件对象
 * @note  封装了文本标签的静态属性与动态渲染状态，支持自动换行与残留擦除。
 */
typedef struct
{
    // === 静态属性 (初始化配置) ===
    uint16_t x;           ///< 起始 X 坐标
    uint16_t y;           ///< 起始 Y 坐标
    uint16_t limit_width; ///< 换行宽度限制 (若为0则延伸至屏幕边缘)

    const font_info_t* font;     ///< 字体对象指针
    uint16_t           fg_color; ///< 前景色
    uint16_t           bg_color; ///< 背景色 (用于擦除填充)

    // === 动态状态 (自动维护，无需手动修改) ===
    Cursor_Pos_t last_pos; ///< 记录上一次绘制结束的光标坐标 (end_x, end_y)
                           ///< 用于比对新旧区域，实现多行精准擦除。
                           ///< 初始化时建议设为 {x, y} 或全 0。
} UI_Label_t;

/* ==================================================================
 * 1. UI 模块函数接口
 * ================================================================== */

/**
 * @brief  UI 模块初始化
 * @note   负责绘制静态的背景框架（分割线、标签等）。
 *         此函数应在系统启动时调用一次，依赖 ST7789_Init() 已完成。
 *         初始化后屏幕进入待更新状态，全屏DMA清背景一次，包括开机屏显示 2s + 主页面框架。
 * @retval None
 */
void APP_UI_Init(void);

/**
 * @brief  刷新天气数据（动态内容）
 * @note   根据传入的天气数据，更新屏幕上对应的显示区域。
 *         此函数会自动处理局部刷新，不会造成全屏闪烁。
 * @param  data: 指向 APP_Weather_Data_t 结构体的指针，包含最新的天气信息（城市、温度等）。
 * @retval None
 */
void APP_UI_Update(const APP_Weather_Data_t* data);

/**
 * @brief  显示顶部状态栏信息
 * @note   在屏幕顶部显示系统运行状态（如 "Connecting...", "Updated!"）。
 *         会自动擦除旧的状态文字。

 * @param  status: 要显示的字符串（建议不超过 20 个字符，NULL 终止）。
 * @param  color:  字体颜色（RGB565格式，如 WHITE, RED, BLUE）。
 * @retval None
 */
void APP_UI_ShowStatus(const char* status, uint16_t color);

/**
 * @brief  打印智能标签（自动处理渲染、换行与残留擦除）
 * @note   该函数为业务层核心接口，自动维护控件状态，无需手动管理清屏。
 *
 * @param  label  标签控件对象指针 (需持久化存在，不能是局部变量)
 * @param  str    待显示的新字符串
 * @return Cursor_Pos_t  本次绘制结束后的光标坐标，供后续流式布局使用。
 */
Cursor_Pos_t UI_Print_Label(UI_Label_t* label, const char* str);

#endif /* __APP_UI_H */