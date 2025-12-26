#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "task_runner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void task_runner_run(void *arg)
{
    task_runner_t *task_runner = (task_runner_t *)arg;
    while (task_runner->is_running)
    {
        for (size_t i = 0; i < task_runner->count; i++)
        {
            task_runner->tasks[i].callback(task_runner->tasks[i].arg);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

void task_runner_init(task_runner_t* task_runner)
{
    task_runner->tasks = NULL;
    task_runner->size = 0;
    task_runner->count = 0;
}

void task_runner_deinit(task_runner_t* task_runner)
{
    free(task_runner->tasks);
    task_runner->tasks = NULL;
    task_runner->size = 0;
    task_runner->count = 0;
}

void task_runner_add(task_runner_t* task_runner,task_callback_t callback, void *arg)
{
    if (task_runner->count >= task_runner->size)
    {
        // 扩容
        uint32_t new_size = task_runner->size == 0 ? 1 : task_runner->size * 2;
        task_t *new_tasks = realloc(task_runner->tasks, new_size * sizeof(task_t));
        if (!new_tasks)
        {
            return;
        }
        task_runner->tasks = new_tasks;
        task_runner->size = new_size;
    }
    task_runner->tasks[task_runner->count].callback = callback;
    task_runner->tasks[task_runner->count].arg = arg;
    task_runner->count++;
}

void task_runner_start(task_runner_t* task_runner)
{
    task_runner->is_running = true;
    xTaskCreate(task_runner_run, "task_runner", 4096, task_runner, 5, NULL);
}

void task_runner_stop(task_runner_t* task_runner)
{
    task_runner->is_running = false;
}
