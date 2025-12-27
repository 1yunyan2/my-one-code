#pragma once

#include "bsp_config.h"
#include "led_indicator.h"

typedef enum
{
    LED_BLINK_TYPE_OFF,
    LED_BLINK_TYPE_BREATH,
    LED_BLINK_TYPE_TRANSITION,
    LED_BLINK_TYPE_MAX,
} bsp_board_led_blink_type_t;

typedef struct
{
    led_indicator_handle_t led_indicator;
    bsp_board_led_blink_type_t blink_type;
} bsp_board_t;

bsp_board_t *bsp_board_get_instance(void);

void bsp_board_led_indicator_init(bsp_board_t *bsp_board);

void bsp_board_led_indicator_set_blink_type(bsp_board_t *bsp_board, bsp_board_led_blink_type_t blink_type);
