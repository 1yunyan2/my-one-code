#include "audio_processor.h"
#include "audio_sr.h"
#include "audio_encoder.h"
#include "audio_decoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "object.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"

#define TAG "Audio Processor"

#define AUDIO_PROCESSOR_TASK_STACK_SIZE (4 * 1024)
#define AUDIO_PROCESSOR_TASK_PRIORITY 5
#define AUDIO_PROCESSOR_TASK_CORE_ID (0)

struct audio_processor
{
    audio_sr_t *sr;
    audio_encoder_t *encoder;
    audio_decoder_t *decoder;

    RingbufHandle_t enc_input;
    RingbufHandle_t enc_output;
    RingbufHandle_t dec_input;
    RingbufHandle_t dec_output;

    bool is_running;
};

static void audio_processor_play_task(void *arg)
{
    audio_processor_t *audio_processor = (audio_processor_t *)arg;
    bsp_board_t *board = bsp_board_get_instance();
    while (1)
    {
        size_t size_read = 0;
        void *buf = xRingbufferReceiveUpTo(audio_processor->dec_output, &size_read, portMAX_DELAY, 2048);
        esp_codec_dev_write(board->codec_dev, buf, size_read);
        vRingbufferReturnItem(audio_processor->dec_output, buf);
    }
}

audio_processor_t *audio_processor_create(void)
{
    audio_processor_t *audio_processor = (audio_processor_t *)malloc_zeroed(sizeof(audio_processor_t));
    audio_processor->sr = audio_sr_create();
    audio_processor->encoder = audio_encoder_create(BSP_CODEC_SAMPLE_RATE, 1);
    audio_processor->decoder = audio_decoder_create(BSP_CODEC_SAMPLE_RATE, 2);

    audio_processor->enc_input = xRingbufferCreateWithCaps(20480, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    audio_processor->enc_output = xRingbufferCreateWithCaps(2560, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    audio_processor->dec_input = xRingbufferCreateWithCaps(5120, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    audio_processor->dec_output = xRingbufferCreateWithCaps(40960, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);

    audio_sr_set_output_buffer(audio_processor->sr, audio_processor->enc_input);
    audio_encoder_set_buffer(audio_processor->encoder, audio_processor->enc_input, audio_processor->enc_output);
    audio_decoder_set_buffer(audio_processor->decoder, audio_processor->dec_input, audio_processor->dec_output);

    return audio_processor;
}

void audio_processor_destroy(audio_processor_t *audio_processor)
{
    vRingbufferDelete(audio_processor->enc_input);
    vRingbufferDelete(audio_processor->enc_output);
    vRingbufferDelete(audio_processor->dec_input);
    vRingbufferDelete(audio_processor->dec_output);

    audio_encoder_destroy(audio_processor->encoder);
    audio_decoder_destroy(audio_processor->decoder);
    audio_sr_destroy(audio_processor->sr);

    free(audio_processor);
}

void audio_processor_start(audio_processor_t *audio_processor)
{
    audio_processor->is_running = true;
    audio_sr_start(audio_processor->sr);
    audio_encoder_start(audio_processor->encoder);
    audio_decoder_start(audio_processor->decoder);

    xTaskCreatePinnedToCoreWithCaps(audio_processor_play_task, "play_task",
                                    AUDIO_PROCESSOR_TASK_STACK_SIZE, audio_processor,
                                    AUDIO_PROCESSOR_TASK_PRIORITY, NULL,
                                    AUDIO_PROCESSOR_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

void audio_processor_stop(audio_processor_t *audio_processor)
{
    audio_processor->is_running = false;
    audio_sr_stop(audio_processor->sr);
    audio_encoder_stop(audio_processor->encoder);
    audio_decoder_stop(audio_processor->decoder);
}

size_t audio_processor_read(audio_processor_t *audio_processor, void *buffer, size_t size)
{
    size_t size_read = 0;
    void *buf_read = xRingbufferReceive(audio_processor->enc_output, &size_read, portMAX_DELAY);
    if (!buf_read)
    {
        return 0;
    }

    if (size_read > size)
    {
        ESP_LOGW(TAG, "Buffer size is too small");
        size_read = size;
    }

    memcpy(buffer, buf_read, size_read);
    vRingbufferReturnItem(audio_processor->enc_output, buf_read);

    return size_read;
}

void audio_processor_write(audio_processor_t *audio_processor, void *buffer, size_t size)
{
    xRingbufferSend(audio_processor->dec_input, buffer, size, portMAX_DELAY);
}

void audio_processor_register_event_cb(audio_processor_t *audio_processor, esp_event_handler_t callback, void *arg)
{
    audio_sr_register_event_cb(audio_processor->sr, callback, arg);
}

void audio_processor_set_vad_state(audio_processor_t *audio_processor, bool state)
{
    audio_sr_set_vad_state(audio_processor->sr, state);
}
