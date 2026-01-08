#pragma once

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "object.h"

typedef struct
{
    void **ptr;
    size_t capcity;
    size_t size;
} mylist_t;

static inline mylist_t *mylist_create()
{
    return (mylist_t *)malloc_zeroed(sizeof(mylist_t));
}

static inline void mylist_destroy(mylist_t *list)
{
    free(list->ptr);
    free(list);
}

static inline void mylist_add(mylist_t *list, void *item)
{
    // 检查容量，如果容量小则扩容
    if (list->size >= list->capcity)
    {
        size_t new_capcity = list->capcity * 2 + 1;
        void **new_ptr = (void **)heap_caps_realloc(list->ptr, new_capcity * sizeof(void *), MALLOC_CAP_SPIRAM);
        if (!new_ptr)
        {
            return;
        }
        list->ptr = new_ptr;
        list->capcity = new_capcity;
    }

    // 添加元素
    list->ptr[list->size] = item;
    list->size++;
}

#define mylist_for_each(item, list) for (size_t i = 0; i < list->size && (item = list->ptr[i]); i++)
