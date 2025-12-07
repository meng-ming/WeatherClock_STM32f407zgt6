/**
 * @file    app_weather.c
 * @brief   天气业务逻辑核心引擎（华为级加固版 - 全非阻塞优化）
 * @author  meng-ming
 * @version 2.1
 * @date    2025-12-07
 * @note    关键特性升级：
 * - 彻底移除所有 BSP_Delay_ms 阻塞延时，保证系统实时性
 * - 引入 retry_target_state 实现动态错误跳转机制
 * - 优化串口读取逻辑，实现 0ms 等待（先问再取）
 * - 内存安全操作，防止缓冲区溢出
 */

#include "app_weather.h"
#include "app_data.h"
#include "app_ui_config.h"
#include "st7789.h"
#include "esp32_module.h"
#include "sys_log.h"
#include "uart_driver.h"
#include "uart_handle_variable.h"
#include "BSP_Tick_Delay.h"
#include "weather_parser.h"
#include "ui_main_page.h"
#include "city_code.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ========================== 配置区 ========================== */
// 最大失败重试次数，超过则进入长待机
#define WEATHER_CONFIG_MAX_RETRY 3
// HTTP 请求总超时时间 (ms)，涵盖发送请求到接收完数据
#define WEATHER_CONFIG_HTTP_TIMEOUT_MS 10000
// 正常更新间隔 (10分钟)，单位 ms
#define WEATHER_CONFIG_UPDATE_INTERVAL_MS (10UL * 60UL * 1000UL)
// 失败后的短延时重试间隔 (3秒)，单位 ms
#define WEATHER_CONFIG_RETRY_DELAY_MS 3000
// 接收缓冲区大小，必须大于最大的 HTTP 响应包 (心知天气通常 < 1KB)
#define WEATHER_CONFIG_RX_BUF_SIZE 2048

/* ========================== 状态机枚举 ========================== */
typedef enum
{
    WEATHER_STATE_INIT = 0,     // 初始化状态
    WEATHER_STATE_RESET_ESP,    // 发送复位指令
    WEATHER_STATE_RESET_WAIT,   // 等待复位完成
    WEATHER_STATE_AT_CHECK,     // 检查 AT 指令响应
    WEATHER_STATE_WIFI_CONNECT, // 连接 Wi-Fi
    WEATHER_STATE_SNTP_CONFIG,  // 配置 SNTP
    WEATHER_STATE_SNTP_QUERY,   // 发起时间查询
    WEATHER_STATE_SNTP_WAIT,    // 等待时间返回
    WEATHER_STATE_HTTP_REQUEST, // 发送 HTTP GET 请求
    WEATHER_STATE_HTTP_WAIT,    // 等待并接收 HTTP 响应数据
    WEATHER_STATE_PARSE,        // 解析 JSON 数据
    WEATHER_STATE_IDLE,         // 空闲等待 (10分钟更新一次)
    WEATHER_STATE_ERROR_DELAY,  // 错误延时
} Weather_State_t;

/* ========================== 引擎核心结构体 ========================== */
typedef struct
{
    Weather_State_t          state;              // 当前状态
    Weather_State_t          retry_target_state; // 发生错误后，延时回来要重试的目标状态
    uint32_t                 timer;              // 通用计时器 (用于超时检查或延时)
    uint8_t                  retry_cnt;          // 当前重试计数
    uint16_t                 rx_index;           // 接收缓冲区写入指针
    char                     rx_buffer[WEATHER_CONFIG_RX_BUF_SIZE]; // 大容量接收缓冲
    char                     current_city[32];                      // 当前城市名 (支持运行时切换)
    APP_Weather_Data_t       cache;                                 // 天气数据缓存
    Weather_DataCallback_t   data_cb;                               // UI 数据回调
    Weather_StatusCallback_t status_cb;                             // UI 状态回调
    bool                     is_running;                            // 引擎运行开关
} Weather_Engine_t;

/* ========================== 静态实例（单例模式） ========================== */
static Weather_Engine_t g_weather = {.state              = WEATHER_STATE_INIT,
                                     .retry_target_state = WEATHER_STATE_INIT, // 默认复位
                                     .current_city       = "北京",             // 默认城市
                                     .is_running         = false};

