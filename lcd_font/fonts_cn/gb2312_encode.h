#ifndef __GB2312_ENCODE_H__
#define __GB2312_ENCODE_H__

/**
 * @brief GB2312编码函数声明
 * 
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_types.h"

#define INVALID_GB2312 0xFFFF

// Convert Unicode code point to GB2312 encoding
// Returns INVALID_GB2312 if the character is not found
uint16_t unicode_to_gb2312(uint16_t unicode);

#ifdef __cplusplus
}
#endif

#endif // __GB2312_ENCODE_H__
