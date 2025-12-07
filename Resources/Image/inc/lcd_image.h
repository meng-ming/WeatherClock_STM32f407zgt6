/**
 * @file    image.h
 * @brief   图片显示接口 (BMP 或自定义格式渲染)
 * @note    提供底层图片数据到 LCD 的高效渲染接口
 *          图片数据需预转换为 RGB565 格式。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __IMAGE_H
#define __IMAGE_H

#include <stdint.h>

/* ==================================================================
 * 1. 函数接口声明 (Function Declarations)
 * ================================================================== */

/**
 * @brief  在指定位置显示图片
 * @note   将图片数据渲染到 LCD 指定区域，支持透明通道和边界裁剪。
 *         图片数据格式: 按行优先的字节数组 (RGB565)，上层需确保数据对齐。
 * @param  x:      起始 X 坐标 (像素)
 * @param  y:      起始 Y 坐标 (像素)
 * @param  w:      图片宽度 (像素)
 * @param  h:      图片高度 (像素)
 * @param  pData:  图片数据指针 (const unsigned char*)
 * @retval None
 */
void LCD_Show_Image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const unsigned char* pData);

#endif /* __IMAGE_H */