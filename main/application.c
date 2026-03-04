/**
 * @file application.c
 * @brief 应用程序核心管理模块
 * 
 * 负责整个应用的状态管理、事件处理和模块协调
 * 主要功能包括：
 * - 系统状态机管理（启动、激活、空闲、唤醒等状态）
 * - 音频处理器事件回调处理
 * - 协议层事件回调处理
 * - IoT 设备管理和命令分发
 * - 系统初始化和任务调度
 */

#include "application.h"
#include "bsp/bsp_board.h"
#include "audio/audio_processor.h"
#include "protocol/ota.h"
#include "protocol/protocol.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ui/ui.h"
#include "esp_random.h"
#include <string.h>
#include "iot/speaker_thing.h"

#define TAG "Application"

/**
 * @brief 打印当前内部堆内存大小宏
 * 
 * 用于调试和监控 FreeRTOS 内部内存使用情况
 * 在关键位置调用可追踪内存变化
 */
#define PRINT_INTERNAL_HEAP \
    ESP_LOGE(TAG, "[%s:%d] heap size: %lu", __FILE__, __LINE__, esp_get_free_internal_heap_size())

/**
 * @brief 应用程序状态字符串数组
 * 
 * 将状态枚举映射为可读字符串，用于日志输出和 UI 显示
 */
static const char *app_state_str[] = {
    "启动中",      ///< APP_STATE_STARTING
    "激活中",      ///< APP_STATE_ACTIVATING
    "空闲",        ///< APP_STATE_IDLE
    "连接中",      ///< APP_STATE_CONNECTING
    "唤醒中",      ///< APP_STATE_WAKEUP
    "正在监听",    ///< APP_STATE_LISTENING
    "正在讲话",    ///< APP_STATE_SPEAKING
};

/**
 * @brief 应用程序状态枚举
 * 
 * 定义系统运行的七种核心状态，状态机基于此进行转换
 */
typedef enum
{
    APP_STATE_STARTING,      ///< 系统启动中：初始化硬件和软件模块
    APP_STATE_ACTIVATING,    ///< 设备激活中：获取服务端配置和认证信息
    APP_STATE_IDLE,          ///< 空闲状态：等待用户交互
    APP_STATE_CONNECTING,    ///< 连接服务器中：建立 WebSocket 连接
    APP_STATE_WAKEUP,        ///< 唤醒状态：检测到唤醒词
    APP_STATE_LISTENING,     ///< 正在监听用户语音：录音并上传
    APP_STATE_SPEAKING,      ///< 正在播放语音回复：下载并播放 TTS 音频
} app_state_t;

/**
 * @brief 应用程序全局数据结构
 * 
 * 包含所有核心组件的句柄、状态信息和定时器
 */
typedef struct
{
    app_state_t state;              ///< 当前应用状态
    audio_processor_t *processor;   ///< 音频处理器句柄
    protocol_t *protocol;           ///< 通信协议句柄
    TaskHandle_t upload_task;       ///< 音频上传任务句柄

    esp_timer_handle_t wakeup_timer;    ///< 唤醒超时计时器（防止长时间无操作）
    esp_timer_handle_t status_timer;    ///< 状态更新计时器（定期上报状态）

    things_t *things;               ///< IoT 设备列表（物模型）
} application_t;

/// 全局应用程序实例（单例模式）
static application_t s_app;

/**
 * @brief 设置应用程序状态
 * 
 * 执行状态切换并触发相应的动作：
 * - 记录状态转换日志
 * - 更新 UI 显示新状态
 * - 管理唤醒超时计时器（进入监听状态时启动）
 * 
 * @param app 应用程序实例指针
 * @param state 目标状态
 */
static void application_set_state(application_t *app, app_state_t state)
{
    // 如果目标状态与当前状态相同，则忽略
    if (app->state == state)
    {
        return;
    }
    ESP_LOGI(TAG, "状态切换：%s -> %s", app_state_str[app->state], app_state_str[state]);
    app->state = state;
    ui_update_status(app_state_str[app->state]);

    // 在唤醒状态下启动超时计时器
    if (app->state == APP_STATE_WAKEUP)
    {
        esp_timer_start_once(app->wakeup_timer, 5 * 1000 * 1000);
    }
    else
    {
        esp_timer_stop(app->wakeup_timer);
    }
}

