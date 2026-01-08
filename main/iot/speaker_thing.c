#include "speaker_thing.h"
#include "esp_log.h"
#include "bsp/bsp_board.h"

#define TAG "Speaker thing"

void speaker_thing_set_mute_method_callback(void *arg, properties_t *parameters)
{
    property_t *mute_property = properties_get_by_name(parameters, "mute");
    if (!mute_property)
    {
        ESP_LOGE(TAG, "mute parameter is missing");
        return;
    }

    bsp_board_t *board = bsp_board_get_instance();
    esp_codec_dev_set_out_mute(board->codec_dev, mute_property->value.boolean);

    // 更新thing的mute state
    thing_t *speaker_thing = (thing_t *)arg;
    property_t *mute = properties_get_by_name(speaker_thing->properties, "mute");
    mute->value.boolean = mute_property->value.boolean;
}

void speaker_thing_set_volume_method_callback(void *arg, properties_t *parameters)
{
    // 设置音量需要volume参数
    property_t *volume_property = properties_get_by_name(parameters, "volume");
    if (!volume_property)
    {
        ESP_LOGE(TAG, "volume parameter is missing");
        return;
    }

    bsp_board_t *board = bsp_board_get_instance();
    esp_codec_dev_set_out_vol(board->codec_dev, (int)volume_property->value.number);

    // 更新thing的volume state
    thing_t *speaker_thing = (thing_t *)arg;
    property_t *volume = properties_get_by_name(speaker_thing->properties, "volume");
    volume->value.number = volume_property->value.number;
}

properties_t *speaker_thing_init_properties()
{
    // 静音属性
    property_t *mute_property = property_create();
    property_set(mute_property,
                 "mute",
                 "mute status",
                 PROPERTY_TYPE_BOOLEAN,
                 (property_value_t){.boolean = false});

    // 音量属性
    property_t *volume_property = property_create();
    property_set(volume_property,
                 "volume",
                 "volume level",
                 PROPERTY_TYPE_NUMBER,
                 (property_value_t){.number = 60});

    // 添加properties
    properties_t *properties = mylist_create();
    mylist_add(properties, mute_property);
    mylist_add(properties, volume_property);

    return properties;
}

methods_t *speaker_thing_init_methods(thing_t *thing)
{
    // 设置静音方法
    method_t *set_mute_method = method_create();
    property_t *mute_parameter = property_create();
    property_set(mute_parameter,
                 "mute",
                 "mute status",
                 PROPERTY_TYPE_BOOLEAN,
                 (property_value_t){.boolean = false});

    properties_t *set_mute_parameters = mylist_create();
    mylist_add(set_mute_parameters, mute_parameter);
    method_set(set_mute_method,
               "SetMute",
               "Set mute status",
               set_mute_parameters,
               speaker_thing_set_mute_method_callback,
               thing);

    // 设置音量方法
    method_t *set_volume_method = method_create();
    property_t *volume_parameter = property_create();
    property_set(volume_parameter,
                 "volume",
                 "volume level[0,100]",
                 PROPERTY_TYPE_NUMBER,
                 (property_value_t){.number = 0});
    properties_t *set_volume_parameters = mylist_create();
    mylist_add(set_volume_parameters, volume_parameter);
    method_set(set_volume_method,
               "SetVolume",
               "Set volume level",
               set_volume_parameters,
               speaker_thing_set_volume_method_callback,
               thing);

    methods_t *methods = mylist_create();
    mylist_add(methods, set_mute_method);
    mylist_add(methods, set_volume_method);
    return methods;
}

thing_t *speaker_thing_create()
{
    // 创建speaker thing
    thing_t *speaker_thing = thing_create();

    // 添加属性
    properties_t *speaker_properties = speaker_thing_init_properties();

    // 添加方法
    methods_t *speaker_methods = speaker_thing_init_methods(speaker_thing);

    thing_set(speaker_thing,
              "Speaker",
              "Speaker",
              speaker_properties,
              speaker_methods);

    return speaker_thing;
}