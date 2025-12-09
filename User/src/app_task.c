/**
 ******************************************************************************
 * @file    app_tasks.c
 * @author  meng-ming
 * @version V1.0
 * @date    2025-12-08
 * @brief   FreeRTOS 任务具体实现文件
 * @note    包含所有业务任务的主循环逻辑：
 * - Weather: 网络通信与解析
 * - UI: 界面刷新
 * - Calendar: 实时时钟
 * - Daemon: 系统健康监控
 ******************************************************************************
 */

#include "main.h"
#include "sys_log.h"
#include "bsp_iwdg.h"
#include "bsp_rtc.h"
#include "stm32f4xx_iwdg.h"

/* 业务层模块头文件 */
#include "app_weather.h"
#include "app_ui.h"
#include "app_calendar.h"
#include "ui_main_page.h"

/* ==================================================================
 * 全局变量定义 (Global Definitions)
 * ================================================================== */
QueueHandle_t      g_weather_queue = NULL;
EventGroupHandle_t g_event_alive   = NULL;

TaskHandle_t StartTask_Handler;
TaskHandle_t WeatherTask_Handler;
TaskHandle_t UITask_Handler;
TaskHandle_t CalendarTask_Handler;
TaskHandle_t DaemonTask_Handler;

/* ==================================================================
 * 静态函数声明 (Static Declarations)
 * ================================================================== */
static void weather_task(void* pvParameters);
static void ui_task(void* pvParameters);
static void calendar_task(void* pvParameters);
static void daemon_task(void* pvParameters);

/* ==================================================================
 * 任务实现 (Task Implementation)
 * ================================================================== */

/**
 * @brief  启动任务 (Start Task)
 * @note   负责系统核心硬件初始化、OS组件创建、业务任务创建。
 * 任务创建完成后，该任务将自行删除以释放内存。
 * @param  pvParameters: 任务传参 (未使用)
 * @retval None
 */
void start_task(void* pvParameters)
{
    /* 1. 硬件与业务层初始化 */
    BSP_RTC_Status_e rtc_status = BSP_RTC_Init();
    if (rtc_status != BSP_RTC_OK && rtc_status != BSP_RTC_ALREADY_INIT)
    {
        LOG_E("[Start Task] RTC Error: %d", rtc_status);
    }

    LOG_I("[Start Task] Initializing UI...");
    APP_UI_Init();

    /* 注册 UI 回调函数给天气模块 (用于解耦，尽管Phase 3已用队列，但保留回调接口灵活性) */
    APP_Weather_Init(APP_UI_UpdateWeather, APP_UI_ShowStatus);

    /* 2. 创建 OS 通信对象 */
    /* 队列深度设为1，采用 overwrite 模式，保证 UI 总是显示最新天气 */
    g_weather_queue = xQueueCreate(1, sizeof(APP_Weather_Data_t));
    g_event_alive   = xEventGroupCreate();

    if (g_weather_queue == NULL || g_event_alive == NULL)
    {
        LOG_E("[Start Task] OS Object Create Failed! System Halt.");
        /* 严重故障：资源不足，死循环触发看门狗复位 */
        while (1);
    }

    /* 3. 创建所有业务任务 (进入临界区保护，防止创建过程中发生调度) */
    taskENTER_CRITICAL();

    xTaskCreate(weather_task,
                "Weather",
                WEATHER_TASK_STACK_SIZE,
                NULL,
                WEATHER_TASK_PRIO,
                &WeatherTask_Handler);
    xTaskCreate(calendar_task,
                "Calendar",
                CALENDAR_TASK_STACK_SIZE,
                NULL,
                CALENDAR_TASK_PRIO,
                &CalendarTask_Handler);
    xTaskCreate(ui_task, "UI", UI_TASK_STACK_SIZE, NULL, UI_TASK_PRIO, &UITask_Handler);
    xTaskCreate(
        daemon_task, "Daemon", DAEMON_TASK_STACK_SIZE, NULL, DAEMON_TASK_PRIO, &DaemonTask_Handler);

    LOG_I("[Start Task] All Tasks Created. System Running...");
    taskEXIT_CRITICAL();

    /* 4. 任务使命完成，自杀 */
    vTaskDelete(NULL);
}

/**
 * @brief  天气业务任务
 * @note   事件驱动模式：
 * - 等待串口空闲中断通知 (IDLE)
 * - 超时(10ms)运行状态机，处理连接超时逻辑
 * - 解析成功后将数据推入队列
 * @param  pvParameters: 未使用
 */
