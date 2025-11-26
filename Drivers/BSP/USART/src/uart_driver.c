#include "uart_driver.h"
#include "global_variable_init.h"
#include "misc.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "string.h"
#include <stdint.h>
#include "BSP_Tick_Delay.h"

extern UART_Handle_t g_esp_uart_handler;
extern UART_Handle_t g_debug_uart_handler;

// 内部函数，发送一个字节
static void UART_Send_Byte(USART_TypeDef* USART_X, uint8_t data)
{
    // 如果发送标志位为空
    while (USART_GetFlagStatus(USART_X, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(USART_X, data);
}

void UART_Init(UART_Handle_t* UART_Handle)
{
    // 结构变量
    GPIO_InitTypeDef  GPIO_InitStructre;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    // 1. 使能 USART 和 GPIO 对应时钟
    if (UART_Handle->Is_APB2)
    {
        RCC_APB2PeriphClockCmd(UART_Handle->RCC_APBPeriph_USART_X, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(UART_Handle->RCC_APBPeriph_USART_X, ENABLE);
    }

    RCC_AHB1PeriphClockCmd(UART_Handle->AHB_Clock_Enable_GPIO_Bit, ENABLE);

    // 2. 配置 GPIO 复用功能 (TX 和 RX)
    // 假设 TX/RX 在同一个端口，如果不在，需要分开两次 Init
    GPIO_PinAFConfig(UART_Handle->TX_Port, UART_Handle->TX_PinSource_X, UART_Handle->TX_AF);
    GPIO_PinAFConfig(UART_Handle->RX_Port, UART_Handle->RX_PinSource_X, UART_Handle->RX_AF);

    GPIO_InitStructre.GPIO_Mode  = GPIO_Mode_AF; // 复用功能
    GPIO_InitStructre.GPIO_Pin   = UART_Handle->TX_Pin | UART_Handle->RX_Pin;
    GPIO_InitStructre.GPIO_PuPd  = GPIO_PuPd_UP;  // 启用上拉电阻
    GPIO_InitStructre.GPIO_OType = GPIO_OType_PP; // 推挽输出
    GPIO_InitStructre.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_Handle->TX_Port, &GPIO_InitStructre);

    // 3. 配置 USART 参数
    USART_InitStructure.USART_BaudRate            = UART_Handle->BaudRate;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx; // 发送和接收
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_Init(UART_Handle->USART_X, &USART_InitStructure);

    // 4. 配置中断
    USART_ITConfig(UART_Handle->USART_X, USART_IT_RXNE, ENABLE);

    // 5. 配置 NVIC
    NVIC_InitStructure.NVIC_IRQChannel                   = UART_Handle->USART_IRQ_Channel;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_Init(&NVIC_InitStructure);

    // 6. 使能 USART
    USART_Cmd(UART_Handle->USART_X, ENABLE);
}

/**
 * @brief UART 发送数据
 */
void UART_Send_Data(UART_Handle_t* UART_Handle, const char* data, uint32_t data_len)
{
    uint32_t i;
    for (i = 0; i < data_len; i++)
    {
        UART_Send_Byte(UART_Handle->USART_X, data[i]);
    }
}

/**
 * @brief UART 发送AT命令，自动添加 '\r\n'
 */
void UART_Send_AT_Command(UART_Handle_t* UART_Handle, const char* command)
{
    UART_Send_Data(UART_Handle, command, strlen(command));
    UART_Send_Data(UART_Handle, "\r\n", 2);
}

/* ========================== 环形缓冲区接口实现 ========================== */

uint16_t UART_RingBuf_Available(UART_Handle_t* handle)
{
    return (RX_BUFFER_SIZE + handle->rx_write_index - handle->rx_read_index) % RX_BUFFER_SIZE;
}

uint8_t UART_RingBuf_ReadByte(UART_Handle_t* handle, uint8_t* pData)
{
    if (handle->rx_read_index == handle->rx_write_index)
        return 0; // 空

    *pData                = handle->rx_buffer[handle->rx_read_index];
    handle->rx_read_index = (handle->rx_read_index + 1) % RX_BUFFER_SIZE;
    return 1;
}

uint16_t
UART_RingBuf_ReadLine(UART_Handle_t* handle, char* buf, uint16_t max_len, uint32_t timeout_ms)
{
    uint32_t start = BSP_GetTick_ms();
    uint16_t pos   = 0;
    uint8_t  ch;

    while ((BSP_GetTick_ms() - start) < timeout_ms)
    {
        if (UART_RingBuf_ReadByte(handle, &ch))
        {
            if (pos < max_len - 1)
                buf[pos++] = ch;

            if (ch == '\n')
            {
                buf[pos] = '\0';
                // 去掉前面的 \r
                if (pos > 0 && buf[pos - 1] == '\r')
                    buf[pos - 1] = '\0';
                return pos;
            }
        }
        else
        {
            BSP_Delay_us(50); // 微小延时，省CPU
        }
    }

    buf[pos] = '\0';
    return pos; // 超时返回
}

void UART_RingBuf_Clear(UART_Handle_t* handle)
{
    handle->rx_read_index   = 0;
    handle->rx_write_index  = 0;
    handle->rx_overflow_cnt = 0;
}

/* ========================== 中断服务函数（必须放在 .c 文件） ========================== */

/* ESP32 模块串口中断 */
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t) USART_ReceiveData(USART2);

        uint16_t next_write = (g_esp_uart_handler.rx_write_index + 1) % RX_BUFFER_SIZE;

        if (next_write != g_esp_uart_handler.rx_read_index)
        {
            // 缓冲区未满，正常写入
            g_esp_uart_handler.rx_buffer[g_esp_uart_handler.rx_write_index] = data;
            g_esp_uart_handler.rx_write_index                               = next_write;
        }
        else
        {
            // 缓冲区满：覆盖最旧数据（最新优先策略）+ 溢出计数
            g_esp_uart_handler.rx_overflow_cnt++;
            g_esp_uart_handler.rx_read_index =
                (g_esp_uart_handler.rx_read_index + 1) % RX_BUFFER_SIZE; // 丢弃最旧
            g_esp_uart_handler.rx_buffer[g_esp_uart_handler.rx_write_index] = data;
            g_esp_uart_handler.rx_write_index                               = next_write;
        }
    }
    // RXNE 标志读取 DR 后自动清除，无需手动清
}

/* Debug 串口中断（USART1） */
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
