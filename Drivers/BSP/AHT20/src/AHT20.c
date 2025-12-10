/**
 * @file    AHT20.c
 * @brief   AHT20 驱动实现 (软件 I2C)
 * @note    所有底层 I2C 操作均为 private (static)，仅对外暴露 Init 和 Read 接口。
 * @author  meng-ming
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "AHT20.h"
#include "BSP_Tick_Delay.h"
#include "stm32f4xx.h"

/* ==================================================================
 * 1. 私有宏定义
 * ================================================================== */
#define AHT20_ADDRESS 0x70 // I2C 写地址

/* 引脚配置 (根据原理图修改) */
#define AHT20_I2C_SCL_PORT GPIOB
#define AHT20_I2C_SCL_PIN GPIO_Pin_6
#define AHT20_I2C_SCL_CLK RCC_AHB1Periph_GPIOB

#define AHT20_I2C_SDA_PORT GPIOB
#define AHT20_I2C_SDA_PIN GPIO_Pin_7
#define AHT20_I2C_SDA_CLK RCC_AHB1Periph_GPIOB

/* 快速操作宏 */
#define I2C_SCL_H() GPIO_SetBits(AHT20_I2C_SCL_PORT, AHT20_I2C_SCL_PIN)
#define I2C_SCL_L() GPIO_ResetBits(AHT20_I2C_SCL_PORT, AHT20_I2C_SCL_PIN)
#define I2C_SDA_H() GPIO_SetBits(AHT20_I2C_SDA_PORT, AHT20_I2C_SDA_PIN)
#define I2C_SDA_L() GPIO_ResetBits(AHT20_I2C_SDA_PORT, AHT20_I2C_SDA_PIN)
#define I2C_READ_SDA() GPIO_ReadInputDataBit(AHT20_I2C_SDA_PORT, AHT20_I2C_SDA_PIN)

/* ==================================================================
 * 2. 私有函数声明 (Static 封印)
 * ================================================================== */
static void    I2C_Delay(void);
static void    I2C_Soft_Init(void);
static void    I2C_Start(void);
static void    I2C_Stop(void);
static void    I2C_Send_Byte(uint8_t txd);
static uint8_t I2C_Read_Byte(uint8_t ack);
static uint8_t I2C_Wait_Ack(void);
static void    I2C_Ack(void);
static void    I2C_NAck(void);
static uint8_t AHT20_Send_Command(uint8_t cmd, uint8_t data1, uint8_t data2);

/* ==================================================================
 * 3. I2C 底层实现
 * ================================================================== */

// 速率控制
static void I2C_Delay(void)
{
    BSP_Delay_us(10);
}

// GPIO 初始化
static void I2C_Soft_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(AHT20_I2C_SCL_CLK | AHT20_I2C_SDA_CLK, ENABLE);

    // 开漏输出模式 (OD)，兼顾输入输出
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;

    GPIO_InitStructure.GPIO_Pin = AHT20_I2C_SCL_PIN;
    GPIO_Init(AHT20_I2C_SCL_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = AHT20_I2C_SDA_PIN;
    GPIO_Init(AHT20_I2C_SDA_PORT, &GPIO_InitStructure);

    I2C_SCL_H();
    I2C_SDA_H();
}

// 起始信号
static void I2C_Start(void)
{
    I2C_SDA_H();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SDA_L();
    I2C_Delay();
    I2C_SCL_L();
}

// 停止信号
static void I2C_Stop(void)
{
    I2C_SCL_L();
    I2C_SDA_L();
    I2C_Delay();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SDA_H();
    I2C_Delay();
}

// 发送字节
static void I2C_Send_Byte(uint8_t byte)
{
    uint8_t t;
    I2C_SCL_L();
    for (t = 0; t < 8; t++)
    {
        if ((byte & 0x80) >> 7)
            I2C_SDA_H();
        else
            I2C_SDA_L();
        byte <<= 1;
        I2C_Delay();
        I2C_SCL_H();
        I2C_Delay();
        I2C_SCL_L();
        I2C_Delay();
    }
}

