#ifndef __LCD_MODEL_SSD1312_H__
#define __LCD_MODEL_SSD1312_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_model_type.h"

// Note: _set_page_address_ssd1306 is defined in lcd_ssd1306.h
// It should be included before this file (e.g., via lcd_models.h)

/*
    FOR SSD1312
    0xAE,           #display off
    0x20, 0x09,     #Set Memory Addressing Mode 【A3 * A1 A0】
                    # 0b 01b - COM-Page H-Mode,
                    # 0b 10b - Page Addressing Mode(RESET),
                    # 1b 01b - SEG-Page H-Mode        
    0xC0,           #Set COM Output Scan Direction, C0H - normal mode, C8H - inverted mode
    0x40,           #--set start line address
    0x81, 0x7F,     #--set contrast control register, 0x00~0xff
    0xA1,           #--set segment re-map 0 to 127, A0H - normal mode, A1H - remap 0 to 127
    0xA6,           #--set normal display, A6H - normal display, A7H - inverted display
    0xA8, 0x3F,     #--set multiplex ratio(1 to 64)
    0xA4,           #0xA4,Output follows RAM content;0xA5,Output ignores RAM content
    0xD3, 0x00,     #-set display offset, 0x00 - no offset
    0xD5, 0xF0,     #--set display clock divide ratio/oscillator frequency, 
    0xD9, 0x22,     #--set pre-charge period, 
    0xDA, 0x10,     #--set com pins hardware configuration
    0xDB, 0x20,     #--set vcomh, 0x20,0.77xVcc
    0x8D, 0x12,     #--set DC-DC enable
    0xAF,           #--turn on oled panel


    显存示意图@PAGE MODE
    128 COL0｜｜｜｜｜｜｜｜｜｜COL127
   | P0[||||||||||||||||||||||||||||||] 
64 | P1[||||||||||||||||||||||||||||||] 
   | P2[||||||||||||||||||||||||||||||] 
   | P3[||||||||||||||||||||||||||||||] 
   | P4[||||||||||||||||||||||||||||||] 
   | P5[||||||||||||||||||||||||||||||] 
   | P6[||||||||||||||||||||||||||||||] 
   | P7[||||||||||||||||||||||||||||||] 
    
    每页128字节，共8页，共1024字节
    每页的字节0，8个位，对应像素bit0-行0列0， bit1-行1列0， bit2-行2列0， bit3-行3列0， bit4-行4列0， bit5-行5列0， bit6-行6列0， bit7-行7列0
    每页的字节1，8个位，对应像素bit0-行0列1， bit1-行1列1， bit2-行2列1， bit3-行3列1， bit4-行4列1， bit5-行5列1， bit6-行6列1， bit7-行7列1
    ...

    显存刷新方式：
    设置PAGE地址（0xB0+page），设置列地址0，然后写128个数据
    再写一下PAGE地址，再写一下列地址0，然后写128个数据
    ...

     - PAGE地址:    B0-B7H: 
     - 设置列地址： 00H-0FH, 10H-17H 
    取CMD0的低4位， 取CMD1的低3位， 拼接成一个列地址：0-127    
*/
#define SSD1312_128X64_INIT_DATAS { \
0xAE, 0x00, 0x10, 0x20, 0x02, 0xC0, 0x40, 0x81, 0x7F, 0xA1, \
0xA6, 0xA8, 0x3F, 0xA4, 0xD3, 0x00, 0xD5, 0xF0, \
0xD9, 0x22, 0xDA, 0x10, 0xDB, 0x20, 0x8D, 0x12, \
0xAF \
}

/// 预定义的MODEL
#define LCD_DEFINE_SSD1312_128X64(_name) \
LCD_MODEL_DEFINE(_name, 128, 64, SSD1312_128X64_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, lcd_set_page_address_ssd1306_compatible)


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_SSD1312_H__

