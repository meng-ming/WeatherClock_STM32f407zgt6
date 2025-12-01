#include "app_ui.h"
#include "BSP_Tick_Delay.h"
#include "lcd_image.h"
#include "st7789.h"
#include "app_ui_config.h"
#include "sys_log.h"
#include "bsp_rtc.h"
#include "lcd_font.h"

// 引入新做好的页面模块
#include "ui_main_page.h"
#include <string.h>

static uint8_t s_last_status_len = 0; // 上个状态文字的长度，用于覆盖刷新

/**
 * @brief  系统开机界面
 * @note   负责初始化 LCD 硬件驱动，清屏，展示整个系统的开机界面。
 * @retval None
 */
static void APP_Start_UP(void)
{
    // 1. ST7789 底层初始化
    ST7789_Init();

    // 2. 显示开机界面
    LCD_Show_Image(0, 0, 240, 320, gImage_Startup_Screen);

    // 3. 延时2s
    BSP_Delay_ms(2000);
}

void APP_UI_Init(void)
{
    // 1. 启动开机界面
    APP_Start_UP();

    // 2. 调用主页面初始化
    APP_UI_MainPage_Init();
}

void APP_UI_Update(const APP_Weather_Data_t* data)
{
    // 调用主页面的更新逻辑
    APP_UI_UpdateWeather(data);

    // 底部状态栏更新
    APP_UI_ShowStatus("Updated!", BLACK);
}

void APP_UI_ShowStatus(const char* status, uint16_t color)
{
    LOG_I("[APP] %s", status);
    // 防止长度不一，不能完全覆盖
    TFT_Fill_Rect(35, BOX_STATUS_Y + 5, s_last_status_len * 8, 16, UI_STATUS_BG);
    // 在屏幕上方显示
    LCD_Show_String(35, BOX_STATUS_Y + 5, status, &font_16, color, UI_STATUS_BG);

    if (strlen(status) != s_last_status_len)
        s_last_status_len = strlen(status);
}
