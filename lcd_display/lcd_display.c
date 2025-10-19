/**
 * @file lcd_display.c
 * @author LiuChuansen (1797120666@qq.com)
 * @brief 单色LCD显示驱动实现
 * @version 0.1
 * @date 2025-05-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "lcd_display.h"
#include "lcd_driver.h"
#include "esp_log.h"
#include "lcd_fonts.h"
#include "lcd_img.h"
#include "lcd_model_type.h"
#include "uptime.h"
#include <stdint.h>
#include <stdbool.h>

static const char *TAG = "lcd-mono";

/*
单色LCD显示基本实现思路
 
上层API -> 写到MCU的显存里 -> 刷新驱动（从MCU显存搬到LCD的内部DRAM中）

MCU显存设定：
水平方向为X轴，垂直方向为Y轴， 起点在左上角
显存字节0，对应为行0的前面8个像素（列0-7） bit0- 列0， bit1- 列1， bit2- 列2， bit3- 列3， bit4- 列4， bit5- 列5， bit6- 列6， bit7- 列7
以此类推
*/



/**
 * @brief OLED显示适配
 * 
 */
typedef struct 
{
    /// 指向驱动
    const lcd_driver_ops_t *driver;
    /// 指向模型
    const lcd_model_t *model;
    /// 大小
    uint16_t xsize, ysize;
    /// 旋转角度0,90,180,270
    lcd_rotation_t rotation;
    /// 标志
#define LCD_FLAG_EXTERN_MEM         (1 << 0)
#define LCD_FLAG_PRINT_REFRESH_TIME (1 << 1)
    uint32_t flags;
    /// DRAM的大小
    uint32_t dram_size;
    /// 指向分配的内存
    uint8_t *dram;
    /// 指赂数据获取方式
    uint8_t(*dram_get_data)(const void *disp, uint16_t, uint16_t);
    /// 指向默认ASCII字体
    const lcd_font_t *default_ascii_font;
    /// 指向默认UNICDOE字体
    const lcd_font_t *default_unicode_font;
}lcd_display_t;


/**
* @brief 位反转函数 - 将字节的位顺序反转
* 
* @param byte 输入字节
* @return uint8_t 反转后的字节
*/
static inline uint8_t reverse_bits(uint8_t byte)
{
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= ((byte >> i) & 1) << (7 - i);
    }
    return result;
}

/**
 * @brief 获取RAM数据, 默认布局
 * 
 * @param disp 
 * @param dram_page_x 页X索引， 将X按字节分页
 * @param dram_y 行坐标
 * @return uint8_t 
 */
 static inline uint8_t dram_get_data(const void *disp, uint16_t dram_page_x, uint16_t dram_y)
 {
     const lcd_display_t *lcd = (const lcd_display_t *)disp;
     uint16_t row_byte_num = (lcd->xsize + 7) / 8;
     uint16_t offs = row_byte_num * dram_y + dram_page_x;
     return lcd->dram[offs];
 }
 
 /**
  * @brief 获取RAM数据
  * 
  * @param disp 显示handle
  * @param dram_page_y 页Y索引， 将Y按字节分页
  * @param dram_x 列坐标
  * @return uint8_t 
  * 
  *    # 将横向DRAM转换为竖向的数据
  *    # [B0][B1][B2]...[B15]
  *    # [B16]...
  *    # [7][6][5]... page p, clo c,
  *    # [7][6][5]...
  *    # [7][6][5]...
  *    # b[7] = ram.0[7]  0 = ((128/8)*8)*p + [7-7] * (128 / 8)  7 = 7 - (c % 8)
  *    # b[6] = ram.16[7] 16 = ((128/8)*8)*p + [7-6] * (128 / 8) c = 1, 6 c = 8, 7
  *    # b[5] = ram.32[7] 32 = ((128/8)*8)*p + [7-5] * (128 / 8)
  *    #  
  *    # 要取出8个字节，每个字节取一位
  *    # 取字偏移量公式f(p,b) = 128 * p + [7 - b] * 16
  *    # 取字位公式 g(c) = 7 - (c % 8)
  *    def dram_get_data(self, page, col)->int:
  *        ret = 0
  *        bit_ofs = 7 - (col % 8)
  *        for b in range(8):
  *            offs = self.xsize * page + b * (self.xsize // 8) + (col // 8)
  *            ret >>= 1
  *            if self.__dram[offs] & (1 << bit_ofs):
  *                ret |= 0x80
  *        return ret
  */
static inline uint8_t dram_get_data_vertical(const void *disp, uint16_t dram_x, uint16_t dram_page_y)
{
    const lcd_display_t *lcd = (const lcd_display_t *)disp;
    int page_size = lcd->xsize;
    uint8_t ret = 0;
    int bit_offset = 7 - (dram_x & 0x07);
    int offs = page_size * dram_page_y + (dram_x >> 3);
    for (int i = 0; i < 8; i ++)
    {
        ret >>= 1;
        if (lcd->dram[offs] & (1 << bit_offset))
        {
            ret |= 0x80;
        }
        offs += (page_size >> 3);
    }

    return ret;
}

