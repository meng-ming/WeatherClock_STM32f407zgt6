#include "lcd_font.h"
#include "st7789.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "lcd_image.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "uart_driver.h"
#include "BSP_Cortex_M4_Delay.h"
#include "uart_debug.h"
#include "string.h"

#define RX_LINE_MAX_LEN 256

// === 字库 16 的定义 ===
typedef struct
{
    uint16_t index;
    uint8_t  matrix[32]; // 16*16/8 = 32
} HZK_16_t;

extern const HZK_16_t HZK_16[];     // 你的数据数组
extern const uint8_t  ASCII_8x16[]; // 你的ASCII数组
extern const uint8_t  gImage_image[];

// 这里的配置对应 16 字体
font_info_t font_16 = {
    .ascii_w         = 8,
    .ascii_h         = 16,
    .ascii_map       = ASCII_8x16,
    .cn_w            = 16,
    .cn_h            = 16,
    .hzk_table       = HZK_16,
    .hzk_count       = 24,               // 假设有100个字
    .hzk_struct_size = sizeof(HZK_16_t), // 关键！告诉函数一步跨多大
    .hzk_data_offset = 2,                // index 占 2 字节
    .hzk_data_size   = 32                // 数据占 32 字节
};

UART_Handle_t g_esp_uart_handler = {
    .USART_X                   = USART2,
    .BaudRate                  = 115200,
    .USART_IRQ_Channel         = USART2_IRQn,
    .RCC_APBPeriph_USART_X     = RCC_APB1Periph_USART2,
    .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
    .Is_APB2                   = 0, // APB1 外设

    // TX: PA2
    .TX_Port        = GPIOA,
    .TX_Pin         = GPIO_Pin_2,
    .TX_PinSource_X = GPIO_PinSource2,
    .TX_AF          = GPIO_AF_USART2,

    // RX: PA3
    .RX_Port        = GPIOA,
    .RX_Pin         = GPIO_Pin_3,
    .RX_PinSource_X = GPIO_PinSource3,
    .RX_AF          = GPIO_AF_USART2,

    // 缓冲区和指针默认是 0
};

// ==========================================================
// 1. 调试串口句柄 (USART1)
// ==========================================================
UART_Handle_t g_debug_uart_handler = {
    // A. 硬件配置参数 (USART1 是 APB2 外设)
    .USART_X                   = USART1,
    .BaudRate                  = 115200,
    .USART_IRQ_Channel         = USART1_IRQn,           // 中断号
    .RCC_APBPeriph_USART_X     = RCC_APB2Periph_USART1, // APB2 时钟位
    .AHB_Clock_Enable_GPIO_Bit = RCC_AHB1Periph_GPIOA,
    .Is_APB2                   = 1, // APB2 外设

    // B. TX/RX 引脚配置 (PA9/TX, PA10/RX)
    .TX_Port        = GPIOA,
    .TX_Pin         = GPIO_Pin_9,
    .TX_PinSource_X = GPIO_PinSource9,
    .TX_AF          = GPIO_AF_USART1,

    .RX_Port        = GPIOA, // 简化：假设在同一端口
    .RX_Pin         = GPIO_Pin_10,
    .RX_PinSource_X = GPIO_PinSource10,
    .RX_AF          = GPIO_AF_USART1,

    // 注意：调试串口的 RX 功能通常可以省略初始化或只用于简单接收
};

int main(void)
{
    BSP_Cortex_M4_Delay_Init();
    UART_Init(&g_debug_uart_handler);

    printf("UART Debug Test: %d\n", 123);
    ST7789_Init();
    TFT_clear();
    // // 显示小字
    // // lcd_show_string(10, 10, "Small 16px", &font_16, WHITE, BLACK);
    // // lcd_show_string(10, 30, "朱家明喜欢叶祥梦9999", &font_16, RED, WHITE);

    // TFT_ShowImage(20, 20, 200, 200, gImage_image);
    // while (1)
    //     ;
    // 初始化 ESP32 通信链路
    UART_Init(&g_esp_uart_handler);

    BSP_Cortex_M4_Delay_ms(3000);

    lcd_show_string(0, 0, "STM32/ESP32 Test Start", &font_16, GREEN, BLACK);

    const char* data = "nihaoya!";

    UART_Send_Data(&g_debug_uart_handler, data, strlen(data));

    // --- 2. 裸机 Super-Loop 核心测试 ---

    char     rx_line[RX_LINE_MAX_LEN];
    bool     link_ok    = false;
    uint32_t send_count = 0;

    while (1)
    {
        // // A. 轮询接收缓冲区 (处理 ESP32 返回的所有数据行)
        // while (UART_Read_Line(&g_esp_uart_handler, rx_line, RX_LINE_MAX_LEN))
        // {
        //     // 收到一行数据，立即打印到调试串口
        //     printf_usart("RX[%u]: %s", send_count, rx_line);

        //     // 检查是否收到关键响应 (OK)
        //     if (strstr(rx_line, "OK") != NULL)
        //     {
        //         // 收到 OK，链路打通
        //         link_ok = true;
        //         printf_usart("\r\n--- SUCCESS: AT Link is Operational! ---\r\n");
        //         // 一旦成功，就跳出内层 while 循环
        //     }
        // }

        // // B. AT 命令管理 (定时发送 AT)
        // if (link_ok == false)
        // {
        //     // 链路未打通，持续发送 AT 进行测试
        //     UART_Send_AT_Command(&g_esp_uart_handler, "AT");
        //     send_count++;
        //     printf_usart("TX[%u]: AT\r\n", send_count);
        //     BSP_Cortex_M4_Delay_ms(1000); // 间隔1秒重试
        // }
        // else
        // {
        //     // 链路打通后，发送下一条命令（例如，查询版本）
        //     UART_Send_AT_Command(&g_esp_uart_handler, "AT+GMR");
        //     printf_usart("TX: AT+GMR\r\n");
        //     BSP_Cortex_M4_Delay_ms(5000); // 5秒后继续查询
        // }

        // // C. 其他主程序逻辑 (例如，LED 闪烁，检查按键)
        // // LED_Toggle();
        // // Delay_ms(100);
    }
}