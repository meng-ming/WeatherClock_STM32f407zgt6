#include "global_variable_init.h"
#include "uart_driver.h"
#include "BSP_Tick_Delay.h"
#include "esp32_weather.h"
#include <stdio.h>
#include "st7789.h"
#include "lcd_font.h"

#define WIFI_SSID "7041"
#define WIFI_PWD "auto7041"
#define WEATHER_URL                                                                                \
    "https://api.seniverse.com/v3/weather/"                                                        \
    "now.json?key=SyLymtirx8EY1K_Dz&location=beijing&language=en&unit=c"

// UI 刷新辅助函数
void UI_Refresh_Weather(Weather_Info_t* info)
{
    // 1. 清除旧数据的区域 (用背景色填充矩形，防止重叠)
    // 假设背景是黑色 BLACK，字体是白色 WHITE
    // 也可以直接全屏清空 TFT_clear()，但会闪屏，局部刷新更好

    // 显示标题
    lcd_show_string(30, 10, "Weather Clock", &font_16, YELLOW, BLACK);

    // 显示分割线
    lcd_show_string(10, 30, "--------------------", &font_16, WHITE, BLACK);

    // 显示具体数据
    // City
    lcd_show_string(10, 50, "City:", &font_16, GREEN, BLACK);
    lcd_show_string(60, 50, info->city, &font_16, WHITE, BLACK); // Beijing

    // Weather
    lcd_show_string(10, 80, "Weat:", &font_16, GREEN, BLACK);
    lcd_show_string(60, 80, info->weather, &font_16, WHITE, BLACK); // Cloudy

    // Temp
    lcd_show_string(10, 110, "Temp:", &font_16, GREEN, BLACK);
    lcd_show_string(60, 110, info->temp, &font_16, RED, BLACK); // 6 C

    // Update Time
    lcd_show_string(10, 140, "Time:", &font_16, GREEN, BLACK);
    lcd_show_string(60, 140, info->update_time, &font_16, WHITE, BLACK);

    // 底部状态
    lcd_show_string(40, 200, "Updated!", &font_16, BLUE, BLACK);
}

int main(void)
{
    BSP_SysTick_Init();
    Global_Variable_Init();
    UART_Init(&g_debug_uart_handler);
    UART_Init(&g_esp_uart_handler);
    ST7789_Init();

    printf("\r\n=== Final Weather Clock Demo ===\r\n");

    if (!ESP32_Weather_Init(&g_esp_uart_handler))
    {
        printf("ESP32 Init Failed!\r\n");
        while (1)
            ;
    }

    if (!ESP32_WiFi_Connect(&g_esp_uart_handler, WIFI_SSID, WIFI_PWD))
    {
        printf("WiFi Connect Failed!\r\n");
        while (1)
            ;
    }

    Weather_Info_t info;
    if (ESP32_Get_Weather(&g_esp_uart_handler, WEATHER_URL, &info, 15000))
    {
        Weather_Print_Info(&info); // 串口打印保持
                                   // === 核心：上屏显示 ===
        TFT_clear();               // 为了简单先全刷，防止残影
        UI_Refresh_Weather(&info);

        // 休息
        BSP_Delay_ms(60000); // 1分钟后刷新
    }
    else
    {
        printf("Get Weather Failed!\r\n");
        lcd_show_string(40, 200, "Update Fail!", &font_16, RED, BLACK);
        BSP_Delay_ms(5000);
    }

    while (1)
        BSP_Delay_ms(1000);
}