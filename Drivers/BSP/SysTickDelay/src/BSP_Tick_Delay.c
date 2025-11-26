/**
 * @file    BSP_Tick_Delay.c
 * @brief   SysTick 延时服务实现
 */

#include "BSP_Tick_Delay.h"
#include "stm32f4xx.h" // 确保包含芯片头文件以访问 SysTick 寄存器
#include <stddef.h>    // for NULL

// === 内部变量 ===
static volatile uint64_t s_tick_ms_counter = 0; // 毫秒计数器 (由中断驱动)
static uint32_t          s_ticks_per_us    = 0; // 每微秒的 tick 数
static uint32_t          s_ticks_per_ms    = 0; // 每毫秒的 tick 数
static uint8_t           s_is_initialized  = 0; // 初始化标志位

/**
 * @brief  初始化 SysTick
 */
void BSP_SysTick_Init(void)
{
    // 防止重复初始化
    if (s_is_initialized)
        return;

    // 1. 更新系统时钟频率，确保 SystemCoreClock 变量正确
    SystemCoreClockUpdate();

    // 2. 计算参数
    // SysTick 频率通常等于 HCLK (SystemCoreClock)
    // 异常检查：防止主频过低导致除法结果为 0
    if (SystemCoreClock < 1000000)
    {
        // 理论上 F4 不会跑这么慢，但为了防御性编程，强制设为最小值或报错
        s_ticks_per_us = 1;
    }
    else
    {
        s_ticks_per_us = SystemCoreClock / 1000000;
    }

    s_ticks_per_ms = SystemCoreClock / 1000;

    // 3. 配置 SysTick
    // Reload Value = 1ms 的 tick 数 - 1
    SysTick->LOAD = s_ticks_per_ms - 1;
    SysTick->VAL  = 0; // 清空计数器

    // 4. 设置优先级 (SysTick 优先级通常设为最低，以免影响通信中断)
    NVIC_SetPriority(SysTick_IRQn, 0x0F); // 4位优先级，0xF 为最低

    // 5. 开启 SysTick
    // CLKSOURCE=1 (内核时钟), TICKINT=1 (开中断), ENABLE=1 (使能)
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    s_is_initialized = 1;
}

/**
 * @brief  SysTick 中断服务函数
 * @note   需在 stm32f4xx_it.c 中调用，或者直接在此处定义(取决于工程结构)
 * 如果 stm32f4xx_it.c 里已有同名函数，请将其注释掉或在其中调用本函数。
 */
void SysTick_Handler(void)
{
    s_tick_ms_counter++;
}

/**
 * @brief  获取毫秒时间戳 (原子读取)
 */
uint64_t BSP_GetTick_ms(void)
{
    uint64_t tick_val_1, tick_val_2;

    // 双重读取法：防止在读取 64 位变量的过程中发生中断导致高低位不一致
    // 这种方法比开关全局中断效率更高，且不影响系统实时性
    do
    {
        tick_val_1 = s_tick_ms_counter;
        tick_val_2 = s_tick_ms_counter;
    } while (tick_val_1 != tick_val_2);

    return tick_val_1;
}

/**
 * @brief  获取微秒时间戳
 */
uint64_t BSP_GetTick_us(void)
{
    uint32_t val_current;
    uint64_t ms_current;
    uint32_t load = SysTick->LOAD;

    // 异常检查：未初始化则返回 0，防止除零崩溃
    if (s_ticks_per_us == 0)
        return 0;

    // 核心逻辑：确保读取的 VAL 和 ms 是同一周期的
    do
    {
        ms_current  = BSP_GetTick_ms();
        val_current = SysTick->VAL;

        // 如果在读取 VAL 后，ms 发生了变化，说明刚才临界时刻发生了中断
        // 此时读到的 VAL 可能是重装载后的值，需要重新读取
    } while (ms_current != BSP_GetTick_ms());

    // 计算公式：
    // 总微秒 = (毫秒数 * 1000) + (当前ms内已走过的微秒数)
    // SysTick 是向下计数的，所以 (LOAD - VAL) 是已走过的 tick 数
    uint64_t us_offset = (load - val_current) / s_ticks_per_us;

    return (ms_current * 1000) + us_offset;
}

/**
 * @brief  毫秒延时
 */
void BSP_Delay_ms(uint32_t ms)
{
    // 转换为微秒处理
    BSP_Delay_us(ms * 1000);
}

/**
 * @brief  微秒延时
 */
void BSP_Delay_us(uint32_t us)
{
    // 异常检查：未初始化直接返回
    if (!s_is_initialized)
        return;

    uint64_t start_time = BSP_GetTick_us();

    // 阻塞等待
    // 使用差值计算 (now - start)，即使计数器溢出也能正确计算 (只要 uint64 不溢出)
    while ((BSP_GetTick_us() - start_time) < us)
    {
        // 空转
        // 可选：加入 __NOP() 防止编译器过度优化空循环
        __NOP();
    }
}