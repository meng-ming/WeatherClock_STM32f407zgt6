A. 应用层 (APPlication Layer)
main.c (系统入口)

职能：负责硬件初始化（BSP）、业务初始化（APP）和超级循环（Super Loop）调度。

特点：作为“总指挥”，只负责调度，不处理具体业务细节。

APP_weather.c (天气业务核心)

职能：天气业务的“大脑”。维护一个非阻塞状态机 (FSM)，负责管理 WiFi 连接、TCP 链路、发送 HTTP 请求、处理超时重试。

设计模式：采用依赖注入机制，通过回调函数 (Weather_DataCallback_t) 将数据推送到 UI 层，不直接依赖 UI 代码。

APP_ui.c / ui_main_page.c (界面显示)

职能：

APP_ui.c：UI 控制器，对外提供统一接口（如 APP_UI_Update）。

ui_main_page.c：具体的“绘图员”，负责计算坐标、绘制背景框、图标和文字。

特点：采用局部刷新策略，只重绘变动的数据区域，提升刷屏效率。

B. 中间件/逻辑层 (Middleware/Logic)
weather_parser.c (数据解析)

职能：纯逻辑模块。接收原始 JSON 字符串，利用 cJSON 库提取关键字段（城市、温度、天气），并清洗存入标准结构体。

特点：防御性编程，也就是不管网络发来什么垃圾数据，这个模块保证不会让程序崩溃（判空、防溢出）。

cJSON (第三方库)

职能：轻量级 JSON 解析器，用于处理心知天气返回的数据格式。

C. 硬件驱动层 (BSP/Drivers)
esp32_module.c (网络模组驱动)

职能：负责 STM32 与 ESP32 之间的 UART 通信。封装了复杂的 AT 指令交互，对外提供简单的 Connect、Send 接口。

特点：内置了健壮性处理（Busy 等待、错误重试），这是商业级代码的关键。

st7789.c (屏幕驱动)

职能：配置 STM32 的 SPI 接口，驱动 TFT 屏幕。实现了底层的画点、画矩形和显存窗口设置。

uart_driver.c (串口驱动)

职能：管理 STM32 的硬件串口，实现了环形缓冲区 (Ring Buffer)，解决高速数据收发时的数据包丢失或粘包问题。