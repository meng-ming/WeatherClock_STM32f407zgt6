#include "weather_parser.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

uint8_t Weather_Parser_Execute(const char* json_str, APP_Weather_Data_t* out_data)
{
    if (json_str == NULL || out_data == NULL)
        return 0;

    // 1. 解析 JSON
    cJSON* root = cJSON_Parse(json_str);
    if (!root)
        return 0;

    uint8_t ret = 0;
    memset(out_data, 0, sizeof(APP_Weather_Data_t));

    // 2. 直接提取根节点下的字段
    cJSON* city = cJSON_GetObjectItem(root, "city");
    cJSON* wea  = cJSON_GetObjectItem(root, "wea");
    cJSON* tem  = cJSON_GetObjectItem(root, "tem");

    // 只要有核心数据，就算成功
    if (city && wea && tem)
    {
        // === 基础信息 ===
        if (city->valuestring)
            snprintf(out_data->city, sizeof(out_data->city), "%s", city->valuestring);
        if (wea->valuestring)
            snprintf(out_data->weather, sizeof(out_data->weather), "%s", wea->valuestring);
        if (tem->valuestring)
            snprintf(out_data->temp, sizeof(out_data->temp), "%s℃", tem->valuestring);

        // === 更新时间 ===
        // API 返回 "update_time":"20:17"
        cJSON* upd = cJSON_GetObjectItem(root, "update_time");
        if (upd && upd->valuestring)
        {
            snprintf(out_data->update_time, sizeof(out_data->update_time), "%s", upd->valuestring);
        }

        // === 扩展信息 (温差) ===
        // API 返回 tem_day="17", tem_night="6" -> 拼成 "6~17℃"
        cJSON* t_day   = cJSON_GetObjectItem(root, "tem_day");
        cJSON* t_night = cJSON_GetObjectItem(root, "tem_night");
        if (t_day && t_night)
        {
            snprintf(out_data->temp_range,
                     sizeof(out_data->temp_range),
                     "%s~%s℃",
                     t_night->valuestring,
                     t_day->valuestring);
        }

        // === 扩展信息 (风向风力) ===
        // API 返回 win="东南风", win_speed="2级" -> 拼成 "东南风 2级"
        cJSON* win     = cJSON_GetObjectItem(root, "win");
        cJSON* win_spd = cJSON_GetObjectItem(root, "win_speed");
        if (win && win_spd)
        {
            snprintf(out_data->wind,
                     sizeof(out_data->wind),
                     "%s %s",
                     win->valuestring,
                     win_spd->valuestring);
        }

        // === 扩展信息 (空气质量) ===
        cJSON* air = cJSON_GetObjectItem(root, "air");
        if (air && air->valuestring)
        {
            snprintf(out_data->air, sizeof(out_data->air), "%s", air->valuestring);
        }

        // === 扩展信息 (湿度) ===
        cJSON* hum = cJSON_GetObjectItem(root, "humidity");
        if (hum && hum->valuestring)
        {
            snprintf(out_data->humidity, sizeof(out_data->humidity), "%s", hum->valuestring);
        }

        // === 扩展信息 (气压) ===
        cJSON* press = cJSON_GetObjectItem(root, "pressure");
        if (press && press->valuestring)
        {
            snprintf(out_data->pressure, sizeof(out_data->pressure), "%s", press->valuestring);
        }

        ret = 1;
    }

    cJSON_Delete(root);
    return ret;
}