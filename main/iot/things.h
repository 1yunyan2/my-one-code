#pragma once

#include "methods.h"

typedef struct
{
    char *name;
    char *description;
    properties_t *properties;
    methods_t *methods;
} thing_t;

#define things_t mylist_t

thing_t *thing_create();
void thing_set(thing_t *thing,
               const char *name,
               const char *description,
               properties_t *properties,
               methods_t *methods);

thing_t *things_get_by_name(things_t *things, const char *name);
cJSON *things_get_descriptor_json(things_t *things);
cJSON *things_get_state_json(things_t *things);
void things_invoke(things_t *things, cJSON *commands);
