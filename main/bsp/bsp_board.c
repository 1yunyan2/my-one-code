#include "bsp_board.h"
#include "nvs_flash.h"

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
}

bool bsp_board_check_status(bsp_board_t *bsp_board, EventBits_t bits_to_check, TickType_t wait_ticks)
{
    EventBits_t bits = xEventGroupWaitBits(bsp_board->board_status, bits_to_check, pdFALSE, pdTRUE, wait_ticks);
    return (bits & bits_to_check) == bits_to_check;
}
