/**
 * @file    BSP_Tick_Delay.h
 * @brief   基于 SysTick 的高精度延时与时间戳服务 (FreeRTOS 适配版)
 * @note    提供微秒级 (us) 和毫秒级 (ms) 的延时，以及系统运行时间戳获取。
 * 已适配 FreeRTOS：
 * - 调度器启动前：使用 SysTick 轮询模式。
 * - 调度器启动后：ms 延时自动切换为 vTaskDelay (释放CPU)。
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __BSP_TICK_DELAY_H__
#define __BSP_TICK_DELAY_H__

#include <stdint.h>

/* ==================================================================
 * 1. SysTick 延时与时间戳函数接口
 * ================================================================== */

/**
 * @brief  初始化 SysTick 定时器
 * @note   在 FreeRTOS 模式下，此函数主要用于计算微秒延时参数。
 * SysTick 的硬件中断后续将由 OS 接管。
 * 必须在系统启动初期（main 函数开头）调用。
 */
void BSP_SysTick_Init(void);

/**
 * @brief  SysTick 计数增加回调 (FreeRTOS Hook)
 * @note   **核心函数**：需在 vApplicationTickHook 中调用此函数，
 * 以维持 BSP 层的毫秒计数器更新。
 */
void BSP_Tick_Increment(void);

/**
 * @brief  获取系统启动以来的毫秒数
 * @note   线程安全，支持原子读取。
 * @retval uint64_t: 系统启动后的总毫秒数
 */
uint64_t BSP_GetTick_ms(void);

/**
 * @brief  获取系统启动以来的微秒数
 * @note   结合毫秒计数器和 SysTick 当前计数值计算。
 * @retval uint64_t: 系统启动后的总微秒数
 */
uint64_t BSP_GetTick_us(void);

/**
 * @brief  毫秒级延时 (智能切换)
 * @note   - OS 运行中：调用 vTaskDelay (阻塞，释放CPU)。
 * - OS 未启动：调用死等延时 (不释放CPU)。
 * @param  ms: 要延时的毫秒数
 */
void BSP_Delay_ms(uint32_t ms);

/**
 * @brief  微秒级延时 (死等)
 * @note   始终使用死等方式，用于底层硬件时序控制 (如 I2C/LCD)。
 * @param  us: 要延时的微秒数
 */
void BSP_Delay_us(uint32_t us);

#endif /* __BSP_TICK_DELAY_H__ */