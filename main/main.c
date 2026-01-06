#include "application.h"
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "Main"

void heap_monitor_task(void *arg)
{
    while (1)
    {
        uint32_t heap_size = esp_get_free_internal_heap_size();
        ESP_LOGE(TAG, "heap size: %lu", heap_size);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // xTaskCreate(heap_monitor_task, "heap_monitor_task", 4096, NULL, 5, NULL);
    application_init();
}
