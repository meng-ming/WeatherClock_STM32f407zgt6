#include "st7789.h"
#include "BSP_Cortex_M4_Delay.h"

// ====================================================================
// 汉字字模数据 (16x16)
// ====================================================================
const unsigned char chines_word[][32] = {
    {0x00, 0x00, 0xE4, 0x3F, 0x28, 0x20, 0x28, 0x25, 0x81, 0x08, 0x42,
     0x10, 0x02, 0x02, 0x08, 0x02, 0xE8, 0x3F, 0x04, 0x02, 0x07, 0x07,
     0x84, 0x0A, 0x44, 0x12, 0x34, 0x62, 0x04, 0x02, 0x00, 0x02}, /*"深",0*/

    {0x88, 0x20, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0xBF,
     0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24,
     0xB8, 0x24, 0x87, 0x24, 0x42, 0x24, 0x40, 0x20, 0x20, 0x20}, /*"圳",1*/

    {0x10, 0x09, 0x10, 0x11, 0x10, 0x11, 0x08, 0x01, 0xE8, 0x7F, 0x0C,
     0x05, 0x0C, 0x05, 0x0A, 0x05, 0x09, 0x05, 0x08, 0x05, 0x88, 0x04,
     0x88, 0x44, 0x88, 0x44, 0x48, 0x44, 0x48, 0x78, 0x28, 0x00}, /*"市",2*/

    {0x04, 0x00, 0x7C, 0x3E, 0x12, 0x22, 0x10, 0x22, 0xFF, 0x22, 0x28,
     0x22, 0x44, 0x3E, 0x02, 0x00, 0xF8, 0x0F, 0x08, 0x08, 0x08, 0x08,
     0xF8, 0x0F, 0x08, 0x08, 0x08, 0x08, 0xF8, 0x0F, 0x08, 0x08}, /*"普",3*/

    {0xF8, 0x0F, 0x08, 0x08, 0xF8, 0x0F, 0x08, 0x08, 0xF8, 0x0F, 0x80,
     0x00, 0xFF, 0x7F, 0x00, 0x00, 0xF8, 0x0F, 0x08, 0x08, 0x08, 0x08,
     0xF8, 0x0F, 0x80, 0x00, 0x84, 0x10, 0xA2, 0x20, 0x40, 0x00}, /*"中",4*/

    {0x10, 0x08, 0xB8, 0x08, 0x0F, 0x09, 0x08, 0x09, 0x08, 0x08, 0xBF,
     0x08, 0x08, 0x09, 0x1C, 0x09, 0x2C, 0x08, 0x0A, 0x78, 0xCA, 0x0F,
     0x09, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, /*"科",5*/

    {0x08, 0x04, 0x08, 0x04, 0x08, 0x04, 0xC8, 0x7F, 0x3F, 0x04, 0x08,
     0x04, 0x08, 0x04, 0xA8, 0x3F, 0x18, 0x21, 0x0C, 0x11, 0x0B, 0x12,
     0x08, 0x0A, 0x08, 0x04, 0x08, 0x0A, 0x8A, 0x11, 0x64, 0x60} /*"技",6*/
};

// ====================================================================
// 硬件层
// ====================================================================
void ST7789_SPI_SendByte(uint8_t byte)
{
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET)
        ;
    SPI_I2S_SendData(ST7789_SPI_PERIPH, byte);
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET)
        ;
}

void TFT_SEND_CMD(uint8_t o_command)
{
    LCD_DC_CLR();
    LCD_CS_CLR();
    ST7789_SPI_SendByte(o_command);
    LCD_CS_SET();
}

void TFT_SEND_DATA(uint8_t o_data)
{
    LCD_DC_SET();
    LCD_CS_CLR();
    ST7789_SPI_SendByte(o_data);
    LCD_CS_SET();
}

