/**
 * @file ping_service.h
 * @brief Ping服务组件
 */

#ifndef _PING_SERVICE_H_
#define _PING_SERVICE_H_

#include <stdint.h>
#include "esp_err.h"

typedef void (*ping_response_cb_t)(void *response, void *result, void *user_data);

esp_err_t ping_service_init(void);
void ping_service_deinit(void);
esp_err_t ping_service_quick_start(const char *host, uint32_t interval_ms,
                                    ping_response_cb_t callback, void *user_data);

#endif
