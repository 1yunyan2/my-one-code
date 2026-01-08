#include "properties.h"

property_t *property_create()
{
    return (property_t *)malloc_zeroed(sizeof(property_t));
}

void property_set(property_t *property, const char *name, const char *description, property_type_t type, property_value_t value)
{
    property->name = malloc_zeroed(strlen(name) + 1);
    strcpy(property->name, name);

    property->description = malloc_zeroed(strlen(description) + 1);
    strcpy(property->description, description);

    property->type = type;
    property->value = value;
}

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

cJSON *properties_get_descriptor_json(properties_t *properties)
{
    static const char *types[] = {"boolean", "number", "string"};
    cJSON *root = cJSON_CreateObject();
    property_t *property = NULL;
    mylist_for_each(property, properties)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "description", property->description);
        cJSON_AddStringToObject(item, "type", types[property->type]);

        cJSON_AddItemToObject(root, property->name, item);
    }
    return root;
}

cJSON *properties_get_state_json(properties_t *properties)
{
    cJSON *root = cJSON_CreateObject();
    property_t *property = NULL;
    mylist_for_each(property, properties)
    {
        switch (property->type)
        {
        case PROPERTY_TYPE_BOOLEAN:
            cJSON_AddBoolToObject(root, property->name, property->value.boolean);
            break;
        case PROPERTY_TYPE_NUMBER:
            cJSON_AddNumberToObject(root, property->name, property->value.number);
            break;
        case PROPERTY_TYPE_STRING:
            cJSON_AddStringToObject(root, property->name, property->value.string);
            break;
        default:
            break;
        }
    }
    return root;
}
