#ifndef __LCD_DRIVER_SPI_H__
#define __LCD_DRIVER_SPI_H__

/**
 * @file lcd_driver_spi.h
 * @author LiuChuansen (1797120666@qq.com)
 * @brief LCD驱动头文件 - SPI
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

#include "lcd_driver.h"
#include "driver/gpio.h"



/**
 * @brief SPI驱动数据接口
 * 
 */
typedef struct 
{
    int16_t sda;
    int16_t scl;
    int16_t dc;
    int16_t rst;
    int16_t cs;
}lcd_gpio_spi_data_t;


/// SPI 初始化
extern void lcd_ops_gpio_spi_init(const void *drv);
/// SPI写
extern void lcd_ops_gpio_spi_write_command(const void *drv, const uint8_t *data, uint16_t size);
/// SPI写数据
extern void lcd_ops_gpio_spi_write_dram_data(const void *drv, const uint8_t *data, uint16_t size);
/// SPI 复位
extern void lcd_ops_gpio_spi_reset(const void *drv);


///声明一个SPI的OLED驱动 
#define LCD_DEFINE_DRIVER_GPIO_SPI(_name, _data, _clk, _dc, _rst, _cs) \
static const lcd_gpio_spi_data_t s_lcd_data_##_name = {_data, _clk, _dc, _rst, _cs}; \
static const lcd_driver_ops_t s_lcd_driver_##_name = \
{ \
    .data = &s_lcd_data_##_name, \
    .init = lcd_ops_gpio_spi_init, \
    .write_command = lcd_ops_gpio_spi_write_command, \
    .write_dram_data = lcd_ops_gpio_spi_write_dram_data, \
    .reset = lcd_ops_gpio_spi_reset \
}

#ifdef __cplusplus
}
#endif

#endif // __LCD_DRIVER_SPI_H__
