/**
 * @file lcd_st7789.c
 * @brief ST7789 LCD Driver - Boot screen with pokeball and progress
 */

#include "lcd_st7789.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

static const char *TAG = "LCD_ST7789";

#define LCD_PIN_MOSI    41
#define LCD_PIN_CLK     40
#define LCD_PIN_CS      39
#define LCD_PIN_DC      38
#define LCD_PIN_RST     42
#define LCD_PIN_BL      20

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

static int s_last_percent = -1;
static bool s_progress_dirty = true;

// Swap bytes for display
static inline uint16_t swap_bytes(uint16_t color) {
    return (color >> 8) | (color << 8);
}

void lcd_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (!panel_handle) return;
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;
    static DRAM_ATTR uint16_t pixel_buf;
    pixel_buf = swap_bytes(color);
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &pixel_buf);
}

void lcd_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (!panel_handle) return;
    if (w <= 0 || h <= 0) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    
    int x_end = x + w;
    int y_end = y + h;
    if (x_end > LCD_WIDTH) x_end = LCD_WIDTH;
    if (y_end > LCD_HEIGHT) y_end = LCD_HEIGHT;
    if (x_end <= x || y_end <= y) return;

    static DRAM_ATTR uint16_t line_buf[LCD_WIDTH];
    uint16_t swapped = swap_bytes(color);
    int pixels = x_end - x;
    
    for (int i = 0; i < pixels; i++) {
        line_buf[i] = swapped;
    }

    for (int row = y; row < y_end; row++) {
        esp_lcd_panel_draw_bitmap(panel_handle, x, row, x_end, row + 1, line_buf);
    }
}

void lcd_clear(uint16_t color)
{
    lcd_fill(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
    s_progress_dirty = true;
}

void lcd_draw_pokeball(void)
{
    int cx = LCD_WIDTH / 2;
    int cy = LCD_HEIGHT / 2;  // Centered
    
    lcd_clear(COLOR_BLACK);
    
    // Upper half - Red (flipped)
    for (int dy = -60; dy < 0; dy++) {
        int row = cy - dy;
        if (row < 0 || row >= LCD_HEIGHT) continue;
        int dx = (int)sqrtf(3600 - dy*dy);
        if (dx > 0) lcd_fill(cx - dx, row, dx * 2, 1, COLOR_RED);
    }
    
    // Lower half - White (flipped)
    for (int dy = 0; dy <= 60; dy++) {
        int row = cy - dy;
        if (row < 0 || row >= LCD_HEIGHT) continue;
        int dx = (int)sqrtf(3600 - dy*dy);
        if (dx > 0) lcd_fill(cx - dx, row, dx * 2, 1, COLOR_WHITE);
    }
    
    // Center black line
    lcd_fill(cx - 65, cy - 3, 130, 6, COLOR_BLACK);
    
    // Center button - outer white circle (radius 18)
    for (int dy = -18; dy <= 18; dy++) {
        int row = cy + dy;
        if (row < 0 || row >= LCD_HEIGHT) continue;
        int dx = (int)sqrtf(324 - dy*dy);
        if (dx > 0) lcd_fill(cx - dx, row, dx * 2, 1, COLOR_WHITE);
    }
    
    // Inner black circle (radius 10)
    for (int dy = -10; dy <= 10; dy++) {
        int row = cy + dy;
        if (row < 0 || row >= LCD_HEIGHT) continue;
        int dx = (int)sqrtf(100 - dy*dy);
        if (dx > 0) lcd_fill(cx - dx, row, dx * 2, 1, COLOR_BLACK);
    }
    
    // Core white circle (radius 5)
    for (int dy = -5; dy <= 5; dy++) {
        int row = cy + dy;
        if (row < 0 || row >= LCD_HEIGHT) continue;
        int dx = (int)sqrtf(25 - dy*dy);
        if (dx > 0) lcd_fill(cx - dx, row, dx * 2, 1, COLOR_WHITE);
    }
}

// Simple 3x5 digit font (each digit is 3x5 pixels)
static const uint8_t digit_font[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b001, 0b010, 0b010}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
};

