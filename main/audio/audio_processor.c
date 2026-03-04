/**
 * @file audio_processor.c
 * @brief 音频处理器核心实现
 * 
 * 负责管理整个音频处理链路，整合语音识别、编码、解码功能，
 * 通过环形缓冲区实现高效的数据流传输。
 */

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

// 音频处理器任务配置
#define AUDIO_PROCESSOR_TASK_STACK_SIZE (4 * 1024)  ///< 播放任务堆栈大小
#define AUDIO_PROCESSOR_TASK_PRIORITY 5              ///< 播放任务优先级
#define AUDIO_PROCESSOR_TASK_CORE_ID (0)             ///< 播放任务绑定的 CPU 核心

/**
 * @brief 音频处理器数据结构
 * 
 * 整合所有音频处理子模块，管理数据流和缓冲区
 */
struct audio_processor
{
    audio_sr_t *sr;           ///< 语音识别模块句柄
    audio_encoder_t *encoder; ///< 音频编码器句柄
    audio_decoder_t *decoder; ///< 音频解码器句柄

    RingbufHandle_t enc_input;   ///< 编码器输入环形缓冲区
    RingbufHandle_t enc_output;  ///< 编码器输出环形缓冲区
    RingbufHandle_t dec_input;   ///< 解码器输入环形缓冲区
    RingbufHandle_t dec_output;  ///< 解码器输出环形缓冲区

    bool is_running;             ///< 运行状态标志
};

/**
 * @brief 播放任务：从解码输出缓冲区读取 PCM 数据并发送到 Codec 播放
 * @param arg 用户参数，指向 audio_processor 结构体
 */
static void audio_processor_play_task(void *arg)
{
    audio_processor_t *audio_processor = (audio_processor_t *)arg;
    bsp_board_t *board = bsp_board_get_instance();
    while (1)
    {
        size_t size_read = 0;
        // 从解码输出缓冲区读取 PCM 数据（阻塞等待）
        void *buf = xRingbufferReceiveUpTo(audio_processor->dec_output, &size_read, portMAX_DELAY, 2048);
        // 将 PCM 数据写入 Codec 设备进行播放
        esp_codec_dev_write(board->codec_dev, buf, size_read);
        vRingbufferReturnItem(audio_processor->dec_output, buf);
    }
}

/**
 * @brief 创建音频处理器实例
 * 
 * 分配并初始化音频处理器，包括：
 * - 创建语音识别模块
 * - 创建音频编码器和解码器
 * - 配置环形缓冲区
 * 
 * @return audio_processor_t* 新创建的音频处理器实例
 */
