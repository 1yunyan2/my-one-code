#pragma once

#include <stdbool.h>
#include "cJSON.h"
#include "mylist.h"

typedef enum
{
    PROPERTY_TYPE_BOOLEAN,
    PROPERTY_TYPE_NUMBER,
    PROPERTY_TYPE_STRING,
} property_type_t;

typedef union
{
    bool boolean;
    float number;
    char *string;
} property_value_t;

typedef struct property
{
    char *name;
    char *description;
    property_type_t type;
    property_value_t value;
} property_t;

#define properties_t mylist_t

property_t *property_create();
void property_set(property_t *property,
                  const char *name,
                  const char *description,
                  property_type_t type,
                  property_value_t value);

property_t *properties_get_by_name(properties_t *properties, const char *name);
cJSON* properties_get_descriptor_json(properties_t *properties);
cJSON* properties_get_state_json(properties_t *properties);
