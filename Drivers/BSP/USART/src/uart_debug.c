#include <errno.h>
#include <sys/unistd.h> // for STDOUT_FILENO/STDERR_FILENO
#include "uart_debug.h"

static int uart_putchar(int ch)
{
    USART_TypeDef* usart = g_debug_uart_handler.USART_X;
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(usart, (uint8_t) ch);
    while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET)
        ;
    return ch;
}

// used by printf in newlib
int _write(int file, const char* ptr, int len)
{
    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        for (int i = 0; i < len; ++i)
        {
            uart_putchar(ptr[i]);
            // if (ptr[i] == '\n')
            //     uart_putchar('\r'); // optional: add CR after LF
        }
        return len;
    }
    errno = EBADF;
    return -1;
}

// optional alias if any code expects it
int __io_putchar(int ch)
{
    return uart_putchar(ch);
}
