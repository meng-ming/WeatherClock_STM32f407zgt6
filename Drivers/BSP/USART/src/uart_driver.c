#include "uart_driver.h"
#include "misc.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "string.h"
#include <stdint.h>

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

/**
 * @brief 从环形缓冲区中读取一行数据 (以 \n 结束)
 */
bool UART_Read_Line(UART_Handle_t* UART_Handle, char* out_buffer, uint16_t max_len)
{
    // 逻辑不变，但使用 UART_Handle-> 访问缓冲区
    uint16_t start_index   = UART_Handle->rx_read_index;
    bool     found_newline = false;

    // 1. 扫描缓冲区
    while (start_index != UART_Handle->rx_write_index)
    {
        if (UART_Handle->rx_buffer[start_index] == '\n')
        {
            found_newline = true;
            break;
        }
        start_index = (start_index + 1) % RX_BUFFER_SIZE;
    }

    if (found_newline)
    {
        // 2. 拷贝数据并移动读指针
        uint16_t i = 0;
        __disable_irq(); // 关中断！

        while (UART_Handle->rx_read_index != start_index)
        {
            if (i < max_len - 1)
            {
                out_buffer[i++] = UART_Handle->rx_buffer[UART_Handle->rx_read_index];
            }
            UART_Handle->rx_read_index = (UART_Handle->rx_read_index + 1) % RX_BUFFER_SIZE;
        }

        // 拷贝最后一个 \n 字符并跳过
        if (i < max_len - 1)
        {
            out_buffer[i++] = UART_Handle->rx_buffer[UART_Handle->rx_read_index];
        }
        UART_Handle->rx_read_index = (UART_Handle->rx_read_index + 1) % RX_BUFFER_SIZE;

        __enable_irq(); // 开中断

        out_buffer[i] = '\0';
        return true;
    }

    return false;
}

/**
 * @brief 通用处理中断函数
 */
void UART_IRQ_Handler(UART_Handle_t* UART_Handle)
{
    uint8_t  rx_data;
    uint16_t next_write_index;

    // 1. 检查是否是接收中断
    if (USART_GetITStatus(UART_Handle->USART_X, USART_IT_RXNE) == SET)
    {
        // 2.立刻读取数据，清除中断标志
        rx_data = USART_ReceiveData(UART_Handle->USART_X);

        // 3. 计算环形缓冲区索引
        next_write_index = (UART_Handle->rx_write_index + 1) % RX_BUFFER_SIZE;

        // 4.防溢出检查
        // 如果写入位置等于读取位置，说明Buffer满了,选择覆盖旧数据

        // 这里我教你个严谨的：如果还没满，才写入
        if (next_write_index != UART_Handle->rx_read_index)
        {
            UART_Handle->rx_buffer[UART_Handle->rx_write_index] = rx_data;
            UART_Handle->rx_write_index                         = next_write_index;
        }
        else
        {
            // 记录一个错误标志，或者这里做一个极简的计数
            // UART_Handle->buffer_overflow_count++;
            // 实际项目中这里要报警
        }
    }

    // 5.【错误处理】这一段必须要有，否则一旦出现溢出(ORE)，中断可能会卡死或者一直进不来
    if (USART_GetFlagStatus(UART_Handle->USART_X, USART_FLAG_ORE) != RESET)
    {
        // 读取数据寄存器可以清除 ORE 标志位 (根据STM32手册)
        (void) USART_ReceiveData(UART_Handle->USART_X);
    }
}

extern UART_Handle_t g_esp_uart_handler;   // 引入全局句柄
extern UART_Handle_t g_debug_uart_handler; // 引入全局句柄

// USART2 专用中断函数名
void USART2_IRQHandler(void)
{
    // 调用通用处理函数，并传入 USART2 对应的句柄
    UART_IRQ_Handler(&g_esp_uart_handler);
}

// USART1 专用中断函数名
void USART1_IRQHandler(void)
{
    // 调用通用处理函数，并传入 USART1 对应的句柄
    UART_IRQ_Handler(&g_debug_uart_handler);
}
