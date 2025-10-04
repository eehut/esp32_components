#ifndef __LCD_MODELS_H__
#define __LCD_MODELS_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_model_type.h"

static inline void _set_page_address_ssd1306(const void *disp, const uint16_t page, uint16_t offset)
{
    uint8_t cmd[3] = {0xb0 + page, offset & 0x0f, 0x10 + (offset >> 4)};
    lcd_write_commands(disp, cmd, 3);
}

static inline void _set_page_address_sh1108(const void *disp, const uint16_t page, uint16_t offset)
{
    uint8_t cmd[4] = {0xb0,  page, offset & 0x0f, 0x11 + (offset >> 4)};
    lcd_write_commands(disp, cmd, 4);
}

static inline void _set_page_address_sh1122(const void *disp, const uint16_t page, uint16_t offset)
{
    uint8_t cmd[4] = {0xb0 , page, offset & 0x0f, 0x10 + (offset >> 4)};
    lcd_write_commands(disp, cmd, 4);
}

static inline void _custom_refresh_for_sh1122(const void *disp, const lcd_model_t *model)
{
    int x_num = (model->xsize + 7) / 8;
    int y_num = model->ysize;

    // 从MCU搬动显示数据到LCD的内部DRAM中，这个方式是固定的，跟具体怎么旋转无关。
    // 显示方向旋转只是改变读取数据的方式，而不是改变搬动数据的方式。
    model->set_page_address(disp, 0, 0);

    uint8_t buffer[128]; // 256 /2

    for (int y = 0; y < y_num; y ++)
    {
        // 设置页地址
        //model->set_page_address(disp, y, 0);
        // 读取数据, SH1122是带灰度的屏，每个字节对应两个位，需要进行转换
        uint16_t index = 0;
        for (int x = 0; x < x_num; x ++)
        {
            uint8_t data = lcd_get_dram_data(disp, x, y);
            for (int i = 0; i < 4; i ++)
            {
                buffer[index] = (data & 0x01) ? 0xf0 : 0x00;
                data >>= 1;
                buffer[index] |= (data & 0x01) ? 0x0f : 0x00;
                data >>= 1;
                index++;
            }
        }
        lcd_write_datas(disp, buffer, sizeof(buffer));
    }
    
}

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
LCD_MODEL_DEFINE(LCD_SSD1306_128X64, _name, 128, 64, SSD1306_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, _set_page_address_ssd1306)


/*
    FOR SSD1312
    0xAE,           #display off
    0x20, 0x10,     #Set Memory Addressing Mode #00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid        
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
#define SSD1312_INIT_DATAS { \
0xAE, 0x20, 0x10, 0xC0, 0x40, 0x81, 0x7F, 0xA1, \
0xA6, 0xA8, 0x3F, 0xA4, 0xD3, 0x00, 0xD5, 0xF0, \
0xD9, 0x22, 0xDA, 0x10, 0xDB, 0x20, 0x8D, 0x12, \
0xAF \
}

/// 预定义的MODEL
#define LCD_DEFINE_SSD1312_128X64(_name) \
LCD_MODEL_DEFINE(_name, 128, 64, SSD1312_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, _set_page_address_ssd1306)

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

#define SH1108_INIT_DATAS { \
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
LCD_MODEL_DEFINE(LCD_SH1108_160X128, _name, 128, 160, SH1108_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, _set_page_address_sh1108)


/*
    FOR SH1122 (256x64 OLED, 16-bit grayscale)
    初始化步骤参考数据手册 Power-On Sequence 与 Command Set

    注意：
    - 某些命令是双字节的（需要紧跟参数字节）
    - 初始化时必须先关闭显示，设置好所有参数后再开启显示

    关键说明
✅ 灰度表 (0xB8 命令)
共 9 个字节，定义了 16 级灰阶对应的电流比例。
这里给的是线性灰度（1/15、2/15、…、15/15），如果需要 Gamma 校正可调整。
✅ 相位长度 (0xB0) 和帧频 (0xB1)
相位长度和帧频影响显示稳定性和刷新率，可按手册建议值设置。
✅ 电流和电压设置 (0xC1, 0xC7, 0xBB, 0xBE)
这些影响亮度、功耗和对比度，通常先用典型值，如果显示太暗可逐步调高。
*/

#define SH1122_INIT_DATAS { \
    0xAE,             /* ✅ Display OFF (关闭显示) 进入休眠模式 */ \
    0x81, 0x80,       /* ✅ Set Contrast Control: 0x00~0xFF 调节段驱动电流，默认0x80 */ \
    0xA0,             /* ✅ Set Segment Re-map: A0=正常方向, A1=左右镜像 */ \
    0xA4,             /* ✅ Entire Display ON: A4=显示RAM内容, A5=点亮所有像素 */ \
    0xA6,             /* ✅ Set Normal/Inverse Display: A6=正常显示, A7=反相显示 */ \
    0xAD, 0x80,       /* ✅ Set DC-DC Control: bit0=DCDC开关(1=开), bit[3:1]选择开关频率 */ \
    0xB0, 0x00,       /* ✅ Set Row Address of Display RAM: 0-63 POR:0 */ \
    0xD5, 0x50,       /* ✅ Set Display Clock Divider / Oscillator Frequency (F[3:0]=分频, F[7:4]=频率因子)  默认50h */ \
    0xD9, 0x22,       /* ✅ Set Discharge/Precharge Period: [3:0] pre-charge, [7:4] discharge (默认22h) */ \
    0xDB, 0x35,       /* ✅ Set VCOMH Deselect Level: 设置COM端去选电平 (默认35h)  */ \
    0xDC, 0x35,       /* ✅ Set VSEGM Level: 设置段预充电电压 (默认35h) */ \
    0x30,             /* ✅ Set Segment Output Discharge Level (VSL): 30H-3FH, 默认30H=0V */ \
    0xAF              /* ✅ Display ON (开启显示) */ \
}

#define LCD_DEFINE_SH1122_256X64(_name) \
LCD_MODEL_DEFINE_WITH_CUSTOM_REFRESH(_name, 256, 64, SH1122_INIT_DATAS, LCD_DRAM_MODE_DEFAULT, _set_page_address_sh1122, _custom_refresh_for_sh1122)


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODELS_H__
