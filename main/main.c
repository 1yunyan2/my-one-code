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

    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_BREATH);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_TRANSITION);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_OFF);
}
