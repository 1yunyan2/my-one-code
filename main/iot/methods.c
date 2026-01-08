#include "methods.h"

method_t *method_create()
{
    return malloc_zeroed(sizeof(method_t));
}

void method_set(method_t *method, const char *name, const char *description, properties_t *parameters, method_callback_t callback, void *callback_arg)
{
    method->name = malloc_zeroed(strlen(name) + 1);
    strcpy(method->name, name);

    method->description = malloc_zeroed(strlen(description) + 1);
    strcpy(method->description, description);

    method->parameters = parameters;
    method->callback = callback;
    method->callback_arg = callback_arg;
}

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

cJSON *methods_get_descriptor_json(methods_t *methods)
{
    cJSON *root = cJSON_CreateObject();
    method_t *method = NULL;
    mylist_for_each(method, methods)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "description", method->description);
        cJSON_AddItemToObject(item, "parameters", properties_get_descriptor_json(method->parameters));

        cJSON_AddItemToObject(root, method->name, item);
    }
    return root;
}

void methods_invoke(methods_t *methods, cJSON *command)
{
    // 解析函数名称
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

    // 根据名称找到具体method
    method_t *method = methods_get_by_name(methods, method_name->valuestring);
    if (!method)
    {
        return;
    }

    property_t *parameter = NULL;
    mylist_for_each(parameter, method->parameters)
    {
        cJSON *parameter_value = cJSON_GetObjectItem(parameters, parameter->name);
        if (!parameter_value)
        {
            return;
        }

        switch (parameter->type)
        {
        case PROPERTY_TYPE_BOOLEAN:
            parameter->value.boolean = cJSON_IsTrue(parameter_value);
            break;
        case PROPERTY_TYPE_NUMBER:
            parameter->value.number = (float)parameter_value->valuedouble;
            break;
        case PROPERTY_TYPE_STRING:
            parameter->value.string = parameter_value->valuestring;
            break;

        default:
            break;
        }
    }

    method->callback(method->callback_arg, method->parameters);
}
