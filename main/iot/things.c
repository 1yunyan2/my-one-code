#include "things.h"

thing_t *thing_create()
{
    return malloc_zeroed(sizeof(thing_t));
}

void thing_set(thing_t *thing, const char *name, const char *description, properties_t *properties, methods_t *methods)
{
    thing->name = malloc_zeroed(strlen(name) + 1);
    strcpy(thing->name, name);

    thing->description = malloc_zeroed(strlen(description) + 1);
    strcpy(thing->description, description);

    thing->properties = properties;
    thing->methods = methods;
}

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

cJSON *things_get_descriptor_json(things_t *things)
{
    cJSON *root = cJSON_CreateArray();
    thing_t *thing = NULL;
    mylist_for_each(thing, things)
    {

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", thing->name);
        cJSON_AddStringToObject(item, "description", thing->description);
        cJSON_AddItemToObject(item, "properties", properties_get_state_json(thing->properties));
        cJSON_AddItemToObject(item, "methods", methods_get_descriptor_json(thing->methods));

        cJSON_AddItemToArray(root, item);
    }
    return root;
}

cJSON *things_get_state_json(things_t *things)
{
    cJSON *root = cJSON_CreateArray();
    thing_t *thing = NULL;
    mylist_for_each(thing, things)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", thing->name);
        cJSON_AddItemToObject(item, "state", properties_get_state_json(thing->properties));
        cJSON_AddItemToArray(root, item);
    }
    return root;
}

void things_invoke(things_t *things, cJSON *commands)
{
    cJSON *command = NULL;
    cJSON_ArrayForEach(command, commands)
    {
        cJSON *name = cJSON_GetObjectItem(command, "name");
        if (!cJSON_IsString(name))
        {
            continue;
        }
        thing_t *thing = things_get_by_name(things, name->valuestring);
        methods_invoke(thing->methods, command);
    }
}
