#include "lcd_image.h"
#include "st7789.h"

/**
 * @brief 显示图片 (RGB565 数组)
 * @param x, y: 起始坐标
 * @param w, h: 图片宽、高
 * @param pData: 图片数组指针 (const uint8_t*)
 */
void LCD_Show_Image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const unsigned char* pData)
{
    uint32_t size = w * h * 2; // RGB565 每个像素 2 字节
    uint32_t i;

    // 1. 设置显示窗口
    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x & 0xFF);
    TFT_SEND_DATA((x + w - 1) >> 8);
    TFT_SEND_DATA((x + w - 1) & 0xFF);

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y & 0xFF);
    TFT_SEND_DATA((y + h - 1) >> 8);
    TFT_SEND_DATA((y + h - 1) & 0xFF);

    // 2. 开始写入内存
    TFT_SEND_CMD(0x2C);

    LCD_DC_SET();
    LCD_CS_CLR();

    // 3. 循环发送数据 (纯阻塞发送)
    // 这里的效率其实很低，后面做项目我会教你用 DMA 瞬间发完
    for (i = 0; i < size; i++)
    {
        ST7789_SPI_SendByte(pData[i]);
    }

    LCD_CS_SET();
}