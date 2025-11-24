#include "lcd_font.h"
#include "st7789.h"
#include <stdint.h>
#include <stddef.h>

// === 字库 16 的定义 ===
typedef struct
{
    uint16_t index;
    uint8_t  matrix[32]; // 16*16/8 = 32
} HZK_16_t;

extern const HZK_16_t HZK_16[];     // 你的数据数组
extern const uint8_t  ASCII_8x16[]; // 你的ASCII数组

// 这里的配置对应 16 字体
font_info_t font_16 = {
    .ascii_w         = 8,
    .ascii_h         = 16,
    .ascii_map       = ASCII_8x16,
    .cn_w            = 16,
    .cn_h            = 16,
    .hzk_table       = HZK_16,
    .hzk_count       = 24,               // 假设有100个字
    .hzk_struct_size = sizeof(HZK_16_t), // 关键！告诉函数一步跨多大
    .hzk_data_offset = 2,                // index 占 2 字节
    .hzk_data_size   = 32                // 数据占 32 字节
};
int main(void)
{
    ST7789_Init();
    TFT_clear();
    // 显示小字
    lcd_show_string(10, 10, "Small 16px", &font_16, WHITE, BLACK);
    lcd_show_string(10, 30, "朱家明喜欢叶祥梦9999", &font_16, RED, WHITE);

    while (1)
        ;
}