static void weather_task(void* pvParameters)
{
    /* 清理启动时可能残留的信号量 */
    ulTaskNotifyTake(pdTRUE, 0);

    /* 定义一个计时器，用于定期检查 RTC 状态 */
    static uint32_t s_rtc_check_timer = 0;

    while (1)
    {
        /* 1. 等待任务通知 (来自 UART IDLE 中断)
         * 超时 10ms 运行一次状态机
         */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

        /* 2. 执行天气业务状态机 */
        APP_Weather_Task();

        /* 3. 周期性检查 RTC 时间是否丢失 (每 5秒 检查一次) */
        if (xTaskGetTickCount() - s_rtc_check_timer > pdMS_TO_TICKS(5000))
        {
            s_rtc_check_timer = xTaskGetTickCount();

            // 检查 RTC 是否变成了 2000年
            if (BSP_RTC_Is_Time_Invalid())
            {
                LOG_W("[Weather] RTC Time Lost (Year 2000)! Force Syncing...");

                // 调用在 app_weather.c 里的强制更新函数
                APP_Weather_Force_Update();
            }
        }

        /* 4. 喂“软狗”：向事件组打卡 */
        if (g_event_alive)
        {
            xEventGroupSetBits(g_event_alive, TASK_BIT_WEATHER);
        }
    }
}

/**
 * @brief  日历任务
 * @note   周期性任务 (20ms)，负责 RTC 时间读取与同步。
 */
static void calendar_task(void* pvParameters)
{
    TickType_t       xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency    = 20; // 50Hz 刷新率

    while (1)
    {
        APP_Calendar_Task();

        /* 喂“软狗” */
        if (g_event_alive)
        {
            xEventGroupSetBits(g_event_alive, TASK_BIT_CALENDAR);
        }

        /* 绝对延时，保证时间精度 */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief  UI 消费者任务
 * @note   阻塞等待 g_weather_queue 消息。
 * 任何时候收到消息，立即申请屏幕锁并刷新。
 */
static void ui_task(void* pvParameters)
{
    APP_Weather_Data_t weather_cache;

    while (1)
    {
        /* * 等待队列消息
         * 超时设为 1000ms：确保即使没收到天气数据，也能每秒醒来喂软狗
         */
        if (xQueueReceive(g_weather_queue, &weather_cache, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            LOG_I("[UI] Received msg, updating screen...");
            APP_UI_Update(&weather_cache);
        }

        /* 关键：打卡必须在 if 外面，确保超时醒来也能打卡 */
        if (g_event_alive)
        {
            xEventGroupSetBits(g_event_alive, TASK_BIT_UI);
        }
    }
}

/**
 * @brief  守护任务 (Daemon)
 * @note   全域监控系统的核心：
 * 1. 负责喂硬件看门狗 (IWDG)
 * 2. 监控所有子任务的存活状态 (Event Group)
 * 3. 定期打印系统性能报表 (Stack Watermark, Heap)
 */
static void daemon_task(void* pvParameters)
{
    /* 初始化硬件看门狗: 125Hz, 1000 count -> 8秒超时 */
    BSP_IWDG_Init(IWDG_Prescaler_256, 1000);

    static uint32_t s_print_timer = 0;
    static char     s_info_buffer[512]; /* 静态Buffer，避免爆栈 */

    while (1)
    {
        /* * 等待所有任务打卡 (AND 逻辑)
         * 5秒内必须所有人到齐，否则视为系统故障
         */
        EventBits_t uxBits = xEventGroupWaitBits(g_event_alive,
                                                 ALL_TASK_BITS,
                                                 pdTRUE, /* 退出时清除位 */
                                                 pdTRUE, /* Wait For ALL */
                                                 pdMS_TO_TICKS(5000));

        if ((uxBits & ALL_TASK_BITS) == ALL_TASK_BITS)
        {
            /* 1. 系统健康 -> 喂硬件狗 */
            BSP_IWDG_Feed();

            /* 2. 每 10秒 打印性能报表 */
            if (xTaskGetTickCount() - s_print_timer > pdMS_TO_TICKS(10000))
            {
                s_print_timer = xTaskGetTickCount();

                LOG_D("==================================================");
                /* 使用 FreeRTOS 内核函数生成任务列表字符串 */
                vTaskList(s_info_buffer);
                LOG_D("%s", s_info_buffer);
                LOG_D("--------------------------------------------------");
                /* 打印 CCM 堆剩余空间 */
                LOG_D("Heap Free: %d Bytes", xPortGetFreeHeapSize());
                LOG_D("==================================================");
            }
        }
        else
        {
            /* * 系统故障 (有人没打卡)
             * 策略：停止喂狗，打印“尸检报告”，等待复位
             */
            LOG_E("System HANG! Bits: 0x%X", (unsigned int) uxBits);
            if (!(uxBits & TASK_BIT_WEATHER))
                LOG_E("ERROR: Weather Task Died!");
            if (!(uxBits & TASK_BIT_UI))
                LOG_E("ERROR: UI Task Died!");
            if (!(uxBits & TASK_BIT_CALENDAR))
                LOG_E("ERROR: Calendar Task Died!");
        }
    }
}