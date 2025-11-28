/**
 * @file    esp32_module.c
 * @brief   ESP32 AT 模组驱动实现 (高健壮性版)
 */

#include "esp32_module.h"
#include "BSP_Tick_Delay.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sys_log.h"
#include "uart_driver.h"
#include "bsp_rtc.h"

// 内部静态变量：保存 UART 句柄
static UART_Handle_t* g_module_uart = NULL;

// 月份缩写映射表
static const char* MONTH_STR[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// === 异常处理宏 ===
// 检查模块是否已初始化
#define CHECK_INIT()                                                                               \
    do                                                                                             \
    {                                                                                              \
        if (g_module_uart == NULL)                                                                 \
        {                                                                                          \
            LOG_E("[ESP Error] Driver Not Initialized!");                                          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

// 检查指针参数是否为空
#define CHECK_PARAM(ptr)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if ((ptr) == NULL)                                                                         \
        {                                                                                          \
            LOG_E("[ESP Error] Invalid Parameter (NULL)");                                         \
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
        LOG_E("[ESP Error] Init failed: Handle is NULL");
        return;
    }

    g_module_uart = uart_handler;

    // 硬件层初始化 (UART_Init 内部也要有健壮性，但这里我们负责调用)
    UART_Init(g_module_uart);

    // 清理环境
    UART_RingBuf_Clear(g_module_uart);

    LOG_I("[ESP Info] Module Init OK");
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
                // LOG_D("[ESP RX] %s", line_buf);

                // A. 成功匹配
                if (strstr(line_buf, expect_resp) != NULL)
                {
                    response_found = true;
                    break;
                }

                // B. 显式错误 (收到 ERROR) -> 立即终止本次等待，进入下一次重试
                if (strstr(line_buf, "ERROR") != NULL)
                {
                    LOG_E("[ESP Error] Cmd '%s' -> ERROR", cmd);
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
            LOG_D("[ESP Info] Retry %d/%d: %s\r\n", i + 1, retry, cmd);
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
        LOG_E("[ESP Error] Set CWMODE failed");
        return false;
    }

    // 2. 格式化指令 (使用 snprintf 防止缓冲区溢出)
    int len = snprintf(cmd_buf, sizeof(cmd_buf), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (len < 0 || len >= sizeof(cmd_buf))
    {
        LOG_E("[ESP Error] SSID/PWD too long!");
        return false;
    }

    // 3. 发送连接 (长超时 15s)
    // 成功通常返回 OK，有时返回 WIFI GOT IP，视固件而定，这里以 OK 为准
    if (ESP_Send_AT(cmd_buf, "OK", 15000, retry))
    {
        return true;
    }

    LOG_E("[ESP Error] WiFi Connect Failed after %d retries", retry);
    return false;
}

bool ESP_SNTP_Config(void)
{
    LOG_I("[ESP] Configing SNTP!");
    return ESP_Send_AT("AT+CIPSNTPCFG=1,8,\"ntp1.aliyun.com\"", "OK", 2000, 2);
}

bool ESP_SNTP_Sync_RTC(void)
{
    char line_buf[128];
    bool ret = false;

    UART_RingBuf_Clear(g_module_uart);

    UART_Send_AT_Command(g_module_uart, "AT+CIPSNTPTIME?");

    uint64_t start = BSP_GetTick_ms();
    // 返回值示例：+CIPSNTPTIME:Mon Oct 18 20:12:27 2021
    while ((BSP_GetTick_ms() - start) < 2000)
    {
        if (UART_RingBuf_ReadLine(g_module_uart, line_buf, sizeof(line_buf), 50) > 0)
        {
            // 可调试打印
            // LOG_D("[SNTP] Receive Data:%s", line_buf);

            char* pData = strstr(line_buf, "+CIPSNTPTIME:");
            if (pData)
            {
                pData += 13; // 跳过 +CIPSNTPTIME: 前缀

                char wday_str[4], mon_str[4];
                int  day, hour, min, sec, year;
                // 使用 sscanf 格式化取出数据
                int count = sscanf(pData,
                                   "%3s %3s %d %d:%d:%d %d",
                                   wday_str,
                                   mon_str,
                                   &day,
                                   &hour,
                                   &min,
                                   &sec,
                                   &year);
                // 如果取出数据正确
                if (count >= 7)
                {
                    int month_idx = 0;
                    // 解析月份
                    for (int i = 0; i < 12; i++)
                    {
                        if (strncmp(mon_str, MONTH_STR[i], 3) == 0)
                        {
                            month_idx = i + 1;
                            break;
                        }
                    }
                    if (month_idx > 0)
                    {
                        LOG_I("[SNTP] Net Time: %04d-%02d-%02d %02d:%02d:%02d",
                              year,
                              month_idx,
                              day,
                              hour,
                              min,
                              sec);

                        // === 同步到 RTC ===
                        BSP_RTC_SetDate(year, month_idx, day);
                        BSP_RTC_SetTime(hour, min, sec);

                        ret = true;
                        break; // 成功，退出循环
                    }
                }
            }
        }
    }
    return ret;
}

bool ESP_HTTP_Get(const char* url, uint32_t timeout_ms)
{
    char cmd[512]; // 确保缓冲区够大，URL 很长

    // 清空缓存
    UART_RingBuf_Clear(g_module_uart);

    // 构造指令
    // 格式: AT+HTTPCLIENT=<opt>,<content-type>,<url>,[<host>],[<path>],<transport_type>
    // opt=2 (GET)
    // content-type=1 (application/json)
    // transport_type=2 (TCP/HTTP), 如果是 https 则为 2

    // 为了防止 URL 里有特殊字符影响 snprintf，最好确保 URL 是干净的
    snprintf(cmd, sizeof(cmd), "AT+HTTPCLIENT=2,1,\"%s\",,,1", url);

    // 发送指令，等待 "+HTTPCLIENT:" 或 "OK"
    // 注意：HTTP 请求可能较慢，超时时间给足
    return ESP_Send_AT(cmd, NULL, timeout_ms, 2); // 这里 expect_resp 传 NULL，在外面自己解析数据
}