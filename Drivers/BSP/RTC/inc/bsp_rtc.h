/**
 * @file    bsp_rtc.h
 * @brief   高健壮性 RTC (实时时钟) 驱动接口
 * @note    本驱动实现了商业级的高可靠性时钟管理：
 * 1. 双时钟源策略：优先 LSE (32.768kHz)，故障时自动切换 LSI。
 * 2. 自适应校准：使用 LSI 时，自动利用 TIM5 测量频率并修正分频系数。
 * 3. 故障自愈：运行时检测 LSE 停振，自动复位备份域并降级运行，保活系统。
 * @author  meng-ming
 * @version 2.0
 * @date    2025-12-09
 */

#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include <stdint.h>

/* ==================================================================
 * 1. 类型定义 (Type Definitions)
 * ================================================================== */

/**
 * @brief 日历结构体
 * @note  用于存储人类可读的日期时间格式
 */
typedef struct
{
    uint16_t year;  ///< 年份 (如 2025)
    uint8_t  month; ///< 月份 (1~12)
    uint8_t  date;  ///< 日期 (1~31)

    uint8_t hour; ///< 小时 (0~23, 24小时制)
    uint8_t min;  ///< 分钟 (0~59)
    uint8_t sec;  ///< 秒   (0~59)

    uint8_t week; ///< 星期 (1~7, 1=周一 ... 7=周日)
} BSP_RTC_Calendar_t;

/**
 * @brief RTC 操作状态枚举
 */
typedef enum
{
    BSP_RTC_OK           = 0, ///< 操作成功 (冷启动初始化成功)
    BSP_RTC_ERROR        = 1, ///< 操作失败 (硬件故障)
    BSP_RTC_TIMEOUT      = 2, ///< 操作超时 (时钟源未就绪)
    BSP_RTC_ALREADY_INIT = 3  ///< 已初始化 (热启动/复位，无需重新配置)
} BSP_RTC_Status_e;

/* ==================================================================
 * 2. 核心控制接口 (Core Control API)
 * ================================================================== */

/**
 * @brief  初始化 RTC 外设 (带故障自愈功能)
 * @note   启动流程：
 * 1. 优先尝试启动 LSE (外部 32.768kHz)。
 * 2. 若 LSE 启动超时，自动切换至 LSI (内部 RC)。
 * 3. 若使用 LSI，自动调用 TIM5 测量其实际频率并校准 RTC 分频系数。
 * 4. 若检测到 LSE 在运行中损坏，自动复位备份域并切换至 LSI。
 * @retval BSP_RTC_OK:           初始化成功 (首次上电)
 * @retval BSP_RTC_ALREADY_INIT: 系统复位但 RTC 仍在运行 (无需配置)
 * @retval BSP_RTC_ERROR:        严重硬件故障
 */
BSP_RTC_Status_e BSP_RTC_Init(void);

/**
 * @brief  检查 RTC 时间是否无效 (即是否发生过掉电或复位)
 * @note   用于判断是否需要立即触发网络校时 (NTP)。
 * 原理：当发生备份域复位（LSE故障或电池掉电）时，
 * 底层驱动会将时间重置为 2000-01-01。
 * @retval 1: 时间无效 (年份为 2000，需联网更新)
 * @retval 0: 时间正常 (年份非 2000)
 */
uint8_t BSP_RTC_Is_Time_Invalid(void);

/* ==================================================================
 * 3. 时间读写接口 (Time Access API)
 * ================================================================== */

/**
 * @brief  设置时间
 * @note   设置当前时、分、秒 (24小时制)
 * @param  hour: 小时 (0~23)
 * @param  min:  分钟 (0~59)
 * @param  sec:  秒   (0~59)
 * @retval BSP_RTC_OK: 成功
 * @retval BSP_RTC_ERROR: 失败
 */
BSP_RTC_Status_e BSP_RTC_SetTime(uint8_t hour, uint8_t min, uint8_t sec);

/**
 * @brief  设置日期
 * @note   设置年、月、日，内部会自动计算星期几
 * @param  year: 年份 (0~99, 代表 2000~2099)
 * @param  mon:  月份 (1~12)
 * @param  date: 日期 (1~31)
 * @retval BSP_RTC_OK: 成功
 * @retval BSP_RTC_ERROR: 失败
 */
BSP_RTC_Status_e BSP_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t date);

/**
 * @brief  获取当前日历
 * @note   原子操作读取日期和时间，并转换为十进制格式
 * @param  calendar: 日历结构体指针 (输出参数)
 * @retval None
 */
void BSP_RTC_GetCalendar(BSP_RTC_Calendar_t* calendar);

#endif /* __BSP_RTC_H */