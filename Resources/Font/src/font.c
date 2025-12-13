#include "sys_log.h"
#include "st7789.h"
#include <stdint.h>
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

Cursor_Pos_t TFT_Show_String(uint16_t           x,
                             uint16_t           y,
                             uint16_t           limit_width,
                             const char*        str,
                             const font_info_t* font,
                             uint16_t           color_fg,
                             uint16_t           color_bg)
{
    // 定义返回结构体
    Cursor_Pos_t cursor_end_pos;

    // === 1. 资源互斥保护 ===
    // 获取递归互斥锁，保障多任务环境下屏幕资源访问的原子性
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);
    }

    // === 2. 参数安全性检查 ===
    // 防止空指针解引用导致 HardFault 异常
    if (str == NULL || font == NULL)
    {
        // 发生错误时，返回起始坐标作为默认结束位置
        cursor_end_pos.end_x = x;
        cursor_end_pos.end_y = y;
        goto exit;
    }

    // 初始化内部光标
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    // === 3. 计算物理渲染边界 ===
    // 确定当前绘制区域的右侧极限坐标
    uint16_t right_boundary;
    if (limit_width == 0 || (x + limit_width > TFT_COLUMN_NUMBER))
    {
        // 若未指定限制或限制超出屏幕，则以屏幕物理边界为准
        right_boundary = TFT_COLUMN_NUMBER;
    }
    else
    {
        // 使用用户指定的局部区域边界
        right_boundary = x + limit_width;
    }

    // 预计算行高 (优先使用汉字高度以确保行间距一致性)
    uint16_t line_height = (font->cn_h > 0) ? font->cn_h : font->ascii_h;

    // === 4. 字符串遍历与渲染循环 ===
    while (*str)
    {
        // ----------------------------------------------------
        // 特殊控制字符处理：换行符 '\n'
        // ----------------------------------------------------
        if (*str == '\n')
        {
            cursor_x = x;            // X 轴回归至区域起始点
            cursor_y += line_height; // Y 轴下移一个 line_height 行高
            str++;                   // 移动指针至下一字符
            continue;                // 跳过后续绘制逻辑
        }

        // ----------------------------------------------------
        // 垂直越界检测
        // ----------------------------------------------------
        // 若当前行已超出屏幕物理底部，强制终止绘制，防止显存溢出
        if (cursor_y + line_height > TFT_LINE_NUMBER)
        {
            break;
        }

        // ----------------------------------------------------
        // 预判字符宽度与自动换行逻辑
        // ----------------------------------------------------
        uint16_t current_char_w;

        // 判断当前字符类型以获取对应宽度
        if (*str >= 0x20 && *str <= 0x7E)
        {
            current_char_w = font->ascii_w;
        }
        else
        {
            current_char_w = font->cn_w;
        }

        // 检查：当前光标 + 字符宽度 是否超出右侧边界
        if (cursor_x + current_char_w > right_boundary)
        {
            cursor_x = x;            // 换行：回归起始 X
            cursor_y += line_height; // 换行：Y 轴增加行高

            // 换行后再次进行垂直越界检查
            if (cursor_y + line_height > TFT_LINE_NUMBER)
            {
                break;
            }
        }

        // ----------------------------------------------------
        // 字模绘制逻辑
        // ----------------------------------------------------

        // 场景 A: 标准 ASCII 字符 (0x20 ~ 0x7E)
        if (*str >= 0x20 && *str <= 0x7E)
        {
            uint32_t char_idx = *str - 0x20;

            // 计算字模在数组中的字节偏移
            // 公式：((Width + 7) / 8) * Height
            uint32_t bytes_per_row = (font->ascii_w + 7) / 8;
            uint32_t char_size     = bytes_per_row * font->ascii_h;

            // 调用 DMA 底层绘图函数
            TFT_Draw_Glyph_Base(cursor_x,
                                cursor_y,
                                font->ascii_w,
                                font->ascii_h,
                                font->ascii_map + (char_idx * char_size),
                                color_fg,
                                color_bg);

            cursor_x += font->ascii_w; // 光标前进
            str++;                     // 指针前进 1 字节
        }
        // 场景 B: 汉字或多字节字符
        else
        {
            const uint8_t* p_target_data = NULL;
            int            match_len     = 0;

            // 检索汉字字库
            if (font->hzk_table && font->hzk_count > 0)
            {
                const uint8_t* p_base = (const uint8_t*) font->hzk_table;

                // 遍历映射表
                for (uint16_t i = 0; i < font->hzk_count; i++)
                {
                    // 获取条目指针与 Key
                    const uint8_t* p_entry = p_base + (i * font->hzk_struct_size);
                    const char*    key     = *(const char**) p_entry;
                    size_t         key_len = strlen(key);

                    // 字符串前缀匹配
                    if (strncmp(str, key, key_len) == 0)
                    {
                        // 匹配成功，计算数据偏移
                        p_target_data = p_entry + font->hzk_data_offset;
                        match_len     = key_len;
                        break;
                    }
                }
            }

            if (p_target_data)
            {
                // 绘制汉字字模
                TFT_Draw_Glyph_Base(
                    cursor_x, cursor_y, font->cn_w, font->cn_h, p_target_data, color_fg, color_bg);

                cursor_x += font->cn_w;
                str += match_len; // 跳过匹配的字节数 (UTF-8通常为3)
            }
            else
            {
                // 异常处理：字库未命中
                // 绘制红色色块占位，便于调试发现缺失字符
                TFT_Fill_Rect_DMA(cursor_x, cursor_y, font->cn_w, font->cn_h, RED);

                cursor_x += font->cn_w;
                str++; // 强制跳过 1 字节以尝试重新对齐
            }
        }
    }

    // 记录最终结束位置
    cursor_end_pos.end_x = cursor_x;
    cursor_end_pos.end_y = cursor_y;

exit:
    // === 5. 释放资源 ===
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }

    return cursor_end_pos;
}