void ST7789_Hardware_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE,
                           ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

    GPIO_InitStructure.GPIO_Pin = ST7789_CS_PIN;
    GPIO_Init(ST7789_CS_PORT, &GPIO_InitStructure);
    LCD_CS_SET();

    GPIO_InitStructure.GPIO_Pin = ST7789_DC_PIN;
    GPIO_Init(ST7789_DC_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = ST7789_RST_PIN;
    GPIO_Init(ST7789_RST_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = ST7789_BL_PIN;
    GPIO_Init(ST7789_BL_PORT, &GPIO_InitStructure);
    LCD_BL_SET();

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_InitStructure.GPIO_Pin = ST7789_SPI_SCK_PIN;
    GPIO_Init(ST7789_SPI_SCK_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ST7789_SPI_SCK_PORT, GPIO_PinSource10, GPIO_AF_SPI2);

    GPIO_InitStructure.GPIO_Pin = ST7789_SPI_MOSI_PIN;
    GPIO_Init(ST7789_SPI_MOSI_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ST7789_SPI_MOSI_PORT, GPIO_PinSource3, GPIO_AF_SPI2);

    GPIO_InitStructure.GPIO_Pin = ST7789_SPI_MISO_PIN;
    GPIO_Init(ST7789_SPI_MISO_PORT, &GPIO_InitStructure);
    GPIO_PinAFConfig(ST7789_SPI_MISO_PORT, GPIO_PinSource2, GPIO_AF_SPI2);

    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(ST7789_SPI_PERIPH, &SPI_InitStructure);

    SPI_Cmd(ST7789_SPI_PERIPH, ENABLE);
}

void ST7789_Init(void)
{
    ST7789_Hardware_Init();
    BSP_Cortex_M4_Delay_Init();

    LCD_RST_SET();
    BSP_Cortex_M4_Delay_ms(10);
    LCD_RST_CLR();
    BSP_Cortex_M4_Delay_ms(1);
    LCD_RST_SET();
    BSP_Cortex_M4_Delay_ms(50);

    TFT_SEND_CMD(0x11);
    BSP_Cortex_M4_Delay_ms(10);

    TFT_SEND_CMD(0x3A);
    TFT_SEND_DATA(0x05);
    TFT_SEND_CMD(0xC5);
    TFT_SEND_DATA(0x1A);
    TFT_SEND_CMD(0x36);
    TFT_SEND_DATA(0x00);

    TFT_SEND_CMD(0xB2);
    TFT_SEND_DATA(0x05);
    TFT_SEND_DATA(0x05);
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x33);
    TFT_SEND_DATA(0x33);

    TFT_SEND_CMD(0xB7);
    TFT_SEND_DATA(0x05);

    TFT_SEND_CMD(0xBB);
    TFT_SEND_DATA(0x3F);
    TFT_SEND_CMD(0xC0);
    TFT_SEND_DATA(0x2C);
    TFT_SEND_CMD(0xC2);
    TFT_SEND_DATA(0x01);
    TFT_SEND_CMD(0xC3);
    TFT_SEND_DATA(0x0F);
    TFT_SEND_CMD(0xC4);
    TFT_SEND_DATA(0x20);
    TFT_SEND_CMD(0xC6);
    TFT_SEND_DATA(0x01);
    TFT_SEND_CMD(0xD0);
    TFT_SEND_DATA(0xA4);
    TFT_SEND_DATA(0xA1);
    TFT_SEND_CMD(0xE8);
    TFT_SEND_DATA(0x03);
    TFT_SEND_CMD(0xE9);
    TFT_SEND_DATA(0x09);
    TFT_SEND_DATA(0x09);
    TFT_SEND_DATA(0x08);

    TFT_SEND_CMD(0xE0);
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

    TFT_SEND_CMD(0xE1);
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

    TFT_SEND_CMD(0x20);
    TFT_SEND_CMD(0x29);
}

// ====================================================================
// 核心绘图函数 (阻塞式版本 - 稳定可靠)
// ====================================================================
void TFT_Fill_Rect(
    uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    uint32_t i, total_pixels;

    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(x_start >> 8);
    TFT_SEND_DATA(x_start & 0xFF);
    TFT_SEND_DATA(x_end >> 8);
    TFT_SEND_DATA(x_end & 0xFF);

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(y_start >> 8);
    TFT_SEND_DATA(y_start & 0xFF);
    TFT_SEND_DATA(y_end >> 8);
    TFT_SEND_DATA(y_end & 0xFF);

    TFT_SEND_CMD(0x2C);

    // 准备颜色数据 (高8位和低8位)
    uint8_t color_h = color >> 8;
    uint8_t color_l = color & 0xFF;

    total_pixels = (uint32_t) (x_end - x_start + 1) * (y_end - y_start + 1);

    LCD_DC_SET();
    LCD_CS_CLR();

    // 纯阻塞发送，慢但稳
    for (i = 0; i < total_pixels; i++)
    {
        ST7789_SPI_SendByte(color_h);
        ST7789_SPI_SendByte(color_l);
    }

    LCD_CS_SET();
}

