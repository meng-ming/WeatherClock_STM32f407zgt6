/**
 * @file    st7789.h
 * @brief   ST7789 LCD 屏幕底层驱动接口
 * @note    负责 SPI 初始化、屏幕配置、基础绘图（画点、填充）。
 *          字符显示等高级功能建议上层实现，保持驱动层纯净。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __ST7789_H
#define __ST7789_H

#include "stm32f4xx.h"
#include <stdint.h>

/* ==================================================================
 * 1. 硬件引脚定义 (Hardware Pin Definitions)
 * ================================================================== */

/**
 * @brief SPI 外设定义
 */
#define ST7789_SPI_PERIPH SPI2

/**
 * @brief SPI SCK 引脚定义
 */
#define ST7789_SPI_SCK_PORT GPIOB
#define ST7789_SPI_SCK_PIN GPIO_Pin_10

/**
 * @brief SPI MOSI 引脚定义
 */
#define ST7789_SPI_MOSI_PORT GPIOC
#define ST7789_SPI_MOSI_PIN GPIO_Pin_3

/**
 * @brief SPI MISO 引脚定义
 */
#define ST7789_SPI_MISO_PORT GPIOC
#define ST7789_SPI_MISO_PIN GPIO_Pin_2

/**
 * @brief LCD 控制引脚定义
 */
#define ST7789_CS_PORT GPIOC
#define ST7789_CS_PIN GPIO_Pin_4
#define ST7789_DC_PORT GPIOC
#define ST7789_DC_PIN GPIO_Pin_5
#define ST7789_RST_PORT GPIOE
#define ST7789_RST_PIN GPIO_Pin_3
#define ST7789_BL_PORT GPIOB
#define ST7789_BL_PIN GPIO_Pin_15

/* ==================================================================
 * 2. 引脚操作宏 (Pin Operation Macros)
 * ================================================================== */

/**
 * @brief CS 片选宏
 */
#define LCD_CS_SET() GPIO_SetBits(ST7789_CS_PORT, ST7789_CS_PIN)
#define LCD_CS_CLR() GPIO_ResetBits(ST7789_CS_PORT, ST7789_CS_PIN)

/**
 * @brief DC 数据/命令宏
 */
#define LCD_DC_SET() GPIO_SetBits(ST7789_DC_PORT, ST7789_DC_PIN)
#define LCD_DC_CLR() GPIO_ResetBits(ST7789_DC_PORT, ST7789_DC_PIN)

/**
 * @brief RST 复位宏
 */
#define LCD_RST_SET() GPIO_SetBits(ST7789_RST_PORT, ST7789_RST_PIN)
#define LCD_RST_CLR() GPIO_ResetBits(ST7789_RST_PORT, ST7789_RST_PIN)

/**
 * @brief BL 背光宏
 */
#define LCD_BL_SET() GPIO_SetBits(ST7789_BL_PORT, ST7789_BL_PIN)
#define LCD_BL_CLR() GPIO_ResetBits(ST7789_BL_PORT, ST7789_BL_PIN)

/* ==================================================================
 * 3. 屏幕参数定义 (Display Parameters)
 * ================================================================== */

/**
 * @brief 屏幕分辨率定义
 */
#define TFT_COLUMN_NUMBER 240 ///< 屏幕宽度 (像素)
#define TFT_LINE_NUMBER 320   ///< 屏幕高度 (像素)

/* ==================================================================
 * 4. 颜色定义 (RGB565 Color Definitions)
 * ================================================================== */

/**
 * @brief 常用 RGB565 颜色宏
 */
#define WHITE 0xFFFF   ///< 白色
#define BLACK 0x0000   ///< 黑色
#define BLUE 0x001F    ///< 蓝色
#define RED 0xF800     ///< 红色
#define GREEN 0x07E0   ///< 绿色
#define YELLOW 0xFFE0  ///< 黄色
#define GRAY 0X8430    ///< 灰色
#define MAGENTA 0xF81F ///< 品红色
#define CYAN 0x7FFF    ///< 青色

