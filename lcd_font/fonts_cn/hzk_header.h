#ifndef __HZK_HEADER_H__
#define __HZK_HEADER_H__

/**
 * @file hzk_header.h
 * @author Liu Chuansen (179712066@qq.com)
 * @brief 汉字库头信息
 * @version 0.1
 * @date 2025-10-18
 * 
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_types.h"

/**
 * @brief 汉字库头信息, 共64字节对齐
 * 
 */
typedef struct 
{
    uint32_t magic;// HZK1, 
    uint32_t font_width;
    uint32_t font_height;
    uint32_t font_code_size;
    uint32_t font_data_offset;
    uint32_t font_data_size;
    uint32_t font_data_checksum;// CRC32
    uint32_t reserved[8];
    uint32_t header_checksum; // CRC32
}hzk_header_t;


#ifdef __cplusplus
}
#endif

#endif // __HZK_HEADER_H__
