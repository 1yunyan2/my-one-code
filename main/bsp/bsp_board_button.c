/**
 * @file bsp_board_button.c
 * @brief 按钮输入驱动实现
 * 
 * 配置 ADC 通道检测两个物理按钮（SW2 和 SW3），
 * 支持短按和长按事件检测。
 */

#include "bsp_board.h"
#include "button_adc.h"

/**
 * @brief 初始化按钮
 * 
 * 创建两个 ADC 按钮设备：
 * - SW2: ADC1_CH7，电压范围 0-20（对应 GPIO 按键）
 * - SW3: ADC1_CH7，电压范围 1600-1700（对应另一个 GPIO 按键）
 * 
 * 每个按钮都配置了：
 * - 短按时间阈值：120ms
 * - 长按时间阈值：800ms
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_button_init(bsp_board_t *bsp_board)
{
    // 配置按钮通用参数
    button_config_t button_config = {
        .short_press_time = 120,   // 短按判定时间：120ms
        .long_press_time = 800,    // 长按判定时间：800ms
    };
    
    // 配置 ADC 硬件参数（用于 SW2 按钮）
    button_adc_config_t adc_config = {
        .unit_id = ADC_UNIT_1,          // 使用 ADC1 单元
        .adc_channel = ADC_CHANNEL_7,   // ADC 通道 7
        .button_index = 0,              // 按钮索引 0（SW2）
        .min = 0,                       // 最小 ADC 值
        .max = 20,                      // 最大 ADC 值
    };
    
    // 创建 SW2 按钮设备
    ESP_ERROR_CHECK(iot_button_new_adc_device(&button_config, &adc_config, &bsp_board->sw2));

    // 重新配置 ADC 参数（用于 SW3 按钮）
    adc_config.button_index = 1;   // 按钮索引 1（SW3）
    adc_config.min = 1600;         // 最小 ADC 值 1600
    adc_config.max = 1700;         // 最大 ADC 值 1700
    
    // 创建 SW3 按钮设备
    ESP_ERROR_CHECK(iot_button_new_adc_device(&button_config, &adc_config, &bsp_board->sw3));

    // 设置按钮初始化完成标志
    xEventGroupSetBits(bsp_board->board_status, BUTTON_BIT);
}
