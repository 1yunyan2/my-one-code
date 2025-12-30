#pragma once

#include <stddef.h>

typedef enum
{
    AUDIO_PROCESSOR_EVENT_WAKEUP,
    AUDIO_PROCESSOR_EVENT_SPEECH,
    AUDIO_PROCESSOR_EVENT_SILENCE,
} audio_processor_event_t;

typedef void (*audio_processor_event_cb_t)(audio_processor_event_t event, void *arg);

typedef struct audio_processor audio_processor_t;

audio_processor_t *audio_processor_create(void);

void audio_processor_destroy(audio_processor_t *audio_processor);

/**
 * @brief 读取opus编码的录音
 *
 * @param audio_processor
 * @param buffer
 * @param size
 */
void audio_processor_read(audio_processor_t *audio_processor, void *buffer, size_t size);

/**
 * @brief 播放opus编码的声音
 *
 * @param audio_processor
 * @param buffer
 * @param size
 */
void audio_processor_write(audio_processor_t *audio_processor, void *buffer, size_t size);

/**
 * @brief 注册事件回调，事件列表如上
 *
 * @param audio_processor
 * @param event
 * @param callback
 * @param arg
 */
void audio_processor_register_event_cb(audio_processor_t *audio_processor, audio_processor_event_cb_t callback, void *arg);
