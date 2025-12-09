/**
 * @file    font.h
 * @brief   通用 LCD 字体显示引擎接口 (DMA 高性能版)
 * @note    本模块负责字符/字符串的渲染逻辑，底层依赖 st7789 驱动的 DMA 搬运能力。
 * 实现了从单色位图到 RGB565 显存数据的转换 (Color Expansion)。
 * 核心特性：
 * 1. 编码兼容：支持 GBK/ASCII 混合排版。
 * 2. 智能排版：支持自动换行、越界裁剪。
 * 3. 极致性能：DMA 显存缓冲队列发送，释放 CPU。
 * @author  meng-ming
 * @version 2.0
 * @date    2025-12-09
 */

#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>
#include "font_variable.h" // 引用字体结构体定义

/* ==================================================================
 * 1. 默认配置宏 (Default Configuration)
 * ================================================================== */

// 如果项目中未定义颜色，提供默认 RGB565 定义
#ifndef WHITE
#define WHITE 0xFFFF
#define BLACK 0x0000
#endif

/* ==================================================================
 * 2. 核心接口声明 (Core API Declarations)
 * ================================================================== */

/**
 * @brief  在指定位置显示字符串 (核心接口)
 * @note   1. 支持中英混合字符串 (GBK/UTF-8兼容设计，视字库而定)。
 * 2. 具备自动换行功能：当 X 坐标超出屏幕宽度时，自动折行。
 * 3. 具备越界保护：当 Y 坐标超出屏幕高度时，停止渲染。
 * 4. 线程安全：内部集成递归互斥锁 (Recursive Mutex)，防止多任务冲突。
 * 5. 高性能：使用 DMA 批量传输，非阻塞等待（取决于底层实现）。
 * @param  x        起始 X 坐标 (0 ~ SCREEN_WIDTH-1)
 * @param  y        起始 Y 坐标 (0 ~ SCREEN_HEIGHT-1)
 * @param  str      字符串指针 (必须以 NULL 结尾)
 * @param  font     字体描述符指针 (包含 ASCII 和 汉字库信息)
 * @param  color_fg 字体前景色 (RGB565)
 * @param  color_bg 字体背景色 (RGB565)
 * @retval None
 */
void TFT_Show_String(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg);

#endif /* __FONT_H */