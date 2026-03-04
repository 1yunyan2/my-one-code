/**
 * @file bsp_board_lcd.c
 * @brief LCD 显示屏驱动实现
 * 
 * 配置 ST7789 LCD 面板，通过 SPI 接口通信：
 * - 初始化 SPI 总线
 * - 配置 LCD 控制器
 * - 控制背光开关
 */

#include "bsp_board.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

/**
 * @brief 初始化 LCD 显示屏
 * 
 * 完整初始化流程：
 * 1. 配置背光 GPIO
 * 2. 初始化 SPI 总线
 * 3. 创建 SPI 通信接口
 * 4. 初始化 ST7789 面板驱动
 * 5. 复位并启动 LCD
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_lcd_init(bsp_board_t *bsp_board)
{
    // ========== 1. 配置背光 GPIO ==========
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,                // 输出模式
        .pin_bit_mask = 1ULL << BSP_LCD_BK_PIN,  // 背光引脚
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // ========== 2. 初始化 SPI 总线 ==========
    spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SCLK_PIN,   // 时钟线
        .mosi_io_num = BSP_LCD_MOSI_PIN,   // 数据线（主出从入）
        .miso_io_num = -1,                 // 不使用 MISO（只写不读）
        .quadwp_io_num = -1,               // 不使用四线模式
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // ========== 3. 创建 SPI 通信接口 ==========
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC_PIN,     // 数据/命令选择引脚
        .cs_gpio_num = BSP_LCD_CS_PIN,     // 片选引脚
        .pclk_hz = (80 * 1000 * 1000),     // SPI 时钟频率 80MHz
        .lcd_cmd_bits = 8,                 // 命令位宽 8 位
        .lcd_param_bits = 8,               // 参数位宽 8 位
        .spi_mode = 0,                     // SPI 模式 0
        .trans_queue_depth = 10,           // 事务队列深度
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &bsp_board->lcd_io));

    // ========== 4. 初始化 ST7789 LCD 面板 ==========
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST_PIN,       // 复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // RGB 排列顺序
        .bits_per_pixel = 16,                    // 每像素 16 位（RGB565）
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE, // 小端字节序
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(bsp_board->lcd_io, &panel_config, &bsp_board->lcd_panel));

    // ========== 5. 关闭背光（避免初始化过程中的不可预测显示） ==========
    ESP_ERROR_CHECK(gpio_set_level(BSP_LCD_BK_PIN, 0));

    // ========== 6. 复位显示屏 ==========
    ESP_ERROR_CHECK(esp_lcd_panel_reset(bsp_board->lcd_panel));

    // ========== 7. 初始化 LCD 面板驱动 ==========
    ESP_ERROR_CHECK(esp_lcd_panel_init(bsp_board->lcd_panel));

    // ========== 8. 关闭显示（等待上层主动开启） ==========
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(bsp_board->lcd_panel, false));
}

/**
 * @brief 打开 LCD 背光和显示
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_lcd_on(bsp_board_t *bsp_board)
{
    // 启用显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(bsp_board->lcd_panel, true));
    // 打开背光
    ESP_ERROR_CHECK(gpio_set_level(BSP_LCD_BK_PIN, 1));
}

/**
 * @brief 关闭 LCD 背光和显示
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_lcd_off(bsp_board_t *bsp_board)
{
    // 关闭背光
    ESP_ERROR_CHECK(gpio_set_level(BSP_LCD_BK_PIN, 0));
    // 禁用显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(bsp_board->lcd_panel, false));
}
