/**
 * @file bsp_board.h
 * @brief 板级支持包（Board Support Package）头文件
 * 
 * 提供硬件抽象层接口，封装了所有硬件驱动：
 * - LED 指示灯
 * - 按钮输入
 * - WiFi 网络连接
 * - NVS 非易失性存储
 * - 音频编解码器
 * - LCD 显示屏
 * 
 * 采用单例模式设计，通过 bsp_board_get_instance() 获取全局实例
 */

#pragma once

#include "bsp_config.h"
#include "led_indicator.h"
#include "iot_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_codec_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_dev.h"

/// 设备状态位定义（用于 EventGroup）
#define LED_BIT BIT0        ///< LED 初始化完成标志
#define BUTTON_BIT BIT1     ///< 按钮初始化完成标志
#define WIFI_BIT BIT2       ///< WiFi 连接成功标志
#define NVS_BIT BIT3        ///< NVS 初始化完成标志
#define CODEC_BIT BIT4      ///< 编解码器初始化完成标志
#define LCD_BIT BIT5        ///< LCD 初始化完成标志

/**
 * @brief LED 闪烁类型枚举
 * 
 * 定义不同的 LED 指示效果
 */
typedef enum
{
    LED_BLINK_TYPE_OFF,         ///< 关闭
    LED_BLINK_TYPE_BREATH,      ///< 呼吸灯效果
    LED_BLINK_TYPE_TRANSITION,  ///< 渐变效果
    LED_BLINK_TYPE_MAX,         ///< 最大值（用于边界检查）
} bsp_board_led_blink_type_t;

/**
 * @brief 板级支持包结构体
 * 
 * 包含所有硬件驱动的句柄和设备信息
 */
typedef struct
{
    EventGroupHandle_t board_status;        ///< 设备状态事件组句柄
    led_indicator_handle_t led_indicator;   ///< LED 指示灯句柄
    bsp_board_led_blink_type_t blink_type;  ///< 当前 LED 闪烁类型
    button_handle_t sw2;                    ///< SW2 按钮句柄
    button_handle_t sw3;                    ///< SW3 按钮句柄

    esp_codec_dev_handle_t codec_dev;       ///< 音频编解码器句柄

    esp_lcd_panel_io_handle_t lcd_io;       ///< LCD 传输接口句柄
    esp_lcd_panel_handle_t lcd_panel;       ///< LCD 面板句柄

    char mac_addr[18];                      ///< MAC 地址字符串（格式：XX:XX:XX:XX:XX:XX）
    char uuid[37];                          ///< 设备 UUID 字符串
} bsp_board_t;

/**
 * @brief 获取板级支持包单例实例
 * 
 * 返回全局唯一的 BSP 实例指针
 * 
 * @return bsp_board_t* BSP 实例指针
 */
bsp_board_t *bsp_board_get_instance(void);

/**
 * @brief 初始化 LED 指示灯
 * 
 * 配置 LED GPIO 和指示灯驱动
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_led_indicator_init(bsp_board_t *bsp_board);

/**
 * @brief 设置 LED 闪烁类型
 * 
 * 改变 LED 的显示效果（关闭、呼吸、渐变等）
 * 
 * @param bsp_board BSP 实例指针
 * @param blink_type 闪烁类型
 */
void bsp_board_led_indicator_set_blink_type(bsp_board_t *bsp_board, bsp_board_led_blink_type_t blink_type);

/**
 * @brief 初始化按钮
 * 
 * 配置按钮 GPIO 和输入检测驱动
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_button_init(bsp_board_t *bsp_board);

/**
 * @brief 初始化 WiFi 网络
 * 
 * 执行以下操作：
 * 1. 初始化网络接口
 * 2. 创建默认 WiFi STA
 * 3. 检查配网状态
 * 4. 如未配网则启动 BLE 配网服务
 * 
 * @param bsp_board BSP 实例指针
 * @param qrcode_payload 用于返回配网二维码数据的缓冲区
 * @param qrcode_payload_len 缓冲区长度
 */
void bsp_board_wifi_init(bsp_board_t *bsp_board, char* qrcode_payload, size_t qrcode_payload_len);

/**
 * @brief 重置 WiFi 配网状态
 * 
 * 清除已保存的 WiFi 配置并重启设备，
 * 使设备重新进入配网模式
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_wifi_reset_provisioning(bsp_board_t *bsp_board);

/**
 * @brief 获取 WiFi 信号强度
 * 
 * @param bsp_board BSP 实例指针
 * @return int RSSI 值（负数，越接近 0 信号越好），失败返回 0
 */
int bsp_board_wifi_get_rssi(bsp_board_t *bsp_board);

/**
 * @brief 初始化 NVS 非易失性存储
 * 
 * 用于保存 WiFi 配置、设备参数等持久化数据
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_nvs_init(bsp_board_t *bsp_board);

/**
 * @brief 初始化音频编解码器
 * 
 * 配置 I2S 接口和 Codec 芯片（ES8311）
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_codec_init(bsp_board_t *bsp_board);

/**
 * @brief 初始化 LCD 显示屏
 * 
 * 配置 SPI 接口和 LCD 面板驱动
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_lcd_init(bsp_board_t *bsp_board);

/**
 * @brief 打开 LCD 背光
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_lcd_on(bsp_board_t *bsp_board);

/**
 * @brief 关闭 LCD 背光
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_lcd_off(bsp_board_t *bsp_board);

/**
 * @brief 检查设备状态标志
 * 
 * 等待指定的状态位全部置位，超时返回 false
 * 
 * @param bsp_board BSP 实例指针
 * @param bits_to_check 需要检查的状态位掩码
 * @param wait_ticks 等待时间（系统滴答），portMAX_DELAY 表示无限等待
 * @return bool 所有指定位都置位返回 true，否则返回 false
 */
bool bsp_board_check_status(bsp_board_t *bsp_board, EventBits_t bits_to_check, TickType_t wait_ticks);
