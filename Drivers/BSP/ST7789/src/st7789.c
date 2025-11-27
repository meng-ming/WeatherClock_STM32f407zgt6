/**
 * @file    st7789.c
 * @brief   ST7789 LCD 底层驱动实现
 */

#include "st7789.h"
#include "BSP_Tick_Delay.h"

// ====================================================================
// 硬件层初始化 (私有函数)
// ====================================================================
static void ST7789_Hardware_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    // 1. 开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE,
                           ENABLE);

    // 2. 配置控制引脚 (CS, DC, RST, BL)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

    // CS
    GPIO_InitStructure.GPIO_Pin = ST7789_CS_PIN;
    GPIO_Init(ST7789_CS_PORT, &GPIO_InitStructure);
    LCD_CS_SET();

    // DC
    GPIO_InitStructure.GPIO_Pin = ST7789_DC_PIN;
    GPIO_Init(ST7789_DC_PORT, &GPIO_InitStructure);

    // RST
    GPIO_InitStructure.GPIO_Pin = ST7789_RST_PIN;
    GPIO_Init(ST7789_RST_PORT, &GPIO_InitStructure);

    // BL
    GPIO_InitStructure.GPIO_Pin = ST7789_BL_PIN;
    GPIO_Init(ST7789_BL_PORT, &GPIO_InitStructure);
    LCD_BL_SET(); // 点亮背光

    // 3. 配置 SPI 引脚 (SCK, MOSI, MISO)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    // SCK
    GPIO_InitStructure.GPIO_Pin = ST7789_SPI_SCK_PIN;
    GPIO_Init(ST7789_SPI_SCK_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ST7789_SPI_SCK_PORT, GPIO_PinSource10, GPIO_AF_SPI2);

    // MOSI
    GPIO_InitStructure.GPIO_Pin = ST7789_SPI_MOSI_PIN;
    GPIO_Init(ST7789_SPI_MOSI_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ST7789_SPI_MOSI_PORT, GPIO_PinSource3, GPIO_AF_SPI2);

    // MISO (虽然不用，但为了配置完整性)
    GPIO_InitStructure.GPIO_Pin = ST7789_SPI_MISO_PIN;
    GPIO_Init(ST7789_SPI_MISO_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ST7789_SPI_MISO_PORT, GPIO_PinSource2, GPIO_AF_SPI2);

    // 4. 配置 SPI 参数
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode      = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize  = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL      = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA      = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS       = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler =
        SPI_BaudRatePrescaler_2; // 尽可能快，F4 APB1=42M, /2=21M
    SPI_InitStructure.SPI_FirstBit      = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(ST7789_SPI_PERIPH, &SPI_InitStructure);

    SPI_Cmd(ST7789_SPI_PERIPH, ENABLE);
}

// ====================================================================
// 基础通信接口
// ====================================================================
void ST7789_SPI_SendByte(uint8_t byte)
{
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET)
        ;
    SPI_I2S_SendData(ST7789_SPI_PERIPH, byte);
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET)
        ;
}

void TFT_SEND_CMD(uint8_t cmd)
{
    LCD_DC_CLR(); // 命令模式
    LCD_CS_CLR();
    ST7789_SPI_SendByte(cmd);
    LCD_CS_SET();
}

void TFT_SEND_DATA(uint8_t data)
{
    LCD_DC_SET(); // 数据模式
    LCD_CS_CLR();
    ST7789_SPI_SendByte(data);
    LCD_CS_SET();
}

