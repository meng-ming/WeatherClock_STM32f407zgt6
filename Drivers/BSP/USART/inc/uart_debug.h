#ifndef __UART_DEBUG_H
#define __UART_DEBUG_H

#include "stdio.h"
#include "uart_driver.h"

// 外部引用调试句柄
extern UART_Handle_t g_debug_uart_handler;

/**
 * @brief C 库重定向钩子：printf 函数最终调用的字符输出函数
 * @note 必须实现这个函数，才能让 C 标准库的 printf 知道该往哪儿输出
 */
int fputc(int ch, FILE* f);

/**
 * @brief 你的自定义调试输出函数 (现在可以直接使用标准的 printf)
 */
int printf_usart(const char* format, ...);
#endif