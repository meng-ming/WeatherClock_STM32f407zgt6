/**
 * @file  bsp_rtc.c
 * @brief
 * RTC板级支持包。处理LSI/LSE自动切换与校准、时间日期设置/获取、周计算。确保断电重启时钟源稳定，优先LSE
 * @author  meng-ming
 *
 * Copyright (c) 2025 meng-ming
 * SPDX-License-Identifier: MIT
 */

#include "bsp_rtc.h"
#include "stm32f4xx.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_rcc.h"
#include <stdint.h>
#include "stm32f4xx_rtc.h"
#include "sys_log.h"
#include <inttypes.h> // 引入头文件，提供 PRIu32 宏

#define FIRST_BKP_REGISTER 0xA0A5

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

/**
 * @brief  测量 LSI 的实际频率
 * @note   利用 TIM5 CH4 的输入捕获功能测量 LSI 周期
 * 前提：系统主时钟已经配置好 (SystemCoreClock)
 * @retval LSI 实际频率 (Hz)，如果测量失败返回默认 32000
 */
static uint32_t BSP_RTC_Get_LSI_Freq(void)
{
    uint32_t lsi_freq      = 32000;
    uint32_t capture_val_1 = 0, capture_val_2 = 0;
    uint32_t pclk1_freq = 0;
    uint32_t timeout    = 0;

    // 1. 开启 TIM5 时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    // 2. 开启 LSI (如果还没开)
    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
        if (++timeout > 0x3FFFF)
            return 32000; // 超时返回默认值
    }

    // 3. 关键配置：将 LSI 连接到 TIM5_CH4 输入
    // STM32F407 参考手册：TIM5_OR 寄存器的 TI4_RMP [7:6] 位
    // 01: LSI internal clock is connected to TIM5_CH4 input
    TIM5->OR |= (0x01 << 6);

    // 4. 配置 TIM5 基础单元
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period        = 0xFFFFFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler     = 0; // 不分频，精度最高
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

    // 5. 配置输入捕获 (IC4)
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_ICInitStructure.TIM_Channel     = TIM_Channel_4;
    TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising; // 上升沿捕获
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV8; // 8分频，减少抖动误差
    TIM_ICInitStructure.TIM_ICFilter    = 0;
    TIM_ICInit(TIM5, &TIM_ICInitStructure);

    // 6. 启动定时器
    TIM_Cmd(TIM5, ENABLE);

    // 7. 开始测量
    // 等待第一次捕获
    TIM_ClearFlag(TIM5, TIM_FLAG_CC4);
    timeout = 0;
    while (TIM_GetFlagStatus(TIM5, TIM_FLAG_CC4) == RESET)
    {
        if (++timeout > 0xFFFFF)
            goto end;
    }
    capture_val_1 = TIM_GetCapture4(TIM5);

    // 等待第二次捕获
    TIM_ClearFlag(TIM5, TIM_FLAG_CC4);
    timeout = 0;
    while (TIM_GetFlagStatus(TIM5, TIM_FLAG_CC4) == RESET)
    {
        if (++timeout > 0xFFFFF)
            goto end;
    }
    capture_val_2 = TIM_GetCapture4(TIM5);

    // 8. 计算频率
    // 计数差值 * IC分频(8) = LSI 经过的时钟周期数
    // 计数器时钟 = PCLK1 * 2 (如果 APB1 分频!=1)

    // 获取 PCLK1 频率
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);
    pclk1_freq = RCC_Clocks.PCLK1_Frequency;

    // F407 如果 APB1 分频不为 1，定时器时钟是 PCLK1 的 2 倍
    uint32_t tim_clk = (RCC_Clocks.HCLK_Frequency != pclk1_freq) ? (pclk1_freq * 2) : pclk1_freq;

    // 防止溢出回绕计算
    uint32_t diff = 0;
    if (capture_val_2 >= capture_val_1)
        diff = capture_val_2 - capture_val_1;
    else
        diff = (0xFFFFFFFF - capture_val_1) + capture_val_2 + 1;

    // 频率公式：LSI_Freq = TIM_Clk / (Diff / IC_Prescaler)
    // 即：LSI_Freq = (TIM_Clk * 8) / Diff
    if (diff != 0)
    {
        lsi_freq = (tim_clk * 8) / diff;
    }

    LOG_I("[RTC] Measured LSI Freq: %" PRIu32 "Hz", lsi_freq);

