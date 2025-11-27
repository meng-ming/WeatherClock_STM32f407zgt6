#include "app_ui.h"
#include "st7789.h"
#include "lcd_font.h"
#include "font_variable.h"
#include <stdio.h>

// 引入新做好的页面模块
#include "ui_main_page.h"

void App_UI_Init(void)
{
    // 1. 底层初始化
    ST7789_Init();

    // 2. 调用主页面初始化 (画出5个格子)
    UI_MainPage_Init();

    // 3. 初始显示
    App_UI_ShowStatus("System Init...", BLACK);
}

void App_UI_Update(const App_Weather_Data_t* data)
{
    // 调用主页面的更新逻辑
    UI_MainPage_Update(data);

    // 底部状态栏更新
    App_UI_ShowStatus("Updated!", BLACK);
}

void App_UI_ShowStatus(const char* status, uint16_t color)
{
    // 串口打印日志
    printf("[UI Status] %s\r\n", status);

    // 在屏幕最下方显示 (如果不希望覆盖室内数据模块，可以调整Y坐标)
    // 假设我们在最底部 300-320 区域显示调试信息，或者暂时覆盖在 Room 模块上调试
    // 这里为了不破坏布局，建议暂时只打印串口，或者在 Status Bar 显示
    // 这里的实现视你的调试需求而定
}