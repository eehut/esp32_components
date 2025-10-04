#ifndef __LCD_MODEL_TYPE_H__
#define __LCD_MODEL_TYPE_H__

/**
 * @file lcd_model_type.h
 * @author LiuChuansen (1797120666@qq.com)
 * @brief LCD模型头文件
 * @version 0.1
 * @date 2025-05-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_types.h"


/// 显存模式
typedef enum {
    /// 默认模式, 存储布局跟显存保持一致 
    LCD_DRAM_MODE_DEFAULT = 0,
    /// 行页模式, 将Y轴分为8页, 通过dram_get_ypage_data（page, x）获取数据
    LCD_DRAM_MODE_VERTICAL = 1,
}lcd_dram_mode_t;


struct LcdModel
{
    /// 名称    
    const char *name;
    /// 大小
    uint16_t xsize, ysize;
    /// 指向初始数据
    const uint8_t *init_datas;
    /// 指向初始数据长度
    uint16_t init_data_size;
    /// 显存模式
    uint8_t dram_mode;
    /// 设置页地址
    void (*set_page_address)(const void *, const uint16_t, uint16_t);

    /// 自定义刷新函数， 如果为空，则使用默认刷新函数
    void (*custom_refresh)(const void *,  const struct LcdModel * model);
};

typedef struct LcdModel lcd_model_t;

/// 定义一个OLED模型
#define LCD_MODEL_DEFINE(_name, _xsize, _ysize, _init, _dram_mode, _set_page_func) \
static const uint8_t s_lcd_init_data_##_name[] = _init; \
static const lcd_model_t s_lcd_model_##_name = { \
    .name = #_name,  \
    .xsize = _xsize, .ysize = _ysize, \
    .init_datas = s_lcd_init_data_##_name, \
    .init_data_size = sizeof(s_lcd_init_data_##_name), \
    .dram_mode = (uint8_t)_dram_mode, \
    .set_page_address = _set_page_func, \
    .custom_refresh = NULL, \
} 

#define LCD_MODEL_DEFINE_WITH_CUSTOM_REFRESH(_name, _xsize, _ysize, _init, _dram_mode, _set_page_func, _custom_refresh) \
static const uint8_t s_lcd_init_data_##_name[] = _init; \
static const lcd_model_t s_lcd_model_##_name = { \
    .name = #_name,  \
    .xsize = _xsize, .ysize = _ysize, \
    .init_datas = s_lcd_init_data_##_name, \
    .init_data_size = sizeof(s_lcd_init_data_##_name), \
    .dram_mode = (uint8_t)_dram_mode, \
    .set_page_address = _set_page_func, \
    .custom_refresh = _custom_refresh, \
}

/// 引用一个MODEL
#define LCD_MODEL(_name)  &s_lcd_model_##_name


/**
 * @brief 写命令
 * 
 * @param disp 显示句柄
 * @param cmd 命令数组
 * @param size 命令长度
 */
 extern void lcd_write_commands(const void *disp, const uint8_t *cmd, uint16_t size);

 /**
  * @brief 写数据
  * 
  * @param disp 显示句柄
  * @param data 数据数组
  * @param size 数据长度
  */
 extern void lcd_write_datas(const void *disp, const uint8_t *data, uint16_t size);

 /**
 * @brief 获取DRAM数据, 根据布局不同，参数不同
 * 
 * @param disp 
 * @param page_x_or_x 页X索引， 将X按字节分页
 * @param page_y_or_y 页Y索引， 将Y按字节分页
 * @return uint8_t 
 */
extern uint8_t lcd_get_dram_data(const void *disp, uint16_t page_x_or_x, uint16_t page_y_or_y);


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_TYPE_H__
