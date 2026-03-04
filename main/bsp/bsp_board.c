/**
 * @file bsp_board.c
 * @brief 板级支持包（Board Support Package）核心实现
 * 
 * 提供硬件抽象层接口，封装了所有硬件驱动的初始化和状态管理：
 * - LED 指示灯
 * - 按钮输入
 * - WiFi 网络连接
 * - NVS 非易失性存储
 * - 音频编解码器
 * - LCD 显示屏
 * 
 * 采用单例模式设计，通过 bsp_board_get_instance() 获取全局实例
 */

#include "bsp_board.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_random.h"

/// 全局唯一的 BSP 实例（静态变量）
static bsp_board_t bsp_board = {0};

/**
 * @brief 获取板级支持包单例实例
 * 
 * 返回全局唯一的 BSP 实例指针，如果事件组未创建则自动初始化
 * 
 * @return bsp_board_t* BSP 实例指针
 */
bsp_board_t *bsp_board_get_instance(void)
{
    // 如果事件组未创建，则创建
    if (!bsp_board.board_status)
    {
        bsp_board.board_status = xEventGroupCreate();
    }
    return &bsp_board;
}

/**
 * @brief 初始化 NVS 非易失性存储
 * 
 * 执行以下操作：
 * 1. 初始化 NVS 分区（键值对数据库）
 * 2. 读取或生成设备 UUID
 * 3. 设置 NVS 初始化完成标志
 * 
 * @param bsp_board BSP 实例指针
 */
void bsp_board_nvs_init(bsp_board_t *bsp_board)
{
    // 初始化 NVS 分区
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 如果 NVS 空间不足或版本不匹配，则擦除后重新初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // 设置 NVS 初始化完成标志
    xEventGroupSetBits(bsp_board->board_status, NVS_BIT);

    // 尝试从 NVS 获取 UUID
    nvs_handle_t nvs_handle = 0;
    ESP_ERROR_CHECK(nvs_open("Settings", NVS_READWRITE, &nvs_handle));

    size_t length = 37;
    ret = nvs_get_str(nvs_handle, "uuid", bsp_board->uuid, &length);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // 未找到 UUID，随机生成一个新的 UUID v4
        uint8_t data[16];
        esp_fill_random(data, 16);
        
        // 设置 UUID v4 的版本位（第 7 个字节的高 4 位为 0100）
        data[6] = (data[6] & 0x0F) | 0x40;
        // 设置 UUID v4 的变体位（第 9 个字节的高 2 位为 10）
        data[8] = (data[8] & 0x3F) | 0x80;

        // 格式化为字符串（格式：xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）
        snprintf(bsp_board->uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 data[0], data[1], data[2], data[3],
                 data[4], data[5], data[6], data[7],
                 data[8], data[9], data[10], data[11],
                 data[12], data[13], data[14], data[15]);
        
        // 将新生成的 UUID 写入 NVS 保存
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "uuid", bsp_board->uuid));
        ESP_ERROR_CHECK(nvs_commit(nvs_handle));

        ret = ESP_OK;
    }
    ESP_ERROR_CHECK(ret);
    nvs_close(nvs_handle);
}

/**
 * @brief 检查设备状态标志
 * 
 * 等待指定的状态位全部置位，超时返回 false
 * 
 * @param bsp_board BSP 实例指针
 * @param bits_to_check 需要检查的状态位掩码（如 LED_BIT | BUTTON_BIT）
 * @param wait_ticks 等待时间（系统滴答），portMAX_DELAY 表示无限等待
 * @return bool 所有指定位都置位返回 true，否则返回 false
 */
bool bsp_board_check_status(bsp_board_t *bsp_board, EventBits_t bits_to_check, TickType_t wait_ticks)
{
    // 等待指定的事件位
    EventBits_t bits = xEventGroupWaitBits(bsp_board->board_status, bits_to_check, pdFALSE, pdTRUE, wait_ticks);
    // 检查是否所有指定位都已置位
    return (bits & bits_to_check) == bits_to_check;
}
