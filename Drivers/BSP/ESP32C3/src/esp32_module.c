/**
 * @file    esp32_module.c
 * @brief   ESP32 AT 模组驱动实现 (高健壮性版)
 */

#include "esp32_module.h"
#include "BSP_Tick_Delay.h"
#include <string.h>
#include <stdio.h>

// 内部静态变量：保存 UART 句柄
static UART_Handle_t* g_module_uart = NULL;

// === 异常处理宏 ===
// 检查模块是否已初始化
#define CHECK_INIT()                                                                               \
    do                                                                                             \
    {                                                                                              \
        if (g_module_uart == NULL)                                                                 \
        {                                                                                          \
            printf("[ESP Error] Driver Not Initialized!\r\n");                                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

// 检查指针参数是否为空
#define CHECK_PARAM(ptr)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if ((ptr) == NULL)                                                                         \
        {                                                                                          \
            printf("[ESP Error] Invalid Parameter (NULL)\r\n");                                    \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

/**
 * @brief  初始化 ESP32 模块
 */
void ESP_Module_Init(UART_Handle_t* uart_handler)
{
    if (uart_handler == NULL)
    {
        printf("[ESP Error] Init failed: Handle is NULL\r\n");
        return;
    }

    g_module_uart = uart_handler;

    // 硬件层初始化 (UART_Init 内部也要有健壮性，但这里我们负责调用)
    UART_Init(g_module_uart);

    // 清理环境
    UART_RingBuf_Clear(g_module_uart);

    // printf("[ESP Info] Module Init OK\r\n");
}

/**
 * @brief  发送 AT 指令 (带重试和异常处理)
 */
bool ESP_Send_AT(const char* cmd, const char* expect_resp, uint32_t timeout_ms, uint8_t retry)
{
    // 1. 异常检查：未初始化或指令为空
    CHECK_INIT();
    CHECK_PARAM(cmd);
    // expect_resp 允许为 NULL，表示不等待特定响应

    char     line_buf[512];
    uint32_t start_tick;
    bool     success = false;

    // 2. 重试循环
    for (int i = 0; i <= retry; i++)
    {
        // 每次重试前清空缓冲区，防止数据粘连
        UART_RingBuf_Clear(g_module_uart);

        // 发送指令
        UART_Send_AT_Command(g_module_uart, cmd);

        // 如果不需要等待响应，直接算成功 (特殊用途)
        if (expect_resp == NULL)
        {
            success = true;
            break;
        }

        // 3. 接收循环
        start_tick          = BSP_GetTick_ms();
        bool response_found = false;

        while ((BSP_GetTick_ms() - start_tick) < timeout_ms)
        {
            // 尝试读一行 (短超时轮询)
            if (UART_RingBuf_ReadLine(g_module_uart, line_buf, sizeof(line_buf), 20) > 0)
            {
                // 调试打印 (排查问题时打开)
                // printf("[ESP RX] %s\r\n", line_buf);

                // A. 成功匹配
                if (strstr(line_buf, expect_resp) != NULL)
                {
                    response_found = true;
                    break;
                }

                // B. 显式错误 (收到 ERROR) -> 立即终止本次等待，进入下一次重试
                if (strstr(line_buf, "ERROR") != NULL)
                {
                    // printf("[ESP Error] Cmd '%s' -> ERROR\r\n", cmd);
                    break;
                }

                // C. 异常状态 (收到 busy p...) -> 稍作延时再继续等
                if (strstr(line_buf, "busy") != NULL)
                {
                    BSP_Delay_ms(100);
                }
            }
        }

        if (response_found)
        {
            success = true;
            break; // 任务完成，跳出重试循环
        }

        // 本次尝试失败，准备重试
        if (i < retry)
        {
            // printf("[ESP Info] Retry %d/%d: %s\r\n", i+1, retry, cmd);
            BSP_Delay_ms(500); // 避让时间，给模块恢复
        }
    }

    return success;
}

/**
 * @brief  连接 WiFi (带参数检查)
 */
bool ESP_WiFi_Connect(const char* ssid, const char* pwd, uint8_t retry)
{
    CHECK_INIT();
    CHECK_PARAM(ssid);
    CHECK_PARAM(pwd);

    char cmd_buf[128];

    // 1. 设置模式 (重试1次)
    if (!ESP_Send_AT("AT+CWMODE=1", "OK", 1000, 1))
    {
        printf("[ESP Error] Set CWMODE failed\r\n");
        return false;
    }

    // 2. 格式化指令 (使用 snprintf 防止缓冲区溢出)
    int len = snprintf(cmd_buf, sizeof(cmd_buf), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (len < 0 || len >= sizeof(cmd_buf))
    {
        printf("[ESP Error] SSID/PWD too long!\r\n");
        return false;
    }

    // 3. 发送连接 (长超时 15s)
    // 成功通常返回 OK，有时返回 WIFI GOT IP，视固件而定，这里以 OK 为准
    if (ESP_Send_AT(cmd_buf, "OK", 15000, retry))
    {
        return true;
    }

    printf("[ESP Error] WiFi Connect Failed after %d retries\r\n", retry);
    return false;
}