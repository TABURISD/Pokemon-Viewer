/*
 * Custom static allocator for lodepng to avoid heap fragmentation on ESP32
 */
#include <stddef.h>
#include <string.h>

#define LODEPNG_POOL_SIZE (96 * 1024)

static unsigned char s_lodepng_pool[LODEPNG_POOL_SIZE];
static size_t s_lodepng_offset = 0;

void lodepng_alloc_reset(void)
{
    s_lodepng_offset = 0;
}

void* lodepng_malloc(size_t size)
{
    if (size == 0) return NULL;

    size_t aligned = (size + 3u) & ~3u;
    if (s_lodepng_offset + aligned > LODEPNG_POOL_SIZE) {
        return NULL;
    }

    void* ptr = &s_lodepng_pool[s_lodepng_offset];
    s_lodepng_offset += aligned;
    return ptr;
}

void lodepng_free(void* ptr)
{
    (void)ptr;
}

void* lodepng_realloc(void* ptr, size_t new_size)
{
    if (new_size == 0) {
        return NULL;
    }

    void* new_ptr = lodepng_malloc(new_size);
    if (!new_ptr) {
        return NULL;
    }

    if (ptr) {
        /* We don't track old size; copy new_size bytes.
         * This is safe in the bump pool because old memory is never reused. */
        memcpy(new_ptr, ptr, new_size);
    }

    return new_ptr;
}
