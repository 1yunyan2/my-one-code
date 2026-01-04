#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_event.h"

typedef enum
{
    PROTOCOL_EVENT_CONNECTED,          // event_data为NULL
    PROTOCOL_EVENT_DISCONNECTED,       // event_data为NULL
    PROTOCOL_EVENT_HELLO,              // event_data为NULL
    PROTOCOL_EVENT_STT,                // event_data为char*
    PROTOCOL_EVENT_LLM,                // event_data为char*
    PROTOCOL_EVENT_TTS_START,          // event_data为NULL
    PROTOCOL_EVENT_TTS_SENTENCE_START, // event_data为char*
    PROTOCOL_EVENT_TTS_STOP,           // event_data为NULL
    PROTOCOL_EVENT_AUDIO,              // event_data为binary_data_t*
} protocol_event_t;

typedef enum
{
    PROTOCOL_LISTEN_TYPE_AUTO,
    PROTOCOL_LISTEN_TYPE_MANUAL,
    PROTOCOL_LISTEN_TYPE_REALTIME,
} protocol_listen_type_t;

typedef struct
{
    void *ptr;
    size_t size;
} binary_data_t;

typedef struct protocol protocol_t;

protocol_t *protocol_create(char *url, char *token);
void protocol_destroy(protocol_t *protocol);

// websocket连接相关接口
void protocol_connect(protocol_t *protocol);
void protocol_disconnect(protocol_t *protocol);
bool protocol_is_connected(protocol_t *protocol);

// 协议内容接口
void protocol_send_hello(protocol_t *protocol);
void protocol_send_wake_word(protocol_t *protocol, char *wake_word);
void protocol_send_start_listening(protocol_t *protocol, protocol_listen_type_t type);
void protocol_send_stop_listening(protocol_t *protocol);
void protocol_send_audio_data(protocol_t *protocol, binary_data_t *data);
void protocol_send_abord_speaking(protocol_t *protocol);

void protocol_register_callback(protocol_t *protocol, esp_event_handler_t callback, void *handler_args);