/**
 * @brief 按钮回调函数
 * 
 * 处理长按按钮事件，触发 WiFi 配网重置流程
 * 
 * @param button_handle 按钮句柄
 * @param usr_data 用户数据（未使用）
 */
static void application_button_cb(void *button_handle, void *usr_data)
{
    bsp_board_t *bsp_board = bsp_board_get_instance();
    // 重置 WiFi 配网并重启设备
    bsp_board_wifi_reset_provisioning(bsp_board);
}

/**
 * @brief 检查设备激活状态
 * 
 * 循环执行 OTA 请求直到设备激活成功：
 * - 如果不需要激活码，表示已激活，返回启动流程
 * - 如果需要激活码，在 UI 上显示给用户并等待
 * 
 * @param app 应用程序实例指针
 * @param ota OTA 实例指针
 */
static void application_check_activation(application_t *app, ota_t *ota)
{
    while (1)
    {
        // 执行 OTA 请求获取配置
        ota_perform(ota);
        
        // 如果不需要激活码，说明设备已激活
        if (!ota->activation_code)
        {
            application_set_state(app, APP_STATE_STARTING);
            ESP_LOGI(TAG, "Activated");
            return;
        }

        // 需要激活码，显示激活状态
        application_set_state(app, APP_STATE_ACTIVATING);

        // 在 UI 上显示激活码供用户查看
        ESP_LOGI(TAG, "Activation code: %s", ota->activation_code);
        ui_show_notification("激活码", ota->activation_code, 5000);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief 音频处理器事件回调函数
 * 
 * 处理音频处理器产生的各类事件：
 * - 唤醒词检测
 * - 语音开始检测
 * - 静音检测
 * 
 * @param event_handler_arg 事件处理参数
 * @param event_base 事件基础类型
 * @param event_id 事件 ID
 * @param event_data 事件数据
 */
static void application_audio_processor_callback(void *event_handler_arg,
                                                 esp_event_base_t event_base,
                                                 int32_t event_id,
                                                 void *event_data)
{
    application_t *app = (application_t *)event_handler_arg;
    switch (event_id)
    {
    case AUDIO_PROCESSOR_EVENT_WAKEUP:
        // 检测到唤醒词
        if (app->state == APP_STATE_IDLE)
        {
            application_set_state(app, APP_STATE_CONNECTING);
            // 尝试连接服务器
            protocol_connect(app->protocol);
        }
        else if (app->state == APP_STATE_SPEAKING)
        {
            // 打断当前播放，重新进入唤醒状态
            protocol_send_abort_speaking(app->protocol);
            application_set_state(app, APP_STATE_WAKEUP);
            audio_processor_set_vad_state(app->processor, true);
            protocol_send_wake_word(app->protocol, "你好小智");
        }
        break;
    case AUDIO_PROCESSOR_EVENT_SPEECH:
        // 检测到用户开始说话
        if (app->state == APP_STATE_WAKEUP)
        {
            protocol_send_start_listening(app->protocol, PROTOCOL_LISTEN_TYPE_MANUAL);
            application_set_state(app, APP_STATE_LISTENING);
        }
        break;
    case AUDIO_PROCESSOR_EVENT_SILENCE:
        // 检测到用户说话结束（静音）
        if (app->state == APP_STATE_LISTENING)
        {
            protocol_send_stop_listening(app->protocol);
            application_set_state(app, APP_STATE_WAKEUP);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief 协议层事件回调函数
 * 
 * 处理来自服务器的各类协议事件：
 * - 连接/断开事件
 * - Hello 握手响应
 * - STT 语音识别结果
 * - LLM 大模型回复
 * - TTS 语音合成事件
 * - IoT 设备控制指令
 * 
 * @param event_handler_arg 事件处理参数
 * @param event_base 事件基础类型
 * @param event_id 事件 ID
 * @param event_data 事件数据
 */
static void application_protocol_callback(void *event_handler_arg,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data)
{
    application_t *app = (application_t *)event_handler_arg;
    switch (event_id)
    {
    case PROTOCOL_EVENT_CONNECTED: // 连接成功事件
        if (app->state == APP_STATE_CONNECTING)
        {
            protocol_send_hello(app->protocol);
        }
        break;
    case PROTOCOL_EVENT_DISCONNECTED: // 连接断开事件
        application_set_state(app, APP_STATE_IDLE);
        audio_processor_set_vad_state(app->processor, false);
        break;
    case PROTOCOL_EVENT_HELLO: // 收到服务器 Hello 响应
        if (app->state == APP_STATE_CONNECTING)
        {
            application_set_state(app, APP_STATE_WAKEUP);
            // 发送 IoT 设备描述和状态
            protocol_send_iot(app->protocol, MESSAGE_TYPE_DESCRIPTOR, things_get_descriptor_json(app->things));
            protocol_send_iot(app->protocol, MESSAGE_TYPE_STATE, things_get_state_json(app->things));
            protocol_send_wake_word(app->protocol, "你好小智");
            audio_processor_set_vad_state(app->processor, true);
        }
        break;
    case PROTOCOL_EVENT_STT: // 语音识别结果
        ESP_LOGI(TAG, "STT: %s", (char *)event_data);
        ui_update_text((char *)event_data);
        break;
    case PROTOCOL_EVENT_LLM: // 大模型回复内容
        ESP_LOGI(TAG, "LLM: %s", (char *)event_data);
        ui_update_emotion((char *)event_data);
        break;
    case PROTOCOL_EVENT_TTS_START: // TTS 开始播放
        if (app->state == APP_STATE_WAKEUP)
        {
            audio_processor_set_vad_state(app->processor, false);
            application_set_state(app, APP_STATE_SPEAKING);
        }
        break;
    case PROTOCOL_EVENT_TTS_SENTENCE_START: // TTS 句子开始
        ESP_LOGI(TAG, "TTS: %s", (char *)event_data);
        ui_update_text((char *)event_data);
        break;
    case PROTOCOL_EVENT_TTS_STOP: // TTS 播放停止
        if (app->state == APP_STATE_SPEAKING)
        {
            application_set_state(app, APP_STATE_WAKEUP);
            audio_processor_set_vad_state(app->processor, true);
        }
        break;

    case PROTOCOL_EVENT_AUDIO: // 收到音频数据
        if (app->state == APP_STATE_SPEAKING)
        {
            binary_data_t *data = (binary_data_t *)event_data;
            audio_processor_write(app->processor, data->ptr, data->size);
        }
        break;

    case PROTOCOL_EVENT_IOT: // IoT 控制指令
        things_invoke(app->things, (cJSON *)event_data);
        break;
    default:
        break;
    }
}

/**
 * @brief 音频数据上传任务
 * 
 * 持续从音频处理器读取编码后的音频数据，
 * 并在监听状态下上传到服务器。
 * 
 * @param arg 任务参数（应用程序实例）
 */
static void application_upload_task(void *arg)
{
    application_t *app = (application_t *)arg;
    uint8_t buffer[300];
    while (1)
    {
        size_t size_read = audio_processor_read(app->processor, buffer, sizeof(buffer));
        if (size_read == 0)
        {
            continue;
        }
        // 仅在监听状态下上传音频
        if (app->state == APP_STATE_LISTENING)
        {
            binary_data_t data = {.ptr = buffer, .size = size_read};
            protocol_send_audio_data(app->protocol, &data);
        }
    }
}

/**
 * @brief 状态更新计时器回调
 * 
 * 定期更新 UI 显示的电池电量和 WiFi 信号强度
 * 
 * @param arg 任务参数
 */
static void application_status_timer_callback(void *arg)
{
    // 生成随机电量值（模拟）
    uint8_t battery_soc = 0;
    esp_fill_random(&battery_soc, sizeof(battery_soc));
    if (battery_soc > 100)
    {
        battery_soc = 100;
    }

    // 获取 WiFi 信号强度
    int wifi_rssi = bsp_board_wifi_get_rssi(bsp_board_get_instance());

    ui_update_battery(battery_soc);
    ui_update_wifi(wifi_rssi);
}

/**
 * @brief 应用程序初始化函数
 * 
 * 按顺序初始化所有系统模块：
 * 1. LCD 显示屏和 UI
 * 2. LED 指示灯和按钮
 * 3. NVS 存储和 WiFi 网络
 * 4. 音频编解码器
 * 5. OTA 检查和激活
 * 6. WebSocket 协议模块
 * 7. 音频处理器
 * 8. 计时器和 IoT 设备
 * 
 * 这是整个系统的启动入口点
 */
void application_init(void)
{
    s_app.state = APP_STATE_STARTING;

    bsp_board_t *bsp_board = bsp_board_get_instance();
    PRINT_INTERNAL_HEAP;

    // 先初始化 LCD 显示屏
    bsp_board_lcd_init(bsp_board);
    ui_init();
    PRINT_INTERNAL_HEAP;
    bsp_board_lcd_on(bsp_board);
    
    // 初始化 LED 指示灯和按钮
    bsp_board_led_indicator_init(bsp_board);
    bsp_board_button_init(bsp_board);
    PRINT_INTERNAL_HEAP;

    // 注册按钮长按回调
    iot_button_register_cb(bsp_board->sw2, BUTTON_LONG_PRESS_START, NULL, application_button_cb, NULL);

    // 初始化 NVS 非易失性存储
    bsp_board_nvs_init(bsp_board);
    PRINT_INTERNAL_HEAP;
    
    // 初始化 WiFi 网络
    char payload[150] = {0};
    bsp_board_wifi_init(bsp_board, payload, sizeof(payload));
    if (strlen(payload) > 0)
    {
        ui_show_qrcode("扫描二维码配网", payload);
    }
    PRINT_INTERNAL_HEAP;

    // 初始化音频编解码器
    bsp_board_codec_init(bsp_board);

    // 打开音频设备并设置音量和增益
    esp_codec_dev_set_out_vol(bsp_board->codec_dev, 60);
    esp_codec_dev_set_in_gain(bsp_board->codec_dev, 10);
    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = BSP_CODEC_SAMPLE_RATE,
        .bits_per_sample = BSP_CODEC_BITS_PER_SAMPLE,
        .channel = 2,
    };
    esp_codec_dev_open(bsp_board->codec_dev, &sample_info);
    PRINT_INTERNAL_HEAP;

    // 等待所有硬件模块初始化完成
    bool ret = bsp_board_check_status(bsp_board, LED_BIT | BUTTON_BIT | CODEC_BIT | NVS_BIT | WIFI_BIT, portMAX_DELAY);
    if (!ret)
    {
        ESP_LOGE(TAG, "设备启动失败");
        return;
    }
    ui_show_qrcode(NULL, NULL);

    // 创建 OTA 实例并检查激活状态
    ota_t *ota = ota_create();
    PRINT_INTERNAL_HEAP;

    application_check_activation(&s_app, ota);

    // 创建 WebSocket 协议实例
    s_app.protocol = protocol_create(ota->websocket_url, ota->websocket_token);
    ota_destroy(ota);
    PRINT_INTERNAL_HEAP;

    // 创建音频处理器实例
    s_app.processor = audio_processor_create();
    PRINT_INTERNAL_HEAP;

    // 注册事件回调函数
    audio_processor_register_event_cb(s_app.processor, application_audio_processor_callback, &s_app);
    protocol_register_callback(s_app.protocol, application_protocol_callback, &s_app);

    // 启动所有模块
    audio_processor_start(s_app.processor);
    xTaskCreatePinnedToCoreWithCaps(application_upload_task, "upload_task", 4096, &s_app, 5, &s_app.upload_task, 1, MALLOC_CAP_SPIRAM);

    PRINT_INTERNAL_HEAP;
    // 创建唤醒超时计时器
    esp_timer_create_args_t timer_config = {
        .callback = (esp_timer_cb_t)protocol_disconnect,
        .arg = s_app.protocol,
        .name = "wakeup_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_config, &s_app.wakeup_timer));
    PRINT_INTERNAL_HEAP;

    // 创建状态更新计时器
    timer_config.callback = (esp_timer_cb_t)application_status_timer_callback;
    timer_config.arg = &s_app;
    timer_config.name = "status_timer";
    ESP_ERROR_CHECK(esp_timer_create(&timer_config, &s_app.status_timer));
    esp_timer_start_periodic(s_app.status_timer, 1000 * 1000);
    PRINT_INTERNAL_HEAP;

    // 初始化 IoT 设备列表
    s_app.things = mylist_create();
    mylist_add(s_app.things, speaker_thing_create());

    // 切换到空闲状态，等待用户交互
    application_set_state(&s_app, APP_STATE_IDLE);
}
