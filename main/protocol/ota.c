#include "ota.h"
#include "object.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "bsp/bsp_board.h"
#include "cJSON.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

#define TAG "OTA"

typedef struct
{
    ota_t ota;
    char *response;
    size_t response_len;
} ota_wrapper_t;

// 使用这个函数构建post_body, 返回的字符串需要调用free()来释放
static char *ota_get_post_body(void)
{
    cJSON *root = cJSON_CreateObject();

    // 添加application字段
    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "version", "1.0.0");

    // 获取运行分区的hash
    uint8_t sha256_digest[32];
    ESP_ERROR_CHECK(esp_partition_get_sha256(esp_ota_get_running_partition(), sha256_digest));
    char sha256_str[65];
    for (int i = 0; i < 32; i++)
    {
        sprintf(sha256_str + i * 2, "%02x", sha256_digest[i]);
    }
    cJSON_AddStringToObject(application, "elf_sha256", sha256_str);

    cJSON_AddItemToObject(root, "application", application);

    // 添加board字段
    cJSON *board = cJSON_CreateObject();
    cJSON_AddStringToObject(board, "type", "atguigu-doorbell");
    cJSON_AddStringToObject(board, "name", "atguigu-doorbell");
    cJSON_AddStringToObject(board, "ssid", "abcdefgh");
    cJSON_AddNumberToObject(board, "rssi", -40);

    cJSON_AddItemToObject(root, "board", board);

    // 转换为字符串
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)evt->user_data;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        int err_no = esp_http_client_get_errno(evt->client);
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR, errno: %d", err_no);
        break;
    case HTTP_EVENT_ON_DATA:
        // 收到服务器数据，做数据拼接
        // 首先检查状态码：
        int status_code = esp_http_client_get_status_code(evt->client);
        if (status_code != 200)
        {
            ESP_LOGW(TAG, "Request status code: %d", status_code);
            return ESP_OK;
        }
        size_t new_len = ota_wrapper->response_len + evt->data_len;
        char *new_buffer = realloc(ota_wrapper->response, new_len);
        if (!new_buffer)
        {
            return ESP_FAIL;
        }
        memcpy(new_buffer + ota_wrapper->response_len, evt->data, evt->data_len);
        ota_wrapper->response = new_buffer;
        ota_wrapper->response_len = new_len;
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "Response received: %.*s", ota_wrapper->response_len, ota_wrapper->response);
        break;
    default:
        ESP_LOGD(TAG, "Unhandled event");
        break;
    }
    return ESP_OK;
}

ota_t *ota_create(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ota_wrapper_t *ota_wrapper = malloc_zeroed(sizeof(ota_wrapper_t));
    return (ota_t *)ota_wrapper;
}

void ota_destroy(ota_t *ota)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)ota;
    free(ota_wrapper->response);
    free(ota_wrapper->ota.activation_code);
    free(ota_wrapper->ota.websocket_token);
    free(ota_wrapper->ota.websocket_url);
}

void ota_perform(ota_t *ota)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)ota;
    esp_err_t ret = ESP_OK;
    bsp_board_t *board = bsp_board_get_instance();
    free(ota_wrapper->response);
    ota_wrapper->response = NULL;
    ota_wrapper->response_len = 0;

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
    cJSON *root = cJSON_ParseWithLength(ota_wrapper->response, ota_wrapper->response_len);
    if (!root)
    {
        ESP_LOGW(TAG, "Failed to parse OTA response");
        return;
    }

    // 解析activation
    free(ota_wrapper->ota.activation_code);
    ota_wrapper->ota.activation_code = NULL;

    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (activation)
    {
        cJSON *activation_code = cJSON_GetObjectItem(activation, "code");
        if (cJSON_IsString(activation_code))
        {
            ota_wrapper->ota.activation_code = strdup(activation_code->valuestring);
        }
    }

    // 解析websocket
    free(ota_wrapper->ota.websocket_token);
    ota_wrapper->ota.websocket_token = NULL;
    free(ota_wrapper->ota.websocket_url);
    ota_wrapper->ota.websocket_url = NULL;

    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (websocket)
    {
        cJSON *token = cJSON_GetObjectItem(websocket, "token");
        if (cJSON_IsString(token))
        {
            ota_wrapper->ota.websocket_token = strdup(token->valuestring);
        }
        cJSON *url = cJSON_GetObjectItem(websocket, "url");
        if (cJSON_IsString(url))
        {
            ota_wrapper->ota.websocket_url = strdup(url->valuestring);
        }
    }

    cJSON_Delete(root);
}
