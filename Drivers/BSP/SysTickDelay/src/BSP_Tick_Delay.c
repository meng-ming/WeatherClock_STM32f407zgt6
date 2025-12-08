/**
 * @file    BSP_Tick_Delay.c
 * @brief   SysTick 延时服务实现 (FreeRTOS 适配版)
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */
/* === FreeRTOS 头文件 === */
#include "FreeRTOS.h"
#include "task.h"

#include "BSP_Tick_Delay.h"
#include "stm32f4xx.h"
#include <stddef.h>

// === 内部变量 ===
static volatile uint64_t s_tick_ms_counter = 0;
static uint32_t          s_ticks_per_us    = 0;
static uint8_t           s_is_initialized  = 0;

void BSP_SysTick_Init(void)
{
    if (s_is_initialized)
        return;

    SystemCoreClockUpdate();

    if (SystemCoreClock < 1000000)
    {
        s_ticks_per_us = 1;
    }
    else
    {
        s_ticks_per_us = SystemCoreClock / 1000000;
    }

    // 配置 SysTick (用于调度器启动前的裸机延时)
    uint32_t ticks_per_ms = SystemCoreClock / 1000;
    SysTick->LOAD         = ticks_per_ms - 1;
    SysTick->VAL          = 0;
    NVIC_SetPriority(SysTick_IRQn, 0x0F);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    s_is_initialized = 1;
}

// 供 FreeRTOS 的 vApplicationTickHook 调用
void BSP_Tick_Increment(void)
{
    s_tick_ms_counter++;
}

uint64_t BSP_GetTick_ms(void)
{
    uint64_t tick_val_1, tick_val_2;

    // 双重读取防撕裂
    do
    {
        tick_val_1 = s_tick_ms_counter;
        tick_val_2 = s_tick_ms_counter;
    } while (tick_val_1 != tick_val_2);

    return tick_val_1;
}

uint64_t BSP_GetTick_us(void)
{
    uint32_t val_current;
    uint64_t ms_current;
    uint32_t load = SysTick->LOAD;

    if (s_ticks_per_us == 0)
        return 0;

    do
    {
        ms_current  = BSP_GetTick_ms();
        val_current = SysTick->VAL;
        // 校验读取过程中是否发生了中断溢出
    } while (ms_current != BSP_GetTick_ms());

    uint64_t us_offset = (load - val_current) / s_ticks_per_us;

    return (ms_current * 1000) + us_offset;
}

void BSP_Delay_ms(uint32_t ms)
{
    // 如果调度器运行中，使用 OS 延时释放 CPU
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else
    {
        BSP_Delay_us(ms * 1000);
    }
}

void BSP_Delay_us(uint32_t us)
{
    if (!s_is_initialized)
        return;

    uint64_t start_time = BSP_GetTick_us();

    while ((BSP_GetTick_us() - start_time) < us)
    {
        __NOP();
    }
}