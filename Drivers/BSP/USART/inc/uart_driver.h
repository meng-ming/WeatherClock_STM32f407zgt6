#ifndef __UART_DRIVER_H
#define __UART_DRIVER_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

// 接收缓冲区定义
#define RX_BUFFER_SIZE 2048

// 定义通用结构体
typedef struct
{
    // 1. 硬件配置参数（初始化时传入）
    USART_TypeDef* USART_X;               // 对应外设的基地址（例如USART1）
    uint32_t       BaudRate;              // 波特率
    uint8_t        USART_IRQ_Channel;     // NVIC中断通道
    uint32_t       RCC_APBPeriph_USART_X; // 对应外设的时钟使能位（例如RCC_APB1Periph_USART2)
    uint8_t        Is_APB2;               // 0:APB1,1:APB2

    // 2. TX/RX 引脚配置参数（初始化时传入）
    uint32_t      AHB_Clock_Enable_GPIO_Bit; // RX和TX所在引脚
    GPIO_TypeDef* TX_Port;
    uint16_t      TX_Pin;
    uint16_t      TX_PinSource_X;
    uint8_t       TX_AF;

    GPIO_TypeDef* RX_Port;
    uint16_t      RX_Pin;
    uint16_t      RX_PinSource_X;
    uint8_t       RX_AF;

    // 3. 运行时状态和缓冲区
    volatile uint8_t  rx_buffer[RX_BUFFER_SIZE];
    volatile uint16_t rx_read_index;
    volatile uint16_t rx_write_index;

} UART_Handle_t;

// 通用接口函数 (传入句柄指针)
void UART_Init(UART_Handle_t* UART_Handle);
void UART_Send_Data(UART_Handle_t* UART_Handle, const char* data, uint32_t len);
void UART_Send_AT_Command(UART_Handle_t* UART_Handle, const char* command);
bool UART_Read_Line(UART_Handle_t* UART_Handle, char* out_buffer, uint16_t max_len);

// 中断处理函数 (供 stm32f4xx_it.c 调用)
void UART_IRQ_Handler(UART_Handle_t* UART_Handle);
#endif