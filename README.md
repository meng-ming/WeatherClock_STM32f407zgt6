[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
# WeatherClock (STM32F4 + FreeRTOS + CMake)

> **一个基于现代构建系统与商业级架构设计的嵌入式天气时钟项目。**
>
> *采用 STM32F407 + FreeRTOS + CMake + Ninja 架构，集成了网络校时、温湿度监测与高性能 UI 渲染引擎。*

---

## 项目架构概述

本项目摒弃了传统的 IDE 开发模式，采用**“驱动层 - 中间层 - 应用层”**严格解耦的分层架构，实现了从底层硬件到上层业务的完全隔离。

### 核心技术栈 (Tech Stack)

* **构建系统**: CMake + Ninja (秒级增量编译，支持跨平台)。
* **操作系统**: FreeRTOS V10.6.2 (抢占式内核，任务/队列/互斥锁/事件组)。
* **内存管理**: **CCM RAM 深度优化**。将 OS 堆栈强制分配至 CCM (64KB)，释放 SRAM (112KB) 供 DMA 专用，彻底解决总线冲突。
* **硬件驱动**:
  * **通信层**: UART (DMA+IDLE RingBuffer) / Soft I2C / SPI (DMA)。
  * **外设层**: ESP32 (AT Command) / AHT20 / ST7789。
* **调试机制**:
  * 集成 `-u _printf_float` 支持浮点格式化。
  * 递归互斥锁保护日志与 UI 资源。

---
## 硬件配置 (Hardware Configuration)

本项目基于以下核心硬件平台开发与验证：

| 模块分类 | 核心型号 | 规格参数/备注 |
| :--- | :--- | :--- |
| **主控芯片** | **STM32F407ZGT6** | Cortex-M4F, 168MHz, 1MB Flash, 192KB SRAM |
| **温湿度传感器** | **AHT20** | I2C 接口 (模拟), 工业级高精度温湿度监测 |
| **Wi-Fi 模组** | **ESP32-C3 Super Mini** | UART 接口 (AT 指令集), 2.4GHz Wi-Fi, 体积小巧 |
| **显示屏** | **2.4寸 TFT LCD** | **ST7789** 驱动芯片, **240x320** 点阵分辨率, SPI 接口 |
---

## 开发环境要求 (Prerequisites)

请务必安装以下工具链，以确保构建系统正常工作。

| 工具                  | 版本要求              | 下载地址                                                                 |
|-----------------------|-----------------------|--------------------------------------------------------------------------|
| **arm-gnu-toolchain** | 13.2.Rel1（强烈推荐） | [点击下载](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
| **OpenOCD**           | xpack-openocd ≥ 0.12.0 | [点击下载](https://github.com/xpack-dev-tools/openocd-xpack/releases)   |
| **Ninja**             | ≥ 1.11                | [点击下载](https://github.com/ninja-build/ninja/releases)                |
| **Clangd**            | 17+（推荐）           | [点击下载](https://github.com/clangd/clangd/releases)                    |
| **VS Code 插件**      | CMake Tools、clangd、Cortex-Debug、clang-format(Xaver Hellauer) | VS Code Marketplace                                                      |
| **clang-format.exe**  | 将 bin/.clang-format.exe 放到指定目录下，并在 settings.json 中设置 | [点击下载](https://github.com/llvm/llvm-project/releases/)               |

---

## 重要注意事项 (Configuration)

| 序号 | 注意事项               | 解决方案                                                                 |
|------|------------------------|--------------------------------------------------------------------------|
| **1** | **BOOT0 引脚状态**     | 必须接 **GND (低电平)**，否则进入 Bootloader 无法运行用户程序。          |
| **2** | **OpenOCD 驱动报错**   | 遇到 `LIBUSB_ERROR_ACCESS` 时，请使用 **Zadig** 将 ST-Link 驱动更换为 **WinUSB**。<br>下载地址：[https://zadig.akeo.ie](https://zadig.akeo.ie) |
| **3** | **中文乱码问题**       | 请确保所有源码文件为 **UTF-8** 编码。全局配置命令：<br>`git config --global i18n.commitEncoding utf-8` |
| **4** | **代码补全失效**       | 请在 `.vscode/settings.json` 中禁用 C/C++ 插件，仅启用 **clangd**。     |

---

## 快速开始 (Quick Start)

### 1. 编译工程
推荐使用 VS Code 的 **CMake Tools** 插件底部状态栏按钮进行构建，或者使用终端命令：

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 生成构建配置 (Ninja 生成器 + Debug 模式)
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..

# 3. 开始编译
ninja
```

注意: 如果遇到浮点数打印为空 (如 temp=)，请确认 CmakeLists.txt 中已包含 -u _printf_float 链接选项。

### 2. 下载与调试
* 一键下载: 运行 Task 中的 Flash Device 或命令行 make flash (需自定义 Target)。
* 在线调试: 按下 F5 启动 Cortex-Debug，自动连接 OpenOCD 并停在 main() 入口。
---

## 目录结构说明 (Detailed Structure)

```
WeatherClock_STM32f407zgt6/
├── CmakeLists.txt              # 主构建脚本 (项目入口)
├── Config.cmake                # 核心编译选项 (FPU/DEBUG/HSE)
├── arm-gcc.cmake               # 交叉编译工具链定义
├── Core/                       # 芯片级启动文件
│   ├── linker/                 # 链接脚本 (.ld) - 定义 CCM/SRAM 内存布局
│   └── startup/                # 启动汇编 (.s)
├── Drivers/                    # 硬件驱动层 (HAL/BSP)
│   ├── BSP/                    # 板级支持包 (Board Support Package)
│   │   ├── AHT20/              # [I2C] 温湿度传感器驱动 (Soft I2C)
│   │   ├── ESP32C3/            # [UART] Wi-Fi 模组 AT 指令封装
│   │   ├── ST7789/             # [SPI] LCD 屏幕驱动 (DMA 优化)
│   │   ├── RTC/                # [RTC] 实时时钟管理 (LSI/LSE 自动切换)
│   │   ├── IWDG/               # [WDT] 独立看门狗
│   │   └── USART/              # [UART] 串口底层驱动 (DMA RingBuffer)
│   ├── SPL_F4/                 # STM32F4 标准外设库 (StdPeriph Lib)
│   └── CMSIS/                  # Cortex-M 核心库
├── Middleware/                 # 中间件层
│   ├── FreeRTOS/               # 实时操作系统内核 (Kernel + Port)
│   └── cJSON/                  # 轻量级 JSON 解析器 (用于天气数据解析)
├── Resources/                  # 静态资源数据
│   ├── Font/                   # 屏幕字库 (ASCII + GBK + Weather Icons)
│   ├── Image/                  # UI 图片资源 (C Arrays)
│   └── City/                   # 城市代码数据库 (Binary Search 优化)
└── User/                       # 用户应用层 (Application Layer)
    ├── inc/                    # 全局头文件 (main.h, FreeRTOSConfig.h)
    ├── src/                    # 全局源文件 (main.c - 堆内存定义, app_task.c - 任务调度)
    └── APP/                    # 核心业务模块
        ├── Weather/            # 天气业务 (状态机, 网络请求, 数据解析)
        ├── UI/                 # UI 逻辑 (界面布局, 局部刷新, 互斥锁保护)
        └── Calendar/           # 日历业务 (时间同步, 传感器定时读取)
```
---

### 各个模块配套资料下载地址

**网盘链接**：https://pan.baidu.com/s/1Xt1_zTLxCBrZN7gFr50a6Q?pwd=raai
**提取码**：raai
**分享有效期**：永久有效（百度网盘超级会员V1账号持续维护）

---

## 许可证
本项目基于 MIT License 开源。
使用的第三方库 (FreeRTOS, cJSON, SPL) 遵循其原有的许可证协议。