#include "bsp_board.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_random.h"

static bsp_board_t bsp_board = {0};

bsp_board_t *bsp_board_get_instance(void)
{
    if (!bsp_board.board_status)
    {
        bsp_board.board_status = xEventGroupCreate();
    }
    return &bsp_board;
}

void bsp_board_nvs_init(bsp_board_t *bsp_board)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    xEventGroupSetBits(bsp_board->board_status, NVS_BIT);

    // 尝试获取UUID
    nvs_handle_t nvs_handle = 0;
    ESP_ERROR_CHECK(nvs_open("Settings", NVS_READWRITE, &nvs_handle));

    size_t length = 37;
    ret = nvs_get_str(nvs_handle, "uuid", bsp_board->uuid, &length);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // 随机生成一个新的UUID
        uint8_t data[16];
        esp_fill_random(data, 16);
        // 设置UUID v4的版本和变体位
        // 第6个字节的高4位设置为4 (版本4)
        data[6] = (data[6] & 0x0F) | 0x40;
        // 第8个字节的高2位设置为10 (变体1)
        data[8] = (data[8] & 0x3F) | 0x80;

        snprintf(bsp_board->uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 data[0], data[1], data[2], data[3],
                 data[4], data[5], data[6], data[7],
                 data[8], data[9], data[10], data[11],
                 data[12], data[13], data[14], data[15]);
        // 写入nvs
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "uuid", bsp_board->uuid));
        ESP_ERROR_CHECK(nvs_commit(nvs_handle));

        ret = ESP_OK;
    }
    ESP_ERROR_CHECK(ret);
    nvs_close(nvs_handle);
}

bool bsp_board_check_status(bsp_board_t *bsp_board, EventBits_t bits_to_check, TickType_t wait_ticks)
{
    EventBits_t bits = xEventGroupWaitBits(bsp_board->board_status, bits_to_check, pdFALSE, pdTRUE, wait_ticks);
    return (bits & bits_to_check) == bits_to_check;
}
