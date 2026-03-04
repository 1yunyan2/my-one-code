/**
 * @file audio_processor.h
 * @brief 音频处理器模块头文件
 * 
 * 负责管理整个音频处理链路，包括：
 * - 语音识别（SR）唤醒和 VAD 检测
 * - 音频编码（OPUS）
 * - 音频解码（OPUS）
 * - 环形缓冲区管理
 */

#pragma once

#include <stddef.h>
#include "esp_event.h"

/**
 * @brief 音频处理器事件类型枚举
 */
typedef enum
{
    AUDIO_PROCESSOR_EVENT_WAKEUP,   ///< 检测到唤醒词事件
    AUDIO_PROCESSOR_EVENT_SPEECH,   ///< 检测到用户开始说话事件
    AUDIO_PROCESSOR_EVENT_SILENCE,  ///< 检测到用户说话结束（静音）事件
} audio_processor_event_t;

/// 音频处理器结构体前向声明
typedef struct audio_processor audio_processor_t;

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
audio_processor_t *audio_processor_create(void);

/**
 * @brief 销毁音频处理器实例
 * 
 * 释放音频处理器及其子模块占用的所有资源
 * 
 * @param audio_processor 要销毁的音频处理器实例
 */
void audio_processor_destroy(audio_processor_t *audio_processor);

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
void audio_processor_start(audio_processor_t *audio_processor);

/**
 * @brief 停止音频处理器
 * 
 * 停止所有内部任务和数据处理
 * 
 * @param audio_processor 音频处理器实例
 */
void audio_processor_stop(audio_processor_t *audio_processor);

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
size_t audio_processor_read(audio_processor_t *audio_processor, void *buffer, size_t size);

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
void audio_processor_write(audio_processor_t *audio_processor, void *buffer, size_t size);

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
void audio_processor_register_event_cb(audio_processor_t *audio_processor, esp_event_handler_t callback, void *arg);

/**
 * @brief 设置 VAD（语音活动检测）状态
 * 
 * 启用或禁用语音活动检测功能。
 * 在设备播放语音时通常禁用 VAD 以避免误触发。
 * 
 * @param audio_processor 音频处理器实例
 * @param state true-启用 VAD, false-禁用 VAD
 */
void audio_processor_set_vad_state(audio_processor_t *audio_processor, bool state);
