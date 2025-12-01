#ifndef __SYS_LOG_H
#define __SYS_LOG_H
#include <stdio.h>

// 定义日志开关 (1: 开启, 0: 关闭)
// 量产时改为 0，全世界都清净了
#define SYS_LOG_ENABLE 1
#define SYS_LOG_D_ENABLE 1

// 定义日志级别颜色 (ANSI 转义码，串口助手支持的话很酷)
#define LOG_CLR_RED "\033[31m"
#define LOG_CLR_GREEN "\033[32m"
#define LOG_CLR_YELLOW "\033[33m"
#define LOG_CLR_RESET "\033[0m"

#if SYS_LOG_ENABLE
// 普通日志
#define LOG_I(fmt, ...) printf("[INFO] " fmt LOG_CLR_RESET "\r\n", ##__VA_ARGS__)
// 警告日志 (黄色高亮)
#define LOG_W(fmt, ...) printf(LOG_CLR_YELLOW "[WARNING] " fmt LOG_CLR_RESET "\r\n", ##__VA_ARGS__)
// 错误日志 (红色高亮)
#define LOG_E(fmt, ...) printf(LOG_CLR_RED "[ERROR] " fmt LOG_CLR_RESET "\r\n", ##__VA_ARGS__)
// 原始打印 (不换行，不带头)
#define LOG_RAW(fmt, ...) printf(fmt, ##__VA_ARGS__)

#else
// 关闭时，宏展开为空，完全不占代码空间
#define LOG_I(fmt, ...)
#define LOG_W(fmt, ...)
#define LOG_E(fmt, ...)
#define LOG_RAW(fmt, ...)
#endif

#if SYS_LOG_D_ENABLE
// 调试日志
#define LOG_D(fmt, ...) printf(LOG_CLR_GREEN "[DEBUG] " fmt LOG_CLR_RESET "\r\n", ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...)
#endif

#endif