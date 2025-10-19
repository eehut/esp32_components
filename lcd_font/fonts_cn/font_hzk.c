/**
 * @file font_hzk.c
 * @author Liu Chuansen (179712066@qq.com)
 * @brief 从字库分区中加载汉字数据
 * @version 0.1
 * @date 2025-10-19
 * 
 * @copyright Copyright (c) 2025
 * @note 实现逻辑，每个汉字库分区对应一种字体，所以需要根据字体大小和编码大小来计算数据偏移量
 * 汉字库的名称是 hzk16， 对应数据就存在hzk16的分区中，这个分区的header格式为hzk_header_t。
 * 所以需要先读取hzk_header_t，然后根据font_data_offset和font_data_size来读取数据。
 */

#include "lcd_font_type.h"
#include "hzk_header.h"
#include "gb2312_encode.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "font_hzk";

// 加载GB2312编码库
#if defined(CONFIG_GB2312_ENCODE_HZ3500)
#include "gb2312_encode_hz3500.c"
#else
#include "gb2312_encode_hz2000.c"
#endif // CONFIG_GB2312_ENCODE_HZ3500


// 创建一个字体管理对象，在初始时从HZK分区中读取HEADER信息，并校验数据的有效性
// 提供一个数据缓存，字库获取数据的返回值，并引入RTOS的锁，保证数据访问的线程安全
typedef struct {
    const char *name;
    bool valid;
    uint8_t *data_cache;
    SemaphoreHandle_t mutex;
    const esp_partition_t *partition;
    uint32_t data_offset;
    uint32_t font_data_size;
    uint32_t font_code_size;
}hzk_font_manager_t;

// 声明函数
static bool load_and_check(const lcd_font_t *font, hzk_font_manager_t *manager);
const uint8_t *lcd_font_get_hzk_code(const void *self, uint32_t ch);

#if CONFIG_LCD_FONT_HZK_16

static hzk_font_manager_t s_hzk16_manager = { 0 };
LCD_FONT_DEFINE_NO_DATA(hzk16, 16, 16, lcd_font_get_hzk_code);
#endif // CONFIG_LCD_FONT_HZK_16

#if CONFIG_LCD_FONT_HZK_24

static hzk_font_manager_t s_hzk24_manager = { 0 };
LCD_FONT_DEFINE_NO_DATA(hzk24, 24, 24, lcd_font_get_hzk_code);
#endif // CONFIG_LCD_FONT_HZK_24

/**
 * @brief 计算CRC32校验和
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return uint32_t CRC32值
 */
static uint32_t calculate_crc32(const uint8_t *data, size_t len)
{
    return esp_crc32_le(0, data, len);
}

/**
 * @brief 从分区加载并校验汉字库
 * 
 * @param font 字体对象
 * @param manager 字体管理对象
 * @return bool 是否成功
 */
static bool load_and_check(const lcd_font_t *font, hzk_font_manager_t *manager)
{
    if (font == NULL || manager == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    ESP_LOGI(TAG, "Loading HZK font: %s", font->name);
    
    // 查找分区
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, font->name);
    
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found", font->name);
        return false;
    }
    
    // 读取头部信息
    hzk_header_t header;
    esp_err_t ret = esp_partition_read(partition, 0, &header, sizeof(hzk_header_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read header from partition '%s': %s", font->name, esp_err_to_name(ret));
        return false;
    }
    
    // 验证魔数
    if (header.magic != 0x314B5A48) { // "HZK1"
        ESP_LOGE(TAG, "Invalid magic number in partition '%s': 0x%08X", font->name, header.magic);
        return false;
    }
    
    // 验证头部校验和
    uint32_t calculated_header_crc = calculate_crc32((uint8_t*)&header, 
        sizeof(hzk_header_t) - sizeof(header.header_checksum));
    
    if (calculated_header_crc != header.header_checksum) {
        ESP_LOGE(TAG, "Header checksum mismatch in partition '%s': stored=0x%08X, calculated=0x%08X", 
                 font->name, header.header_checksum, calculated_header_crc);
        return false;
    }
    
    // 验证字体数据大小
    if (header.font_data_offset + header.font_data_size > partition->size) {
        ESP_LOGE(TAG, "Font data size exceeds partition size in '%s'", font->name);
        return false;
    }
    
    // 验证字符数量是否合理
    uint32_t expected_code_size = ((header.font_width * header.font_height + 7) / 8);
    if (header.font_code_size != expected_code_size) {
        ESP_LOGE(TAG, "Code size mismatch in partition '%s': stored=%u, expected=%u", 
                 font->name, header.font_code_size, expected_code_size);
        return false;
    }
    
    if (header.font_data_size % header.font_code_size != 0) {
        ESP_LOGW(TAG, "Font data size not aligned to code size in partition '%s'", font->name);
    }
    
    // 分配数据缓存
    manager->data_cache = malloc(font->code_size);
    if (manager->data_cache == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for data cache in '%s'", font->name);
        return false;
    }
    
    // 创建互斥锁
    manager->mutex = xSemaphoreCreateMutex();
    if (manager->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for '%s'", font->name);
        free(manager->data_cache);
        manager->data_cache = NULL;
        return false;
    }
    
    // 初始化manager对象
    manager->name = font->name;
    manager->partition = partition;
    manager->data_offset = header.font_data_offset;
    manager->font_data_size = header.font_data_size;
    manager->font_code_size = header.font_code_size;
    manager->valid = true;
    
    uint32_t char_count = header.font_data_size / header.font_code_size;
    ESP_LOGI(TAG, "HZK font '%s' loaded successfully: %dx%d, %u characters, %u bytes", 
             font->name, header.font_width, header.font_height, char_count, header.font_data_size);
    
    return true;
}

