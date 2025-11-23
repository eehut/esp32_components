/**
 * @file time_sync.c
 * @author LiuChuansen (179712066@qq.com)
 * @brief 时间同步组件实现 - 简化的SNTP封装
 * @version 0.2
 * @date 2025-01-XX
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "time_sync.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "uptime.h"
#include "app_event_loop.h"

static const char *TAG = "time_sync";

#define NVS_NAMESPACE "time_sync"
#define NVS_KEY_SERVER_URL "server_url"
#define NVS_KEY_SYNC_INTERVAL "sync_interval"

#define DEFAULT_SERVER_URL "cn.pool.ntp.org"
#define DEFAULT_TIMEZONE "CST-8"

// 定义事件基础
ESP_EVENT_DEFINE_BASE(TIME_SYNC_EVENT);

typedef struct {
    bool initialized;
    bool started;
    time_sync_config_t config;
    time_sync_status_t status;
    TimerHandle_t check_timer;  // 用于检查同步状态
    nvs_handle_t nvs_handle;
} time_sync_ctx_t;

static time_sync_ctx_t s_ctx = {0};

static void sntp_sync_notification_cb(struct timeval *tv);
static void check_timer_callback(TimerHandle_t xTimer);
static esp_err_t load_config_from_nvs(void);
static esp_err_t save_config_to_nvs(void);

static void sntp_sync_notification_cb(struct timeval *tv)
{
    s_ctx.status.synced = true;
    s_ctx.status.synced_time = uptime();
    
    // 确保服务器地址已设置
    strncpy(s_ctx.status.server_url, s_ctx.config.server_url, 
            sizeof(s_ctx.status.server_url) - 1);
    
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    ESP_LOGI(TAG, "Time synchronized successfully from %s", s_ctx.status.server_url);
    ESP_LOGI(TAG, "Current time: %s", time_str);
    
    // 发送同步成功事件
    app_event_post(TIME_SYNC_EVENT, TIME_SYNC_EVENT_COMPLETED, NULL, 0, 0);
}

static void check_timer_callback(TimerHandle_t xTimer)
{
    if (!s_ctx.started) {
        return;
    }
    
    // 检查SNTP同步状态
    sntp_sync_status_t sync_status = sntp_get_sync_status();
    
    if (sync_status == SNTP_SYNC_STATUS_COMPLETED && !s_ctx.status.synced) {
        // 首次同步完成
        s_ctx.status.synced = true;
        s_ctx.status.synced_time = uptime();
        ESP_LOGI(TAG, "Time sync completed");
    } else if (sync_status == SNTP_SYNC_STATUS_RESET && s_ctx.status.synced) {
        // 同步状态被重置，等待下次同步
        ESP_LOGD(TAG, "Waiting for next sync");
    }
}

static esp_err_t load_config_from_nvs(void)
{
    esp_err_t ret;
    size_t required_size;

    // 加载服务器地址
    required_size = sizeof(s_ctx.config.server_url);
    ret = nvs_get_str(s_ctx.nvs_handle, NVS_KEY_SERVER_URL, 
                      s_ctx.config.server_url, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded server URL from NVS: %s", s_ctx.config.server_url);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Server URL not found in NVS, using default");
        strncpy(s_ctx.config.server_url, DEFAULT_SERVER_URL, 
                sizeof(s_ctx.config.server_url) - 1);
    } else {
        ESP_LOGW(TAG, "Failed to load server URL from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // 加载同步间隔
    ret = nvs_get_u32(s_ctx.nvs_handle, NVS_KEY_SYNC_INTERVAL, &s_ctx.config.sync_interval);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded sync interval from NVS: %lu seconds", s_ctx.config.sync_interval);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Sync interval not found in NVS, using default");
        s_ctx.config.sync_interval = TIME_SYNC_SYNC_INTERVAL_DEFAULT;
    } else {
        ESP_LOGW(TAG, "Failed to load sync interval from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static esp_err_t save_config_to_nvs(void)
{
    esp_err_t ret;

    // 保存服务器地址
    ret = nvs_set_str(s_ctx.nvs_handle, NVS_KEY_SERVER_URL, s_ctx.config.server_url);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save server URL: %s", esp_err_to_name(ret));
        return ret;
    }

    // 保存同步间隔
    ret = nvs_set_u32(s_ctx.nvs_handle, NVS_KEY_SYNC_INTERVAL, s_ctx.config.sync_interval);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save sync interval: %s", esp_err_to_name(ret));
        return ret;
    }

    // 提交更改
    ret = nvs_commit(s_ctx.nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Config saved to NVS");
    return ESP_OK;
}

esp_err_t time_sync_init(void)
{
    if (s_ctx.initialized) {
        ESP_LOGW(TAG, "Time sync already initialized");
        return ESP_OK;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));

    // 设置默认配置
    strncpy(s_ctx.config.server_url, DEFAULT_SERVER_URL, 
            sizeof(s_ctx.config.server_url) - 1);
    s_ctx.config.sync_interval = TIME_SYNC_SYNC_INTERVAL_DEFAULT;

    // 打开NVS命名空间
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_ctx.nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // 从NVS加载配置
    ret = load_config_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config from NVS, using defaults");
    }

    // 设置时区
    setenv("TZ", DEFAULT_TIMEZONE, 1);
    tzset();

    // 创建状态检查定时器（每5秒检查一次）
    s_ctx.check_timer = xTimerCreate(
        "time_sync_check",
        pdMS_TO_TICKS(5000),
        pdTRUE,  // 自动重载
        NULL,
        check_timer_callback
    );

    if (s_ctx.check_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create check timer");
        nvs_close(s_ctx.nvs_handle);
        return ESP_ERR_NO_MEM;
    }

    s_ctx.initialized = true;
    ESP_LOGI(TAG, "Time sync initialized, server: %s, interval: %lu seconds", 
             s_ctx.config.server_url, s_ctx.config.sync_interval);

    return ESP_OK;
}

esp_err_t time_sync_set_config(const time_sync_config_t *config)
{
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "Time sync not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    // 验证服务器地址
    if (strlen(config->server_url) == 0 || 
        strlen(config->server_url) >= TIME_SYNC_SERVER_URL_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    // 验证同步间隔（0表示只同步一次，或者在有效范围内）
    if (config->sync_interval != 0 && 
        (config->sync_interval < TIME_SYNC_SYNC_INTERVAL_MIN || 
         config->sync_interval > TIME_SYNC_SYNC_INTERVAL_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 保存配置
    memcpy(&s_ctx.config, config, sizeof(time_sync_config_t));

    // 保存到NVS
    esp_err_t ret = save_config_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save config to NVS: %s", esp_err_to_name(ret));
    }

    // 如果已经启动，重启SNTP以应用新配置
    if (s_ctx.started) {
        ESP_LOGI(TAG, "Restarting SNTP with new config");
        esp_sntp_stop();
        
        esp_sntp_setservername(0, s_ctx.config.server_url);
        
        if (s_ctx.config.sync_interval > 0) {
            sntp_set_sync_interval(s_ctx.config.sync_interval * 1000);
        }
        
        sntp_restart();
    }

    ESP_LOGI(TAG, "Config updated: server=%s, interval=%lu", 
             s_ctx.config.server_url, s_ctx.config.sync_interval);

    return ESP_OK;
}

esp_err_t time_sync_get_config(time_sync_config_t *config)
{
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "Time sync not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(config, &s_ctx.config, sizeof(time_sync_config_t));
    return ESP_OK;
}

esp_err_t time_sync_get_status(time_sync_status_t *status)
{
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "Time sync not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    // 更新状态信息
    // 如果已经标记为已同步，保持该状态
    // 否则检查SNTP状态或系统时间来判断是否已同步
    if (!s_ctx.status.synced && s_ctx.started) {
        sntp_sync_status_t sync_status = sntp_get_sync_status();
        if (sync_status == SNTP_SYNC_STATUS_COMPLETED) {
            // 同步刚完成，更新状态
            s_ctx.status.synced = true;
            s_ctx.status.synced_time = uptime();
        } else {
            // 检查系统时间是否有效（如果时间同步成功，time()应该返回有效值）
            time_t now = time(NULL);
            if (now > 946684800) {  // 2000-01-01 00:00:00 之后的时间戳
                // 系统时间有效，说明已经同步过
                s_ctx.status.synced = true;
                // 如果synced_time为0，说明是首次检测到，设置一个合理的值
                if (s_ctx.status.synced_time == 0) {
                    s_ctx.status.synced_time = uptime();
                }
            }
        }
    }
    
    strncpy(s_ctx.status.server_url, s_ctx.config.server_url, 
            sizeof(s_ctx.status.server_url) - 1);
    s_ctx.status.sync_interval = s_ctx.config.sync_interval;

    memcpy(status, &s_ctx.status, sizeof(time_sync_status_t));
    return ESP_OK;
}

esp_err_t time_sync_start(void)
{
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "Time sync not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.started) {
        ESP_LOGW(TAG, "Time sync already started");
        return ESP_OK;
    }

    // 初始化SNTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, s_ctx.config.server_url);
    sntp_set_time_sync_notification_cb(sntp_sync_notification_cb);
    
    // 设置同步间隔
    if (s_ctx.config.sync_interval > 0) {
        sntp_set_sync_interval(s_ctx.config.sync_interval * 1000);
        ESP_LOGI(TAG, "Sync interval set to %lu seconds", s_ctx.config.sync_interval);
    } else {
        // 0表示只同步一次，设置一个很大的值
        sntp_set_sync_interval(86400000);  // 24小时
        ESP_LOGI(TAG, "Sync once mode enabled");
    }
    
    esp_sntp_init();

    s_ctx.started = true;
    
    // 更新状态
    strncpy(s_ctx.status.server_url, s_ctx.config.server_url, 
            sizeof(s_ctx.status.server_url) - 1);
    s_ctx.status.sync_interval = s_ctx.config.sync_interval;
    s_ctx.status.synced = false;

    // 启动状态检查定时器
    if (s_ctx.check_timer) {
        xTimerStart(s_ctx.check_timer, 0);
    }

    ESP_LOGI(TAG, "Time sync started, server: %s", s_ctx.config.server_url);
    return ESP_OK;
}

esp_err_t time_sync_stop(void)
{
    if (!s_ctx.started) {
        return ESP_OK;
    }

    // 停止SNTP
    esp_sntp_stop();

    // 停止定时器
    if (s_ctx.check_timer) {
        xTimerStop(s_ctx.check_timer, 0);
    }

    s_ctx.started = false;
    s_ctx.status.synced = false;

    ESP_LOGI(TAG, "Time sync stopped");
    return ESP_OK;
}
