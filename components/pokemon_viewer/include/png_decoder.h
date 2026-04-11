/*
 * PNG解码器头文件 - 直接输出RGB565
 */
#ifndef _PNG_DECODER_H_
#define _PNG_DECODER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decode PNG directly to RGB565 buffer (2 bytes per pixel)
bool png_decode_buffer(const uint8_t *png_data, size_t png_size,
                       uint16_t *out_buffer, int width, int height);

#ifdef __cplusplus
}
#endif

#endif
