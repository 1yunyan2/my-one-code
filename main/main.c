#include <stdio.h>
#include "task_runner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void print_something(void *arg)
{
    char *str = (char *)arg;
    printf("%s\n", str);
}

void app_main(void)
{
    task_runner_handle_t runner1 = task_runner_create();
    task_runner_handle_t runner2 = task_runner_create();

    task_runner_add(runner1, print_something, "Hello World!");
    task_runner_add(runner2, print_something, "xxyyzz!");
    task_runner_start(runner1);
    vTaskDelay(pdMS_TO_TICKS(500));
    task_runner_start(runner2);
    vTaskDelay(pdMS_TO_TICKS(5000));
    task_runner_stop(runner1);
    task_runner_stop(runner2);
    vTaskDelay(pdMS_TO_TICKS(1000));
    task_runner_delete(runner1);
    task_runner_delete(runner2);

}
