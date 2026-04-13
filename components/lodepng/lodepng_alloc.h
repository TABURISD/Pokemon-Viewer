#ifndef LODEPNG_ALLOC_H
#define LODEPNG_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void lodepng_alloc_reset(void);
void* lodepng_malloc(size_t size);
void lodepng_free(void* ptr);
void* lodepng_realloc(void* ptr, size_t new_size);

#ifdef __cplusplus
}
#endif

#endif /* LODEPNG_ALLOC_H */
