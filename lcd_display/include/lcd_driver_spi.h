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
#include "driver/spi_master.h"
#include "esp_err.h"

/**
 * @brief SPI GPIO配置结构体
 */
 typedef struct {
    gpio_num_t sda;     // MOSI/SDA引脚
    gpio_num_t scl;     // SCLK引脚
    gpio_num_t dc;      // DC引脚（数据/命令选择）
    gpio_num_t rst;     // RST引脚（复位），可选，-1表示不使用
    gpio_num_t cs;      // CS引脚（片选），可选，-1表示不使用
} lcd_spi_gpio_config_t;

/**
 * @brief 硬件SPI配置结构体
 */
typedef struct {
    uint8_t user_id;
#define LCD_SPI_MODE_HARDWARE    0
#define LCD_SPI_MODE_SIMULATION  1
    uint8_t mode;
    spi_host_device_t host_id;      // SPI主机ID (SPI2_HOST, SPI3_HOST等)
    int max_transfer_sz;            // 最大传输大小，字节
    uint32_t clock_speed_hz;        // SPI时钟频率
    lcd_spi_gpio_config_t gpio;     // LCD-SPI的GPIO配置
} lcd_spi_config_t;

/**
 * @brief 初始化SPI
 * 
 * @param configs SPI配置
 * @param num SPI数量
 * @return esp_err_t 
 */
esp_err_t lcd_spi_config_init(const lcd_spi_config_t *configs, int num);

/**
 * @brief SPI数据接口
 * 
 */
typedef struct 
{
    /// SPI总线ID
    uint8_t user_id;

}lcd_spi_data_t;


/// SPI 初始化
extern void lcd_ops_spi_init(const void *drv);
/// SPI写
extern void lcd_ops_spi_write_command(const void *drv, const uint8_t *data, uint16_t size);
/// SPI写数据
extern void lcd_ops_spi_write_dram_data(const void *drv, const uint8_t *data, uint16_t size);
/// SPI 复位
extern void lcd_ops_spi_reset(const void *drv);


///声明一个SPI的LCD驱动
#define LCD_DEFINE_DRIVER_SPI(_name, _user_id) \
static const lcd_spi_data_t s_lcd_data_##_name = {_user_id}; \
static const lcd_driver_ops_t s_lcd_driver_##_name = \
{ \
    .data = &s_lcd_data_##_name, \
    .init = lcd_ops_spi_init, \
    .write_command = lcd_ops_spi_write_command, \
    .write_dram_data = lcd_ops_spi_write_dram_data, \
    .reset = lcd_ops_spi_reset \
}

#ifdef __cplusplus
}
#endif

#endif // __LCD_DRIVER_SPI_H__
