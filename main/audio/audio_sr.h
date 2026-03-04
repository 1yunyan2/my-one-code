#pragma once

#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

/**
 * @brief 语音识别（Speech Recognition）句柄类型定义
 */
typedef struct audio_sr audio_sr_t;

/**
 * @brief 创建语音识别实例
 * @return 返回创建的语音识别句柄
 */
audio_sr_t *audio_sr_create(void);

/**
 * @brief 销毁语音识别实例
 * @param sr 语音识别句柄
 */
void audio_sr_destroy(audio_sr_t *sr);

/**
 * @brief 启动语音识别任务
 * @param sr 语音识别句柄
 */
void audio_sr_start(audio_sr_t *sr);

/**
 * @brief 停止语音识别任务
 * @param sr 语音识别句柄
 */
void audio_sr_stop(audio_sr_t *sr);

/**
 * @brief 注册语音识别事件回调函数
 * @param sr 语音识别句柄
 * @param callback 事件回调函数
 * @param arg 回调函数的用户参数
 */
void audio_sr_register_event_cb(audio_sr_t *sr, esp_event_handler_t callback, void *arg);

/**
 * @brief 设置语音识别输出缓冲区
 * @param sr 语音识别句柄
 * @param output_buffer 输出环形缓冲区句柄
 */
void audio_sr_set_output_buffer(audio_sr_t *sr, RingbufHandle_t output_buffer);

/**
 * @brief 设置语音活动检测（VAD）状态
 * @param sr 语音识别句柄
 * @param state true-启用 VAD，false-禁用 VAD
 */
void audio_sr_set_vad_state(audio_sr_t *sr, bool state);