/* ========================== 状态通知宏 (安全调用回调) ========================== */
#define WEATHER_NOTIFY_STATUS(engine, msg, color)                                                  \
    do                                                                                             \
    {                                                                                              \
        if ((engine)->status_cb)                                                                   \
        {                                                                                          \
            (engine)->status_cb(msg, color);                                                       \
        }                                                                                          \
    } while (0)

/* ========================== 内部函数前向声明 ========================== */
static void weather_change_state(Weather_Engine_t* eng, Weather_State_t new_state);
static void weather_error_handle(Weather_Engine_t* eng, const char* msg);
static bool weather_sntp_sync_once(void);
static bool weather_send_http_request(Weather_Engine_t* eng);

/* ========================== 状态跳转工具函数 ========================== */
/**
 * @brief 切换状态机状态，并自动重置计时器
 */
static void weather_change_state(Weather_Engine_t* eng, Weather_State_t new_state)
{
    eng->state = new_state;
    eng->timer = BSP_GetTick_ms(); // 记录进入新状态的时间点
    LOG_I("[Weather] State -> %d", new_state);
}

/* ========================== 错误处理 (非阻塞优化核心) ========================== */
/**
 * @brief 统一错误处理器
 * @note  不会阻塞 CPU，而是切换到 ERROR_DELAY 状态利用主循环计时
 */
static void weather_error_handle(Weather_Engine_t* eng, const char* msg)
{
    LOG_W("[Weather] %s", msg);
    eng->retry_cnt++;

    // 超过最大重试次数，彻底放弃，进入长待机
    if (eng->retry_cnt >= WEATHER_CONFIG_MAX_RETRY)
    {
        LOG_E("[Weather] Max retry reached, enter IDLE");
        WEATHER_NOTIFY_STATUS(eng, "Update Failed", RED);
        weather_change_state(eng, WEATHER_STATE_IDLE);
        eng->retry_cnt = 0;
    }
    // 还在重试范围内，进入短延时状态
    else
    {
        LOG_W("[Weather] Retry %d/%d after %dms",
              eng->retry_cnt,
              WEATHER_CONFIG_MAX_RETRY,
              WEATHER_CONFIG_RETRY_DELAY_MS);

        // 跳转到 ERROR_DELAY 状态，利用主循环计时，不再死等 delay
        weather_change_state(eng, WEATHER_STATE_ERROR_DELAY);
    }
}

/* ========================== SNTP 单次同步 ========================== */
static bool weather_sntp_sync_once(void)
{
    if (!ESP_SNTP_Config())
    {
        LOG_W("[SNTP] Config failed");
        return false;
    }
    // 这里的 Sync_RTC 内部目前还是有阻塞的，后续可优化，但在 Task 中暂可接受
    if (ESP_SNTP_Sync_RTC())
    {
        LOG_I("[SNTP] Time sync success");
        return true;
    }
    return false;
}

/* ========================== 发送 HTTP 请求 ========================== */
static bool weather_send_http_request(Weather_Engine_t* eng)
{
    // 获取城市代码
    const char* city_id = City_Get_Code(eng->current_city);
    if (!city_id || strlen(city_id) == 0)
    {
        LOG_W("[Weather] City '%s' not found, use default", eng->current_city);
        city_id = "101010100"; // 默认北京
    }

    // 拼接 URL (注意：使用 snprintf 防止缓冲区溢出)
    char url[512] = {0};
    int  len      = snprintf(url,
                       sizeof(url),
                       "http://%s/free/day?appid=%s&appsecret=%s&unescape=1&cityid=%s",
                       WEATHER_HOST,
                       WEATHER_APPID,
                       WEATHER_APPSECRET,
                       city_id);

    if (len >= sizeof(url) || len < 0)
    {
        LOG_E("[Weather] URL too long!");
        return false;
    }

    // 清空接收缓冲区，准备接收新数据
    eng->rx_index = 0;
    memset(eng->rx_buffer, 0, sizeof(eng->rx_buffer));

    // 发送 AT+HTTPCLIENT 指令
    if (!ESP_HTTP_Get(url, WEATHER_CONFIG_HTTP_TIMEOUT_MS / 1000))
    {
        LOG_W("[Weather] HTTP GET failed");
        return false;
    }

    return true;
}

