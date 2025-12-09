/**
 * @file    uart_debug.c
 * @brief   串口重定向与调试输出实现
 * @note    本模块负责将 C 标准库的 printf/fprintf 输出重定向到指定的 UART 端口。
 * 核心特性：
 * 1. 实现了 _write 底层桩函数，支持 Newlib 标准库。
 * 2. 集成了 FreeRTOS 递归互斥锁 (Recursive Mutex)，保证多任务并发打印不乱序。
 * 3. 自动处理换行符转换 (\n -> \r\n)，适配 Windows 串口调试助手。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"
#include "semphr.h" // 信号量相关

#include "portmacro.h"

#include <errno.h>
#include <sys/reent.h>
#include <sys/unistd.h> // 包含 STDOUT_FILENO 定义
#include "uart_driver.h"

/* ==================================================================
 * 外部依赖 (External Variables)
 * ================================================================== */

// 日志互斥锁 (由 main.c 创建)
extern SemaphoreHandle_t g_mutex_log;

// 调试串口句柄 (通常指向 USART1)
extern UART_Handle_t g_debug_uart_handler;

/* ==================================================================
 * 内部辅助函数 (Private Functions)
 * ================================================================== */

/**
 * @brief  向调试串口发送单个字符 (阻塞式)
 * @note   该函数为裸机层实现，不包含互斥锁保护。
 * 内部通过轮询 TXE 标志位等待发送缓冲区空闲。
 * @param  ch: 要发送的字符
 * @retval int: 发送的字符值
 */
static int uart_putchar(int ch)
{
    USART_TypeDef* usart = g_debug_uart_handler.USART_X;

    // 等待发送数据寄存器空 (Transmit Data Register Empty)
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET);

    // 发送数据
    USART_SendData(usart, (uint8_t) ch);

    // TC (Transmission Complete) 等待可选：
    // - 高吞吐场景去掉，防止阻塞过久影响 CPU 效率
    // - 低速或低功耗场景保留，确保最后一位移位完成
    // while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET);

    return ch;
}

/* ==================================================================
 * 标准库重定向接口 (Newlib Stubs)
 * ================================================================== */

/**
 * @brief  重写 C 库底层 write 函数 (实现 printf 重定向)
 * @note   当调用 printf("Hello") 时，C 库会先把数据格式化到内部缓冲区，
 * 然后一次性调用 _write 传输整块数据。这比 fputc 逐字调用效率更高。
 * 本函数加入了 FreeRTOS 递归锁，实现了线程安全的原子打印。
 * @param  file: 文件描述符 (只处理 stdout/stderr)
 * @param  ptr:  数据缓冲区指针
 * @param  len:  数据长度
 * @retval int:  实际写入的字节数，失败返回 -1
 */
int _write(int file, const char* ptr, int len)
{
    // 仅处理标准输出 (stdout) 和标准错误 (stderr)
    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        // === 进入临界区 ===
        // 使用递归锁，允许同一个任务多次获取 (防止在中断或回调中死锁)
        // portMAX_DELAY 表示一直等待直到获取锁
        if (g_mutex_log != NULL)
        {
            xSemaphoreTakeRecursive(g_mutex_log, portMAX_DELAY);
        }

        for (int i = 0; i < len; ++i)
        {
            uart_putchar(ptr[i]);

            // 自动补全回车符 (CR)，适配 Windows 换行习惯
            if (ptr[i] == '\n')
            {
                uart_putchar('\r');
            }
        }

        // === 退出临界区 ===
        if (g_mutex_log != NULL)
        {
            xSemaphoreGiveRecursive(g_mutex_log);
        }

        return len;
    }

    // 非标准输出，返回“坏文件描述符”错误
    errno = EBADF;
    return -1;
}

/**
 * @brief  STM32CubeIDE/GCC 某些标准库版本依赖的底层输出函数
 * @note   作为 _write 的补充，确保兼容性
 */
int __io_putchar(int ch)
{
    return uart_putchar(ch);
}