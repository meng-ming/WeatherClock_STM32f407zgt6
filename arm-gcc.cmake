# =======================================================
# arm-gcc.cmake - 工具链文件（再也不写死 D:/ 了！）
# =======================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 方案 A：最狠最干净，直接用 PATH 里的工具链（推荐！）
find_program(CMAKE_C_COMPILER   NAMES arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES arm-none-eabi-g++ REQUIRED)
find_program(CMAKE_ASM_COMPILER NAMES arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_OBJCOPY      NAMES arm-none-eabi-objcopy REQUIRED)
find_program(CMAKE_SIZE         NAMES arm-none-eabi-size REQUIRED)

# 如果你非要保留手动指定路径的权利，用方案 B（下面被注释的部分）
# set(TOOLCHAIN_DIR "$ENV{ARM_TOOLCHAIN_PATH}" CACHE PATH "arm-gnu-toolchain bin directory")
# if(NOT TOOLCHAIN_DIR)
#     message(FATAL_ERROR "请设置环境变量 ARM_TOOLCHAIN_PATH 或加 -DTOOLCHAIN_DIR=xxx")
# endif()
# get_filename_component(TOOLCHAIN_DIR_ABS "${TOOLCHAIN_DIR}" ABSOLUTE)
# set(CMAKE_C_COMPILER   "${TOOLCHAIN_DIR_ABS}/arm-none-eabi-gcc${CMAKE_EXECUTABLE_SUFFIX}")

# 裸机必备
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_WORKS   TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_ASM_COMPILER_WORKS TRUE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE  ONLY)