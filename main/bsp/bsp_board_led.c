#include "bsp_board.h"
#include "led_indicator_strips.h"

static const blink_step_t blink_off[] = {
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 100},
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t blink_breath[] = {
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 1000},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 500},
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 1000},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 500},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t blink_transition[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 0},
    {LED_BLINK_RGB_RING, SET_IRGB(MAX_INDEX, 0, 0, 0xFF), 4000},
    {LED_BLINK_RGB_RING, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 4000},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t *blink_step_list[LED_BLINK_TYPE_MAX] = {blink_off, blink_breath, blink_transition};

void bsp_board_led_indicator_init(bsp_board_t *bsp_board)
{
    // 创建一个灯带设备
    led_indicator_config_t led_indicator_config = {
        .blink_lists = blink_step_list,
        .blink_list_num = LED_BLINK_TYPE_MAX,
    };
    led_indicator_strips_config_t strip_config = {
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .flags.with_dma = true,
        },
        .led_strip_cfg = {
            .strip_gpio_num = BSP_LED_PIN,
            .max_leds = 2,
            .led_model = LED_MODEL_WS2812,
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        },
    };
    esp_err_t ret = led_indicator_new_strips_device(
        &led_indicator_config,
        &strip_config,
        &bsp_board->led_indicator);

    ESP_ERROR_CHECK(ret);
    xEventGroupSetBits(bsp_board->board_status, LED_BIT);
}

void bsp_board_led_indicator_set_blink_type(bsp_board_t *bsp_board, bsp_board_led_blink_type_t blink_type)
{
    led_indicator_stop(bsp_board->led_indicator, bsp_board->blink_type);
    bsp_board->blink_type = blink_type;
    led_indicator_start(bsp_board->led_indicator, blink_type);
}
