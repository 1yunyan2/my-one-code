/**
 * @file audio_sr.c
 * @brief 语音识别（Speech Recognition）模块实现
 * 
 * 负责音频采集、唤醒词检测、语音活动检测（VAD）功能
 */

#include "audio_sr.h"
#include "esp_afe_sr_models.h"
#include "object.h"
#include "bsp/bsp_board.h"
#include "audio_processor.h"

// Feed 任务配置：负责从 Codec 读取音频数据并输入到 SR 模型
#define FEED_TASK_CORE_ID 1
#define FEED_TASK_STACK_SIZE 4096
#define FEED_TASK_PRIORITY 5

// Fetch 任务配置：负责从 SR 模型获取处理结果
#define FETCH_TASK_CORE_ID 1
#define FETCH_TASK_STACK_SIZE 4096
#define FETCH_TASK_PRIORITY 5

// 定义音频 SR 事件基类
ESP_EVENT_DEFINE_BASE(AUDIO_SR_EVENT);

/**
 * @brief 语音识别数据结构
 */
struct audio_sr
{
    const esp_afe_sr_iface_t *afe_handle;  ///< SR 算法句柄
    esp_afe_sr_data_t *afe_data;           ///< SR 算法实例数据

    RingbufHandle_t output_buffer;         ///< 输出环形缓冲区

    // event_loop
    esp_event_loop_handle_t event_loop;    ///< 事件循环句柄

    vad_state_t last_state;                ///< 上一次 VAD 状态

    bool is_running;                       ///< 运行状态标志
};

/**
 * @brief Feed 任务：从 Codec 读取音频数据并输入到 SR 模型
 * @param arg 用户参数，指向 audio_sr 结构体
 */
static void feed_task(void *arg)
{
    audio_sr_t *sr = (audio_sr_t *)arg;
    const esp_afe_sr_iface_t *afe_handle = sr->afe_handle;
    esp_afe_sr_data_t *afe_data = sr->afe_data;

    // 获取要输入的声音的采样数量
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // 获取声道数量
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    // 计算一次 feed 的片段大小
    int feed_size = feed_chunksize * feed_nch * sizeof(int16_t);
    // 申请内存
    int16_t *feed_buff = (int16_t *)malloc_zeroed(feed_size);

    bsp_board_t *board = bsp_board_get_instance();
    while (sr->is_running)
    {
        // 从 Codec 设备读取音频数据
        esp_codec_dev_read(board->codec_dev, feed_buff, feed_size);
        // 将音频数据输入到 SR 模型
        sr->afe_handle->feed(sr->afe_data, feed_buff);
    }
    free(feed_buff);
    vTaskDelete(NULL);
}

/**
 * @brief Fetch 任务：从 SR 模型获取处理结果并分发事件
 * @param arg 用户参数，指向 audio_sr 结构体
 */
