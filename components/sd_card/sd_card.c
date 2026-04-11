/**
 * @file sd_card.c
 * @brief SD卡管理组件实现
 */

#include "sd_card.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "SD_CARD";

// SPI引脚配置
#define SD_PIN_MOSI     18
#define SD_PIN_MISO     16
#define SD_PIN_CLK      21
#define SD_PIN_CS       17

static sdmmc_card_t *card = NULL;

esp_err_t sd_card_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // 使用SPI3（HSPI）模式，与LCD的SPI2分开
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // 初始化SPI3总线（与LCD的SPI2独立）
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, card);

    // 创建宝可梦图片目录
    sd_mkdir(SD_POKEMON_DIR);

    return ESP_OK;
}

bool sd_file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

int sd_get_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return st.st_size;
}

esp_err_t sd_read_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t read = fread(buffer, 1, buffer_size, f);
    fclose(f);

    if (out_size) {
        *out_size = read;
    }

    return (read > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t sd_write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    return (written == size) ? ESP_OK : ESP_FAIL;
}

esp_err_t sd_mkdir(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == 0) {
            ESP_LOGI(TAG, "Created directory: %s", path);
            return ESP_OK;
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t sd_get_info(uint64_t *total_bytes, uint64_t *free_bytes)
{
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    if (f_getfree("0:", &fre_clust, &fs) != FR_OK) {
        return ESP_FAIL;
    }

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    if (total_bytes) *total_bytes = (uint64_t)tot_sect * 512;
    if (free_bytes) *free_bytes = (uint64_t)fre_sect * 512;

    return ESP_OK;
}