/* ========================== 对外公共接口 ========================== */

void APP_Weather_Init(Weather_DataCallback_t data_cb, Weather_StatusCallback_t status_cb)
{
    if (!data_cb || !status_cb)
    {
        LOG_E("[Weather] Callback cannot be NULL");
        return;
    }

    g_weather.data_cb    = data_cb;
    g_weather.status_cb  = status_cb;
    g_weather.is_running = true;

    // 初始化默认城市
    strncpy(g_weather.current_city, CITY_NAME, sizeof(g_weather.current_city) - 1);
    g_weather.current_city[sizeof(g_weather.current_city) - 1] = '\0';

    LOG_I("[Weather] Engine initialized, city: %s", g_weather.current_city);
    WEATHER_NOTIFY_STATUS(&g_weather, "Weather Init", UI_TEXT_WHITE);
}

void APP_Weather_Deinit(void)
{
    g_weather.is_running = false;
    g_weather.data_cb    = NULL;
    g_weather.status_cb  = NULL;
    LOG_I("[Weather] Engine deinitialized");
}

bool APP_Weather_Set_City(const char* city_name)
{
    if (!city_name || strlen(city_name) == 0 || strlen(city_name) >= sizeof(g_weather.current_city))
    {
        return false;
    }
    strncpy(g_weather.current_city, city_name, sizeof(g_weather.current_city) - 1);
    g_weather.current_city[sizeof(g_weather.current_city) - 1] = '\0';
    LOG_I("[Weather] City changed to: %s", g_weather.current_city);

    // 如果当前处于空闲状态，立即唤醒进行更新
    if (g_weather.state == WEATHER_STATE_IDLE)
    {
        weather_change_state(&g_weather, WEATHER_STATE_HTTP_REQUEST);
    }
    return true;
}

/* ========================== 主任务循环 (核心逻辑) ========================== */

