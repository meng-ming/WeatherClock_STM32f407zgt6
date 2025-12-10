/**
 * @file    uart_handle_variable.h
 * @brief   UART 句柄变量声明
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#ifndef __UART_HANDLE_VARIABLE_H
#define __UART_HANDLE_VARIABLE_H

#include "uart_driver.h"

/* ==================================================================
 * 1. 外部引用声明 (External Declarations)
 * ================================================================== */

/**
 * @brief ESP32 串口句柄 (USART2)
 * @note  用于WiFi模块通信
 */
extern UART_Handle_t g_esp_uart_handler;

/**
 * @brief Debug 串口句柄 (USART1)
 * @note  用于系统调试输出
 */
extern UART_Handle_t g_debug_uart_handler;

#endif /* __UART_HANDLE_VARIABLE_H */