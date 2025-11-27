/**
 * @file    lcd_font.c
 * @brief   通用 LCD 字体显示引擎实现
 */

#include "lcd_font.h"
#include "st7789.h" // 依赖底层驱动的绘图指令
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief  底层绘图：绘制单个字模 (私有)
 * @param  x, y: 起始坐标
 * @param  w, h: 字模宽高
 * @param  dots: 字模数据指针
 * @param  size: 字模数据总字节数
 * @param  fg, bg: 前景色/背景色
 */
static void LCD_Draw_Glyph_Base(uint16_t       x,
                                uint16_t       y,
                                uint16_t       w,
                                uint16_t       h,
                                const uint8_t* dots,
                                uint32_t       size, // 这个参数现在仅作保险用
                                uint16_t       fg,
                                uint16_t       bg)
{
    // 1. 设置窗口 (保持不变)
    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x & 0xFF);
    TFT_SEND_DATA((x + w - 1) >> 8);
    TFT_SEND_DATA((x + w - 1) & 0xFF);

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y & 0xFF);
    TFT_SEND_DATA((y + h - 1) >> 8);
    TFT_SEND_DATA((y + h - 1) & 0xFF);

    TFT_SEND_CMD(0x2C);

    uint8_t fg_h = fg >> 8, fg_l = fg & 0xFF;
    uint8_t bg_h = bg >> 8, bg_l = bg & 0xFF;

    LCD_DC_SET();
    LCD_CS_CLR();

    // === 核心修改：计算每行实际占用的字节数 (向上取整) ===
    // 例如宽度 12：(12+7)/8 = 2 字节
    uint8_t bytes_per_row = (w + 7) / 8;

    for (uint16_t row = 0; row < h; row++)
    {
        // 定位到当前行的起始数据位置
        const uint8_t* row_data = dots + (row * bytes_per_row);

        for (uint16_t col = 0; col < w; col++)
        {
            // 计算当前点在这一行里的 字节索引 和 位索引
            uint8_t byte_idx = col / 8;
            uint8_t bit_idx  = col % 8;

            // 取出对应的字节
            uint8_t pixel_byte = row_data[byte_idx];

            // 判断该位是否点亮
            // 你的字模是 LSB First (低位在前)，所以用 (1 << bit_idx)
            // 如果是 MSB First，则用 (0x80 >> bit_idx)
            if (pixel_byte & (1 << bit_idx))
            {
                ST7789_SPI_SendByte(fg_h);
                ST7789_SPI_SendByte(fg_l);
            }
            else
            {
                ST7789_SPI_SendByte(bg_h);
                ST7789_SPI_SendByte(bg_l);
            }
        }
        // 一行画完，循环结束。
        // 下次循环 row++，row_data 指针会自动跳过行末的 padding bit，对齐到下一行首。
    }

    LCD_CS_SET();
}

/**
 * @brief  显示字符串 (智能匹配 GBK/UTF-8 编码)
 */
void LCD_Show_String(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg)
{
    // 1. 参数检查
    if (str == NULL || font == NULL)
        return;

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    while (*str)
    {
        // === A. 处理换行符 ===
        if (*str == '\n')
        {
            cursor_x = x;
            cursor_y += font->ascii_h;
            str++;
            continue;
        }

        // === B. 处理 ASCII 字符 (标准 ASCII < 0x80) ===
        // 注意：这里只要最高位是0，就认为是 ASCII，兼容 UTF-8 和 GBK 的 ASCII 部分
        if (*str >= 0x20 && *str <= 0x7E)
        {
            // ... 换行和越界检测保持不变 ...

            uint32_t char_idx = *str - 0x20;

            // === 核心修改：计算单个字符占用的字节数 ===
            // 公式：(行宽字节数) * 高度
            // 之前是 (w*h)/8，现在必须考虑字节对齐 padding
            uint32_t bytes_per_row = (font->ascii_w + 7) / 8;
            uint32_t char_size     = bytes_per_row * font->ascii_h;

            LCD_Draw_Glyph_Base(cursor_x,
                                cursor_y,
                                font->ascii_w,
                                font->ascii_h,
                                font->ascii_map + (char_idx * char_size),
                                char_size,
                                color_fg,
                                color_bg);

            cursor_x += font->ascii_w;
            str++;
        }
        // === C. 处理 汉字/特殊符号 (智能字符串匹配模式) ===
        else
        {
            const uint8_t* p_target_data = NULL;
            int            match_len     = 0; // 记录匹配到的字符串长度 (用于跳过)

            // 越界保护
            if (cursor_y + font->cn_h > TFT_LINE_NUMBER)
                break;

            // 自动换行检测
            if (cursor_x + font->cn_w > TFT_COLUMN_NUMBER)
            {
                cursor_x = x;
                cursor_y += font->cn_h;
            }

            // --- 遍历字库比对字符串 ---
            if (font->hzk_table && font->hzk_count > 0)
            {
                const uint8_t* p_base = (const uint8_t*) font->hzk_table;

                for (uint16_t i = 0; i < font->hzk_count; i++)
                {
                    const uint8_t* p_entry = p_base + (i * font->hzk_struct_size);

                    // 1. 获取字库中当前字的字符串索引 (假设它在结构体首地址)
                    // 这里的 *(const char**) 是指：取结构体前4个字节，当作一个字符串指针
                    const char* key = *(const char**) p_entry;

                    // 2. 获取 key 的长度 (例如 "朱" 在 UTF-8 是 3，GBK 是 2)
                    size_t key_len = strlen(key);

                    // 3. 比对：看当前的 str 开头是否和 key 一模一样
                    if (strncmp(str, key, key_len) == 0)
                    {
                        // 匹配成功！
                        // 计算数据偏移：结构体首地址 + 数据偏移量
                        p_target_data = p_entry + font->hzk_data_offset;
                        match_len     = key_len; // 记下长度，后面 str 要跳过这么多
                        break;
                    }
                }
            }

            if (p_target_data)
            {
                // 绘制汉字
                LCD_Draw_Glyph_Base(cursor_x,
                                    cursor_y,
                                    font->cn_w,
                                    font->cn_h,
                                    p_target_data,
                                    font->hzk_data_size,
                                    color_fg,
                                    color_bg);
                cursor_x += font->cn_w;
                str += match_len; // 关键：匹配了几个字节就跳过几个
            }
            else
            {
                // 没找到字 (字库缺失或乱码)
                // 绘制红色方块提示 (调试用)
                TFT_Fill_Rect(cursor_x, cursor_y, font->cn_w, font->cn_h, 0xF800); // RED

                cursor_x += font->cn_w; // 占位宽度

                // 强制跳过 1 个字节，防止死循环 (尝试重新对齐)
                // 商业代码里这里可以优化，但跳过1字节是最安全的兜底
                str++;
            }
        }
    }
}