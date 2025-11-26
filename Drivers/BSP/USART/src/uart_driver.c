/**
 * @file    uart_driver.c
 * @brief   STM32 通用 UART 驱动实现
 */

#include "uart_driver.h"
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

// 宏：参数检查
#define IS_HANDLE_VALID(h) ((h) != NULL && (h)->USART_X != NULL)

/**
 * @brief  内部函数：发送单字节 (阻塞等待 TXE)
 */
static void UART_Send_Byte(USART_TypeDef* USART_X, uint8_t data)
{
    // 等待发送数据寄存器空 (TXE)
    // 增加超时退出机制防止死锁 (虽然概率极低，但商业代码要防)
    uint32_t timeout = 0xFFFFF;
    while (USART_GetFlagStatus(USART_X, USART_FLAG_TXE) == RESET)
    {
        if (timeout-- == 0)
            return; // 硬件故障，直接退出
    }
    USART_SendData(USART_X, data);
}

/**
 * @brief  初始化 UART
 */
void UART_Init(UART_Handle_t* UART_Handle)
{
    if (UART_Handle == NULL)
        return;

    GPIO_InitTypeDef  GPIO_InitStructre;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    // 1. 初始化环形缓冲区指针 (重要！防止脏数据)
    UART_Handle->rx_read_index   = 0;
    UART_Handle->rx_write_index  = 0;
    UART_Handle->rx_overflow_cnt = 0;
    // memset((void*)UART_Handle->rx_buffer, 0, RX_BUFFER_SIZE); // 可选：清零内存

    // 2. 使能时钟
    if (UART_Handle->Is_APB2)
    {
        RCC_APB2PeriphClockCmd(UART_Handle->RCC_APBPeriph_USART_X, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(UART_Handle->RCC_APBPeriph_USART_X, ENABLE);
    }
    RCC_AHB1PeriphClockCmd(UART_Handle->AHB_Clock_Enable_GPIO_Bit, ENABLE);

    // 3. 配置 GPIO 复用
    GPIO_PinAFConfig(UART_Handle->TX_Port, UART_Handle->TX_PinSource_X, UART_Handle->TX_AF);
    GPIO_PinAFConfig(UART_Handle->RX_Port, UART_Handle->RX_PinSource_X, UART_Handle->RX_AF);

    GPIO_InitStructre.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructre.GPIO_Pin   = UART_Handle->TX_Pin | UART_Handle->RX_Pin;
    GPIO_InitStructre.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructre.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructre.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_Handle->TX_Port, &GPIO_InitStructre);

    // 4. 配置 USART 参数
    USART_InitStructure.USART_BaudRate            = UART_Handle->BaudRate;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_Init(UART_Handle->USART_X, &USART_InitStructure);

    // 5. 配置中断
    USART_ITConfig(UART_Handle->USART_X, USART_IT_RXNE, ENABLE);

    // 6. 配置 NVIC
    NVIC_InitStructure.NVIC_IRQChannel                   = UART_Handle->USART_IRQ_Channel;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_Init(&NVIC_InitStructure);

    // 7. 使能 USART
    USART_Cmd(UART_Handle->USART_X, ENABLE);
}

/**
 * @brief  发送数据
 */
void UART_Send_Data(UART_Handle_t* UART_Handle, const char* data, uint32_t data_len)
{
    if (!IS_HANDLE_VALID(UART_Handle) || data == NULL || data_len == 0)
        return;

    for (uint32_t i = 0; i < data_len; i++)
    {
        UART_Send_Byte(UART_Handle->USART_X, (uint8_t) data[i]);
    }
}

/**
 * @brief  发送 AT 命令
 */
void UART_Send_AT_Command(UART_Handle_t* UART_Handle, const char* command)
{
    if (!IS_HANDLE_VALID(UART_Handle) || command == NULL)
        return;

    UART_Send_Data(UART_Handle, command, strlen(command));
    UART_Send_Data(UART_Handle, "\r\n", 2);
}

/* ========================== 环形缓冲区接口实现 ========================== */

