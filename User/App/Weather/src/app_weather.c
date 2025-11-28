/**
 * @file    app_weather.c
 * @brief   天气业务逻辑实现 (商业级加固版)
 */

#include "app_weather.h"
#include "esp32_module.h" // 确保这是纯粹的底层驱动
#include "sys_log.h"
#include "uart_driver.h"
#include "BSP_Tick_Delay.h"
#include "weather_parser.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// === 状态机定义 ===
typedef enum
{
    STATE_INIT = 0,
    STATE_CHECK_AT,
    STATE_CONNECT_WIFI,
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
        LOG_E("[APP Weather Init]Error: Callbacks cannot be NULL");
    }

    s_data_cb   = data_cb;
    s_status_cb = status_cb;
    s_state     = STATE_INIT;
}

// TCP 连接重试计数器
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

        // 加上 "+++" 退出可能存在的透传模式，防止干扰
        UART_Send_Data(&g_esp_uart_handler, "+++", 3);
        BSP_Delay_ms(500);

        // AT 测试，给 2 次重试机会
        if (ESP_Send_AT("AT", "OK", 1000, 2))
        {
            s_state = STATE_CONNECT_WIFI;
        }
        else
        {
            // -------- 得处理 AT 命令异常 ----------------

            NOTIFY_STATUS("AT Fail!", 0xF800);
            BSP_Delay_ms(1000);
        }
        break;

    case STATE_CONNECT_WIFI:
        NOTIFY_STATUS("Link WiFi...", 0xFFFF);
        // WiFi 连接，给 3 次重试机会
        if (ESP_WiFi_Connect(WIFI_SSID, WIFI_PWD, 3))
        {
            // === WiFi 连上后，立刻配置 SNTP 并对时 ===

            NOTIFY_STATUS("Sync Time...", 0xFFFF);
            LOG_I("[SNTP] Sync Time ...");
            // 1. 配置 SNTP (只需一次)
            if (ESP_SNTP_Config() != true)
                LOG_W("[SNTP] Configing SNTP Failed!");
            BSP_Delay_ms(500);

            // 2. 获取时间并同步 RTC
            // 尝试 3 次，增加成功率
            for (int i = 0; i < 3; i++)
            {
                if (ESP_SNTP_Sync_RTC())
                {
                    LOG_D("[SNTP] Sync Time Success!");
                    break;
                }
                if (i == 2)
                {
                    LOG_W("[SNTP] Sync Time Failed!");
                }
            }
            BSP_Delay_ms(1000);

            s_state     = STATE_SEND_REQ;
            s_retry_cnt = 0; // 进入新阶段，清零计数器
        }
        else
        {
            // ---------- 同样处理 WIFI 连接异常 --------------------
            NOTIFY_STATUS("WiFi Fail!", 0xF800);
            BSP_Delay_ms(2000);
        }
        break;
    case STATE_SEND_REQ:
        NOTIFY_STATUS("Get Data...", 0xFFE0);

        char url[256];
        // 拼装 URL
        snprintf(url,
                 sizeof(url),
                 "http://%s/free/day?appid=%s&appsecret=%s&unescape=1&city=%s",
                 WEATHER_HOST,
                 WEATHER_APPID,
                 WEATHER_APPSECRET,
                 CITY_LOC);

        // 一键发送！
        if (ESP_HTTP_Get(url, 5000))
        {
            s_timer = BSP_GetTick_ms();
            s_state = STATE_WAIT_RESP;
        }
        else
        {
            // -------------- 处理获取天气信息异常 -------------------

            NOTIFY_STATUS("Req Fail!", 0xF800);
            BSP_Delay_ms(2000);
            // 失败重试，或者回退到 CHECK_AT
        }
        break;
    case STATE_WAIT_RESP:
        if (UART_RingBuf_ReadLine(&g_esp_uart_handler, s_line_buf, sizeof(s_line_buf), 20) > 0)
        {
            // 调试打印，查看AT命令返回的信息
            // LOG_D("[HTTP] Weather Info: %s", s_line_buf);

            // 寻找 .json 数据的起始位置
            char* p_json_start = strchr(s_line_buf, '{');
            if (p_json_start != NULL)
            {
                if (Weather_Parser_Execute(p_json_start, &s_cache_data))
                {
                    // 解析成功！数据已经填入 s_cache_data 了
                    if (s_data_cb)
                        s_data_cb(&s_cache_data); // 通知 UI

                    NOTIFY_STATUS("Weather OK", 0x07E0);
                    LOG_I("[Weather] Weather Info Parser Success!");
                    s_state = STATE_IDLE; // 解析成功，去休息
                    s_timer = BSP_GetTick_ms();
                }
            }
        }
        if (BSP_GetTick_ms() - s_timer > 10000)
        {
            NOTIFY_STATUS("Timeout!", 0xF800);
            s_state = STATE_EXIT_PASS;
        }
        break;

    case STATE_IDLE:
        if (BSP_GetTick_ms() - s_timer > 60000)
        {
            s_state = STATE_SEND_REQ; // 直接回去发请求，不用重连 WiFi
        }
        break;
    }
}