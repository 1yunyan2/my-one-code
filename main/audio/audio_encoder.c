#include "audio_encoder.h"
#include "esp_audio_enc.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_enc_reg.h"
#include "object.h"
#include "bsp/bsp_board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define TAG "[AP] Encoder"

#define AUDIO_ENCODER_TASK_CORE_ID 0
#define AUDIO_ENCODER_TASK_STACK_SIZE 32768
#define AUDIO_ENCODER_TASK_PRIORITY 5

struct audio_encoder
{
    RingbufHandle_t input_buffer;
    RingbufHandle_t output_buffer;
    esp_audio_enc_handle_t enc;

    bool is_running;
};

void audio_encoder_task(void *arg)
{
    audio_encoder_t *audio_encoder = (audio_encoder_t *)arg;
    // 获取输入和输出帧大小
    int in_frame_size = 0, out_frame_size = 0;
    esp_audio_enc_get_frame_size(audio_encoder->enc, &in_frame_size, &out_frame_size);

    // 给输入输出帧分配内存
    void *in_buf = malloc_zeroed(in_frame_size);
    assert(in_buf);
    void *out_buf = malloc_zeroed(out_frame_size);
    assert(out_buf);

    esp_audio_enc_in_frame_t in_frame = {
        .buffer = in_buf,
        .len = in_frame_size,
    };

    esp_audio_enc_out_frame_t out_frame = {
        .buffer = out_buf,
        .len = out_frame_size,
    };

    while (audio_encoder->is_running)
    {
        // 从输入缓冲区读取数据
        size_t size_read = 0;
        void *buf_read = xRingbufferReceiveUpTo(audio_encoder->input_buffer, &size_read, pdMS_TO_TICKS(100), in_frame_size);
        if (!buf_read)
        {
            continue;
        }
        memcpy(in_buf, buf_read, size_read);
        vRingbufferReturnItem(audio_encoder->input_buffer, buf_read);
        in_frame_size -= size_read;
        in_buf += size_read;
        if (in_frame_size > 0)
        {
            continue;
        }

        // 将缓存重置，准备接收下一帧数据
        in_buf = in_frame.buffer;
        in_frame_size = in_frame.len;

        // 编码
        esp_audio_enc_process(audio_encoder->enc, &in_frame, &out_frame);

        // 写入输出缓冲区
        BaseType_t ret = xRingbufferSend(audio_encoder->output_buffer, out_frame.buffer, out_frame.encoded_bytes, 0);
        if (ret == pdFAIL)
        {
            ESP_LOGW(TAG, "Failed to write to output buffer");
        }
    }
    free(in_frame.buffer);
    free(out_frame.buffer);
    vTaskDelete(NULL);
}

audio_encoder_t *audio_encoder_create(int sample_rate, int channels)
{
    audio_encoder_t *audio_encoder = (audio_encoder_t *)malloc_zeroed(sizeof(audio_encoder_t));

    // 初始化enc
    ESP_ERROR_CHECK(esp_opus_enc_register());

    esp_opus_enc_config_t opus_config = {
        .sample_rate = sample_rate,
        .bits_per_sample = BSP_CODEC_BITS_PER_SAMPLE,
        .channel = channels,
        .bitrate = 32000,
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .complexity = 0,
        .application_mode = ESP_OPUS_ENC_APPLICATION_VOIP,
        .enable_fec = false,
        .enable_dtx = false,
        .enable_vbr = false,
    };
    esp_audio_enc_config_t enc_config = {
        .cfg = &opus_config,
        .cfg_sz = sizeof(esp_opus_enc_config_t),
        .type = ESP_AUDIO_TYPE_OPUS,
    };
    ESP_ERROR_CHECK(esp_audio_enc_open(&enc_config, &audio_encoder->enc));

    return audio_encoder;
}

void audio_encoder_destroy(audio_encoder_t *audio_encoder)
{
    esp_audio_enc_close(audio_encoder->enc);
    esp_audio_enc_unregister(ESP_AUDIO_TYPE_OPUS);
    free(audio_encoder);
}

void audio_encoder_set_buffer(audio_encoder_t *audio_encoder, RingbufHandle_t input_buffer, RingbufHandle_t output_buffer)
{
    audio_encoder->input_buffer = input_buffer;
    audio_encoder->output_buffer = output_buffer;
}

void audio_encoder_start(audio_encoder_t *audio_encoder)
{
    if (!audio_encoder->input_buffer || !audio_encoder->output_buffer)
    {
        ESP_LOGW(TAG, "Input or output buffer not set");
        return;
    }

    audio_encoder->is_running = true;
    xTaskCreatePinnedToCoreWithCaps(audio_encoder_task, "encoder_task",
                                    AUDIO_ENCODER_TASK_STACK_SIZE, audio_encoder,
                                    AUDIO_ENCODER_TASK_PRIORITY, NULL,
                                    AUDIO_ENCODER_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

void audio_encoder_stop(audio_encoder_t *audio_encoder)
{
    audio_encoder->is_running = false;
    vTaskDelay(pdMS_TO_TICKS(200));
}