end:
    // 9. 关闭 TIM5 省电
    TIM_Cmd(TIM5, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);

    return lsi_freq;
}

/**
 * @brief  LSI 自动校准并配置 RTC 分频
 * @note   无论是初次初始化还是重启，只要用 LSI，都调这个来确保精度
 */
static void BSP_RTC_Config_LSI_AutoCalib(void)
{
    LOG_W("[RTC] Performing LSI Auto-Calibration...");

    // 1. 确保 LSI 开启并稳定
    RCC_LSICmd(ENABLE);
    uint32_t wait_tick = 0;
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
        if (++wait_tick > 0x3FFFF)
        {
            LOG_E("[RTC] LSI Start Failed!");
            return;
        }
    }

    // 2. 实时测量当前 LSI 频率
    uint32_t real_lsi_freq = BSP_RTC_Get_LSI_Freq();

    // 3. 计算分频系数
    // Freq = RTCCLK / ((Asynch + 1) * (Synch + 1))
    // 目标 1Hz。固定 Asynch = 127
    // Synch = (Freq / 128) - 1
    uint32_t asynch_div = 127;
    uint32_t synch_div  = (real_lsi_freq / (asynch_div + 1)) - 1;

    // 4. 配置 RTC 分频寄存器 (PRER)
    // 注意：修改 PRER 需要进入 Init 模式，这会暂停日历一瞬间，但不会重置时间
    RTC_InitTypeDef RTC_InitStructure;
    RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;
    RTC_InitStructure.RTC_AsynchPrediv = asynch_div;
    RTC_InitStructure.RTC_SynchPrediv  = synch_div;

    // 确保选源 (虽然可能已经选了，再选一次保险)
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    RCC_RTCCLKCmd(ENABLE);

    RTC_WaitForSynchro(); // 等待同步

    // 调用库函数更新分频系数
    if (RTC_Init(&RTC_InitStructure) == ERROR)
    {
        LOG_E("[RTC] LSI Calibration Update Failed!");
    }
    else
    {
        LOG_I("[RTC] LSI Calibrated! Freq=%" PRIu32 "Hz, SynchDiv=%" PRIu32 "",
              real_lsi_freq,
              synch_div);
    }
}

uint8_t BSP_RTC_Is_Time_Invalid(void)
{
    BSP_RTC_Calendar_t now;
    BSP_RTC_GetCalendar(&now);

    // 如果年份是 2000年，说明时间还没校准过
    if (now.year == 2000)
        return 1;
    else
        return 0;
}

