/**
 * @file    bsp_iwdg.h
 * @brief   独立看门狗驱动接口
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __BSP_IWDG_H
#define __BSP_IWDG_H

#include "stm32f4xx.h"

/**
 * @brief  初始化独立看门狗
 * @note   LSI 频率约为 32kHz (30~60kHz 之间波动)
 * 超时时间公式: Tout = (4 * 2^prer * rlr) / LSI
 * @param  prescaler: 预分频系数 (IWDG_Prescaler_4 ~ IWDG_Prescaler_256)
 * @param  reload:    重装载值 (0 ~ 0xFFF)
 */
void BSP_IWDG_Init(uint8_t prescaler, uint16_t reload);

/**
 * @brief  喂狗 (重置计数器)
 * @note   必须在超时时间内调用，否则系统复位
 */
void BSP_IWDG_Feed(void);

#endif /* __BSP_IWDG_H */