static uint8_t dram_get_data_r0(const void *disp, uint16_t pos_page_x, uint16_t pos_y)
{
    return reverse_bits(dram_get_data(disp, pos_page_x, pos_y));
}

static uint8_t dram_get_data_r0_vertical(const void *disp, uint16_t pos_x, uint16_t pos_page_y)
{
    return dram_get_data_vertical(disp, pos_x, pos_page_y);
}


static uint8_t dram_get_data_r90(const void *disp, uint16_t pos_page_x, uint16_t pos_y)
{
    /*
    90度旋转，x和y互换
    使用dram_get_data_vertical函数，需要求对应的 dram_x 和 dram_page_y 
    256X64 为例：
    当 pos_page_x = 0, pos_y 时， dram_page_y = 0, dram_x = lcd->xsize - 1 - pos_y
    */
    lcd_display_t *lcd = (lcd_display_t *)disp;
    return dram_get_data_vertical(disp, lcd->xsize - 1 - pos_y, pos_page_x);
}

static uint8_t dram_get_data_r90_vertical(const void *disp, uint16_t pos_x, uint16_t pos_page_y)
{
    /*
    90度旋转，x和y互换
    使用dram_get_data函数，需要求对应的 dram_page_x 求dram_y 
    dram_page_x : page_num - pos_page_y - 1
    dram_y : pos_x
    */
    lcd_display_t *lcd = (lcd_display_t *)disp;
    uint8_t page_num = (lcd->xsize + 7) / 8;
    return dram_get_data(disp, page_num - pos_page_y - 1, pos_x);
}

static uint8_t dram_get_data_r180(const void *disp, uint16_t pos_page_x, uint16_t pos_y)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    int page_num = (lcd->xsize + 7) / 8;
    return dram_get_data(disp, page_num - 1 - pos_page_x, lcd->ysize - 1 - pos_y);
}

static uint8_t dram_get_data_r180_vertical(const void *disp, uint16_t pos_x, uint16_t pos_page_y)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    int page_num = (lcd->ysize + 7) / 8;

    uint8_t data = dram_get_data_vertical(disp, lcd->xsize - 1 - pos_x, page_num - 1 - pos_page_y);
    return reverse_bits(data);
}

static uint8_t dram_get_data_r270(const void *disp, uint16_t pos_page_x, uint16_t pos_y)
{
    /*
    270度旋转，x和y互换
    使用dram_get_data_vertical函数，需要求对应的 dram_x 和 dram_page_y 
    dram_x : pos_y
    dram_page_y : page_num - pos_page_x - 1
    */
    lcd_display_t *lcd = (lcd_display_t *)disp;
    uint8_t page_num = (lcd->ysize + 7) / 8;
    uint8_t data = dram_get_data_vertical(disp, pos_y, page_num - pos_page_x - 1);
    return reverse_bits(data);
}

static uint8_t dram_get_data_r270_vertical(const void *disp, uint16_t pos_x, uint16_t pos_page_y)
{
    /*
    90度旋转，x和y互换
    使用dram_get_data函数，需要求对应的 dram_page_x 求dram_y 
    dram_page_x : pos_page_y
    dram_y : dram_ysize - pos_x - 1
    */
    lcd_display_t *lcd = (lcd_display_t *)disp;
    uint8_t data = dram_get_data(disp, pos_page_y, lcd->ysize - pos_x - 1);
    return reverse_bits(data);
}

/**
 * @brief 创建一个OLED显示屏
 * 
 * @param driver 指向驱动
 * @param model 指向显示模型
 * @param rotation 旋转角度
 * @param static_mem 静态内存，如果为空，使用动态分配的内存
 * @param mem_size 静态内存大小
 * @return void* 
 * 返回一个OLED的HANDLE
 */
