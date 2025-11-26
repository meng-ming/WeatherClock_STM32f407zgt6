/**
 * @file    lcd_font.c
 * @brief   通用 LCD 字体显示引擎实现
 */

#include "lcd_font.h"
#include "st7789.h" // 依赖底层驱动的绘图指令
#include <stdint.h>
#include <stddef.h>

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
                                uint32_t       size,
                                uint16_t       fg,
                                uint16_t       bg)
{
    // 1. 设置绘图窗口
    // 这里的边界检查交给底层驱动或硬件去做，上层保证传入参数合理即可
    TFT_SEND_CMD(0x2A); // Column Address Set
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x & 0xFF);
    TFT_SEND_DATA((x + w - 1) >> 8);
    TFT_SEND_DATA((x + w - 1) & 0xFF);

    TFT_SEND_CMD(0x2B); // Row Address Set
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y & 0xFF);
    TFT_SEND_DATA((y + h - 1) >> 8);
    TFT_SEND_DATA((y + h - 1) & 0xFF);

    TFT_SEND_CMD(0x2C); // Memory Write

    // 2. 准备颜色字节 (拆分为高低位，优化发送)
    uint8_t fg_h = fg >> 8, fg_l = fg & 0xFF;
    uint8_t bg_h = bg >> 8, bg_l = bg & 0xFF;

    // 3. 开始批量发送像素
    // LCD_DC_SET/CLR 操作建议封装在 TFT_SEND_DATA 内部或保持现状
    // 为了性能，这里直接操作 GPIO 宏是合理的
    LCD_DC_SET();
    LCD_CS_CLR();

    uint32_t drawn_pixels = 0;
    uint32_t total_pixels = w * h;

    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t temp = dots[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (drawn_pixels >= total_pixels)
                break; // 防止溢出

            // 判断当前位是 1 还是 0
            // 假设字模是 LSB First (低位在左)，即 bit0 对应第一个像素
            // 如果你的字模是 MSB First，请改为 if (temp & 0x80) 并 temp <<= 1
            if (temp & 0x01)
            {
                ST7789_SPI_SendByte(fg_h);
                ST7789_SPI_SendByte(fg_l);
            }
            else
            {
                ST7789_SPI_SendByte(bg_h);
                ST7789_SPI_SendByte(bg_l);
            }

            temp >>= 1; // 移位到下一个像素
            drawn_pixels++;
        }
    }

    LCD_CS_SET();
}

/**
 * @brief  显示字符串 (对外接口)
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
            cursor_y += font->ascii_h; // 换行高度按 ASCII 高度算，或者取 max(ascii_h, cn_h)
            str++;
            continue;
        }

        // === B. 处理 ASCII 字符 (0x20 ~ 0x7E) ===
        if ((uint8_t) *str < 0x80)
        {
            if (*str >= 0x20 && *str <= 0x7E)
            {
                // 自动换行检测
                if (cursor_x + font->ascii_w > TFT_COLUMN_NUMBER)
                {
                    cursor_x = x;
                    cursor_y += font->ascii_h;
                }

                // 越界保护
                if (cursor_y + font->ascii_h > TFT_LINE_NUMBER)
                    break;

                // 计算字模偏移
                uint32_t char_idx = *str - 0x20;
                // 假设每个 ASCII 字符占用的字节数是固定的
                // 计算公式：(宽 * 高) / 8，向上取整（如果不是8的倍数）
                // 这里简化假设是字节对齐的
                uint32_t char_size = (font->ascii_w * font->ascii_h) / 8;

                // 绘制
                LCD_Draw_Glyph_Base(cursor_x,
                                    cursor_y,
                                    font->ascii_w,
                                    font->ascii_h,
                                    font->ascii_map + (char_idx * char_size),
                                    char_size,
                                    color_fg,
                                    color_bg);

                cursor_x += font->ascii_w;
            }
            str++;
        }
        // === C. 处理汉字 (GBK 双字节) ===
        else
        {
            // 保护：如果字符串意外结束（只有半个汉字）
            if (*(str + 1) == '\0')
                break;

            // 自动换行检测
            if (cursor_x + font->cn_w > TFT_COLUMN_NUMBER)
            {
                cursor_x = x;
                cursor_y += font->cn_h; // 汉字换行按汉字高度
            }

            // 越界保护
            if (cursor_y + font->cn_h > TFT_LINE_NUMBER)
                break;

            // 组合 GBK 编码 (注意端序)
            // 假设编译器里的字符串是按地址顺序存的，byte0是高位，byte1是低位
            uint16_t       gb_code  = ((uint8_t) *str << 8) | (uint8_t) *(str + 1);
            const uint8_t* p_target = NULL;

            // 查表
            if (font->hzk_table && font->hzk_count > 0)
            {
                const uint8_t* p_base = (const uint8_t*) font->hzk_table;

                for (uint16_t i = 0; i < font->hzk_count; i++)
                {
                    // 计算当前汉字结构体地址
                    const uint8_t* p_entry = p_base + (i * font->hzk_struct_size);

                    // 读出索引值 (假设 index 在 offset 0，且为 uint16_t)
                    // 注意处理大小端：STM32 是小端，如果字库是大端存的，需要交换
                    // 这里做一个简单的兼容判断
                    uint16_t idx_val = *(const uint16_t*) p_entry;

                    // 尝试直接匹配或反序匹配
                    if (idx_val == gb_code || idx_val == (((gb_code & 0xFF) << 8) | (gb_code >> 8)))
                    {
                        // 找到了！获取字模数据指针
                        p_target = p_entry + font->hzk_data_offset;
                        break;
                    }
                }
            }

            if (p_target)
            {
                LCD_Draw_Glyph_Base(cursor_x,
                                    cursor_y,
                                    font->cn_w,
                                    font->cn_h,
                                    p_target,
                                    font->hzk_data_size,
                                    color_fg,
                                    color_bg);
            }
            else
            {
                // 没找到汉字：画一个红框或者方块表示缺失
                // 这里简单跳过或者画背景色块
                TFT_Fill_Rect(cursor_x,
                              cursor_y,
                              cursor_x + font->cn_w - 1,
                              cursor_y + font->cn_h - 1,
                              color_bg); // 或者 RED 用于调试
            }

            cursor_x += font->cn_w;
            str += 2; // 跳过两个字节
        }
    }
}