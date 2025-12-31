#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/bsp_board.h"
#include "audio/audio_processor.h"
#include "esp_log.h"

#define TAG "main"

void audio_sr_event_cb(void *event_handler_arg,
                       esp_event_base_t event_base,
                       int32_t event_id,
                       void *event_data)
{
    audio_processor_t *audio_processor = (audio_processor_t *)event_handler_arg;
    switch (event_id)
    {
    case AUDIO_PROCESSOR_EVENT_WAKEUP:
        ESP_LOGI(TAG, "Wakeup");
        audio_processor_set_vad_state(audio_processor, true);
        break;
    case AUDIO_PROCESSOR_EVENT_SPEECH:
        ESP_LOGI(TAG, "Speech");
        /* code */
        break;
    case AUDIO_PROCESSOR_EVENT_SILENCE:
        ESP_LOGI(TAG, "Silence");
        /* code */
        break;
    default:
        break;
    }
}

void button_cb(void *button_handle, void *usr_data)
{
    bsp_board_t *bsp_board = bsp_board_get_instance();

    if (button_handle == bsp_board->sw2)
    {
        // 按下sw2按键
        button_event_t event = iot_button_get_event(button_handle);
        switch (event)
        {
        case BUTTON_SINGLE_CLICK:
            ESP_LOGI(TAG, "sw2 single click");
            break;
        case BUTTON_DOUBLE_CLICK:
            ESP_LOGI(TAG, "sw2 double click");
            break;
        case BUTTON_LONG_PRESS_START:
            bsp_board_wifi_reset_provisioning(bsp_board);
            break;
        default:
            break;
        }
    }
}

void heap_monitor_task(void *arg)
{
    while (1)
    {
        uint32_t heap_size = esp_get_free_internal_heap_size();
        ESP_LOGE(TAG, "heap size: %lu", heap_size);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    xTaskCreate(heap_monitor_task, "heap_monitor_task", 4096, NULL, 5, NULL);
    bsp_board_t *bsp_board = bsp_board_get_instance();
    bsp_board_led_indicator_init(bsp_board);
    bsp_board_button_init(bsp_board);

    iot_button_register_cb(bsp_board->sw2, BUTTON_SINGLE_CLICK, NULL, button_cb, NULL);
    iot_button_register_cb(bsp_board->sw2, BUTTON_DOUBLE_CLICK, NULL, button_cb, NULL);
    iot_button_register_cb(bsp_board->sw2, BUTTON_LONG_PRESS_START, NULL, button_cb, NULL);

    bsp_board_nvs_init(bsp_board);
    bsp_board_wifi_init(bsp_board);
    bsp_board_codec_init(bsp_board);

    // 打开音频设备
    esp_codec_dev_set_out_vol(bsp_board->codec_dev, 80);
    esp_codec_dev_set_in_gain(bsp_board->codec_dev, 20);
    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = BSP_CODEC_SAMPLE_RATE,
        .bits_per_sample = BSP_CODEC_BITS_PER_SAMPLE,
        .channel = 2,
    };
    esp_codec_dev_open(bsp_board->codec_dev, &sample_info);

    bool ret = bsp_board_check_status(bsp_board, WIFI_BIT, pdMS_TO_TICKS(30000));
    if (!ret)
    {
        ESP_LOGE(TAG, "wifi init failed");
    }

    audio_processor_t *audio_processor = audio_processor_create();
    audio_processor_register_event_cb(audio_processor, audio_sr_event_cb, audio_processor);
    audio_processor_start(audio_processor);

    void *buffer = malloc(1024);
    while (1)
    {
        size_t size_read = audio_processor_read(audio_processor, buffer, 1024);
        audio_processor_write(audio_processor, buffer, size_read);
    }
}
