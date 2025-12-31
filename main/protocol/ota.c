#include "ota.h"
#include "object.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "bsp/bsp_board.h"

#define TAG "OTA"

typedef struct
{
    ota_t ota;
} ota_wrapper_t;

// 使用这个函数构建post_body, 返回的字符串需要调用free()来释放
static char *ota_get_post_body(void) {}

static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt)
{
}

ota_t *ota_create(void)
{
    ota_wrapper_t *ota_wrapper = malloc_zeroed(sizeof(ota_wrapper_t));
    return (ota_t *)ota_wrapper;
}

void ota_destroy(ota_t *ota)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)ota;
    free(ota_wrapper->ota.activation_code);
    free(ota_wrapper->ota.websocket_token);
    free(ota_wrapper->ota.websocket_url);
}

void ota_perform(ota_t *ota)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)ota;
    esp_err_t ret = ESP_OK;
    bsp_board_t *board = bsp_board_get_instance();

    // 构建OTA POST请求
    esp_http_client_config_t config = {
        .url = OTA_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST,
        .event_handler = ota_http_event_handler,
        .user_data = ota_wrapper,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 添加自定义Header
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Device-Id", board->mac_addr);
    esp_http_client_set_header(client, "Client-Id", board->uuid);
    esp_http_client_set_header(client, "User-Agent", "atguigu-doorbell/1.0.0");

    // 添加Post Body
    char *post_body = ota_get_post_body();
    esp_http_client_set_post_field(client, post_body, strlen(post_body));

    // 发送请求
    ret = esp_http_client_perform(client);
    free(post_body);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to send OTA request: %s", esp_err_to_name(ret));
        return;
    }

    // 检查状态码
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (status_code != 200)
    {
        ESP_LOGW(TAG, "OTA request failed with status code: %d", status_code);
        return;
    }

    // 解析响应
}
