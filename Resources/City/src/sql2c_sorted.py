#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@file sql2c_clean.py
@brief SQL转C城市数据库生成工具
@note 仅生成 city_code.c/.h,无测试代码
@note 需要将 cityid.sql 与当前程序放在同一文件夹下
"""

import re
import os
import sys
import argparse
import logging

# 配置默认
DEFAULT_INPUT = 'cityid.sql'
DEFAULT_OUTPUT_PREFIX = 'city_code'

def setup_logging():
    logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(message)s')
    return logging.getLogger(__name__)

def parse_data(input_file):
    """
    @brief 解析SQL提取城市数据
    """
    logger = logging.getLogger(__name__)
    city_data = []

    if not os.path.exists(input_file):
        logger.error(f"找不到文件: {input_file}")
        return []

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()

        # 匹配模式：('CN数字', '英文', '中文', ...)
        pattern = r"\('CN(\d+)',\s*'[^']+',\s*'([^']+)'"
        matches = re.findall(pattern, content)

        if not matches:
            logger.warning("未匹配到数据,请检查 SQL 格式")
            return []

        for city_id, city_zh in matches:
            city_data.append((city_id, city_zh))

    except IOError as e:
        logger.error(f"读文件失败: {e}")
        return []

    # 去重 + 排序 (关键！二分查找必须基于排序)
    unique_data = {}
    for id_num, zh in city_data:
        unique_data[zh] = id_num

    # 按中文名称排序
    sorted_data = sorted(unique_data.items(), key=lambda x: x[0])

    logger.info(f"解析完成: 原始 {len(city_data)} -> 去重后 {len(sorted_data)}")
    return sorted_data

def gen_header(output_prefix):
    header_file = output_prefix + '.h'
    guard = output_prefix.upper() + '_H'

    with open(header_file, 'w', encoding='utf-8') as f:
        f.write(f'''/**
 * @file {header_file}
 * @brief 城市代码数据库 (自动生成)
 * @note  基于二分查找,时间复杂度 O(log n)
 */
#ifndef __{guard}__
#define __{guard}__

#include <stdint.h>

typedef struct {{
    const char* name;  // 城市中文名 (Key)
    const char* id;    // 城市ID (Value)
}} City_Info_t;

/**
 * @brief  查找城市ID (二分查找)
 * @param  name: 城市中文名 (如 "南京")
 * @return 城市ID字符串,未找到返回 NULL
 */
const char* City_Get_Code(const char* name);

#endif
''')
    return header_file

def gen_source(city_data, output_prefix):
    source_file = output_prefix + '.c'

    with open(source_file, 'w', encoding='utf-8') as f:
        f.write(f'''/**
 * @file {source_file}
 * @brief 城市数据库源文件
 */
#include "{output_prefix}.h"
#include <string.h>

/* 有序城市数组 (Flash存储) */
static const City_Info_t g_city_db[] = {{
''')
        # 写入数据
        for zh, id_num in city_data:
            f.write(f'    {{"{zh}", "{id_num}"}},\n')

        f.write('    {0, 0} // 哨兵\n};\n\n')

        # 写入二分查找算法
        f.write(f'''
// 数组元素个数 (不含哨兵)
#define CITY_DB_SIZE  (sizeof(g_city_db) / sizeof(g_city_db[0]) - 1)

static int binary_search(const char* name)
{{
    int low = 0;
    int high = CITY_DB_SIZE - 1;

    while (low <= high)
    {{
        int mid = (low + high) / 2;
        int cmp = strcmp(g_city_db[mid].name, name);

        if (cmp == 0)
            return mid; // 找到
        else if (cmp < 0)
            low = mid + 1; // 在右半区
        else
            high = mid - 1; // 在左半区
    }}
    return -1; // 未找到
}}

const char* City_Get_Code(const char* name)
{{
    if (!name) return NULL;
    int idx = binary_search(name);
    if (idx >= 0) {{
        return g_city_db[idx].id;
    }}
    return NULL;
}}
''')
    return source_file

def main():
    parser = argparse.ArgumentParser(description="SQL -> C 转换工具")
    parser.add_argument('-i', '--input', default=DEFAULT_INPUT, help='输入SQL文件')
    parser.add_argument('-o', '--output', default=DEFAULT_OUTPUT_PREFIX, help='输出文件前缀')
    args = parser.parse_args()

    logger = setup_logging()

    # 1. 解析数据
    city_data = parse_data(args.input)
    if not city_data:
        return

    # 2. 生成文件
    gen_header(args.output)
    gen_source(city_data, args.output) # 这里不再需要 add_main_test 参数了

    logger.info(f"生成成功: {args.output}.c / .h")
    logger.info(f"数据量: {len(city_data)} 条, Flash占用: ~{len(city_data)*20/1024:.2f} KB")

if __name__ == '__main__':
    main()