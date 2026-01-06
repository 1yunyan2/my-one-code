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

#define TAG "Application"
#define PRINT_INTERNAL_HEAP \
    ESP_LOGE(TAG, "[%s:%d] heap size: %lu", __FILE__, __LINE__, esp_get_free_internal_heap_size())

static const char *app_state_str[] = {
    "启动中",
    "激活中",
    "空闲",
    "连接中",
    "唤醒中",
    "正在监听",
    "正在讲话",
};

typedef enum
{
    APP_STATE_STARTING,
    APP_STATE_ACTIVATING,
    APP_STATE_IDLE,
    APP_STATE_CONNECTING,
    APP_STATE_WAKEUP,
    APP_STATE_LISTENING,
    APP_STATE_SPEAKING,
} app_state_t;

typedef struct
{
    app_state_t state;
    audio_processor_t *processor;
    protocol_t *protocol;
    TaskHandle_t upload_task;

    // 唤醒超时计时器
    esp_timer_handle_t wakeup_timer;

    // 状态更新计时器
    esp_timer_handle_t status_timer;
} application_t;

static application_t s_app;

static void application_set_state(application_t *app, app_state_t state)
{
    if (app->state == state)
    {
        return;
    }
    ESP_LOGI(TAG, "状态切换: %s -> %s", app_state_str[app->state], app_state_str[state]);
    app->state = state;
    ui_update_status(app_state_str[app->state]);

    if (app->state == APP_STATE_WAKEUP)
    {
        esp_timer_start_once(app->wakeup_timer, 5 * 1000 * 1000);
    }
    else
    {
        esp_timer_stop(app->wakeup_timer);
    }
}

static void application_button_cb(void *button_handle, void *usr_data)
{
    bsp_board_t *bsp_board = bsp_board_get_instance();
    bsp_board_wifi_reset_provisioning(bsp_board);
}

