/*
 * PNG解码封装 - 直接输出RGB565
 */
#include "png_decoder.h"
#include "lodepng.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "string.h"
#include "stdlib.h"

static const char *TAG = "PNG_DECODER";

extern "C" void lodepng_alloc_reset(void);

// Decode PNG directly to RGB565 buffer (2 bytes per pixel)
bool png_decode_buffer(const uint8_t *png_data, size_t png_size,
                       uint16_t *out_buffer, int width, int height)
{
    unsigned char *image = NULL;
    unsigned png_w, png_h;

    ESP_LOGI(TAG, "Decoding %d bytes...", (int)png_size);

    size_t largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    ESP_LOGD(TAG, "Heap largest free block: %d bytes", (int)largest_free);

    lodepng_alloc_reset();

    unsigned error = lodepng_decode32(&image, &png_w, &png_h, png_data, png_size);
    if (error) {
        ESP_LOGE(TAG, "PNG decode error %u: %s", error, lodepng_error_text(error));
        return false;
    }

    ESP_LOGI(TAG, "PNG: %ux%u -> %dx%d RGB565", png_w, png_h, width, height);

    // Decode to RGB565 with vertical flip
    for (int y = 0; y < height; y++) {
        int src_y = (height - 1 - y) * png_h / height;
        if (src_y >= (int)png_h) src_y = png_h - 1;

        for (int x = 0; x < width; x++) {
            int src_x = x * png_w / width;
            if (src_x >= (int)png_w) src_x = png_w - 1;

            int src_idx = (src_y * png_w + src_x) * 4;
            uint8_t r = image[src_idx];
            uint8_t g = image[src_idx + 1];
            uint8_t b = image[src_idx + 2];

            // RGB888 to RGB565 (lcd_fill will handle byte swap)
            out_buffer[y * width + x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
    }

    /* image was allocated by our custom bump allocator; no need to free it */
    return true;
}
