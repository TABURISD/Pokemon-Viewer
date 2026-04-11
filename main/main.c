/**
 * @file main.c
 * @brief ESP32 宝可梦查看器 - SD卡缓存版
 * 
 * 每10秒显示一张随机宝可梦图片
 * 首次从网络下载，后续从SD卡读取
 * 
 * SD卡接线：
 *   MOSI -> GPIO18
 *   MISO -> GPIO16
 *   CLK  -> GPIO21
 *   CS   -> GPIO17
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_core.h"

/* 修改为你的WiFi信息 */
#define MY_WIFI_SSID    "H165-383_0F28"
#define MY_WIFI_PASS    "tian0830"

void app_main(void)
{
    app_config_t config = {
        .wifi_ssid = MY_WIFI_SSID,
        .wifi_pass = MY_WIFI_PASS,
        .wifi_timeout_ms = 30000,
        .update_interval_ms = 10000,
    };
    
    app_core_init();
    app_core_start(&config);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
