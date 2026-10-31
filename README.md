# ESP32-S3-Smart-Desk-Terminal
# ESP32-S3 Smart Desk Terminal (智能桌面气象终端)

![Status](https://img.shields.io/badge/Build-Stable-success.svg)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)
![Framework](https://img.shields.io/badge/Framework-Arduino-orange.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

## 📖 项目概述 (Project Overview)

本项目是一个基于 **ESP32-S3** 微控制器与 **1.8 英寸 ST7735 SPI 显示屏** 构建的嵌入式智能桌面终端系统。

有别于传统的单片机教学级时钟，本项目在架构设计上遵循了**高内聚、低耦合、非阻塞**的工程原则。在实现高精度网络授时与实时气象展示的基础之上，本项目重点探讨了资源受限设备（MCU）在边缘计算场景下的应用，包括：大语言模型（LLM）的异步请求与 Token 消耗控制、非易失性存储（NVS）的断电状态保护，以及基于局域网 WebServer 的跨端交互方案。

## ✨ 核心技术特性 (Core Features)

*   **边缘智能交互 (Edge AI Integration)**：异步接入 DeepSeek 大语言模型 API，结合本地实时天气、节气与节假日数据，动态生成个性化的语义反馈。
*   **状态持久化与防抖机制 (Persistence & Debounce)**：采用 ESP32 `Preferences` 库实现毫秒级心跳与断电状态记忆；针对付费 API 设计了严格的时间戳防抖（Debounce）逻辑，防止设备重启导致的额度滥用。
*   **双缓冲 UI 渲染引擎 (Double-Buffered UI)**：底层基于 `TFT_eSprite` 技术，将所有的图形绘制（图元生成、文字排版）在内存中独立完成并一次性推流至屏幕，彻底解决 SPI 屏幕的局部刷新闪烁（Tearing）问题。
*   **异步网络服务架构 (Asynchronous WebServer)**：板载轻量级 WebServer，提供局域网控制面板。支持一键同步移动端时间戳、实时查看设备运行状态（心跳日志）。
*   **无缝配网方案 (Seamless Provisioning)**：集成 `WiFiManager`，首次开机自动开启 AP 热点并触发 Captive Portal（强制门户），实现无代码硬编码的 WiFi 凭证注入。
注意:如果正常连接完成 WiFi 时，请点击网页端的校准按钮，程序即可正常运行!
本设备支持2.4gWiFi
## 🏗️ 系统架构 (System Architecture)

```text
├── 核心控制层 (Core Layer)
│   ├── ESP32-S3 双核处理器 (240MHz)
│   └── FreeRTOS 非阻塞任务调度 (Loop / WebServer Handler)
├── 数据持久层 (Persistence Layer)
│   ├── NVS (Preferences) - 存储 AI 回复、心跳时间戳、防抖状态
│   └── RTC (System Time) - NTP 网络授时与本地时区转换
├── 交互展示层 (Presentation Layer)
│   ├── TFT_eSPI - 底层屏幕驱动 (SPI 总线)
│   └── U8g2 - 高精度中文字体渲染 (UTF-8 支持)
└── 网络通信层 (Network Layer)
    ├── HTTPClient / WiFiClientSecure - 外部 API 请求 (HTTPS)
    └── WebServer - 本地局域网 Web 控制台服务
硬件依赖清单 (Hardware Requirements)
组件	型号/规格	备注
核心主控	ESP32-S3 DevKit (N16R8 推荐)	需具备 PSRAM 与 16MB Flash
显示终端	1.8 寸 TFT LCD (ST7735 驱动)	分辨率 128*160，SPI 接口
连接线材	杜邦线若干	建议使用短距线材保证 SPI 信号完整性


快速部署 (Quick Start)
1. 环境准备：使用 Arduino IDE，安装 ESP32 开发板支持包，并选择 ESP32S3 Dev Module。
2. 依赖库安装：通过库管理器安装 TFT_eSPI、U8g2_for_TFT_eSPI、WiFiManager、ArduinoJson (v6)。
3. 源码配置：克隆本仓库，打开 ESP32-S3-Smart-Desk-Terminal.ino。
◦ 修改 const char* API_KEY = "sk-xxxxxxxx"; 填入您的 DeepSeek API Key。
◦ 修改 const float LATITUDE 与 LONGITUDE 为实际地理坐标。
4. 编译选项：开发板分区方案需选择 Huge APP (3MB No OTA/1MB SPIFFS) 以保证代码空间充足。
5. 烧录与配网：点击上传，烧录完成后，手机连接 ESP32-S3-Clock 热点进行 WiFi 配置。配网成功后，通过串口或屏幕获取局域网 IP 访问控制台。



安全声明与免责 (Security & Disclaimer)
HTTPS 证书校验：为保证在无 RTC 电池的设备上能够正常发起 HTTPS 请求，本项目采用了 setInsecure() 跳过证书校验。请勿将本设备用于处理敏感数据的传输。

局域网隔离：控制台与 API 调用依赖于本地局域网。切勿在路由器中将该设备的 IP 进行公网端口映射，以免 API Key 泄露或设备被恶意控制。

API 计费风险：接入大语言模型（LLM）属于按量计费服务，请确保您已理解代码中的防抖机制。作者不对任何因配置不当导致的 API 额度消耗承担责任。

 注意，如果正常连接完成 WiFi 时，请点击网页端的校准按钮，程序即可正常运行
