/**
 * @file bsp_board_led.c
 * @brief LED 指示灯驱动实现
 * 
 * 封装 WS2812 RGB 灯带控制，提供多种闪烁效果：
 * - 关闭状态
 * - 呼吸灯效果
 * - 渐变过渡效果
 */

#include "bsp_board.h"
#include "led_indicator_strips.h"

/// 关闭效果的闪烁序列（亮度设为 0）
static const blink_step_t blink_off[] = {
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 100},
    {LED_BLINK_STOP, 0, 0},
};

/// 呼吸灯效果的闪烁序列（渐变亮 - 保持 - 渐变暗 - 保持）
static const blink_step_t blink_breath[] = {
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 1000},   // 从暗渐变到亮（1 秒）
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 500}, // 保持最亮（0.5 秒）
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 1000},    // 从亮渐变到暗（1 秒）
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 500},  // 保持最暗（0.5 秒）
    {LED_BLINK_LOOP, 0, 0},                                              // 循环执行
};

/// 渐变过渡效果的闪烁序列（红色和蓝色交替旋转）
static const blink_step_t blink_transition[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 0},       // 立即设置为红色
    {LED_BLINK_RGB_RING, SET_IRGB(MAX_INDEX, 0, 0, 0xFF), 4000}, // 蓝色旋转（4 秒）
    {LED_BLINK_RGB_RING, SET_IRGB(MAX_INDEX, 0xFF, 0, 0), 4000}, // 红色旋转（4 秒）
    {LED_BLINK_LOOP, 0, 0},                                    // 循环执行
};

/// 闪烁效果列表数组（索引对应 bsp_board_led_blink_type_t 枚举值）
static const blink_step_t *blink_step_list[LED_BLINK_TYPE_MAX] = {blink_off, blink_breath, blink_transition};

/**
 * @brief 初始化 LED 指示灯
 * 
 * 创建 WS2812 灯带设备并配置 RMT 外设，使用 DMA 传输优化性能
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_led_indicator_init(bsp_board_t *bsp_board)
{
    // 配置 LED 指示器参数
    led_indicator_config_t led_indicator_config = {
        .blink_lists = blink_step_list,     // 闪烁序列列表
        .blink_list_num = LED_BLINK_TYPE_MAX, // 列表数量
    };
    
    // 配置灯带硬件参数
    led_indicator_strips_config_t strip_config = {
        .led_strip_driver = LED_STRIP_RMT,  // 使用 RMT 外设驱动
        .led_strip_rmt_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,  // 默认时钟源
            .flags.with_dma = true,          // 启用 DMA 提高传输效率
        },
        .led_strip_cfg = {
            .strip_gpio_num = BSP_LED_PIN,   // LED 数据引脚
            .max_leds = 2,                   // 最多 2 个 LED
            .led_model = LED_MODEL_WS2812,   // WS2812 型号
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // GRB 颜色格式
        },
    };
    
    // 创建 LED 指示器设备
    esp_err_t ret = led_indicator_new_strips_device(
        &led_indicator_config,
        &strip_config,
        &bsp_board->led_indicator);

    ESP_ERROR_CHECK(ret);
    // 设置 LED 初始化完成标志
    xEventGroupSetBits(bsp_board->board_status, LED_BIT);
}

/**
 * @brief 设置 LED 闪烁类型
 * 
 * 停止当前闪烁效果并切换到新的效果
 * 
 * @param bsp_board BSP 实例指针
 * @param blink_type 新的闪烁类型（关闭/呼吸/渐变）
 */
void bsp_board_led_indicator_set_blink_type(bsp_board_t *bsp_board, bsp_board_led_blink_type_t blink_type)
{
    // 停止当前的闪烁效果
    led_indicator_stop(bsp_board->led_indicator, bsp_board->blink_type);
    // 更新闪烁类型
    bsp_board->blink_type = blink_type;
    // 启动新的闪烁效果
    led_indicator_start(bsp_board->led_indicator, blink_type);
}
