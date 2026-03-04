/**
 * @file bsp_board_codec.c
 * @brief 音频编解码器驱动实现
 * 
 * 配置 ES8311 Codec 芯片，包括：
 * - I2C 控制总线（配置寄存器）
 * - I2S 数据接口（音频数据传输）
 * - 支持双工模式（同时录音和播放）
 */

#include "bsp_board.h"
#include "esp_codec_dev_defaults.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"

/**
 * @brief 初始化 I2C 控制总线
 * 
 * 用于配置 ES8311 Codec 芯片的寄存器
 * 
 * @param bsp_board BSP 实例指针
 * @param bus_handle 返回的 I2C 总线句柄指针
 */
static void bsp_board_codec_i2c_init(bsp_board_t *bsp_board, i2c_master_bus_handle_t *bus_handle)
{
    // 配置 I2C 总线参数
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,                // 使用 I2C0 端口
        .sda_io_num = BSP_CODEC_SDA_PIN,      // SDA 数据线引脚
        .scl_io_num = BSP_CODEC_SCL_PIN,      // SCL 时钟线引脚
        .clk_source = I2C_CLK_SRC_DEFAULT,    // 默认时钟源
        .glitch_ignore_cnt = 7,               // 毛刺过滤阈值
        .flags.enable_internal_pullup = true, // 启用内部上拉电阻
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, bus_handle));
}

/**
 * @brief 初始化 I2S 数据接口
 * 
 * 配置 I2S 标准模式，用于音频数据的输入输出传输
 * 
 * @param bsp_board BSP 实例指针
 * @param rx_handle 返回的接收通道句柄指针
 * @param tx_handle 返回的发送通道句柄指针
 */
static void bsp_board_codec_i2s_init(bsp_board_t *bsp_board, i2s_chan_handle_t *rx_handle, i2s_chan_handle_t *tx_handle)
{
    // 配置 I2S 通道为 Master 模式
    i2s_chan_config_t i2s_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    // 启用回调后自动清除缓冲区，防止 DMA 循环发送最后一帧数据
    i2s_chan_config.auto_clear_after_cb = true;
    
    // 创建 I2S 收发通道
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_chan_config, tx_handle, rx_handle));

    // 配置 I2S 标准模式参数
    i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_CODEC_SAMPLE_RATE),         // 采样率配置
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(BSP_CODEC_BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO), // 单声道槽位
        .gpio_cfg = {
            .mclk = BSP_CODEC_MCLK_PIN,   // 主时钟引脚
            .bclk = BSP_CODEC_BCLK_PIN,   // 位时钟引脚
            .ws = BSP_CODEC_WS_PIN,       // 字选择（左右声道）引脚
            .dout = BSP_CODEC_DOUT_PIN,   // 数据输出引脚
            .din = BSP_CODEC_DIN_PIN,     // 数据输入引脚
        },
    };
    
    // 初始化 I2S 为标准模式并启用通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx_handle, &std_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx_handle, &std_config));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(*tx_handle));
}

/**
 * @brief 初始化音频编解码器
 * 
 * 完整初始化 ES8311 Codec 芯片：
 * 1. 创建 I2C 控制接口
 * 2. 创建 GPIO 控制接口
 * 3. 创建 I2S 数据接口
 * 4. 创建 Codec 设备句柄
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_codec_init(bsp_board_t *bsp_board)
{
    // ========== 1. 创建 I2C 控制接口 ==========
    i2c_master_bus_handle_t bus_handle = NULL;
    bsp_board_codec_i2c_init(bsp_board, &bus_handle);
    
    audio_codec_i2c_cfg_t i2c_cfg = {
        .bus_handle = bus_handle,           // I2C 总线句柄
        .addr = ES8311_CODEC_DEFAULT_ADDR,  // ES8311 默认地址
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    
    // ========== 2. 创建 GPIO 控制接口 ==========
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    
    // ========== 3. 配置 ES8311 Codec 参数 ==========
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,                 // 控制接口
        .gpio_if = gpio_if,                 // GPIO 接口
        .pa_pin = BSP_CODEC_PA_PIN,         // 功放使能引脚
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH, // 双工模式（录音 + 播放）
        .use_mclk = true,                   // 使用主时钟
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);

    // ========== 4. 创建 I2S 数据接口 ==========
    i2s_chan_handle_t rx_handle = NULL, tx_handle = NULL;
    bsp_board_codec_i2s_init(bsp_board, &rx_handle, &tx_handle);
    
    audio_codec_i2s_cfg_t i2s_config = {
        .rx_handle = rx_handle,             // 接收通道（录音）
        .tx_handle = tx_handle,             // 发送通道（播放）
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_config);

    // ========== 5. 创建音频设备句柄 ==========
    esp_codec_dev_cfg_t codec_config = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,  // 输入输出类型
        .codec_if = codec_if,                   // Codec 接口
        .data_if = data_if,                     // 数据接口
    };
    bsp_board->codec_dev = esp_codec_dev_new(&codec_config);
    assert(bsp_board->codec_dev);
    
    // 设置 Codec 初始化完成标志
    xEventGroupSetBits(bsp_board->board_status, CODEC_BIT);
}
