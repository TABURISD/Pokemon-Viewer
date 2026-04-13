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

/* 使用 GitHub Raw 地址，避免 CDN 301 重定向导致的不稳定 */
#define POKEMON_IMAGE_URL "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png"
#define MAX_PNG_SIZE      65536
#define DISPLAY_SIZE      192

/* 静态大缓冲区，避免频繁 malloc/free 导致堆碎片化 */
static uint8_t  s_png_buf[MAX_PNG_SIZE];
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

/* 从SD卡加载并显示PNG（直接解码到RGB565） */
static bool show_png_from_sd(int id)
{
    if (!s_sd_ready) return false;
    
    char path[64];
    snprintf(path, sizeof(path), "%s/%d.png", SD_POKEMON_DIR, id);
    
    size_t size = 0;
    if (sd_read_file(path, s_png_buf, MAX_PNG_SIZE, &size) != ESP_OK || size == 0) {
        return false;
    }
    
    bool ok = png_decode_buffer(s_png_buf, size, s_img_buf, DISPLAY_SIZE, DISPLAY_SIZE);
    if (ok) {
        display_rgb565_to_lcd(s_img_buf, DISPLAY_SIZE);
    }
    
    return ok;
}

/* HTTP下载处理 - 使用 ctx 替代 static 变量，避免重定向时数据错乱 */
static esp_err_t download_event_handler(esp_http_client_event_t *evt)
{
    download_ctx_t *ctx = (download_ctx_t *)evt->user_data;
    if (!ctx) return ESP_OK;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ctx->buffer = NULL;
            ctx->len = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            if (!ctx->buffer) {
                ctx->buffer = (uint8_t *)malloc(MAX_PNG_SIZE);
                ctx->len = 0;
            }
            if (ctx->buffer && ctx->len + evt->data_len < MAX_PNG_SIZE) {
                memcpy(ctx->buffer + ctx->len, evt->data, evt->data_len);
                ctx->len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (ctx->buffer && ctx->len > 0) {
                char path[64];
                snprintf(path, sizeof(path), "%s/%d.png", SD_POKEMON_DIR, ctx->id);
                sd_write_file(path, ctx->buffer, ctx->len);
                ESP_LOGI(TAG, "Saved %d bytes to %s", (int)ctx->len, path);
            }
            if (ctx->buffer) {
                free(ctx->buffer);
                ctx->buffer = NULL;
            }
            ctx->len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            if (ctx->buffer) {
                free(ctx->buffer);
                ctx->buffer = NULL;
            }
            ctx->len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* 下载单个宝可梦 */
static bool download_pokemon(int id)
{
    char url[128];
    snprintf(url, sizeof(url), POKEMON_IMAGE_URL, id);
    
    if (!wifi_manager_is_connected()) return false;
    
    ESP_LOGI(TAG, "Downloading #%d...", id);
    
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
        .max_redirection_count = 5,
        .disable_auto_redirect = false,
        .user_agent = "esp32-pokemon-viewer/1.0",
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    
    /* 清理可能未释放的缓冲区 */
    if (ctx.buffer) {
        free(ctx.buffer);
        ctx.buffer = NULL;
    }
    
    if (err == ESP_OK && status == 200) {
        ESP_LOGI(TAG, "Download OK");
        return true;
    }
    
    ESP_LOGW(TAG, "Download failed: %s, status %d", esp_err_to_name(err), status);
    return false;
}

static bool is_cached(int id) {
    if (!s_sd_ready) return false;
    char path[64];
    snprintf(path, sizeof(path), "%s/%d.png", SD_POKEMON_DIR, id);
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
