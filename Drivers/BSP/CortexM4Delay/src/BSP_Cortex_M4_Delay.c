#include "BSP_Cortex_M4_Delay.h"

// 静态变量保存每微秒的 tick 数
static uint32_t s_fac_us = 0;

/**
 * @brief  初始化 DWT 延时计数器
 * @note   必须在系统时钟配置完成后调用
 */
void BSP_Cortex_M4_Delay_Init(void)
{
    // 1. 关闭 TRC (Trace)
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
    // 2. 开启 TRC
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // 3. 清除计数器
    DWT->CYCCNT = 0;
    // 4. 开启 CYCCNT 计数
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // 计算每微秒需要的时钟周期数
    // SystemCoreClock 是库函数维护的系统频率，例如 168000000
    s_fac_us = SystemCoreClock / 1000000;
}

/**
 * @brief  微秒级延时 (阻塞式，精确)
 * @param  us: 延时微秒数
 */
void BSP_Cortex_M4_Delay_us(uint32_t us)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = DWT->CYCCNT; // 只是为了获取当前值，DWT不需要reload，它是32位自增

    ticks = us * s_fac_us;
    told  = DWT->CYCCNT;

    while (1)
    {
        tnow = DWT->CYCCNT;
        if (tnow != told)
        {
            // 处理 32 位计数器溢出回绕的情况
            if (tnow > told)
                tcnt += tnow - told;
            else
                tcnt += (UINT32_MAX - told) + tnow + 1; // 修正溢出计算

            told = tnow;

            if (tcnt >= ticks)
                break;
        }
    }
}

/**
 * @brief  毫秒级延时 (阻塞式，用于驱动初始化)
 * @param  ms: 延时毫秒数
 */
void BSP_Cortex_M4_Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        BSP_Cortex_M4_Delay_us(1000);
    }
}