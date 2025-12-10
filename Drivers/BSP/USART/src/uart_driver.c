/**
 * @file    uart_driver.c
 * @brief   STM32 通用 UART 驱动实现 (DMA + RingBuffer 高性能版)
 * @author  meng-ming
 * @version 1.2
 * @date    2025-12-07
 * @note    支持 DMA 循环接收不定长数据，集成空闲中断 (IDLE) 处理机制。
 * 符合 MISRA-C 规范思想，增强了内存安全性和代码健壮性。
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "uart_driver.h"
#include "stm32f4xx_dma.h"
#include "uart_handle_variable.h"
#include "misc.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "string.h"
#include <stdint.h>
#include <stdio.h>
#include "BSP_Tick_Delay.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"

// 外部变量声明
extern TaskHandle_t WeatherTask_Handler;

/* ==================================================================
 * 宏定义与私有函数声明
 * ================================================================== */

// 参数检查宏：确保句柄指针和外设基地址有效
#define IS_HANDLE_VALID(h) ((h) != NULL && (h)->USART_X != NULL)

static void UART_Send_Byte(USART_TypeDef* USART_X, uint8_t data)
{
    // 设置超时阈值，防止因硬件故障导致的死循环 (Watchdog思想)
    uint32_t timeout = 0xFFFFF;

    // 等待发送数据寄存器为空 (TXE: Transmit Data Register Empty)
    while (USART_GetFlagStatus(USART_X, USART_FLAG_TXE) == RESET)
    {
        if (timeout-- == 0)
        {
            return; // 硬件故障，直接返回，避免死机
        }
    }

    // 写入数据到 DR 寄存器
    USART_SendData(USART_X, data);
}

/* ==================================================================
 * 核心驱动接口实现
 * ================================================================== */

/**
 * @brief  配置外设与 GPIO 时钟
 */
static void UART_Config_Clock(UART_Handle_t* handle)
{
    // 1. 使能 USART 时钟
    if (handle->Is_APB2)
    {
        RCC_APB2PeriphClockCmd(handle->RCC_APBPeriph_USART_X, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(handle->RCC_APBPeriph_USART_X, ENABLE);
    }

    // 2. 使能 GPIO 时钟
    RCC_AHB1PeriphClockCmd(handle->AHB_Clock_Enable_GPIO_Bit, ENABLE);
}

/**
 * @brief  配置 GPIO 引脚复用
 */
static void UART_Config_GPIO(UART_Handle_t* handle)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 设置引脚复用映射 (AF)
    GPIO_PinAFConfig(handle->TX_Port, handle->TX_PinSource_X, handle->TX_AF);
    GPIO_PinAFConfig(handle->RX_Port, handle->RX_PinSource_X, handle->RX_AF);

    // 2. 初始化 GPIO 参数 (复用推挽, 上拉, 高速)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    // TX 引脚
    GPIO_InitStructure.GPIO_Pin = handle->TX_Pin;
    GPIO_Init(handle->TX_Port, &GPIO_InitStructure);

    // RX 引脚 (通常配置相同)
    GPIO_InitStructure.GPIO_Pin = handle->RX_Pin;
    GPIO_Init(handle->RX_Port, &GPIO_InitStructure);
}

/**
 * @brief  配置 NVIC 中断优先级
 */
static void UART_Config_NVIC(UART_Handle_t* handle)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    NVIC_InitStructure.NVIC_IRQChannel                   = handle->USART_IRQ_Channel;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0; // 子优先级
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  配置 DMA 传输
 */
static void UART_Config_DMA(UART_Handle_t* handle)
{
    if (handle->RX_DMA_Stream == NULL)
    {
        // 如果没配 DMA，直接退出，不要误操作
        return;
    }

    DMA_InitTypeDef DMA_InitStructure;

    // 1. 开启 DMA 时钟
    RCC_AHB1PeriphClockCmd(handle->RCC_AHB1Periph_DMA_X, ENABLE);

    // 2. 复位 DMA 流
    DMA_DeInit(handle->RX_DMA_Stream);

    // 3. 填充参数
    DMA_InitStructure.DMA_Channel            = handle->RX_DMA_Channel;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &handle->USART_X->DR; // 关键：取地址
    DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t) handle->rx_buffer;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize         = RX_BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular; // 循环模式
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;

    DMA_Init(handle->RX_DMA_Stream, &DMA_InitStructure);

    // 4. 开启 DMA 和 串口 DMAR
    DMA_Cmd(handle->RX_DMA_Stream, ENABLE);
    USART_DMACmd(handle->USART_X, USART_DMAReq_Rx, ENABLE);
}

