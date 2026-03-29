#ifndef __LCD_MODEL_TYPE_H__
#define __LCD_MODEL_TYPE_H__

/**
 * @file lcd_model_type.h
 * @author LiuChuansen (1797120666@qq.com)
 * @brief LCD 模型头文件
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
    /// 默认模式，存储布局跟显存保持一致 
    LCD_DRAM_MODE_DEFAULT = 0,
    /// 行页模式，将 Y 轴分为 8 页，通过 dram_get_ypage_data（page, x）获取数据
    LCD_DRAM_MODE_VERTICAL = 1,
}lcd_dram_mode_t;

/// 显示控制命令
typedef enum {
    LCD_CMD_DISPLAY_ON = 0,      ///< 开启显示
    LCD_CMD_DISPLAY_OFF = 1,     ///< 关闭显示
}lcd_cmd_t;

/// 显示控制返回值
#define LCD_CMD_OK              0   ///< 命令执行成功
#define LCD_CMD_NOT_SUPPORTED   -1  ///< 不支持的命令

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

    /// 自定义刷新函数，如果为空，则使用默认刷新函数
    void (*custom_refresh)(const void *,  const struct LcdModel * model);

    /// 显示控制函数，支持开启/关闭显示等命令
    /// @param disp 显示句柄
    /// @param command 命令类型 (LCD_CMD_DISPLAY_ON/OFF)
    /// @param data 命令附加数据 (可选)
    /// @param len 数据长度
    /// @return int LCD_CMD_OK 成功，LCD_CMD_NOT_SUPPORTED 不支持的命令
    int (*display_control)(const void *disp, uint8_t command, const uint8_t *data, uint16_t len);
};

typedef struct LcdModel lcd_model_t;

/// 定义一个 OLED 模型
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
    .display_control = NULL, \
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
    .display_control = NULL, \
}

/// 定义带自定义显示控制函数的模型
#define LCD_MODEL_DEFINE_WITH_DISPLAY_CONTROL(_name, _xsize, _ysize, _init, _dram_mode, _set_page_func, _display_control) \
static const uint8_t s_lcd_init_data_##_name[] = _init; \
static const lcd_model_t s_lcd_model_##_name = { \
    .name = #_name,  \
    .xsize = _xsize, .ysize = _ysize, \
    .init_datas = s_lcd_init_data_##_name, \
    .init_data_size = sizeof(s_lcd_init_data_##_name), \
    .dram_mode = (uint8_t)_dram_mode, \
    .set_page_address = _set_page_func, \
    .custom_refresh = NULL, \
    .display_control = _display_control, \
}

/// 定义带自定义刷新和显示控制函数的模型
#define LCD_MODEL_DEFINE_WITH_CUSTOM_REFRESH_AND_DISPLAY_CONTROL(_name, _xsize, _ysize, _init, _dram_mode, _set_page_func, _custom_refresh, _display_control) \
static const uint8_t s_lcd_init_data_##_name[] = _init; \
static const lcd_model_t s_lcd_model_##_name = { \
    .name = #_name,  \
    .xsize = _xsize, .ysize = _ysize, \
    .init_datas = s_lcd_init_data_##_name, \
    .init_data_size = sizeof(s_lcd_init_data_##_name), \
    .dram_mode = (uint8_t)_dram_mode, \
    .set_page_address = _set_page_func, \
    .custom_refresh = _custom_refresh, \
    .display_control = _display_control, \
}

/// 引用一个 MODEL
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
 * @brief 获取 DRAM 数据，根据布局不同，参数不同
 * 
 * @param disp 
 * @param page_x_or_x 页 X 索引，将 X 按字节分页
 * @param page_y_or_y 页 Y 索引，将 Y 按字节分页
 * @return uint8_t 
 */
extern uint8_t lcd_get_dram_data(const void *disp, uint16_t page_x_or_x, uint16_t page_y_or_y);



/**
 * @brief 设置 SSD1306 的页地址
 * 
 * @param disp 
 * @param page 
 * @param offset 
 */
static inline void lcd_set_page_address_ssd1306_compatible(const void *disp, const uint16_t page, uint16_t offset)
{
    uint8_t cmd[3] = {0xb0 + page, offset & 0x0f, 0x10 + (offset >> 4)};
    lcd_write_commands(disp, cmd, 3);
}

/**
 * @brief 设置 SH1108 的页地址
 * 
 * @param disp 
 * @param page 
 * @param offset 
 */
static inline void lcd_set_page_address_sh1108_compatible(const void *disp, const uint16_t page, uint16_t offset)
{
    uint8_t cmd[4] = {0xb0,  page, offset & 0x0f, 0x10 + (offset >> 4)};
    lcd_write_commands(disp, cmd, 4);
}


#ifdef __cplusplus
}
#endif

#endif // __LCD_MODEL_TYPE_H__
