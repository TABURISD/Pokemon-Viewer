/**
 * @file wifi_manager.h
 * @brief WiFi管理组件
 */

#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define WIFI_MAX_SSID_LEN       32
#define WIFI_MAX_PASS_LEN       64

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
} wifi_state_t;

typedef void (*wifi_connect_cb_t)(wifi_state_t state, const char *ip_addr, void *user_data);

typedef struct {
    wifi_connect_cb_t on_connected;
    wifi_connect_cb_t on_disconnected;
    wifi_connect_cb_t on_failed;
    void *user_data;
} wifi_event_callbacks_t;

esp_err_t wifi_manager_init(void);
void wifi_manager_deinit(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
void wifi_manager_disconnect(void);
void wifi_manager_set_callbacks(const wifi_event_callbacks_t *callbacks);
wifi_state_t wifi_manager_get_state(void);
bool wifi_manager_get_ip(char *ip_str);
bool wifi_manager_wait_connected(uint32_t timeout_ms);
bool wifi_manager_is_connected(void);

#endif
