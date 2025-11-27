/**
 * @file qweather.c
 * @brief 和风天气组件实现
 * @version 0.1
 * @date 2025-01-XX
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "qweather.h"
#include "qweather_event.h"
#include "app_event_loop.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "tweetnacl.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// 使用ESP-IDF官方的zlib组件进行gzip解压
#include "zlib.h"
#define HAVE_ZLIB 1

// 确保strptime可用
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

static const char *TAG = "qweather";

// 定义事件基础
ESP_EVENT_DEFINE_BASE(QWEATHER_EVENTS);

// 用于在事件处理中访问JWT token（线程安全：每次请求时设置，请求完成后清除）
static char s_jwt_token_buffer[512] = {0};

// 用于在HTTP事件处理中收集响应数据
typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t data_len;
    bool collecting;
    bool is_gzip;  // 标记响应是否为gzip压缩
} http_response_ctx_t;

static http_response_ctx_t s_http_response_ctx = {0};

// JWT Token缓存
typedef struct {
    char token[512];
    time_t expires_at;
} jwt_cache_t;

// 上下文结构
typedef struct {
    bool initialized;
    const char *project_id;         ///< 项目ID指针
    const char *key_id;             ///< 密钥ID指针
    const char *api_host;            ///< API主机地址指针
    const char *private_key;         ///< Ed25519私钥（PEM格式）指针
    SemaphoreHandle_t config_mutex;
    jwt_cache_t jwt_cache;
    SemaphoreHandle_t jwt_mutex;
    bool query_task_running;        ///< 查询任务是否正在运行
    SemaphoreHandle_t query_mutex;  ///< 保护查询任务状态的互斥锁
} qweather_ctx_t;

static qweather_ctx_t s_ctx = {0};

// 错误码定义
#define QWEATHER_ERR_CONFIG_INVALID 1001
#define QWEATHER_ERR_NETWORK_ERROR 1002
#define QWEATHER_ERR_JSON_PARSE 1003
#define QWEATHER_ERR_JWT_GENERATE 1004

/**
 * @brief 验证配置是否有效
 */
static bool validate_config(const char *project_id, const char *key_id, 
                           const char *api_host, const char *private_key)
{
    if (project_id == NULL || key_id == NULL || api_host == NULL || private_key == NULL) {
        return false;
    }
    
    // 检查必要字段是否为空
    if (strlen(project_id) == 0 ||
        strlen(key_id) == 0 ||
        strlen(api_host) == 0 ||
        strlen(private_key) == 0) {
        return false;
    }
    
    // 检查私钥是否包含 BEGIN 和 END 标记
    if (strstr(private_key, "BEGIN PRIVATE KEY") == NULL ||
        strstr(private_key, "END PRIVATE KEY") == NULL) {
        ESP_LOGW(TAG, "Private key may be missing BEGIN/END markers");
        return false;
    }
    
    return true;
}

/**
 * @brief 初始化组件
 */
