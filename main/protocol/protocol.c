#include "protocol.h"
#include "esp_websocket_client.h"
#include "object.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "bsp/bsp_board.h"
#include "cJSON.h"
#include <string.h>

ESP_EVENT_DEFINE_BASE(PROTOCOL_EVENT);

#define TAG "Protocol"

#define protocol_send_text(protocol, fmtstr, ...)                                                                    \
    do                                                                                                               \
    {                                                                                                                \
        if (!esp_websocket_client_is_connected(protocol->websocket_client))                                          \
        {                                                                                                            \
            ESP_LOGW(TAG, "Websocket is not connected");                                                             \
            return;                                                                                                  \
        }                                                                                                            \
        char *_message = NULL;                                                                                       \
        asprintf(&_message, fmtstr, ##__VA_ARGS__);                                                                  \
        esp_websocket_client_send_text(protocol->websocket_client, _message, strlen(_message), pdMS_TO_TICKS(5000)); \
        free(_message);                                                                                              \
    } while (0)

struct protocol
{
    esp_websocket_client_handle_t websocket_client;
    char *session_id;

    esp_event_handler_t callback;
    void *handler_args;
};

static void protocol_hello_handler(protocol_t *protocol, cJSON *root)
{
    // 解析session_id
    free(protocol->session_id);
    protocol->session_id = NULL;
    
    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id))
    {
        size_t len = strlen(session_id->valuestring) + 1;
        protocol->session_id = malloc_zeroed(len);
        assert(protocol->session_id);
        memcpy(protocol->session_id, session_id->valuestring, len);
    }
    // 调用hello事件回调
    protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_HELLO, NULL);
}
static void protocol_llm_handler(protocol_t *protocol, cJSON *root)
{
    // 提取emotion
    cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
    if (!cJSON_IsString(emotion))
    {
        ESP_LOGW(TAG, "Text data received, but emotion is not string");
        return;
    }

    // 调用回调
    protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_LLM, emotion->valuestring);
}
static void protocol_stt_handler(protocol_t *protocol, cJSON *root)
{
    // 获取text
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (!cJSON_IsString(text))
    {
        ESP_LOGW(TAG, "Text data received, but text is not string");
        return;
    }

    // 调用回调
    protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_STT, text->valuestring);
}
static void protocol_tts_handler(protocol_t *protocol, cJSON *root)
{
    // 获取state
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (!cJSON_IsString(state))
    {
        ESP_LOGW(TAG, "Text data received, but state is not string");
        return;
    }

    if (strcmp(state->valuestring, "start") == 0)
    {
        protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_TTS_START, NULL);
    }
    else if (strcmp(state->valuestring, "stop") == 0)
    {
        protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_TTS_STOP, NULL);
    }
    else if (strcmp(state->valuestring, "sentence_start") == 0)
    {
        // 获取text
        cJSON *text = cJSON_GetObjectItem(root, "text");
        if (!cJSON_IsString(text))
        {
            ESP_LOGW(TAG, "Text data received, but text is not string");
            return;
        }
        protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_TTS_SENTENCE_START, text->valuestring);
    }
}

static void protocol_websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    protocol_t *protocol = (protocol_t *)handler_args;
    if (!protocol->callback)
    {
        ESP_LOGW(TAG, "No callback registered");
        return;
    }

    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_ERROR: /*!< This event occurs when there are any errors during execution */
        ESP_LOGE(TAG, "Websocket error");
        break;
    case WEBSOCKET_EVENT_CONNECTED: /*!< Once the Websocket has been connected to the server, no data exchange has been performed */
        ESP_LOGD(TAG, "Websocket connected");
        // 调用protocol回调
        protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_CONNECTED, NULL);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED: /*!< The connection has been disconnected */
        ESP_LOGD(TAG, "Websocket disconnected");
        break;
    case WEBSOCKET_EVENT_DATA: /*!< When receiving data from the server, possibly multiple portions of the packet */
        // 检查数据类型
        if (data->op_code != 0x01 && data->op_code != 0x02)
        {
            ESP_LOGD(TAG, "Websocket data (opcode %u) received, no need to handle", data->op_code);
            return;
        }

        // 如果收到二进制
        if (data->op_code == 0x02)
        {
            // 音频片段
            binary_data_t bin = {.ptr = (void *)data->data_ptr, .size = data->data_len};
            protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_AUDIO, &bin);
            return;
        }

        // 文本数据
        cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
        if (!root)
        {
            ESP_LOGW(TAG, "Text data received, but parse failed: %.*s", data->data_len, data->data_ptr);
            return;
        }
        // 解析type字段
        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (!cJSON_IsString(type))
        {
            ESP_LOGW(TAG, "Text data received, but type is not string");
            cJSON_Delete(root);
            return;
        }

        if (strcmp(type->valuestring, "hello") == 0)
        {
            protocol_hello_handler(protocol, root);
        }
        else if (strcmp(type->valuestring, "llm") == 0)
        {
            protocol_llm_handler(protocol, root);
        }
        else if (strcmp(type->valuestring, "stt") == 0)
        {
            protocol_stt_handler(protocol, root);
        }
        else if (strcmp(type->valuestring, "tts") == 0)
        {
            protocol_tts_handler(protocol, root);
        }
        cJSON_Delete(root);
        break;
    case WEBSOCKET_EVENT_CLOSED: /*!< The connection has been closed cleanly */
        ESP_LOGD(TAG, "Websocket closed");
        break;
    case WEBSOCKET_EVENT_BEFORE_CONNECT: /*!< The event occurs before connecting */
        ESP_LOGD(TAG, "Websocket before connect");
        break;
    case WEBSOCKET_EVENT_BEGIN: /*!< The event occurs once after thread creation, before event loop */
        ESP_LOGD(TAG, "Websocket begin");
        break;
    case WEBSOCKET_EVENT_FINISH: /*!< The event occurs once after event loop, before thread destruction */
        ESP_LOGD(TAG, "Websocket finish");
        protocol->callback(protocol->handler_args, PROTOCOL_EVENT, PROTOCOL_EVENT_DISCONNECTED, NULL);
        break;
    default:
        break;
    }
}

