/**
 * @file    global_variable_init.c
 * @brief   全局变量管理与初始化实现
 */

#include "global_variable_init.h"
#include <string.h>

/* ==================================================================
 * 字体配置实例化
 * ================================================================== */

/**
 * @brief 默认 16 点阵字体配置
 */
font_info_t font_16 = {.ascii_w         = 8,
                       .ascii_h         = 16,
                       .ascii_map       = ASCII_8x16,
                       .cn_w            = 16,
                       .cn_h            = 16,
                       .hzk_table       = HZK_16,
                       .hzk_count       = 24, // 根据实际字库大小调整
                       .hzk_struct_size = sizeof(HZK_16_t),
                       .hzk_data_offset = 2, // 偏移量，跳过 index 字段
                       .hzk_data_size   = 32};

/* ==================================================================
 * UART 句柄实例化 (硬件配置)
 * ================================================================== */

/**
 * @brief ESP32 串口句柄 (USART2)
 * @note  PA2(TX), PA3(RX), 115200, APB1
 */
UART_Handle_t g_esp_uart_handler = {.USART_X                   = USART2,
                                    .BaudRate                  = 115200,
                                    .USART_IRQ_Channel         = USART2_IRQn,
                                    .RCC_APBPeriph_USART_X     = RCC_APB1Periph_USART2,
                                    .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
                                    .Is_APB2                   = 0,

                                    // TX 引脚配置
                                    .TX_Port        = GPIOA,
                                    .TX_Pin         = GPIO_Pin_2,
                                    .TX_PinSource_X = GPIO_PinSource2,
                                    .TX_AF          = GPIO_AF_USART2,

                                    // RX 引脚配置
                                    .RX_Port        = GPIOA,
                                    .RX_Pin         = GPIO_Pin_3,
                                    .RX_PinSource_X = GPIO_PinSource3,
                                    .RX_AF          = GPIO_AF_USART2,

                                    // 运行时状态 (默认全 0，由 UART_Init 负责最终复位)
                                    .rx_read_index   = 0,
                                    .rx_write_index  = 0,
                                    .rx_overflow_cnt = 0};

/**
 * @brief Debug 串口句柄 (USART1)
 * @note  PA9(TX), PA10(RX), 115200, APB2
 */
UART_Handle_t g_debug_uart_handler = {.USART_X                   = USART1,
                                      .BaudRate                  = 115200,
                                      .USART_IRQ_Channel         = USART1_IRQn,
                                      .RCC_APBPeriph_USART_X     = RCC_APB2Periph_USART1,
                                      .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
                                      .Is_APB2                   = 1,

                                      // TX 引脚配置
                                      .TX_Port        = GPIOA,
                                      .TX_Pin         = GPIO_Pin_9,
                                      .TX_PinSource_X = GPIO_PinSource9,
                                      .TX_AF          = GPIO_AF_USART1,

                                      // RX 引脚配置
                                      .RX_Port        = GPIOA,
                                      .RX_Pin         = GPIO_Pin_10,
                                      .RX_PinSource_X = GPIO_PinSource10,
                                      .RX_AF          = GPIO_AF_USART1,

                                      // 运行时状态
                                      .rx_read_index   = 0,
                                      .rx_write_index  = 0,
                                      .rx_overflow_cnt = 0};

/* ==================================================================
 * 初始化函数实现
 * ================================================================== */

/**
 * @brief  全局变量初始化
 */
void Global_Variable_Init(void)
{
    // 目前这里为空，因为：
    // 1. 静态变量已经在定义时有了初始值。
    // 2. 动态状态（如 RingBuffer 指针）已移交到底层驱动的 Init 函数中处理。
    //
    // 预留此接口是为了将来如果有需要在运行时动态计算的全局参数，可以在这里初始化。
    // 例如：加载 Flash 中的用户配置到全局变量等。

    // Example:
    // Load_Config_From_Flash();
}