#ifndef __LCD_MODEL_SH1108_H__
#define __LCD_MODEL_SH1108_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_model_type.h"


/*
128，实际操作范围16-143， 偏移地址为16
*/
static inline void _set_page_address_sh1108_160x128(const void *disp, const uint16_t page, uint16_t offset)
{
    lcd_set_page_address_sh1108_compatible(disp, page, offset + 16);
}

/*
    FOR SH1108 (160x160 OLED)
    初始化步骤参考数据手册 Power-On Sequence 与 Command Set

    Display Resolution Option：
- 64 COM× 160 SEG；
- 96 COM× 160 SEG；
- 128 COM× 160 SEG；[*]
- 160 COM× 160 SEG；


    显存示意图@PAGE MODE 128X160
     128 COL0｜｜｜｜｜｜｜｜｜｜COL127
    | P0[||||||||||||||||||||||||||||||] 
160 | P1[||||||||||||||||||||||||||||||] 
    | P2[||||||||||||||||||||||||||||||] 
    | P3[||||||||||||||||||||||||||||||] 
    ...
    | P19[||||||||||||||||||||||||||||||] 

    --> 128
    D0
    D1
 P  D2
 A  D3
 G  D4
 E  D5
    D6
    D7

    - SET PAGE ADDRESS: 双字节命令： B0， 0 - 19
    - 设置列地址, 两个单字节指令组成 
    CMD0: 00H-0FH, 
    CMD1: 10H-19H 
    取CMD0的低4位->A3A2A1A0， 取CMD1的低4位->A7A6A5A4， 拼接成一个列地址：0-160 （如果列是128，实际操作范围16-143）  

*/

#define SH1108_160X128_INIT_DATAS { \
    0xAE,       /* Display OFF (进入初始化时必须关闭显示) */ \
    0x81, 0xD0, /* Set Contrast Control Register: 0x00~0xFF (默认80H) 调节亮度 */ \
    0xA4,       /* Entire Display ON/OFF: A4=显示RAM内容, A5=点亮所有像素 */ \
    0xA6,       /* Normal/Reverse Display: A6=正常显示, A7=反相显示 */ \
    0xA9, 0x02, /* Display Resolution Control: 00=64COM, 01=96COM, 02=128COM, 03=160COM */ \
    0xAD, 0x80, /* DC-DC Control: bit0=DCDC开关(1=开), bit[3:1]=开关频率 */ \
    0xC0,       /* Common Output Scan Direction: C0=COM0开始, C8=COM159开始(上下翻转) */ \
    0xA0,       /* Segment Re-map: A0=正常, A1=左右翻转SEG顺序 */ \
    0xD5, 0x40, /* Set Display Clock Divide Ratio / Oscillator Frequency: 默认0x40 */ \
    0xD9, 0x2F, /* Set Pre-charge Period: [3:0]预充电周期, [7:4]放电周期 */ \
    0xDB, 0x3F, /* Set VCOMH Deselect Level: 默认0x35~0x3F, 调节灰阶 */ \
    0x20,       /* Set Memory Addressing Mode: 20=Page addressing, 21=Vertical addressing */ \
    0xDC, 0x35, /* Set VSEGM Deselect Level: 调整SEG pad电平 */ \
    0x30,       /* Set Segment Output Discharge Level (VSL): 30H-3FH, 默认30H=0V */ \
    0xAF        /* Display ON (初始化完成后开启显示) */ \
}


#define LCD_DEFINE_SH1108_160X128(_name) \
LCD_MODEL_DEFINE(_name, 128, 160, SH1108_160X128_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, _set_page_address_sh1108_160x128)


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_SH1108_H__

