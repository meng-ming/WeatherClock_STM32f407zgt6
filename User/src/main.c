/**
 ******************************************************************************
 * @file    main.c
 * @author  meng-ming
 * @version V1.0
 * @date    2025-12-08
 * @brief   系统主入口文件
 * @note    包含 main() 函数、FreeRTOS Hook 函数以及堆内存定义。
 ******************************************************************************
 */

#include "main.h"
#include "BSP_Tick_Delay.h"
#include "uart_handle_variable.h"
#include "sys_log.h"

/* ==================================================================
 * 内存分布定义 (Memory Allocation)
 * ================================================================== */
/* * [核心优化] FreeRTOS 堆定义
 * 强制放置于 CCM RAM (64KB) 区域，释放 SRAM 供 DMA 使用
 * 需配合 Linker Script 中的 .ccmram (NOLOAD) 段定义
 */
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".ccmram")));

/* ==================================================================
 * 全局变量定义 (Global Definitions)
 * ================================================================== */
SemaphoreHandle_t g_mutex_lcd = NULL;
SemaphoreHandle_t g_mutex_log = NULL;

/* ==================================================================
 * OS 钩子函数 (Hooks)
 * ================================================================== */

/**
 * @brief  系统滴答钩子
 * @note   在 SysTick 中断中调用，用于维持裸机延时函数的时基
 */
void vApplicationTickHook(void)
{
    BSP_Tick_Increment();
}

/**
 * @brief  内存申请失败钩子
 * @note   当堆内存不足时调用。商业级代码应在此记录日志并复位。
 */
void vApplicationMallocFailedHook(void)
{
    LOG_E("FATAL: Malloc Failed! Heap exhausted!");
    for (;;);
}

/**
 * @brief  栈溢出钩子
 * @param  xTask: 溢出任务句柄
 * @param  pcTaskName: 溢出任务名称
 * @note   需在 FreeRTOSConfig.h 中开启 configCHECK_FOR_STACK_OVERFLOW
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    LOG_E("FATAL: Stack Overflow! Task: %s", pcTaskName ? pcTaskName : "NULL");
    for (;;);
}

/* ==================================================================
 * 主函数 (Entry Point)
 * ================================================================== */

int main(void)
{
    /* 1. 基础硬件层初始化 */
    /* 设置中断优先级分组 (FreeRTOS 强依赖 NVIC_PriorityGroup_4) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* 初始化系统滴答定时器 (裸机/OS共用) */
    BSP_SysTick_Init();

    /* 初始化调试串口 (printf 重定向) */
    UART_Init(&g_debug_uart_handler);

    /* 2. 创建系统级互斥锁 (递归锁) */
    g_mutex_lcd = xSemaphoreCreateRecursiveMutex();
    g_mutex_log = xSemaphoreCreateRecursiveMutex();

    if (g_mutex_lcd == NULL || g_mutex_log == NULL)
    {
        LOG_E("Critical Error: Mutex Create Failed!");
        while (1);
    }

    LOG_I("System Booting... (Build: %s %s)", __DATE__, __TIME__);

    /* 3. 创建启动任务 (Root Task) */
    BaseType_t xReturn = xTaskCreate(
        start_task, "Start_Task", START_TASK_STACK_SIZE, NULL, START_TASK_PRIO, &StartTask_Handler);

    if (xReturn == pdPASS)
    {
        LOG_I("Starting Scheduler...");
        /* 4. 移交控制权给调度器 (永不返回) */
        vTaskStartScheduler();
    }
    else
    {
        LOG_E("Failed to create Start Task!");
    }

    /* 5. 异常处理 (理论上不会运行到这里) */
    while (1)
    {
        LOG_E("Scheduler Failed or Heap too small!");
    }
}