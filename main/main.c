#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/bsp_board.h"
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

    uint8_t* buf = malloc(1024);
    while (1)
    {
        esp_codec_dev_read(bsp_board->codec_dev, buf, 1024);
        esp_codec_dev_write(bsp_board->codec_dev, buf, 1024);
    }
    

    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_BREATH);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_TRANSITION);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_OFF);
}
