/**
 * @file    main.c
 * @author  meng-ming
 * @version V1.0
 * @date    2025-12-08
 * @brief   ϵͳ������ļ�?
 * @note    ���� main() ������FreeRTOS Hook
 * �����Լ����ڴ涨�塣
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "main.h"
#include "BSP_Tick_Delay.h"
#include "uart_handle_variable.h"
#include "sys_log.h"

/* ==================================================================
 * �ڴ�ֲ�����? (Memory Allocation)
 * ================================================================== */
/* * [�����Ż�] FreeRTOS �Ѷ���
 * ǿ�Ʒ����� CCM RAM (64KB) �����ͷ� SRAM �� DMA ʹ��
 * �����? Linker Script �е� .ccmram (NOLOAD) �ζ���
 */
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".ccmram")));

/* ==================================================================
 * ȫ�ֱ������� (Global Definitions)
 * ================================================================== */
SemaphoreHandle_t g_mutex_lcd = NULL;
SemaphoreHandle_t g_mutex_log = NULL;

/* ==================================================================
 * OS ���Ӻ��� (Hooks)
 * ================================================================== */

/**
 * @brief  ϵͳ�δ���
 * @note   �� SysTick �ж��е��ã�����ά�������ʱ������ʱ��?
 */
void vApplicationTickHook(void)
{
    BSP_Tick_Increment();
}

/**
 * @brief  �ڴ�����ʧ�ܹ���
 * @note   �����ڴ治��ʱ���á���ҵ������Ӧ�ڴ˼�¼��־����λ��
 */
void vApplicationMallocFailedHook(void)
{
    LOG_E("[Main] Malloc Failed! System Resetting...");
    BSP_Delay_ms(100);
    NVIC_SystemReset(); // 这种错误没法救，直接复位是最好的解脱
}

/**
 * @brief  ջ�������?
 * @param  xTask: ���������?
 * @param  pcTaskName: �����������?
 * @note   ���� FreeRTOSConfig.h �п��� configCHECK_FOR_STACK_OVERFLOW
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    LOG_E("[Main] FATAL: Stack Overflow! Task: %s", pcTaskName ? pcTaskName : "NULL");
    for (;;);
}

/* ==================================================================
 * ������ (Entry Point)
 * ================================================================== */

int main(void)
{
    /* 1. ����Ӳ�����ʼ��? */
    /* �����ж����ȼ����� (FreeRTOS ǿ���� NVIC_PriorityGroup_4) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* ��ʼ��ϵͳ�δ�ʱ�� (���?/OS����) */
    BSP_SysTick_Init();

    /* ��ʼ�����Դ��� (printf �ض���) */
    UART_Init(&g_debug_uart_handler);

    /* 2. ����ϵͳ�������� (�ݹ���) */
    g_mutex_lcd = xSemaphoreCreateRecursiveMutex();
    g_mutex_log = xSemaphoreCreateRecursiveMutex();

    if (g_mutex_lcd == NULL || g_mutex_log == NULL)
    {
        LOG_E("[Main] Critical Error: Mutex Create Failed!");
        while (1);
    }

    LOG_I("[Main] System Booting... (Build: %s %s)", __DATE__, __TIME__);

    /* 3. ������������ (Root Task) */
    BaseType_t xReturn = xTaskCreate(
        start_task, "Start_Task", START_TASK_STACK_SIZE, NULL, START_TASK_PRIO, &StartTask_Handler);

    if (xReturn == pdPASS)
    {
        LOG_I("[Main] Starting Scheduler...");
        /* 4. �ƽ�����Ȩ�������� (��������) */
        vTaskStartScheduler();
    }
    else
    {
        LOG_E("[Main] Failed to create Start Task!");
    }

    /* 5. �쳣���� (�����ϲ������е�����) */
    while (1)
    {
        LOG_E("[Main] Scheduler Failed or Heap too small!");
    }
}