#ifndef __ESP_MODULE_H
#define __ESP_MODULE_H

#include "uart_driver.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief ESP模块初始化
 * @param handle: UART句柄指针 (e.g. &g_esp_uart_handler)
 * @retval None
 * @note  开中断/时钟，依赖UART驱动
 */
void ESP_Init(UART_Handle_t* handle);

/**
 * @brief 发送AT命令并等待响应
 * @param handle: UART句柄指针
 * @param cmd: AT命令字符串 (e.g. "AT")
 * @param expect: 期望响应关键字 (e.g. "OK")
 * @param timeout_ms: 超时毫秒 (e.g. 5000)
 * @retval true: 找到expect
 * @retval false: 超时或"ERROR"
 * @note  轮询读行
 */
bool ESP_Send_AT(UART_Handle_t* handle, const char* cmd, const char* expect, uint32_t timeout_ms);

/**
 * @brief ESP重启
 * @param handle: UART句柄指针
 * @retval true: ready回
 * @retval false: 失败
 * @note  AT+RST，清config
 */
bool ESP_Reset(UART_Handle_t* handle);

/**
 * @brief WiFi连网初始化
 * @param handle: UART句柄指针
 * @param ssid: WiFi名称字符串
 * @param pwd: WiFi密码字符串
 * @retval true: 连网成功
 * @retval false: 失败（超时/ERROR）
 * @note  CWMODE=1 + CWJAP，超时10s
 */
bool ESP_WiFi_Connect(UART_Handle_t* handle, const char* ssid, const char* pwd);

/**
 * @brief HTTP GET拉天气API
 * @param handle: UART句柄指针
 * @param url: API URL字符串 (e.g. "http://v1.yiketianqi.com/free/day?...")
 * @param json_buffer: JSON响应缓冲 (e.g. char[1024])
 * @param buf_size: 缓冲大小 (e.g. 1024)
 * @retval true: 200 OK + JSON读完
 * @retval false: 失败（超时/ERROR）
 * @note  AT+HTTPCLIENT GET，读body到buffer
 */
bool ESP_HTTP_Get_Weather(UART_Handle_t* handle,
                          const char*    url,
                          char*          json_buffer,
                          uint16_t       buf_size);

#endif