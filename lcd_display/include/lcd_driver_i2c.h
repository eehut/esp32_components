#ifndef __LCD_DRIVER_I2C_H__
#define __LCD_DRIVER_I2C_H__

/**
 * @file lcd_driver_i2c.h
 * @author LiuChuansen (1797120666@qq.com)
 * @brief LCD驱动头文件 - I2C
 * @version 0.1
 * @date 2025-10-02
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 #include <stdint.h>
#ifdef __cplusplus
 extern "C" {
 #endif

 #include "esp_types.h"
 #include "driver/i2c_master.h"
 #include "lcd_driver.h"
 #include "bus_manager.h"


/**
 * @brief I2C驱动总线接口
 * 
 */
typedef struct 
{
    i2c_bus_t bus;       // 总线ID
    uint16_t address;      // 设备地址
}lcd_i2c_data_t;

/// I2C 初始化
extern void lcd_ops_i2c_init(const void *drv);
/// I2C写
extern void lcd_ops_i2c_write_command(const void *drv, const uint8_t *data, uint16_t size);
/// I2C写数据
extern void lcd_ops_i2c_write_dram_data(const void *drv, const uint8_t *data, uint16_t size);

/// 声明一个I2C的驱动 
#define LCD_DEFINE_DRIVER_I2C(_name, _bus, _addr) \
static const lcd_i2c_data_t s_lcd_data_##_name = {.bus = _bus, .address = _addr}; \
static const lcd_driver_ops_t s_lcd_driver_##_name = \
{ \
    .data = &s_lcd_data_##_name, \
    .init = lcd_ops_i2c_init, \
    .write_command = lcd_ops_i2c_write_command, \
    .write_dram_data = lcd_ops_i2c_write_dram_data, \
    .reset = lcd_ops_dummy \
}

#ifdef __cplusplus
}
#endif

#endif // __LCD_DRIVER_I2C_H__
