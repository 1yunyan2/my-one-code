#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/bsp_board.h"
#include "audio/audio_encoder.h"
#include "audio/audio_decoder.h"
#include "esp_log.h"

#define TAG "main"

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

static void input_task(void *arg)
{
    RingbufHandle_t input_buffer = (RingbufHandle_t)arg;
    bsp_board_t *board = bsp_board_get_instance();
    void *buf = malloc(1024);
    while (1)
    {
        esp_codec_dev_read(board->codec_dev, buf, 1024);
        xRingbufferSend(input_buffer, buf, 1024, portMAX_DELAY);
    }
}

static void output_task(void *arg)
{
    RingbufHandle_t output_buffer = (RingbufHandle_t)arg;
    bsp_board_t *board = bsp_board_get_instance();
    while (1)
    {
        size_t size_read = 0;
        void *buf = xRingbufferReceiveUpTo(output_buffer, &size_read, portMAX_DELAY, 1024);
        esp_codec_dev_write(board->codec_dev, buf, size_read);
        vRingbufferReturnItem(output_buffer, buf);
    }
    
}

void app_main(void)
{
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
    esp_codec_dev_set_out_vol(bsp_board->codec_dev, 60);
    esp_codec_dev_set_in_gain(bsp_board->codec_dev, 10);
    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = BSP_CODEC_SAMPLE_RATE,
        .bits_per_sample = BSP_CODEC_BITS_PER_SAMPLE,
        .channel = 1,
    };
    esp_codec_dev_open(bsp_board->codec_dev, &sample_info);

    bool ret = bsp_board_check_status(bsp_board, WIFI_BIT, pdMS_TO_TICKS(30000));
    if (!ret)
    {
        ESP_LOGE(TAG, "wifi init failed");
    }

    RingbufHandle_t input_buffer = xRingbufferCreate(16384, RINGBUF_TYPE_BYTEBUF);
    RingbufHandle_t mid_buffer = xRingbufferCreate(2048, RINGBUF_TYPE_NOSPLIT);
    RingbufHandle_t output_buffer = xRingbufferCreate(2048, RINGBUF_TYPE_BYTEBUF);

    audio_encoder_t *audio_encoder = audio_encoder_create(BSP_CODEC_SAMPLE_RATE, 1);
    audio_encoder_set_buffer(audio_encoder, input_buffer, mid_buffer);

    audio_decoder_t *audio_decoder = audio_decoder_create(BSP_CODEC_SAMPLE_RATE, 1);
    audio_decoder_set_buffer(audio_decoder, mid_buffer, output_buffer);

    audio_encoder_start(audio_encoder);
    audio_decoder_start(audio_decoder);

    xTaskCreate(input_task, "input_task", 4096, input_buffer, 5, NULL);
    xTaskCreate(output_task, "output_task", 4096, output_buffer, 5, NULL);

    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_BREATH);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_TRANSITION);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_OFF);
}
