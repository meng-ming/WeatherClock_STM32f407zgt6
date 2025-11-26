/**
 * @file    BSP_Tick_Delay.h
 * @brief   基于 SysTick 的高精度延时与时间戳服务
 * @note    提供微秒级 (us) 和毫秒级 (ms) 的阻塞延时，以及系统运行时间戳获取。
 * 使用 64 位计数器，解决 32 位计数器 49 天溢出问题。
 */

#ifndef __BSP_TICK_DELAY_H__
#define __BSP_TICK_DELAY_H__

#include <stdint.h>

/**
 * @brief  初始化 SysTick 定时器
 * @note   配置 SysTick 为 1ms 中断一次，并校准每微秒的计数值。
 * 必须在系统启动初期（如 main 函数开头）调用且仅调用一次。
 * @retval None
 */
void BSP_SysTick_Init(void);

/**
 * @brief  获取系统启动以来的毫秒数
 * @note   线程安全，使用双重读取法防止读取过程中发生中断导致的 64 位数据撕裂。
 * @retval uint64_t: 系统启动后的总毫秒数
 */
uint64_t BSP_GetTick_ms(void);

/**
 * @brief  获取系统启动以来的微秒数
 * @note   结合毫秒计数器和 SysTick 当前计数值，计算精确的微秒时间戳。
 * 精度取决于 SysTick 时钟源频率。
 * @retval uint64_t: 系统启动后的总微秒数
 */
uint64_t BSP_GetTick_us(void);

/**
 * @brief  毫秒级阻塞延时
 * @note   基于微秒时间戳实现，相比简单的循环延时更精准，且不依赖编译器优化等级。
 * @param  ms: 要延时的毫秒数
 * @retval None
 */
void BSP_Delay_ms(uint32_t ms);

/**
 * @brief  微秒级阻塞延时
 * @note   基于系统时间戳差值计算，即使跨越毫秒中断边界也能准确延时。
 * @param  us: 要延时的微秒数
 * @retval None
 */
void BSP_Delay_us(uint32_t us);

#endif /* __BSP_TICK_DELAY_H__ */