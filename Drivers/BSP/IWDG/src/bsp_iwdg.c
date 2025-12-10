/**
 * @file    bsp_iwdg.c
 * @brief   独立看门狗驱动实现
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "bsp_iwdg.h"
#include "stm32f4xx_iwdg.h"

/**
 * @brief  初始化独立看门狗
 * @example BSP_IWDG_Init(IWDG_Prescaler_64, 1000); // 约 2 秒超时
 */
void BSP_IWDG_Init(uint8_t prescaler, uint16_t reload)
{
    // 1. 取消写保护 (必须先写入 0x5555 才能修改 PR 和 RLR)
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    // 2. 设置预分频系数
    IWDG_SetPrescaler(prescaler);

    // 3. 设置重装载值
    IWDG_SetReload(reload);

    // 4. 把重装载值加载到计数器 (第一次喂狗)
    IWDG_ReloadCounter();

    // 5. 使能看门狗 (一旦开启无法关闭，直到复位)
    IWDG_Enable();
}

/**
 * @brief  喂狗
 */
void BSP_IWDG_Feed(void)
{
    // 写入 0xAAAA 喂狗
    IWDG_ReloadCounter();
}