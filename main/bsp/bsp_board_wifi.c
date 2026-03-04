#include "bsp_board.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

#define TAG "[BSP] WiFi"

/**
 * @brief WiFi事件处理回调函数
 *
 * 处理各种WiFi相关事件：
 * - STA启动事件：自动连接WiFi
 * - STA断开事件：自动重连
 * - 获取IP事件：设置WiFi连接成功标志
 * - 配网结束事件：清理配网管理器
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    bsp_board_t *board = bsp_board_get_instance();

    // 处理WiFi事件
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // STA模式启动，开始连接WiFi
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // WiFi断开连接，自动重连
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // 成功获取IP地址，设置WiFi连接成功标志
        xEventGroupSetBits(board->board_status, WIFI_BIT);
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END)
    {
        // 配网完成，清理配网管理器
        wifi_prov_mgr_deinit();
    }
}

/**
 * !@brief 初始化WiFi模块
 *
 * 执行以下操作：
 * 1. 初始化网络接口
 * 2. 创建默认WiFi STA接口
 * 3. 初始化WiFi驱动
 * 4. 注册事件处理回调
 * 5. 初始化配网管理器
 * 6. 检查配网状态并执行相应操作
 *
 * @param bsp_board 板级支持实例指针
 * @param payload 用于返回配网二维码数据的缓冲区
 * @param len payload缓冲区长度
 */
void bsp_board_wifi_init(bsp_board_t *bsp_board, char *payload, size_t len)
{
    //************初始化判断NVS*************** */
    //! 1：看一下定义的nvs是否配置
    // 检查NVS是否已初始化
    if (!bsp_board_check_status(bsp_board, NVS_BIT, 0))
    {
        ESP_LOGE(TAG, "NVS not initialized");
        return;
    }

    //! 2：初始化TCP/IP网络接口栈
    ESP_ERROR_CHECK(esp_netif_init());

    //! 3:创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //! 4: 创建默认WiFi STA网络接口
    esp_netif_create_default_wifi_sta();

    //! 5: 初始化WiFi配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //! 6:注册WiFi和IP事件处理回调
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_prov_end;

    // 注册WiFi事件处理回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    // 注册IP获取事件处理回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    // 注册配网结束事件处理回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT,
                                                        WIFI_PROV_END,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_prov_end));

    /** 初始化WiFi配网管理器 */
    wifi_prov_mgr_config_t prov_config = {
        .scheme = wifi_prov_scheme_ble,                                      // 使用BLE配网方案
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM // BLE事件处理器
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_config));

    /* 检查设备是否已经配过网 */
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned)); // 检查设备是否已经配过网

    // 获取设备MAC地址用于标识
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    snprintf(bsp_board->mac_addr, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (!provisioned)
    {
        // 设备未配网，启动配网流程
        // 配网加密密钥
        const char *security_key = "abcd1234";

        // 生成设备蓝牙名称 (格式: XIAOZHI-XXYYZZ)
        char service_name[15];
        snprintf(service_name, 15, "XIAOZHI-%02X%02X%02X", mac[3], mac[4], mac[5]);

        // 启动配网服务
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1,       // 使用WPA2-PSK加密
                                                         (const void *)security_key, // 配网密钥
                                                         service_name,               // 服务名称
                                                         NULL));                     // 配网回调函数

        // 生成配网二维码JSON数据
        snprintf(payload, len, "{\"ver\":\"v1\",\"name\":\"%s\""
                               ",\"pop\":\"%s\",\"transport\":\"ble\"}",
                 service_name, security_key);
        ESP_LOGI(TAG, "QR code: %s", payload);
    }
    else
    {
        // 设备已配网，直接连接WiFi
        wifi_prov_mgr_deinit();                            // 清理配网管理器
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 设置为STA模式
        ESP_ERROR_CHECK(esp_wifi_start());                 // 启动WiFi
    }
}

/**
 * @brief 重置WiFi配网状态
 *
 * 清除已保存的WiFi配置信息并重启设备，
 * 使设备重新进入配网模式
 *
 * @param bsp_board 板级支持实例指针
 */
void bsp_board_wifi_reset_provisioning(bsp_board_t *bsp_board)
{
    wifi_prov_mgr_reset_provisioning(); // 重置配网状态
    esp_restart();                      // 重启设备
}

/**
 * @brief 获取当前WiFi信号强度
 *
 * @param bsp_board 板级支持实例指针
 * @return int 当前RSSI值，获取失败返回0
 */
int bsp_board_wifi_get_rssi(bsp_board_t *bsp_board)
{
    int rssi = 0;
    esp_err_t ret = esp_wifi_sta_get_rssi(&rssi); // 获取STA模式下的RSSI
    if (ret == ESP_OK)
    {
        return rssi;
    }
    return 0;
}
