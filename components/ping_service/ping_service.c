/**
 * @file ping_service.c
 * @brief Ping服务组件 - 已禁用
 */

#include "ping_service.h"
#include "esp_log.h"

static const char *TAG = "PING_SVC";

esp_err_t ping_service_init(void)
{
    ESP_LOGI(TAG, "Ping service disabled");
    return ESP_OK;
}

void ping_service_deinit(void)
{
}

esp_err_t ping_service_quick_start(const char *host, uint32_t interval_ms,
                                    ping_response_cb_t callback, void *user_data)
{
    (void)host;
    (void)interval_ms;
    (void)callback;
    (void)user_data;
    ESP_LOGW(TAG, "Ping service is disabled");
    return ESP_OK;
}
