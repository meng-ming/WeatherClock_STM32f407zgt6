#ifndef __BSP_CORTEX_M4_DELAY_H
#define __BSP_CORTEX_M4_DELAY_H

#include <stdint.h>

void BSP_Cortex_M4_Delay_Init(void);
void BSP_Cortex_M4_Delay_us(uint32_t us);
void BSP_Cortex_M4_Delay_ms(uint32_t ms);

#endif