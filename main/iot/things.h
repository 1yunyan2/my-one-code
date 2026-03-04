/**
 * @file things.h
 * @brief IoT 设备（Thing）管理模块头文件
 * 
 * 实现物模型（Thing Model）的概念，每个 Thing 代表一个 IoT 设备：
 * - 包含设备的属性（Properties）
 * - 包含设备的方法（Methods）
 * - 支持 JSON 序列化和命令调用
 * 
 * 使用 mylist 管理多个设备
 */

#pragma once

#include "methods.h"

/**
 * @brief IoT 设备结构体
 * 
 * 描述一个完整的 IoT 设备，包括其属性和方法
 */
typedef struct
{
    char *name;           ///< 设备名称
    char *description;    ///< 设备描述
    properties_t *properties;  ///< 设备属性列表
    methods_t *methods;        ///< 设备方法列表
} thing_t;

/// 设备列表类型定义（使用 mylist_t）
#define things_t mylist_t

/**
 * @brief 创建 IoT 设备实例
 * 
 * @return thing_t* 新创建的设备指针
 */
thing_t *thing_create(void);

/**
 * @brief 设置设备信息
 * 
 * @param thing 设备实例指针
 * @param name 设备名称
 * @param description 设备描述
 * @param properties 设备属性列表
 * @param methods 设备方法列表
 */
void thing_set(thing_t *thing,
               const char *name,
               const char *description,
               properties_t *properties,
               methods_t *methods);

/**
 * @brief 根据名称查找设备
 * 
 * @param things 设备列表
 * @param name 要查找的设备名称
 * @return thing_t* 找到的设备指针，未找到返回 NULL
 */
thing_t *things_get_by_name(things_t *things, const char *name);

/**
 * @brief 获取设备描述符的 JSON 表示
 * 
 * 用于向服务器上报设备能力描述
 * 
 * @param things 设备列表
 * @return cJSON* 设备描述 JSON 对象
 */
cJSON *things_get_descriptor_json(things_t *things);

/**
 * @brief 获取设备状态的 JSON 表示
 * 
 * 用于向服务器上报当前设备状态
 * 
 * @param things 设备列表
 * @return cJSON* 设备状态 JSON 对象
 */
cJSON *things_get_state_json(things_t *things);

/**
 * @brief 调用设备方法
 * 
 * 解析服务器下发的命令并调用对应设备的方法
 * 
 * @param things 设备列表
 * @param commands 命令 JSON 对象
 */
void things_invoke(things_t *things, cJSON *commands);
