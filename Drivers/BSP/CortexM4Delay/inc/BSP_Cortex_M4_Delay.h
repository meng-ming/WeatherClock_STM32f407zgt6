#ifndef __BSP_CORTEX_M4_DELAY_H
#define __BSP_CORTEX_M4_DELAY_H

#include "stm32f4xx.h" // 根据你的具体型号调整头文件

void BSP_Cortex_M4_Delay_Init(void);
void BSP_Cortex_M4_Delay_us(uint32_t us);
void BSP_Cortex_M4_Delay_ms(uint32_t ms);

#endif