#ifndef __LCD_DRIVER_H__
#define __LCD_DRIVER_H__

/**
 * @file lcd_driver.h
 * @author LiuChuansen (1797120666@qq.com)
 * @brief LCD驱动头文件
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
/**
 * @brief LCD驱动操作接口
 * 
 */
typedef struct 
{
    /// 驱动数据 
    const void *data;
    /// 初始化
    void (*init)(const void *);    
    /// 复位
    void (*reset)(const void *);
    /// 写命令
    void (*write_command)(const void *, const uint8_t *, uint16_t);
    /// 写数据
    void (*write_dram_data)(const void *, const uint8_t *, uint16_t);
}lcd_driver_ops_t;


///声明快速OPS操作函数
static inline void _lcd_init(const lcd_driver_ops_t *ops)
{
    ops->init(ops->data);
}

static inline void _lcd_reset(const lcd_driver_ops_t *ops)
{
    ops->reset(ops->data);
}

static inline void _lcd_write_command(const lcd_driver_ops_t *ops, const uint8_t *cmd, uint16_t size)
{
    ops->write_command(ops->data, cmd, size);
}

static inline void _lcd_write_command0(const lcd_driver_ops_t *ops, uint8_t cmd)
{
    ops->write_command(ops->data, &cmd, 1);
}

static inline void _lcd_write_command1(const lcd_driver_ops_t *ops, uint8_t cmd, uint8_t data)
{
    uint8_t cmd_data[2] = {cmd, data};
    ops->write_command(ops->data, cmd_data, 2);
}

static inline void _lcd_write_command2(const lcd_driver_ops_t *ops, uint8_t cmd, uint8_t data1, uint8_t data2)
{
    uint8_t cmd_data[3] = {cmd, data1, data2};
    ops->write_command(ops->data, cmd_data, 3);
}

static inline void _lcd_write_data(const lcd_driver_ops_t *ops, const uint8_t *data, uint16_t size)
{
    ops->write_dram_data(ops->data, data, size);
}


/// 声明驱动接口函数

/**
 * @brief 空操作
 * 
 * @param data 
 */
static inline void lcd_ops_dummy(const void *drv)
{
    // DO NOTHING
}

/// 引用一个驱动 
#define LCD_DRIVER(_name) &s_lcd_driver_##_name


#ifdef __cplusplus
}
#endif

#endif // __LCD_DRIVER_H__