// ====================================================================
// 核心功能实现
// ====================================================================
void ST7789_Init(void)
{
    ST7789_Hardware_Init();
    BSP_SysTick_Init(); // 确保延时基准已初始化

    // 复位序列
    LCD_RST_SET();
    BSP_Delay_ms(10);
    LCD_RST_CLR();
    BSP_Delay_ms(10); // 至少 10us
    LCD_RST_SET();
    BSP_Delay_ms(120); // Wait for reset complete

    // 初始化命令序列
    TFT_SEND_CMD(0x11); // Sleep Out
    BSP_Delay_ms(120);

    TFT_SEND_CMD(0x36);  // Memory Data Access Control
    TFT_SEND_DATA(0x00); // 根据实际屏幕方向调整: 0x00, 0xC0, 0x70, 0xA0 等

    TFT_SEND_CMD(0x3A);  // Interface Pixel Format
    TFT_SEND_DATA(0x05); // 16-bit/pixel

    // Porch Setting
    TFT_SEND_CMD(0xB2);
    TFT_SEND_DATA(0x0C);
    TFT_SEND_DATA(0x0C);
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x33);
    TFT_SEND_DATA(0x33);

    TFT_SEND_CMD(0xB7); // Gate Control
    TFT_SEND_DATA(0x35);

    TFT_SEND_CMD(0xBB); // VCOM Setting
    TFT_SEND_DATA(0x19);

    TFT_SEND_CMD(0xC0); // LCM Control
    TFT_SEND_DATA(0x2C);

    TFT_SEND_CMD(0xC2); // VDV and VRH Command Enable
    TFT_SEND_DATA(0x01);

    TFT_SEND_CMD(0xC3); // VRH Set
    TFT_SEND_DATA(0x12);

    TFT_SEND_CMD(0xC4); // VDV Set
    TFT_SEND_DATA(0x20);

    TFT_SEND_CMD(0xC6); // Frame Rate Control
    TFT_SEND_DATA(0x0F);

    TFT_SEND_CMD(0xD0); // Power Control 1
    TFT_SEND_DATA(0xA4);
    TFT_SEND_DATA(0xA1);

    // Gamma Setting (如果不准可微调)
    TFT_SEND_CMD(0xE0); // Set Gamma
    TFT_SEND_DATA(0xD0);
    TFT_SEND_DATA(0x05);
    TFT_SEND_DATA(0x09);
    TFT_SEND_DATA(0x09);
    TFT_SEND_DATA(0x08);
    TFT_SEND_DATA(0x14);
    TFT_SEND_DATA(0x28);
    TFT_SEND_DATA(0x33);
    TFT_SEND_DATA(0x3F);
    TFT_SEND_DATA(0x07);
    TFT_SEND_DATA(0x13);
    TFT_SEND_DATA(0x14);
    TFT_SEND_DATA(0x28);
    TFT_SEND_DATA(0x30);

    TFT_SEND_CMD(0XE1); // Set Gamma
    TFT_SEND_DATA(0xD0);
    TFT_SEND_DATA(0x05);
    TFT_SEND_DATA(0x09);
    TFT_SEND_DATA(0x09);
    TFT_SEND_DATA(0x08);
    TFT_SEND_DATA(0x03);
    TFT_SEND_DATA(0x24);
    TFT_SEND_DATA(0x32);
    TFT_SEND_DATA(0x32);
    TFT_SEND_DATA(0x3B);
    TFT_SEND_DATA(0x14);
    TFT_SEND_DATA(0x13);
    TFT_SEND_DATA(0x28);
    TFT_SEND_DATA(0x2F);

    TFT_SEND_CMD(0x20); // Display Inversion On
    TFT_SEND_CMD(0x29); // Display On
}

void TFT_Fill_Rect(uint16_t x_start, uint16_t y_start, uint16_t w, uint16_t h, uint16_t color)
{
    // 1. 参数合法性基础检查：如果宽或高为0，直接返回，不浪费时间
    if (w == 0 || h == 0)
        return;

    // 2. 起始坐标越界检查
    if (x_start >= TFT_COLUMN_NUMBER || y_start >= TFT_LINE_NUMBER)
        return;

    // 3. 计算结束坐标 (注意：坐标是从0开始的，所以要减1)
    uint16_t x_end = x_start + w - 1;
    uint16_t y_end = y_start + h - 1;

    // 4. 右/下边界 硬件裁剪 (Clipping)
    // 如果计算出的结束坐标超出了屏幕最大分辨率，强制限制在边界上
    if (x_end >= TFT_COLUMN_NUMBER)
        x_end = TFT_COLUMN_NUMBER - 1;
    if (y_end >= TFT_LINE_NUMBER)
        y_end = TFT_LINE_NUMBER - 1;

    // 重新设置窗口指令
    TFT_SEND_CMD(0x2A); // Column Address Set
    TFT_SEND_DATA(x_start >> 8);
    TFT_SEND_DATA(x_start & 0xFF);
    TFT_SEND_DATA(x_end >> 8);
    TFT_SEND_DATA(x_end & 0xFF);

    TFT_SEND_CMD(0x2B); // Row Address Set
    TFT_SEND_DATA(y_start >> 8);
    TFT_SEND_DATA(y_start & 0xFF);
    TFT_SEND_DATA(y_end >> 8);
    TFT_SEND_DATA(y_end & 0xFF);

    TFT_SEND_CMD(0x2C); // Memory Write

    // 准备颜色数据
    uint8_t color_h = color >> 8;
    uint8_t color_l = color & 0xFF;

    // 计算实际需要发送的像素点总数
    // 注意：这里必须用处理过后的 x_end/y_end 来计算，否则会溢出写
    uint32_t total_pixels = (uint32_t) (x_end - x_start + 1) * (y_end - y_start + 1);

    LCD_DC_SET();
    LCD_CS_CLR();

    // 纯阻塞发送
    while (total_pixels--)
    {
        ST7789_SPI_SendByte(color_h);
        ST7789_SPI_SendByte(color_l);
    }

    LCD_CS_SET();
}

void TFT_full(uint16_t color)
{
    TFT_Fill_Rect(0, 0, TFT_COLUMN_NUMBER, TFT_LINE_NUMBER, color);
}

void TFT_clear(void)
{
    TFT_full(WHITE);
}