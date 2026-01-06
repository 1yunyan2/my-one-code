#include "bsp_board.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

void bsp_board_lcd_init(bsp_board_t *bsp_board)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_LCD_BK_PIN};
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SCLK_PIN,
        .mosi_io_num = BSP_LCD_MOSI_PIN,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC_PIN,
        .cs_gpio_num = BSP_LCD_CS_PIN,
        .pclk_hz = (80 * 1000 * 1000),
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &bsp_board->lcd_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
    };
    // Initialize the LCD configuration
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(bsp_board->lcd_io, &panel_config, &bsp_board->lcd_panel));

    // Turn off backlight to avoid unpredictable display on the LCD screen while initializing
    // the LCD panel driver. (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(BSP_LCD_BK_PIN, 0));

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(bsp_board->lcd_panel));

    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(bsp_board->lcd_panel));

    // Turn on the screen
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(bsp_board->lcd_panel, false));
}

void bsp_board_lcd_on(bsp_board_t *bsp_board)
{
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(bsp_board->lcd_panel, true));
    ESP_ERROR_CHECK(gpio_set_level(BSP_LCD_BK_PIN, 1));
}

void bsp_board_lcd_off(bsp_board_t *bsp_board)
{
    ESP_ERROR_CHECK(gpio_set_level(BSP_LCD_BK_PIN, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(bsp_board->lcd_panel, false));
}
