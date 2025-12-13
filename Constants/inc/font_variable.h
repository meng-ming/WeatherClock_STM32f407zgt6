/**
 * @file    font_variable.h
 * @brief   点阵字体变量与配置定义
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __FONT_VARIABLE_H
#define __FONT_VARIABLE_H

#include <stdint.h>

/* ==================================================================
 * 1. 类型定义
 * ================================================================== */

/**
 * @brief 字符串末尾光标位置结构体
 * @note  用于确定打印完字符串后光标的确切位置
 */
typedef struct
{
    uint16_t end_x; // 字符串结束后的 X 坐标
    uint16_t end_y; // 字符串结束后的 Y 坐标 (换行时会变)
} Cursor_Pos_t;

/**
 * @brief 16点阵汉字结构体
 * @note  用于存储单个汉字的索引和点阵数据
 */
typedef struct
{
    const char* index_str;  ///< 汉字索引 字符串指针，自动适应编码
    uint8_t     matrix[32]; ///< 点阵数据 (16*16/8 = 32字节)
} HZK_16_t;

/**
 * @brief 20点阵汉字结构体 (周日专用)
 * @note  用于存储周日汉字的索引和点阵数据
 */
typedef struct
{
    const char* index_str;  ///< 汉字索引 字符串指针，自动适应编码
    uint8_t     matrix[60]; ///< 点阵数据 ((20+7)/8*20 = 60字节)
} HZK_Week_20_t;

/**
 * @brief 字体配置描述符
 * @note  用于描述一套字体的属性（宽、高、字库地址等）
 */
typedef struct
{
    // === ASCII 部分 ===
    uint8_t        ascii_w;   ///< ASCII 字符宽度 (px)
    uint8_t        ascii_h;   ///< ASCII 字符高度 (px)
    const uint8_t* ascii_map; ///< ASCII 字库数组指针 (0x20~0x7E)

    // === 汉字 部分 ===
    uint8_t     cn_w;      ///< 汉字宽度 (px)
    uint8_t     cn_h;      ///< 汉字高度 (px)
    const void* hzk_table; ///< 汉字字库结构体数组指针

    // === 寻址参数 ===
    uint16_t hzk_count;       ///< 汉字总数
    uint16_t hzk_struct_size; ///< 单个汉字结构体的大小 (字节)
    uint16_t hzk_data_offset; ///< 字模数据在结构体中的偏移量 (字节)
    uint16_t hzk_data_size;   ///< 单个字模的数据大小 (字节)

} font_info_t;

/* ==================================================================
 * 2. 外部引用声明 (External Declarations)
 * ================================================================== */

// 原始字库数组 (定义在 Resources/Font/src/*.c 中)
extern const HZK_16_t      HZK_16[];
extern const HZK_Week_20_t HZK_Week_20[];
extern const uint8_t       ASCII_8x16[];
extern const uint8_t       ASCII_10x20[];
extern const uint8_t       ASCII_30x60[];

/**
 * @brief 全局点阵字体配置对象
 * @note  给 APP_ui.c 使用
 */
extern font_info_t font_16;
extern font_info_t font_time_20;
extern font_info_t font_time_30x60;

#endif /* __FONT_VARIABLE_H */