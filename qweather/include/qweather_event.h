/**
 * @file qweather_event.h
 * @brief 和风天气事件类型定义
 * @version 0.1
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
#include "qweather.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 和风天气事件基础
 */
ESP_EVENT_DECLARE_BASE(QWEATHER_EVENTS);

/**
 * @brief 和风天气事件ID
 */
typedef enum {
    QWEATHER_EVENT_UPDATE,      ///< 天气更新完成
} qweather_event_id_t;

/**
 * @brief 和风天气事件数据结构
 */
typedef struct {
    qweather_info_t info;       ///< 天气信息
} qweather_event_data_t;

#ifdef __cplusplus
}
#endif

