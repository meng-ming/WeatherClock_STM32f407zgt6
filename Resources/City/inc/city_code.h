/**
 * @file    city_code.h
 * @brief   城市代码数据库 (自动生成)
 * @note    基于二分查找,时间复杂度 O(log n)
 * @author  meng-ming
 * @version 1.0
 * @date    2025-12-07
 */

#ifndef __CITY_CODE_H__
#define __CITY_CODE_H__

#include <stdint.h>

/* ==================================================================
 * 1. 类型定义
 * ================================================================== */

/**
 * @brief 城市信息结构体
 * @note  用于存储城市名称和对应 ID
 */
typedef struct
{
    const char* name; ///< 城市中文名 (Key)
    const char* id;   ///< 城市ID (Value)
} City_Info_t;

/* ==================================================================
 * 2. 函数接口
 * ================================================================== */

/**
 * @brief  查找城市ID (二分查找)
 * @note   在预排序的城市数据库中快速查找
 * @param  name: 城市中文名 (如 "南京")
 * @retval 城市ID字符串,未找到返回 NULL
 */
const char* City_Get_Code(const char* name);

#endif /* __CITY_CODE_H__ */