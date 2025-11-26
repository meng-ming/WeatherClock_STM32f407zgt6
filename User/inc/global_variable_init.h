/**
 * @file    global_variable_init.h
 * @brief   全局变量管理与初始化模块
 * @note    负责管理项目中跨模块使用的全局变量（如字库、图片资源、UART句柄等）。
 * 提供统一的初始化接口，确保系统启动时所有全局资源处于已知状态。
 */

#ifndef __GLOBAL_VARIABLE_H
#define __GLOBAL_VARIABLE_H

#include <stdint.h>
#include "uart_driver.h"
#include "lcd_font.h"

/* ==================================================================
 * 字库资源定义
 * ================================================================== */

/**
 * @brief 16点阵汉字结构体
 */
typedef struct
{
    uint16_t index;      ///< 汉字索引 (或内码)
    uint8_t  matrix[32]; ///< 点阵数据 (16*16/8 = 32字节)
} HZK_16_t;

/* 外部引用字库资源 (定义在对应的 font/image 源文件中) */
extern const HZK_16_t HZK_16[];       ///< 汉字字库数组
extern const uint8_t  ASCII_8x16[];   ///< 8x16 ASCII 字符数组
extern const uint8_t  gImage_image[]; ///< 测试图片数据

/**
 * @brief 全局 16 点阵字体配置对象
 * @note  用于 LCD 显示函数调用时指定字体格式
 */
extern font_info_t font_16;

/* ==================================================================
 * 硬件句柄定义
 * ================================================================== */

/**
 * @brief ESP32 通信专用串口句柄 (USART2)
 */
extern UART_Handle_t g_esp_uart_handler;

/**
 * @brief 调试日志专用串口句柄 (USART1)
 */
extern UART_Handle_t g_debug_uart_handler;

/* ==================================================================
 * 接口函数
 * ================================================================== */

/**
 * @brief  全局变量初始化
 * @note   负责初始化应用层全局变量的默认值（如字体配置、状态标志等）。
 * ***注意：底层的硬件句柄状态（如 ringbuffer index）应由驱动层 Init 函数负责，此处不再重复处理***。
 * @retval None
 */
void Global_Variable_Init(void);

#endif /* __GLOBAL_VARIABLE_H */