/**
 * @brief 根据字体对象获取对应的manager
 * 
 * @param font 字体对象
 * @return hzk_font_manager_t* 对应的manager，NULL表示不支持
 */
static hzk_font_manager_t *get_manager_by_font(const lcd_font_t *font)
{
    if (font == NULL) {
        return NULL;
    }
    
#if CONFIG_LCD_FONT_HZK_16
    if (font->width == 16 && font->height == 16) {
        return &s_hzk16_manager;
    }
#endif

#if CONFIG_LCD_FONT_HZK_24
    if (font->width == 24 && font->height == 24) {
        return &s_hzk24_manager;
    }
#endif

    return NULL;
}

/**
 * @brief 获取HZK字体编码数据
 * 
 * @param self 字体对象
 * @param ch 字符编码，Unicode编码
 * @return const uint8_t* 返回编码数据地址，使用f->code_size长度的数据即可
 * @note self* 是 lcd_font_t 类型， 所以需要强制转换为 lcd_font_t 类型
 * @note 使用manager的data_cache来存储读取的数据，加锁保护
 */
const uint8_t *lcd_font_get_hzk_code(const void *self, uint32_t ch)
{
    const lcd_font_t *f = (const lcd_font_t *)self;
    
    // 检查字体对象是否有效
    if (f == NULL) {
        return NULL;
    }
    
    // 获取对应的manager
    hzk_font_manager_t *manager = get_manager_by_font(f);
    if (manager == NULL) {
        ESP_LOGW(TAG, "Unsupported font: %s", f->name);
        return NULL;
    }
    
    // 如果manager未初始化，则进行初始化
    if (!manager->valid) {
        if (!load_and_check(f, manager)) {
            ESP_LOGE(TAG, "Failed to load font: %s", f->name);
            return NULL;
        }
    }
    
    // 将Unicode转换为GB2312编码
    uint16_t gb2312_code = unicode_to_gb2312((uint16_t)ch);
    if (gb2312_code == INVALID_GB2312) {
        ESP_LOGW(TAG, "Invalid GB2312 code for character 0x%04X", ch);
        return NULL;
    }
    
    // 计算字符在字体数据中的索引
    // GB2312编码范围：0xA1A1-0xFEFE
    // 需要转换为线性索引
    uint8_t high_byte = (gb2312_code >> 8) & 0xFF;
    uint8_t low_byte = gb2312_code & 0xFF;
    
    // 检查GB2312编码范围
    if (high_byte < 0xA1 || high_byte > 0xFE || low_byte < 0xA1 || low_byte > 0xFE) {
        ESP_LOGW(TAG, "GB2312 code out of range: 0x%04X", gb2312_code);
        return NULL;
    }
    
    // 计算字符索引：GB2312编码转换为线性索引
    uint32_t char_index = ((high_byte - 0xA1) * 94) + (low_byte - 0xA1);
    
    // 检查字符索引是否在字体数据范围内
    uint32_t max_chars = manager->font_data_size / f->code_size;
    if (char_index >= max_chars) {
        ESP_LOGW(TAG, "Character index out of range: %u >= %u", char_index, max_chars);
        return NULL;
    }
    
    // 加锁保护data_cache
    if (xSemaphoreTake(manager->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex for font: %s", f->name);
        return NULL;
    }
    
    // 计算字符在分区中的偏移量
    uint32_t char_offset = manager->data_offset + (char_index * f->code_size);
    
    // 检查是否超出分区范围
    if (char_offset + f->code_size > manager->partition->size) {
        ESP_LOGE(TAG, "Character offset out of partition range");
        xSemaphoreGive(manager->mutex);
        return NULL;
    }
    
    // 从分区读取字符数据到data_cache
    esp_err_t ret = esp_partition_read(manager->partition, char_offset, manager->data_cache, f->code_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read character data for GB2312: 0x%04X, error: %s", 
                 gb2312_code, esp_err_to_name(ret));
        xSemaphoreGive(manager->mutex);
        return NULL;
    }
    
    // 释放锁
    xSemaphoreGive(manager->mutex);
    
    return manager->data_cache;
}



void lcd_font_hzk_init(void)
{
    #if CONFIG_LCD_FONT_HZK_16
    load_and_check(LCD_FONT(hzk16), &s_hzk16_manager);
    #endif

    #if CONFIG_LCD_FONT_HZK_24
    load_and_check(LCD_FONT(hzk24), &s_hzk24_manager);
    #endif
}
