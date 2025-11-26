#ifndef __UART_DRIVER_H
#define __UART_DRIVER_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

#define RX_BUFFER_SIZE 2048

typedef struct
{
    // 硬件配置（初始化时填）
    USART_TypeDef* USART_X;
    uint32_t       BaudRate;
    uint8_t        USART_IRQ_Channel;
    uint32_t       RCC_APBPeriph_USART_X;
    uint8_t        Is_APB2;

    uint32_t      AHB_Clock_Enable_GPIO_Bit;
    GPIO_TypeDef* TX_Port;
    uint16_t      TX_Pin;
    uint16_t      TX_PinSource_X;
    uint8_t       TX_AF;
    GPIO_TypeDef* RX_Port;
    uint16_t      RX_Pin;
    uint16_t      RX_PinSource_X;
    uint8_t       RX_AF;

    // 运行时环形缓冲区（必须 volatile）
    volatile uint8_t  rx_buffer[RX_BUFFER_SIZE];
    volatile uint16_t rx_read_index;   // Tail：应用层读指针
    volatile uint16_t rx_write_index;  // Head：中断写指针
    volatile uint32_t rx_overflow_cnt; // 溢出计数（调试神器）

} UART_Handle_t;

/* ========================== 基础接口 ========================== */
void UART_Init(UART_Handle_t* UART_Handle);
void UART_Send_Data(UART_Handle_t* UART_Handle, const char* data, uint32_t len);
void UART_Send_AT_Command(UART_Handle_t* UART_Handle, const char* command);

/* ========================== 环形缓冲区高级接口（企业级必备） ========================== */

/**
 * @brief  获取环形缓冲区当前可用字节数
 * @param  handle: UART句柄
 * @retval 可用字节数
 */
uint16_t UART_RingBuf_Available(UART_Handle_t* handle);

/**
 * @brief  从环形缓冲区读取一个字节（非阻塞）
 * @param  handle: UART句柄
 * @param  pData:  读取的数据指针
 * @retval 1=成功 0=缓冲区空
 */
uint8_t UART_RingBuf_ReadByte(UART_Handle_t* handle, uint8_t* pData);

/**
 * @brief  读取一行数据（直到 \n 或超时），自动处理 \r\n
 * @param  handle:     UART句柄
 * @param  buf:        输出缓冲区
 * @param  max_len:    缓冲区最大长度（包含\0）
 * @param  timeout_ms: 超时时间（毫秒）
 * @retval 读取的字符数（不含结束符）
 */
uint16_t
UART_RingBuf_ReadLine(UART_Handle_t* handle, char* buf, uint16_t max_len, uint32_t timeout_ms);

/**
 * @brief  清空环形缓冲区（复位读写指针）
 * @param  handle: UART句柄
 */
void UART_RingBuf_Clear(UART_Handle_t* handle);

#endif