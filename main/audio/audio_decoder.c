#include "audio_decoder.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec_reg.h"
#include "object.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"

#define TAG "[AP] Decoder"

#define AUDIO_DECODER_TASK_CORE_ID 0
#define AUDIO_DECODER_TASK_STACK_SIZE 32768
#define AUDIO_DECODER_TASK_PRIORITY 5

struct audio_decoder
{
    RingbufHandle_t input_buffer;
    RingbufHandle_t output_buffer;
    esp_audio_dec_handle_t dec;

    int sample_rate;
    int channels;

    bool is_running;
};

void audio_decoder_task(void *arg)
{
    audio_decoder_t *audio_decoder = (audio_decoder_t *)arg;

    size_t out_buffer_size = audio_decoder->sample_rate * audio_decoder->channels * 2 / 1000 * 60;
    void *out_buffer = malloc_zeroed(out_buffer_size);
    esp_audio_dec_out_frame_t out_frame = {
        .buffer = out_buffer,
        .len = out_buffer_size,
    };
    while (audio_decoder->is_running)
    {
        // 读取opus数据
        size_t size_read = 0;
        void *buf_read = xRingbufferReceive(audio_decoder->input_buffer, &size_read, pdMS_TO_TICKS(100));
        if (!buf_read)
        {
            continue;
        }

        esp_audio_dec_in_raw_t in_frame = {
            .buffer = buf_read,
            .len = size_read,
        };

        // 解码
        esp_audio_err_t ret = esp_audio_dec_process(audio_decoder->dec, &in_frame, &out_frame);
        vRingbufferReturnItem(audio_decoder->input_buffer, buf_read);
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH)
        {
            // 缓冲区不够大，警告
            ESP_LOGW(TAG, "Output buffer not enough");
        }
        // 输出pcm数据
        if (ret != ESP_OK)
        {
            continue;
        }
        BaseType_t buf_ret = xRingbufferSend(audio_decoder->output_buffer, out_frame.buffer, out_frame.decoded_size, 0);
        if (buf_ret != pdTRUE)
        {
            ESP_LOGW(TAG, "Output buffer full");
        }
    }
    vTaskDelete(NULL);
}

audio_decoder_t *audio_decoder_create(int sample_rate, int channels)
{
    audio_decoder_t *audio_decoder = (audio_decoder_t *)malloc_zeroed(sizeof(audio_decoder_t));

    audio_decoder->sample_rate = sample_rate;
    audio_decoder->channels = channels;

    // 初始化解码器句柄
    ESP_ERROR_CHECK(esp_opus_dec_register());
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate = sample_rate,
        .channel = channels,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,
        .self_delimited = false,
    };
    esp_audio_dec_cfg_t dec_cfg = {
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(esp_opus_dec_cfg_t),
        .type = ESP_AUDIO_TYPE_OPUS,
    };
    ESP_ERROR_CHECK(esp_audio_dec_open(&dec_cfg, &audio_decoder->dec));
    return audio_decoder;
}

void audio_decoder_destroy(audio_decoder_t *audio_decoder)
{
    esp_audio_dec_close(audio_decoder->dec);
    esp_audio_dec_unregister(ESP_AUDIO_TYPE_OPUS);
    free(audio_decoder);
}

void audio_decoder_set_buffer(audio_decoder_t *audio_decoder, RingbufHandle_t input_buffer, RingbufHandle_t output_buffer)
{
    audio_decoder->input_buffer = input_buffer;
    audio_decoder->output_buffer = output_buffer;
}

void audio_decoder_start(audio_decoder_t *audio_decoder)
{
    if (!audio_decoder->input_buffer || !audio_decoder->output_buffer)
    {
        ESP_LOGW(TAG, "Input or output buffer not set");
        return;
    }

    audio_decoder->is_running = true;
    xTaskCreatePinnedToCoreWithCaps(audio_decoder_task, "decoder_task",
                                    AUDIO_DECODER_TASK_STACK_SIZE, audio_decoder,
                                    AUDIO_DECODER_TASK_PRIORITY, NULL,
                                    AUDIO_DECODER_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

void audio_decoder_stop(audio_decoder_t *audio_decoder)
{
    audio_decoder->is_running = false;
    vTaskDelay(pdMS_TO_TICKS(200));
}
