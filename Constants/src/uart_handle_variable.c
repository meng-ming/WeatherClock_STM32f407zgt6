#include "uart_handle_variable.h"
#include "stm32f4xx.h" // 包含硬件定义

/* ==================================================================
 * 变量实体定义 (Definitions)
 * ================================================================== */

/**
 * @brief ESP32 串口句柄实例
 * @note  PA2(TX), PA3(RX), 115200, APB1
 */
UART_Handle_t g_esp_uart_handler = {.USART_X                   = USART2,
                                    .BaudRate                  = 115200,
                                    .USART_IRQ_Channel         = USART2_IRQn,
                                    .RCC_APBPeriph_USART_X     = RCC_APB1Periph_USART2,
                                    .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
                                    .Is_APB2                   = 0,

                                    // TX
                                    .TX_Port        = GPIOA,
                                    .TX_Pin         = GPIO_Pin_2, // 对应ESP32 GPIO_6
                                    .TX_PinSource_X = GPIO_PinSource2,
                                    .TX_AF          = GPIO_AF_USART2,

                                    // RX
                                    .RX_Port        = GPIOA,
                                    .RX_Pin         = GPIO_Pin_3, // 对应ESP32 GPIO_7
                                    .RX_PinSource_X = GPIO_PinSource3,
                                    .RX_AF          = GPIO_AF_USART2,

                                    // 状态初始化 (由 UART_Init 再次复位，这里给默认值)
                                    .rx_read_index   = 0,
                                    .rx_write_index  = 0,
                                    .rx_overflow_cnt = 0};

/**
 * @brief Debug 串口句柄实例
 * @note  PA9(TX), PA10(RX), 115200, APB2,可直接使用 type-C 接口连接电脑和单片机
 */
UART_Handle_t g_debug_uart_handler = {.USART_X                   = USART1,
                                      .BaudRate                  = 115200,
                                      .USART_IRQ_Channel         = USART1_IRQn,
                                      .RCC_APBPeriph_USART_X     = RCC_APB2Periph_USART1,
                                      .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
                                      .Is_APB2                   = 1,

                                      // TX
                                      .TX_Port        = GPIOA,
                                      .TX_Pin         = GPIO_Pin_9,
                                      .TX_PinSource_X = GPIO_PinSource9,
                                      .TX_AF          = GPIO_AF_USART1,

                                      // RX
                                      .RX_Port        = GPIOA,
                                      .RX_Pin         = GPIO_Pin_10,
                                      .RX_PinSource_X = GPIO_PinSource10,
                                      .RX_AF          = GPIO_AF_USART1,

                                      .rx_read_index   = 0,
                                      .rx_write_index  = 0,
                                      .rx_overflow_cnt = 0};