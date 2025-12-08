/**
 ******************************************************************************
 * @file    main.h
 * @author  meng-ming
 * @version V1.0
 * @date    2025-12-08
 * @brief   系统全局配置与资源声明文件
 * @note    包含任务优先级配置、堆栈大小定义、全局信号量/队列声明。
 * 本文件应被 main.c 和 app_tasks.c 共同包含。
 ******************************************************************************
 */

#ifndef __MAIN_H
#define __MAIN_H

/* ==================================================================
 * 头文件包含 (Includes)
 * ================================================================== */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

/* ==================================================================
 * 任务配置 (Task Configuration)
 * @note 优先级数值越大，优先级越高 (FreeRTOS标准)
 * ================================================================== */

/* 启动任务: 负责创建所有业务任务，运行后自动删除 */
#define START_TASK_PRIO 1
#define START_TASK_STACK_SIZE 256

/* 天气任务: 负责网络请求与数据解析 (较耗栈) */
#define WEATHER_TASK_PRIO 2
#define WEATHER_TASK_STACK_SIZE 512

/* UI任务: 负责屏幕刷新 (生产者-消费者模型中的消费者) */
#define UI_TASK_PRIO 4
#define UI_TASK_STACK_SIZE 256 /* 优化后: 512->256 */

/* 日历任务: 负责本地时间维护 (轻量级) */
#define CALENDAR_TASK_PRIO 5
#define CALENDAR_TASK_STACK_SIZE 192 /* 优化后: 256->192 */

/* 守护任务: 负责全域监控与看门狗喂狗 (最高优先级，防止被饿死) */
#define DAEMON_TASK_PRIO 10
#define DAEMON_TASK_STACK_SIZE 192 /* 优化后: 128->192 (增加printf缓冲) */

/* ==================================================================
 * 事件组位定义 (Event Group Bits)
 * @note 用于 Daemon 任务监控各子系统存活状态
 * ================================================================== */
#define TASK_BIT_WEATHER (1 << 0)
#define TASK_BIT_UI (1 << 1)
#define TASK_BIT_CALENDAR (1 << 2)
#define ALL_TASK_BITS (TASK_BIT_WEATHER | TASK_BIT_UI | TASK_BIT_CALENDAR)

/* ==================================================================
 * 全局对象声明 (Global Externs)
 * ================================================================== */

/* 消息队列 */
extern QueueHandle_t g_weather_queue; /* 天气数据传输队列 */

/* 事件组 */
extern EventGroupHandle_t g_event_alive; /* 任务存活监控事件组 */

/* 互斥锁 */
extern SemaphoreHandle_t g_mutex_lcd; /* 保护 LCD 硬件资源 (递归锁) */
extern SemaphoreHandle_t g_mutex_log; /* 保护 串口打印资源 (递归锁) */

/* 任务句柄 (用于调试或挂起恢复) */
extern TaskHandle_t StartTask_Handler;
extern TaskHandle_t WeatherTask_Handler;
extern TaskHandle_t UITask_Handler;
extern TaskHandle_t CalendarTask_Handler;
extern TaskHandle_t DaemonTask_Handler;

/* ==================================================================
 * 外部接口声明 (Exported Functions)
 * ================================================================== */

/**
 * @brief  启动任务入口函数
 * @param  pvParameters: FreeRTOS 任务传参
 * @retval None
 */
void start_task(void* pvParameters);

#endif /* __MAIN_H */