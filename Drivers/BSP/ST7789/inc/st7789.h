#ifndef __ST7789_H
#define __ST7789_H

#include "stm32f4xx.h"

// ====================================================================
// 硬件引脚定义 (根据你的原理图)
// ====================================================================
#define ST7789_SPI_PERIPH SPI2
#define ST7789_SPI_SCK_PORT GPIOB
#define ST7789_SPI_SCK_PIN GPIO_Pin_10
#define ST7789_SPI_MOSI_PORT GPIOC
#define ST7789_SPI_MOSI_PIN GPIO_Pin_3
#define ST7789_SPI_MISO_PORT GPIOC
#define ST7789_SPI_MISO_PIN GPIO_Pin_2

#define ST7789_CS_PORT GPIOC
#define ST7789_CS_PIN GPIO_Pin_4
#define ST7789_DC_PORT GPIOC
#define ST7789_DC_PIN GPIO_Pin_5
#define ST7789_RST_PORT GPIOE
#define ST7789_RST_PIN GPIO_Pin_3
#define ST7789_BL_PORT GPIOB
#define ST7789_BL_PIN GPIO_Pin_15

// ====================================================================
// 引脚操作宏
// ====================================================================
#define LCD_CS_SET() GPIO_SetBits(ST7789_CS_PORT, ST7789_CS_PIN)
#define LCD_CS_CLR() GPIO_ResetBits(ST7789_CS_PORT, ST7789_CS_PIN)

#define LCD_DC_SET() GPIO_SetBits(ST7789_DC_PORT, ST7789_DC_PIN)
#define LCD_DC_CLR() GPIO_ResetBits(ST7789_DC_PORT, ST7789_DC_PIN)

#define LCD_RST_SET() GPIO_SetBits(ST7789_RST_PORT, ST7789_RST_PIN)
#define LCD_RST_CLR() GPIO_ResetBits(ST7789_RST_PORT, ST7789_RST_PIN)

#define LCD_BL_SET() GPIO_SetBits(ST7789_BL_PORT, ST7789_BL_PIN)
#define LCD_BL_CLR() GPIO_ResetBits(ST7789_BL_PORT, ST7789_BL_PIN)

// ====================================================================
// 屏幕参数
// ====================================================================
#define TFT_COLUMN_NUMBER 240
#define TFT_LINE_NUMBER 320

// ====================================================================
// 颜色定义 (RGB565)
// ====================================================================
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40
#define BRRED 0XFC07
#define GRAY 0X8430

/**
 * @brief  RGB888 转 RGB565 宏
 */
#define TFT_RGB(R, G, B)                                                                           \
    ((uint16_t) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xF8) >> 3)))

// ====================================================================
// 函数声明
// ====================================================================
void ST7789_Init(void);
void ST7789_Hardware_Init(void);
void TFT_full(uint16_t color);
void TFT_clear(void);
void TFT_Fill_Rect(
    uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);
void display_char16_16(unsigned int  x,
                       unsigned int  y,
                       unsigned long color,
                       unsigned char word_serial_number);

void TFT_ShowChar_1632(uint16_t x, uint16_t y, char chr, uint16_t color, uint16_t bgcolor);
void TFT_ShowString_1632(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bgcolor);

#endif