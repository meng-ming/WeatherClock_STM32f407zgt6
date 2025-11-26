#ifndef __UART_HANDLE_VARIABLE_H
#define __UART_HANDLE_VARIABLE_H

#include "uart_driver.h"

/* ==================================================================
 * 外部引用声明 (External Declarations)
 * ================================================================== */

/**
 * @brief ESP32 串口句柄 (USART2)
 */
extern UART_Handle_t g_esp_uart_handler;

/**
 * @brief Debug 串口句柄 (USART1)
 */
extern UART_Handle_t g_debug_uart_handler;

#endif /* __UART_HANDLE_VARIABLE_H */