#ifndef __LCD_MODEL_SH1122_H__
#define __LCD_MODEL_SH1122_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_model_type.h"


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

#define SH1122_256X64_INIT_DATAS { \
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
LCD_MODEL_DEFINE_WITH_CUSTOM_REFRESH(_name, 256, 64, SH1122_256X64_INIT_DATAS, LCD_DRAM_MODE_DEFAULT, lcd_set_page_address_sh1108_compatible, _custom_refresh_for_sh1122)


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_SH1122_H__

