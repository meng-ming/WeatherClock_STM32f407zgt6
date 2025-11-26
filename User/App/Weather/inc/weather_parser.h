/**
 * @file    weather_parser.h
 * @brief   天气数据解析模块接口
 */

#ifndef __WEATHER_PARSER_H
#define __WEATHER_PARSER_H

#include "app_data.h" // 复用数据结构
#include <stdint.h>

/**
 * @brief  解析心知天气 JSON 数据
 * @note   这是一个纯函数，不依赖任何硬件状态
 * @param  json_str: 原始 JSON 字符串 (输入)
 * @param  out_data: 解析后的数据结构体指针 (输出)
 * @retval 1: 解析成功
 * @retval 0: 解析失败 (JSON格式错误或字段缺失)
 */
uint8_t Weather_Parser_Execute(const char* json_str, App_Weather_Data_t* out_data);

#endif