uint16_t UART_RingBuf_Available(UART_Handle_t* handle)
{
    if (!IS_HANDLE_VALID(handle))
        return 0;

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
        return 0;

    if (handle->rx_read_index == handle->rx_write_index)
    {
        return 0; // 缓冲区空
    }

    *pData                = handle->rx_buffer[handle->rx_read_index];
    handle->rx_read_index = (handle->rx_read_index + 1) % RX_BUFFER_SIZE;
    return 1;
}

uint16_t
UART_RingBuf_ReadLine(UART_Handle_t* handle, char* buf, uint16_t max_len, uint32_t timeout_ms)
{
    if (!IS_HANDLE_VALID(handle) || buf == NULL || max_len == 0)
        return 0;

    uint32_t start = BSP_GetTick_ms();
    uint16_t pos   = 0;
    uint8_t  ch;

    while ((BSP_GetTick_ms() - start) < timeout_ms)
    {
        if (UART_RingBuf_ReadByte(handle, &ch))
        {
            // 防止缓冲区溢出
            if (pos < max_len - 1)
            {
                buf[pos++] = ch;
            }

            if (ch == '\n')
            {
                buf[pos] = '\0'; // 封口
                // 移除行尾的 \r
                if (pos > 1 && buf[pos - 2] == '\r')
                {
                    buf[pos - 2] = '\0';
                    pos--; // 调整长度计数
                }
                // 移除行尾的 \n (如果只想留存纯内容，这里pos已经指向\0了)
                // 通常 readline 返回的字符串不包含 \r\n 会更方便处理
                if (pos > 0 && buf[pos - 1] == '\n')
                {
                    buf[pos - 1] = '\0';
                    pos--;
                }
                return pos;
            }
        }
        else
        {
            BSP_Delay_us(50); // 微小延时，释放总线压力
        }
    }

    buf[pos] = '\0'; // 超时封口
    return pos;
}

void UART_RingBuf_Clear(UART_Handle_t* handle)
{
    if (!IS_HANDLE_VALID(handle))
        return;

    handle->rx_read_index   = 0;
    handle->rx_write_index  = 0;
    handle->rx_overflow_cnt = 0;
}

/* ========================== 中断服务函数 ========================== */

// ESP32 模块串口中断
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t) USART_ReceiveData(USART2);

        uint16_t next_write = (g_esp_uart_handler.rx_write_index + 1) % RX_BUFFER_SIZE;

        if (next_write != g_esp_uart_handler.rx_read_index)
        {
            g_esp_uart_handler.rx_buffer[g_esp_uart_handler.rx_write_index] = data;
            g_esp_uart_handler.rx_write_index                               = next_write;
        }
        else
        {
            // 缓冲区满：丢弃最旧数据（最新优先策略），确保读到的是最新的
            g_esp_uart_handler.rx_overflow_cnt++;
            g_esp_uart_handler.rx_read_index =
                (g_esp_uart_handler.rx_read_index + 1) % RX_BUFFER_SIZE;
            g_esp_uart_handler.rx_buffer[g_esp_uart_handler.rx_write_index] = data;
            g_esp_uart_handler.rx_write_index                               = next_write;
        }
    }
}

// Debug 串口中断
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t) USART_ReceiveData(USART1);

        uint16_t next_write = (g_debug_uart_handler.rx_write_index + 1) % RX_BUFFER_SIZE;

        if (next_write != g_debug_uart_handler.rx_read_index)
        {
            g_debug_uart_handler.rx_buffer[g_debug_uart_handler.rx_write_index] = data;
            g_debug_uart_handler.rx_write_index                                 = next_write;
        }
        else
        {
            g_debug_uart_handler.rx_overflow_cnt++;
            g_debug_uart_handler.rx_read_index =
                (g_debug_uart_handler.rx_read_index + 1) % RX_BUFFER_SIZE;
            g_debug_uart_handler.rx_buffer[g_debug_uart_handler.rx_write_index] = data;
            g_debug_uart_handler.rx_write_index                                 = next_write;
        }
    }
}