/*
 * FreeRTOS Kernel V10.x.x
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* --------------------------------------------------------------------------
 * 1. 头文件与时钟定义
 * -------------------------------------------------------------------------- */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
#include <stdint.h>
extern uint32_t SystemCoreClock;
#endif

/* --------------------------------------------------------------------------
 * 2. 基础内核配置
 * -------------------------------------------------------------------------- */
#define configUSE_PREEMPTION 1 /* 1: 抢占式调度 (标准), 0: 协作式 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION                                                    \
    1                             /* 1: 使用硬件计算前导零指令 (CLZ) 加速调度 (Cortex-M 专用) */
#define configUSE_TICKLESS_IDLE 0 /* 0: 关闭低功耗 tickless 模式 (时钟项目暂不需要) */
#define configCPU_CLOCK_HZ (SystemCoreClock)   /* CPU 主频 */
#define configTICK_RATE_HZ ((TickType_t) 1000) /* 系统节拍: 1000Hz (1ms) */
#define configMAX_PRIORITIES (32)              /* 最大优先级数 (建议设大点，反正只占少量RAM) */
#define configMINIMAL_STACK_SIZE ((unsigned short) 128) /* 空闲任务的最小堆栈大小 (字) */
#define configTOTAL_HEAP_SIZE ((size_t) (60 * 1024))    /* 系统堆大小: 60KB (SRAM1) */
#define configMAX_TASK_NAME_LEN (16)                    /* 任务名最大长度 */
#define configUSE_16_BIT_TICKS 0                        /* 0: 使用 32位 Tick 计数器 (防止溢出) */
#define configIDLE_SHOULD_YIELD 1                       /* 1: 空闲任务让出 CPU */
#define configUSE_TASK_NOTIFICATIONS 1                  /* 1: 开启任务通知 */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1         /* 每个任务的通知数组大小 */
#define configUSE_MUTEXES 1                             /* 1: 开启互斥锁 */
#define configUSE_RECURSIVE_MUTEXES 1                   /* 1: 开启递归互斥锁 */
#define configUSE_COUNTING_SEMAPHORES 1                 /* 1: 开启计数信号量 */
#define configUSE_ALTERNATIVE_API 0                     /* 已废弃，必须为 0 */
#define configQUEUE_REGISTRY_SIZE 8                     /* 队列注册表大小 (用于调试查看队列名) */
#define configUSE_QUEUE_SETS 1                          /* 1: 开启队列集合 */
#define configUSE_EVENT_GROUPS 1                        /* 1: 开启事件组 (用于全域监控) */
#define configUSE_TIME_SLICING 1                        /* 1: 同优先级任务轮转调度 */
#define configUSE_NEWLIB_REENTRANT 0 /* 0: 不使用 Newlib 重入保护 (除非你用的库非常依赖 malloc) */
#define configENABLE_BACKWARD_COMPATIBILITY 0 /* 0: 关闭向后兼容 */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configUSE_TRACE_FACILITY 1 /* 1： 开启可视化跟踪功能*/
#define configUSE_STATS_FORMATTING_FUNCTIONS                                                       \
    1 /* 开启统计格式化函数 (就是 vTaskList 和 vTaskGetRunTimeStats)*/

/* --------------------------------------------------------------------------
 * 3. 内存与 Hook (系统健壮性配置)
 * -------------------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION 0  /* 0: 不使用静态内存分配 */
#define configSUPPORT_DYNAMIC_ALLOCATION 1 /* 1: 使用动态内存分配 (heap_4.c) */
#define configAPPLICATION_ALLOCATED_HEAP 1 /* 开启应用程序分配堆*/

/* 钩子函数开关 */
#define configUSE_IDLE_HOOK 0            /* 空闲钩子 */
#define configUSE_TICK_HOOK 1            /* 开启 Tick 钩子 (驱动 BSP_Tick_Delay) */
#define configCHECK_FOR_STACK_OVERFLOW 2 /* 2: 深度栈溢出检测 (不仅查栈顶，还查填充值) */
#define configUSE_MALLOC_FAILED_HOOK 1   /* 开启内存申请失败钩子 (堆不足时报警) */
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0

/* --------------------------------------------------------------------------
 * 4. 软件定时器 (Software Timers)
 * -------------------------------------------------------------------------- */
#define configUSE_TIMERS 1                                   /* 1: 开启软件定时器 */
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1) /* 定时器任务优先级 (最高) */
#define configTIMER_QUEUE_LENGTH 10                          /* 定时器命令队列长度 */
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2)

/* --------------------------------------------------------------------------
 * 5. Cortex-M 中断优先级配置
 * -------------------------------------------------------------------------- */
#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS __NVIC_PRIO_BITS
#else
#define configPRIO_BITS 4 /* STM32F4 使用 4 位优先级 */
#endif

/* 最低中断优先级 (15) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 0x0F

/* * 能够调用 FreeRTOS API 的最高中断优先级
 * 0x05 = 5. 这意味着优先级 0~4 的中断绝对不能调用 FreeRTOS API，
 * 只有 5~15 的中断可以调用 xxxFromISR 函数。
 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 0x05

/* 内核中断优先级 (必须是最低，防止阻塞 OS) */
#define configKERNEL_INTERRUPT_PRIORITY                                                            \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* 系统调用中断优先级 (即 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 左移) */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY                                                       \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* 断言配置: 开发阶段的神器，出错死循环并保留现场 */
#define configASSERT(x)                                                                            \
    if ((x) == 0)                                                                                  \
    {                                                                                              \
        taskDISABLE_INTERRUPTS();                                                                  \
        for (;;);                                                                                  \
    }

/* --------------------------------------------------------------------------
 * 6. API 函数裁剪 (1=启用, 0=禁用)
 * -------------------------------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskCleanUpResources 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1 /* BSP_Tick_Delay.c 依赖此函数判断 OS 状态 */
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1 /* 用于调试查看任务栈剩余空间 */
#define INCLUDE_xTaskGetHandle 1
#define INCLUDE_eTaskGetState 1

/* --------------------------------------------------------------------------
 * 7. 中断处理函数映射 (对接 STM32 标准库/HAL库)
 * -------------------------------------------------------------------------- */
/* 将 FreeRTOS 的内核中断映射到 STM32 启动文件的中断向量名 */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */