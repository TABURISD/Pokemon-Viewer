/**
 * @file app_core.h
 * @brief 应用核心模块
 */

#ifndef _APP_CORE_H_
#define _APP_CORE_H_

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    const char *wifi_ssid;
    const char *wifi_pass;
    uint32_t wifi_timeout_ms;
    uint32_t update_interval_ms;
} app_config_t;

esp_err_t app_core_init(void);
esp_err_t app_core_start(const app_config_t *config);
void app_core_stop(void);
app_config_t app_core_get_default_config(void);

#endif
