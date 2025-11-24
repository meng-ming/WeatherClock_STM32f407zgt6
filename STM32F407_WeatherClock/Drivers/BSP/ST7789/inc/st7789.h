#ifndef __ST7789_H
#define __ST7789_H

#include "stm32f4xx.h"
#include <stdint.h>

// =======================================================
// 1. 颜色定义 (RGB565 格式)
// =======================================================
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define WHITE 0xFFFF
#define BLACK 0x0000
#define YELLOW 0xFFE0
#define GRAY0 0xEF7D
#define GRAY1 0x8410
#define GRAY2 0x4208

// =======================================================
// 2. 屏幕参数
// =======================================================
#define TFT_COLUMN_NUMBER 240
#define TFT_LINE_NUMBER 320

// =======================================================
// 3. 硬件接口定义 (STM32F407 SPI2)
// =======================================================
// SPI 接口
#define ST7789_SPI_PERIPH SPI2
#define ST7789_SPI_CLK_FUN RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE)
#define ST7789_SPI_SCK_PORT GPIOB
#define ST7789_SPI_SCK_PIN GPIO_Pin_10
#define ST7789_SPI_MOSI_PORT GPIOC
#define ST7789_SPI_MOSI_PIN GPIO_Pin_3
#define ST7789_SPI_MISO_PORT GPIOC // 不使用但定义，防止悬空
#define ST7789_SPI_MISO_PIN GPIO_Pin_2

// 控制引脚
#define ST7789_CS_PORT GPIOC
#define ST7789_CS_PIN GPIO_Pin_4
#define ST7789_DC_PORT GPIOC
#define ST7789_DC_PIN GPIO_Pin_5
#define ST7789_RST_PORT GPIOE
#define ST7789_RST_PIN GPIO_Pin_3
#define ST7789_BL_PORT GPIOB
#define ST7789_BL_PIN GPIO_Pin_15

// =======================================================
// 4. GPIO 操作宏 (使用 SPL 库函数)
// =======================================================
#define LCD_CS_CLR() GPIO_ResetBits(ST7789_CS_PORT, ST7789_CS_PIN)
#define LCD_CS_SET() GPIO_SetBits(ST7789_CS_PORT, ST7789_CS_PIN)

#define LCD_DC_CLR() GPIO_ResetBits(ST7789_DC_PORT, ST7789_DC_PIN)
#define LCD_DC_SET() GPIO_SetBits(ST7789_DC_PORT, ST7789_DC_PIN)

#define LCD_RST_CLR() GPIO_ResetBits(ST7789_RST_PORT, ST7789_RST_PIN)
#define LCD_RST_SET() GPIO_SetBits(ST7789_RST_PORT, ST7789_RST_PIN)

#define LCD_BL_CLR() GPIO_ResetBits(ST7789_BL_PORT, ST7789_BL_PIN)
#define LCD_BL_SET() GPIO_SetBits(ST7789_BL_PORT, ST7789_BL_PIN)

// =======================================================
// 5. 函数声明
// =======================================================
void ST7789_Hardware_Init(void); // 硬件初始化
void ST7789_Init(void);          // 屏幕命令初始化
void TFT_full(uint16_t color);   // 全屏填充颜色
void TFT_clear(void);            // 清屏(白色)
void display_char16_16(unsigned int  x,
                       unsigned int  y,
                       unsigned long color,
                       unsigned char word_serial_number); // 显示汉字

#endif