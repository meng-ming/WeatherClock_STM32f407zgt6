#include "FreeRTOS.h" // 第一位，必须最先包含
#include "task.h"
#include "semphr.h" // 信号量相关

#include "portmacro.h"

#include <errno.h>
#include <sys/reent.h>
#include <sys/unistd.h> // STDOUT_FILENO
#include "uart_driver.h"

// 导入外部全局变量
extern SemaphoreHandle_t g_mutex_log;
extern UART_Handle_t     g_debug_uart_handler;

static int uart_putchar(int ch)
{
    USART_TypeDef* usart = g_debug_uart_handler.USART_X;
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET); // 空等TXE
    USART_SendData(usart, (uint8_t) ch);
    // TC等待可选：高吞吐去掉，防卡；低速留
    // while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET);
    return ch;
}

// 当调用 printf("Hello") 时，C 库会先把数据格式化好，放到一个缓冲区里，然后一次性把这块内存的地址
// ptr 和长度 len 扔给 _write 函数。

// 要是用 fputc的话，调用 printf("Hello") 时，C 库内部会搞一个循环，每取到一个字符，就调用一次fputc

int _write(int file, const char* ptr, int len)
{
    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        if (g_mutex_log != NULL)
        {
            // 一直等到关锁才行
            xSemaphoreTakeRecursive(g_mutex_log, portMAX_DELAY);
        }

        for (int i = 0; i < len; ++i)
        {
            uart_putchar(ptr[i]);
            if (ptr[i] == '\n')
            {
                uart_putchar('\r'); // CRLF，串口工具友好
            }
        }

        // 释放锁
        if (g_mutex_log != NULL)
        {
            xSemaphoreGiveRecursive(g_mutex_log);
        }

        return len;
    }
    errno = EBADF;
    return -1;
}

int __io_putchar(int ch)
{
    return uart_putchar(ch);
}