/**
 * @file pokemon_viewer.h
 * @brief 宝可梦图片查看器
 */

#ifndef _POKEMON_VIEWER_H_
#define _POKEMON_VIEWER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define POKEMON_ID_MIN      1
#define POKEMON_ID_MAX      1025
#define POKEMON_UPDATE_MS   10000

esp_err_t pokemon_viewer_init(void);
esp_err_t pokemon_viewer_start(void);
void pokemon_viewer_stop(void);
void pokemon_viewer_set_interval(uint32_t interval_ms);
void pokemon_viewer_set_wifi_ready(bool ready);

#endif
