/**
 * @file main.c
 * @brief 应用程序主入口文件
 * 
 * 该文件包含 ESP-IDF 应用程序的入口函数 app_main()，
 * 负责启动整个系统的初始化流程。
 */

#include "application.h"
#include "iot/mylist.h"

/**
 * @brief 应用程序主入口函数
 * 
 * ESP-IDF 框架会在系统启动后自动调用此函数，
 * 作为整个应用程序的执行起点。
 * 
 * 主要功能:
 * - 调用 application_init() 初始化所有系统模块
 * - 包括硬件驱动、网络、音频处理、UI 等组件
 */
void app_main(void)
{
    // 初始化应用程序，启动所有系统模块
    application_init();
}
