// User/src/main.c
#include "stm32f4xx.h"
#include "uart_driver.h"    // 你的UART
#include "BSP_Tick_Delay.h" // SysTick延时
#include "misc.h"           // SystemInit
#include <stdio.h>          // printf
#include "global_variable_init.h"
#include "esp32_module.h"

int main(void)
{
    // 1. 系统Init：时钟默认168MHz
    SystemInit();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 中断分组

    // 2. GPIO时钟使能（UART AF用）
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // 3. SysTick延时（1ms tick）
    BSP_SysTick_Init();

    // 4. Debug UART Init（USART1 PA9/10 115200）
    UART_Init(&g_debug_uart_handler);

    // 5. 测试printf重定向（syscalls.c钩_write）
    printf("=== Weather Clock Boot OK! ===\r\n");
    printf("SysTick Ready, HCLK: %u Hz\r\n", (unsigned int) SystemCoreClock); // 应168000000

    ESP_Init(&g_esp_uart_handler); // 模块Init
    BSP_Delay_ms(3000);            // 延时3s，防ESP上电ready慢
    const char* ssid = "7041";     // 你的
    const char* pwd  = "auto7041"; // 你的
    if (ESP_WiFi_Connect(&g_esp_uart_handler, ssid, pwd))
    {
        printf("WiFi Ready! Get Weather\r\n");

        // HTTP拉天气
        char weather_json[1024] = {0};
        char api_url[512];
        sprintf(
            api_url,
            "http://v1.yiketianqi.com/free/"
            "day?appid=91768283&appsecret=b68BdGrM&unescape=1&cityid=101010100"); // 北京，换你的
        if (ESP_HTTP_Get_Weather(&g_esp_uart_handler, api_url, weather_json, sizeof(weather_json)))
        {
            printf("HTTP OK! JSON: %s\r\n", weather_json); // 粗暴打印，明天解析
        }
        else
        {
            printf("HTTP Fail, Retry Later\r\n");
        }
    }
    else
    {
        printf("WiFi Fail, Retry Later\r\n");
    }
}