lcd_handle_t lcd_display_create(const lcd_driver_ops_t *driver, const lcd_model_t *model, lcd_rotation_t rotation, uint8_t *static_mem, uint32_t mem_size)
{
    lcd_display_t *lcd;
    int dram_size;
    int dx, dy;

    // 检查一下model 参数 
    if (driver == NULL || model == NULL)
    {
        ESP_LOGE(TAG, "LCD driver or model is NULL");
        return NULL;
    }
    // xsize 和 ysize 必须至少为8， 否则无法正常显示
    if (model->xsize < 8 || model->ysize < 8)
    {
        ESP_LOGE(TAG, "Model xsize or ysize is less than 8");
        return NULL;
    }

    if (model->set_page_address == NULL)
    {
        ESP_LOGE(TAG, "Model 'set_page_address' is NULL");
        return NULL;
    }

    if (rotation == LCD_ROTATION_90 || rotation == LCD_ROTATION_270)
    {
        dx = model->ysize;
        dy = model->xsize;
    }
    else
    {
        dx = model->xsize;
        dy = model->ysize;
    }

    // 计算显存大小, X/8 并向后取整
    // 显存的大小跟布局是设计出来的，跟物理显存无关
    dram_size = (dx + 7) / 8 * dy;

    /// 使用静态内存，确认是否足够
    if (static_mem && mem_size)
    {
        if (sizeof(*lcd) + dram_size > mem_size)
        {
            ESP_LOGE(TAG, "Static memory size is too small, expected:%d, got:%d", sizeof(*lcd) + dram_size, mem_size);
            return NULL;
        }

        memset(static_mem, 0, mem_size);

        lcd = (lcd_display_t *)static_mem;
        lcd->dram = &static_mem[sizeof(*lcd)];    
        lcd->flags |= LCD_FLAG_EXTERN_MEM;    
    }
    else 
    {
        lcd = (lcd_display_t *)malloc(sizeof(*lcd) + dram_size);
        if (lcd == NULL)
        {
            ESP_LOGE(TAG, "malloc(%d) failed", sizeof(*lcd) + dram_size);
            return NULL;
        }

        memset(lcd, 0, sizeof(*lcd) + dram_size);

        lcd->dram = (uint8_t *)&lcd[1];
    }

    lcd->driver = driver;
    lcd->model = model;
    lcd->xsize = dx;
    lcd->ysize = dy;
    lcd->dram_size = dram_size;
    lcd->rotation = rotation;

    if (lcd->rotation == LCD_ROTATION_90)
    {
        lcd->dram_get_data = model->dram_mode == LCD_DRAM_MODE_VERTICAL ? dram_get_data_r90_vertical : dram_get_data_r90;
    }
    else if (lcd->rotation == LCD_ROTATION_180)
    {
        lcd->dram_get_data = model->dram_mode == LCD_DRAM_MODE_VERTICAL ? dram_get_data_r180_vertical : dram_get_data_r180;
    }
    else if (lcd->rotation == LCD_ROTATION_270)
    {          
        lcd->dram_get_data = model->dram_mode == LCD_DRAM_MODE_VERTICAL ? dram_get_data_r270_vertical : dram_get_data_r270;
    }
    else 
    {
        lcd->dram_get_data = model->dram_mode == LCD_DRAM_MODE_VERTICAL ? dram_get_data_r0_vertical : dram_get_data_r0;
    }

    // 默认打印刷新时间
    lcd->flags |= LCD_FLAG_PRINT_REFRESH_TIME;

    // 初始化函数 
    driver->init(driver->data);

    ESP_LOGI(TAG, "lcd display created, %dX%d Rotate:%d", model->xsize, model->ysize, lcd->rotation);
    
    return lcd;    
}


/**
 * @brief 销废一个显示器
 * 
 * @param disp 
 */
void lcd_display_destory(lcd_handle_t disp)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;

    if (lcd && !(lcd->flags & LCD_FLAG_EXTERN_MEM))    
    {
        free(lcd);
    }
}

/**
 * @brief 写命令
 * 
 * @param disp 显示句柄
 * @param cmd 命令数组
 * @param size 命令长度
 */
void lcd_write_commands(const void *disp, const uint8_t *cmd, uint16_t size)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    _lcd_write_command(lcd->driver, cmd, size);
}
/**
 * @brief 写数据
 * 
 * @param disp 显示句柄
 * @param data 数据数组
 * @param size 数据长度
 */
void lcd_write_datas(const void *disp, const uint8_t *data, uint16_t size)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    _lcd_write_data(lcd->driver, data, size);
}


/**
 * @brief 获取DRAM数据, 根据布局不同，参数不同
 * 
 * @param disp 
 * @param page_x_or_x 页X索引， 将X按字节分页
 * @param page_y_or_y 页Y索引， 将Y按字节分页
 * @return uint8_t 
 */
uint8_t lcd_get_dram_data(const void *disp, uint16_t page_x_or_x, uint16_t page_y_or_y)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    return lcd->dram_get_data(disp, page_x_or_x, page_y_or_y);
}

/**
 * @brief 刷新屏幕数据
 * 
 * @param disp 
 */
// static void lcd_refresh1(const lcd_display_t *disp)
// {
//     for (int p = 0; p < disp->page_num; p ++)
//     {
//         // 页地址B0-B7H
//         // _set_command(disp, 0xb0 + p);
//         // // 列地址总是从0开始, 00H-0FH 10H-17H 
//         // _set_command(disp, 0x00);
//         // _set_command(disp, 0x10);

//         uint8_t cmd[3] = {0xb0 + p, 0x00, 0x10};
//         _set_multi_command(disp, cmd, 3);

//         uint8_t data[disp->page_size];
//         for (int c = 0; c < disp->page_size; c ++)
//         {
//             data[c] = disp->dram_get_data(disp, p, c);
//         }
//         _set_data_array(disp, data, disp->page_size);
//     }
// }

// /**
//  * @brief 刷新屏幕数据(多页刷新)
//  * 
//  * @param disp 
//  */
// static void lcd_refresh2(const lcd_display_t *disp)
// {
//     for (int p = 0; p < disp->page_num; p ++)
//     {
//         // _set_command(disp, 0xb0);
//         // _set_command(disp, p);
//         // _set_command(disp, 0x00);
//         // _set_command(disp, 0x11);

//         uint8_t cmd[4] = {0xb0, p, 0x00, 0x11};
//         _set_multi_command(disp, cmd, 4);

