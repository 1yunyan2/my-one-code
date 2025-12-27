#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/bsp_board.h"

void app_main(void)
{
    bsp_board_t *bsp_board = bsp_board_get_instance();
    bsp_board_led_indicator_init(bsp_board);

    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_BREATH);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_TRANSITION);
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_board_led_indicator_set_blink_type(bsp_board, LED_BLINK_TYPE_OFF);
}
