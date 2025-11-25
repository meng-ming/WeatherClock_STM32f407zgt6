#include "BSP_Tick_Delay.h"
#include "stm32f4xx.h" // 确保包含正确的芯片头文件

static volatile uint64_t s_tick_ms_counter = 0;
static uint32_t          s_ticks_per_us    = 0;
static uint32_t          s_ticks_per_ms    = 0;

void BSP_SysTick_Init(void)
{
    // 1. 更新系统时钟频率，防止 SystemCoreClock 变量不对
    SystemCoreClockUpdate();

    // 2. 计算参数
    // SysTick 频率通常等于 HCLK (SystemCoreClock)
    s_ticks_per_us = SystemCoreClock / 1000 / 1000;
    s_ticks_per_ms = SystemCoreClock / 1000;

    // 3. 配置 SysTick
    SysTick->LOAD = s_ticks_per_ms - 1; // 计数是从 N-1 到 0
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | // 使用内核时钟 HCLK
                    SysTick_CTRL_TICKINT_Msk |   // 开启中断
                    SysTick_CTRL_ENABLE_Msk;     // 使能定时器
}

// 供中断服务函数调用 (需在 stm32f4xx_it.c 中调用)
void SysTick_Handler(void)
{
    s_tick_ms_counter++;
}

// 无锁读取64位变量，防止读取过程中被中断修改
uint64_t BSP_GetTick_ms(void)
{
    uint64_t tick_val_1, tick_val_2;
    do
    {
        tick_val_1 = s_tick_ms_counter;
        tick_val_2 = s_tick_ms_counter;
    } while (tick_val_1 != tick_val_2);

    return tick_val_1;
}

// 获取精确的微秒时间戳
uint64_t BSP_GetTick_us(void)
{
    uint32_t val_current;
    uint64_t ms_current;
    uint32_t load = SysTick->LOAD;

    // 这里有个坑：如果在读取 VAL 之后，SysTick 刚好溢出触发中断，
    // 导致 s_tick_ms_counter 增加，那么算出来的时间会回退。
    // 必须确保读取 VAL 和读取 ms_counter 是匹配的。
    do
    {
        ms_current  = BSP_GetTick_ms();
        val_current = SysTick->VAL;

        // 再次检查 ms 是否变化，如果变了，说明刚才读 VAL 期间发生了中断
        // 需要重新读取，保证数据一致性
    } while (ms_current != BSP_GetTick_ms());

    // 计算公式：ms * 1000 + (LOAD - VAL) / ticks_per_us
    // (LOAD - VAL) 是当前这 1ms 内已经走过的 tick 数
    uint64_t us_offset = (load - val_current) / s_ticks_per_us;

    return (ms_current * 1000) + us_offset;
}

void BSP_Delay_ms(uint32_t ms)
{
    // 基于微秒时间戳实现，避免依赖 tick 中断的边界效应
    BSP_Delay_us(ms * 1000);
}

void BSP_Delay_us(uint32_t us)
{
    uint64_t start_time = BSP_GetTick_us();

    // 即使 us 很大，或者跨越了 ms 边界，因为我们用的是 64位单调递增时间戳
    // 只要做减法，溢出问题自然解决 (在几百年内不会溢出)
    while ((BSP_GetTick_us() - start_time) < us)
    {
        // 这里可以加 __WFI() 但在纯延时函数里慎用，可能影响唤醒延迟
        // 纯空转即可
    }
}