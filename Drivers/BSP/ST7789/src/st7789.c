/**
 * @file    st7789.c
 * @brief   ST7789 LCD 底层驱动实现
 */

#include "st7789.h"
#include "BSP_Tick_Delay.h"

#include "FreeRTOS.h"
#include "portmacro.h"
#include "semphr.h"
#include "stm32f4xx_dma.h"
#include "sys_log.h"
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

// 字体渲染缓冲区大小定义
// 依据：最大支持 64x64 像素的汉字，RGB565 格式每个像素 2 字节
// 计算：64 * 64 * 2 = 8192 Bytes (8KB)
// 说明：STM32F407 SRAM 充足 (192KB)，使用静态缓冲区避免栈溢出或频繁 malloc 碎片
#define FONT_DMA_BUFFER_SIZE (64 * 64 * 2)

static uint8_t s_font_render_buf[FONT_DMA_BUFFER_SIZE];

/**
 * @brief  [内部函数] 绘制单个字模的底层实现 (Color Expansion + DMA)
 * @note   核心算法：
 * 1. 读取单色字模的每一位 (Bit)。
 * 2. 若为 1，向缓冲区填入前景色 (2 Bytes)。
 * 3. 若为 0，向缓冲区填入背景色 (2 Bytes)。
 * 4. 使用 DMA 一次性搬运缓冲区数据到屏幕 GRAM。
 * @param  x, y   绘制起始坐标
 * @param  w, h   字符宽高
 * @param  dots   单色字模数据指针
 * @param  fg, bg 前景/背景颜色 (RGB565)
 */
static void TFT_Draw_Glyph_Base(
    uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* dots, uint16_t fg, uint16_t bg)
{
    // 1. 安全检查：防止缓冲区溢出
    if ((uint32_t) w * h * 2 > FONT_DMA_BUFFER_SIZE)
    {
        LOG_E("[ST7789] Font Size Overflow!");
        return; // 字符尺寸超过缓冲区限制，放弃绘制
    }

    // 2. 颜色预处理：拆分为高低字节 (ST7789 为大端序 MSB First)
    uint8_t fg_h = fg >> 8;
    uint8_t fg_l = fg & 0xFF;
    uint8_t bg_h = bg >> 8;
    uint8_t bg_l = bg & 0xFF;

    // 3. 计算对齐参数
    // 字模数据通常按字节对齐，每行不足 8 位也会占用 1 个字节
    // 例如宽度 12 像素 -> (12+7)/8 = 2 字节/行
    uint8_t bytes_per_row = (w + 7) / 8;

    uint32_t buf_idx = 0;

    // 4. 颜色扩展循环 (Color Expansion Loop)
    for (uint16_t row = 0; row < h; row++)
    {
        // 定位到当前行的字模数据起始地址
        const uint8_t* row_data = dots + (row * bytes_per_row);

        for (uint16_t col = 0; col < w; col++)
        {
            // 定位当前像素在字节中的位置
            // 假设取模格式为：横向取模，字节内低位在前 (LSB First)
            // 如果显示的字左右反了，请修改为: (0x80 >> bit_idx)
            uint8_t pixel_byte = row_data[col / 8];
            uint8_t bit_idx    = col % 8;

            if (pixel_byte & (1 << bit_idx))
            {
                // 笔画：填入前景色
                s_font_render_buf[buf_idx++] = fg_h;
                s_font_render_buf[buf_idx++] = fg_l;
            }
            else
            {
                // 背景：填入背景色
                s_font_render_buf[buf_idx++] = bg_h;
                s_font_render_buf[buf_idx++] = bg_l;
            }
        }
    }

    // 5. 启动 DMA 传输
    // 此时 CPU 已经完成工作，DMA 将负责把 s_font_render_buf 搬运到 SPI
    TFT_ShowImage_DMA(x, y, w, h, s_font_render_buf);
}

