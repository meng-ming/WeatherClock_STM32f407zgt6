/* lcd_font.c —— 终极懒人版显示核心（支持随便扔字模） */
#include "lcd_font.h"
#include "st7789.h"
#include <stdint.h>
#include <stddef.h>

/* * 私有：底层绘图函数 (解耦了font结构体，支持任意宽高)
 * w: 字符宽度
 * h: 字符高度
 * size: 字模数据字节数
 */
static void lcd_draw_glyph_base(uint16_t       x,
                                uint16_t       y,
                                uint16_t       w,
                                uint16_t       h,
                                const uint8_t* dots,
                                uint32_t       size,
                                uint16_t       color_fg,
                                uint16_t       color_bg)
{
    uint32_t i, pixel_count = 0;
    uint8_t  byte;

    /* 设置开窗区域 */
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

    LCD_DC_SET();
    LCD_CS_CLR();

    uint8_t fg_h = color_fg >> 8, fg_l = color_fg & 0xFF;
    uint8_t bg_h = color_bg >> 8, bg_l = color_bg & 0xFF;

    for (i = 0; i < size; i++)
    {
        byte = dots[i];
        for (int j = 0; j < 8; j++)
        {
            if (pixel_count >= w * h)
                goto end; // 防止溢出

            // 低位先行还是高位先行？根据你的取模软件设置
            // 通常是低位(LSB)先行或者高位(MSB)
            // 这里假设 data & 0x01 是第一个点 (LSB First)
            // 如果字模显示反了，改成 if(byte & 0x80) 并 byte <<= 1
            if (byte & 0x01)
            {
                ST7789_SPI_SendByte(fg_h);
                ST7789_SPI_SendByte(fg_l);
            }
            else
            {
                ST7789_SPI_SendByte(bg_h);
                ST7789_SPI_SendByte(bg_l);
            }
            byte >>= 1;
            pixel_count++;
        }
    }
end:
    LCD_CS_SET();
}

// ----------------------------------------------------------------------------
// 公共接口：通用字符串显示
// ----------------------------------------------------------------------------
void lcd_show_string(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    if (!str || !font)
        return;

    while (*str)
    {
        // === 1. 处理换行 ===
        if (*str == '\n')
        {
            cursor_x = x;
            cursor_y += font->ascii_h;
            str++;
            continue;
        }

        // === 2. 处理 ASCII (0x00 - 0x7F) ===
        if ((uint8_t) *str < 0x80)
        {
            if (*str >= 32 && *str <= 126)
            {
                // 计算 ASCII 偏移
                uint32_t char_size = (font->ascii_w * font->ascii_h) / 8;
                uint32_t offset    = (*str - 32) * char_size;

                lcd_draw_glyph_base(cursor_x,
                                    cursor_y,
                                    font->ascii_w,
                                    font->ascii_h,
                                    &font->ascii_map[offset],
                                    char_size,
                                    color_fg,
                                    color_bg);
                cursor_x += font->ascii_w;
            }
            str++;
        }
        // === 3. 处理 汉字 (通用查表法 - 真正使用 hzk_struct_size) ===
        else
        {
            if (!str[1])
                break; // 保护：如果只剩半个字节，直接退出

            // 组合 GBK 码 (假设大端或编译器默认顺序)
            uint16_t       gb_code       = ((uint8_t) str[0] << 8) | (uint8_t) str[1];
            const uint8_t* p_target_data = NULL;

            // 如果配置了字库表
            if (font->hzk_table && font->hzk_count > 0)
            {
                // 将 void* 转为 byte* 以便按字节步进
                const uint8_t* p_base = (const uint8_t*) font->hzk_table;

                for (uint32_t k = 0; k < font->hzk_count; k++)
                {
                    // 计算当前汉字结构体的首地址：基地址 + (序号 * 单个结构体大小)
                    const uint8_t* p_entry = p_base + (k * font->hzk_struct_size);

                    // 读出索引 (假设 index 在结构体起始位置，即 offset 0，且为大端序)
                    // 如果你的单片机是小端序，这里可能需要 swap，但通常字库也是按大端存的
                    // 你的 hzk16.c 写的是 0xD6EC，在内存里可能是 EC D6 (小端)，也可能是 D6 EC
                    // 这里我们按标准网络字节序(大端)拼，如果不行就反过来试一下

                    // 尝试匹配 (兼容两种大小端情况，或者直接取 uint16_t 指针解引用)
                    uint16_t idx_val = *(const uint16_t*) p_entry;

                    // 如果你的字库是 const HZK_16_t 数组，在 STM32 (小端) 上：
                    // 0xD6EC 会被存为 EC D6。
                    // 而 gb_code 由 str[0]<<8 | str[1] 组成，如果是 D6 EC，则是 0xD6EC。
                    // 所以这里直接强转比较通常没问题，除非字库源文件就是字节反的。

                    if (idx_val == gb_code || idx_val == (((gb_code & 0xFF) << 8) | (gb_code >> 8)))
                    {
                        // 找到了！数据地址 = 结构体首地址 + 数据偏移量
                        p_target_data = p_entry + font->hzk_data_offset;
                        break;
                    }
                }
            }

            if (p_target_data)
            {
                // 绘制汉字
                lcd_draw_glyph_base(cursor_x,
                                    cursor_y,
                                    font->cn_w,
                                    font->cn_h,
                                    p_target_data,
                                    font->hzk_data_size,
                                    color_fg,
                                    color_bg);
            }
            else
            {
                // 没找到，画红框提示 (调试用)
                TFT_Fill_Rect(cursor_x,
                              cursor_y,
                              cursor_x + font->cn_w - 1,
                              cursor_y + font->cn_h - 1,
                              RED); // 确保 RED 已定义，或者用 0xF800
            }

            cursor_x += font->cn_w;
            str += 2; // GBK 汉字占 2 字节
        }

        // 自动换行
        if (cursor_x + font->ascii_w > TFT_COLUMN_NUMBER)
        {
            cursor_x = x;
            cursor_y += font->cn_h;
        }
    }
}