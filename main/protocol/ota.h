#pragma once

/**
 * @file ota.h
 * @brief OTA(Over-The-Air)远程升级模块头文件
 * 
 * 该模块负责设备的远程固件升级和激活功能，
 * 通过HTTP协议与OTA服务器通信，获取设备激活状态
 * 和WebSocket连接信息。
 */

/** OTA服务器URL地址 */
#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"

/**
 * @brief OTA数据结构体
 * 
 * 存储OTA相关的核心信息：
 * - activation_code: 设备激活码，用于首次激活设备
 * - websocket_url: WebSocket服务器地址，用于后续通信
 * - websocket_token: WebSocket认证令牌
 */
typedef struct {
    char* activation_code;    /**< 设备激活码 */
    char* websocket_url;      /**< WebSocket服务器URL */
    char* websocket_token;    /**< WebSocket认证令牌 */
} ota_t;

/**
 * @brief 创建OTA实例
 * 
 * 分配并初始化OTA结构体内存
 * 
 * @return ota_t* 新创建的OTA实例指针，失败返回NULL
 */
ota_t* ota_create(void);

/**
 * @brief 销毁OTA实例
 * 
 * 释放OTA实例及其关联的所有内存资源
 * 
 * @param ota 要销毁的OTA实例指针
 */
void ota_destroy(ota_t* ota);

/**
 * @brief 执行OTA检查流程
 * 
 * 向OTA服务器发送设备信息，检查设备激活状态，
 * 并更新WebSocket连接信息。
 * 
 * @param ota OTA实例指针
 */
void ota_perform(ota_t* ota);