static void application_check_activation(application_t *app, ota_t *ota)
{
    while (1)
    {
        ota_perform(ota);
        if (!ota->activation_code)
        {
            application_set_state(app, APP_STATE_STARTING);
            ESP_LOGI(TAG, "Activated");
            return;
        }

        application_set_state(app, APP_STATE_ACTIVATING);

        ESP_LOGI(TAG, "Activation code: %s", ota->activation_code);
        ui_show_notification("激活码", ota->activation_code, 5000);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void application_audio_processor_callback(void *event_handler_arg,
                                                 esp_event_base_t event_base,
                                                 int32_t event_id,
                                                 void *event_data)
{
    application_t *app = (application_t *)event_handler_arg;
    switch (event_id)
    {
    case AUDIO_PROCESSOR_EVENT_WAKEUP:
        if (app->state == APP_STATE_IDLE)
        {
            application_set_state(app, APP_STATE_CONNECTING);
            // 尝试连接服务器
            protocol_connect(app->protocol);
        }
        else if (app->state == APP_STATE_SPEAKING)
        {
            protocol_send_abort_speaking(app->protocol);
            application_set_state(app, APP_STATE_WAKEUP);
            audio_processor_set_vad_state(app->processor, true);
            protocol_send_wake_word(app->protocol, "你好小智");
        }
        break;
    case AUDIO_PROCESSOR_EVENT_SPEECH:
        if (app->state == APP_STATE_WAKEUP)
        {
            protocol_send_start_listening(app->protocol, PROTOCOL_LISTEN_TYPE_MANUAL);
            application_set_state(app, APP_STATE_LISTENING);
        }
        break;
    case AUDIO_PROCESSOR_EVENT_SILENCE:
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
static void application_protocol_callback(void *event_handler_arg,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data)
{
    application_t *app = (application_t *)event_handler_arg;
    switch (event_id)
    {
    case PROTOCOL_EVENT_CONNECTED: // event_data为NULL
        if (app->state == APP_STATE_CONNECTING)
        {
            protocol_send_hello(app->protocol);
        }
        break;
    case PROTOCOL_EVENT_DISCONNECTED: // event_data为NULL
        application_set_state(app, APP_STATE_IDLE);
        audio_processor_set_vad_state(app->processor, false);
        break;
    case PROTOCOL_EVENT_HELLO: // event_data为NULL
        if (app->state == APP_STATE_CONNECTING)
        {
            application_set_state(app, APP_STATE_WAKEUP);
            protocol_send_wake_word(app->protocol, "你好小智");
            audio_processor_set_vad_state(app->processor, true);
        }
        break;
    case PROTOCOL_EVENT_STT: // event_data为char*
        ESP_LOGI(TAG, "STT: %s", (char *)event_data);
        ui_update_text((char *)event_data);
        break;
    case PROTOCOL_EVENT_LLM: // event_data为char*
        ESP_LOGI(TAG, "LLM: %s", (char *)event_data);
        ui_update_emotion((char *)event_data);
        break;
    case PROTOCOL_EVENT_TTS_START: // event_data为NULL
        if (app->state == APP_STATE_WAKEUP)
        {
            audio_processor_set_vad_state(app->processor, false);
            application_set_state(app, APP_STATE_SPEAKING);
        }
        break;
    case PROTOCOL_EVENT_TTS_SENTENCE_START: // event_data为char*
        ESP_LOGI(TAG, "TTS: %s", (char *)event_data);
        ui_update_text((char *)event_data);
        break;
    case PROTOCOL_EVENT_TTS_STOP: // event_data为NULL
        if (app->state == APP_STATE_SPEAKING)
        {
            application_set_state(app, APP_STATE_WAKEUP);
            audio_processor_set_vad_state(app->processor, true);
        }
        break;

    case PROTOCOL_EVENT_AUDIO: // event_data为binary_data_t*:
        if (app->state == APP_STATE_SPEAKING)
        {
            binary_data_t *data = (binary_data_t *)event_data;
            audio_processor_write(app->processor, data->ptr, data->size);
        }

        break;
    default:
        break;
    }
}

// 将音频上传到服务器
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
        if (app->state == APP_STATE_LISTENING)
        {
            binary_data_t data = {.ptr = buffer, .size = size_read};
            protocol_send_audio_data(app->protocol, &data);
        }
    }
}

static void application_status_timer_callback(void *arg)
{
    // 获取电池电量
    uint8_t battery_soc = 0;
    esp_fill_random(&battery_soc, sizeof(battery_soc));
    if (battery_soc > 100)
    {
        battery_soc = 100;
    }

    // 获取wifi强度
    int wifi_rssi = bsp_board_wifi_get_rssi(bsp_board_get_instance());

    ui_update_battery(battery_soc);
    ui_update_wifi(wifi_rssi);
}
void application_init(void)
{
    s_app.state = APP_STATE_STARTING;

    bsp_board_t *bsp_board = bsp_board_get_instance();
    PRINT_INTERNAL_HEAP;

    // 先初始化LCD
    bsp_board_lcd_init(bsp_board);
    ui_init();
    PRINT_INTERNAL_HEAP;
    bsp_board_lcd_on(bsp_board);
    bsp_board_led_indicator_init(bsp_board);
    bsp_board_button_init(bsp_board);
    PRINT_INTERNAL_HEAP;

    iot_button_register_cb(bsp_board->sw2, BUTTON_LONG_PRESS_START, NULL, application_button_cb, NULL);

    bsp_board_nvs_init(bsp_board);
    PRINT_INTERNAL_HEAP;
    char payload[150] = {0};
    bsp_board_wifi_init(bsp_board, payload, sizeof(payload));
    if (strlen(payload) > 0)
    {
        ui_show_qrcode("扫描二维码配网", payload);
    }
    PRINT_INTERNAL_HEAP;

    bsp_board_codec_init(bsp_board);

    // 打开音频设备
    esp_codec_dev_set_out_vol(bsp_board->codec_dev, 60);
    esp_codec_dev_set_in_gain(bsp_board->codec_dev, 10);
    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = BSP_CODEC_SAMPLE_RATE,
        .bits_per_sample = BSP_CODEC_BITS_PER_SAMPLE,
        .channel = 2,
    };
    esp_codec_dev_open(bsp_board->codec_dev, &sample_info);
    PRINT_INTERNAL_HEAP;

    bool ret = bsp_board_check_status(bsp_board, LED_BIT | BUTTON_BIT | CODEC_BIT | NVS_BIT | WIFI_BIT, portMAX_DELAY);
    if (!ret)
    {
        ESP_LOGE(TAG, "设备启动失败");
        return;
    }
    ui_show_qrcode(NULL, NULL);

    ota_t *ota = ota_create();
    PRINT_INTERNAL_HEAP;

    application_check_activation(&s_app, ota);

    // 初始化音频处理器和协议模块
    s_app.protocol = protocol_create(ota->websocket_url, ota->websocket_token);
    ota_destroy(ota);
    PRINT_INTERNAL_HEAP;

    s_app.processor = audio_processor_create();
    PRINT_INTERNAL_HEAP;

    // 设置音频处理模块和协议模块的回调函数
    audio_processor_register_event_cb(s_app.processor, application_audio_processor_callback, &s_app);
    protocol_register_callback(s_app.protocol, application_protocol_callback, &s_app);

    // 启动模块
    audio_processor_start(s_app.processor);
    xTaskCreatePinnedToCoreWithCaps(application_upload_task, "upload_task", 4096, &s_app, 5, &s_app.upload_task, 1, MALLOC_CAP_SPIRAM);

    PRINT_INTERNAL_HEAP;
    // 初始化超时计时器
    esp_timer_create_args_t timer_config = {
        .callback = (esp_timer_cb_t)protocol_disconnect,
        .arg = s_app.protocol,
        .name = "wakeup_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_config, &s_app.wakeup_timer));
    PRINT_INTERNAL_HEAP;

    timer_config.callback = (esp_timer_cb_t)application_status_timer_callback;
    timer_config.arg = &s_app;
    timer_config.name = "status_timer";
    ESP_ERROR_CHECK(esp_timer_create(&timer_config, &s_app.status_timer));
    esp_timer_start_periodic(s_app.status_timer, 1000 * 1000);
    PRINT_INTERNAL_HEAP;

    // 切换到空闲模式
    application_set_state(&s_app, APP_STATE_IDLE);
}