/**
 * @file    esp32_weather.c
 * @brief   ESP32-C3天气驱动实现
 */

#include "esp32_weather.h"
#include "BSP_Tick_Delay.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

static UART_Handle_t* g_handle = NULL; // 临时保存句柄

// 内部辅助：等待响应
static bool wait_response(const char* expect, uint32_t timeout_ms)
{
    char     line[256];
    uint32_t start = BSP_GetTick_ms();

    while (BSP_GetTick_ms() - start < timeout_ms)
    {
        uint16_t len = UART_RingBuf_ReadLine(g_handle, line, sizeof(line), 50);
        if (len > 0)
        {
            printf("[RX] %s", line);
            if (strstr(line, expect))
                return true;
            // 如果收到ERROR，直接失败
            if (strstr(line, "ERROR"))
                return false;
        }
    }
    return false;
}

bool ESP32_Weather_Init(UART_Handle_t* handle)
{
    g_handle = handle;
    UART_RingBuf_Clear(handle);

    // 关键修复：复位前先清空缓冲区
    UART_Send_AT_Command(handle, "AT+RST");
    BSP_Delay_ms(5000);

    // 关键修复：复位后，ESP32会吐 "ready" 和乱码，必须全部读走！
    // 不读走的话，wait_response 会卡在这些旧数据上
    UART_RingBuf_Clear(handle);
    BSP_Delay_ms(1000); // 再等1秒，让ready吐完

    // 先发一个AT，把ready冲掉
    UART_Send_AT_Command(handle, "AT");
    if (!wait_response("OK", 2000))
    {
        printf("AT Test Failed after RST!\r\n");
        return false;
    }

    UART_Send_AT_Command(handle, "AT+CWMODE=1");
    return wait_response("OK", 1000);
}

bool ESP32_WiFi_Connect(UART_Handle_t* handle, const char* ssid, const char* pwd)
{
    char cmd[128];
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    UART_Send_AT_Command(handle, cmd);
    return wait_response("OK", 15000);
}

bool ESP32_Get_Weather(UART_Handle_t*  handle,
                       const char*     url,
                       Weather_Info_t* info,
                       uint32_t        timeout_ms)
{
    char     cmd[512];
    char     full_json[1024] = {0}; // 确保栈空间够大，或者定义为全局static
    uint32_t start           = BSP_GetTick_ms();

    g_handle = handle;

    // 发送请求
    sprintf(cmd, "AT+HTTPCLIENT=2,0,\"%s\",,,2", url);
    UART_Send_AT_Command(handle, cmd);

    while (BSP_GetTick_ms() - start < timeout_ms)
    {
        char     line[512]; // 稍微大一点，防溢出
        uint16_t len = UART_RingBuf_ReadLine(handle, line, sizeof(line), 50);

        if (len > 0)
        {
            // 调试打印 (可选)
            // printf("[RX] %s", line);

            // === 核心修复逻辑开始 ===

            // 状态 1: JSON 还没开始拼接
            if (strlen(full_json) == 0)
            {
                // 寻找 JSON 头 '{'
                char* json_start = strchr(line, '{');
                if (json_start)
                {
                    strcpy(full_json, json_start); // 找到了！作为开头
                }
            }
            // 状态 2: JSON 已经开始拼接了
            else
            {
                // 只要不是结束符 OK/ERROR，统统拼接到后面
                // 这样就不会漏掉 '{' 前面的符号（比如 : 和 [ ）
                if (!strstr(line, "OK") && !strstr(line, "ERROR") && !strstr(line, "+HTTPCLIENT"))
                {
                    // 防止缓冲区溢出保护
                    if (strlen(full_json) + strlen(line) < sizeof(full_json) - 1)
                    {
                        strcat(full_json, line);
                    }
                }
            }
            // === 核心修复逻辑结束 ===

            // 结束条件判断
            if (strstr(line, "OK") && strlen(full_json) > 10)
            {
                printf("\r\n>>> JSON Fix Complete! Len: %d\r\n", strlen(full_json));
                printf(">>> Raw: %s\r\n", full_json); // 这次打印出来肯定是对的

                cJSON* root = cJSON_Parse(full_json);
                if (!root)
                {
                    printf("cJSON Parse Failed! (Check brackets)\r\n");
                    return false;
                }

                // 解析逻辑保持不变
                cJSON* results = cJSON_GetObjectItem(root, "results");
                if (cJSON_IsArray(results))
                {
                    cJSON* first = cJSON_GetArrayItem(results, 0);
                    if (first)
                    {
                        cJSON* loc  = cJSON_GetObjectItem(first, "location");
                        cJSON* now  = cJSON_GetObjectItem(first, "now");
                        cJSON* last = cJSON_GetObjectItem(first, "last_update");

                        if (loc && now)
                        {
                            // 城市名
                            cJSON* name = cJSON_GetObjectItem(loc, "name");
                            if (name)
                                strncpy(info->city, name->valuestring, sizeof(info->city) - 1);

                            // 天气现象
                            cJSON* text = cJSON_GetObjectItem(now, "text");
                            if (text)
                                strncpy(
                                    info->weather, text->valuestring, sizeof(info->weather) - 1);

                            // 温度
                            cJSON* temp = cJSON_GetObjectItem(now, "temperature");
                            if (temp)
                                sprintf(info->temp, "%s C", temp->valuestring);

                            // 更新时间
                            if (last)
                                strncpy(info->update_time, last->valuestring + 11, 5);

                            cJSON_Delete(root);
                            return true; // 成功！
                        }
                    }
                }
                cJSON_Delete(root);
                printf("JSON Structure Error (No results/now)\r\n");
                return false;
            }

            // 遇到 ERROR 直接退出
            if (strstr(line, "ERROR"))
                return false;
        }
    }
    return false; // 超时
}

void Weather_Print_Info(const Weather_Info_t* info)
{
    printf("\r\n");
    printf("╔══════════════════════════════════╗\r\n");
    printf("║        WEATHER CLOCK LIVE        ║\r\n");
    printf("║                                  ║\r\n");
    printf("║  City   : %-22s ║\r\n", info->city);
    printf("║  Weather: %-22s ║\r\n", info->weather);
    printf("║  Temp   : %-22s ║\r\n", info->temp);
    printf("║  Update : %-22s ║\r\n", info->update_time);
    printf("║                                  ║\r\n");
    printf("╚══════════════════════════════════╝\r\n");
}