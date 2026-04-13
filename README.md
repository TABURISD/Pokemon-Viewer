# Pokemon Viewer / 宝可梦图片查看器

一个基于 **ESP32-S3** 的宝可梦图片查看器，使用 **ESP-IDF** 框架开发。设备通过 WiFi 从多个网络源下载宝可梦精灵图片，实时解码并显示在 **ST7789 240×240 LCD** 屏幕上，同时将预解码的缓存保存到 **SD 卡**，下次无需联网即可直接读取。

A Pokémon sprite viewer based on the **ESP32-S3**, developed with the **ESP-IDF** framework. It downloads Pokémon sprites from multiple network sources via WiFi, decodes them on-the-fly, and displays them on a **ST7789 240×240 LCD**. Pre-decoded caches are saved to an **SD card**, so offline viewing works instantly.

---

## 功能特性 / Features

- 🎲 **随机展示 / Random Display**：每 10 秒自动切换一只随机宝可梦（编号 1 ~ 1025）  
  Automatically switches to a random Pokémon (No. 1 ~ 1025) every 10 seconds.
- 🌐 **多源自动容错 / Multi-source Fallback**：支持 3 个下载源自动切换（GitHub Raw、Showdown、jsDelivr），每个源 3 次重试  
  Supports 3 download sources with automatic failover and up to 3 retries per source.
- 💾 **SD 卡预解码缓存 / SD Pre-decoded Cache**：下载的 PNG 会被解码为 96×96 RGB565 `.raw` 文件存到 SD 卡，后续直接本地读取  
  Downloaded PNGs are decoded into 96×96 RGB565 `.raw` files on the SD card for fast local loading.
- ⚡ **零 PSRAM 设计 / No PSRAM Required**：通过自定义 128 KB 静态 bump allocator 替代 lodepng 的堆分配，彻底避免内存碎片  
  Uses a custom 128 KB static bump allocator to replace lodepng heap allocations, eliminating fragmentation.
- 📺 **行级刷屏 / Row-based Blitting**：通过 `lcd_draw_bitmap_row()` 逐行上传像素，避免 SPI 总线竞争  
  Uploads pixels row-by-row via `lcd_draw_bitmap_row()` to avoid SPI bus contention.
- 📶 **WiFi 管理 / WiFi Management**：内置 WiFi STA 自动重连，断网后自动显示精灵球回退图案  
  Built-in WiFi STA with auto-reconnect; falls back to a Pokéball pattern when offline.
- 🎨 **开机动画 / Boot Animation**：开机显示精灵球，初始化期间带有彩虹进度条，完成后显示 "OK"  
  Shows a Pokéball and rainbow progress bar during boot, finishing with an "OK" indicator.

---

## 硬件平台 / Hardware Platform

| 项目 / Item | 说明 / Description |
|------|------|
| 主控芯片 / MCU | ESP32-S3 (No PSRAM) |
| 显示屏 / Display | ST7789 240×240 SPI LCD |
| 存储 / Storage | MicroSD 卡（SPI 模式）/ MicroSD (SPI mode) |
| 开发框架 / Framework | ESP-IDF v5.x+ (CMake) |

---

## 硬件接线 / Pinout

### LCD (ST7789)

> 接线定义在 `components/lcd_st7789/lcd_st7789.c` 中。  
> Pin definitions are in `components/lcd_st7789/lcd_st7789.c`.

| 引脚 / Pin | GPIO |
|------------|------|
| MOSI       | 41   |
| CLK        | 40   |
| CS         | 39   |
| DC         | 38   |
| RST        | 42   |
| BL (背光)   | 20   |

### SD 卡 (SPI 模式) / SD Card (SPI Mode)

| SD 卡引脚 / SD Pin | ESP32-S3 GPIO |
|--------------------|---------------|
| MOSI               | GPIO 18       |
| MISO               | GPIO 16       |
| CLK                | GPIO 21       |
| CS                 | GPIO 17       |

---

## 项目结构 / Project Structure

```
Pokemon-Viewer/
├── main/                      # 主程序入口 / Main entry
│   └── main.c                 # 初始化 WiFi / 启动应用核心
├── components/                # 组件库 / Components
│   ├── app_core/              # 应用核心：初始化流程、开机进度条
│   ├── pokemon_viewer/        # 宝可梦查看器：下载、解码、显示主逻辑
│   ├── lcd_st7789/            # ST7789 LCD 驱动（含精灵球、进度条绘制）
│   ├── sd_card/               # SD 卡文件系统操作
│   ├── wifi_manager/          # WiFi 连接管理（STA + 自动重连）
│   ├── lodepng/               # PNG 解码库（含 128KB 静态 bump allocator）
│   └── png_decoder/           # RGB565 PNG 解码封装
├── CMakeLists.txt             # 项目 CMake 配置
├── partitions.csv             # 分区表
└── sdkconfig                  # ESP-IDF 配置（目标芯片 ESP32-S3）
```

---

## 快速开始 / Quick Start

### 1. 环境准备 / Prerequisites

确保已安装 [ESP-IDF 开发环境](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)（建议 v5.x 以上）。  
Make sure the [ESP-IDF development environment](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) (v5.x+) is installed.

### 2. 配置 WiFi / Configure WiFi

编辑 `main/main.c`，修改为你的 WiFi 信息：  
Edit `main/main.c` and set your WiFi credentials:

```c
#define MY_WIFI_SSID    "你的WiFi名称"
#define MY_WIFI_PASS    "你的WiFi密码"
```

### 3. 编译与烧录 / Build & Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

> 将 `COMx` 替换为实际的串口号。  
> Replace `COMx` with your actual serial port.

### 4. 首次运行 / First Run

- 开机后屏幕显示精灵球和进度条  
  Boot screen shows a Pokéball and progress bar.
- WiFi 连接成功后，自动下载并显示第一只宝可梦  
  After WiFi connects, the first Pokémon sprite is downloaded and displayed.
- 后续每隔 10 秒切换下一只随机宝可梦  
  Then it switches to a new random Pokémon every 10 seconds.

---

## 工作原理 / How It Works

```
开机 / Boot
 ├── 初始化 LCD（显示精灵球 + 0%）/ Init LCD (Pokéball + 0%)
 ├── 初始化 NVS / Init NVS
 ├── 初始化 Pokemon Viewer / Init Pokemon Viewer
 ├── 连接 WiFi（进度条同步推进）/ Connect WiFi (progress bar updates)
 └── 启动 Viewer 任务 / Start Viewer Task
      ├── 生成随机宝可梦编号 / Generate random Pokémon ID
      ├── 检查 SD 卡是否已有 .raw 缓存 / Check SD cache
      │    ├── 无缓存 → 通过 WiFi 下载 PNG → 解码为 RGB565 .raw → 保存
      │    └── 有缓存 → 直接读取 96×96 .raw
      ├── 将 96×96 放大到 240×240 并逐行刷屏显示 / Upscale 96×96 → 240×240 and blit row-by-row
      └── 失败时显示精灵球（fallback）/ On failure, show Pokéball fallback
```

---

## 依赖 / Dependencies

- [ESP-IDF](https://github.com/espressif/esp-idf)
- [lodepng](https://github.com/lvandeve/lodepng)（已集成静态 bump allocator）
- [PokeAPI Sprites](https://github.com/PokeAPI/sprites)

---

## 许可证 / License

本项目仅供学习与交流使用。宝可梦相关素材版权归 © Nintendo / Game Freak / The Pokémon Company 所有。

This project is for educational and communication purposes only. Pokémon-related assets are copyrighted by © Nintendo / Game Freak / The Pokémon Company.
