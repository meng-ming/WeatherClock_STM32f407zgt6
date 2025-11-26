#ifndef __GLOBAL_VARIABLE_H
#define __GLOBAL_VARIABLE_H

#include <stdint.h>
#include "uart_driver.h"
#include "lcd_font.h"

// === 字库 16 的定义 ===
typedef struct
{
    uint16_t index;
    uint8_t  matrix[32]; // 16*16/8 = 32
} HZK_16_t;

extern const HZK_16_t HZK_16[];     // 你的数据数组
extern const uint8_t  ASCII_8x16[]; // 你的ASCII数组
extern const uint8_t  gImage_image[];

// 这里的配置对应 16 字体
extern font_info_t font_16;

// UART句柄：extern声明（.h里）
extern UART_Handle_t g_esp_uart_handler;
extern UART_Handle_t g_debug_uart_handler;

void Global_Variable_Init(void);

#endif