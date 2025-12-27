#include "bsp_board.h"
#include "button_adc.h"

void bsp_board_button_init(bsp_board_t *bsp_board)
{
    button_config_t button_config = {
        .short_press_time = 120,
        .long_press_time = 800,
    };
    button_adc_config_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = ADC_CHANNEL_7,
        .button_index = 0,
        .min = 0,
        .max = 20,
    };
    ESP_ERROR_CHECK(iot_button_new_adc_device(&button_config, &adc_config, &bsp_board->sw2));

    adc_config.button_index = 1;
    adc_config.min = 1600;
    adc_config.max = 1700;
    ESP_ERROR_CHECK(iot_button_new_adc_device(&button_config, &adc_config, &bsp_board->sw3));

    xEventGroupSetBits(bsp_board->board_status, BUTTON_BIT);
}
