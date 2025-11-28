#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include <stdint.h>

// 日历结构体
typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  date;

    uint8_t hour;
    uint8_t min;
    uint8_t sec;

    uint8_t week;
} BSP_RTC_Calendar_t;

// 定义状态码
typedef enum
{
    BSP_RTC_OK           = 0,
    BSP_RTC_ERROR        = 1,
    BSP_RTC_TIMEOUT      = 2,
    BSP_RTC_ALREADY_INIT = 3
} BSP_RTC_Status_e;

/**
 * @brief 初始化 RTC 时钟外设
 * @retval 0:失败 1：成功
 */
BSP_RTC_Status_e BSP_RTC_Init(void);

/**
 * @brief RTC 设置时间
 * @param  hour,min,sec:24小时制
 */
BSP_RTC_Status_e BSP_RTC_SetTime(uint8_t hour, uint8_t min, uint8_t sec);

/**
 * @brief RTC 设置日期
 * @param year: 0~99 (代表 2000~2099)
 * @param week:1~7 (周一~周日)
 */
BSP_RTC_Status_e BSP_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t date);

/**
 * @brief 获取当前日历
 * @param calendar:日历结构体
 */
void BSP_RTC_GetCalendar(BSP_RTC_Calendar_t* calendar);

#endif /* __BSP_RTC_H */