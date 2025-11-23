/**
 * @file time_sync.h
 * @author LiuChuansen (179712066@qq.com)
 * @brief 时间同步组件头文件 - 简化的SNTP封装
 * @version 0.2
 * @date 2025-01-XX
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明，避免包含uptime.h
typedef uint32_t sys_tick_t;

#define TIME_SYNC_SERVER_URL_MAX_LEN 64
#define TIME_SYNC_SYNC_INTERVAL_MIN 60      // 最小同步间隔：60秒
#define TIME_SYNC_SYNC_INTERVAL_MAX 86400   // 最大同步间隔：24小时
#define TIME_SYNC_SYNC_INTERVAL_DEFAULT 3600 // 默认同步间隔：1小时

/**
 * @brief 时间同步配置结构体
 */
typedef struct {
    char server_url[TIME_SYNC_SERVER_URL_MAX_LEN];  ///< NTP服务器地址
    uint32_t sync_interval;  ///< 同步间隔（秒），0表示只同步一次
} time_sync_config_t;

/**
 * @brief 时间同步状态结构体
 */
typedef struct {
    bool synced;                ///< 是否已同步
    sys_tick_t synced_time;     ///< 同步时间（系统tick）
    char server_url[TIME_SYNC_SERVER_URL_MAX_LEN];  ///< 当前服务器地址
    uint32_t sync_interval;     ///< 同步间隔（秒）
} time_sync_status_t;

/**
 * @brief 时间同步事件基础
 */
ESP_EVENT_DECLARE_BASE(TIME_SYNC_EVENT);

/**
 * @brief 时间同步事件ID
 */
typedef enum {
    TIME_SYNC_EVENT_COMPLETED,   ///< 时间同步完成
    TIME_SYNC_EVENT_FAILED,      ///< 时间同步失败
} time_sync_event_id_t;

/**
 * @brief 初始化时间同步组件
 * 
 * @return ESP_OK 成功
 *         其他错误码 失败
 */
esp_err_t time_sync_init(void);

/**
 * @brief 设置时间同步配置
 * 
 * @param config 配置结构体指针
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         其他错误码 失败
 */
esp_err_t time_sync_set_config(const time_sync_config_t *config);

/**
 * @brief 获取时间同步配置
 * 
 * @param config 配置结构体指针，用于存储结果
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         其他错误码 失败
 */
esp_err_t time_sync_get_config(time_sync_config_t *config);

/**
 * @brief 获取时间同步状态
 * 
 * @param status 状态结构体指针，用于存储结果
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         其他错误码 失败
 */
esp_err_t time_sync_get_status(time_sync_status_t *status);

/**
 * @brief 启动时间同步
 * 
 * @return ESP_OK 成功
 *         其他错误码 失败
 */
esp_err_t time_sync_start(void);

/**
 * @brief 停止时间同步
 * 
 * @return ESP_OK 成功
 *         其他错误码 失败
 */
esp_err_t time_sync_stop(void);

#ifdef __cplusplus
}
#endif
