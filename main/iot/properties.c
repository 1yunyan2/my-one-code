/**
 * @file properties.c
 * @brief IoT 设备属性管理模块实现
 * 
 * 实现属性的创建、设置、查询和 JSON 序列化功能
 */

#include "properties.h"

/**
 * @brief 创建一个新的属性对象
 * 
 * 分配并初始化属性结构体，所有字段初始化为零
 * 
 * @return property_t* 新创建的属性指针
 */
property_t *property_create()
{
    return (property_t *)malloc_zeroed(sizeof(property_t));
}

/**
 * @brief 设置属性参数
 * @param property 属性对象指针
 * @param name 属性名称
 * @param description 属性描述
 * @param type 属性类型
 * @param value 属性值
 */
void property_set(property_t *property, const char *name, const char *description, property_type_t type, property_value_t value)
{
    // 分配内存并复制属性名称
    property->name = malloc_zeroed(strlen(name) + 1);
    strcpy(property->name, name);

    // 分配内存并复制属性描述
    property->description = malloc_zeroed(strlen(description) + 1);
    strcpy(property->description, description);

    // 设置属性类型和值
    property->type = type;
    property->value = value;
}

/**
 * @brief 根据名称查找属性
 * @param properties 属性列表
 * @param name 要查找的属性名称
 * @return property_t* 找到的属性指针，未找到返回 NULL
 */
property_t *properties_get_by_name(properties_t *properties, const char *name)
{
    property_t *property = NULL;
    mylist_for_each(property, properties)
    {
        if (strcmp(name, property->name) == 0)
        {
            return property;
        }
    }
    return NULL;
}

/**
 * @brief 获取属性的描述符 JSON 格式
 * 
 * 将属性列表转换为 JSON 对象，包含每个属性的描述和类型信息
 * 
 * @param properties 属性列表
 * @return cJSON* 描述符 JSON 对象
 */
cJSON *properties_get_descriptor_json(properties_t *properties)
{
    static const char *types[] = {"boolean", "number", "string"};  ///< 类型字符串映射表
    cJSON *root = cJSON_CreateObject();
    property_t *property = NULL;
    mylist_for_each(property, properties)
    {
        cJSON *item = cJSON_CreateObject();
        // 添加属性描述
        cJSON_AddStringToObject(item, "description", property->description);
        // 添加属性类型
        cJSON_AddStringToObject(item, "type", types[property->type]);

        // 将属性添加到根对象
        cJSON_AddItemToObject(root, property->name, item);
    }
    return root;
}

/**
 * @brief 获取属性的状态 JSON 格式
 * 
 * 将属性列表转换为 JSON 对象，只包含每个属性的当前值
 * 
 * @param properties 属性列表
 * @return cJSON* 状态 JSON 对象
 */
cJSON *properties_get_state_json(properties_t *properties)
{
    cJSON *root = cJSON_CreateObject();
    property_t *property = NULL;
    mylist_for_each(property, properties)
    {
        // 根据属性类型分别处理
        switch (property->type)
        {
        case PROPERTY_TYPE_BOOLEAN:
            // 布尔类型
            cJSON_AddBoolToObject(root, property->name, property->value.boolean);
            break;
        case PROPERTY_TYPE_NUMBER:
            // 数字类型
            cJSON_AddNumberToObject(root, property->name, property->value.number);
            break;
        case PROPERTY_TYPE_STRING:
            // 字符串类型
            cJSON_AddStringToObject(root, property->name, property->value.string);
            break;
        default:
            break;
        }
    }
    return root;
}
