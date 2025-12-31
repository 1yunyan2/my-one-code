#include "audio_sr.h"
#include "esp_afe_sr_models.h"
#include "object.h"
#include "bsp/bsp_board.h"
#include "audio_processor.h"

#define FEED_TASK_CORE_ID 1
#define FEED_TASK_STACK_SIZE 4096
#define FEED_TASK_PRIORITY 5
#define FETCH_TASK_CORE_ID 1
#define FETCH_TASK_STACK_SIZE 4096
#define FETCH_TASK_PRIORITY 5

ESP_EVENT_DEFINE_BASE(AUDIO_SR_EVENT);

struct audio_sr
{
    const esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;

    RingbufHandle_t output_buffer;

    // event_loop
    esp_event_loop_handle_t event_loop;

    vad_state_t last_state;

    bool is_running;
};

static void feed_task(void *arg)
{
    audio_sr_t *sr = (audio_sr_t *)arg;
    const esp_afe_sr_iface_t *afe_handle = sr->afe_handle;
    esp_afe_sr_data_t *afe_data = sr->afe_data;

    // 获取要输入的声音的采样数量
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // 获取声道数量
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    // 计算一次feed的片段大小
    int feed_size = feed_chunksize * feed_nch * sizeof(int16_t);
    // 申请内存
    int16_t *feed_buff = (int16_t *)malloc_zeroed(feed_size);

    bsp_board_t *board = bsp_board_get_instance();
    while (sr->is_running)
    {
        esp_codec_dev_read(board->codec_dev, feed_buff, feed_size);
        sr->afe_handle->feed(sr->afe_data, feed_buff);
    }
    free(feed_buff);
    vTaskDelete(NULL);
}
static void fetch_task(void *arg)
{
    audio_sr_t *sr = (audio_sr_t *)arg;
    while (sr->is_running)
    {
        afe_fetch_result_t *result = sr->afe_handle->fetch(sr->afe_data);

        if (result->wakeup_state == WAKENET_DETECTED)
        {
            // 触发唤醒词
            esp_event_post_to(sr->event_loop, AUDIO_SR_EVENT,
                              AUDIO_PROCESSOR_EVENT_WAKEUP, NULL, 0, 0);
        }

        if (result->vad_state != sr->last_state)
        {
            // vad状态发生变化
            sr->last_state = result->vad_state;
            esp_event_post_to(sr->event_loop, AUDIO_SR_EVENT,
                              result->vad_state ? AUDIO_PROCESSOR_EVENT_SPEECH : AUDIO_PROCESSOR_EVENT_SILENCE, NULL, 0, 0);
        }

        if (sr->last_state == VAD_SPEECH)
        {
            if (result->vad_cache_size > 0)
            {
                xRingbufferSend(sr->output_buffer, result->vad_cache, result->vad_cache_size, 0);
            }

            // 输出处理结果
            xRingbufferSend(sr->output_buffer, result->data, result->data_size, 0);
        }
    }
    vTaskDelete(NULL);
}

audio_sr_t *audio_sr_create(void)
{
    audio_sr_t *sr = malloc_zeroed(sizeof(audio_sr_t));

    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init("MR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);

    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    // 获取句柄
    sr->afe_handle = esp_afe_handle_from_config(afe_config);
    // 创建实例
    sr->afe_data = sr->afe_handle->create_from_config(afe_config);

    sr->afe_handle->disable_vad(sr->afe_data);

    free(afe_config);

    // 创建event_loop
    esp_event_loop_args_t event_loop_config = {
        .queue_size = 10,
        .task_name = "sr_events",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0};
    ESP_ERROR_CHECK(esp_event_loop_create(&event_loop_config, &sr->event_loop));

    return sr;
}

void audio_sr_destroy(audio_sr_t *sr)
{
    sr->afe_handle->destroy(sr->afe_data);
    // 销毁event_loop
    esp_event_loop_delete(sr->event_loop);
    free(sr);
}

void audio_sr_start(audio_sr_t *sr)
{
    sr->is_running = true;
    xTaskCreatePinnedToCoreWithCaps(feed_task, "feed_task",
                                    FEED_TASK_STACK_SIZE, sr,
                                    FEED_TASK_PRIORITY, NULL,
                                    FEED_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCoreWithCaps(fetch_task, "fetch_task",
                                    FETCH_TASK_STACK_SIZE, sr,
                                    FETCH_TASK_PRIORITY, NULL,
                                    FETCH_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

void audio_sr_stop(audio_sr_t *sr)
{
    sr->is_running = false;
    vTaskDelay(pdMS_TO_TICKS(100));
}

void audio_sr_register_event_cb(audio_sr_t *sr, esp_event_handler_t callback, void *arg)
{
    esp_event_handler_instance_register_with(sr->event_loop, AUDIO_SR_EVENT,
                                             ESP_EVENT_ANY_ID, callback, arg, NULL);
}

void audio_sr_set_output_buffer(audio_sr_t *sr, RingbufHandle_t output_buffer)
{
    sr->output_buffer = output_buffer;
}

void audio_sr_set_vad_state(audio_sr_t *sr, bool state)
{
    if (state)
    {
        sr->afe_handle->enable_vad(sr->afe_data);
    }
    else
    {
        sr->afe_handle->disable_vad(sr->afe_data);
    }
}
