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

/**
 * @brief OTA包装结构体
 * 
 * 包含OTA数据和HTTP响应缓冲区
 */
typedef struct
{
    ota_t ota;              // OTA基本信息
    char *response;         // HTTP响应数据缓冲区
    size_t response_len;    // 响应数据长度
} ota_wrapper_t;

/**
 * @brief 构建OTA请求的JSON数据体
 * 
 * 包含以下信息：
 * - 应用程序版本号
 * - 当前固件的SHA256哈希值
 * - 设备硬件信息
 * 
 * @return char* JSON字符串，需要调用free()释放内存
 */
static char *ota_get_post_body(void)
{
    cJSON *root = cJSON_CreateObject();

    // 添加应用程序信息
    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "version", "1.0.0");  // 应用版本

    // 获取当前运行分区的SHA256哈希值用于版本识别
    uint8_t sha256_digest[32];
    ESP_ERROR_CHECK(esp_partition_get_sha256(esp_ota_get_running_partition(), sha256_digest));
    char sha256_str[65];
    for (int i = 0; i < 32; i++)
    {
        sprintf(sha256_str + i * 2, "%02x", sha256_digest[i]);  // 转换为十六进制字符串
    }
    cJSON_AddStringToObject(application, "elf_sha256", sha256_str);

    cJSON_AddItemToObject(root, "application", application);

    // 添加设备硬件信息
    cJSON *board = cJSON_CreateObject();
    cJSON_AddStringToObject(board, "type", "atguigu-doorbell");     // 设备类型
    cJSON_AddStringToObject(board, "name", "atguigu-doorbell");     // 设备名称
    cJSON_AddStringToObject(board, "ssid", "abcdefgh");             // WiFi SSID占位符
    cJSON_AddNumberToObject(board, "rssi", -40);                    // 信号强度占位符

    cJSON_AddItemToObject(root, "board", board);

    // 转换为紧凑的JSON字符串
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/**
 * @brief HTTP客户端事件处理回调函数
 * 
 * 处理OTA请求过程中的各种HTTP事件：
 * - 错误事件：记录错误信息
 * - 数据接收事件：拼接响应数据
 * - 请求完成事件：记录响应内容
 * 
 * @param evt HTTP事件结构体指针
 * @return esp_err_t ESP_OK表示处理成功
 */
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)evt->user_data;
    
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        // HTTP请求发生错误
        int err_no = esp_http_client_get_errno(evt->client);
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR, errno: %d", err_no);
        break;
        
    case HTTP_EVENT_ON_DATA:
        // 接收到服务器响应数据，进行数据拼接
        // 首先检查HTTP状态码
        int status_code = esp_http_client_get_status_code(evt->client);
        if (status_code != 200)
        {
            ESP_LOGW(TAG, "Request status code: %d", status_code);
            return ESP_OK;
        }
        
        // 动态扩展响应缓冲区
        size_t new_len = ota_wrapper->response_len + evt->data_len;
        char *new_buffer = realloc(ota_wrapper->response, new_len);
        if (!new_buffer)
        {
            return ESP_FAIL;  // 内存分配失败
        }
        memcpy(new_buffer + ota_wrapper->response_len, evt->data, evt->data_len);
        ota_wrapper->response = new_buffer;
        ota_wrapper->response_len = new_len;
        break;
        
    case HTTP_EVENT_ON_FINISH:
        // HTTP请求完成，记录接收到的响应
        ESP_LOGD(TAG, "Response received: %.*s", ota_wrapper->response_len, ota_wrapper->response);
        break;
        
    default:
        ESP_LOGD(TAG, "Unhandled event");
        break;
    }
    return ESP_OK;
}

/**
 * @brief 创建OTA实例
 * 
 * 分配并初始化OTA包装结构体
 * 
 * @return ota_t* OTA实例指针，失败返回NULL
 */
ota_t *ota_create(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);  // 设置日志级别
    ota_wrapper_t *ota_wrapper = malloc_zeroed(sizeof(ota_wrapper_t));
    return (ota_t *)ota_wrapper;
}

/**
 * @brief 销毁OTA实例
 * 
 * 释放OTA实例占用的所有内存资源
 * 
 * @param ota OTA实例指针
 */
void ota_destroy(ota_t *ota)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)ota;
    
    // 释放所有动态分配的内存
    free(ota_wrapper->response);
    free(ota_wrapper->ota.activation_code);
    free(ota_wrapper->ota.websocket_token);
    free(ota_wrapper->ota.websocket_url);
}

/**
 * @brief 执行OTA检查和激活流程
 * 
 * 主要功能：
 * 1. 向OTA服务器发送设备信息请求
 * 2. 解析服务器响应获取激活状态
 * 3. 提取WebSocket连接信息
 * 4. 更新OTA实例中的相关信息
 * 
 * @param ota OTA实例指针
 */
void ota_perform(ota_t *ota)
{
    ota_wrapper_t *ota_wrapper = (ota_wrapper_t *)ota;
    esp_err_t ret = ESP_OK;
    bsp_board_t *board = bsp_board_get_instance();
    
    // 清理之前的响应数据
    free(ota_wrapper->response);
    ota_wrapper->response = NULL;
    ota_wrapper->response_len = 0;

    // 构建OTA HTTP POST请求配置
    esp_http_client_config_t config = {
        .url = OTA_URL,                         // OTA服务器URL
        .crt_bundle_attach = esp_crt_bundle_attach,  // SSL证书验证
        .method = HTTP_METHOD_POST,             // POST方法
        .event_handler = ota_http_event_handler,     // 事件回调
        .user_data = ota_wrapper,               // 用户数据传递
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 添加HTTP请求头
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Device-Id", board->mac_addr);    // 设备MAC地址
    esp_http_client_set_header(client, "Client-Id", board->uuid);        // 设备UUID
    esp_http_client_set_header(client, "User-Agent", "atguigu-doorbell/1.0.0");  // 用户代理

    // 构建并添加POST请求体
    char *post_body = ota_get_post_body();
    esp_http_client_set_post_field(client, post_body, strlen(post_body));

    // 发送HTTP请求
    ret = esp_http_client_perform(client);
    free(post_body);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to send OTA request: %s", esp_err_to_name(ret));
        return;
    }

    // 检查HTTP响应状态码
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (status_code != 200)
    {
        ESP_LOGW(TAG, "OTA request failed with status code: %d", status_code);
        return;
    }

    // 解析服务器JSON响应
    cJSON *root = cJSON_ParseWithLength(ota_wrapper->response, ota_wrapper->response_len);
    if (!root)
    {
        ESP_LOGW(TAG, "Failed to parse OTA response");
        return;
    }

    // 解析激活码信息
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

    // 解析WebSocket连接信息
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
