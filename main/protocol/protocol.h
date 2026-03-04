/**
 * @file protocol.h
 * @brief WebSocket 通信协议模块头文件
 * 
 * 实现与服务器的 WebSocket 通信协议，处理各类事件：
 * - 连接管理（建立、断开、重连）
 * - 消息收发（JSON 格式）
 * - 音频流传输（二进制）
 * - IoT 设备控制
 * 
 * 采用事件驱动模型，通过回调函数通知上层应用
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_event.h"
#include "cJSON.h"

/**
 * @brief 协议事件类型枚举
 * 
 * 定义所有可能发生的协议事件及其数据类型
 */
typedef enum
{
    PROTOCOL_EVENT_CONNECTED,          ///< 连接成功事件，event_data: NULL
    PROTOCOL_EVENT_DISCONNECTED,       ///< 连接断开事件，event_data: NULL
    PROTOCOL_EVENT_HELLO,              ///< 收到服务器 Hello 响应，event_data: NULL
    PROTOCOL_EVENT_STT,                ///< 收到语音识别结果，event_data: char* (识别文本)
    PROTOCOL_EVENT_LLM,                ///< 收到大模型回复，event_data: char* (回复内容)
    PROTOCOL_EVENT_TTS_START,          ///< TTS 开始播放，event_data: NULL
    PROTOCOL_EVENT_TTS_SENTENCE_START, ///< TTS 句子开始，event_data: char* (当前句子)
    PROTOCOL_EVENT_TTS_STOP,           ///< TTS 播放停止，event_data: NULL
    PROTOCOL_EVENT_AUDIO,              ///< 收到音频数据，event_data: binary_data_t* (音频数据指针)
    PROTOCOL_EVENT_IOT,                ///< 收到 IoT 控制指令，event_data: cJSON* (命令 JSON)
} protocol_event_t;

/**
 * @brief 监听模式类型枚举
 * 
 * 定义语音监听的三种模式
 */
typedef enum
{
    PROTOCOL_LISTEN_TYPE_AUTO,      ///< 自动监听：检测到语音自动开始
    PROTOCOL_LISTEN_TYPE_MANUAL,    ///< 手动监听：需要显式启动/停止
    PROTOCOL_LISTEN_TYPE_REALTIME,  ///< 实时监听：持续监听并上传
} protocol_listen_type_t;

/**
 * @brief IoT 消息类型枚举
 * 
 * 定义 IoT 设备消息的两种类型
 */
typedef enum
{
    MESSAGE_TYPE_DESCRIPTOR,  ///< 设备描述符：描述设备能力和属性
    MESSAGE_TYPE_STATE,       ///< 设备状态：上报设备当前状态
} protocol_iot_message_type_t;

/**
 * @brief 二进制数据结构体
 * 
 * 用于封装二进制数据指针和大小
 */
typedef struct
{
    void *ptr;     ///< 数据指针
    size_t size;   ///< 数据大小（字节数）
} binary_data_t;

/// 协议结构体前向声明
typedef struct protocol protocol_t;

/**
 * @brief 创建协议实例
 * 
 * 初始化 WebSocket 客户端并配置连接参数
 * 
 * @param url WebSocket 服务器 URL
 * @param token 认证令牌（Bearer Token）
 * @return protocol_t* 新创建的协议实例
 */
protocol_t *protocol_create(char *url, char *token);

/**
 * @brief 销毁协议实例
 * 
 * 关闭 WebSocket 连接并释放资源
 * 
 * @param protocol 要销毁的协议实例
 */
void protocol_destroy(protocol_t *protocol);

// ==================== WebSocket 连接管理接口 ====================

/**
 * @brief 建立 WebSocket 连接
 * 
 * @param protocol 协议实例
 */
void protocol_connect(protocol_t *protocol);

/**
 * @brief 断开 WebSocket 连接
 * 
 * @param protocol 协议实例
 */
void protocol_disconnect(protocol_t *protocol);

/**
 * @brief 检查是否已连接
 * 
 * @param protocol 协议实例
 * @return bool true-已连接，false-未连接
 */
bool protocol_is_connected(protocol_t *protocol);

// ==================== 协议消息发送接口 ====================

/**
 * @brief 发送 Hello 握手消息
 * 
 * 连接成功后向服务器发送问候消息
 * 
 * @param protocol 协议实例
 */
void protocol_send_hello(protocol_t *protocol);

/**
 * @brief 发送唤醒词
 * 
 * 告知服务器用户使用的唤醒词
 * 
 * @param protocol 协议实例
 * @param wake_word 唤醒词字符串
 */
void protocol_send_wake_word(protocol_t *protocol, char *wake_word);

/**
 * @brief 开始监听
 * 
 * 通知服务器开始接收语音数据
 * 
 * @param protocol 协议实例
 * @param type 监听模式（自动/手动/实时）
 */
void protocol_send_start_listening(protocol_t *protocol, protocol_listen_type_t type);

/**
 * @brief 停止监听
 * 
 * 通知服务器停止接收语音数据
 * 
 * @param protocol 协议实例
 */
void protocol_send_stop_listening(protocol_t *protocol);

/**
 * @brief 发送音频数据
 * 
 * 将编码后的 OPUS 音频数据上传到服务器
 * 
 * @param protocol 协议实例
 * @param data 音频数据指针（包含数据和大小）
 */
void protocol_send_audio_data(protocol_t *protocol, binary_data_t *data);

/**
 * @brief 中断对话
 * 
 * 打断服务器当前的 TTS 播放，用于抢话功能
 * 
 * @param protocol 协议实例
 */
void protocol_send_abort_speaking(protocol_t *protocol);

/**
 * @brief 发送 IoT 消息
 * 
 * 上报设备描述或状态信息
 * 
 * @param protocol 协议实例
 * @param type 消息类型（描述符/状态）
 * @param json IoT 消息 JSON 对象
 */
void protocol_send_iot(protocol_t *protocol, protocol_iot_message_type_t type, cJSON *json);

/**
 * @brief 注册事件回调函数
 * 
 * 注册用于接收协议事件的回调函数
 * 
 * @param protocol 协议实例
 * @param callback 事件回调函数指针
 * @param handler_args 传递给回调函数的参数
 */
void protocol_register_callback(protocol_t *protocol, esp_event_handler_t callback, void *handler_args);
