#pragma once

#include "bsp_config.h"
#include "led_indicator.h"
#include "iot_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_codec_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_dev.h"

#define LED_BIT BIT0
#define BUTTON_BIT BIT1
#define WIFI_BIT BIT2
#define NVS_BIT BIT3
#define CODEC_BIT BIT4
#define LCD_BIT BIT5

typedef enum
{
    LED_BLINK_TYPE_OFF,
    LED_BLINK_TYPE_BREATH,
    LED_BLINK_TYPE_TRANSITION,
    LED_BLINK_TYPE_MAX,
} bsp_board_led_blink_type_t;

typedef struct
{
    EventGroupHandle_t board_status;
    led_indicator_handle_t led_indicator;
    bsp_board_led_blink_type_t blink_type;
    button_handle_t sw2;
    button_handle_t sw3;

    // 音频设备
    esp_codec_dev_handle_t codec_dev;

    // LCD
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;

    // 必要信息
    char mac_addr[18];
    char uuid[37];
} bsp_board_t;

bsp_board_t *bsp_board_get_instance(void);

void bsp_board_led_indicator_init(bsp_board_t *bsp_board);

void bsp_board_led_indicator_set_blink_type(bsp_board_t *bsp_board, bsp_board_led_blink_type_t blink_type);

void bsp_board_button_init(bsp_board_t *bsp_board);

void bsp_board_wifi_init(bsp_board_t *bsp_board, char* qrcode_payload, size_t qrcode_payload_len);

void bsp_board_wifi_reset_provisioning(bsp_board_t *bsp_board);

int bsp_board_wifi_get_rssi(bsp_board_t *bsp_board);

void bsp_board_nvs_init(bsp_board_t *bsp_board);

void bsp_board_codec_init(bsp_board_t *bsp_board);

void bsp_board_lcd_init(bsp_board_t *bsp_board);

void bsp_board_lcd_on(bsp_board_t *bsp_board);

void bsp_board_lcd_off(bsp_board_t *bsp_board);

/* 检查特定标志位，如果所有检查的位都成功，返回true，否则返回false */
bool bsp_board_check_status(bsp_board_t *bsp_board, EventBits_t bits_to_check, TickType_t wait_ticks);
