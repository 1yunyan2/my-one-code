#pragma once

#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

typedef struct audio_sr audio_sr_t;

audio_sr_t *audio_sr_create(void);

void audio_sr_destroy(audio_sr_t *sr);

void audio_sr_start(audio_sr_t *sr);

void audio_sr_stop(audio_sr_t *sr);

void audio_sr_register_event_cb(audio_sr_t *sr, esp_event_handler_t callback, void *arg);

void audio_sr_set_output_buffer(audio_sr_t *sr, RingbufHandle_t output_buffer);

void audio_sr_set_vad_state(audio_sr_t *sr, bool state);
