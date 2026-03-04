/**
 * @file methods.h
 * @brief IoT 设备方法管理模块头文件
 * 
 * 负责定义和管理 IoT 设备的可调用方法，支持参数传递和回调函数机制。
 * 提供方法的创建、设置、查询、JSON 序列化和调用功能。
 */

#pragma once

#include "properties.h"

/**
 * @brief 方法回调函数类型定义
 * 
 * @param arg 用户自定义参数
 * @param parameters 方法参数列表
 */
typedef void (*method_callback_t)(void *arg, properties_t *parameters);

/**
 * @brief 方法结构体
 * 
 * 描述 IoT 设备的一个可调用方法，包括名称、描述、参数和回调函数
 */
typedef struct
{
    char *name;              ///< 方法名称
    char *description;       ///< 方法描述
    properties_t *parameters; ///< 方法参数列表

    // 方法回调函数指针
    method_callback_t callback;  ///< 回调函数
    void *callback_arg;          ///< 回调函数的用户参数
} method_t;

/// 方法列表类型定义（使用 mylist 实现）
#define methods_t mylist_t

/**
 * @brief 创建一个新的方法对象
 * @return method_t* 新创建的方法指针
 */
method_t *method_create();

/**
 * @brief 设置方法参数
 * @param method 方法对象指针
 * @param name 方法名称
 * @param description 方法描述
 * @param parameters 方法参数列表
 * @param callback 方法回调函数
 * @param callback_arg 回调函数的用户参数
 */
void method_set(method_t *method,
                const char *name,
                const char *description,
                properties_t *parameters,
                method_callback_t callback,
                void *callback_arg);

/**
 * @brief 根据名称查找方法
 * @param methods 方法列表
 * @param name 要查找的方法名称
 * @return method_t* 找到的方法指针，未找到返回 NULL
 */
method_t *methods_get_by_name(methods_t *methods, const char *name);

/**
 * @brief 获取方法的描述符 JSON 格式
 * @param methods 方法列表
 * @return cJSON* 描述符 JSON 对象
 */
cJSON *methods_get_descriptor_json(methods_t *methods);

/**
 * @brief 调用指定的方法
 * @param methods 方法列表
 * @param command 包含方法名和参数的 JSON 命令
 */
void methods_invoke(methods_t *methods, cJSON *command);
