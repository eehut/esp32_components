/**
 * @file lcd_driver_spi.c
 * @author LiuChuansen (1797120666@qq.com)
 * @brief LCD底层驱动实现 - SPI
 * @version 0.1
 * @date 2025-10-02
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "lcd_driver_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "uptime.h"

#include <stdint.h>

static const char *TAG = "lcd-spi";

/// SPI 初始化
void lcd_ops_gpio_spi_init(const void *drv)
{
    const lcd_gpio_spi_data_t *gpio_spi = (const lcd_gpio_spi_data_t *)drv;
    bool has_cs = gpio_spi->cs >= 0;
    bool has_rst = gpio_spi->rst >= 0;

    // sda, scl, dc, 这三个引脚不能为无效引脚
    if (gpio_spi->sda < 0 || gpio_spi->scl < 0 || gpio_spi->dc < 0) {
        ESP_LOGE(TAG, "Invalid SPI pins: sda=%d, scl=%d, dc=%d", gpio_spi->sda, gpio_spi->scl, gpio_spi->dc);
        return;
    }
    
    gpio_config_t cfg = {0};

    cfg.pin_bit_mask = (1ULL << gpio_spi->sda) | (1ULL << gpio_spi->scl) | (1ULL << gpio_spi->dc);

    if (has_cs) {
        cfg.pin_bit_mask |= (1ULL << gpio_spi->cs);
    }

    if (has_rst) {
        cfg.pin_bit_mask |= (1ULL << gpio_spi->rst);
    }
    
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;

    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "gpio-spi driver init success");
}


/// SPI写
static inline void lcd_ops_gpio_spi_write(const lcd_gpio_spi_data_t *spi, bool cmd, uint8_t data)
{
    // 决定写的是命令还是数据 
    gpio_set_level(spi->dc, cmd ? 0 : 1);

    for (int i = 7; i >= 0; i --)
    {
        gpio_set_level(spi->sda, (data >> i) & 0x01);
        gpio_set_level(spi->scl, 0);
        udelay(1);
        gpio_set_level(spi->scl, 1);
        udelay(1);
    }
}

void lcd_ops_gpio_spi_write_command(const void *drv, const uint8_t *data, uint16_t size)
{
    const lcd_gpio_spi_data_t *spi = (const lcd_gpio_spi_data_t *)drv;
    if (spi->cs >= 0)
    {
        gpio_set_level(spi->cs, 0);
    }

    for (int i = 0; i < size; i++)
    {
        lcd_ops_gpio_spi_write(spi, true, data[i]);
    }

    if (spi->cs >= 0)
    {
        gpio_set_level(spi->cs, 1);
    }
}

void lcd_ops_gpio_spi_write_dram_data(const void *drv, const uint8_t *data, uint16_t size)
{
    const lcd_gpio_spi_data_t *spi = (const lcd_gpio_spi_data_t *)drv;

    if (spi->cs >= 0)
    {
        gpio_set_level(spi->cs, 0);
    }

    for (int i = 0; i < size; i++)
    {
        lcd_ops_gpio_spi_write(spi, false, data[i]);
    }

    if (spi->cs >= 0)
    {
        gpio_set_level(spi->cs, 1);
    }
}

/// SPI 复位
void lcd_ops_gpio_spi_reset(const void *drv)
{
    const lcd_gpio_spi_data_t *spi = (const lcd_gpio_spi_data_t *)drv;

    if (spi->rst >= 0)
    {
        gpio_set_level(spi->rst, 1);
        mdelay(10);
        gpio_set_level(spi->rst, 0);
        mdelay(100);
        gpio_set_level(spi->rst, 1);
        mdelay(10);
    }

    if (spi->cs >= 0)
    {
        // 拉高CS，释放SPI总线
        gpio_set_level(spi->cs, 1);
    }

    gpio_set_level(spi->dc, 0);
    gpio_set_level(spi->scl, 1);// 时钟是高电平开始， 参考SH1108的时序图，有可能不同的型号不一样。
    gpio_set_level(spi->sda, 0);
}
