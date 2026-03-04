/**
 * @file speaker_thing.c
 * @brief 扬声器设备（Speaker Thing）实现
 * 
 * 定义并实现一个具体的 IoT 设备 - 扬声器：
 * - 属性：静音状态、音量大小
 * - 方法：设置静音、设置音量
 */

#include "speaker_thing.h"
#include "esp_log.h"
#include "bsp/bsp_board.h"

#define TAG "Speaker thing"

/**
 * @brief 设置静音方法的回调函数
 * 
 * 根据云端下发的指令设置扬声器的静音状态，并同步更新设备属性
 * 
 * @param arg 用户参数，指向 speaker thing 对象
 * @param parameters 方法参数列表，包含 mute 布尔值
 */
void speaker_thing_set_mute_method_callback(void *arg, properties_t *parameters)
{
    // 从参数列表中获取 "mute" 参数
    property_t *mute_property = properties_get_by_name(parameters, "mute");
    if (!mute_property)
    {
        ESP_LOGE(TAG, "mute parameter is missing");
        return;
    }

    // 获取开发板实例并设置音频编码器的静音状态
    bsp_board_t *board = bsp_board_get_instance();
    esp_codec_dev_set_out_mute(board->codec_dev, mute_property->value.boolean);

    // 更新 thing 对象的静音状态属性
    thing_t *speaker_thing = (thing_t *)arg;
    property_t *mute = properties_get_by_name(speaker_thing->properties, "mute");
    mute->value.boolean = mute_property->value.boolean;
}

/**
 * @brief 设置音量方法的回调函数
 * 
 * 根据云端下发的指令设置扬声器的音量大小，并同步更新设备属性
 * 
 * @param arg 用户参数，指向 speaker thing 对象
 * @param parameters 方法参数列表，包含 volume 数值
 */
void speaker_thing_set_volume_method_callback(void *arg, properties_t *parameters)
{
    // 设置音量需要 volume 参数
    property_t *volume_property = properties_get_by_name(parameters, "volume");
    if (!volume_property)
    {
        ESP_LOGE(TAG, "volume parameter is missing");
        return;
    }

    // 获取开发板实例并设置音频编码器的输出音量
    bsp_board_t *board = bsp_board_get_instance();
    esp_codec_dev_set_out_vol(board->codec_dev, (int)volume_property->value.number);

    // 更新 thing 对象的音量属性
    thing_t *speaker_thing = (thing_t *)arg;
    property_t *volume = properties_get_by_name(speaker_thing->properties, "volume");
    volume->value.number = volume_property->value.number;
}

/**
 * @brief 初始化扬声器设备的属性列表
 * 
 * 创建并配置扬声器的所有属性：
 * - 静音属性（布尔类型，默认 false）
 * - 音量属性（数字类型，默认 60）
 * 
 * @return properties_t* 属性列表
 */
properties_t *speaker_thing_init_properties()
{
    // 创建静音属性
    property_t *mute_property = property_create();
    property_set(mute_property,
                 "mute",                                // 属性名称
                 "mute status",                         // 属性描述
                 PROPERTY_TYPE_BOOLEAN,                 // 属性类型：布尔值
                 (property_value_t){.boolean = false}); // 默认值：未静音

    // 创建音量属性
    property_t *volume_property = property_create();
    property_set(volume_property,
                 "volume",                          // 属性名称
                 "volume level",                    // 属性描述
                 PROPERTY_TYPE_NUMBER,              // 属性类型：数字
                 (property_value_t){.number = 60}); // 默认值：60

    // 创建属性列表并添加属性
    properties_t *properties = mylist_create();
    mylist_add(properties, mute_property);      // 添加静音属性
    mylist_add(properties, volume_property);    // 添加音量属性

    return properties;
}

/**
 * @brief 初始化扬声器设备的方法列表
 * 
 * 创建并配置扬声器的所有可调用方法：
 * - 设置静音方法（SetMute）
 * - 设置音量方法（SetVolume）
 * 
 * @param thing 指向扬声器 thing 的指针
 * @return methods_t* 方法列表
 */
methods_t *speaker_thing_init_methods(thing_t *thing)
{
    // ========== 1. 创建设置静音方法 ==========
    method_t *set_mute_method = method_create();

    // 创建方法参数：mute（布尔值）
    property_t *mute_parameter = property_create();
    property_set(mute_parameter,
                 "mute",                                // 参数名称
                 "mute status",                         // 参数描述
                 PROPERTY_TYPE_BOOLEAN,                 // 参数类型
                 (property_value_t){.boolean = false}); // 默认值

    // 创建参数列表并添加参数
    properties_t *set_mute_parameters = mylist_create();
    mylist_add(set_mute_parameters, mute_parameter);

    // 配置方法：设置静音
    method_set(set_mute_method,
               "SetMute",                              // 方法名称
               "Set mute status",                      // 方法描述
               set_mute_parameters,                    // 参数列表
               speaker_thing_set_mute_method_callback, // 回调函数
               thing);                                 // 用户数据（thing 对象）

    // ========== 2. 创建设置音量方法 ==========
    method_t *set_volume_method = method_create();

    // 创建方法参数：volume（数字，范围 0-100）
    property_t *volume_parameter = property_create();
    property_set(volume_parameter,
                 "volume",                         // 参数名称
                 "volume level[0,100]",            // 参数描述（带范围说明）
                 PROPERTY_TYPE_NUMBER,             // 参数类型
                 (property_value_t){.number = 0}); // 默认值

    // 创建参数列表并添加参数
    properties_t *set_volume_parameters = mylist_create();
    mylist_add(set_volume_parameters, volume_parameter);

    // 配置方法：设置音量
    method_set(set_volume_method,
               "SetVolume",                              // 方法名称
               "Set volume level",                       // 方法描述
               set_volume_parameters,                    // 参数列表
               speaker_thing_set_volume_method_callback, // 回调函数
               thing);                                   // 用户数据

    // ========== 3. 创建方法列表并添加方法 ==========
    methods_t *methods = mylist_create();
    mylist_add(methods, set_mute_method);   // 添加设置静音方法
    mylist_add(methods, set_volume_method); // 添加设置音量方法

    return methods;
}

/**
 * @brief 创建扬声器 thing 对象
 * 
 * 整合属性和方法，创建一个完整的扬声器 IoT 设备实例
 * 
 * @return thing_t* 创建的扬声器设备指针
 */
thing_t *speaker_thing_create()
{
    // 1. 创建基础的 thing 对象
    thing_t *speaker_thing = thing_create();

    // 2. 初始化扬声器属性（状态）
    properties_t *speaker_properties = speaker_thing_init_properties();

    // 3. 初始化扬声器方法（控制接口）
    methods_t *speaker_methods = speaker_thing_init_methods(speaker_thing);

    // 4. 配置 thing 对象
    thing_set(speaker_thing,
              "Speaker",          // 设备名称
              "Speaker",          // 设备描述
              speaker_properties, // 属性列表
              speaker_methods);   // 方法列表

    return speaker_thing;
}
