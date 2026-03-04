/**
 * @file audio_encoder.c
 * @brief 音频编码器模块实现
 * 
 * 负责将原始 PCM 音频数据编码为 OPUS 压缩格式，减少网络传输数据量
 */

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

// 编码器任务配置：绑定到 CPU 核心 0，使用 SPIRAM
#define AUDIO_ENCODER_TASK_CORE_ID 0
#define AUDIO_ENCODER_TASK_STACK_SIZE 32768
#define AUDIO_ENCODER_TASK_PRIORITY 5

/**
 * @brief 音频编码器数据结构
 */
struct audio_encoder
{
    RingbufHandle_t input_buffer;   ///< 输入环形缓冲区句柄
    RingbufHandle_t output_buffer;  ///< 输出环形缓冲区句柄
    esp_audio_enc_handle_t enc;     ///< 编码器句柄

    bool is_running;                ///< 运行状态标志
};

/**
 * @brief 编码器任务：从输入缓冲区读取 PCM 数据，编码后写入输出缓冲区
 * @param arg 用户参数，指向 audio_encoder 结构体
 */
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

    // 初始化输入帧结构
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = in_buf,
        .len = in_frame_size,
    };

    // 初始化输出帧结构
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
            // 当前帧数据不足，继续读取
            continue;
        }

        // 将缓存重置，准备接收下一帧数据
        in_buf = in_frame.buffer;
        in_frame_size = in_frame.len;

        // 执行 OPUS 编码
        esp_audio_enc_process(audio_encoder->enc, &in_frame, &out_frame);

        // 将编码后的数据写入输出缓冲区
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

/**
 * @brief 创建音频编码器实例
 * @param sample_rate 采样率（单位：Hz）
 * @param channels 声道数
 * @return 返回创建的编码器句柄
 */
audio_encoder_t *audio_encoder_create(int sample_rate, int channels)
{
    audio_encoder_t *audio_encoder = (audio_encoder_t *)malloc_zeroed(sizeof(audio_encoder_t));

    // 注册 OPUS 编码器
    ESP_ERROR_CHECK(esp_opus_enc_register());

    // 配置 OPUS 编码参数
    esp_opus_enc_config_t opus_config = {
        .sample_rate = sample_rate,
        .bits_per_sample = BSP_CODEC_BITS_PER_SAMPLE,
        .channel = channels,
        .bitrate = 32000,                          // 比特率：32kbps
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,  // 帧时长：60ms
        .complexity = 0,                           // 复杂度：最低
        .application_mode = ESP_OPUS_ENC_APPLICATION_VOIP,    // 应用场景：VoIP
        .enable_fec = false,                       // 禁用前向纠错
        .enable_dtx = false,                       // 禁用不连续传输
        .enable_vbr = false,                       // 禁用可变比特率
    };
    esp_audio_enc_config_t enc_config = {
        .cfg = &opus_config,
        .cfg_sz = sizeof(esp_opus_enc_config_t),
        .type = ESP_AUDIO_TYPE_OPUS,
    };
    // 打开编码器
    ESP_ERROR_CHECK(esp_audio_enc_open(&enc_config, &audio_encoder->enc));

    return audio_encoder;
}

/**
 * @brief 销毁音频编码器实例
 * @param audio_encoder 编码器句柄
 */
void audio_encoder_destroy(audio_encoder_t *audio_encoder)
{
    // 关闭编码器
    esp_audio_enc_close(audio_encoder->enc);
    // 注销 OPUS 编码器
    esp_audio_dec_unregister(ESP_AUDIO_TYPE_OPUS);
    free(audio_encoder);
}

/**
 * @brief 设置编码器的输入和输出缓冲区
 * @param audio_encoder 编码器句柄
 * @param input_buffer 输入环形缓冲区句柄
 * @param output_buffer 输出环形缓冲区句柄
 */
void audio_encoder_set_buffer(audio_encoder_t *audio_encoder, RingbufHandle_t input_buffer, RingbufHandle_t output_buffer)
{
    audio_encoder->input_buffer = input_buffer;
    audio_encoder->output_buffer = output_buffer;
}

/**
 * @brief 启动编码器任务
 * @param audio_encoder 编码器句柄
 */
void audio_encoder_start(audio_encoder_t *audio_encoder)
{
    if (!audio_encoder->input_buffer || !audio_encoder->output_buffer)
    {
        ESP_LOGW(TAG, "Input or output buffer not set");
        return;
    }

    audio_encoder->is_running = true;
    // 创建编码器任务：绑定到 CPU 核心 0，使用 SPIRAM
    xTaskCreatePinnedToCoreWithCaps(audio_encoder_task, "encoder_task",
                                    AUDIO_ENCODER_TASK_STACK_SIZE, audio_encoder,
                                    AUDIO_ENCODER_TASK_PRIORITY, NULL,
                                    AUDIO_ENCODER_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

/**
 * @brief 停止编码器任务
 * @param audio_encoder 编码器句柄
 */
void audio_encoder_stop(audio_encoder_t *audio_encoder)
{
    audio_encoder->is_running = false;
    vTaskDelay(pdMS_TO_TICKS(200));
}