protocol_t *protocol_create(char *url, char *token)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    protocol_t *protocol = (protocol_t *)malloc_zeroed(sizeof(protocol_t));
    bsp_board_t *board = bsp_board_get_instance();

    char *headers = NULL;
    asprintf(&headers, "Device-Id: %s\r\nClient-Id: %s\r\nAuthorization: Bearer %s\r\nProtocol-Version: 1\r\n",
             board->mac_addr, board->uuid, token);
    esp_websocket_client_config_t websocket_cfg = {
        .uri = url,
        .headers = headers,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .network_timeout_ms = 5000,
        .reconnect_timeout_ms = 5000,
    };
    protocol->websocket_client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(protocol->websocket_client, WEBSOCKET_EVENT_ANY, protocol_websocket_event_handler, protocol);

    free(headers);

    return protocol;
}

void protocol_destroy(protocol_t *protocol)
{
    free(protocol->session_id);
    esp_websocket_client_destroy(protocol->websocket_client);
    free(protocol);
}

void protocol_connect(protocol_t *protocol)
{
    if (esp_websocket_client_is_connected(protocol->websocket_client))
    {
        ESP_LOGW(TAG, "Websocket is already connected");
        return;
    }
    esp_err_t ret = esp_websocket_client_start(protocol->websocket_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Websocket connect failed: %s", esp_err_to_name(ret));
    }
}

void protocol_disconnect(protocol_t *protocol)
{
    if (!esp_websocket_client_is_connected(protocol->websocket_client))
    {
        ESP_LOGW(TAG, "Websocket is already disconnected");
        return;
    }
    esp_err_t ret = esp_websocket_client_stop(protocol->websocket_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Websocket disconnect failed: %s", esp_err_to_name(ret));
    }
}

bool protocol_is_connected(protocol_t *protocol)
{
    return esp_websocket_client_is_connected(protocol->websocket_client);
}

void protocol_send_hello(protocol_t *protocol)
{
    protocol_send_text(protocol, "{\"audio_params\":{\"channels\":1,\"format\":\"opus\",\"frame_duration\":60,\"sample_rate\":%d},\"transport\":\"websocket\",\"type\":\"hello\",\"version\":1}", BSP_CODEC_SAMPLE_RATE);
}

void protocol_send_wake_word(protocol_t *protocol, char *wake_word)
{
    protocol_send_text(protocol, "{\"session_id\":\"%s\",\"state\":\"detect\",\"text\":\"%s\",\"type\":\"listen\"}", protocol->session_id, wake_word);
}

void protocol_send_start_listening(protocol_t *protocol, protocol_listen_type_t type)
{
    static const char *mode_str[] = {"auto", "manual", "realtime"};
    protocol_send_text(protocol, "{\"mode\":\"%s\",\"session_id\":\"%s\",\"state\":\"start\",\"type\":\"listen\"}", mode_str[type], protocol->session_id);
}

void protocol_send_stop_listening(protocol_t *protocol)
{
    protocol_send_text(protocol, "{\"session_id\":\"%s\",\"state\":\"stop\",\"type\":\"listen\"}", protocol->session_id);
}

void protocol_send_audio_data(protocol_t *protocol, binary_data_t *data)
{
    if (!esp_websocket_client_is_connected(protocol->websocket_client))
    {
        ESP_LOGW(TAG, "Websocket is not connected");
        return;
    }
    esp_websocket_client_send_bin(protocol->websocket_client, data->ptr, data->size, pdMS_TO_TICKS(10000));
}

void protocol_send_abort_speaking(protocol_t *protocol)
{
    protocol_send_text(protocol, "{\"reason\":\"wake_word_detected\",\"session_id\":\"%s\",\"type\":\"abort\"}", protocol->session_id);
}

void protocol_register_callback(protocol_t *protocol, esp_event_handler_t callback, void *handler_args)
{
    protocol->callback = callback;
    protocol->handler_args = handler_args;
}
