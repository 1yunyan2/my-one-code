/**
 * @file things.c
 * @brief IoT 设备（Thing）管理模块实现
 * 
 * 实现物模型（Thing Model）的概念，每个 Thing 代表一个 IoT 设备：
 * - 包含设备的属性（Properties）
 * - 包含设备的方法（Methods）
 * - 支持 JSON 序列化和命令调用
 * 
 * 使用 mylist 管理多个设备
 */

#include "things.h"

/**
 * @brief 创建 IoT 设备实例
 * 
 * 分配并初始化 thing 结构体，所有字段初始化为零
 * 
 * @return thing_t* 新创建的设备指针
 */
thing_t *thing_create()
{
    return malloc_zeroed(sizeof(thing_t));
}

/**
 * @brief 设置设备信息
 * 
 * 配置设备的名称、描述、属性和方法
 * 
 * @param thing 设备实例指针
 * @param name 设备名称
 * @param description 设备描述
 * @param properties 设备属性列表
 * @param methods 设备方法列表
 */
void thing_set(thing_t *thing, const char *name, const char *description, properties_t *properties, methods_t *methods)
{
    // 分配内存并复制设备名称
    thing->name = malloc_zeroed(strlen(name) + 1);
    strcpy(thing->name, name);

    // 分配内存并复制设备描述
    thing->description = malloc_zeroed(strlen(description) + 1);
    strcpy(thing->description, description);

    // 设置设备属性列表和方法列表
    thing->properties = properties;
    thing->methods = methods;
}

/**
 * @brief 根据名称查找设备
 * 
 * 在设备列表中遍历查找指定名称的设备
 * 
 * @param things 设备列表
 * @param name 要查找的设备名称
 * @return thing_t* 找到的设备指针，未找到返回 NULL
 */
thing_t *things_get_by_name(things_t *things, const char *name)
{
    thing_t *thing;
    mylist_for_each(thing, things)
    {
        if (strcmp(thing->name, name) == 0)
        {
            return thing;
        }
    }
    return NULL;
}

/**
 * @brief 获取设备描述符的 JSON 表示
 * 
 * 用于向服务器上报设备能力描述，包含所有设备的名称、描述、属性和方法
 * 
 * @param things 设备列表
 * @return cJSON* 设备描述 JSON 对象（数组格式）
 */
cJSON *things_get_descriptor_json(things_t *things)
{
    cJSON *root = cJSON_CreateArray();
    thing_t *thing = NULL;
    mylist_for_each(thing, things)
    {
        cJSON *item = cJSON_CreateObject();
        // 添加设备名称
        cJSON_AddStringToObject(item, "name", thing->name);
        // 添加设备描述
        cJSON_AddStringToObject(item, "description", thing->description);
        // 添加设备属性描述
        cJSON_AddItemToObject(item, "properties", properties_get_state_json(thing->properties));
        // 添加设备方法描述
        cJSON_AddItemToObject(item, "methods", methods_get_descriptor_json(thing->methods));

        // 将设备添加到数组
        cJSON_AddItemToArray(root, item);
    }
    return root;
}

/**
 * @brief 获取设备状态的 JSON 表示
 * 
 * 用于向服务器上报当前设备状态，只包含所有设备的名称和属性值
 * 
 * @param things 设备列表
 * @return cJSON* 设备状态 JSON 对象（数组格式）
 */
cJSON *things_get_state_json(things_t *things)
{
    cJSON *root = cJSON_CreateArray();
    thing_t *thing = NULL;
    mylist_for_each(thing, things)
    {
        cJSON *item = cJSON_CreateObject();
        // 添加设备名称
        cJSON_AddStringToObject(item, "name", thing->name);
        // 添加设备属性状态
        cJSON_AddItemToObject(item, "state", properties_get_state_json(thing->properties));
        // 将设备状态添加到数组
        cJSON_AddItemToArray(root, item);
    }
    return root;
}

/**
 * @brief 调用设备方法
 * 
 * 解析服务器下发的命令并调用对应设备的方法
 * 
 * @param things 设备列表
 * @param commands 命令 JSON 数组，每个元素包含设备名和方法信息
 */
void things_invoke(things_t *things, cJSON *commands)
{
    cJSON *command = NULL;
    cJSON_ArrayForEach(command, commands)
    {
        // 获取设备名称
        cJSON *name = cJSON_GetObjectItem(command, "name");
        if (!cJSON_IsString(name))
        {
            continue;
        }
        // 查找对应的设备
        thing_t *thing = things_get_by_name(things, name->valuestring);
        // 调用设备的方法
        methods_invoke(thing->methods, command);
    }
}
