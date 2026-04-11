/**
 * @file lcd_demo.h
 * @brief LCD演示模块
 */

#ifndef _LCD_DEMO_H_
#define _LCD_DEMO_H_

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint32_t interval_ms;
    uint8_t auto_start;
} lcd_demo_config_t;

typedef enum {
    LCD_DEMO_SOLID_COLOR = 0,
    LCD_DEMO_VERTICAL_STRIPES,
    LCD_DEMO_HORIZONTAL_STRIPES,
    LCD_DEMO_GRADIENT,
    LCD_DEMO_PATTERN_COUNT
} lcd_demo_pattern_t;

esp_err_t lcd_demo_init(const lcd_demo_config_t *config);
esp_err_t lcd_demo_start(void);
void lcd_demo_stop(void);
void lcd_demo_set_interval(uint32_t interval_ms);
void lcd_demo_next_pattern(void);
void lcd_demo_set_pattern(lcd_demo_pattern_t pattern);

#endif
