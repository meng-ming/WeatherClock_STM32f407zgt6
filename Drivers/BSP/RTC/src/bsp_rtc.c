#include "bsp_rtc.h"
#include "stm32f4xx.h"
#include "app_data.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_rcc.h"
#include <stdint.h>
#include "stm32f4xx_rtc.h"
#include "sys_log.h"

/**
 * @brief  根据年月日计算星期几 (基姆拉尔森公式)
 * @param  year  : 20xx (比如 2025)
 * @param  month : 1~12
 * @param  day   : 1~31
 * @retval 1~7 (1=周一, 7=周日) 符合 STM32 RTC 标准
 */
static uint8_t BSP_RTC_Cal_Week(uint16_t year, uint8_t month, uint8_t day)
{

    // 算法要求：1月和2月要看作上一年的13月和14月
    if (month < 3)
    {
        month += 12;
        year--;
    }

    int w = (day + 2 * month + 3 * (month + 1) / 5 + year + year / 4 - year / 100 + year / 400) % 7;

    // 此时 w 的范围是 0~6
    // STM32 定义：RTC_WeekDay_Monday = 0x01 ... RTC_WeekDay_Sunday = 0x07
    // 因此需要 +1

    return (uint8_t) (w + 1);
}

BSP_RTC_Status_e BSP_RTC_Init(void)
{
    // 1. 使能 PWR 时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

    // 2. 解锁备份域
    PWR_BackupAccessCmd(ENABLE);

    // 3. 配置时钟(首选LSE 32.768khz,次选 LSI)
    // 判断是否是第一次配置RTC
    if (RTC_ReadBackupRegister(RTC_BKP_DR0) != 0xA0A2)
    {
        // 退出 RTC 死锁
        RCC_BackupResetCmd(ENABLE);  // 按下复位键
        RCC_BackupResetCmd(DISABLE); // 松开复位键

        // 3.1 尝试启动 LSE
        RCC_LSEConfig(RCC_LSE_ON);
        LOG_I("[RTC] First Init!");

        // 3.2 等待保护机制，防止 LSE 一直不启动
        uint32_t wait_lse_tick = 0;
        while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
        {
            wait_lse_tick++;
            if (wait_lse_tick > 0xFFFF)
            {
                LOG_W("[RTC] LSE Failed, switching to LSI.");
                break;
            }
        }

        // 3.3 再次判断 LSE 是否成功启动
        if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == SET)
        {
            RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
            LOG_I("[RTC] LSE Source Selected!");
        }
        else
        {
            // 启动 LSI
            RCC_LSICmd(ENABLE);

            wait_lse_tick = 0;
            while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
            {
                wait_lse_tick++;
                if (wait_lse_tick > 0xFFFF)
                {
                    LOG_W("[RTC] LSE And LSI All Failed !!!");
                    return BSP_RTC_TIMEOUT; // 两个时钟都不起作用
                }
            }

            RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
            LOG_W("[RTC] LSE Timeout!LSI Source Selected! (Low Precision)");
        }

        // 4. 开启外设时钟
        RCC_RTCCLKCmd(ENABLE);

        // 5. 等待 RTC 内部影子寄存器和 APB 总线同步
        RTC_WaitForSynchro();

        // 6. 配置分频，保证周期为1s
        // ck_freq(1Hz) = RTCCLK / ((AsynchPrediv + 1) * (SynchPrediv + 1))
        // 只要使用 LSE 外部时钟，直接就填入这两个参数
        RTC_InitTypeDef RTC_InitStructure;
        RTC_InitStructure.RTC_AsynchPrediv = 0x7F; // 127
        RTC_InitStructure.RTC_SynchPrediv  = 0xFF; // 255
        RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;
        if (RTC_Init(&RTC_InitStructure) == ERROR)
        {
            LOG_E("[RTC] Prescaler Init Failed!");
            return BSP_RTC_ERROR;
        }

        // 8. 设置默认时间
        if (BSP_RTC_SetTime(12, 0, 0) == BSP_RTC_ERROR)
            return BSP_RTC_ERROR;
        if (BSP_RTC_SetDate(25, 11, 2) == BSP_RTC_ERROR)
            return BSP_RTC_ERROR;

        RTC_WriteBackupRegister(RTC_BKP_DR0, 0xA0A2);
        LOG_I("[RTC] Init Success!");
        return BSP_RTC_OK;
    }
    else
    {
        // 当在 if 条件中外部晶振失效并且启动LSI之后，如果复位或者重启，由于 BKP
        // 掉电不丢失，导致不会再次进入 if 条件判断中，从而导致不会重启 LSI

        // 1. 检查当前 RTC 的时钟源是不是 LSI
        // (RCC_BDCR 寄存器里的 RTCSEL 位)
        if (((RCC->BDCR >> 8) & 0x03) == 0x02) // 0x02 代表 LSI
        {
            LOG_I("[RTC] RTC is using LSI. Re-enabling LSI...");

            // 2. 因为复位会关掉 LSI，所以必须手动重新开启
            RCC_LSICmd(ENABLE);

            // 3. 等待 LSI 就绪
            uint16_t retry = 0;
            while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
            {
                retry++;
                if (retry > 0x1FFF)
                {
                    LOG_W("[RTC] LSE Failed And LSI Restart Failed !!!");
                    return BSP_RTC_TIMEOUT; // 两个时钟都不起作用
                }
            }
        }

        RTC_WaitForSynchro();
        RTC_ClearFlag(RTC_FLAG_RSF);
        LOG_I("[RTC] Init Success!(Already Init)");
        return BSP_RTC_ALREADY_INIT;
    }
}

