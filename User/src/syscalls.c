/**
 * @file    syscalls.c
 * @brief   STM32 Newlib 桩函数实现 (用于消除链接器警告)
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// 1. 声明环境指针 (Newlib 需要)
char*  __env[1] = {0};
char** environ  = __env;

// 2. 退出函数
void _exit(int status)
{
    (void) status;
    while (1); // 死循环，防止复位
}

// 3. 关闭文件
int _close(int file)
{
    (void) file;
    return -1;
}

// 4. 文件状态
int _fstat(int file, struct stat* st)
{
    (void) file;
    st->st_mode = S_IFCHR; // 标记为字符设备
    return 0;
}

// 5. 判断是否为终端
int _isatty(int file)
{
    (void) file;
    return 1; // 假装是终端，允许 stdio 缓冲优化
}

// 6. 文件定位
int _lseek(int file, int ptr, int dir)
{
    (void) file;
    (void) ptr;
    (void) dir;
    return 0;
}

// 7. 读文件 (这是 stdin 的底层，如果没做串口输入，这里留空即可)
int _read(int file, char* ptr, int len)
{
    (void) file;
    (void) ptr;
    (void) len;
    return 0;
}

// 8. 获取进程 ID
int _getpid(void)
{
    return 1;
}

// 9. 发送信号
int _kill(int pid, int sig)
{
    (void) pid;
    (void) sig;
    errno = EINVAL;
    return -1;
}

// 注意：_write 函数通常在 uart_driver.c 中实现了 (用于 printf 重定向)
// 如果这里再写 _write 会冲突。