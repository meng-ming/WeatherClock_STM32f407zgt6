#include "app_ui.h"
#include "st7789.h"   // 引用驱动层
#include "lcd_font.h" // 引用字体
#include "font_variable.h"
#include <stdio.h>

// 初始化 UI
void App_UI_Init(void)
{
    // 1. 初始化底层屏幕驱动
    ST7789_Init();
    TFT_clear();

    // 2. 画静态框架
    LCD_Show_String(30, 10, "Weather Clock", &font_16, YELLOW, BLACK);
    LCD_Show_String(10, 30, "--------------------", &font_16, WHITE, BLACK);

    // 画标签 (Label)
    LCD_Show_String(10, 50, "City:", &font_16, GREEN, BLACK);
    LCD_Show_String(10, 80, "Weat:", &font_16, GREEN, BLACK);
    LCD_Show_String(10, 110, "Temp:", &font_16, GREEN, BLACK);
    LCD_Show_String(10, 140, "Time:", &font_16, GREEN, BLACK);
}

// 更新动态数据
void App_UI_Update(const App_Weather_Data_t* data)
{
    // 直接覆盖显示动态数据
    LCD_Show_String(60, 50, data->city, &font_16, WHITE, BLACK);
    LCD_Show_String(60, 80, data->weather, &font_16, WHITE, BLACK);
    LCD_Show_String(60, 110, data->temp, &font_16, RED, BLACK);
    LCD_Show_String(60, 140, data->update_time, &font_16, WHITE, BLACK);

    // 更新底部状态为成功
    App_UI_ShowStatus("Updated!", BLUE);
}

// 显示状态信息 (自带局部擦除功能)
void App_UI_ShowStatus(const char* status, uint16_t color)
{
    // 1. 串口打印 (增加这行，作为 Log 输出)
    printf("[System Status] %s\r\n", status);

    // 2. 屏幕显示 (保持不变)
    // 简单覆盖法：先打印一串空格清空旧状态
    LCD_Show_String(40, 200, "                ", &font_16, BLACK, BLACK);
    LCD_Show_String(40, 200, status, &font_16, color, BLACK);
}