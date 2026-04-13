/**
 * @file lcd_st7789.h
 * @brief ST7789 LCD Driver
 */

#ifndef _LCD_ST7789_H_
#define _LCD_ST7789_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define LCD_WIDTH       240
#define LCD_HEIGHT      240

// Colors (RGB565 format)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0

// Initialize LCD and show boot screen immediately
esp_err_t lcd_init(void);

// Update boot progress (0-100)
void lcd_boot_progress(int percent);

// Basic drawing functions
void lcd_clear(uint16_t color);
void lcd_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void lcd_draw_pixel(int16_t x, int16_t y, uint16_t color);
void lcd_draw_bitmap_row(int16_t y, const uint16_t *data, int16_t w);

// Draw pokeball pattern
void lcd_draw_pokeball(void);

#endif
