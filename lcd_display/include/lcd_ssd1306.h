#ifndef __LCD_MODEL_SSD1306_H__
#define __LCD_MODEL_SSD1306_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_model_type.h"


/*
    FOR SSD1306
    0xae, #display off
    0x20, #Set Memory Addressing Mode    
    0x10, #00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
    0xb0, #Set Page Start Address for Page Addressing Mode,0-7
    0xc8, #Set COM Output Scan Direction
    0x00, #---set low column address
    0x10, #---set high column address
    0x40, #--set start line address
    0x81, #--set contrast control register
    0xff, #äº®åº¦è°ƒèŠ‚ 0x00~0xff
    0xa1, #--set segment re-map 0 to 127
    0xa6, #--set normal display
    0xa8, #--set multiplex ratio(1 to 64)
    0x3f, #
    0xa4, #0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    0xd3, #-set display offset
    0x00, #-not offset
    0xd5, #--set display clock divide ratio/oscillator frequency
    0xf0, #--set divide ratio
    0xd9, #--set pre-charge period
    0x22, #
    0xda, #--set com pins hardware configuration
    0x12,
    0xdb, #--set vcomh
    0x20, #0x20,0.77xVcc
    0x8d, #--set DC-DC enable
    0x14, #
    0xaf #--turn on oled panel
*/
#define SSD1306_INIT_DATAS { \
0xae, 0x20, 0x10, 0xb0, 0xc8, 0x00, 0x10, 0x40, \
0x81, 0xff, 0xa1, 0xa6, 0xa8, 0x3f, 0xa4, 0xd3, \
0x00, 0xd5, 0xf0, 0xd9, 0x22, 0xda, 0x12, 0xdb, \
0x20, 0x8d, 0x14, 0xaf \
}

/// 预定义的MODEL
#define LCD_DEFINE_SSD1306_128X64(_name) \
LCD_MODEL_DEFINE(_name, 128, 64, SSD1306_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, lcd_set_page_address_ssd1306_compatible)


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_SSD1306_H__

