#include "esp32_module.h"
#include "BSP_Tick_Delay.h"
#include <string.h>
#include <stdio.h>

static char esp_rx_line[256]; // 模块内缓冲，解耦main

void ESP_Init(UART_Handle_t* handle)
{
    UART_Init(handle); // 开RCC/GPIO/NVIC
    printf("ESP Module Ready\r\n");
}

bool ESP_Send_AT(UART_Handle_t* handle, const char* cmd, const char* expect, uint32_t timeout_ms)
{
    // 清缓冲防旧数据
    while (UART_Read_Line(handle, esp_rx_line, sizeof(esp_rx_line)))
        ;

    // 发送命令
    UART_Send_AT_Command(handle, cmd);
    printf("ESP Sent: %s\r\n", cmd);

    // 轮询响应
    uint32_t start = BSP_GetTick_ms();
    while ((BSP_GetTick_ms() - start) < timeout_ms)
    {
        if (UART_Read_Line(handle, esp_rx_line, sizeof(esp_rx_line)))
        {
            printf("ESP Recv: %s", esp_rx_line);
            printf("ESP Recv Full: [%s]\r\n", esp_rx_line); // 临时全行打印，测乱码/ready
            if (strstr(esp_rx_line, expect))
                return true;
            if (strstr(esp_rx_line, "ERROR"))
                return false;
        }
        BSP_Delay_ms(10); // 低负载
    }
    printf("ESP Timeout: No %s\r\n", expect);
    return false;
}

/**
 * @brief ESP重启
 * @param handle: UART句柄指针
 * @retval true: ready回
 * @retval false: 失败
 * @note  AT+RST，清config
 */
bool ESP_Reset(UART_Handle_t* handle)
{
    if (!ESP_Send_AT(handle, "AT+RST", "OK", 3000))
    { // 3s超时
        printf("ESP RST Fail\r\n");
        return false;
    }
    printf("ESP Reset OK\r\n");
    return true;
}

bool ESP_WiFi_Connect(UART_Handle_t* handle, const char* ssid, const char* pwd)
{
    char cmd[128];

    // 0. 重启清config
    if (!ESP_Reset(handle))
    {
        return false;
    }
    BSP_Delay_ms(2000); // 2s boot

    // 1. 设站模式
    if (!ESP_Send_AT(handle, "AT+CWMODE=1", "OK", 1000))
    {
        printf("ESP CWMODE Fail\r\n");
        return false;
    }

    // 2. 连WiFi
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (ESP_Send_AT(handle, cmd, "WIFI CONNECTED", 10000))
    {
        printf("ESP WiFi Connected OK!\r\n");
        return true;
    }
    else
    {
        printf("ESP WiFi Fail: Check SSID/PWD/Signal\r\n");
        return false;
    }
}

bool ESP_HTTP_Get_Weather(UART_Handle_t* handle,
                          const char*    url,
                          char*          json_buffer,
                          uint16_t       buf_size)
{
    char     cmd[512]; // URL长，缓冲大
    uint16_t len = 0;

    // 1. HTTP GET
    sprintf(cmd, "AT+HTTPCLIENT=1,1,\"%s\"", url); // 拼接
    if (!ESP_Send_AT(handle, cmd, "+HTTPCLIENT:1,200", 20000))
    {
        printf("ESP HTTP GET Fail\r\n");
        return false;
    }

    // 2. 读响应体（多行，直到+HTTPCLIENT:1,0结束）
    memset(json_buffer, 0, buf_size);
    while (len < buf_size - 1)
    {
        if (ESP_Send_AT(handle, "", "+HTTPCLIENT:1,0", 1000))
            break; // 空cmd读结束
        // 实际读：用UART_Read_Line，但封装用Send_AT改expect空
        // 临时：轮询读
        char line[256];
        if (UART_Read_Line(handle, line, sizeof(line)))
        {
            printf("JSON Line: %s", line); // 日志
            if (strstr(line, "+HTTPCLIENT:1,0"))
                break;
            strcat(json_buffer + len, line);
            len += strlen(line);
        }
        BSP_Delay_ms(50);
    }
    printf("ESP HTTP JSON Len: %d\r\n", len);
    return true;
}