/**
 * @file    weather_parser.c
 * @brief   天气数据解析器实现
 * @note    这是一个纯逻辑模块，不依赖任何硬件，只依赖 cJSON 和 app_data 数据结构。
 */

#include "weather_parser.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h> // for snprintf

/**
 * @brief  执行解析
 * @param  json_str: 原始 JSON 字符串
 * @param  out_data: [输出] 解析结果存到这里
 * @retval 1: 成功, 0: 失败
 */
uint8_t Weather_Parser_Execute(const char* json_str, App_Weather_Data_t* out_data)
{
    // 1. 入口参数检查 (防御性编程)
    if (json_str == NULL || out_data == NULL)
        return 0;

    // 2. 解析 JSON 字符串
    cJSON* root = cJSON_Parse(json_str);
    if (!root)
        return 0;

    uint8_t ret = 0; // 默认为失败

    // 3. 解析前先清空输出结构体，防止上一轮的垃圾数据残留
    memset(out_data, 0, sizeof(App_Weather_Data_t));

    // 4. 开始剥洋葱：results -> [0] -> location/now
    cJSON* results = cJSON_GetObjectItem(root, "results");
    if (cJSON_IsArray(results))
    {
        cJSON* first = cJSON_GetArrayItem(results, 0);
        if (first)
        {
            cJSON* loc = cJSON_GetObjectItem(first, "location");
            cJSON* now = cJSON_GetObjectItem(first, "now");
            cJSON* upd = cJSON_GetObjectItem(first, "last_update");

            // 必须同时有 location 和 now 才算有效数据
            if (loc && now)
            {
                cJSON* name = cJSON_GetObjectItem(loc, "name");
                cJSON* text = cJSON_GetObjectItem(now, "text");
                cJSON* temp = cJSON_GetObjectItem(now, "temperature");

                // [安全优化] 使用 sizeof 确保长度安全，防止缓冲区溢出
                // [安全优化] strncpy 不保证自动封口，必须手动赋值 \0

                // 1. 提取城市名
                if (name && name->valuestring)
                {
                    strncpy(out_data->city, name->valuestring, sizeof(out_data->city) - 1);
                    out_data->city[sizeof(out_data->city) - 1] = '\0';
                }

                // 2. 提取天气现象
                if (text && text->valuestring)
                {
                    strncpy(out_data->weather, text->valuestring, sizeof(out_data->weather) - 1);
                    out_data->weather[sizeof(out_data->weather) - 1] = '\0';
                }

                // 3. 提取温度 (使用 snprintf 格式化并防止溢出)
                if (temp && temp->valuestring)
                {
                    snprintf(out_data->temp, sizeof(out_data->temp), "%s C", temp->valuestring);
                }

                // 4. 提取更新时间
                if (upd && upd->valuestring && strlen(upd->valuestring) > 16)
                {
                    // "2023-11-26T14:30:00..." -> 取 "14:30"
                    strncpy(out_data->update_time, upd->valuestring + 11, 5);
                    out_data->update_time[5] = '\0';
                }

                ret = 1; // 标记为成功
            }
        }
    }

    // 5. 极其重要：释放 JSON 对象内存
    cJSON_Delete(root);

    return ret;
}