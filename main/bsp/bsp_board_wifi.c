#include "bsp_board.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

#define TAG "[BSP] WiFi"

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    bsp_board_t *board = bsp_board_get_instance();
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(board->board_status, WIFI_BIT);
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END)
    {
        wifi_prov_mgr_deinit();
    }
}

void bsp_board_wifi_init(bsp_board_t *bsp_board, char* payload, size_t len)
{
    if (!bsp_board_check_status(bsp_board, NVS_BIT, 0))
    {
        ESP_LOGE(TAG, "NVS not initialized");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_prov_end;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT,
                                                        WIFI_PROV_END,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_prov_end));
    /** 初始化provisioning manager */
    wifi_prov_mgr_config_t prov_config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM};
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_config));

    /* 检查是否配过网 */
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    snprintf(bsp_board->mac_addr, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (!provisioned)
    {
        // 尝试配网
        // 加密密钥
        const char *security_key = "abcd1234";

        // 设备蓝牙名称
        char service_name[15];
        snprintf(service_name, 15, "XIAOZHI-%02X%02X%02X", mac[3], mac[4], mac[5]);

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, (const void *)security_key, service_name, NULL));

        // 生成二维码
        snprintf(payload, len, "{\"ver\":\"v1\",\"name\":\"%s\""
                                           ",\"pop\":\"%s\",\"transport\":\"ble\"}",
                 service_name, security_key);
        ESP_LOGI(TAG, "QR code: %s", payload);
    }
    else
    {
        wifi_prov_mgr_deinit();
        // 直接连接
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}

void bsp_board_wifi_reset_provisioning(bsp_board_t *bsp_board)
{
    wifi_prov_mgr_reset_provisioning();
    esp_restart();
}

int bsp_board_wifi_get_rssi(bsp_board_t *bsp_board)
{
    int rssi = 0;
    esp_err_t ret = esp_wifi_sta_get_rssi(&rssi);
    if (ret == ESP_OK)
    {
        return rssi;
    }
    return 0;
}
