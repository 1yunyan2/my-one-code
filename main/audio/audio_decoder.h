#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

typedef struct audio_decoder audio_decoder_t;

audio_decoder_t *audio_decoder_create(int sample_rate, int channels);

void audio_decoder_destroy(audio_decoder_t *audio_decoder);

/* 给decoder设置输入和输出的缓冲区 */
/* 输入类型为NOSPLIT，输出类型为字节型*/
void audio_decoder_set_buffer(audio_decoder_t *audio_decoder, RingbufHandle_t input_buffer, RingbufHandle_t output_buffer);

void audio_decoder_start(audio_decoder_t *audio_decoder);

void audio_decoder_stop(audio_decoder_t *audio_decoder);