//         // 每次读出一列数据
//         // 第一列数据，应该是MSB[b7@0.0][b7@0.1]
//         uint8_t data[disp->page_size];        
//         for (int c = 0; c < disp->page_size; c ++)
//         {
//             data[c] = disp->dram_get_data(disp, p, c);
//             //data[c] = 0xF0;
//         }
//         _set_data_array(disp, data, disp->page_size);
//     }
// }

// static void lcd_refresh3(const lcd_display_t *disp)
// {
//     for (int p = 0; p < disp->ysize; p ++)
//     {
//         uint8_t cmd[4] = {0xb0, p, 0x00, 0x10};
//         _set_multi_command(disp, cmd, 4);

//         uint8_t data[128];
//         for (int c = 0; c < 128; c ++)
//         {
//             //uint8_t level = (p & 0x01) ? 0xF : 0x0;
//             // uint8_t level = ((p >> 2) & 0x0f); // 0-15, 16级灰度
//             uint8_t level = ((p >> 2) & 0x01) ? 0xF : 0x0; // 黑白相间
//             data[c] = (level << 4) | level;
//         }
//         _set_data_array(disp, data, sizeof(data));
//     }
// }


/**
 * @brief 刷新屏幕数据
 * 
 * @param disp 
 */
void lcd_refresh(lcd_handle_t disp)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    sys_tick_t start_time = uptime();

    // 基于两种显示存布局，总以一行行一写入数据，只是行数与每行字节数，根据这两种模式有所不同。
    const lcd_model_t *model = lcd->model;

    if (model->custom_refresh)
    {
        model->custom_refresh(disp, model);
    }
    else 
    {
        int x_num = (model->dram_mode == LCD_DRAM_MODE_VERTICAL) ? model->xsize : (model->xsize + 7) / 8;
        int y_num = (model->dram_mode == LCD_DRAM_MODE_VERTICAL) ? (model->ysize + 7) / 8 : model->ysize;
    
        // 从MCU搬动显示数据到LCD的内部DRAM中，这个方式是固定的，跟具体怎么旋转无关。
        // 显示方向旋转只是改变读取数据的方式，而不是改变搬动数据的方式。
    
        for (int y = 0; y < y_num; y ++)
        {
            model->set_page_address(disp, y, 0);
            uint8_t data[x_num];
            for (int x = 0; x < x_num; x ++)
            {
                data[x] = lcd->dram_get_data(disp, x, y);
            }
            lcd_write_datas(disp, data, x_num);
        }
    }

    sys_tick_t end_time = uptime();

    if (lcd->flags & LCD_FLAG_PRINT_REFRESH_TIME)
    {
        ESP_LOGI(TAG, "lcd refresh time: %d ms", end_time - start_time);
        lcd->flags &= ~LCD_FLAG_PRINT_REFRESH_TIME;
    }
}

/**
 * @brief 启动显示器
 * 
 * @param disp 
 */
void lcd_startup(lcd_handle_t disp)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;

    _lcd_reset(lcd->driver);

    _lcd_write_command(lcd->driver, lcd->model->init_datas, lcd->model->init_data_size);
}


/**
 * @brief 填充指定的数据 
 * 
 * @param disp 
 * @param data 
 */
void lcd_fill(lcd_handle_t disp, uint8_t data)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    memset(lcd->dram, data, lcd->dram_size);
}


/**
 * @brief 显示指定的连续位，高字节开始有效
 * 
 * @param disp 
 * @param x 
 * @param y 
 * @param value 
 * @param nbits 
 * @param reverse 是否反向显示
 */
static inline void _set_dram_bits(const lcd_display_t *disp, int x, int y, uint8_t value, uint8_t nbits, bool reverse)
{
    // 求出所在坐标点的字节位偏移量
    int offs = y * disp->xsize + x;

    for (int i = 0; i < nbits; i ++, offs ++)
    {
        // 有位偏移后，根据位偏移，得到字节偏移: offs >> 3， 得到位偏移为(offs & 0x07）
        // 因为我们显存是高位在左，低位在右（字库也是按这样排的） [B0] == {b7 b6 b5 b4 b3 b2 b1 b0}
        // 位偏移为0，表示在第七位，位偏移为1，表示在第6位...

        // 总是从高位开始处理，高位放在左边
        bool bit_value = (value & 0x80) != 0;
        
        // 如果是反向显示，则反转位值
        if (reverse) {
            bit_value = !bit_value;
        }
        
        if (bit_value)
        {
            disp->dram[offs >> 3] |= (1 << (7 - (offs & 0x07)));
        } 
        else 
        {
            disp->dram[offs >> 3] &= ~(1 << (7 - (offs & 0x07)));
        }
        value <<= 1;
    }
}


/**
 * @brief 设置默认字体
 * 
 * @param disp 
 * @param ascii_font 
 * @param unicode_font 
 */
void lcd_set_default_fonts(lcd_handle_t disp, const lcd_font_t *ascii_font, const lcd_font_t *unicode_font)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    lcd->default_ascii_font = ascii_font;
    lcd->default_unicode_font = unicode_font;
}