void TFT_Show_String(uint16_t           x,
                     uint16_t           y,
                     const char*        str,
                     const font_info_t* font,
                     uint16_t           color_fg,
                     uint16_t           color_bg)
{
    // === 获取互斥锁 ===
    // 保护屏幕资源，防止多任务重入导致花屏
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreTakeRecursive(g_mutex_lcd, portMAX_DELAY);
    }

    // 参数指针判空，防止空指针解引用导致 HardFault
    if (str == NULL || font == NULL)
    {
        goto exit;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    // 遍历字符串直到遇到结束符 '\0'
    while (*str)
    {
        // --- 特殊字符 '\n' ---
        if (*str == '\n')
        {
            cursor_x = x;              // 回车：X 回到起始点
            cursor_y += font->ascii_h; // 换行：Y 增加行高
            str++;
            continue;
        }

        // --- 越界检测 ---
        // 如果当前行已经超出屏幕底部，终止绘制
        if (cursor_y + font->cn_h > TFT_LINE_NUMBER)
        {
            break;
        }

        // 自动换行：如果当前字超出屏幕右侧
        if (cursor_x + font->cn_w > TFT_COLUMN_NUMBER)
        {
            cursor_x = x;
            cursor_y += font->cn_h; // 按照汉字高度换行，因为通常汉字比 ASCII 高
        }

        // --- 字符分类处理 (ASCII vs 汉字) ---

        // 场景 1: 标准 ASCII 字符 (0x20 ~ 0x7E)
        // 只要最高位是 0，即可兼容 GBK 中的 ASCII 部分
        if (*str >= 0x20 && *str <= 0x7E)
        {
            uint32_t char_idx = *str - 0x20;

            // 计算该字符在字库数组中的字节偏移量
            // 对齐计算：((Width + 7) / 8) * Height
            uint32_t bytes_per_row = (font->ascii_w + 7) / 8;
            uint32_t char_size     = bytes_per_row * font->ascii_h;

            // 调用 DMA 绘图
            TFT_Draw_Glyph_Base(cursor_x,
                                cursor_y,
                                font->ascii_w,
                                font->ascii_h,
                                font->ascii_map + (char_idx * char_size),
                                color_fg,
                                color_bg);

            // 光标右移
            cursor_x += font->ascii_w;
            str++; // 移动 1 字节
        }
        // 场景 2: 汉字或扩展字符 (GBK/Multibyte)
        else
        {
            const uint8_t* p_target_data = NULL;
            int            match_len     = 0;

            // 检查汉字库是否有效
            if (font->hzk_table && font->hzk_count > 0)
            {
                const uint8_t* p_base = (const uint8_t*) font->hzk_table;

                // 遍历汉字映射表查找匹配项
                for (uint16_t i = 0; i < font->hzk_count; i++)
                {
                    // 获取当前字库条目的指针
                    const uint8_t* p_entry = p_base + (i * font->hzk_struct_size);

                    // 获取对应的字符串索引 (假设结构体第一个成员是指向字符串的指针)
                    const char* key     = *(const char**) p_entry;
                    size_t      key_len = strlen(key);

                    // 字符串前缀匹配
                    if (strncmp(str, key, key_len) == 0)
                    {
                        // 找到匹配，计算字模数据地址
                        p_target_data = p_entry + font->hzk_data_offset;
                        match_len     = key_len;
                        break; // 退出查找循环
                    }
                }
            }

            if (p_target_data)
            {
                // 绘制汉字
                TFT_Draw_Glyph_Base(
                    cursor_x, cursor_y, font->cn_w, font->cn_h, p_target_data, color_fg, color_bg);

                cursor_x += font->cn_w;
                str += match_len; // 跳过已匹配的字节数 (如 GBK 为 2，UTF-8 为 3)
            }
            else
            {
                // 异常处理：未找到字模 (绘制纯色块占位，用于调试)
                TFT_Fill_Rect_DMA(cursor_x, cursor_y, font->cn_w, font->cn_h, RED);

                cursor_x += font->cn_w;

                // 强制跳过 1 字节，尝试重新对齐编码
                str++;
            }
        }
    }

exit:
    // === 释放互斥锁 ===
    if (g_mutex_lcd != NULL)
    {
        xSemaphoreGiveRecursive(g_mutex_lcd);
    }
}