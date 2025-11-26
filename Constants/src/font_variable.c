#include "font_variable.h"
#include <stddef.h> // for sizeof

/* ==================================================================
 * 变量实体定义 (Definitions)
 * ================================================================== */

/**
 * @brief 全局 16 点阵字体配置对象实例
 */
font_info_t font_16 = {
    // --- ASCII 部分 ---
    .ascii_w   = 8,
    .ascii_h   = 16,
    .ascii_map = ASCII_8x16,

    // --- 汉字 部分 ---
    .cn_w      = 16,
    .cn_h      = 16,
    .hzk_table = (const void*) HZK_16,

    // 这里的 count 最好是用 sizeof 动态算，或者写个宏
    .hzk_count = 24,

    // --- 寻址参数 ---
    .hzk_struct_size = sizeof(HZK_16_t),
    .hzk_data_offset = 2, // index 占 2 字节
    .hzk_data_size   = 32};