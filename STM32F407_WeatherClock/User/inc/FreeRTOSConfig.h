#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_MALLOC_FAILED_HOOK 0 // 调试阶段先关闭
#define configUSE_TICK_HOOK 0
#define configUSE_IDLE_HOOK 0

/* CMSIS Headers 必须使用的宏定义 */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
#include <stdint.h>
// 假设你的时钟配置在 SPL 的 system_stm32f4xx.c 中设置了 SystemCoreClock 变量
extern uint32_t SystemCoreClock;
#endif

/* 基础配置，请勿随意改动 */
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configCPU_CLOCK_HZ (SystemCoreClock)   // 使用系统时钟
#define configTICK_RATE_HZ ((TickType_t) 1000) // 1ms 节拍
#define configMAX_PRIORITIES (5)
#define configMINIMAL_STACK_SIZE ((uint16_t) 128)
#define configTOTAL_HEAP_SIZE ((size_t) (30 * 1024)) // 30KB堆空间
#define configMAX_TASK_NAME_LEN (16)
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TASK_NOTIFICATIONS 1

/* 内存和钩子函数配置 */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1

/* 关键中断配置：适配 Cortex-M4F */
// 优先级分组必须与你的 SPL 中断分组一致 (通常是 NVIC_PriorityGroup_4)
#define configKERNEL_INTERRUPT_PRIORITY (255) // 最低优先级 15 (0xFF)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY                                                       \
    (191) // 优先级 11 (0xB0)。中断优先级必须小于等于此值才能调用 FreeRTOS API。

/* 将 FreeRTOS 端口中断映射到 CMSIS 启动文件 */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/* 常用 API 开关，减小体积 */
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1

#endif /* FREERTOS_CONFIG_H */