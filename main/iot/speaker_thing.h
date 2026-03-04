/**
 * @file speaker_thing.h
 * @brief 扬声器设备（Speaker Thing）头文件
 * 
 * 定义并实现一个具体的 IoT 设备 - 扬声器：
 * - 属性：静音状态、音量大小
 * - 方法：设置静音、设置音量
 */

#pragma once 

#include "things.h"

/**
 * @brief 创建扬声器设备实例
 * 
 * 初始化扬声器设备的属性和方法，包括：
 * - 静音属性（布尔类型）
 * - 音量属性（数字类型）
 * - 设置静音方法
 * - 设置音量方法
 * 
 * @return thing_t* 创建的扬声器设备指针
 */
thing_t* speaker_thing_create();