/**
 * @brief 解析UTF-8字符并返回Unicode码点
 * 
 * @param utf8_str UTF-8编码的字符串指针
 * @param unicode 输出Unicode码点
 * @return int 返回消耗的字节数，0表示解析失败
 */
static int parse_utf8_char(const char *utf8_str, uint32_t *unicode)
{
    if (!utf8_str || !unicode) {
        return 0;
    }
    
    unsigned char first_byte = (unsigned char)utf8_str[0];
    
    // ASCII字符 (0-127)
    if (first_byte < 0x80) {
        *unicode = first_byte;
        return 1;
    }
    
    // 多字节UTF-8字符
    int bytes = 0;
    if ((first_byte & 0xE0) == 0xC0) {
        bytes = 2;  // 2字节UTF-8
    } else if ((first_byte & 0xF0) == 0xE0) {
        bytes = 3;  // 3字节UTF-8
    } else if ((first_byte & 0xF8) == 0xF0) {
        bytes = 4;  // 4字节UTF-8
    } else {
        // 无效的UTF-8起始字节
        return 0;
    }
    
    // 检查是否有足够的字节
    for (int i = 1; i < bytes; i++) {
        if ((utf8_str[i] & 0xC0) != 0x80) {
            // 无效的UTF-8延续字节
            return 0;
        }
    }
    
    // 提取Unicode码点
    *unicode = 0;
    if (bytes == 2) {
        *unicode = ((first_byte & 0x1F) << 6) | (utf8_str[1] & 0x3F);
    } else if (bytes == 3) {
        *unicode = ((first_byte & 0x0F) << 12) | 
                   ((utf8_str[1] & 0x3F) << 6) | 
                   (utf8_str[2] & 0x3F);
    } else if (bytes == 4) {
        *unicode = ((first_byte & 0x07) << 18) | 
                   ((utf8_str[1] & 0x3F) << 12) | 
                   ((utf8_str[2] & 0x3F) << 6) | 
                   (utf8_str[3] & 0x3F);
    }
    
    return bytes;
}

/**
 * @brief 判断字符是否为ASCII字符
 * 
 * @param unicode Unicode码点
 * @return true 是ASCII字符
 * @return false 不是ASCII字符
 */
static bool is_ascii_char(uint32_t unicode)
{
    return unicode < 0x80;
}


/**
 * @brief 查找字库并显示，较底层函数，支持部分显示。
 * 
 * @param disp disp handle
 * @param x 
 * @param y 
 * @param ch 字符，有可能是ASCII，也有可能是UNICODE
 * @param font 字体，如果为NULL，使用默认字体
 * @param reverse 是否反向显示
 * @return int 返回实际显示的像素宽度
 */
int lcd_display_char(lcd_handle_t disp, int x, int y, int ch, const lcd_font_t *font, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;    
    int data_index = 0;
    int displayed_width = 0;

    if (font == NULL)
    {
        ESP_LOGE(TAG, "No font specified!!");
        return 0;
    }

    const uint8_t *font_code = font->get_code_data(font, ch);
    if (font_code == NULL)
    {
        ESP_LOGE(TAG, "Unabled to find font data of %06x", ch);
        return 0;
    }

    // 检查是否完全在屏幕外
    if (x >= lcd->xsize || y >= lcd->ysize || x + font->width <= 0 || y + font->height <= 0)
    {
        return 0;
    }

    // 计算实际可显示的区域
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + font->width > lcd->xsize) ? lcd->xsize : x + font->width;
    int end_y = (y + font->height > lcd->ysize) ? lcd->ysize : y + font->height;
    
    displayed_width = end_x - start_x;

    // 根据字体的宽度和大小设置显存位
    for (int h = 0; h < font->height; h++)
    {
        // 跳过屏幕外的垂直行
        if (y + h < start_y || y + h >= end_y)
        {
            data_index += (font->width + 7) / 8;  // 跳过这一行的字体数据
            continue;
        }

        // 算出字库一行有多少个字节，注意字宽度不为8的整数时
        int left_bits = font->width;
        //int byte_index = 0;
        int x_offset = 0;  // 用于跟踪水平方向的偏移

        while (left_bits > 0)
        {
            // 拿到第一个字节的数据，根据宽度处理数据
            int fbits = (left_bits > 8) ? 8 : left_bits;
            uint8_t fdata = font_code[data_index++];

            // 检查当前8像素组是否在显示范围内
            if (x + x_offset + 8 > start_x && x + x_offset < end_x)
            {
                // 计算需要显示的位数
                int display_start = (x + x_offset < start_x) ? start_x - (x + x_offset) : 0;
                int display_end = (x + x_offset + fbits > end_x) ? end_x - (x + x_offset) : fbits;
                
                if (display_end > display_start)
                {
                    // 调整数据，只显示可见部分
                    uint8_t adjusted_data = fdata;
                    if (display_start > 0)
                    {
                        adjusted_data &= (0xFF >> display_start);
                    }
                    if (display_end < 8)
                    {
                        adjusted_data &= (0xFF << (8 - display_end));
                    }
                    
                    _set_dram_bits(lcd, x + x_offset + display_start, y + h, 
                                 adjusted_data << display_start, display_end - display_start, reverse);
                }
            }

            left_bits -= fbits;
            //byte_index++;
            x_offset += 8;
        }
    }

    return displayed_width;
}


