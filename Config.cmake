# =======================================================
# Config.cmake - 项目核心配置（改这里就够了，别的文件一动都别动！）
# =======================================================

# 芯片型号
set(CHIP_MODEL "STM32F407ZGTX")

# 编译优化等级
set(OPTIMIZATION_FLAGS "-Os")    # 量产用 -Os，调试改 -O0 就行

# 链接脚本（绝对路径，稳如老狗）
set(LINKER_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/Core/linker/STM32F407ZGTX_FLASH.ld")

# ====================== 关键：外部晶振频率（单位：Hz）======================
# 野火/正点原子霸道/战舰：8MHz
# 正点原子阿波罗、部分新板子：25MHz
# 只改这一行就行！改完整个工程时钟全自动对！
set(HSE_VALUE 8000000 CACHE STRING "外部晶振频率，常见 8000000 或 25000000")

# 强制把 HSE_VALUE 塞进编译宏（这一行必须有！）
add_compile_definitions(HSE_VALUE=${HSE_VALUE})