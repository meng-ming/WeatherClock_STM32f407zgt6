/**
 * @file    st7789.h
 * @brief   ST7789 LCD 屏幕底层驱动接口
 * @note    负责 SPI 初始化、屏幕配置、基础绘图（画点、填充）。
 * 字符显示等高级功能建议上层实现，保持驱动层纯净。
 */

#ifndef __ST7789_H
#define __ST7789_H

#include "stm32f4xx.h"
#include <stdint.h>

// ====================================================================
// 硬件引脚定义 (保持不变)
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
#define RED 0xF800
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define GRAY 0X8430
#define MAGENTA 0xF81F
#define CYAN 0x7FFF

// RGB888 转 RGB565 宏
#define TFT_RGB(R, G, B)                                                                           \
    ((uint16_t) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xF8) >> 3)))

// ====================================================================
// 接口函数声明
// ====================================================================

/**
 * @brief  初始化 ST7789 屏幕
 * @note   包含 GPIO、SPI 初始化及屏幕上电序列配置
 * @retval None
 */
void ST7789_Init(void);

/**
 * @brief  SPI 发送一个字节
 * @param  byte: 要发送的数据
 */
void ST7789_SPI_SendByte(uint8_t byte);

/**
 * @brief  发送 LCD 命令
 * @param  cmd: 命令字节
 */
void TFT_SEND_CMD(uint8_t cmd);

/**
 * @brief  发送 LCD 数据
 * @param  data: 数据字节
 */
void TFT_SEND_DATA(uint8_t data);

/**
 * @brief  全屏填充颜色
 * @param  color: RGB565 颜色值
 */
void TFT_full(uint16_t color);

/**
 * @brief  清屏 (填充白色)
 */
void TFT_clear(void);

/**
 * @brief  在指定区域填充颜色
 * @param  x_start: 起始 X 坐标
 * @param  y_start: 起始 Y 坐标
 * @param  x_end:   结束 X 坐标
 * @param  y_end:   结束 Y 坐标
 * @param  color:   填充颜色
 */
void TFT_Fill_Rect(
    uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);

#endif /* __ST7789_H */