/**
 * @brief 显示一串文本，支持部分显示。如果字符超出显示区域，会显示能显示的部分
 * 
 * @param disp 
 * @param x 显示位置X, 水平方向, 从左到右
 * @param y 显示位置Y, 垂直方向, 从上到下
 * @param text 需要显示的文本
 * @param ascii_font ASCII字体
 * @param unicode_font UNICODE 字体
 * @param reverse 是否反向显示
 * 
 * @return int 返回显示的字符数量
 */
int lcd_display_string(lcd_handle_t disp, int x, int y, const char *text, const lcd_font_t *ascii_font, const lcd_font_t *unicode_font, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;       
    const char *ch = text;     
    int count = 0;
    int current_x = x;

    if (!text)
    {
        return count;
    }

    // 如果起始位置完全在屏幕下方，直接返回
    if (y >= lcd->ysize)
    {
        return count;
    }

    if (ascii_font == NULL) 
    {
        ascii_font = lcd->default_ascii_font;
    }

    if (unicode_font == NULL)
    {
        unicode_font = lcd->default_unicode_font;
    }

    // 显示每个字符，即使只能部分显示
    while (*ch)
    {
        uint32_t unicode;
        int bytes_consumed = parse_utf8_char(ch, &unicode);
        
        if (bytes_consumed == 0)
        {
            // UTF-8解析失败，跳过这个字符
            ESP_LOGW(TAG, "Failed to parse UTF-8 character at position %d", (int)(ch - text));
            ch++;
            continue;
        }
        
        // 选择字体
        const lcd_font_t *font = is_ascii_char(unicode) ? ascii_font : unicode_font;

        // 显示字符
        int width = lcd_display_char(disp, current_x, y, unicode, font, reverse);
        if (width > 0)
        {
            count++;
            current_x += font->width;  // 仍然使用完整字体宽度移动位置
        }
        else if (current_x >= lcd->xsize)  // 如果已经完全超出右边界
        {
            break;
        }
        
        ch += bytes_consumed;
    }

    return count;
}

/**
 * @brief 显示单色位图，支持部分显示
 * 
 * @param disp 显示对象 
 * @param x 显示位置X
 * @param y 显示位置Y
 * @param img 位图对象
 * @param reverse 是否反向显示
 * 
 * @return int 返回实际显示的像素宽度，如果完全不可见则返回0
 */
int lcd_display_mono_img(lcd_handle_t disp, int x, int y, const lcd_mono_img_t *img, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;    
    int displayed_width = 0;

    if (!lcd || !img || !img->data)
    {
        ESP_LOGE(TAG, "Invalid parameters for mono image display");
        return 0;
    }

    // 检查是否完全在屏幕外
    if (x >= lcd->xsize || y >= lcd->ysize || 
        x + img->width <= 0 || y + img->height <= 0)
    {
        return 0;
    }

    // 计算实际可显示的区域
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + img->width > lcd->xsize) ? lcd->xsize : x + img->width;
    int end_y = (y + img->height > lcd->ysize) ? lcd->ysize : y + img->height;
    
    displayed_width = end_x - start_x;

    // 遍历图像的每一行
    for (int h = 0; h < img->height; h++)
    {
        // 跳过屏幕外的垂直行
        if (y + h < start_y || y + h >= end_y)
        {
            continue;
        }

        // 计算当前行在图像数据中的起始位置
        // 每行的字节数 = (图像宽度 + 7) / 8
        int row_bytes = (img->width + 7) / 8;
        int row_offset = h * row_bytes;
        int x_offset = 0;  // 用于跟踪水平方向的偏移

        // 处理这一行的所有字节
        for (int byte_idx = 0; byte_idx < row_bytes; byte_idx++)
        {
            uint8_t img_byte = img->data[row_offset + byte_idx];
            int bits_in_byte = (byte_idx == row_bytes - 1 && img->width % 8) ? 
                              (img->width % 8) : 8;

            // 检查当前8像素组是否在显示范围内
            if (x + x_offset + 8 > start_x && x + x_offset < end_x)
            {
                // 计算需要显示的位数
                int display_start = (x + x_offset < start_x) ? start_x - (x + x_offset) : 0;
                int display_end = (x + x_offset + bits_in_byte > end_x) ? 
                                end_x - (x + x_offset) : bits_in_byte;
                
                if (display_end > display_start)
                {
                    // 调整数据，只显示可见部分
                    uint8_t adjusted_data = img_byte;
                    if (display_start > 0)
                    {
                        adjusted_data &= (0xFF >> display_start);
                    }
                    if (display_end < 8)
                    {
                        adjusted_data &= (0xFF << (8 - display_end));
                    }
                    
                    _set_dram_bits(lcd, x + x_offset + display_start, y + h, 
                                 adjusted_data << display_start, display_end - display_start, reverse);
                }
            }
            x_offset += 8;
        }
    }
    
    return displayed_width;
}