/* ==================================================================
 * 核心驱动接口实现 (API)
 * ================================================================== */

void UART_Init(UART_Handle_t* handle)
{
    if (!IS_HANDLE_VALID(handle))
        return;

    USART_InitTypeDef USART_InitStructure;

    // 1. 初始化软件状态
    handle->rx_read_index   = 0;
    handle->rx_write_index  = 0;
    handle->rx_overflow_cnt = 0;

    // 2. 硬件底层配置 (调用辅助函数)
    UART_Config_Clock(handle);
    UART_Config_GPIO(handle);
    UART_Config_NVIC(handle);

    // 3. 配置 USART 通用参数
    USART_InitStructure.USART_BaudRate            = handle->BaudRate;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_Init(handle->USART_X, &USART_InitStructure);

    // 4. 使能串口
    USART_Cmd(handle->USART_X, ENABLE);

    // 5. 根据模式配置数据接收策略 (核心分支)
    if (handle->RX_DMA_Stream != NULL)
    {
        // === DMA 模式 ===
        UART_Config_DMA(handle);

        // 策略：开启 IDLE 中断，关闭 RXNE 中断 (防止冲突)
        USART_ITConfig(handle->USART_X, USART_IT_RXNE, DISABLE);
        USART_ITConfig(handle->USART_X, USART_IT_IDLE, ENABLE);
    }
    else
    {
        // === 中断模式 ===
        // 策略：仅开启 RXNE 中断
        USART_ITConfig(handle->USART_X, USART_IT_RXNE, ENABLE);
        USART_ITConfig(handle->USART_X, USART_IT_IDLE, DISABLE);
    }
}

void UART_Send_Data(UART_Handle_t* handle, const char* data, uint32_t data_len)
{
    if (!IS_HANDLE_VALID(handle) || data == NULL || data_len == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < data_len; i++)
    {
        UART_Send_Byte(handle->USART_X, (uint8_t) data[i]);
    }
}

void UART_Send_AT_Command(UART_Handle_t* handle, const char* command)
{
    if (!IS_HANDLE_VALID(handle) || command == NULL)
    {
        return;
    }

    UART_Send_Data(handle, command, strlen(command));
    UART_Send_Data(handle, "\r\n", 2);
}

/* ==================================================================
 * 环形缓冲区 (RingBuffer) 接口实现
 * ================================================================== */

uint16_t UART_RingBuf_Available(UART_Handle_t* handle)
{
    if (!IS_HANDLE_VALID(handle))
    {
        return 0;
    }

    // 如果启用了 DMA，主动查询 CNDTR 寄存器，实时更新 write_index
    // 确保在 IDLE 中断未触发时（如连续数据流），应用层也能读到最新数据
    if (handle->RX_DMA_Stream != NULL)
    {
        // DMA_GetCurrDataCounter 返回的是“剩余传输量”
        uint32_t remaining     = DMA_GetCurrDataCounter(handle->RX_DMA_Stream);
        handle->rx_write_index = RX_BUFFER_SIZE - remaining;
    }

    // 计算有效数据长度 (处理环形回绕)
    if (handle->rx_write_index >= handle->rx_read_index)
    {
        return handle->rx_write_index - handle->rx_read_index;
    }
    else
    {
        return RX_BUFFER_SIZE - (handle->rx_read_index - handle->rx_write_index);
    }
}

uint8_t UART_RingBuf_ReadByte(UART_Handle_t* handle, uint8_t* pData)
{
    if (!IS_HANDLE_VALID(handle) || pData == NULL)
    {
        return 0;
    }

    // 缓冲区空
    if (handle->rx_read_index == handle->rx_write_index)
    {
        return 0;
    }

    // 读取数据并推进读指针
    *pData                = handle->rx_buffer[handle->rx_read_index];
    handle->rx_read_index = (handle->rx_read_index + 1) % RX_BUFFER_SIZE;

    return 1; // 成功
}

