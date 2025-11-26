#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include <stdint.h>

/**
 * @brief 通用字体描述符 (商业级通用定义)
 * 支持 ASCII 和 汉字 混排，支持任意尺寸
 */
typedef struct
{
    // === ASCII 部分 ===
    uint8_t        ascii_w;   // 字符宽 (例如: 8, 12, 16)
    uint8_t        ascii_h;   // 字符高 (例如: 16, 24, 32)
    const uint8_t* ascii_map; // ASCII 字库数组指针 (按顺序排列)

    // === 汉字 部分 ===
    uint8_t     cn_w;      // 汉字宽 (例如: 16, 24)
    uint8_t     cn_h;      // 汉字高 (例如: 16, 24)
    const void* hzk_table; // 汉字结构体数组指针 (用 void* 是为了兼容不同大小的结构体)

    // === 汉字内存寻址参数 ===
    uint16_t hzk_count;       // 汉字数量
    uint16_t hzk_struct_size; // 汉字数组中单个元素的大小 (sizeof) -> 用于计算指针步进
    uint16_t hzk_data_offset; // 字模数据在结构体中的偏移量 (通常 index 占2字节，所以这里是 2)
    uint16_t hzk_data_size;   // 单个汉字的点阵数据字节数 (例如: 16x16=32, 24x24=72)

} font_info_t;

typedef struct
{
    uint16_t      GBK_Code; // 汉字编码，例如 "你" = 0xC4E3
    const uint8_t Msk[32];  // 字模数据
} CN_Font_16_t;

/* 对外显示接口 */
void LCD_Show_String(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg);

#endif