/* ==================================================================
 * 5. DMA 硬件配置 (DMA Hardware Configuration)
 * ================================================================== */

/**
 * @brief DMA 配置宏 (SPI2_TX 对应 DMA1 Stream 4 Channel 0)
 */
#define LCD_DMA_STREAM DMA1_Stream4
#define LCD_DMA_CHANNEL DMA_Channel_0
#define LCD_DMA_CLK RCC_AHB1Periph_DMA1
#define LCD_DMA_FLAG_TC DMA_FLAG_TCIF4

/* ==================================================================
 * 6. 颜色转换宏 (Color Conversion Macros)
 * ================================================================== */

/**
 * @brief RGB888 转 RGB565 宏
 * @note  输入 R/G/B (0~255)，输出 16位颜色值
 */
#define TFT_RGB(R, G, B)                                                                           \
    ((uint16_t) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xF8) >> 3)))

/* ==================================================================
 * 7. 接口函数声明 (Interface Function Declarations)
 * ================================================================== */

/**
 * @brief  初始化 ST7789 屏幕
 * @note   包含 GPIO、SPI 初始化及屏幕上电序列配置
 * @retval None
 */
void ST7789_Init(void);

/**
 * @brief  SPI 发送一个字节
 * @note   阻塞式发送单个字节
 * @param  byte: 要发送的数据 (8位)
 * @retval None
 */
void ST7789_SPI_SendByte(uint8_t byte);

/**
 * @brief  发送 LCD 命令
 * @note   设置 DC=0，发送命令字节
 * @param  cmd: 命令字节
 * @retval None
 */
void TFT_SEND_CMD(uint8_t cmd);

/**
 * @brief  发送 LCD 数据
 * @note   设置 DC=1，发送数据字节
 * @param  data: 数据字节
 * @retval None
 */
void TFT_SEND_DATA(uint8_t data);

/**
 * @brief  全屏填充颜色
 * @note   使用阻塞式填充整个屏幕
 * @param  color: RGB565 颜色值
 * @retval None
 */
void TFT_full(uint16_t color);

/**
 * @brief  清屏 (填充白色)
 * @note   快速清屏为白色
 * @retval None
 */
void TFT_clear(void);

/**
 * @brief  在指定区域填充颜色 (使用左上角坐标 + 长宽)
 * @note   阻塞式矩形填充，支持边界裁剪
 * @param  x_start: 起始 X 坐标 (0 ~ TFT_COLUMN_NUMBER-1)
 * @param  y_start: 起始 Y 坐标 (0 ~ TFT_LINE_NUMBER-1)
 * @param  w:       填充宽度 (像素)
 * @param  h:       填充高度 (像素)
 * @param  color:   填充颜色 (RGB565)
 * @retval None
 */
void TFT_Fill_Rect(uint16_t x_start, uint16_t y_start, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  使用 DMA 全屏/区域填充颜色 (高性能版)
 * @note   利用 DMA 源地址不自增特性 + SPI 16位模式，实现极速刷屏
 *         CPU 在传输期间处于忙等待 (FreeRTOS下可优化为挂起)
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  w: 宽度 (像素)
 * @param  h: 高度 (像素)
 * @param  color: RGB565 颜色
 * @retval None
 */
void TFT_Fill_Rect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  使用 DMA 全屏填充颜色
 * @note   DMA 封装的全屏填充
 * @param  color: RGB565 颜色值
 * @retval None
 */
void TFT_full_DMA(uint16_t color);

/**
 * @brief  DMA 全屏清屏 (封装函数)
 * @note   DMA 快速清屏为指定颜色
 * @param  color: RGB565 颜色值
 * @retval None
 */
void TFT_Clear_DMA(uint16_t color);

#endif /* __ST7789_H */