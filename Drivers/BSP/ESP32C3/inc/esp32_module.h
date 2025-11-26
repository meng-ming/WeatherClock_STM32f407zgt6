/**
 * @file    esp32_module.h
 * @brief   ESP32 AT 指令模组通用驱动接口 (带重试机制版)
 * @note    本模块只负责底层的 AT 指令收发和基础网络连接，不包含任何具体的业务逻辑。
 * 依赖 uart_driver 模块进行物理层通信。
 */

#ifndef __ESP32_MODULE_H
#define __ESP32_MODULE_H

#include "uart_driver.h" // 依赖串口驱动
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  初始化 ESP32 模块驱动
 * @note   负责初始化对应的 UART 硬件，并清空接收缓冲区。
 * 注意：此函数不会发送 AT 复位指令，仅做物理层准备。
 * @param  uart_handler: 指向 UART 句柄的指针 (如 &g_esp_uart_handler)
 * @retval None
 */
void ESP_Module_Init(UART_Handle_t* uart_handler);

/**
 * @brief  发送 AT 指令并等待预期响应 (支持自动重试)
 * @note   发送指令后，会持续轮询接收缓冲区，直到匹配到 expect_resp 或超时。
 * 如果失败，会自动重试指定次数。
 * 会自动处理换行符，调用者无需手动在 cmd 后加 \r\n。
 * @param  cmd:         要发送的 AT 指令字符串 (如 "AT", "AT+CIPMODE=1")
 * @param  expect_resp: 期待收到的响应字符串 (如 "OK", "CONNECT", "ready")
 * @param  timeout_ms:  单次等待超时时间 (单位: ms)。建议普通指令 1000ms，连接指令 15000ms。
 * @param  retry:       失败后的重试次数。0表示不重试(只发1次)，1表示失败后再发1次。
 * @retval true:  在超时前成功匹配到预期响应
 * @retval false: 所有重试次数用尽后仍失败，或收到 "ERROR"
 */
bool ESP_Send_AT(const char* cmd, const char* expect_resp, uint32_t timeout_ms, uint8_t retry);

/**
 * @brief  连接 WiFi 热点 (Station 模式)
 * @note   内部会自动执行 AT+CWMODE=1 和 AT+CWJAP 指令。
 * 这是一个耗时操作，通常需要 5-10 秒。
 * @param  ssid:  WiFi 名称 (字符串)
 * @param  pwd:   WiFi 密码 (字符串)
 * @param  retry: 连接失败后的重试次数
 * @retval true:  成功连接并获取 IP
 * @retval false: 连接失败或超时
 */
bool ESP_WiFi_Connect(const char* ssid, const char* pwd, uint8_t retry);

#endif /* __ESP32_MODULE_H */