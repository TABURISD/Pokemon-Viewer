/**
 * @file lcd_demo.c
 * @brief LCD演示模块 - 已禁用
 */

#include "lcd_demo.h"
#include "esp_log.h"

static const char *TAG = "LCD_DEMO";

esp_err_t lcd_demo_init(const lcd_demo_config_t *config)
{
    (void)config;
    ESP_LOGI(TAG, "LCD demo is DISABLED");
    return ESP_OK;
}

esp_err_t lcd_demo_start(void)
{
    ESP_LOGW(TAG, "LCD demo start blocked");
    return ESP_OK;
}

void lcd_demo_stop(void)
{
}

void lcd_demo_set_interval(uint32_t interval_ms)
{
    (void)interval_ms;
}

void lcd_demo_next_pattern(void)
{
}

void lcd_demo_set_pattern(lcd_demo_pattern_t pattern)
{
    (void)pattern;
}
