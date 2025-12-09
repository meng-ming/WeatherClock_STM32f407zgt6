#include "sys_log.h"
#include "st7789.h"
#include <string.h>

#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"
#include "font_variable.h"

// 引用外部互斥锁
extern SemaphoreHandle_t g_mutex_lcd;

// 字体渲染缓冲区大小定义
// 依据：最大支持 64x64 像素的汉字，RGB565 格式每个像素 2 字节
// 计算：64 * 64 * 2 = 8192 Bytes (8KB)
// 说明：STM32F407 SRAM 充足 (192KB)，使用静态缓冲区避免栈溢出或频繁 malloc 碎片
#define FONT_DMA_BUFFER_SIZE (64 * 64 * 2)

static uint8_t s_font_render_buf[FONT_DMA_BUFFER_SIZE];

/**
 * @brief  [内部函数] 绘制单个字模的底层实现 (Color Expansion + DMA)
 * @note   核心算法：
 * 1. 读取单色字模的每一位 (Bit)。
 * 2. 若为 1，向缓冲区填入前景色 (2 Bytes)。
 * 3. 若为 0，向缓冲区填入背景色 (2 Bytes)。
 * 4. 使用 DMA 一次性搬运缓冲区数据到屏幕 GRAM。
 * @param  x, y   绘制起始坐标
 * @param  w, h   字符宽高
 * @param  dots   单色字模数据指针
 * @param  fg, bg 前景/背景颜色 (RGB565)
 */
static void TFT_Draw_Glyph_Base(
    uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* dots, uint16_t fg, uint16_t bg)
{
    // 1. 安全检查：防止缓冲区溢出
    if ((uint32_t) w * h * 2 > FONT_DMA_BUFFER_SIZE)
    {
        LOG_E("[ST7789] Font Size Overflow!");
        return; // 字符尺寸超过缓冲区限制，放弃绘制
    }

    // 2. 颜色预处理：拆分为高低字节 (ST7789 为大端序 MSB First)
    uint8_t fg_h = fg >> 8;
    uint8_t fg_l = fg & 0xFF;
    uint8_t bg_h = bg >> 8;
    uint8_t bg_l = bg & 0xFF;

    // 3. 计算对齐参数
    // 字模数据通常按字节对齐，每行不足 8 位也会占用 1 个字节
    // 例如宽度 12 像素 -> (12+7)/8 = 2 字节/行
    uint8_t bytes_per_row = (w + 7) / 8;

    uint32_t buf_idx = 0;

    // 4. 颜色扩展循环 (Color Expansion Loop)
    for (uint16_t row = 0; row < h; row++)
    {
        // 定位到当前行的字模数据起始地址
        const uint8_t* row_data = dots + (row * bytes_per_row);

        for (uint16_t col = 0; col < w; col++)
        {
            // 定位当前像素在字节中的位置
            // 假设取模格式为：横向取模，字节内低位在前 (LSB First)
            // 如果显示的字左右反了，请修改为: (0x80 >> bit_idx)
            uint8_t pixel_byte = row_data[col / 8];
            uint8_t bit_idx    = col % 8;

            if (pixel_byte & (1 << bit_idx))
            {
                // 笔画：填入前景色
                s_font_render_buf[buf_idx++] = fg_h;
                s_font_render_buf[buf_idx++] = fg_l;
            }
            else
            {
                // 背景：填入背景色
                s_font_render_buf[buf_idx++] = bg_h;
                s_font_render_buf[buf_idx++] = bg_l;
            }
        }
    }

    // 5. 启动 DMA 传输
    // 此时 CPU 已经完成工作，DMA 将负责把 s_font_render_buf 搬运到 SPI
    TFT_ShowImage_DMA(x, y, w, h, s_font_render_buf);
}

void TFT_Show_String(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg)
{
    // === 获取互斥锁 ===
    // 保护屏幕资源，防止多任务重入导致花屏
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);
    }

    // 参数指针判空，防止空指针解引用导致 HardFault
    if (str == NULL || font == NULL)
    {
        goto exit;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    // 遍历字符串直到遇到结束符 '\0'
    while (*str)
    {
        // --- 特殊字符 '\n' ---
        if (*str == '\n')
        {
            cursor_x = x;              // 回车：X 回到起始点
            cursor_y += font->ascii_h; // 换行：Y 增加行高
            str++;
            continue;
        }

        // --- 越界检测 ---
        // 如果当前行已经超出屏幕底部，终止绘制
        if (cursor_y + font->cn_h > TFT_LINE_NUMBER)
        {
            break;
        }

        // 自动换行：如果当前字超出屏幕右侧
        if (cursor_x + font->cn_w > TFT_COLUMN_NUMBER)
        {
            cursor_x = x;
            cursor_y += font->cn_h; // 按照汉字高度换行，因为通常汉字比 ASCII 高
        }

        // --- 字符分类处理 (ASCII vs 汉字) ---

        // 场景 1: 标准 ASCII 字符 (0x20 ~ 0x7E)
        // 只要最高位是 0，即可兼容 GBK 中的 ASCII 部分
        if (*str >= 0x20 && *str <= 0x7E)
        {
            uint32_t char_idx = *str - 0x20;

            // 计算该字符在字库数组中的字节偏移量
            // 对齐计算：((Width + 7) / 8) * Height
            uint32_t bytes_per_row = (font->ascii_w + 7) / 8;
            uint32_t char_size     = bytes_per_row * font->ascii_h;

            // 调用 DMA 绘图
            TFT_Draw_Glyph_Base(cursor_x,
                                cursor_y,
                                font->ascii_w,
                                font->ascii_h,
                                font->ascii_map + (char_idx * char_size),
                                color_fg,
                                color_bg);

            // 光标右移
            cursor_x += font->ascii_w;
            str++; // 移动 1 字节
        }
        // 场景 2: 汉字或扩展字符 (GBK/Multibyte)
        else
        {
            const uint8_t* p_target_data = NULL;
            int            match_len     = 0;

            // 检查汉字库是否有效
            if (font->hzk_table && font->hzk_count > 0)
            {
                const uint8_t* p_base = (const uint8_t*) font->hzk_table;

                // 遍历汉字映射表查找匹配项
                for (uint16_t i = 0; i < font->hzk_count; i++)
                {
                    // 获取当前字库条目的指针
                    const uint8_t* p_entry = p_base + (i * font->hzk_struct_size);

                    // 获取对应的字符串索引 (假设结构体第一个成员是指向字符串的指针)
                    const char* key     = *(const char**) p_entry;
                    size_t      key_len = strlen(key);

                    // 字符串前缀匹配
                    if (strncmp(str, key, key_len) == 0)
                    {
                        // 找到匹配，计算字模数据地址
                        p_target_data = p_entry + font->hzk_data_offset;
                        match_len     = key_len;
                        break; // 退出查找循环
                    }
                }
            }

            if (p_target_data)
            {
                // 绘制汉字
                TFT_Draw_Glyph_Base(
                    cursor_x, cursor_y, font->cn_w, font->cn_h, p_target_data, color_fg, color_bg);

                cursor_x += font->cn_w;
                str += match_len; // 跳过已匹配的字节数 (如 GBK 为 2，UTF-8 为 3)
            }
            else
            {
                // 异常处理：未找到字模 (绘制纯色块占位，用于调试)
                TFT_Fill_Rect_DMA(cursor_x, cursor_y, font->cn_w, font->cn_h, RED);

                cursor_x += font->cn_w;

                // 强制跳过 1 字节，尝试重新对齐编码
                str++;
            }
        }
    }

exit:
    // === 释放互斥锁 ===
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }
}