void APP_Weather_Task(void)
{
    if (!g_weather.is_running)
        return;

    Weather_Engine_t* eng = &g_weather;
    char              line_buf[512];

    switch (eng->state)
    {
    case WEATHER_STATE_INIT:
        ESP_Module_Init(&g_esp_uart_handler); // 初始化串口和 DMA
        WEATHER_NOTIFY_STATUS(eng, "Init ESP32", UI_TEXT_WHITE);
        weather_change_state(eng, WEATHER_STATE_RESET_ESP);
        break;

    // 发送复位指令
    case WEATHER_STATE_RESET_ESP:
        WEATHER_NOTIFY_STATUS(eng, "Resetting ESP...", UI_TEXT_WHITE);

        // 1. 清空可能的脏数据
        UART_RingBuf_Clear(&g_esp_uart_handler);
        // 2. 发送复位指令
        UART_Send_AT_Command(&g_esp_uart_handler, "AT+RST");

        // 3. 跳转到等待状态
        weather_change_state(eng, WEATHER_STATE_RESET_WAIT);
        break;

    // 等待复位完成 (非阻塞延时)
    case WEATHER_STATE_RESET_WAIT:
        // ESP32 重启通常需要 2~3 秒
        // 我们利用主循环计时，期间喂狗是安全的 (main里喂了)
        if (BSP_GetTick_ms() - eng->timer >= 3000)
        {
            // 时间到，清洗一下缓冲区（把 ESP32 启动时的乱码 ready 那些清掉）
            UART_RingBuf_Clear(&g_esp_uart_handler);

            // 正式进入 AT 检查流程
            weather_change_state(eng, WEATHER_STATE_AT_CHECK);
        }
        break;

    case WEATHER_STATE_AT_CHECK:
        WEATHER_NOTIFY_STATUS(eng, "Check AT", UI_TEXT_WHITE);

        // [非阻塞优化] 设置失败后的重试目标状态
        // 如果失败，延时后会回到这里再次尝试
        eng->retry_target_state = WEATHER_STATE_AT_CHECK;

        if (ESP_Send_AT("AT", "OK", 1000, 2))
        {
            WEATHER_NOTIFY_STATUS(eng, "AT OK", UI_TEXT_WHITE);
            ESP_Send_AT("ATE0", "OK", 1000, 1); // 关闭回显
            weather_change_state(eng, WEATHER_STATE_WIFI_CONNECT);
        }
        else
        {
            // [非阻塞优化] 调用 error_handle 会切换到 ERROR_DELAY 状态
            weather_error_handle(eng, "AT Check Failed");
        }
        break;

    case WEATHER_STATE_WIFI_CONNECT:
        WEATHER_NOTIFY_STATUS(eng, "Connecting WiFi", UI_TEXT_WHITE);

        // 设置重试目标
        eng->retry_target_state = WEATHER_STATE_WIFI_CONNECT;

        if (ESP_WiFi_Connect(WIFI_SSID, WIFI_PWD, 3))
        {
            WEATHER_NOTIFY_STATUS(eng, "WiFi OK", GREEN);
            APP_UI_Update_WiFi(true, WIFI_SSID);
            weather_change_state(eng, WEATHER_STATE_SNTP_CONFIG);
        }
        else
        {
            // 连接失败，进入错误处理流程
            weather_error_handle(eng, "WiFi connect failed");
        }
        break;

    // 配置 SNTP
    case WEATHER_STATE_SNTP_CONFIG:
        WEATHER_NOTIFY_STATUS(eng, "Config SNTP", UI_TEXT_WHITE);
        // 设置失败回退点
        eng->retry_target_state = WEATHER_STATE_SNTP_CONFIG;

        if (ESP_SNTP_Config()) // 这里内部是 AT 指令，暂且保留阻塞，因为它很快
        {
            weather_change_state(eng, WEATHER_STATE_SNTP_QUERY);
        }
        else
        {
            weather_error_handle(eng, "SNTP Config Failed");
        }
        break;

    // 发起查询
    case WEATHER_STATE_SNTP_QUERY:
        WEATHER_NOTIFY_STATUS(eng, "Sync Time...", UI_TEXT_WHITE);
        eng->retry_target_state = WEATHER_STATE_SNTP_QUERY;

        ESP_SNTP_Query_Start(); // 发送 AT+CIPSNTPTIME?

        // 马上切换到等待状态，并重置超时计时器
        weather_change_state(eng, WEATHER_STATE_SNTP_WAIT);
        break;

    // 等待结果
    case WEATHER_STATE_SNTP_WAIT:
    {
        // 1. 调用非阻塞检查函数
        int8_t status = ESP_SNTP_Query_Check();

        if (status == 1) // 成功
        {
            WEATHER_NOTIFY_STATUS(eng, "Time Synced", GREEN);
            weather_change_state(eng, WEATHER_STATE_HTTP_REQUEST);
        }
        else if (status == -1) // 失败
        {
            weather_error_handle(eng, "SNTP Query Error");
        }
        else // status == 0 (等待中)
        {
            // 2. 检查超时 (比如给它 5秒 时间同步)
            // 这里利用主循环计时，看门狗绝对安全
            if (BSP_GetTick_ms() - eng->timer > 5000)
            {
                weather_error_handle(eng, "SNTP Timeout");
            }
        }
        break;
    }

    case WEATHER_STATE_HTTP_REQUEST:
        WEATHER_NOTIFY_STATUS(eng, "Requesting", UI_TEXT_WHITE);

        eng->retry_target_state = WEATHER_STATE_HTTP_REQUEST;

        if (weather_send_http_request(eng))
        {
            weather_change_state(eng, WEATHER_STATE_HTTP_WAIT);
        }
        else
        {
            weather_error_handle(eng, "HTTP send failed");
        }
        break;

    case WEATHER_STATE_HTTP_WAIT:
    {
        // [非阻塞优化]
        // 1.先判断 RingBuffer 里有数据吗？
        // 如果没有，直接 return，让出 CPU 给其他任务（如 UI 刷新、按键扫描）
        if (UART_RingBuf_Available(&g_esp_uart_handler) == 0)
        {
            LOG_D("UART_RingBuf_Available(&g_esp_uart_handler) == 0");
            // 顺便检查一下是否总超时 (防止一直没数据死等)
            if (BSP_GetTick_ms() - eng->timer > WEATHER_CONFIG_HTTP_TIMEOUT_MS)
            {
                LOG_E("[Timeout] Buffer len: %d", eng->rx_index);
                weather_error_handle(eng, "HTTP timeout");
            }
            return; // 还没数据，下次循环再来，别在这死等
        }

        // 2. 有数据了
        uint16_t len = UART_RingBuf_ReadLine(&g_esp_uart_handler, line_buf, sizeof(line_buf), 10);

        if (len > 0)
        {
            // 过滤无用的 AT 指令回显
            if (strstr(line_buf, "AT+") || strstr(line_buf, "SEND OK"))
            {
                break;
            }

            // 调试打印数据块 (调试阶段可开启)
            LOG_D("[RX Chunk] Len:%d, Content:%s", len, line_buf);

            // 防溢出保护
            if (eng->rx_index + len < WEATHER_CONFIG_RX_BUF_SIZE - 1)
            {
                // 拼接到大缓冲区
                memcpy(&eng->rx_buffer[eng->rx_index], line_buf, len);
                eng->rx_index += len;
                eng->rx_buffer[eng->rx_index] = '\0'; // 封口，确保是合法字符串
            }
            else
            {
                weather_error_handle(eng, "RX buffer overflow");
                break;
            }

            // 3. 实时检查是否收到了完整的 JSON (从 '{' 到 '}')
            // 使用 memchr 而非 strchr，防止 0x00 截断
            char* json_start = memchr(eng->rx_buffer, '{', eng->rx_index);
            if (json_start != NULL)
            {
                size_t offset        = json_start - eng->rx_buffer;
                size_t remaining_len = eng->rx_index - offset;

                // 从 '{' 开始往后找 '}'
                char* json_end = memchr(json_start, '}', remaining_len);

                if (json_end != NULL)
                {
                    LOG_I("[Weather] JSON captured complete! Parsing...");
                    weather_change_state(eng, WEATHER_STATE_PARSE);
                }
            }
        }
        break;
    }

    case WEATHER_STATE_PARSE:
    {
        // 再次定位 JSON 起始位置 (必须用 memchr 穿透前面的杂波)
        char* json_start = (char*) memchr(eng->rx_buffer, '{', eng->rx_index);

        // 调用解析器
        if (Weather_Parser_Execute(json_start, &eng->cache))
        {
            // 解析成功，通过回调通知 UI 层
            if (eng->data_cb)
            {
                eng->data_cb(&eng->cache);
            }
            WEATHER_NOTIFY_STATUS(eng, "Updated!", GREEN);

            // 重置重试计数，进入空闲状态
            eng->retry_cnt = 0;
            weather_change_state(eng, WEATHER_STATE_IDLE);
        }
        else
        {
            // 解析失败，可能是数据不完整，回到请求状态重试
            eng->retry_target_state = WEATHER_STATE_HTTP_REQUEST;
            weather_error_handle(eng, "JSON parse failed");
        }
    }
    break;

    case WEATHER_STATE_IDLE:
        // 定时更新检查 (非阻塞)
        // 计算时间差，只有时间到了才触发更新
        if (BSP_GetTick_ms() - eng->timer >= WEATHER_CONFIG_UPDATE_INTERVAL_MS)
        {
            LOG_I("[Weather] Scheduled update...");
            weather_change_state(eng, WEATHER_STATE_HTTP_REQUEST);
        }
        break;

    case WEATHER_STATE_ERROR_DELAY:
        // [核心非阻塞逻辑] 利用主循环 tick 进行计时
        // 时间到了才跳转回目标状态，期间 CPU 可以去处理别的任务
        if (BSP_GetTick_ms() - eng->timer >= WEATHER_CONFIG_RETRY_DELAY_MS)
        {
            // 动态跳转回刚才出错的地方重试
            weather_change_state(eng, eng->retry_target_state);
        }
        break;

    default:
        weather_change_state(eng, WEATHER_STATE_INIT);
        break;
    }
}