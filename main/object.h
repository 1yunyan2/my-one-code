#pragma once
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"

static inline void *malloc_zeroed(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr)
    {
        memset(ptr, 0, size);
    }
    return ptr;
}