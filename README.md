# Pokemon Viewer

一个基于 **ESP32-S3** 的宝可梦图片查看器，使用 **ESP-IDF** 框架开发。设备通过 WiFi 从网络下载宝可梦精灵图片，显示在 **ST7789 240×240 LCD** 屏幕上，并将图片缓存到 **SD 卡**中，下次无需联网即可直接读取。

---

## 功能特性

- 🎲 **随机展示**：每 10 秒自动切换一只随机宝可梦（编号 1 ~ 1025）
- 🌐 **联网下载**：首次从 [PokeAPI CDN](https://cdn.jsdelivr.net/gh/PokeAPI/sprites) 下载 PNG 图片
- 💾 **SD 卡缓存**：下载后的图片自动保存到 SD 卡，后续直接本地读取
- ⚡ **快速启动**：开机即显示精灵球动画，初始化完成后自动进入轮播
- 📶 **WiFi 管理**：内置 WiFi 连接状态监控，支持断网后的回退显示
- 🛡️ **失败回退**：下载失败或无缓存时，自动显示精灵球图案

---

## 硬件平台

| 项目 | 说明 |
|------|------|
| 主控芯片 | ESP32-S3 |
| 显示屏 | ST7789 240×240 SPI LCD |
| 存储 | MicroSD 卡（SPI 模式） |
| 开发框架 | ESP-IDF (CMake) |

---

## 硬件接线

### LCD (ST7789)
> 请参考 `components/lcd_st7789` 中的具体接线定义（通常为 SPI 接口）。

### SD 卡 (SPI 模式)

| SD 卡引脚 | ESP32-S3 GPIO |
|-----------|---------------|
| MOSI      | GPIO 18       |
| MISO      | GPIO 16       |
| CLK       | GPIO 21       |
| CS        | GPIO 17       |

---

## 项目结构

```
Pokemon-Viewer/
├── main/                      # 主程序入口
│   └── main.c                 # 初始化 WiFi / 启动应用核心
├── components/                # 组件库
│   ├── app_core/              # 应用核心：初始化流程、开机进度条
│   ├── pokemon_viewer/        # 宝可梦查看器：下载、解码、显示主逻辑
│   ├── lcd_st7789/            # ST7789 LCD 驱动
│   ├── sd_card/               # SD 卡文件系统操作
│   ├── wifi_manager/          # WiFi 连接管理
│   ├── lodepng/               # PNG 解码库
│   └── png_decoder/           # RGB565 PNG 解码封装
├── CMakeLists.txt             # 项目 CMake 配置
├── partitions.csv             # 分区表
└── sdkconfig                  # ESP-IDF 配置（目标芯片 ESP32-S3）
```

---

## 主要组件说明

### `app_core`
- 负责系统初始化流程：LCD → NVS → Pokemon Viewer → WiFi
- 开机时显示精灵球，并带有平滑的进度条动画（0% → 100%）

### `pokemon_viewer`
- 核心查看器任务，运行在一个独立的 FreeRTOS Task 中
- 使用 `esp_http_client` 通过 HTTPS 下载宝可梦 PNG 图片
- 使用 `lodepng` 将 PNG 解码为 RGB565 格式后直接刷屏显示
- 支持 RLE（行程长度编码）风格的批量像素填充，提升显示效率

### `lcd_st7789`
- 240×240 分辨率，RGB565 色彩格式
- 提供清屏、填充、像素绘制、精灵球绘制、进度条更新等接口

### `sd_card`
- SD 卡挂载点为 `/sdcard`
- 图片缓存目录：`/sdcard/pokemon/`
- 提供文件读写、目录创建、文件存在性检查等接口

### `wifi_manager`
- 封装了 ESP-IDF 的 WiFi Station 模式连接流程
- 支持连接状态回调和超时等待

---

## 快速开始

### 1. 环境准备

确保已安装 [ESP-IDF 开发环境](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)（建议 v5.x 以上）。

### 2. 配置 WiFi

编辑 `main/main.c`，修改为你的 WiFi 信息：

```c
#define MY_WIFI_SSID    "你的WiFi名称"
#define MY_WIFI_PASS    "你的WiFi密码"
```

### 3. 编译与烧录

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

> 将 `COMx` 替换为实际的串口号。

### 4. 首次运行

- 开机后屏幕显示精灵球和进度条
- WiFi 连接成功后，自动下载并显示第一只宝可梦
- 后续每隔 10 秒切换下一只随机宝可梦

---

## 工作原理

```
开机
 ├── 初始化 LCD（显示精灵球 + 0%）
 ├── 初始化 NVS
 ├── 初始化 Pokemon Viewer
 ├── 连接 WiFi（进度条同步推进）
 └── 启动 Viewer 任务
      ├── 生成随机宝可梦编号
      ├── 检查 SD 卡是否已有缓存
      │    ├── 无缓存 → 通过 WiFi 下载并保存到 SD 卡
      │    └── 有缓存 → 直接读取
      ├── 解码 PNG 为 RGB565 并显示
      └── 失败时显示精灵球（fallback）
```

---

## 依赖

- [ESP-IDF](https://github.com/espressif/esp-idf)
- [lodepng](https://github.com/lvandeve/lodepng)
- [PokeAPI Sprites CDN](https://cdn.jsdelivr.net/gh/PokeAPI/sprites)

---

## 许可证

本项目仅供学习与交流使用。宝可梦相关素材版权归 © Nintendo / Game Freak / The Pokémon Company 所有。
