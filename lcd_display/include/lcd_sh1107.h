#ifndef __LCD_MODEL_SH1107_H__
#define __LCD_MODEL_SH1107_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_model_type.h"


static inline void _set_page_address_sh1107_64x128(const void *disp, const uint16_t page, uint16_t offset)
{
    lcd_set_page_address_ssd1306_compatible(disp, page, offset + 32);
}


/*
    0xAE,//关闭显示
    0xD5,//设置时钟分频因子,震荡频率
    0x50,  //[3:0],分频因子;[7:4],震荡频率 默认0x50
 
    0xA8,//设置驱动路数
    0X7f,//默认(1/64)
 
    0xD3,//设置显示偏移
    0X00,//默认为0
 
    0x40,//设置显示开始行 [5:0],行数.
 
    0x8D,//电荷泵设置
    0x14,//bit2，开启/关闭
    0x20,//设置内存地址模式
    0x02,//[1:0],00，列地址模式;01，行地址模式;10,页地址模式;默认10;
    0xA0,//段重定义设置,bit0:0,0->0;1,0->127;  A1
 
    0xC0,//设置COM扫描方向;bit3:0,普通模式;1,重定义模式 COM[N-1]->COM0;N:驱动路数 C0 翻转显示 C8
 
    0xDA,//设置COM硬件引脚配置
    0x12,//[5:4]配置
 
    0x81,//对比度设置
    0x7f,//1~255;默认0X7F (亮度设置,越大越亮)
 
    0xD9,//设置预充电周期
    0x22,//[3:0],PHASE 1;[7:4],PHASE 2;
 
    0xDB,//设置VCOMH 电压倍率
    0x37,//[6:4] 000,0.65*vcc;001,0.77*vcc;011,0.83*vcc;
 
    0xA4,//全局显示开启;bit0:1,开启;0,关闭;(白屏/黑屏)
 
    0xA6,//设置显示方式;bit0:1,反相显示;0,正常显示
 
    0xAF,//开启显示 
*/
#define SH1107_64X128_INIT_DATAS { \
    0xAE, 0xD5, 0x50, 0xA8, 0x7F, 0xD3, 0x00, 0x40, \
    0x8D, 0x14, 0x20, 0x02, 0xA0, 0xC0, 0x12, 0x81, \
    0x7F, 0xD9, 0x22, 0xDB, 0x37, 0xA4, 0xA6, 0xAF \
}

/// 预定义的MODEL
#define LCD_DEFINE_SH1107_64X128(_name) \
LCD_MODEL_DEFINE(_name, 64, 128, SH1107_64X128_INIT_DATAS, LCD_DRAM_MODE_VERTICAL, _set_page_address_sh1107_64x128)


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_SH1107_H__