uint16_t
UART_RingBuf_ReadLine(UART_Handle_t* handle, char* buf, uint16_t max_len, uint32_t timeout_ms)
{
    if (!IS_HANDLE_VALID(handle) || buf == NULL || max_len == 0)
    {
        return 0;
    }

    uint32_t start_tick = BSP_GetTick_ms();
    uint16_t pos        = 0;
    uint8_t  ch         = 0;

    while ((BSP_GetTick_ms() - start_tick) < timeout_ms)
    {
        if (UART_RingBuf_ReadByte(handle, &ch))
        {
            // 防止输出缓冲区溢出
            if (pos < max_len - 1)
            {
                buf[pos++] = ch;
            }

            // 检测换行符 (兼容 \n 和 \r\n)
            if (ch == '\n')
            {
                buf[pos] = '\0'; // 添加字符串结束符，形成 C 字符串

                // 可选：去除行尾的 \r 和 \n，只保留纯文本内容
                // 这里为了通用性，先保留逻辑，根据需要调整
                if (pos > 1 && buf[pos - 2] == '\r')
                {
                    buf[pos - 2] = '\0';
                    pos--;
                }
                else if (pos > 0 && buf[pos - 1] == '\n')
                {
                    buf[pos - 1] = '\0';
                    pos--;
                }
                return pos; // 成功读取一行
            }
        }
        else
        {
            // 缓冲区暂无数据，微小延时释放总线压力
            BSP_Delay_us(50);
        }
    }

    buf[pos] = '\0'; // 超时未读完一行，也要封口
    return pos;
}

void UART_RingBuf_Clear(UART_Handle_t* handle)
{
    if (!IS_HANDLE_VALID(handle))
    {
        return;
    }

    handle->rx_read_index   = handle->rx_write_index;
    handle->rx_overflow_cnt = 0;
}

/* ==================================================================
 * 中断服务函数 (ISR)
 * ================================================================== */

void USART2_IRQHandler(void)
{
    // 定义 volatile 变量以防止编译器优化读取序列
    volatile uint32_t isr_flags = USART2->SR;
    volatile uint32_t dr_data;
    (void) dr_data;

    // [1] 处理 IDLE (空闲中断)：检测到一帧数据传输完成
    if (isr_flags & USART_FLAG_IDLE)
    {
        // 1.1 清除标志位 (序列：读SR -> 读DR)
        dr_data = USART2->DR;

        // 1.2 更新 DMA 写指针
        if (g_esp_uart_handler.RX_DMA_Stream != NULL)
        {
            uint32_t remaining = DMA_GetCurrDataCounter(g_esp_uart_handler.RX_DMA_Stream);
            g_esp_uart_handler.rx_write_index = RX_BUFFER_SIZE - remaining;
        }

        // 1.3 通知 Weather 任务
        if (WeatherTask_Handler != NULL)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            // 发送直达通知 (Give)
            vTaskNotifyGiveFromISR(WeatherTask_Handler, &xHigherPriorityTaskWoken);

            // 如果被叫醒的任务优先级比当前运行的任务高，
            // 那么中断退出时直接进行上下文切换，实现"零延迟"响应
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

    // [2] 错误处理 (ORE/NE/FE/PE)
    if (isr_flags & (USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE | USART_FLAG_PE))
    {
        dr_data = USART2->DR; // 读数据寄存器清除错误标志
        (void) dr_data;
    }

    // [3] 兜底 RXNE (防止配置失误卡死中断)
    if (isr_flags & USART_FLAG_RXNE)
    {
        dr_data = USART2->DR;
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t) USART_ReceiveData(USART1);

        // 计算下一个写位置
        uint16_t next_write = (g_debug_uart_handler.rx_write_index + 1) % RX_BUFFER_SIZE;

        // 检查缓冲区是否满
        if (next_write != g_debug_uart_handler.rx_read_index)
        {
            g_debug_uart_handler.rx_buffer[g_debug_uart_handler.rx_write_index] = data;
            g_debug_uart_handler.rx_write_index                                 = next_write;
        }
        else
        {
            // 缓冲区溢出：策略为丢弃最旧的数据 (覆盖旧数据，优先保留最新的)
            g_debug_uart_handler.rx_overflow_cnt++;
            // 读指针被迫前移，丢弃一个旧字节
            g_debug_uart_handler.rx_read_index =
                (g_debug_uart_handler.rx_read_index + 1) % RX_BUFFER_SIZE;

            g_debug_uart_handler.rx_buffer[g_debug_uart_handler.rx_write_index] = data;
            g_debug_uart_handler.rx_write_index                                 = next_write;
        }
    }
}