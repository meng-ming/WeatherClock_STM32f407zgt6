/**
 * @file    st7789.c
 * @brief   ST7789 LCD 底层驱动实现
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "st7789.h"
#include "BSP_Tick_Delay.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "portmacro.h"
#include "semphr.h"
#include "stm32f4xx_dma.h"
#include <string.h>

#define DMA_MAX_SIZE 65535 // DMA 硬件单次最大传输限制

// 引入外部变量
extern SemaphoreHandle_t g_mutex_lcd;

/* === 内部使用 === */
// 内部使用的同步信号量 (用于 DMA 等待)
// 注意：当前版本用轮询方式等待TC旗标，注释掉了中断+信号量方式
// 商用项目推荐中断+信号量方式（CPU空闲率高），轮询仅用于调试或极简场景
SemaphoreHandle_t g_lcd_dma_sem = NULL;

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
    SPI_InitStructure.SPI_DataSize  = SPI_DataSize_8b; // 初始化为8位，用于命令发送
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
    // 等待TXE空，发送字节
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(ST7789_SPI_PERIPH, byte);
    // 等待BSY清零，确保字节完全发送
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET);
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
    /* === 上锁 === */
    if (g_mutex_lcd != NULL)
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);

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

    TFT_SEND_CMD(0xE1); // Set Gamma
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

    // 初始化完释放锁
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }
}

// ====================================================================
// DMA 加速接口
// ====================================================================

void TFT_Fill_Rect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    /* === 上锁 === */
    if (g_mutex_lcd != NULL)
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);

    // 1. 参数检查
    if (w == 0 || h == 0)
        goto end;
    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER)
        goto end;

    uint16_t x_end = x + w - 1;
    uint16_t y_end = y + h - 1;

    // 2. 设置绘图窗口
    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x & 0xFF);
    TFT_SEND_DATA(x_end >> 8);
    TFT_SEND_DATA(x_end & 0xFF);

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y & 0xFF);
    TFT_SEND_DATA(y_end >> 8);
    TFT_SEND_DATA(y_end & 0xFF);

    TFT_SEND_CMD(0x2C); // Memory Write

    // 3. 切换 SPI 到 16位模式
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET);

    SPI_Cmd(ST7789_SPI_PERIPH, DISABLE);
    ST7789_SPI_PERIPH->CR2 &= ~SPI_CR2_TXDMAEN; // 先关掉请求，防止意外
    ST7789_SPI_PERIPH->CR1 |= SPI_CR1_DFF;      // 16-bit mode
    SPI_Cmd(ST7789_SPI_PERIPH, ENABLE);

    LCD_DC_SET();
    LCD_CS_CLR();

    // 4. DMA 配置准备
    static uint16_t dma_color_buffer;
    dma_color_buffer = color;

    RCC_AHB1PeriphClockCmd(LCD_DMA_CLK, ENABLE);

    // 读寄存器延时，等待时钟稳定 (防止 HardFault)
    volatile uint32_t temp = LCD_DMA_STREAM->CR;
    (void) temp;

    DMA_DeInit(LCD_DMA_STREAM);
    while (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE);

    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_Channel            = LCD_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &ST7789_SPI_PERIPH->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t) &dma_color_buffer;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_MemoryToPeripheral;

    // 为了防止需要刷新的像素点超过最大值，导致初始化失败，设置为一个合法的值，在后续进行自动分块切割操作
    DMA_InitStructure.DMA_BufferSize = 1;

    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;

    DMA_Init(LCD_DMA_STREAM, &DMA_InitStructure);

    // 开启 SPI 的 DMA 请求
    SPI_I2S_DMACmd(ST7789_SPI_PERIPH, SPI_I2S_DMAReq_Tx, ENABLE);

    // 5. 分块传输逻辑
    uint32_t total_pixels = (uint32_t) w * h;

    while (total_pixels > 0)
    {
        // 计算本次传输值
        uint16_t current_tx_size = (total_pixels > DMA_MAX_SIZE) ? DMA_MAX_SIZE : total_pixels;

        // 必须先关闭 DMA 才能修改 NDTR (传输数量)
        DMA_Cmd(LCD_DMA_STREAM, DISABLE);
        while (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE);

        // 写入真正要传输的大小
        LCD_DMA_STREAM->NDTR = current_tx_size;

        // 清除标志位并启动
        DMA_ClearFlag(LCD_DMA_STREAM, LCD_DMA_FLAG_TC | DMA_FLAG_TEIF4);
        DMA_Cmd(LCD_DMA_STREAM, ENABLE);

        // 等待传输完成
        while (DMA_GetFlagStatus(LCD_DMA_STREAM, LCD_DMA_FLAG_TC) == RESET)
        {
            if (DMA_GetFlagStatus(LCD_DMA_STREAM, DMA_FLAG_TEIF4) == SET)
            {
                DMA_ClearFlag(LCD_DMA_STREAM, DMA_FLAG_TEIF4);
                goto cleanup;
            }
        }

        total_pixels -= current_tx_size;
    }

cleanup:
    // 6. 收尾
    DMA_Cmd(LCD_DMA_STREAM, DISABLE);
    SPI_I2S_DMACmd(ST7789_SPI_PERIPH, SPI_I2S_DMAReq_Tx, DISABLE);

    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET);

    LCD_CS_SET();

    // 恢复 8位
    SPI_Cmd(ST7789_SPI_PERIPH, DISABLE);
    ST7789_SPI_PERIPH->CR1 &= ~SPI_CR1_DFF;
    SPI_Cmd(ST7789_SPI_PERIPH, ENABLE);

