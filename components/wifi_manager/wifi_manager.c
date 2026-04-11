/**
 * @file wifi_manager.c
 * @brief WiFi管理组件 - 带自动重连
 */

#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi_types.h"

static const char *TAG = "WIFI_MGR";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_RECONNECT_BIT  BIT2

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t s_state = WIFI_STATE_DISCONNECTED;
static int s_retry_num = 0;
static char s_ip_addr[16] = {0};
static char s_ssid[32] = {0};
static char s_password[64] = {0};
static wifi_event_callbacks_t s_callbacks = {0};
static TaskHandle_t s_reconnect_task = NULL;
static bool s_should_reconnect = true;

// 前向声明
static void reconnect_task(void *pvParameters);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_state = WIFI_STATE_CONNECTING;
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", event->reason);
        
        s_state = WIFI_STATE_DISCONNECTED;
        if (s_callbacks.on_disconnected) {
            s_callbacks.on_disconnected(s_state, NULL, s_callbacks.user_data);
        }
        
        // 触发重连任务
        if (s_should_reconnect && s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_RECONNECT_BIT);
        }
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_state = WIFI_STATE_CONNECTED;
        ESP_LOGI(TAG, "WiFi connected, IP: %s", s_ip_addr);
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        
        if (s_callbacks.on_connected) {
            s_callbacks.on_connected(s_state, s_ip_addr, s_callbacks.user_data);
        }
    }
}

static void reconnect_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Reconnect task started");
    
    while (1) {
        // 等待重连信号
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_RECONNECT_BIT,
                                               pdTRUE, pdFALSE,
                                               pdMS_TO_TICKS(30000));  // 30秒检查一次
        
        if ((bits & WIFI_RECONNECT_BIT) || s_state != WIFI_STATE_CONNECTED) {
            if (!s_should_reconnect) continue;
            
            // 检查是否已连接
            if (s_state == WIFI_STATE_CONNECTED) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            
            ESP_LOGI(TAG, "Reconnecting... (attempt %d)", ++s_retry_num);
            esp_wifi_connect();
            
            // 等待连接结果
            vTaskDelay(pdMS_TO_TICKS(5000));  // 5秒后重试
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &instance_got_ip));
    
    // 创建重连任务
    if (s_reconnect_task == NULL) {
        xTaskCreate(reconnect_task, "wifi_reconnect", 4096, NULL, 3, &s_reconnect_task);
    }
    
    return ESP_OK;
}

void wifi_manager_set_callbacks(const wifi_event_callbacks_t *callbacks)
{
    if (callbacks) {
        s_callbacks = *callbacks;
    }
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    // 保存SSID和密码用于重连
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    if (password) {
        strncpy(s_password, password, sizeof(s_password) - 1);
    }
    s_should_reconnect = true;
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 清除之前的连接状态
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    return ESP_OK;
}

void wifi_manager_disconnect(void)
{
    s_should_reconnect = false;
    esp_wifi_disconnect();
    esp_wifi_stop();
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_state;
}

bool wifi_manager_get_ip(char *ip_str)
{
    if (s_state != WIFI_STATE_CONNECTED) return false;
    strcpy(ip_str, s_ip_addr);
    return true;
}

bool wifi_manager_wait_connected(uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_manager_is_connected(void)
{
    return s_state == WIFI_STATE_CONNECTED;
}

void wifi_manager_deinit(void)
{
    s_should_reconnect = false;
    if (s_reconnect_task) {
        vTaskDelete(s_reconnect_task);
        s_reconnect_task = NULL;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
}
