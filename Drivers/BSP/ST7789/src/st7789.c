#include "st7789.h"

// =======================================================
// 汉字字模数据 (16x16)
// =======================================================
// 将 51单片机的 code 关键字改为 const
const unsigned char chines_word[][32] = {
    {0x00, 0x00, 0xE4, 0x3F, 0x28, 0x20, 0x28, 0x25, 0x81, 0x08, 0x42,
     0x10, 0x02, 0x02, 0x08, 0x02, 0xE8, 0x3F, 0x04, 0x02, 0x07, 0x07,
     0x84, 0x0A, 0x44, 0x12, 0x34, 0x62, 0x04, 0x02, 0x00, 0x02}, /*"深",0*/

    {0x88, 0x20, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0xBF,
     0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24, 0x88, 0x24,
     0xB8, 0x24, 0x87, 0x24, 0x42, 0x24, 0x40, 0x20, 0x20, 0x20}, /*"圳",1*/

    {0x10, 0x09, 0x10, 0x11, 0x10, 0x11, 0x08, 0x01, 0xE8, 0x7F, 0x0C,
     0x05, 0x0C, 0x05, 0x0A, 0x05, 0x09, 0x05, 0x08, 0x05, 0x88, 0x04,
     0x88, 0x44, 0x88, 0x44, 0x48, 0x44, 0x48, 0x78, 0x28, 0x00}, /*"市",2
                                                                     (这里原注释乱码，我猜是市)*/

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

// =======================================================
// 简单的延时函数
// =======================================================
static void delay_ms(uint32_t ms)
{
    volatile uint32_t i, j; // 必须加 volatile 防止被优化
    for (i = 0; i < ms; i++)
    {
        // 宁愿慢一点，先确保复位信号足够长
        for (j = 0; j < 50000; j++)
        {
            __NOP();
        }
    }
}

// =======================================================
// 硬件层：硬件 SPI 发送字节
// =======================================================
void ST7789_SPI_SendByte(uint8_t byte)
{
    // 1. 等待发送缓冲区空闲
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET)
        ;
    // 2. 发送数据
    SPI_I2S_SendData(ST7789_SPI_PERIPH, byte);
    // 3. 等待发送完成 (忙标志位为0)
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET)
        ;
}

// =======================================================
// 协议层：发送命令
// =======================================================
void TFT_SEND_CMD(uint8_t o_command)
{
    LCD_DC_CLR(); // 命令模式
    LCD_CS_CLR(); // 选中
    ST7789_SPI_SendByte(o_command);
    LCD_CS_SET(); // 取消选中
}

// =======================================================
// 协议层：发送数据
// =======================================================
void TFT_SEND_DATA(uint8_t o_data)
{
    LCD_DC_SET(); // 数据模式
    LCD_CS_CLR(); // 选中
    ST7789_SPI_SendByte(o_data);
    LCD_CS_SET(); // 取消选中
}

// =======================================================
// 初始化：STM32F407 硬件初始化
// =======================================================
void ST7789_Hardware_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    // 1. 开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE,
                           ENABLE);

    // 2. 配置 GPIO (CS, DC, RST, BL 推挽输出)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

    // CS (PC4)
    GPIO_InitStructure.GPIO_Pin = ST7789_CS_PIN;
    GPIO_Init(ST7789_CS_PORT, &GPIO_InitStructure);
    LCD_CS_SET();

    // DC (PC5)
    GPIO_InitStructure.GPIO_Pin = ST7789_DC_PIN;
    GPIO_Init(ST7789_DC_PORT, &GPIO_InitStructure);

    // RST (PE3)
    GPIO_InitStructure.GPIO_Pin = ST7789_RST_PIN;
    GPIO_Init(ST7789_RST_PORT, &GPIO_InitStructure);

    // BL (PB15)
    GPIO_InitStructure.GPIO_Pin = ST7789_BL_PIN;
    GPIO_Init(ST7789_BL_PORT, &GPIO_InitStructure);
    LCD_BL_SET(); // 点亮背光

    // 3. 配置 SPI2 引脚 (SCK=PB10, MOSI=PC3, MISO=PC2) 复用模式
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

    // 4. 配置 SPI2 参数 (Mode 0: CPOL=0, CPHA=1Edge)
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; //
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(ST7789_SPI_PERIPH, &SPI_InitStructure);

    SPI_Cmd(ST7789_SPI_PERIPH, ENABLE);
}

// =======================================================
// 屏幕逻辑：命令初始化序列
// =======================================================
void ST7789_Init(void)
{
    ST7789_Hardware_Init(); // 必须先初始化硬件！

    // 复位操作
    LCD_RST_SET();
    delay_ms(10);
    LCD_RST_CLR();
    delay_ms(50);
    LCD_RST_SET();
    delay_ms(50);

    // ST7789 初始化代码
    TFT_SEND_CMD(0x11); // Sleep Out
    delay_ms(120);

    TFT_SEND_CMD(0x3A);  // Interface Pixel Format
    TFT_SEND_DATA(0x05); // 16bit/pixel

    TFT_SEND_CMD(0xC5); // VCOM Setting
    TFT_SEND_DATA(0x1A);

    TFT_SEND_CMD(0x36);  // Memory Data Access Control
    TFT_SEND_DATA(0x00); // 竖屏

    // Porch Setting
    TFT_SEND_CMD(0xB2);
    TFT_SEND_DATA(0x05);
    TFT_SEND_DATA(0x05);
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x33);
    TFT_SEND_DATA(0x33);

    TFT_SEND_CMD(0xB7); // Gate Control
    TFT_SEND_DATA(0x05);

    // Power Settings
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

    // Gamma Settings
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

    TFT_SEND_CMD(0x20); // Display Inversion ON (通常ST7789需要反显，如果颜色不对改0x20)
    TFT_SEND_CMD(0x29); // Display ON
}

// =======================================================
// 全屏填充颜色
// =======================================================
void TFT_full(uint16_t color)
{
    uint32_t i;
    // 设置全屏窗口
    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(0);
    TFT_SEND_DATA(0);
    TFT_SEND_DATA(0);
    TFT_SEND_DATA(TFT_COLUMN_NUMBER - 1); // 239

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(0);
    TFT_SEND_DATA(0);
    TFT_SEND_DATA(0x01);
    TFT_SEND_DATA(0x3F); // 319

    TFT_SEND_CMD(0x2C); // 写内存

    // 发送颜色数据 (SPI是8位的，16位颜色要分两次发)
    uint8_t color_h = color >> 8;
    uint8_t color_l = color & 0xFF;

    LCD_DC_SET();
    LCD_CS_CLR();
    for (i = 0; i < (TFT_COLUMN_NUMBER * TFT_LINE_NUMBER); i++)
    {
        ST7789_SPI_SendByte(color_h);
        ST7789_SPI_SendByte(color_l);
    }
    LCD_CS_SET();
}

// =======================================================
// 清屏 (填充白色)
// =======================================================
void TFT_clear(void)
{
    TFT_full(WHITE);
}

// =======================================================
// 显示 16x16 汉字
// =======================================================
void display_char16_16(unsigned int  x,
                       unsigned int  y,
                       unsigned long color,
                       unsigned char word_serial_number)
{
    unsigned int  column;
    unsigned char tm, temp, xxx = 0;

    // 设置窗口 16x16
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
                // 背景色：白色
                ST7789_SPI_SendByte(0xFF);
                ST7789_SPI_SendByte(0xFF);
            }
            temp >>= 1;
        }
        xxx++;
    }
    LCD_CS_SET();
}