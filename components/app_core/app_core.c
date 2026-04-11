/**
 * @file app_core.c
 * @brief 应用核心模块
 */

#include "app_core.h"
#include "lcd_st7789.h"
#include "pokemon_viewer.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_CORE";

#define DEFAULT_WIFI_TIMEOUT    30000
#define DEFAULT_UPDATE_INTERVAL 10000

static int s_current_progress = 0;

static void smooth_progress(int target)
{
    if (target < s_current_progress) target = s_current_progress;
    if (target > 100) target = 100;
    
    while (s_current_progress < target) {
        s_current_progress++;
        lcd_boot_progress(s_current_progress);
        vTaskDelay(pdMS_TO_TICKS(30));  // 30ms per percent = ~3s for 100%
    }
}

static void wifi_callback(wifi_state_t state, const char *ip_addr, void *user_data) {
    switch (state) {
        case WIFI_STATE_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected, IP: %s", ip_addr ? ip_addr : "unknown");
            break;
        case WIFI_STATE_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected");
            break;
        case WIFI_STATE_FAILED:
            ESP_LOGE(TAG, "WiFi connection failed");
            break;
        default:
            break;
    }
}

esp_err_t app_core_init(void) {
    ESP_LOGI(TAG, "================== App Core Init ==================");
    
    ESP_LOGI(TAG, "Init LCD...");
    ESP_ERROR_CHECK(lcd_init());  // Shows pokeball + 0%
    s_current_progress = 0;
    
    ESP_LOGI(TAG, "Init NVS...");
    smooth_progress(15);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Init Pokemon viewer...");
    smooth_progress(35);
    ESP_ERROR_CHECK(pokemon_viewer_init());
    
    ESP_LOGI(TAG, "Init WiFi...");
    smooth_progress(55);
    ESP_ERROR_CHECK(wifi_manager_init());
    
    wifi_event_callbacks_t wifi_cbs = {
        .on_connected = wifi_callback,
        .on_disconnected = wifi_callback,
        .on_failed = wifi_callback,
    };
    wifi_manager_set_callbacks(&wifi_cbs);
    
    smooth_progress(70);
    ESP_LOGI(TAG, "================== Init Done ==================");
    return ESP_OK;
}

esp_err_t app_core_start(const app_config_t *config) {
    const char *ssid = (config && config->wifi_ssid) ? config->wifi_ssid : "MyWiFi";
    const char *pass = (config && config->wifi_pass) ? config->wifi_pass : "your_password";
    uint32_t timeout = (config && config->wifi_timeout_ms) ? config->wifi_timeout_ms : DEFAULT_WIFI_TIMEOUT;
    uint32_t interval = (config && config->update_interval_ms) ? config->update_interval_ms : DEFAULT_UPDATE_INTERVAL;
    
    ESP_LOGI(TAG, "================== App Starting ==================");
    
    pokemon_viewer_set_interval(interval);
    
    ESP_LOGI(TAG, "Connecting to WiFi...");
    ESP_ERROR_CHECK(wifi_manager_connect(ssid, pass));
    
    // Slowly advance progress during WiFi connection
    while (s_current_progress < 90 && !wifi_manager_is_connected()) {
        s_current_progress++;
        lcd_boot_progress(s_current_progress);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    if (wifi_manager_wait_connected(timeout)) {
        char ip[16];
        if (wifi_manager_get_ip(ip)) {
            ESP_LOGI(TAG, "WiFi ready, IP: %s", ip);
        }
    } else {
        ESP_LOGW(TAG, "WiFi timeout, continuing...");
    }
    
    ESP_LOGI(TAG, "Starting Pokemon viewer...");
    smooth_progress(95);
    ESP_ERROR_CHECK(pokemon_viewer_start());
    
    smooth_progress(100);
    ESP_LOGI(TAG, "================== All Services Started ==================");
    return ESP_OK;
}

void app_core_stop(void) {
    pokemon_viewer_stop();
    wifi_manager_disconnect();
}

app_config_t app_core_get_default_config(void) {
    app_config_t cfg = {0};
    return cfg;
}