audio_processor_t *audio_processor_create(void)
{
    audio_processor_t *audio_processor = (audio_processor_t *)malloc_zeroed(sizeof(audio_processor_t));
    // 创建语音识别模块
    audio_processor->sr = audio_sr_create();
    // 创建音频编码器（采样率：BSP_CODEC_SAMPLE_RATE，声道数：1）
    audio_processor->encoder = audio_encoder_create(BSP_CODEC_SAMPLE_RATE, 1);
    // 创建音频解码器（采样率：BSP_CODEC_SAMPLE_RATE，声道数：2）
    audio_processor->decoder = audio_decoder_create(BSP_CODEC_SAMPLE_RATE, 2);

    // 创建编码器输入缓冲区（20KB，字节缓冲，SPIRAM）
    audio_processor->enc_input = xRingbufferCreateWithCaps(20480, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    // 创建编码器输出缓冲区（2.5KB，不可分割，SPIRAM）
    audio_processor->enc_output = xRingbufferCreateWithCaps(2560, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    // 创建解码器输入缓冲区（5KB，不可分割，SPIRAM）
    audio_processor->dec_input = xRingbufferCreateWithCaps(5120, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    // 创建解码器输出缓冲区（40KB，字节缓冲，SPIRAM）
    audio_processor->dec_output = xRingbufferCreateWithCaps(40960, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);

    // 设置 SR 模块的输出缓冲区为编码器输入
    audio_sr_set_output_buffer(audio_processor->sr, audio_processor->enc_input);
    // 设置编码器的输入输出缓冲区
    audio_encoder_set_buffer(audio_processor->encoder, audio_processor->enc_input, audio_processor->enc_output);
    // 设置解码器的输入输出缓冲区
    audio_decoder_set_buffer(audio_processor->decoder, audio_processor->dec_input, audio_processor->dec_output);

    return audio_processor;
}

/**
 * @brief 销毁音频处理器实例
 * 
 * 释放音频处理器及其子模块占用的所有资源
 * 
 * @param audio_processor 要销毁的音频处理器实例
 */
void audio_processor_destroy(audio_processor_t *audio_processor)
{
    // 删除所有环形缓冲区
    vRingbufferDelete(audio_processor->enc_input);
    vRingbufferDelete(audio_processor->enc_output);
    vRingbufferDelete(audio_processor->dec_input);
    vRingbufferDelete(audio_processor->dec_output);

    // 销毁子模块
    audio_encoder_destroy(audio_processor->encoder);
    audio_decoder_destroy(audio_processor->decoder);
    audio_sr_destroy(audio_processor->sr);

    free(audio_processor);
}

/**
 * @brief 启动音频处理器
 * 
 * 启动所有内部任务：
 * - 语音识别任务
 * - 编码任务
 * - 解码任务
 * - 播放任务
 * 
 * @param audio_processor 音频处理器实例
 */
void audio_processor_start(audio_processor_t *audio_processor)
{
    audio_processor->is_running = true;
    // 启动语音识别模块
    audio_sr_start(audio_processor->sr);
    // 启动编码器
    audio_encoder_start(audio_processor->encoder);
    // 启动解码器
    audio_decoder_start(audio_processor->decoder);

    // 创建播放任务：绑定到 CPU 核心 0，使用 SPIRAM
    xTaskCreatePinnedToCoreWithCaps(audio_processor_play_task, "play_task",
                                    AUDIO_PROCESSOR_TASK_STACK_SIZE, audio_processor,
                                    AUDIO_PROCESSOR_TASK_PRIORITY, NULL,
                                    AUDIO_PROCESSOR_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

/**
 * @brief 停止音频处理器
 * 
 * 停止所有内部任务和数据处理
 * 
 * @param audio_processor 音频处理器实例
 */
void audio_processor_stop(audio_processor_t *audio_processor)
{
    audio_processor->is_running = false;
    // 停止语音识别模块
    audio_sr_stop(audio_processor->sr);
    // 停止编码器
    audio_encoder_stop(audio_processor->encoder);
    // 停止解码器
    audio_decoder_stop(audio_processor->decoder);
}

/**
 * @brief 读取 OPUS 编码的录音数据
 * 
 * 从编码输出缓冲区读取压缩后的音频数据，
 * 用于上传到服务器进行语音识别。
 * 
 * @param audio_processor 音频处理器实例
 * @param buffer 目标缓冲区指针
 * @param size 缓冲区大小
 * @return size_t 实际读取的字节数
 */
size_t audio_processor_read(audio_processor_t *audio_processor, void *buffer, size_t size)
{
    size_t size_read = 0;
    // 从编码器输出缓冲区读取 OPUS 数据
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

/**
 * @brief 写入 OPUS 编码的播放数据
 * 
 * 将接收到的 OPUS 编码音频数据写入解码输入缓冲区，
 * 用于本地播放。
 * 
 * @param audio_processor 音频处理器实例
 * @param buffer 音频数据缓冲区指针
 * @param size 音频数据大小
 */
void audio_processor_write(audio_processor_t *audio_processor, void *buffer, size_t size)
{
    // 将 OPUS 数据写入解码器输入缓冲区
    xRingbufferSend(audio_processor->dec_input, buffer, size, portMAX_DELAY);
}

/**
 * @brief 注册事件回调函数
 * 
 * 注册用于接收音频处理器事件的回调函数，
 * 包括唤醒词检测、语音开始/结束等事件。
 * 
 * @param audio_processor 音频处理器实例
 * @param callback 事件回调函数指针
 * @param arg 传递给回调函数的参数
 */
void audio_processor_register_event_cb(audio_processor_t *audio_processor, esp_event_handler_t callback, void *arg)
{
    // 在 SR 模块中注册事件回调
    audio_sr_register_event_cb(audio_processor->sr, callback, arg);
}

/**
 * @brief 设置 VAD（语音活动检测）状态
 * 
 * 启用或禁用语音活动检测功能。
 * 在设备播放语音时通常禁用 VAD 以避免误触发。
 * 
 * @param audio_processor 音频处理器实例
 * @param state true-启用 VAD, false-禁用 VAD
 */
void audio_processor_set_vad_state(audio_processor_t *audio_processor, bool state)
{
    // 设置 SR 模块的 VAD 状态
    audio_sr_set_vad_state(audio_processor->sr, state);
}