end:
    if (g_mutex_lcd != NULL)
        xSemaphoreGiveRecursive(g_mutex_lcd);
}
void TFT_Full_DMA(uint16_t color)
{
    TFT_Fill_Rect_DMA(0, 0, TFT_COLUMN_NUMBER, TFT_LINE_NUMBER, color);
}

void TFT_Clear_DMA(uint16_t color)
{
    TFT_Full_DMA(color);
}

void TFT_ShowImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* pData)
{
    /* === 上锁 === */
    if (g_mutex_lcd != NULL)
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);

    if (w == 0 || h == 0)
        goto end;
    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER)
        goto end;

    // 1. 设置显示窗口
    uint16_t x_end = x + w - 1;
    uint16_t y_end = y + h - 1;

    TFT_SEND_CMD(0x2A);
    TFT_SEND_DATA(x >> 8);
    TFT_SEND_DATA(x & 0xFF);
    TFT_SEND_DATA(x_end >> 8);
    TFT_SEND_DATA(x_end & 0xFF);

    TFT_SEND_CMD(0x2B);
    TFT_SEND_DATA(y >> 8);
    TFT_SEND_DATA(y & 0xFF);
    TFT_SEND_DATA(y_end >> 8);
    TFT_SEND_DATA(y_end & 0xFF);

    // 2. 开始写入显存
    TFT_SEND_CMD(0x2C);

    // 3. 准备 DMA 传输环境
    LCD_DC_SET();
    LCD_CS_CLR();

    // 确保 SPI 空闲且 DMA 请求已关闭
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET);
    SPI_I2S_DMACmd(ST7789_SPI_PERIPH, SPI_I2S_DMAReq_Tx, DISABLE);

    // 开启 DMA 时钟
    RCC_AHB1PeriphClockCmd(LCD_DMA_CLK, ENABLE);
    // 读寄存器等待时钟稳定 (防 HardFault)
    volatile uint32_t temp = LCD_DMA_STREAM->CR;
    (void) temp;

    // 4. 配置 DMA (基础配置)
    if (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE)
    {
        DMA_Cmd(LCD_DMA_STREAM, DISABLE);
        while (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE);
    }
    DMA_DeInit(LCD_DMA_STREAM);

    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_Channel            = LCD_DMA_CHANNEL;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &ST7789_SPI_PERIPH->DR;
    // 内存地址稍后在循环里动态更新
    DMA_InitStructure.DMA_Memory0BaseAddr    = (uint32_t) 0;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize         = 1; // 稍后更新
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;        // 【关键】内存地址自增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 8位模式，最安全
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;     // 8位模式
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_FIFOMode           = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold      = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;

    DMA_Init(LCD_DMA_STREAM, &DMA_InitStructure);

    // 开启 SPI 的 DMA 请求
    SPI_I2S_DMACmd(ST7789_SPI_PERIPH, SPI_I2S_DMAReq_Tx, ENABLE);

    // 5. 分块传输 (解决 > 65535 问题)
    // 图片总字节数 = 宽 * 高 * 2 (RGB565)
    uint32_t       total_bytes = (uint32_t) w * h * 2;
    const uint8_t* pCurrent    = pData; // 当前数据指针

    while (total_bytes > 0)
    {
        // 计算本次传输大小 (最大 65535)
        uint16_t tx_cnt = (total_bytes > 0xFFFF) ? 0xFFFF : total_bytes;

        // 必须先关闭 DMA 才能修改 NDTR 和 M0AR
        DMA_Cmd(LCD_DMA_STREAM, DISABLE);
        while (DMA_GetCmdStatus(LCD_DMA_STREAM) != DISABLE);

        // 更新 DMA 寄存器
        LCD_DMA_STREAM->NDTR = tx_cnt;              // 本次传输数量
        LCD_DMA_STREAM->M0AR = (uint32_t) pCurrent; // 更新当前内存地址

        // 清除标志位并启动
        DMA_ClearFlag(LCD_DMA_STREAM, LCD_DMA_FLAG_TC | DMA_FLAG_TEIF4);
        DMA_Cmd(LCD_DMA_STREAM, ENABLE);

        // 等待传输完成 (生产环境建议换成信号量等待)
        while (DMA_GetFlagStatus(LCD_DMA_STREAM, LCD_DMA_FLAG_TC) == RESET)
        {
            if (DMA_GetFlagStatus(LCD_DMA_STREAM, DMA_FLAG_TEIF4) == SET)
            {
                DMA_ClearFlag(LCD_DMA_STREAM, DMA_FLAG_TEIF4);
                goto cleanup; // 硬件错误保护
            }
        }

        // 计算剩余量
        total_bytes -= tx_cnt;
        pCurrent += tx_cnt; // 指针后移
    }

cleanup:
    // 6. 收尾工作
    DMA_Cmd(LCD_DMA_STREAM, DISABLE);
    SPI_I2S_DMACmd(ST7789_SPI_PERIPH, SPI_I2S_DMAReq_Tx, DISABLE);

    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(ST7789_SPI_PERIPH, SPI_I2S_FLAG_BSY) == SET);

    LCD_CS_SET();

end:
    if (g_mutex_lcd != NULL)
        xSemaphoreGiveRecursive(g_mutex_lcd);
}