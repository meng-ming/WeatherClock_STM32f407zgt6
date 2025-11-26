/**
 * @file    app_weather.c
 * @brief   天气业务逻辑实现 (商业级加固版)
 */

#include "app_weather.h"
#include "esp32_module.h" // 确保这是纯粹的底层驱动
#include "uart_driver.h"
#include "BSP_Tick_Delay.h"
#include "weather_parser.h"
#include <string.h>
#include <stdio.h>

// === 配置区 ===
#define WIFI_SSID "7041"
#define WIFI_PWD "auto7041"
#define WEATHER_HOST "api.seniverse.com"
#define WEATHER_PORT 80
#define API_KEY "SyLymtirx8EY1K_Dz"
#define CITY_LOC "beijing"

// === 状态机定义 ===
typedef enum
{
    STATE_INIT = 0,
    STATE_CHECK_AT,
    STATE_CONNECT_WIFI,
    STATE_CONNECT_TCP,
    STATE_ENTER_PASS,
    STATE_SEND_REQ,
    STATE_WAIT_RESP,
    STATE_EXIT_PASS,
    STATE_IDLE
} Weather_State_t;

static Weather_State_t    s_state = STATE_INIT;
static uint32_t           s_timer = 0;
static char               s_line_buf[1024];
static App_Weather_Data_t s_cache_data; // 数据缓存

// 回调函数
static Weather_DataCallback_t   s_data_cb   = NULL;
static Weather_StatusCallback_t s_status_cb = NULL;

// 宏：通知状态 (增加判空保护)
#define NOTIFY_STATUS(msg, color)                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (s_status_cb)                                                                           \
            s_status_cb(msg, color);                                                               \
    } while (0)

// === 外部接口实现 ===

void App_Weather_Init(Weather_DataCallback_t data_cb, Weather_StatusCallback_t status_cb)
{
    // [优化1] 检查回调是否合法，虽然后面有判空，但初始化警告很有必要
    if (data_cb == NULL || status_cb == NULL)
    {
        // 可以在这里打印个错误日志
        // printf("Error: Callbacks cannot be NULL\r\n");
    }

    s_data_cb   = data_cb;
    s_status_cb = status_cb;
    s_state     = STATE_INIT;
}

// 新增：TCP 连接重试计数器
static uint8_t s_retry_cnt = 0;

void App_Weather_Task(void)
{
    extern UART_Handle_t g_esp_uart_handler;

    switch (s_state)
    {
    case STATE_INIT:
        if (s_status_cb)
            s_status_cb("Init System...", 0xFFFF);
        ESP_Module_Init(&g_esp_uart_handler);
        s_state = STATE_CHECK_AT;
        break;

    case STATE_CHECK_AT:
        NOTIFY_STATUS("Check AT...", 0xFFFF);
        UART_Send_Data(&g_esp_uart_handler, "+++", 3);
        BSP_Delay_ms(500);

        // [变更] AT 测试，给 2 次重试机会
        if (ESP_Send_AT("AT", "OK", 1000, 2))
        {
            s_state = STATE_CONNECT_WIFI;
        }
        else
        {
            NOTIFY_STATUS("AT Fail!", 0xF800);
            BSP_Delay_ms(1000);
        }
        break;

    case STATE_CONNECT_WIFI:
        NOTIFY_STATUS("Link WiFi...", 0xFFFF);
        // [变更] WiFi 连接，给 3 次重试机会
        if (ESP_WiFi_Connect(WIFI_SSID, WIFI_PWD, 3))
        {
            s_state     = STATE_CONNECT_TCP;
            s_retry_cnt = 0; // 进入新阶段，清零计数器
        }
        else
        {
            NOTIFY_STATUS("WiFi Fail!", 0xF800);
            BSP_Delay_ms(2000);
        }
        break;

    case STATE_CONNECT_TCP:
        NOTIFY_STATUS("Link Cloud...", 0xFFFF);
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", WEATHER_HOST, WEATHER_PORT);

        // [变更] TCP 连接，单次超时 10s，底层重试 1 次
        if (ESP_Send_AT(cmd, "CONNECT", 10000, 1) ||
            strstr((char*) g_esp_uart_handler.rx_buffer, "ALREADY"))
        {
            s_state     = STATE_ENTER_PASS;
            s_retry_cnt = 0; // 成功清零
        }
        else
        {
            // [新增] 状态机级重试逻辑
            s_retry_cnt++;
            char err_msg[20];
            snprintf(err_msg, sizeof(err_msg), "TCP Err %d/3", s_retry_cnt);
            NOTIFY_STATUS(err_msg, 0xF800);

            if (s_retry_cnt >= 3)
            {
                // 连续 3 次连不上，可能是网断了，回退到检查 AT 重来
                NOTIFY_STATUS("Net Reset!", 0xF800);
                s_state     = STATE_CHECK_AT;
                s_retry_cnt = 0;
            }
            else
            {
                BSP_Delay_ms(2000); // 稍后重试
            }
        }
        break;

    case STATE_ENTER_PASS:
        // [变更] 透传指令，给 2 次重试
        if (!ESP_Send_AT("AT+CIPMODE=1", "OK", 1000, 2))
        {
            s_state = STATE_CONNECT_TCP;
            break;
        }
        if (ESP_Send_AT("AT+CIPSEND", ">", 2000, 2))
        {
            s_state = STATE_SEND_REQ;
        }
        else
        {
            s_state = STATE_CONNECT_TCP;
        }
        break;

    case STATE_SEND_REQ:
        NOTIFY_STATUS("Get Data...", 0xFFE0);
        char req[512];
        // [优化4] 使用 snprintf
        snprintf(req,
                 sizeof(req),
                 "GET /v3/weather/now.json?key=%s&location=%s&language=en&unit=c HTTP/1.1\r\nHost: "
                 "%s\r\nConnection: close\r\n\r\n",
                 API_KEY,
                 CITY_LOC,
                 WEATHER_HOST);
        UART_Send_AT_Command(&g_esp_uart_handler, req);
        s_timer = BSP_GetTick_ms();
        s_state = STATE_WAIT_RESP;
        break;

    case STATE_WAIT_RESP:
        if (UART_RingBuf_ReadLine(&g_esp_uart_handler, s_line_buf, sizeof(s_line_buf), 20) > 0)
        {
            if (strchr(s_line_buf, '{'))
            {
                if (Weather_Parser_Execute(s_line_buf, &s_cache_data))
                {
                    // 解析成功！数据已经填入 s_cache_data 了
                    if (s_data_cb)
                        s_data_cb(&s_cache_data); // 通知 UI
                    s_state = STATE_EXIT_PASS;
                }
            }
        }
        if (BSP_GetTick_ms() - s_timer > 10000)
        {
            NOTIFY_STATUS("Timeout!", 0xF800);
            s_state = STATE_EXIT_PASS;
        }
        break;

    case STATE_EXIT_PASS:
        BSP_Delay_ms(1000);
        UART_Send_Data(&g_esp_uart_handler, "+++", 3);
        BSP_Delay_ms(1000);
        s_state = STATE_IDLE;
        s_timer = BSP_GetTick_ms();
        break;

    case STATE_IDLE:
        if (BSP_GetTick_ms() - s_timer > 60000)
        {
            s_state = STATE_CONNECT_TCP; // 重新连接，因为之前发了 Connection: close
        }
        break;
    }
}