static void fetch_task(void *arg)
{
    audio_sr_t *sr = (audio_sr_t *)arg;
    while (sr->is_running)
    {
        // 从 SR 模型获取处理结果
        afe_fetch_result_t *result = sr->afe_handle->fetch(sr->afe_data);

        if (result->wakeup_state == WAKENET_DETECTED)
        {
            // 触发唤醒词检测事件
            esp_event_post_to(sr->event_loop, AUDIO_SR_EVENT,
                              AUDIO_PROCESSOR_EVENT_WAKEUP, NULL, 0, 0);
        }

        if (result->vad_state != sr->last_state)
        {
            // VAD 状态发生变化，发送相应事件
            sr->last_state = result->vad_state;
            esp_event_post_to(sr->event_loop, AUDIO_SR_EVENT,
                              result->vad_state ? AUDIO_PROCESSOR_EVENT_SPEECH : AUDIO_PROCESSOR_EVENT_SILENCE, NULL, 0, 0);
        }

        if (sr->last_state == VAD_SPEECH)
        {
            // 检测到语音活动，输出音频数据
            if (result->vad_cache_size > 0)
            {
                // 先发送 VAD 缓存数据
                xRingbufferSend(sr->output_buffer, result->vad_cache, result->vad_cache_size, 0);
            }

            // 输出处理后的音频数据到环形缓冲区
            xRingbufferSend(sr->output_buffer, result->data, result->data_size, 0);
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief 创建语音识别实例
 * @return 返回创建的语音识别句柄
 */
audio_sr_t *audio_sr_create(void)
{
    audio_sr_t *sr = malloc_zeroed(sizeof(audio_sr_t));

    // 初始化 SR 模型列表
    srmodel_list_t *models = esp_srmodel_init("model");
    // 初始化 AFE 配置
    afe_config_t *afe_config = afe_config_init("MR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);

    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    // 获取 SR 算法句柄
    sr->afe_handle = esp_afe_handle_from_config(afe_config);
    // 创建 SR 算法实例
    sr->afe_data = sr->afe_handle->create_from_config(afe_config);

    // 默认禁用 VAD
    sr->afe_handle->disable_vad(sr->afe_data);

    free(afe_config);

    // 创建事件循环
    esp_event_loop_args_t event_loop_config = {
        .queue_size = 10,
        .task_name = "sr_events",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0};
    ESP_ERROR_CHECK(esp_event_loop_create(&event_loop_config, &sr->event_loop));

    return sr;
}

/**
 * @brief 销毁语音识别实例
 * @param sr 语音识别句柄
 */
void audio_sr_destroy(audio_sr_t *sr)
{
    // 销毁 SR 算法实例
    sr->afe_handle->destroy(sr->afe_data);
    // 销毁事件循环
    esp_event_loop_delete(sr->event_loop);
    free(sr);
}

/**
 * @brief 启动语音识别任务
 * @param sr 语音识别句柄
 */
void audio_sr_start(audio_sr_t *sr)
{
    sr->is_running = true;
    // 创建 Feed 任务：绑定到 CPU 核心 1，使用 SPIRAM
    xTaskCreatePinnedToCoreWithCaps(feed_task, "feed_task",
                                    FEED_TASK_STACK_SIZE, sr,
                                    FEED_TASK_PRIORITY, NULL,
                                    FEED_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
    // 创建 Fetch 任务：绑定到 CPU 核心 1，使用 SPIRAM
    xTaskCreatePinnedToCoreWithCaps(fetch_task, "fetch_task",
                                    FETCH_TASK_STACK_SIZE, sr,
                                    FETCH_TASK_PRIORITY, NULL,
                                    FETCH_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

/**
 * @brief 停止语音识别任务
 * @param sr 语音识别句柄
 */
void audio_sr_stop(audio_sr_t *sr)
{
    sr->is_running = false;
    vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * @brief 注册语音识别事件回调函数
 * @param sr 语音识别句柄
 * @param callback 事件回调函数
 * @param arg 回调函数的用户参数
 */
void audio_sr_register_event_cb(audio_sr_t *sr, esp_event_handler_t callback, void *arg)
{
    // 在事件循环中注册回调
    esp_event_handler_instance_register_with(sr->event_loop, AUDIO_SR_EVENT,
                                             ESP_EVENT_ANY_ID, callback, arg, NULL);
}

/**
 * @brief 设置语音识别输出缓冲区
 * @param sr 语音识别句柄
 * @param output_buffer 输出环形缓冲区句柄
 */
void audio_sr_set_output_buffer(audio_sr_t *sr, RingbufHandle_t output_buffer)
{
    sr->output_buffer = output_buffer;
}

/**
 * @brief 设置语音活动检测（VAD）状态
 * @param sr 语音识别句柄
 * @param state true-启用 VAD，false-禁用 VAD
 */
void audio_sr_set_vad_state(audio_sr_t *sr, bool state)
{
    if (state)
    {
        // 启用 VAD
        sr->afe_handle->enable_vad(sr->afe_data);
    }
    else
    {
        // 禁用 VAD
        sr->afe_handle->disable_vad(sr->afe_data);
    }
}
