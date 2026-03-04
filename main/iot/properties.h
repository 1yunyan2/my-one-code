/**
 * @file properties.h
 * @brief IoT 设备属性管理模块头文件
 * 
 * 负责定义和管理 IoT 设备的各种属性，支持布尔、数字、字符串三种类型。
 * 提供属性的创建、设置、查询和 JSON 序列化功能。
 */

#pragma once

#include <stdbool.h>
#include "cJSON.h"
#include "mylist.h"

/**
 * @brief 属性类型枚举
 */
typedef enum
{
    PROPERTY_TYPE_BOOLEAN,   ///< 布尔类型属性
    PROPERTY_TYPE_NUMBER,    ///< 数字类型属性
    PROPERTY_TYPE_STRING,    ///< 字符串类型属性
} property_type_t;

/**
 * @brief 属性值联合体
 * 
 * 用于存储不同类型的属性值
 */
typedef union
{
    bool boolean;      ///< 布尔值
    float number;      ///< 数值
    char *string;      ///< 字符串指针
} property_value_t;

/**
 * @brief 属性结构体
 * 
 * 描述 IoT 设备的一个属性，包括名称、描述、类型和值
 */
typedef struct property
{
    char *name;              ///< 属性名称
    char *description;       ///< 属性描述
    property_type_t type;    ///< 属性类型
    property_value_t value;  ///< 属性值
} property_t;

/// 属性列表类型定义（使用 mylist 实现）
#define properties_t mylist_t

/**
 * @brief 创建一个新的属性对象
 * @return property_t* 新创建的属性指针
 */
property_t *property_create();

/**
 * @brief 设置属性参数
 * @param property 属性对象指针
 * @param name 属性名称
 * @param description 属性描述
 * @param type 属性类型
 * @param value 属性值
 */
void property_set(property_t *property,
                  const char *name,
                  const char *description,
                  property_type_t type,
                  property_value_t value);

/**
 * @brief 根据名称查找属性
 * @param properties 属性列表
 * @param name 要查找的属性名称
 * @return property_t* 找到的属性指针，未找到返回 NULL
 */
property_t *properties_get_by_name(properties_t *properties, const char *name);

/**
 * @brief 获取属性的描述符 JSON 格式
 * @param properties 属性列表
 * @return cJSON* 描述符 JSON 对象
 */
cJSON* properties_get_descriptor_json(properties_t *properties);

/**
 * @brief 获取属性的状态 JSON 格式
 * @param properties 属性列表
 * @return cJSON* 状态 JSON 对象
 */
cJSON* properties_get_state_json(properties_t *properties);
