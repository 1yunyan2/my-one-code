/**
 * @file task_runner.h
 * @author your name (you@domain.com)
 * @brief task调用器，可以将注册的task每隔1s调用一次
 * @version 0.1
 * @date 2025-12-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <stdbool.h>

typedef struct task_runner *task_runner_handle_t;
typedef void (*task_callback_t)(void *);

task_runner_handle_t task_runner_create(void);

void task_runner_delete(task_runner_handle_t runner);

void task_runner_add(task_runner_handle_t runner,task_callback_t callback, void *arg);

void task_runner_start(task_runner_handle_t runner);

void task_runner_stop(task_runner_handle_t runner);