/**
 * @brief 绘制垂直线
 * 
 * @param disp LCD显示句柄
 * @param x 起始x坐标
 * @param y 起始y坐标
 * @param length 线的长度(垂直方向)
 * @param width 线宽(水平方向)
 * @param reverse 是否反向显示
 * @return int 成功返回0，失败返回-1
 */
int lcd_draw_vertical_line(lcd_handle_t disp, int x, int y, int length, int width, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    
    if (!lcd || width <= 0 || length <= 0) {
        return -1;
    }
    
    // 检查是否完全在屏幕外
    if (x >= lcd->xsize || y >= lcd->ysize || x + width <= 0 || y + length <= 0) {
        return -1;
    }
    
    ESP_LOGD(TAG, "draw vertical line @(%d,%d), length=%d, width=%d", x, y, length, width);

    // 限制在屏幕范围内
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + width > lcd->xsize) ? lcd->xsize : x + width;
    int end_y = (y + length > lcd->ysize) ? lcd->ysize : y + length;
    
    // 实际绘制的宽度
    int actual_width = end_x - start_x;
    int actual_length = end_y - start_y;
    
    ESP_LOGD(TAG, "actual width=%d, actual length=%d", actual_width, actual_length);

    // 在垂直方向上逐像素绘制
    for (int curr_y = start_y; curr_y < end_y; curr_y++) {
        // 在水平方向上设置宽度
        for (int i = 0; i < actual_width; i++) {
            // 计算内存中的位偏移
            int offs = (curr_y * lcd->xsize + (start_x + i));
            // 根据reverse参数设置或清除位
            if (reverse) {
                lcd->dram[offs >> 3] &= ~(1 << (7 - (offs & 0x07)));
            } else {
                lcd->dram[offs >> 3] |= (1 << (7 - (offs & 0x07)));
            }
        }
    }
    
    return 0;
}

/**
 * @brief 绘制水平线
 * 
 * @param disp LCD显示句柄
 * @param x 起始x坐标
 * @param y 起始y坐标
 * @param length 线的长度(水平方向)
 * @param width 线宽(垂直方向)
 * @param reverse 是否反向显示
 * @return int 成功返回0，失败返回-1
 */
int lcd_draw_horizontal_line(lcd_handle_t disp, int x, int y, int length, int width, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    
    if (!lcd || width <= 0 || length <= 0) {
        return -1;
    }
    
    // 检查是否完全在屏幕外
    if (x >= lcd->xsize || y >= lcd->ysize || x + length <= 0 || y + width <= 0) {
        return -1;
    }

    ESP_LOGD(TAG, "draw horizontal line @(%d,%d), length=%d, width=%d", x, y, length, width);
    
    // 限制在屏幕范围内
    int start_x = (x < 0) ? 0 : x;
    int start_y = (y < 0) ? 0 : y;
    int end_x = (x + length > lcd->xsize) ? lcd->xsize : x + length;
    int end_y = (y + width > lcd->ysize) ? lcd->ysize : y + width;
    
    // 实际绘制的宽度和长度
    int actual_width = end_y - start_y;
    int actual_length = end_x - start_x;

    ESP_LOGD(TAG, "actual width=%d, actual length=%d", actual_width, actual_length);
    
    // 在垂直方向上设置线宽
    for (int curr_y = start_y; curr_y < end_y; curr_y++) {
        // 在水平方向上设置长度
        for (int curr_x = start_x; curr_x < end_x; curr_x++) {
            // 计算内存中的位偏移
            int offs = (curr_y * lcd->xsize + curr_x);
            // 根据reverse参数设置或清除位
            if (reverse) {
                lcd->dram[offs >> 3] &= ~(1 << (7 - (offs & 0x07)));
            } else {
                lcd->dram[offs >> 3] |= (1 << (7 - (offs & 0x07)));
            }
        }
    }
    
    return 0;
}

/**
 * @brief 绘制矩形
 * 
 * @param disp LCD显示句柄
 * @param start_x 左上角x坐标
 * @param start_y 左上角y坐标
 * @param end_x 右下角x坐标
 * @param end_y 右下角y坐标
 * @param width 边框宽度
 * @param reverse 是否反向显示
 * @return int 成功返回0，失败返回-1
 */
