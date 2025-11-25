#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO
#include "uart_driver.h"

extern UART_Handle_t g_debug_uart_handler;

static int uart_putchar(int ch)
{
    USART_TypeDef* usart = g_debug_uart_handler.USART_X;
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET)
        ; // 空等TXE
    USART_SendData(usart, (uint8_t) ch);
    // TC等待可选：高吞吐去掉，防卡；低速留
    // while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET);
    return ch;
}

int _write(int file, const char* ptr, int len)
{
    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        for (int i = 0; i < len; ++i)
        {
            uart_putchar(ptr[i]);
            if (ptr[i] == '\n')
            {
                uart_putchar('\r'); // CRLF，串口工具友好
            }
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