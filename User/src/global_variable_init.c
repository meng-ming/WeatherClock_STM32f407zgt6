#include "global_variable_init.h"
#include <string.h>

// 字库数组
extern const HZK_16_t HZK_16[];       //
extern const uint8_t  ASCII_8x16[];   //
extern const uint8_t  gImage_image[]; //

// 字体配置
font_info_t font_16 = {.ascii_w         = 8,
                       .ascii_h         = 16,
                       .ascii_map       = ASCII_8x16,
                       .cn_w            = 16,
                       .cn_h            = 16,
                       .hzk_table       = HZK_16,
                       .hzk_count       = 24,
                       .hzk_struct_size = sizeof(HZK_16_t),
                       .hzk_data_offset = 2,
                       .hzk_data_size   = 32};

// UART句柄定义（参数不变）
UART_Handle_t g_esp_uart_handler = {
    .USART_X                   = USART2,
    .BaudRate                  = 115200,
    .USART_IRQ_Channel         = USART2_IRQn,
    .RCC_APBPeriph_USART_X     = RCC_APB1Periph_USART2,
    .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
    .Is_APB2                   = 0,
    .TX_Port                   = GPIOA,
    .TX_Pin                    = GPIO_Pin_2,
    .TX_PinSource_X            = GPIO_PinSource2,
    .TX_AF                     = GPIO_AF_USART2,
    .RX_Port                   = GPIOA,
    .RX_Pin                    = GPIO_Pin_3,
    .RX_PinSource_X            = GPIO_PinSource3,
    .RX_AF                     = GPIO_AF_USART2,
    // 缓冲/索引0默认
};

UART_Handle_t g_debug_uart_handler = {
    .USART_X                   = USART1,
    .BaudRate                  = 115200,
    .USART_IRQ_Channel         = USART1_IRQn,
    .RCC_APBPeriph_USART_X     = RCC_APB2Periph_USART1,
    .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
    .Is_APB2                   = 1,
    .TX_Port                   = GPIOA,
    .TX_Pin                    = GPIO_Pin_9,
    .TX_PinSource_X            = GPIO_PinSource9,
    .TX_AF                     = GPIO_AF_USART1,
    .RX_Port                   = GPIOA,
    .RX_Pin                    = GPIO_Pin_10,
    .RX_PinSource_X            = GPIO_PinSource10,
    .RX_AF                     = GPIO_AF_USART1,
    // 缓冲/索引0默认
};

void Global_Variable_Init(void)
{
    // UART句柄缓冲区初始化
    memset((void*) g_esp_uart_handler.rx_buffer, 0, RX_BUFFER_SIZE);
    g_esp_uart_handler.rx_read_index  = 0;
    g_esp_uart_handler.rx_write_index = 0;

    memset((void*) g_debug_uart_handler.rx_buffer, 0, RX_BUFFER_SIZE);
    g_debug_uart_handler.rx_read_index  = 0;
    g_debug_uart_handler.rx_write_index = 0;

    // 环形缓冲区必须清零
    g_esp_uart_handler.rx_read_index   = 0;
    g_esp_uart_handler.rx_write_index  = 0;
    g_esp_uart_handler.rx_overflow_cnt = 0;

    g_debug_uart_handler.rx_read_index   = 0;
    g_debug_uart_handler.rx_write_index  = 0;
    g_debug_uart_handler.rx_overflow_cnt = 0;
}