static void draw_digit(int x, int y, int digit, uint16_t color)
{
    if (digit < 0 || digit > 9) return;
    // Vertical flip: draw from bottom to top
    for (int row = 4; row >= 0; row--) {
        uint8_t pattern = digit_font[digit][row];
        int flipped_row = 4 - row;  // map row 0->4, 4->0
        for (int col = 0; col < 3; col++) {
            if (pattern & (1 << (2 - col))) {
                lcd_fill(x + col * 4, y + flipped_row * 4, 3, 3, color);
            }
        }
    }
}

static void draw_number(int x, int y, int value, uint16_t color)
{
    if (value >= 100) draw_digit(x, y, value / 100, color);
    if (value >= 10) draw_digit(x + 16, y, (value / 10) % 10, color);
    draw_digit(x + 32, y, value % 10, color);
}

static void draw_text_small(int x, int y, const char *text, uint16_t color)
{
    // Very simple 5x7-like characters, only uppercase and spaces
    static const uint8_t font_A[] = {0x7E, 0x09, 0x09, 0x09, 0x7E};
    static const uint8_t font_B[] = {0x7F, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t font_C[] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t font_D[] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t font_E[] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    static const uint8_t font_F[] = {0x7F, 0x09, 0x09, 0x09, 0x01};
    static const uint8_t font_G[] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
    static const uint8_t font_H[] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t font_I[] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t font_J[] = {0x20, 0x40, 0x41, 0x3F, 0x01};
    static const uint8_t font_K[] = {0x7F, 0x08, 0x14, 0x22, 0x41};
    static const uint8_t font_L[] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t font_M[] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t font_N[] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
    static const uint8_t font_O[] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t font_P[] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t font_Q[] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
    static const uint8_t font_R[] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t font_S[] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t font_T[] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t font_U[] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    static const uint8_t font_V[] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t font_W[] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
    static const uint8_t font_X[] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t font_Y[] = {0x07, 0x08, 0x70, 0x08, 0x07};
    static const uint8_t font_Z[] = {0x61, 0x51, 0x49, 0x45, 0x43};
    static const uint8_t font_0[] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t font_1[] = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t font_2[] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t font_3[] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t font_4[] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t font_5[] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t font_6[] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t font_7[] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t font_8[] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t font_9[] = {0x06, 0x49, 0x49, 0x29, 0x1E};
    
    const uint8_t *font_map[36] = {
        font_0, font_1, font_2, font_3, font_4, font_5, font_6, font_7, font_8, font_9,
        font_A, font_B, font_C, font_D, font_E, font_F, font_G, font_H, font_I, font_J,
        font_K, font_L, font_M, font_N, font_O, font_P, font_Q, font_R, font_S, font_T,
        font_U, font_V, font_W, font_X, font_Y, font_Z
    };
    
    int cx = x;
    for (const char *p = text; *p; p++) {
        char c = *p;
        const uint8_t *bitmap = NULL;
        if (c >= '0' && c <= '9') bitmap = font_map[c - '0'];
        else if (c >= 'A' && c <= 'Z') bitmap = font_map[c - 'A' + 10];
        else if (c >= 'a' && c <= 'z') bitmap = font_map[c - 'a' + 10];
        else { cx += 6; continue; } // space or unsupported
        
        if (bitmap) {
            for (int col = 0; col < 5; col++) {
                uint8_t b = bitmap[col];
                for (int row = 0; row < 7; row++) {
                    if (b & (1 << row)) {
                        // Vertical flip for prism
                        lcd_draw_pixel(cx + col, y + (6 - row), color);
                    }
                }
            }
        }
        cx += 6;
    }
}

// Rainbow gradient: green -> yellow -> red
static uint16_t rainbow_color(uint8_t pos)
{
    uint8_t r = (pos > 128) ? 0xFF : (pos * 2);
    uint8_t g = (pos < 128) ? 0xFF : (255 - (pos - 128) * 2);
    uint8_t b = 0;
    return swap_bytes(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void lcd_boot_progress(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    int bar_y = LCD_HEIGHT - 36;
    int bar_width = 200;
    int bar_height = 12;
    int bar_x = (LCD_WIDTH - bar_width) / 2;
    int num_y = bar_y - 34;
    int num_x = bar_x + bar_width / 2 - 30;
    
    int fill_width = (percent * bar_width) / 100;
    
    if (s_last_percent < 0 || s_progress_dirty) {
        // Full redraw: screen may have been cleared by other tasks
        lcd_fill(bar_x - 4, num_y - 4, bar_width + 8, 50, COLOR_BLACK);
        
        // Percentage number
        draw_number(num_x, num_y, percent, COLOR_YELLOW);
        
        // White border
        lcd_fill(bar_x - 2, bar_y - 2, bar_width + 4, 2, COLOR_WHITE);
        lcd_fill(bar_x - 2, bar_y + bar_height, bar_width + 4, 2, COLOR_WHITE);
        lcd_fill(bar_x - 2, bar_y - 2, 2, bar_height + 4, COLOR_WHITE);
        lcd_fill(bar_x + bar_width, bar_y - 2, 2, bar_height + 4, COLOR_WHITE);
        
        // Black inner background
        lcd_fill(bar_x, bar_y, bar_width, bar_height, COLOR_BLACK);
        
        // Draw full progress from start
        for (int xx = 0; xx < fill_width; xx += 2) {
            uint8_t hue = (xx * 255) / bar_width;
            uint16_t c = rainbow_color(hue);
            int w = (xx + 2 > fill_width) ? (fill_width - xx) : 2;
            if (w > 0) {
                lcd_fill(bar_x + xx, bar_y, w, bar_height, c);
            }
        }
        
        s_progress_dirty = false;
    } else {
        // Only update the number (clear old number area)
        lcd_fill(num_x - 2, num_y - 2, 74, 26, COLOR_BLACK);
        draw_number(num_x, num_y, percent, COLOR_YELLOW);
        
        int last_fill = (s_last_percent * bar_width) / 100;
        
        // Only draw the delta progress, not from start
        if (fill_width > last_fill) {
            for (int xx = last_fill; xx < fill_width; xx += 2) {
                uint8_t hue = (xx * 255) / bar_width;
                uint16_t c = rainbow_color(hue);
                int w = (xx + 2 > fill_width) ? (fill_width - xx) : 2;
                if (w > 0) {
                    lcd_fill(bar_x + xx, bar_y, w, bar_height, c);
                }
            }
        }
    }
    
    // Glow tail
    if (fill_width > 0 && fill_width < bar_width) {
        lcd_fill(bar_x + fill_width - 2, bar_y - 1, 4, bar_height + 2, COLOR_WHITE);
    }
    
    s_last_percent = percent;
}

esp_err_t lcd_init(void)
{
    ESP_LOGI(TAG, "Init ST7789");

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_RST) | (1ULL << LCD_PIN_BL),
    };
    gpio_config(&io_conf);
    gpio_set_level(LCD_PIN_BL, 0);

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_CLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 2 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = 20000000,  // Lower SPI clock to reduce noise
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    
    // COLMOD RGB565
    uint8_t colmod = 0x55;
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x3A, &colmod, 1));
    
    uint8_t madctl = 0x00;
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl, 1));
    
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    gpio_set_level(LCD_PIN_BL, 1);
    
    // Show boot screen immediately
    lcd_draw_pokeball();
    lcd_boot_progress(0);
    
    ESP_LOGI(TAG, "Init OK");
    return ESP_OK;
}
