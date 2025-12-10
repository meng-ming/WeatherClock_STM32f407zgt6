/**
 * @file    st7789.h
 * @brief   ST7789 LCD 屏幕底层驱动接口 (DMA 高性能版)
 * @note    本模块遵循“机制与策略分离”原则，仅负责底层的像素传输和硬件控制。
 * 高级绘图功能（如字符、画圆、UI控件）请在 Middleware 或 APP 层实现。
 * 核心特性：
 * 1. DMA 零拷贝传输：极大降低 CPU 占用。
 * 2. 自动分包机制：支持超过 65535 像素的大块数据传输。
 * 3. 线程安全设计：核心接口集成递归锁 (需 OS 支持)。
 * @author  meng-ming
 * @version 2.0
 * @date    2025-12-09
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
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
#define GRAY 0x8430    ///< 灰色
#define MAGENTA 0xF81F ///< 品红色
#define CYAN 0x7FFF    ///< 青色

/* ==================================================================
 * 5. DMA 硬件配置 (DMA Hardware Configuration)
 * ================================================================== */

/**
 * @brief DMA 配置宏 (SPI2_TX 对应 DMA1 Stream 4 Channel 0)
 * @note  F407系列SPI2_TX固定映射到DMA1_Stream4 Channel0
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
 * @param R Red (0~255)
 * @param G Green (0~255)
 * @param B Blue (0~255)
 */
#define TFT_RGB(R, G, B)                                                                           \
    ((uint16_t) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xF8) >> 3)))

/* ==================================================================
 * 7. 接口函数声明 (Interface Function Declarations)
 * ================================================================== */

/**
 * @brief  初始化 ST7789 屏幕
 * @note   包含 GPIO、SPI 初始化及屏幕上电序列配置
 *         必须在任何绘图操作前调用一次
 * @retval None
 */
void ST7789_Init(void);

/**
 * @brief  SPI 发送一个字节
 * @note   阻塞式发送单个字节，用于命令和参数发送
 * @param  byte 要发送的数据 (8位)
 * @retval None
 */
void ST7789_SPI_SendByte(uint8_t byte);

/**
 * @brief  发送 LCD 命令
 * @note   设置 DC=0，发送命令字节
 * @param  cmd 命令字节
 * @retval None
 */
void TFT_SEND_CMD(uint8_t cmd);

/**
 * @brief  发送 LCD 数据
 * @note   设置 DC=1，发送数据字节
 * @param  data 数据字节
 * @retval None
 */
void TFT_SEND_DATA(uint8_t data);

/**
 * @brief  使用 DMA 区域填充颜色
 * @note   高性能填充，支持任意矩形区域
 *         内部使用DMA + SPI 16位模式，CPU空闲率高
 *         必须在ST7789_Init()后调用
 * @param  x 起始 X 坐标 (0 ~ TFT_COLUMN_NUMBER-1)
 * @param  y 起始 Y 坐标 (0 ~ TFT_LINE_NUMBER-1)
 * @param  w 填充宽度 (像素)
 * @param  h 填充高度 (像素)
 * @param  color RGB565 颜色值
 * @retval None
 */
void TFT_Fill_Rect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  使用 DMA 全屏填充颜色
 * @note   封装的全屏填充接口
 * @param  color RGB565 颜色值
 * @retval None
 */
void TFT_Full_DMA(uint16_t color);

/**
 * @brief  使用 DMA 全屏清屏
 * @note   快速清屏为指定颜色（通常为白色或黑色背景）
 * @param  color RGB565 颜色值
 * @retval None
 */
void TFT_Clear_DMA(uint16_t color);

/**
 * @brief  DMA 发送图片数据 (阻塞式等待 DMA 完成，但速度极快)
 * @param  x,y: 起始坐标
 * @param  w,h: 宽, 高
 * @param  pData: 图片数据指针 (RGB565 字节流)
 */
void TFT_ShowImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* pData);

#endif /* __ST7789_H */