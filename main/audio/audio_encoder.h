#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

typedef struct audio_encoder audio_encoder_t;

audio_encoder_t *audio_encoder_create(int sample_rate, int channels);

void audio_encoder_destroy(audio_encoder_t *audio_encoder);

/* 给encoder设置输入和输出的缓冲区 */
/* 输入类型为字节型，输出类型为NoSPLIT型*/
void audio_encoder_set_buffer(audio_encoder_t *audio_encoder, RingbufHandle_t input_buffer, RingbufHandle_t output_buffer);

void audio_encoder_start(audio_encoder_t *audio_encoder);

void audio_encoder_stop(audio_encoder_t *audio_encoder);
