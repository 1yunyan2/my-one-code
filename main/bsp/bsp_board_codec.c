#include "bsp_board.h"
#include "esp_codec_dev_defaults.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"

static void bsp_board_codec_i2c_init(bsp_board_t *bsp_board, i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BSP_CODEC_SDA_PIN,
        .scl_io_num = BSP_CODEC_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, bus_handle));
}
static void bsp_board_codec_i2s_init(bsp_board_t *bsp_board, i2s_chan_handle_t *rx_handle, i2s_chan_handle_t *tx_handle)
{
    i2s_chan_config_t i2s_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    // 防止DMA发送完成后，循环发送最后一帧声音数据
    i2s_chan_config.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_chan_config, tx_handle, rx_handle));

    i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_CODEC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(BSP_CODEC_BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BSP_CODEC_MCLK_PIN,
            .bclk = BSP_CODEC_BCLK_PIN,
            .ws = BSP_CODEC_WS_PIN,
            .dout = BSP_CODEC_DOUT_PIN,
            .din = BSP_CODEC_DIN_PIN,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx_handle, &std_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx_handle, &std_config));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(*tx_handle));
}

void bsp_board_codec_init(bsp_board_t *bsp_board)
{
    // 创建控制通道
    i2c_master_bus_handle_t bus_handle = NULL;
    bsp_board_codec_i2c_init(bsp_board, &bus_handle);
    audio_codec_i2c_cfg_t i2c_cfg = {
        .bus_handle = bus_handle,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = BSP_CODEC_PA_PIN,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .use_mclk = true,
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);

    // 生成数据通道句柄
    i2s_chan_handle_t rx_handle = NULL, tx_handle = NULL;
    bsp_board_codec_i2s_init(bsp_board, &rx_handle, &tx_handle);
    audio_codec_i2s_cfg_t i2s_config = {
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_config);

    // 生成音频设备
    esp_codec_dev_cfg_t codec_config = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    bsp_board->codec_dev = esp_codec_dev_new(&codec_config);
    assert(bsp_board->codec_dev);
    xEventGroupSetBits(bsp_board->board_status, CODEC_BIT);
}
