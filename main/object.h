/**
 * @file object.h
 * @brief 基础对象和内存管理工具
 * 
 * 提供项目通用的内存分配函数，
 * 默认使用 PSRAM（SPIRAM）进行内存分配，
 * 并自动初始化为零以确保安全。
 */

#pragma once
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"

/**
 * @brief 从 PSRAM 分配并清零的内存分配函数
 * 
 * 该函数封装了 ESP-IDF 的 heap_caps_malloc，
 * 专门从 SPIRAM（外部 RAM）分配内存，并自动清零。
 * 
 * 优点：
 * - 使用外部大内存，减少内部 RAM 压力
 * - 自动初始化为 0，避免未初始化数据风险
 * - 适用于音频缓冲区、任务堆栈等大内存需求场景
 * 
 * @param size 需要分配的字节数
 * @return void* 分配成功的内存指针，失败返回 NULL
 */
static inline void *malloc_zeroed(size_t size)
{
    // 从 SPIRAM 分配指定大小的内存
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr)
    {
        // 将分配的内存清零
        memset(ptr, 0, size);
    }
    return ptr;
}