esp_err_t qweather_init(const char *project_id, const char *key_id, const char *api_host, const char *private_key)
{
    if (project_id == NULL || key_id == NULL || api_host == NULL || private_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!validate_config(project_id, key_id, api_host, private_key)) {
        ESP_LOGE(TAG, "Invalid config: missing required fields");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_ctx.initialized) {
        ESP_LOGW(TAG, "QWeather already initialized, reinitializing with new config");
    }
    
    // 创建互斥锁（如果尚未创建）
    if (s_ctx.config_mutex == NULL) {
        s_ctx.config_mutex = xSemaphoreCreateMutex();
        if (s_ctx.config_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create config mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    
    if (s_ctx.jwt_mutex == NULL) {
        s_ctx.jwt_mutex = xSemaphoreCreateMutex();
        if (s_ctx.jwt_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create JWT mutex");
            vSemaphoreDelete(s_ctx.config_mutex);
            s_ctx.config_mutex = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    
    if (s_ctx.query_mutex == NULL) {
        s_ctx.query_mutex = xSemaphoreCreateMutex();
        if (s_ctx.query_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create query mutex");
            vSemaphoreDelete(s_ctx.config_mutex);
            vSemaphoreDelete(s_ctx.jwt_mutex);
            s_ctx.config_mutex = NULL;
            s_ctx.jwt_mutex = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    
    // 存储配置指针
    if (xSemaphoreTake(s_ctx.config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_ctx.project_id = project_id;
    s_ctx.key_id = key_id;
    s_ctx.api_host = api_host;
    s_ctx.private_key = private_key;
    
    xSemaphoreGive(s_ctx.config_mutex);
    
    // 清除JWT缓存（因为配置可能已更改）
    if (xSemaphoreTake(s_ctx.jwt_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memset(&s_ctx.jwt_cache, 0, sizeof(s_ctx.jwt_cache));
        xSemaphoreGive(s_ctx.jwt_mutex);
    }
    
    // 重置查询任务状态
    if (xSemaphoreTake(s_ctx.query_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_ctx.query_task_running = false;
        xSemaphoreGive(s_ctx.query_mutex);
    }
    
    s_ctx.initialized = true;
    ESP_LOGI(TAG, "QWeather component initialized");
    return ESP_OK;
}

/**
 * @brief Base64URL编码（JWT使用）
 */
static int base64url_encode(const unsigned char *input, size_t input_len, 
                            char *output, size_t *output_len)
{
    // 先进行标准Base64编码
    size_t base64_len = 0;
    int ret = mbedtls_base64_encode((unsigned char *)output, *output_len, 
                                    &base64_len, input, input_len);
    if (ret != 0) {
        return ret;
    }
    
    // 转换为Base64URL：替换 + -> -, / -> _, 移除padding =
    size_t base64url_len = 0;
    for (size_t i = 0; i < base64_len; i++) {
        if (output[i] == '+') {
            output[base64url_len++] = '-';
        } else if (output[i] == '/') {
            output[base64url_len++] = '_';
        } else if (output[i] == '=') {
            // 跳过padding，Base64URL不使用padding
            break;
        } else {
            output[base64url_len++] = output[i];
        }
    }
    
    output[base64url_len] = '\0';
    *output_len = base64url_len;
    return 0;
}

/**
 * @brief 创建JWT payload JSON字符串
 */
static int create_jwt_payload(const char *project_id, time_t current_time, 
                              char *payload_str, size_t payload_str_size)
{
    // Payload: {"iat":timestamp-30,"exp":timestamp+900,"sub":"project_id"}
    return snprintf(payload_str, payload_str_size,
                    "{\"iat\":%lld,\"exp\":%lld,\"sub\":\"%s\"}",
                    (long long)(current_time - 30), (long long)(current_time + 900), project_id);
}

/**
 * @brief 创建JWT header JSON字符串
 */
static int create_jwt_header(const char *key_id, char *header_str, size_t header_str_size)
{
    // Header: {"alg":"EdDSA","kid":"key_id","typ":"JWT"}
    return snprintf(header_str, header_str_size,
                    "{\"alg\":\"EdDSA\",\"kid\":\"%s\",\"typ\":\"JWT\"}",
                    key_id);
}

/**
 * @brief 生成JWT token
 */
static esp_err_t generate_jwt_token(char *token, size_t token_size, bool force_new)
{
    if (token == NULL || token_size < 512) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查是否已初始化
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "QWeather not initialized, call qweather_init() first");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 检查配置是否有效
    if (xSemaphoreTake(s_ctx.config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (!validate_config(s_ctx.project_id, s_ctx.key_id, s_ctx.api_host, s_ctx.private_key)) {
        xSemaphoreGive(s_ctx.config_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // 保存配置指针（在释放互斥锁前）
    const char *project_id = s_ctx.project_id;
    const char *key_id = s_ctx.key_id;
    const char *private_key = s_ctx.private_key;
    xSemaphoreGive(s_ctx.config_mutex);
    
    // 检查缓存
    time_t current_time = time(NULL);
    if (current_time < 0) {
        ESP_LOGE(TAG, "Failed to get current time");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 检查时间是否已同步（Unix时间戳应该大于一个合理值，比如2020年1月1日）
    // 如果时间戳太小，说明系统时间未同步
    if (current_time < 1577836800) {  // 2020-01-01 00:00:00 UTC
        ESP_LOGE(TAG, "System time not synchronized: %lld (expected > 1577836800)", 
                (long long)current_time);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Current time: %lld, iat: %lld, exp: %lld", 
            (long long)current_time, 
            (long long)(current_time - 30), 
            (long long)(current_time + 900));
    
    if (!force_new && xSemaphoreTake(s_ctx.jwt_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (s_ctx.jwt_cache.token[0] != '\0' && 
            s_ctx.jwt_cache.expires_at > current_time + 60) {
            strncpy(token, s_ctx.jwt_cache.token, token_size - 1);
            token[token_size - 1] = '\0';
            xSemaphoreGive(s_ctx.jwt_mutex);
            return ESP_OK;
        }
        xSemaphoreGive(s_ctx.jwt_mutex);
    }
    
    // 创建header和payload
    char header_json[256];
    char payload_json[256];
    
    int header_len = create_jwt_header(key_id, header_json, sizeof(header_json));
    int payload_len = create_jwt_payload(project_id, current_time, 
                                        payload_json, sizeof(payload_json));
    
    if (header_len < 0 || payload_len < 0 || 
        header_len >= (int)sizeof(header_json) || 
        payload_len >= (int)sizeof(payload_json)) {
        ESP_LOGE(TAG, "Failed to create JWT JSON");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Base64URL编码header和payload
    char header_b64[512];
    char payload_b64[512];
    size_t header_b64_len = sizeof(header_b64);
    size_t payload_b64_len = sizeof(payload_b64);
    
    if (base64url_encode((unsigned char *)header_json, header_len, 
                         header_b64, &header_b64_len) != 0 ||
        base64url_encode((unsigned char *)payload_json, payload_len, 
                         payload_b64, &payload_b64_len) != 0) {
        ESP_LOGE(TAG, "Failed to encode JWT parts");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 创建签名内容：header.payload
    char signing_input[1024];
    int signing_input_len = snprintf(signing_input, sizeof(signing_input), 
                                     "%s.%s", header_b64, payload_b64);
    if (signing_input_len < 0 || signing_input_len >= (int)sizeof(signing_input)) {
        ESP_LOGE(TAG, "Failed to create signing input");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 解析Ed25519私钥 - mbedTLS不支持Ed25519的PKCS#8解析
    // 需要手动解析PKCS#8格式并提取32字节私钥
    size_t key_len = strlen(private_key);
    ESP_LOGD(TAG, "Parsing Ed25519 private key, length: %zu", key_len);
    
    // 手动解析PEM格式：提取Base64编码的DER数据
    // PEM格式: -----BEGIN PRIVATE KEY-----\n<base64>\n-----END PRIVATE KEY-----
    const char *begin_marker = "-----BEGIN PRIVATE KEY-----";
    const char *end_marker = "-----END PRIVATE KEY-----";
    
    const char *begin_pos = strstr(private_key, begin_marker);
    const char *end_pos = strstr(private_key, end_marker);
    
    if (begin_pos == NULL || end_pos == NULL || end_pos <= begin_pos) {
        ESP_LOGE(TAG, "Invalid PEM format: missing BEGIN/END markers");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 跳过BEGIN标记和换行符
    begin_pos += strlen(begin_marker);
    while (*begin_pos == '\n' || *begin_pos == '\r') {
        begin_pos++;
    }
    
    // 计算Base64数据长度
    size_t base64_len = end_pos - begin_pos;
    while (base64_len > 0 && (begin_pos[base64_len - 1] == '\n' || 
                              begin_pos[base64_len - 1] == '\r' ||
                              begin_pos[base64_len - 1] == ' ')) {
        base64_len--;
    }
    
    // Base64解码DER数据
    unsigned char der_buffer[512];
    size_t der_len = 0;
    int mbedtls_ret = mbedtls_base64_decode(der_buffer, sizeof(der_buffer), &der_len,
                                            (const unsigned char *)begin_pos, base64_len);
    
    if (mbedtls_ret != 0) {
        char error_buf[256];
        mbedtls_strerror(mbedtls_ret, error_buf, sizeof(error_buf));
        ESP_LOGE(TAG, "Failed to decode Base64: %d (%s)", mbedtls_ret, error_buf);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Decoded DER length: %zu", der_len);
    
    // 调试：打印DER数据的前32字节
    ESP_LOGD(TAG, "DER data (first 32 bytes):");
    for (size_t i = 0; i < der_len && i < 32; i++) {
        ESP_LOGD(TAG, "  [%zu] = 0x%02x", i, der_buffer[i]);
    }
    
    // 手动解析PKCS#8格式的Ed25519私钥
    // PKCS#8结构: SEQUENCE { version INTEGER, algorithm SEQUENCE, privateKey OCTET STRING }
    // Ed25519私钥在privateKey OCTET STRING中，是32字节
    unsigned char ed25519_private_key[32];
    
    // 改进的PKCS#8解析：查找OCTET STRING标签后的32字节
    // 对于48字节的DER，结构通常是：
    // - SEQUENCE (0x30, 1字节长度) ~3字节
    // - version INTEGER (0x02, 1字节, 0x00) ~3字节
    // - algorithm SEQUENCE (0x30, 包含OID) ~20-25字节
    // - privateKey OCTET STRING (0x04, 0x20, 32字节数据) ~34字节
    bool found_key = false;
    
    // 查找OCTET STRING (0x04) 标签，Ed25519私钥通常是32字节
    for (size_t i = 0; i < der_len - 2; i++) {
        if (der_buffer[i] == 0x04) {  // OCTET STRING tag
            uint8_t len_byte = der_buffer[i + 1];
            size_t data_offset;
            size_t data_len;
            
            // 处理长度编码
            if (len_byte == 0x20) {
                // 直接长度：0x20 = 32
                data_offset = i + 2;
                data_len = 32;
            } else if (len_byte == 0x81) {
                // 长格式：0x81 后跟1字节长度
                if (i + 2 >= der_len) continue;
                data_len = der_buffer[i + 2];
                data_offset = i + 3;
            } else if (len_byte == 0x82) {
                // 长格式：0x82 后跟2字节长度
                if (i + 3 >= der_len) continue;
                data_len = (der_buffer[i + 2] << 8) | der_buffer[i + 3];
                data_offset = i + 4;
            } else {
                // 其他长度，可能是其他OCTET STRING，跳过
                continue;
            }
            
            // 检查长度是否为32（Ed25519私钥长度）
            if (data_len == 32 && data_offset + 32 <= der_len) {
                memcpy(ed25519_private_key, &der_buffer[data_offset], 32);
                found_key = true;
                ESP_LOGD(TAG, "Found Ed25519 private key at offset %zu (OCTET STRING at %zu, len=%zu)", 
                        data_offset, i, data_len);
                break;
            }
        }
    }
    
    if (!found_key) {
        ESP_LOGE(TAG, "Failed to extract Ed25519 private key from PKCS#8");
        ESP_LOGE(TAG, "DER length: %zu", der_len);
        // 打印DER数据的hex dump用于调试
        char hex_str[256];
        size_t print_len = (der_len < 48) ? der_len : 48;
        for (size_t i = 0; i < print_len; i++) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02x", der_buffer[i]);
            if (i == 0) {
                strcpy(hex_str, hex_byte);
            } else {
                strncat(hex_str, " ", sizeof(hex_str) - strlen(hex_str) - 1);
                strncat(hex_str, hex_byte, sizeof(hex_str) - strlen(hex_str) - 1);
            }
        }
        ESP_LOGE(TAG, "DER data: %s", hex_str);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 使用tweetnacl进行Ed25519签名
    // tweetnacl需要64字节密钥（32字节私钥 + 32字节公钥）
    unsigned char secret_key[64];
    if (crypto_sign_secretkey_from_private(secret_key, ed25519_private_key) != 0) {
        ESP_LOGE(TAG, "Failed to derive secret key from private key");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Extracted Ed25519 private key (first 8 bytes): %02x %02x %02x %02x %02x %02x %02x %02x",
             ed25519_private_key[0], ed25519_private_key[1], ed25519_private_key[2], ed25519_private_key[3],
             ed25519_private_key[4], ed25519_private_key[5], ed25519_private_key[6], ed25519_private_key[7]);
    
    // 使用tweetnacl进行签名（detached模式，只返回签名）
    unsigned char signature[64];
    unsigned long long signature_len;
    
    if (crypto_sign_detached(signature, &signature_len, 
                            (const unsigned char *)signing_input, signing_input_len,
                            secret_key) != 0) {
        ESP_LOGE(TAG, "Failed to sign with Ed25519");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (signature_len != 64) {
        ESP_LOGE(TAG, "Invalid signature length: %llu (expected 64)", signature_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGD(TAG, "Ed25519 signature generated successfully");
    
    // Base64URL编码签名
    char signature_b64[128];
    size_t signature_b64_len = sizeof(signature_b64);
    if (base64url_encode(signature, 64, signature_b64, &signature_b64_len) != 0) {
        ESP_LOGE(TAG, "Failed to encode signature");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 组合JWT token: header.payload.signature
    int token_len = snprintf(token, token_size, "%s.%s.%s", 
                            header_b64, payload_b64, signature_b64);
    if (token_len < 0 || token_len >= (int)token_size) {
        ESP_LOGE(TAG, "Failed to create JWT token");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 缓存token
    if (xSemaphoreTake(s_ctx.jwt_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        strncpy(s_ctx.jwt_cache.token, token, sizeof(s_ctx.jwt_cache.token) - 1);
        s_ctx.jwt_cache.token[sizeof(s_ctx.jwt_cache.token) - 1] = '\0';
        s_ctx.jwt_cache.expires_at = current_time + 900; // 15分钟有效期
        xSemaphoreGive(s_ctx.jwt_mutex);
    }
    
    ESP_LOGD(TAG, "JWT token generated successfully");
    return ESP_OK;
}

/**
 * @brief 解压gzip数据
 * 使用zlib库进行解压
 */
static esp_err_t decompress_gzip(const unsigned char *compressed_data, size_t compressed_len,
                                 unsigned char *decompressed_data, size_t *decompressed_len)
{
#if HAVE_ZLIB
    // 使用zlib解压gzip
    z_stream strm = {0};
    strm.next_in = (Bytef *)compressed_data;
    strm.avail_in = compressed_len;
    strm.next_out = decompressed_data;
    strm.avail_out = *decompressed_len;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    // 使用inflateInit2，窗口大小为16+MAX_WBITS以支持gzip格式
    // 16 + MAX_WBITS 表示自动检测gzip或zlib格式
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        ESP_LOGE(TAG, "inflateInit2 failed: %d", ret);
        return ESP_FAIL;
    }
    
    ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        ESP_LOGE(TAG, "inflate failed: %d", ret);
        inflateEnd(&strm);
        return ESP_FAIL;
    }
    
    *decompressed_len = strm.total_out;
    inflateEnd(&strm);
    return ESP_OK;
    
#else
    ESP_LOGE(TAG, "Gzip decompression requires zlib library");
    ESP_LOGE(TAG, "ESP-IDF HTTP client does not auto-decompress gzip");
    ESP_LOGE(TAG, "Please add zlib component to your project");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

/**
 * @brief HTTP事件处理回调
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            // 在连接建立后再次确保headers已设置（某些情况下需要）
            // if (s_jwt_token_buffer[0] != '\0' && evt->client != NULL) {
            //     char auth_header[576];
            //     snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_jwt_token_buffer);
            //     esp_http_client_set_header(evt->client, "Authorization", auth_header);
            //     esp_http_client_set_header(evt->client, "Accept", "application/json");
            //     ESP_LOGD(TAG, "Headers re-set in ON_CONNECTED event");
            // }
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", 
                    evt->header_key, evt->header_value);
            // 检测Content-Encoding头，判断是否为gzip压缩
            if (evt->header_key != NULL && evt->header_value != NULL) {
                if (strcasecmp(evt->header_key, "Content-Encoding") == 0) {
                    if (strcasecmp(evt->header_value, "gzip") == 0) {
                        s_http_response_ctx.is_gzip = true;
                        ESP_LOGD(TAG, "Response is gzip compressed");
                    }
                }
            }
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // 收集响应数据（ESP-IDF应该自动解压gzip，在事件中提供的是解压后的数据）
            if (s_http_response_ctx.collecting && s_http_response_ctx.buffer != NULL && evt->data != NULL) {
                size_t copy_len = evt->data_len;
                if (s_http_response_ctx.data_len + copy_len >= s_http_response_ctx.buffer_size) {
                    copy_len = s_http_response_ctx.buffer_size - s_http_response_ctx.data_len - 1;
                }
                if (copy_len > 0) {
                    memcpy(s_http_response_ctx.buffer + s_http_response_ctx.data_len, 
                          evt->data, copy_len);
                    s_http_response_ctx.data_len += copy_len;
                    s_http_response_ctx.buffer[s_http_response_ctx.data_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief 执行HTTP请求获取天气数据
 */
static esp_err_t http_get_weather(uint32_t location_code, char *response_buffer, 
                                  size_t response_buffer_size, int *http_status_code)
{
    // 检查是否已初始化
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "QWeather not initialized");
        *http_status_code = QWEATHER_ERR_CONFIG_INVALID;
        return ESP_ERR_INVALID_STATE;
    }
    
    // 获取配置
    if (xSemaphoreTake(s_ctx.config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (!validate_config(s_ctx.project_id, s_ctx.key_id, s_ctx.api_host, s_ctx.private_key)) {
        xSemaphoreGive(s_ctx.config_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // 保存配置指针（在释放互斥锁前）
    const char *api_host = s_ctx.api_host;
    xSemaphoreGive(s_ctx.config_mutex);
    
    // 生成JWT token
    char jwt_token[512];
    esp_err_t ret = generate_jwt_token(jwt_token, sizeof(jwt_token), false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate JWT token");
        return ret;
    }
    
    // 构建URL（与Python实现一致：location作为字符串参数）
    char url[256];
    snprintf(url, sizeof(url), "%s/v7/weather/now?location=%lu", 
             api_host, (unsigned long)location_code);
    
    ESP_LOGD(TAG, "Request URL: %s", url);
    ESP_LOGD(TAG, "JWT Token (first 50 chars): %.50s", jwt_token);
    
    // 保存JWT token供事件处理使用（复制到静态缓冲区）
    strncpy(s_jwt_token_buffer, jwt_token, sizeof(s_jwt_token_buffer) - 1);
    s_jwt_token_buffer[sizeof(s_jwt_token_buffer) - 1] = '\0';
    
    // 配置HTTP客户端
    esp_http_client_config_t http_config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .skip_cert_common_name_check = false,  // 验证证书CN
        .crt_bundle_attach = esp_crt_bundle_attach,  // 使用证书bundle
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        s_jwt_token_buffer[0] = '\0';
        return ESP_ERR_NO_MEM;
    }
    
    // 设置请求方法
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    
    // 设置Authorization头：Bearer <token>（在perform之前设置）
    char auth_header[576];  // "Bearer " + token (max 512)
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", jwt_token);
    esp_err_t header_ret = esp_http_client_set_header(client, "Authorization", auth_header);
    if (header_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Authorization header: %s", esp_err_to_name(header_ret));
        esp_http_client_cleanup(client);
        s_jwt_token_buffer[0] = '\0';
        return header_ret;
    }
    
    // 设置Accept头
    header_ret = esp_http_client_set_header(client, "Accept", "application/json");
    if (header_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Accept header: %s", esp_err_to_name(header_ret));
        esp_http_client_cleanup(client);
        s_jwt_token_buffer[0] = '\0';
        return header_ret;
    }
    
    ESP_LOGD(TAG, "Authorization header set: Bearer <token>");
    ESP_LOGD(TAG, "Accept header set: application/json");
    
    // 初始化响应数据收集上下文
    s_http_response_ctx.buffer = response_buffer;
    s_http_response_ctx.buffer_size = response_buffer_size;
    s_http_response_ctx.data_len = 0;
    s_http_response_ctx.collecting = true;
    s_http_response_ctx.is_gzip = false;  // 重置gzip标志
    response_buffer[0] = '\0';
    
    // 执行请求（数据会在HTTP_EVENT_ON_DATA事件中收集）
    ret = esp_http_client_perform(client);
    
    // 停止收集数据
    s_http_response_ctx.collecting = false;
    
    if (ret == ESP_OK) {
        *http_status_code = esp_http_client_get_status_code(client);
        int64_t content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld, collected = %zu",
                *http_status_code, content_length, s_http_response_ctx.data_len);
        
        // 如果响应是gzip压缩的，需要解压
        if (s_http_response_ctx.is_gzip || 
            (s_http_response_ctx.data_len >= 2 && 
             response_buffer[0] == 0x1f && response_buffer[1] == 0x8b)) {
            ESP_LOGI(TAG, "Decompressing gzip response (compressed size: %zu)", s_http_response_ctx.data_len);
            
            // 分配临时缓冲区存储压缩数据
            unsigned char *compressed_data = (unsigned char *)malloc(s_http_response_ctx.data_len);
            if (compressed_data == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for compressed data");
                ret = ESP_ERR_NO_MEM;
            } else {
                // 保存压缩数据
                memcpy(compressed_data, response_buffer, s_http_response_ctx.data_len);
                size_t compressed_len = s_http_response_ctx.data_len;
                
                // 解压数据（gzip解压后的数据通常比压缩数据大，但JSON通常不会太大）
                // 使用响应缓冲区作为解压缓冲区
                size_t decompressed_len = response_buffer_size - 1;  // 保留一个字节用于null终止
                esp_err_t decompress_ret = decompress_gzip(compressed_data, compressed_len,
                                                          (unsigned char *)response_buffer, 
                                                          &decompressed_len);
                free(compressed_data);
                
                if (decompress_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to decompress gzip data: %s", esp_err_to_name(decompress_ret));
                    ret = ESP_ERR_INVALID_RESPONSE;
                } else {
                    // 更新数据长度
                    s_http_response_ctx.data_len = decompressed_len;
                    ESP_LOGI(TAG, "Gzip decompression successful (decompressed size: %zu)", decompressed_len);
                }
            }
        }
        
        // 确保字符串以null结尾
        if (s_http_response_ctx.data_len < s_http_response_ctx.buffer_size) {
            response_buffer[s_http_response_ctx.data_len] = '\0';
        } else {
            response_buffer[s_http_response_ctx.buffer_size - 1] = '\0';
        }
        
        // 调试：打印响应体的前100个字符
        if (s_http_response_ctx.data_len > 0) {
            size_t print_len = s_http_response_ctx.data_len < 100 ? s_http_response_ctx.data_len : 100;
            ESP_LOGD(TAG, "Response body (len=%zu, first %zu chars): %.*s", 
                    s_http_response_ctx.data_len, print_len, (int)print_len, response_buffer);
        } else {
            ESP_LOGD(TAG, "Response body is empty");
        }
        
        if (*http_status_code == 200) {
            if (s_http_response_ctx.data_len == 0) {
                ESP_LOGE(TAG, "Empty response body");
                ret = ESP_ERR_INVALID_RESPONSE;
            }
        } else {
            ESP_LOGE(TAG, "HTTP error status: %d, response: %s", 
                    *http_status_code, response_buffer);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(ret));
        *http_status_code = QWEATHER_ERR_NETWORK_ERROR;
    }
    
    // 清理
    s_jwt_token_buffer[0] = '\0';
    s_http_response_ctx.buffer = NULL;
    s_http_response_ctx.data_len = 0;
    esp_http_client_cleanup(client);
    return ret;
}

/**
 * @brief 解析JSON响应并填充天气信息
 */
static esp_err_t parse_weather_json(const char *json_str, uint32_t location_code, 
                                    qweather_info_t *info)
{
    if (json_str == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 初始化info结构
    memset(info, 0, sizeof(qweather_info_t));
    info->location_code = location_code;
    info->valid = false;
    
    // 解析JSON
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "JSON parse error before: %s", error_ptr);
        }
        info->status_code = QWEATHER_ERR_JSON_PARSE;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // 检查code字段
    cJSON *code_item = cJSON_GetObjectItem(json, "code");
    if (code_item == NULL || !cJSON_IsString(code_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'code' field");
        cJSON_Delete(json);
        info->status_code = QWEATHER_ERR_JSON_PARSE;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    const char *code_str = cJSON_GetStringValue(code_item);
    int code = atoi(code_str);
    info->status_code = code;
    
    if (code != 200) {
        cJSON *message_item = cJSON_GetObjectItem(json, "message");
        if (message_item != NULL && cJSON_IsString(message_item)) {
            ESP_LOGE(TAG, "API error: %s", cJSON_GetStringValue(message_item));
        }
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 解析now对象
    cJSON *now_item = cJSON_GetObjectItem(json, "now");
    if (now_item == NULL || !cJSON_IsObject(now_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'now' field");
        cJSON_Delete(json);
        info->status_code = QWEATHER_ERR_JSON_PARSE;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // 解析温度
    cJSON *temp_item = cJSON_GetObjectItem(now_item, "temp");
    if (temp_item != NULL && cJSON_IsString(temp_item)) {
        info->temperature = atof(cJSON_GetStringValue(temp_item));
    }
    
    // 解析湿度
    cJSON *humidity_item = cJSON_GetObjectItem(now_item, "humidity");
    if (humidity_item != NULL && cJSON_IsString(humidity_item)) {
        info->humidity = atof(cJSON_GetStringValue(humidity_item));
    }
    
    // 解析天气文本
    cJSON *text_item = cJSON_GetObjectItem(now_item, "text");
    if (text_item != NULL && cJSON_IsString(text_item)) {
        const char *text = cJSON_GetStringValue(text_item);
        strncpy(info->weather_text, text, sizeof(info->weather_text) - 1);
        info->weather_text[sizeof(info->weather_text) - 1] = '\0';
    }
    
    // 解析图标ID
    cJSON *icon_item = cJSON_GetObjectItem(now_item, "icon");
    if (icon_item != NULL && cJSON_IsString(icon_item)) {
        info->weather_icon = (uint16_t)atoi(cJSON_GetStringValue(icon_item));
    }
    
    // 解析更新时间
    cJSON *update_time_item = cJSON_GetObjectItem(json, "updateTime");
    if (update_time_item != NULL && cJSON_IsString(update_time_item)) {
        // 解析ISO 8601格式时间: "2025-11-23T17:02+08:00"
        const char *time_str = cJSON_GetStringValue(update_time_item);
        struct tm tm = {0};
        if (strptime(time_str, "%Y-%m-%dT%H:%M", &tm) != NULL) {
            info->update_time = mktime(&tm);
        }
    } else {
        // 如果没有updateTime，使用当前时间
        info->update_time = time(NULL);
    }
    
    info->valid = true;
    cJSON_Delete(json);
    return ESP_OK;
}

/**
 * @brief 执行天气查询（内部函数）
 */
static esp_err_t query_weather_internal(uint32_t location_code, qweather_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 初始化info
    memset(info, 0, sizeof(qweather_info_t));
    info->location_code = location_code;
    info->valid = false;
    
    // 检查是否已初始化
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "QWeather not initialized, call qweather_init() first");
        info->status_code = QWEATHER_ERR_CONFIG_INVALID;
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_ctx.config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        info->status_code = QWEATHER_ERR_NETWORK_ERROR;
        return ESP_ERR_TIMEOUT;
    }
    
    bool config_valid = validate_config(s_ctx.project_id, s_ctx.key_id, s_ctx.api_host, s_ctx.private_key);
    xSemaphoreGive(s_ctx.config_mutex);
    
    if (!config_valid) {
        ESP_LOGE(TAG, "Invalid configuration");
        info->status_code = QWEATHER_ERR_CONFIG_INVALID;
        return ESP_ERR_INVALID_STATE;
    }
    
    // 执行HTTP请求
    char response_buffer[2048];
    int http_status_code = 0;
    esp_err_t ret = http_get_weather(location_code, response_buffer, sizeof(response_buffer), 
                          &http_status_code);
    
    if (ret != ESP_OK) {
        info->status_code = http_status_code;
        return ret;
    }
    
    // 解析JSON响应
    ret = parse_weather_json(response_buffer, location_code, info);
    if (ret != ESP_OK && info->status_code == 0) {
        info->status_code = http_status_code;
    }
    
    return ret;
}

/**
 * @brief 异步查询任务
 */
static void query_task(void *pvParameters)
{
    uint32_t location_code = (uint32_t)(uintptr_t)pvParameters;
    qweather_info_t info;
    
    ESP_LOGI(TAG, "Async query started for location: %lu", (unsigned long)location_code);
    
    esp_err_t ret = query_weather_internal(location_code, &info);
    
    if (ret == ESP_OK && info.valid) {
        ESP_LOGI(TAG, "Query successful: temp=%.1f°C, humidity=%.1f%%, text=%s",
                info.temperature, info.humidity, info.weather_text);
    } else {
        ESP_LOGE(TAG, "Query failed: %s, status_code=%d", 
                esp_err_to_name(ret), info.status_code);
    }
    
    // 发送事件
    qweather_event_data_t event_data;
    memcpy(&event_data.info, &info, sizeof(qweather_info_t));
    
    app_event_post(QWEATHER_EVENTS, QWEATHER_EVENT_UPDATE, 
                   &event_data, sizeof(event_data), pdMS_TO_TICKS(100));
    
    // 清除查询任务运行标志
    if (xSemaphoreTake(s_ctx.query_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_ctx.query_task_running = false;
        xSemaphoreGive(s_ctx.query_mutex);
    }
    
    vTaskDelete(NULL);
}

esp_err_t qweather_query_async(uint32_t location_code)
{
    // 检查是否已初始化
    if (!s_ctx.initialized) {
        ESP_LOGE(TAG, "QWeather not initialized, call qweather_init() first");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 检查配置
    if (xSemaphoreTake(s_ctx.config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    bool config_valid = validate_config(s_ctx.project_id, s_ctx.key_id, s_ctx.api_host, s_ctx.private_key);
    xSemaphoreGive(s_ctx.config_mutex);
    
    if (!config_valid) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 检查是否已有查询任务正在运行
    if (xSemaphoreTake(s_ctx.query_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (s_ctx.query_task_running) {
        xSemaphoreGive(s_ctx.query_mutex);
        ESP_LOGW(TAG, "Query task already running, ignoring new request");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 标记查询任务正在运行
    s_ctx.query_task_running = true;
    xSemaphoreGive(s_ctx.query_mutex);
    
    // 创建任务执行异步查询
    // 栈大小需要足够大以容纳所有函数调用链中的大数组
    // response_buffer[2048] + JWT相关数组 + HTTP相关数组 + 函数调用栈
    BaseType_t task_ret = xTaskCreate(query_task, "qweather_query", 16384,
                                      (void *)(uintptr_t)location_code,
                                      5, NULL);
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create query task");
        // 清除标志
        if (xSemaphoreTake(s_ctx.query_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            s_ctx.query_task_running = false;
            xSemaphoreGive(s_ctx.query_mutex);
        }
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t qweather_query(uint32_t location_code, qweather_info_t *info)
{
    return query_weather_internal(location_code, info);
}