BSP_RTC_Status_e BSP_RTC_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    // ==========================================
    // 场景 A: 第一次初始化 (BKP 为空)
    // ==========================================
    if (RTC_ReadBackupRegister(RTC_BKP_DR0) != FIRST_BKP_REGISTER)
    {
        RCC_BackupResetCmd(ENABLE);
        RCC_BackupResetCmd(DISABLE);

        RCC_LSEConfig(RCC_LSE_ON);
        LOG_I("[RTC] First Init! Trying LSE...");

        // 等待 LSE
        uint32_t wait_lse_tick = 0;
        while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
        {
            wait_lse_tick++;
            if (wait_lse_tick > 0xFFFFF)
                break; // 超时
        }

        if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == SET)
        {
            // LSE 成功
            RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
            RCC_RTCCLKCmd(ENABLE);
            RTC_WaitForSynchro();

            RTC_InitTypeDef RTC_InitStructure;
            RTC_InitStructure.RTC_AsynchPrediv = 0x7F;
            RTC_InitStructure.RTC_SynchPrediv  = 0xFF;
            RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;
            RTC_Init(&RTC_InitStructure);

            LOG_I("[RTC] LSE Init Success!");
        }
        else
        {
            // LSE 失败 -> 转 LSI 自动校准
            BSP_RTC_Config_LSI_AutoCalib();
        }

        // 设置默认时间
        BSP_RTC_SetTime(12, 0, 0);
        BSP_RTC_SetDate(0, 1, 1);

        RTC_WriteBackupRegister(RTC_BKP_DR0, FIRST_BKP_REGISTER);
        return BSP_RTC_OK;
    }
    // ==========================================
    // 场景 B: 已初始化 (断电重启/复位)
    // ==========================================
    else
    {
        // 检查当前 RTC 时钟源
        // BDCR 寄存器 [9:8] 位: 01=LSE, 10=LSI
        uint32_t clock_source = (RCC->BDCR >> 8) & 0x03;

        if (clock_source == 0x02) // 0x02 代表 LSI
        {
            LOG_I("[RTC] RTC is using LSI. Recalibrating...");

            // 即使是重启，只要发现用的是 LSI，就强制重新校准一次！
            // 这样能消除温漂和电压波动带来的频率误差
            BSP_RTC_Config_LSI_AutoCalib();
        }
        else if (clock_source == 0x01) // 0x01 代表 LSE
        {
            // 检查 LSE 是否还在工作
            // 这里给一点延时尝试，防止刚上电 LSE 还没稳
            uint32_t check_lse = 0;
            while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
            {
                check_lse++;
                if (check_lse > 0x5FFF)
                    break; // 稍微给点时间检测
            }

            if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
            {
                // ========================================================
                // 严重故障：系统配置了 LSE，但 LSE 彻底挂了
                // 策略：断臂求生。强制复位备份域，切换到 LSI。
                // ========================================================
                LOG_E("[RTC] LSE Dead! Performing Backup Domain Reset...");

                // 1. 复位备份域 (注意：这会清除当前时间！)
                RCC_BackupResetCmd(ENABLE);
                RCC_BackupResetCmd(DISABLE);

                // 2. 重新启用 PWR 和 BKP 访问 (复位后可能需要重新开，保险起见)
                PWR_BackupAccessCmd(ENABLE);

                // 3. 此时系统回到了“一穷二白”的状态，像第一次上电一样
                // 直接调用封装好的 LSI 自动校准函数
                BSP_RTC_Config_LSI_AutoCalib();

                // 4. 因为时间丢了，必须重置一个默认安全时间
                // 将年份设为 0 (即 2000年)，作为“时间失效”的特殊标记
                BSP_RTC_SetTime(12, 0, 0);
                BSP_RTC_SetDate(0, 1, 1); // 2000-01-01

                // 5. 标记初始化完成
                RTC_WriteBackupRegister(RTC_BKP_DR0, FIRST_BKP_REGISTER);

                LOG_W("[RTC] Recovered using LSI. Time Reset to 2000-01-01 (Invalid)!");
            }
            else
            {
                LOG_I("[RTC] LSE is Running Normal.");
                // LSE 正常，只需要等待同步
                RTC_WaitForSynchro();
                RTC_ClearFlag(RTC_FLAG_RSF);
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
        // LOG_D("[RTC] Set Time to %02d:%02d:%02d", hour, min, sec);
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
        // LOG_D("[RTC] Set Date to 20%02d-%02d-%02d", year, mon, date);
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
    // LOG_D("Current Calendar: %4d-%02d-%02d,%02d:%02d:%02d %s",
    //       calendar->year,
    //       calendar->month,
    //       calendar->date,
    //       calendar->hour,
    //       calendar->min,
    //       calendar->sec,
    //       WEEK_STR[calendar->week]);
}