// 读取字节
static uint8_t I2C_Read_Byte(uint8_t ack)
{
    unsigned char i, receive = 0;
    I2C_SDA_H();
    for (i = 0; i < 8; i++)
    {
        I2C_SCL_L();
        I2C_Delay();
        I2C_SCL_H();
        receive <<= 1;
        if (I2C_READ_SDA())
            receive++;
        I2C_Delay();
    }

    if (!ack)
        I2C_NAck();
    else
        I2C_Ack();

    return receive;
}

// 等待 ACK
static uint8_t I2C_Wait_Ack(void)
{
    uint8_t ucErrTime = 0;
    I2C_SDA_H();
    I2C_Delay();
    I2C_SCL_H();
    I2C_Delay();

    while (I2C_READ_SDA())
    {
        ucErrTime++;
        if (ucErrTime > 250)
        {
            I2C_Stop();
            return 1;
        }
    }
    I2C_SCL_L();
    return 0;
}

// 发送 ACK
static void I2C_Ack(void)
{
    I2C_SCL_L();
    I2C_SDA_L();
    I2C_Delay();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SCL_L();
}

// 发送 NACK
static void I2C_NAck(void)
{
    I2C_SCL_L();
    I2C_SDA_H();
    I2C_Delay();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SCL_L();
}

// 发送 AHT20 指令序列
static uint8_t AHT20_Send_Command(uint8_t cmd, uint8_t data1, uint8_t data2)
{
    I2C_Start();
    I2C_Send_Byte(AHT20_ADDRESS);
    if (I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }

    I2C_Send_Byte(cmd);
    if (I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }

    I2C_Send_Byte(data1);
    if (I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }

    I2C_Send_Byte(data2);
    if (I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }

    I2C_Stop();
    return 0;
}

/* ==================================================================
 * 4. 公共接口实现 (API)
 * ================================================================== */

// 初始化
uint8_t AHT20_Init(void)
{
    uint8_t status;

    // 1. 底层初始化
    I2C_Soft_Init();
    BSP_Delay_ms(40); // 上电延时

    // 2. 读取状态字
    I2C_Start();
    I2C_Send_Byte(AHT20_ADDRESS | 0x01); // 读模式
    if (I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }
    status = I2C_Read_Byte(0); // NACK
    I2C_Stop();

    // 3. 检查校准位 (Bit3), 若未校准则发送初始化命令
    if ((status & 0x08) == 0)
    {
        AHT20_Send_Command(0xBE, 0x08, 0x00);
        BSP_Delay_ms(10);
    }

    return 0; // AHT20_OK
}

// 读取数据
uint8_t AHT20_Read_Data(float* temp, float* humi)
{
    uint8_t  buf[6];
    uint32_t raw_humi, raw_temp;

    // 1. 触发测量
    if (AHT20_Send_Command(0xAC, 0x33, 0x00))
        return 1;

    // 2. 等待转换
    BSP_Delay_ms(80);

    // 3. 读取 6 字节数据
    I2C_Start();
    I2C_Send_Byte(AHT20_ADDRESS | 0x01);
    if (I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }

    buf[0] = I2C_Read_Byte(1); // Status
    buf[1] = I2C_Read_Byte(1); // Humi
    buf[2] = I2C_Read_Byte(1); // Humi
    buf[3] = I2C_Read_Byte(1); // Humi/Temp
    buf[4] = I2C_Read_Byte(1); // Temp
    buf[5] = I2C_Read_Byte(0); // Temp (NACK)
    I2C_Stop();

    // 4. 检查忙状态 (Bit7)
    if ((buf[0] & 0x80) != 0)
        return 1;

    // 5. 数据转换
    raw_humi = ((uint32_t) buf[1] << 12) | ((uint32_t) buf[2] << 4) | ((buf[3] & 0xF0) >> 4);
    *humi    = (float) raw_humi * 100.0f / 1048576.0f;

    raw_temp = ((uint32_t) (buf[3] & 0x0F) << 16) | ((uint32_t) buf[4] << 8) | buf[5];
    *temp    = ((float) raw_temp * 200.0f / 1048576.0f) - 50.0f;

    return 0; // AHT20_OK
}