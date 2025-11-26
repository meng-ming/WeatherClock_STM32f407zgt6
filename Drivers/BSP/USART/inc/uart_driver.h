/**
 * @file    uart_driver.h
 * @brief   STM32 通用 UART 驱动接口 (带环形缓冲区)
 * @note    支持中断接收 + 环形缓冲读取，适用于 AT 指令模块、调试打印等场景。
 * 集成 RingBuffer 机制，有效防止数据丢包和粘包。
 */

#ifndef __UART_DRIVER_H
#define __UART_DRIVER_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

// 接收缓冲区大小 (根据实际内存情况调整，建议 1KB - 4KB)
#define RX_BUFFER_SIZE 2048

/**
 * @brief UART 句柄结构体
 * @note  包含硬件配置信息和运行时缓冲区状态
 */
typedef struct
{
    // === 1. 硬件配置参数 (初始化时填写) ===
    USART_TypeDef* USART_X;               // 外设基地址 (如 USART1)
    uint32_t       BaudRate;              // 波特率 (如 115200)
    uint8_t        USART_IRQ_Channel;     // NVIC 中断通道号
    uint32_t       RCC_APBPeriph_USART_X; // 串口时钟宏
    uint8_t        Is_APB2;               // 时钟源是否在 APB2 (1:APB2, 0:APB1)

    uint32_t      AHB_Clock_Enable_GPIO_Bit; // GPIO 时钟宏
    GPIO_TypeDef* TX_Port;                   // 发送引脚端口
    uint16_t      TX_Pin;                    // 发送引脚号
    uint16_t      TX_PinSource_X;            // 发送引脚复用源
    uint8_t       TX_AF;                     // 发送引脚复用功能号

    GPIO_TypeDef* RX_Port;        // 接收引脚端口
    uint16_t      RX_Pin;         // 接收引脚号
    uint16_t      RX_PinSource_X; // 接收引脚复用源
    uint8_t       RX_AF;          // 接收引脚复用功能号

    // === 2. 运行时状态 (用户勿动) ===
    volatile uint8_t  rx_buffer[RX_BUFFER_SIZE]; // 环形数据存储区
    volatile uint16_t rx_read_index;             // 读指针 (Tail)，由应用层读取时移动
    volatile uint16_t rx_write_index;            // 写指针 (Head)，由中断接收时移动
    volatile uint32_t rx_overflow_cnt;           // 溢出计数器 (调试用，检测是否丢包)

} UART_Handle_t;

/* ========================== 基础接口 ========================== */

/**
 * @brief  初始化 UART 外设
 * @note   配置 GPIO、时钟、中断优先级，并开启接收中断。
 * 同时会复位环形缓冲区的读写指针。
 * @param  UART_Handle: 指向 UART 句柄的指针
 */
void UART_Init(UART_Handle_t* UART_Handle);

/**
 * @brief  发送原始数据 (阻塞式)
 * @param  UART_Handle: UART 句柄
 * @param  data:        数据指针
 * @param  len:         数据长度
 */
void UART_Send_Data(UART_Handle_t* UART_Handle, const char* data, uint32_t len);

/**
 * @brief  发送 AT 指令 (自动添加 \r\n)
 * @param  UART_Handle: UART 句柄
 * @param  command:     指令字符串 (不带 \r\n，如 "AT")
 */
void UART_Send_AT_Command(UART_Handle_t* UART_Handle, const char* command);

/* ========================== 环形缓冲区高级接口 ========================== */

/**
 * @brief  获取环形缓冲区当前可用数据量
 * @param  handle: UART 句柄
 * @retval 缓冲区中尚未读取的字节数
 */
uint16_t UART_RingBuf_Available(UART_Handle_t* handle);

/**
 * @brief  从环形缓冲区读取一个字节 (非阻塞)
 * @param  handle: UART 句柄
 * @param  pData:  用于接收数据的指针
 * @retval 1: 读取成功
 * @retval 0: 缓冲区为空，读取失败
 */
uint8_t UART_RingBuf_ReadByte(UART_Handle_t* handle, uint8_t* pData);

/**
 * @brief  读取一行数据 (直到 \n 或缓冲区空或超时)
 * @note   会自动去除行尾的 \r\n，并添加 \0 结束符，方便字符串处理。
 * @param  handle:     UART 句柄
 * @param  buf:        目标缓冲区
 * @param  max_len:    目标缓冲区最大长度 (防止溢出)
 * @param  timeout_ms: 最大等待超时时间 (ms)
 * @retval 实际读取到的字符数 (不含结束符)
 */
uint16_t
UART_RingBuf_ReadLine(UART_Handle_t* handle, char* buf, uint16_t max_len, uint32_t timeout_ms);

/**
 * @brief  清空环形缓冲区
 * @note   复位读写指针，丢弃当前缓冲区内所有未读数据。
 * @param  handle: UART 句柄
 */
void UART_RingBuf_Clear(UART_Handle_t* handle);

#endif /* __UART_DRIVER_H */