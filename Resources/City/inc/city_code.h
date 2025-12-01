/**
 * @file city_code.h
 * @brief 城市代码数据库 (自动生成)
 * @note  基于二分查找,时间复杂度 O(log n)
 */
#ifndef __CITY_CODE_H__
#define __CITY_CODE_H__

#include <stdint.h>

typedef struct {
    const char* name;  // 城市中文名 (Key)
    const char* id;    // 城市ID (Value)
} City_Info_t;

/**
 * @brief  查找城市ID (二分查找)
 * @param  name: 城市中文名 (如 "南京")
 * @return 城市ID字符串,未找到返回 NULL
 */
const char* City_Get_Code(const char* name);

#endif
