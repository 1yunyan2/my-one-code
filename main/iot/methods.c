/**
 * @file methods.c
 * @brief IoT 设备方法管理模块实现
 * 
 * 实现方法的创建、设置、查询、JSON 序列化和调用功能
 */

#include "methods.h"

/**
 * @brief 创建一个新的方法对象
 * 
 * 分配并初始化方法结构体，所有字段初始化为零
 * 
 * @return method_t* 新创建的方法指针
 */
method_t *method_create()
{
    return malloc_zeroed(sizeof(method_t));
}

/**
 * @brief 设置方法参数
 * @param method 方法对象指针
 * @param name 方法名称
 * @param description 方法描述
 * @param parameters 方法参数列表
 * @param callback 方法回调函数
 * @param callback_arg 回调函数的用户参数
 */
void method_set(method_t *method, const char *name, const char *description, properties_t *parameters, method_callback_t callback, void *callback_arg)
{
    // 分配内存并复制方法名称
    method->name = malloc_zeroed(strlen(name) + 1);
    strcpy(method->name, name);

    // 分配内存并复制方法描述
    method->description = malloc_zeroed(strlen(description) + 1);
    strcpy(method->description, description);

    // 设置方法参数列表
    method->parameters = parameters;
    // 设置回调函数和用户参数
    method->callback = callback;
    method->callback_arg = callback_arg;
}

/**
 * @brief 根据名称查找方法
 * @param methods 方法列表
 * @param name 要查找的方法名称
 * @return method_t* 找到的方法指针，未找到返回 NULL
 */
method_t *methods_get_by_name(methods_t *methods, const char *name)
{
    method_t *method = NULL;
    mylist_for_each(method, methods)
    {
        if (strcmp(method->name, name) == 0)
        {
            return method;
        }
    }
    return NULL;
}

/**
 * @brief 获取方法的描述符 JSON 格式
 * 
 * 将方法列表转换为 JSON 对象，包含每个方法的描述和参数信息
 * 
 * @param methods 方法列表
 * @return cJSON* 描述符 JSON 对象
 */
cJSON *methods_get_descriptor_json(methods_t *methods)
{
    cJSON *root = cJSON_CreateObject();
    method_t *method = NULL;
    mylist_for_each(method, methods)
    {
        cJSON *item = cJSON_CreateObject();
        // 添加方法描述
        cJSON_AddStringToObject(item, "description", method->description);
        // 添加方法参数描述
        cJSON_AddItemToObject(item, "parameters", properties_get_descriptor_json(method->parameters));

        // 将方法添加到根对象
        cJSON_AddItemToObject(root, method->name, item);
    }
    return root;
}

/**
 * @brief 调用指定的方法
 * 
 * 从 JSON 命令中解析方法名和参数，查找对应方法并执行回调
 * 
 * @param methods 方法列表
 * @param command 包含方法名和参数的 JSON 命令
 */
void methods_invoke(methods_t *methods, cJSON *command)
{
    // 解析方法名称
    cJSON *method_name = cJSON_GetObjectItem(command, "method");
    if (!cJSON_IsString(method_name))
    {
        return;
    }

    // 解析参数值列表
    cJSON *parameters = cJSON_GetObjectItem(command, "parameters");
    if (!parameters)
    {
        return;
    }

    // 根据名称找到具体方法
    method_t *method = methods_get_by_name(methods, method_name->valuestring);
    if (!method)
    {
        return;
    }

    property_t *parameter = NULL;
    mylist_for_each(parameter, method->parameters)
    {
        // 从 JSON 中获取参数值
        cJSON *parameter_value = cJSON_GetObjectItem(parameters, parameter->name);
        if (!parameter_value)
        {
            return;
        }

        // 根据参数类型转换并赋值
        switch (parameter->type)
        {
        case PROPERTY_TYPE_BOOLEAN:
            // 布尔类型
            parameter->value.boolean = cJSON_IsTrue(parameter_value);
            break;
        case PROPERTY_TYPE_NUMBER:
            // 数字类型
            parameter->value.number = (float)parameter_value->valuedouble;
            break;
        case PROPERTY_TYPE_STRING:
            // 字符串类型
            parameter->value.string = parameter_value->valuestring;
            break;

        default:
            break;
        }
    }

    // 调用方法回调函数
    method->callback(method->callback_arg, method->parameters);
}
