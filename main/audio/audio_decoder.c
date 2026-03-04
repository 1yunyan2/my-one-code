/**
 * @file audio_decoder.c
 * @brief 音频解码器模块实现
 * 
 * 负责将接收到的 OPUS 压缩音频数据解码为 PCM 格式用于播放
 */

#include "audio_decoder.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec_reg.h"
#include "object.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"

#define TAG "[AP] Decoder"

// 解码器任务配置：绑定到 CPU 核心 0，使用 SPIRAM
#define AUDIO_DECODER_TASK_CORE_ID 0
#define AUDIO_DECODER_TASK_STACK_SIZE 32768
#define AUDIO_DECODER_TASK_PRIORITY 5

/**
 * @brief 音频解码器数据结构
 */
struct audio_decoder
{
    RingbufHandle_t input_buffer;   ///< 输入环形缓冲区句柄
    RingbufHandle_t output_buffer;  ///< 输出环形缓冲区句柄
    esp_audio_dec_handle_t dec;     ///< 解码器句柄

    int sample_rate;                ///< 采样率（单位：Hz）
    int channels;                   ///< 声道数

    bool is_running;                ///< 运行状态标志
};

/**
 * @brief 解码器任务：从输入缓冲区读取 OPUS 数据，解码后写入输出缓冲区
 * @param arg 用户参数，指向 audio_decoder 结构体
 */
void audio_decoder_task(void *arg)
{
    audio_decoder_t *audio_decoder = (audio_decoder_t *)arg;

    // 计算输出缓冲区大小：采样率 * 声道数 * 每样本字节数 * 帧时长 (ms) / 1000
    size_t out_buffer_size = audio_decoder->sample_rate * audio_decoder->channels * 2 / 1000 * 60;
    void *out_buffer = malloc_zeroed(out_buffer_size);
    esp_audio_dec_out_frame_t out_frame = {
        .buffer = out_buffer,
        .len = out_buffer_size,
    };
    while (audio_decoder->is_running)
    {
        // 读取 OPUS 压缩数据
        size_t size_read = 0;
        void *buf_read = xRingbufferReceive(audio_decoder->input_buffer, &size_read, pdMS_TO_TICKS(100));
        if (!buf_read)
        {
            continue;
        }

        // 初始化输入帧结构
        esp_audio_dec_in_raw_t in_frame = {
            .buffer = buf_read,
            .len = size_read,
        };

        // 执行 OPUS 解码
        esp_audio_err_t ret = esp_audio_dec_process(audio_decoder->dec, &in_frame, &out_frame);
        vRingbufferReturnItem(audio_decoder->input_buffer, buf_read);
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH)
        {
            // 缓冲区不够大，警告
            ESP_LOGW(TAG, "Output buffer not enough");
        }
        // 输出 PCM 数据
        if (ret != ESP_OK)
        {
            continue;
        }
        // 将解码后的 PCM 数据写入输出缓冲区
        BaseType_t buf_ret = xRingbufferSend(audio_decoder->output_buffer, out_frame.buffer, out_frame.decoded_size, 0);
        if (buf_ret != pdTRUE)
        {
            ESP_LOGW(TAG, "Output buffer full");
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief 创建音频解码器实例
 * @param sample_rate 采样率（单位：Hz）
 * @param channels 声道数
 * @return 返回创建的解码器句柄
 */
audio_decoder_t *audio_decoder_create(int sample_rate, int channels)
{
    audio_decoder_t *audio_decoder = (audio_decoder_t *)malloc_zeroed(sizeof(audio_decoder_t));

    audio_decoder->sample_rate = sample_rate;
    audio_decoder->channels = channels;

    // 注册 OPUS 解码器
    ESP_ERROR_CHECK(esp_opus_dec_register());
    
    // 配置 OPUS 解码参数
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate = sample_rate,
        .channel = channels,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,  // 帧时长：60ms
        .self_delimited = false,                               // 不自定界
    };
    esp_audio_dec_cfg_t dec_cfg = {
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(esp_opus_dec_cfg_t),
        .type = ESP_AUDIO_TYPE_OPUS,
    };
    // 打开解码器
    ESP_ERROR_CHECK(esp_audio_dec_open(&dec_cfg, &audio_decoder->dec));
    return audio_decoder;
}

/**
 * @brief 销毁音频解码器实例
 * @param audio_decoder 解码器句柄
 */
void audio_decoder_destroy(audio_decoder_t *audio_decoder)
{
    // 关闭解码器
    esp_audio_dec_close(audio_decoder->dec);
    // 注销 OPUS 解码器
    esp_audio_dec_unregister(ESP_AUDIO_TYPE_OPUS);
    free(audio_decoder);
}

/**
 * @brief 设置解码器的输入和输出缓冲区
 * @param audio_decoder 解码器句柄
 * @param input_buffer 输入环形缓冲区句柄
 * @param output_buffer 输出环形缓冲区句柄
 */
void audio_decoder_set_buffer(audio_decoder_t *audio_decoder, RingbufHandle_t input_buffer, RingbufHandle_t output_buffer)
{
    audio_decoder->input_buffer = input_buffer;
    audio_decoder->output_buffer = output_buffer;
}

/**
 * @brief 启动解码器任务
 * @param audio_decoder 解码器句柄
 */
void audio_decoder_start(audio_decoder_t *audio_decoder)
{
    if (!audio_decoder->input_buffer || !audio_decoder->output_buffer)
    {
        ESP_LOGW(TAG, "Input or output buffer not set");
        return;
    }

    audio_decoder->is_running = true;
    // 创建解码器任务：绑定到 CPU 核心 0，使用 SPIRAM
    xTaskCreatePinnedToCoreWithCaps(audio_decoder_task, "decoder_task",
                                    AUDIO_DECODER_TASK_STACK_SIZE, audio_decoder,
                                    AUDIO_DECODER_TASK_PRIORITY, NULL,
                                    AUDIO_DECODER_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

/**
 * @brief 停止解码器任务
 * @param audio_decoder 解码器句柄
 */
void audio_decoder_stop(audio_decoder_t *audio_decoder)
{
    audio_decoder->is_running = false;
    vTaskDelay(pdMS_TO_TICKS(200));
}
