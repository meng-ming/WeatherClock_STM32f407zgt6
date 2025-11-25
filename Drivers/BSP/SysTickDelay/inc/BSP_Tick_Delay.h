#ifndef __BSP_TICK_DELAY_H__
#define __BSP_TICK_DELAY_H__

#include <stdint.h>

void BSP_SysTick_Init(void);

// 获取系统启动以来的毫秒数 (解决64位原子读取问题)
uint64_t BSP_GetTick_ms(void);

// 获取系统启动以来的微秒数 (核心功能)
uint64_t BSP_GetTick_us(void);

// 真正的精准延时
void BSP_Delay_ms(uint32_t ms);
void BSP_Delay_us(uint32_t us);

#endif