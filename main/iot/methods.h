#pragma once

#include "properties.h"

typedef void (*method_callback_t)(void *arg, properties_t *parameters);

typedef struct
{
    char *name;
    char *description;
    properties_t *parameters;

    // 方法回调
    method_callback_t callback;
    void *callback_arg;
} method_t;

#define methods_t mylist_t

method_t *method_create();
void method_set(method_t *method,
                const char *name,
                const char *description,
                properties_t *parameters,
                method_callback_t callback,
                void *callback_arg);
method_t *methods_get_by_name(methods_t *methods, const char *name);
cJSON *methods_get_descriptor_json(methods_t *methods);
void methods_invoke(methods_t *methods, cJSON *command);
