/**
 * @file sd_card.h
 * @brief SD卡管理组件
 */

#ifndef _SD_CARD_H_
#define _SD_CARD_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define SD_MOUNT_POINT      "/sdcard"
#define SD_POKEMON_DIR      "/sdcard/pokemon"

esp_err_t sd_card_init(void);
bool sd_file_exists(const char *path);
esp_err_t sd_read_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *out_size);
esp_err_t sd_write_file(const char *path, const uint8_t *data, size_t size);
esp_err_t sd_mkdir(const char *path);
int sd_get_file_size(const char *path);
esp_err_t sd_get_info(uint64_t *total_bytes, uint64_t *free_bytes);

#endif
