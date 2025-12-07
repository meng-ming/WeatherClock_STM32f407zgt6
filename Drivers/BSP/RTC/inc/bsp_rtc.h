/**
 * @file    bsp_rtc.h
 * @brief   RTC (实时时钟) 驱动接口
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include <stdint.h>

/* ==================================================================
 * 1. 类型定义
 * ================================================================== */

/**
 * @brief 日历结构体
 * @note  用于存储日期、时间和星期信息
 */
typedef struct
{
    uint16_t year;  ///< 年份 (2000~2099)
    uint8_t  month; ///< 月份 (1~12)
    uint8_t  date;  ///< 日期 (1~31)

    uint8_t hour; ///< 小时 (0~23, 24小时制)
    uint8_t min;  ///< 分钟 (0~59)
    uint8_t sec;  ///< 秒 (0~59)

    uint8_t week; ///< 星期 (1~7, 周一~周日)
} BSP_RTC_Calendar_t;

/**
 * @brief RTC 操作状态枚举
 * @note  用于返回函数执行结果
 */
typedef enum
{
    BSP_RTC_OK           = 0, ///< 操作成功
    BSP_RTC_ERROR        = 1, ///< 操作失败
    BSP_RTC_TIMEOUT      = 2, ///< 操作超时
    BSP_RTC_ALREADY_INIT = 3  ///< 已初始化
} BSP_RTC_Status_e;

/* ==================================================================
 * 2. RTC 函数接口
 * ================================================================== */

/**
 * @brief 初始化 RTC 时钟外设
 * @note  配置 RTC 硬件并启动时钟
 * @retval BSP_RTC_OK: 成功
 * @retval 其他: 失败
 */
BSP_RTC_Status_e BSP_RTC_Init(void);

/**
 * @brief RTC 设置时间
 * @note  设置当前时、分、秒 (24小时制)
 * @param  hour: 小时 (0~23)
 * @param  min:  分钟 (0~59)
 * @param  sec:  秒 (0~59)
 * @retval BSP_RTC_OK: 成功
 * @retval 其他: 失败
 */
BSP_RTC_Status_e BSP_RTC_SetTime(uint8_t hour, uint8_t min, uint8_t sec);

/**
 * @brief RTC 设置日期
 * @note  设置年、月、日和星期
 * @param  year: 年份 (0~99, 代表 2000~2099)
 * @param  mon:  月份 (1~12)
 * @param  date: 日期 (1~31)
 * @retval BSP_RTC_OK: 成功
 * @retval 其他: 失败
 */
BSP_RTC_Status_e BSP_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t date);

/**
 * @brief 获取当前日历
 * @note  读取当前日期和时间到结构体
 * @param  calendar: 日历结构体指针 (输出)
 * @retval None
 */
void BSP_RTC_GetCalendar(BSP_RTC_Calendar_t* calendar);

#endif /* __BSP_RTC_H */