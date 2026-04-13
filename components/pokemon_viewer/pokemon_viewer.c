/**
 * @file pokemon_viewer.c
 * @brief 宝可梦图片查看器 - 直接RGB565，失败显示精灵球
 */

#include "pokemon_viewer.h"
#include "lcd_st7789.h"
#include "sd_card.h"
#include "png_decoder.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "stdlib.h"
#include "math.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "POKEMON";

static TaskHandle_t s_task_handle = NULL;
static uint32_t s_interval_ms = POKEMON_UPDATE_MS;
static uint32_t s_seed = 1;
static int s_current_id = 1;
static bool s_sd_ready = false;

/* 使用 jsdelivr CDN，配合手动重定向处理 */
#define POKEMON_IMAGE_URL "https://cdn.jsdelivr.net/gh/PokeAPI/sprites@master/sprites/pokemon/%d.png"
#define MAX_PNG_SIZE      32768
#define DISPLAY_SIZE      240

/* 静态大缓冲区
 * s_io_buf:  用于HTTP下载PNG原始数据
 * s_img_buf: 用于PNG解码输出以及LCD显示（240x240 RGB565）
 * 两者不会并发使用，因为都在同一个viewer_task中顺序执行 */
static uint8_t  s_io_buf[MAX_PNG_SIZE];
static uint16_t s_img_buf[DISPLAY_SIZE * DISPLAY_SIZE];

typedef struct {
    int id;
    uint8_t *buffer;
    size_t len;
} download_ctx_t;

/* 随机数生成 */
static uint32_t rand_u32(void) {
    s_seed = (s_seed * 1103515245 + 12345) & 0x7fffffff;
    return s_seed;
}

static int get_random_id(void) {
    return POKEMON_ID_MIN + (rand_u32() % (POKEMON_ID_MAX - POKEMON_ID_MIN + 1));
}

/* 绘制精灵球（调用驱动层函数） */
static void draw_pokeball(void)
{
    lcd_draw_pokeball();
}

/* 显示RGB565图像到LCD */
static void display_rgb565_to_lcd(const uint16_t *img_buf, int img_size)
{
    int offset_x = (LCD_WIDTH - img_size) / 2;
    int offset_y = (LCD_HEIGHT - img_size) / 2;
    
    lcd_clear(COLOR_BLACK);
    
    for (int y = 0; y < img_size; y++) {
        int row = offset_y + y;
        if (row < 0 || row >= LCD_HEIGHT) continue;
        
        for (int x = 0; x < img_size; ) {
            uint16_t color = img_buf[y * img_size + x];
            
            int run_len = 1;
            while (x + run_len < img_size && img_buf[y * img_size + x + run_len] == color) {
                run_len++;
            }
            
            lcd_fill(offset_x + x, row, run_len, 1, color);
            x += run_len;
        }
    }
}

/* 从SD卡加载预解码的RAW文件并显示 */
static bool show_png_from_sd(int id)
{
    if (!s_sd_ready) return false;
    
    char path[64];
    snprintf(path, sizeof(path), "%s/%d.raw", SD_POKEMON_DIR, id);
    
    size_t expected_size = DISPLAY_SIZE * DISPLAY_SIZE * sizeof(uint16_t);
    size_t size = 0;
    if (sd_read_file(path, (uint8_t *)s_img_buf, expected_size, &size) != ESP_OK || size != expected_size) {
        return false;
    }
    
    display_rgb565_to_lcd(s_img_buf, DISPLAY_SIZE);
    return true;
}

