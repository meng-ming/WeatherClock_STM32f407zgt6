/**
 * @file    app_ui.h
 * @brief   UI 界面模块对外接口
 * @note    负责所有屏幕显示逻辑，依赖 app_data.h 定义的数据结构
 */

#ifndef __APP_UI_H
#define __APP_UI_H

#include "app_data.h" // 引用通用数据结构
#include <stdint.h>

/**
 * @brief  UI 模块初始化
 * @note   负责绘制静态的背景框架（分割线、标签等）。
 * 此函数应在系统启动时调用一次。
 * @retval None
 */
void APP_UI_Init(void);

/**
 * @brief  刷新天气数据（动态内容）
 * @note   根据传入的天气数据，更新屏幕上对应的显示区域。
 * 此函数会自动处理局部刷新，不会造成全屏闪烁。
 * @param  data: 指向 APP_Weather_Data_t 结构体的指针，包含最新的天气信息（城市、温度等）。
 * @retval None
 */
void APP_UI_Update(const APP_Weather_Data_t* data);

/**
 * @brief  显示底部状态栏信息
 * @note   在屏幕底部显示系统运行状态（如 "Connecting...", "Updated!"）。
 * 会自动擦除旧的状态文字。
 * @param  status: 要显示的字符串（建议不超过 20 个字符）。
 * @param  color:  字体颜色（RGB565格式，如 WHITE, RED, BLUE）。
 * @retval None
 */
void APP_UI_ShowStatus(const char* status, uint16_t color);

#endif