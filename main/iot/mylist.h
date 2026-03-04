/**
 * @file mylist.h
 * @brief 动态数组（列表）实现模块
 * 
 * 提供简单的动态数组功能，支持自动扩容和元素遍历。
 * 使用 SPIRAM 分配内存，适合存储 IoT 设备、属性、方法等列表数据。
 */

#pragma once

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "object.h"

/**
 * @brief 动态列表结构体
 * 
 * 基于数组实现的简单列表，支持自动扩容
 */
typedef struct
{
    void **ptr;       ///< 数组指针
    size_t capcity;   ///< 当前容量
    size_t size;      ///< 当前元素数量
} mylist_t;

/**
 * @brief 创建一个新的空列表
 * 
 * 分配并初始化 mylist 结构体，初始容量为 0
 * 
 * @return mylist_t* 新创建的列表指针
 */
static inline mylist_t *mylist_create()
{
    return (mylist_t *)malloc_zeroed(sizeof(mylist_t));
}

/**
 * @brief 销毁列表
 * 
 * 释放列表占用的内存（不包括元素本身）
 * 
 * @param list 要销毁的列表
 */
static inline void mylist_destroy(mylist_t *list)
{
    free(list->ptr);
    free(list);
}

/**
 * @brief 向列表添加元素
 * 
 * 如果容量不足会自动扩容（2 倍 +1），使用 SPIRAM 分配内存
 * 
 * @param list 列表指针
 * @param item 要添加的元素
 */
static inline void mylist_add(mylist_t *list, void *item)
{
    // 检查容量，如果容量小则扩容
    if (list->size >= list->capcity)
    {
        size_t new_capcity = list->capcity * 2 + 1;
        // 重新分配更大的数组
        void **new_ptr = (void **)heap_caps_realloc(list->ptr, new_capcity * sizeof(void *), MALLOC_CAP_SPIRAM);
        if (!new_ptr)
        {
            return;
        }
        list->ptr = new_ptr;
        list->capcity = new_capcity;
    }

    // 添加元素到数组末尾
    list->ptr[list->size] = item;
    list->size++;
}

/**
 * @brief 列表遍历宏
 * 
 * 用于方便地遍历列表中的所有元素
 * 
 * 使用示例:
 * @code
 * thing_t *thing;
 * mylist_for_each(thing, things)
 * {
 *     // 处理每个 thing
 * }
 * @endcode
 * 
 * @param item 当前元素变量（需要在循环外定义）
 * @param list 要遍历的列表
 */
#define mylist_for_each(item, list) for (size_t i = 0; i < list->size && (item = list->ptr[i]); i++)