BSP_RTC_Status_e BSP_RTC_SetTime(uint8_t hour, uint8_t min, uint8_t sec)
{
    RTC_TimeTypeDef RTC_TimeStructure;
    RTC_TimeStructure.RTC_Hours   = hour;
    RTC_TimeStructure.RTC_Minutes = min;
    RTC_TimeStructure.RTC_Seconds = sec;
    RTC_TimeStructure.RTC_H12     = RTC_H12_AM;

    // RTC_Format_BIN: 使用正常的十进制，而非 0x10
    if (RTC_SetTime(RTC_Format_BIN, &RTC_TimeStructure) == ERROR)
    {
        LOG_E("[RTC] Set Time Failed!");
        return BSP_RTC_ERROR;
    }
    else
    {
        LOG_D("[RTC] Set Time to %02d:%02d:%02d", hour, min, sec);
        return BSP_RTC_OK;
    }
}

BSP_RTC_Status_e BSP_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t date)
{
    RTC_DateTypeDef RTC_DateStructure;

    // 防止上层传入大于 2000 的数
    if (year >= 2000)
    {
        year -= 2000;
    }

    // RTC 的 year：0~99
    uint8_t week = BSP_RTC_Cal_Week(year + 2000, mon, date);

    RTC_DateStructure.RTC_Year    = year;
    RTC_DateStructure.RTC_Month   = mon;
    RTC_DateStructure.RTC_Date    = date;
    RTC_DateStructure.RTC_WeekDay = week;
    if (RTC_SetDate(RTC_Format_BIN, &RTC_DateStructure) == ERROR)
    {
        LOG_E("[RTC] Set Date Failed!");
        return BSP_RTC_ERROR;
    }
    else
    {
        LOG_D("[RTC] Set Date to 20%02d-%02d-%02d", year, mon, date);
        return BSP_RTC_OK;
    }
}

void BSP_RTC_GetCalendar(BSP_RTC_Calendar_t* calendar)
{
    RTC_TimeTypeDef RTC_TimeStructure;
    RTC_DateTypeDef RTC_DateStructure;

    // 获取当前系统里的时间
    // RTC_GetTime()硬件自动锁住了 Date (DR)
    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
    // 必须紧跟着 RTC_GetDate()
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);

    // 赋值
    // STM32 年份是从 0 开始计的，通常我们约定 0 代表 2000 年
    calendar->year  = RTC_DateStructure.RTC_Year + 2000;
    calendar->month = RTC_DateStructure.RTC_Month;
    calendar->date  = RTC_DateStructure.RTC_Date;

    calendar->week = RTC_DateStructure.RTC_WeekDay;

    calendar->hour = RTC_TimeStructure.RTC_Hours;
    calendar->min  = RTC_TimeStructure.RTC_Minutes;
    calendar->sec  = RTC_TimeStructure.RTC_Seconds;

    // 调试当前时间
    LOG_D("Current Calendar: %4d-%02d-%02d,%02d:%02d:%02d %s",
          calendar->year,
          calendar->month,
          calendar->date,
          calendar->hour,
          calendar->min,
          calendar->sec,
          WEEK_STR[calendar->week]);
}