/* HTTP下载处理 - 使用静态缓冲区，避免 malloc */
static esp_err_t download_event_handler(esp_http_client_event_t *evt)
{
    download_ctx_t *ctx = (download_ctx_t *)evt->user_data;
    if (!ctx) return ESP_OK;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ctx->buffer = s_io_buf;
            ctx->len = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            if (ctx->buffer && ctx->len + evt->data_len < MAX_PNG_SIZE) {
                memcpy(ctx->buffer + ctx->len, evt->data, evt->data_len);
                ctx->len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            ctx->len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* 通过只读 response header 来解析 301/302 重定向，完全避免读取 body */
static bool resolve_redirect_url(const char *url, char *out_url, size_t out_size, int depth)
{
    if (depth >= 3) return false;
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent = "esp32-pokemon-viewer/1.0",
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return false;
    }
    
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    char *location = NULL;
    bool resolved = false;
    
    if (status == 200) {
        strncpy(out_url, url, out_size - 1);
        out_url[out_size - 1] = '\0';
        resolved = true;
    } else if ((status == 301 || status == 302) &&
               esp_http_client_get_header(client, "Location", &location) == ESP_OK &&
               location && location[0]) {
        ESP_LOGI(TAG, "Resolve redirect %d -> %s", status, location);
        resolved = resolve_redirect_url(location, out_url, out_size, depth + 1);
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vTaskDelay(pdMS_TO_TICKS(10));
    return resolved;
}

/* 从已解析的最终 URL 下载二进制 */
static bool download_from_url(const char *url, int id)
{
    download_ctx_t ctx = {
        .id = id,
        .buffer = NULL,
        .len = 0,
    };
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = download_event_handler,
        .timeout_ms = 20000,
        .buffer_size = 4096,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
        .user_agent = "esp32-pokemon-viewer/1.0",
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (err == ESP_OK && status == 200 && ctx.len > 0) {
        bool decoded = png_decode_buffer(ctx.buffer, ctx.len, s_img_buf, DISPLAY_SIZE, DISPLAY_SIZE);
        if (decoded) {
            char path_raw[64];
            snprintf(path_raw, sizeof(path_raw), "%s/%d.raw", SD_POKEMON_DIR, id);
            size_t raw_size = DISPLAY_SIZE * DISPLAY_SIZE * sizeof(uint16_t);
            sd_write_file(path_raw, (uint8_t *)s_img_buf, raw_size);
            ESP_LOGI(TAG, "Decoded and saved raw #%d (%d bytes)", id, (int)raw_size);
            return true;
        } else {
            ESP_LOGW(TAG, "PNG decode failed for #%d", id);
        }
    }
    
    ESP_LOGW(TAG, "Download failed: %s, status %d, len %d", esp_err_to_name(err), status, (int)ctx.len);
    return false;
}

/* 下载单个宝可梦（带重试） */
static bool download_pokemon(int id)
{
    char url[128];
    snprintf(url, sizeof(url), POKEMON_IMAGE_URL, id);
    
    if (!wifi_manager_is_connected()) return false;
    
    char final_url[256] = {0};
    if (!resolve_redirect_url(url, final_url, sizeof(final_url), 0)) {
        ESP_LOGW(TAG, "Failed to resolve URL for #%d", id);
        return false;
    }
    
    const int retry_delays_ms[] = {0, 500, 1000};
    const int max_retries = sizeof(retry_delays_ms) / sizeof(retry_delays_ms[0]);
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            ESP_LOGI(TAG, "Retry #%d for #%d after %d ms...", attempt, id, retry_delays_ms[attempt]);
            vTaskDelay(pdMS_TO_TICKS(retry_delays_ms[attempt]));
            if (!wifi_manager_is_connected()) return false;
        }
        
        ESP_LOGI(TAG, "Downloading #%d...", id);
        if (download_from_url(final_url, id)) {
            ESP_LOGI(TAG, "Download OK");
            return true;
        }
    }
    
    ESP_LOGW(TAG, "Download #%d failed after all retries", id);
    return false;
}

static bool is_cached(int id) {
    if (!s_sd_ready) return false;
    char path[64];
    snprintf(path, sizeof(path), "%s/%d.raw", SD_POKEMON_DIR, id);
    return sd_file_exists(path);
}

static void viewer_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== Pokemon Viewer Started ===");
    
    // 初始化SD卡（后台进行）
    if (sd_card_init() == ESP_OK) {
        s_sd_ready = true;
        ESP_LOGI(TAG, "SD OK");
    } else {
        ESP_LOGW(TAG, "SD failed");
    }
    
    s_current_id = get_random_id();
    
    // 主循环
    while (1) {
        bool displayed = false;
        
        // 尝试下载（如果没有缓存）
        if (s_sd_ready && !is_cached(s_current_id) && wifi_manager_is_connected()) {
            download_pokemon(s_current_id);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 尝试显示PNG
        if (s_sd_ready && is_cached(s_current_id)) {
            if (show_png_from_sd(s_current_id)) {
                displayed = true;
                ESP_LOGI(TAG, "Show #%d", s_current_id);
            }
        }
        
        // 失败或没有缓存：显示精灵球
        if (!displayed) {
            draw_pokeball();
            ESP_LOGI(TAG, "Show pokeball (fallback)");
        }
        
        vTaskDelay(pdMS_TO_TICKS(s_interval_ms));
        
        // 下一个
        int new_id = get_random_id();
        while (new_id == s_current_id) new_id = get_random_id();
        s_current_id = new_id;
    }
}

esp_err_t pokemon_viewer_init(void) {
    s_seed = esp_random();
    return ESP_OK;
}

esp_err_t pokemon_viewer_start(void) {
    if (s_task_handle) return ESP_OK;
    if (xTaskCreate(viewer_task, "pokemon_viewer", 8192, NULL, 5, &s_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void pokemon_viewer_stop(void) {
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }
}

void pokemon_viewer_set_interval(uint32_t interval_ms) {
    s_interval_ms = interval_ms;
}

void pokemon_viewer_set_wifi_ready(bool ready) {}