int lcd_draw_rectangle(lcd_handle_t disp, int start_x, int start_y, int end_x, int end_y, int width, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;
    
    if (!lcd || width <= 0) {
        return -1;
    }
    
    // 确保起点和终点顺序正确
    if (start_x > end_x) {
        int temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    
    if (start_y > end_y) {
        int temp = start_y;
        start_y = end_y;
        end_y = temp;
    }

    ESP_LOGD(TAG, "draw rectangle @(%d,%d), end_x=%d, end_y=%d, width=%d", start_x, start_y, end_x, end_y, width);
    
    // 计算矩形宽高
    int rect_width = end_x - start_x + 1;
    int rect_height = end_y - start_y + 1;
    
    // 检查是否是有效的矩形
    if (rect_width <= 0 || rect_height <= 0) {
        return -1;
    }
    
    // 如果线宽超过矩形尺寸的一半，就填充整个矩形
    if (width * 2 >= rect_width || width * 2 >= rect_height) {
        // 填充整个矩形区域
        for (int y = start_y; y <= end_y; y++) {
            for (int x = start_x; x <= end_x; x++) {
                int offs = (y * lcd->xsize + x);
                if (reverse) {
                    lcd->dram[offs >> 3] &= ~(1 << (7 - (offs & 0x07)));
                } else {
                    lcd->dram[offs >> 3] |= (1 << (7 - (offs & 0x07)));
                }
            }
        }
    } else {
        // 绘制四条边
        // 上边
        lcd_draw_horizontal_line(disp, start_x, start_y, rect_width, width, reverse);
        // 下边
        lcd_draw_horizontal_line(disp, start_x, end_y - width + 1, rect_width, width, reverse);
        // 左边
        lcd_draw_vertical_line(disp, start_x, start_y, rect_height, width, reverse);
        // 右边
        lcd_draw_vertical_line(disp, end_x - width + 1, start_y, rect_height, width, reverse);
    }
    
    return 0;
}

/**
 * @brief 绘制矩形方法2
 * 
 * @param disp LCD显示句柄
 * @param start_x 左上角x坐标
 * @param start_y 左上角y坐标
 * @param x_len 矩形宽度
 * @param y_len 矩形高度
 * @param width 线宽(向内缩进)
 * @param reverse 是否反向显示
 * @return int 成功返回0，失败返回-1
 */
int lcd_draw_rectangle1(lcd_handle_t disp, int start_x, int start_y, int x_len, int y_len, int width, bool reverse)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;

    // 基本参数检查
    if (!lcd || width <= 0) {
        return -1;
    }

    // 检查矩形尺寸是否满足最小要求
    // 宽度和高度都必须大于等于2倍的线宽，否则无法正确绘制
    if (x_len < 2 * width || y_len < 2 * width) {
        ESP_LOGW(TAG, "Rectangle size too small for line width. x_len=%d, y_len=%d, width=%d", 
            x_len, y_len, width);
        return -1;
    }

    // 检查是否完全在屏幕外
    if (start_x >= lcd->xsize || start_y >= lcd->ysize || 
        start_x + x_len <= 0 || start_y + y_len <= 0) {
        ESP_LOGW(TAG, "Rectangle out of screen. x=%d, y=%d", start_x, start_y);
        return -1;
    }

    // 计算每条边的起始和结束位置
    for (int i = 0; i < width; i++) {
        // 绘制上边
        lcd_draw_horizontal_line(disp, 
            start_x + i,          // 起始x
            start_y + i,          // 起始y
            x_len - 2 * i,        // 长度
            1,                    // 线宽
            reverse               // 反向显示
        );

        // 绘制下边
        lcd_draw_horizontal_line(disp,
            start_x + i,          // 起始x
            start_y + y_len - i - 1, // 起始y
            x_len - 2 * i,        // 长度
            1,                    // 线宽
            reverse               // 反向显示
        );

        // 绘制左边
        lcd_draw_vertical_line(disp,
            start_x + i,          // 起始x
            start_y + i,          // 起始y
            y_len - 2 * i,        // 长度
            1,                    // 线宽
            reverse               // 反向显示
        );

        // 绘制右边
        lcd_draw_vertical_line(disp,
            start_x + x_len - i - 1, // 起始x
            start_y + i,          // 起始y
            y_len - 2 * i,        // 长度
            1,                    // 线宽
            reverse               // 反向显示
        );
    }

    return 0;
}


/**
 * @brief 清除指定区域的显示内容
 * 
 * @param disp LCD显示句柄
 * @param x 起始x坐标
 * @param y 起始y坐标
 * @param width 要清除的宽度(像素)
 * @param height 要清除的高度(像素)
 * @param value 填充的值: 0 - 清空, 1 - 填充
 * @return int 成功返回0，失败返回-1
 */
int lcd_fill_area(lcd_handle_t disp, int x, int y, int width, int height, uint8_t value)
{
    lcd_display_t *lcd = (lcd_display_t *)disp;    
    
    // 参数检查
    if (!lcd || width <= 0 || height <= 0) {
        return -1;
    }

    // 检查是否超出屏幕范围
    if (x >= lcd->xsize || y >= lcd->ysize) {
        return -1;
    }

    // 调整宽度和高度，确保不超出屏幕
    if (x + width > lcd->xsize) {
        width = lcd->xsize - x;
    }
    if (y + height > lcd->ysize) {
        height = lcd->ysize - y;
    }

    // 根据字体的宽度和大小设置显存位
    for (int h = 0; h < height; h++) {
        // 算出一行有多少个字节
        int left_bits = width;
        int byte_index = 0;

        while (left_bits > 0) {
            // 每次处理8位
            int fbits = (left_bits > 8) ? 8 : left_bits;
            // 使用0来清除显示
            _set_dram_bits(lcd, x + byte_index * 8, y + h, value ? 0xff : 0x00, fbits, false);
            left_bits -= fbits;
            byte_index++;
        }
    }

    return 0;
}