void TFT_full(uint16_t color)
{
    TFT_Fill_Rect(0, 0, TFT_COLUMN_NUMBER - 1, TFT_LINE_NUMBER - 1, color);
}

void TFT_clear(void)
{
    TFT_full(WHITE);
}

void display_char16_16(unsigned int  x,
                       unsigned int  y,
                       unsigned long color,
                       unsigned char word_serial_number)
{
    unsigned int  column;
    unsigned char tm, temp, xxx = 0;

    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x);
    x = x + 15;
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x);

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y);
    y = y + 15;
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y);

    TFT_SEND_CMD(0x2C);

    uint8_t color_h = color >> 8;
    uint8_t color_l = color & 0xFF;

    LCD_DC_SET();
    LCD_CS_CLR();

    for (column = 0; column < 32; column++)
    {
        temp = chines_word[word_serial_number][xxx];
        for (tm = 0; tm < 8; tm++)
        {
            if (temp & 0x01)
            {
                ST7789_SPI_SendByte(color_h);
                ST7789_SPI_SendByte(color_l);
            }
            else
            {
                ST7789_SPI_SendByte(0xFF);
                ST7789_SPI_SendByte(0xFF);
            }
            temp >>= 1;
        }
        xxx++;
    }
    LCD_CS_SET();
}

// // ====================================================================
// // 显示 16x32 ASCII 字符 (大号字体) (适配二维数组字库)
// // 字符宽度: 16, 高度: 32
// // 字模占用: 64 字节/字符
// // ====================================================================
// void TFT_ShowChar_1632(uint16_t x, uint16_t y, char chr, uint16_t color, uint16_t bgcolor)
// {
//     if (chr < 32 || chr > 126)
//         return; // 超出范围直接返回，防崩溃

//     const uint8_t* dots = &ASCII_16x32[(chr - 32) * 64]; // 关键改这里！！！

//     TFT_SEND_CMD(0x2A);
//     TFT_SEND_DATA(x >> 8);
//     TFT_SEND_DATA(x & 0xFF);
//     TFT_SEND_DATA((x + 15) >> 8);
//     TFT_SEND_DATA(x + 15);

//     TFT_SEND_CMD(0x2B);
//     TFT_SEND_DATA(y >> 8);
//     TFT_SEND_DATA(y & 0xFF);
//     TFT_SEND_DATA((y + 31) >> 8);
//     TFT_SEND_DATA(y + 31);

//     TFT_SEND_CMD(0x2C);

//     LCD_DC_SET();
//     LCD_CS_CLR();

//     uint8_t color_h = color >> 8, color_l = color & 0xFF;
//     uint8_t bg_h = bgcolor >> 8, bg_l = bgcolor & 0xFF;

//     for (int i = 0; i < 64; i++)
//     {
//         uint8_t temp = dots[i];
//         for (int j = 0; j < 8; j++)
//         {
//             // 低位先行
//             if (temp & 0x01)
//             {
//                 ST7789_SPI_SendByte(color_h);
//                 ST7789_SPI_SendByte(color_l);
//             }
//             else
//             {
//                 ST7789_SPI_SendByte(bg_h);
//                 ST7789_SPI_SendByte(bg_l);
//             }
//             temp >>= 1;
//         }
//     }
//     LCD_CS_SET();
// }

// // ====================================================================
// // 显示 16x32 英文字符串
// // ====================================================================
// void TFT_ShowString_1632(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t
// bgcolor)
// {
//     while (*str)
//     {
//         // 换行保护：如果这行放不下了 (屏幕宽240，字宽16)
//         if (x > (TFT_COLUMN_NUMBER - 16))
//         {
//             x = 0;
//             y += 32; // 换行高度 32
//         }

//         if (y > (TFT_LINE_NUMBER - 32))
//             break; // 超出屏幕底边

//         TFT_ShowChar_1632(x, y, *str, color, bgcolor);
//         x += 16; // 字宽 16，光标右移
//         str++;
//     }
// }