/**
 * @file    lcd_font.h
 * @brief   通用 LCD 字体显示引擎
 * @note    支持 ASCII 与 GBK 汉字混合排版，支持自定义字库挂载。
 *          本模块不依赖具体的 LCD 驱动，通过底层绘图接口解耦。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include <stdint.h>
#include "font_variable.h"

/* ==================================================================
 * 1. 函数接口声明 (Function Declarations)
 * ================================================================== */

/**
 * @brief  在指定位置显示字符串 (支持中英混合、自动换行)
 * @note   处理 GBK 编码字符串，自动检测 ASCII/汉字并调用对应渲染。
 *         支持屏幕边界自动换行，超出区域裁剪。
 * @param  x:        起始 X 坐标 (像素)
 * @param  y:        起始 Y 坐标 (像素)
 * @param  str:      要显示的字符串 (支持 GBK 编码，NULL 终止)
 * @param  font:     字体配置描述符指针 (font_info_t)
 * @param  color_fg: 前景色 (RGB565)
 * @param  color_bg: 背景色 (RGB565)
 * @retval None
 */
void LCD_Show_String(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg);

#endif /* __LCD_FONT_H */