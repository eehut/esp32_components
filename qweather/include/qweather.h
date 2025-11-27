/**
 * @file qweather.h
 * @brief 和风天气组件API头文件
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

#ifdef __cplusplus
extern "C" {
#endif

#define QWEATHER_PROJECT_ID_MAX_LEN 32
#define QWEATHER_KEY_ID_MAX_LEN 32
#define QWEATHER_API_HOST_MAX_LEN 128
#define QWEATHER_PRIVATE_KEY_MAX_LEN 512
#define QWEATHER_TEXT_MAX_LEN 32

/**
 * @brief 和风天气信息结构体
 */
typedef struct {
    bool valid;                     ///< 是否有效
    int status_code;                ///< 状态码：200表示成功，HTTP状态码或自定义错误码（>=1000）
    uint32_t location_code;         ///< 地区代码
    float temperature;              ///< 温度（摄氏度）
    float humidity;                 ///< 湿度（百分比）
    char weather_text[QWEATHER_TEXT_MAX_LEN];  ///< 天气文本（如：多云）
    uint16_t weather_icon;          ///< 天气图标ID
    uint32_t update_time;           ///< 更新时间（Unix时间戳）
} qweather_info_t;

/**
 * @brief 初始化和风天气组件
 * 
 * @param project_id 项目ID指针（必须保持有效）
 * @param key_id 密钥ID指针（必须保持有效）
 * @param api_host API主机地址指针（必须保持有效）
 * @param private_key Ed25519私钥（PEM格式）指针（必须保持有效）
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效或配置不完整
 *         其他错误码 失败
 * 
 * @note 配置参数使用指针传递，不会复制数据，因此传入的字符串必须在组件使用期间保持有效
 */
esp_err_t qweather_init(const char *project_id, const char *key_id, const char *api_host, const char *private_key);

/**
 * @brief 异步查询天气信息
 * 
 * 在独立任务中执行查询，完成后通过app_event发送事件
 * 
 * @param location_code 地区代码
 * @return ESP_OK 请求已提交
 *         ESP_ERR_INVALID_STATE 配置无效或未初始化
 *         其他错误码 失败
 */
esp_err_t qweather_query_async(uint32_t location_code);

/**
 * @brief 同步查询天气信息
 * 
 * 阻塞等待查询完成并返回结果
 * 
 * @param location_code 地区代码
 * @param info 天气信息结构体指针，用于存储结果
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_INVALID_STATE 配置无效或未初始化
 *         其他错误码 失败
 */
esp_err_t qweather_query(uint32_t location_code, qweather_info_t *info);

#ifdef __cplusplus
}
#endif

