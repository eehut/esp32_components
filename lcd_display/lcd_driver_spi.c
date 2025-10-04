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
#include <string.h>

static const char *TAG = "lcd-spi";

// 定义最大支持的LCD实例数
#ifndef CONFIG_LCD_MAX_SPI_DRIVER_NUM
#define CONFIG_LCD_MAX_SPI_DRIVER_NUM  1
#endif

/// SPI设备句柄结构体
typedef struct {
    uint8_t user_id;
    spi_device_handle_t handle;
    const lcd_spi_config_t *config;
    bool in_use;
}lcd_spi_device_t;

static lcd_spi_device_t s_lcd_spi_devices[CONFIG_LCD_MAX_SPI_DRIVER_NUM] = {0};

/**
 * @brief 根据user_id查找SPI设备
 * 
 * @param user_id 用户ID
 * @return lcd_spi_device_t* 设备指针，未找到返回NULL
 */
static lcd_spi_device_t *find_spi_device_by_id(uint8_t user_id)
{
    for (int i = 0; i < CONFIG_LCD_MAX_SPI_DRIVER_NUM; i++) {
        if (s_lcd_spi_devices[i].in_use && s_lcd_spi_devices[i].user_id == user_id) {
            return &s_lcd_spi_devices[i];
        }
    }
    return NULL;
}

/**
 * @brief 初始化GPIO模拟SPI的引脚
 * 
 * @param gpio GPIO配置
 * @return esp_err_t 
 */
static esp_err_t init_gpio_spi_pins(const lcd_spi_gpio_config_t *gpio)
{
    bool has_cs = gpio->cs >= 0;
    bool has_rst = gpio->rst >= 0;

    // sda, scl, dc, 这三个引脚不能为无效引脚
    if (gpio->sda < 0 || gpio->scl < 0 || gpio->dc < 0) {
        ESP_LOGE(TAG, "Invalid GPIO SPI pins: sda=%d, scl=%d, dc=%d", gpio->sda, gpio->scl, gpio->dc);
        return ESP_ERR_INVALID_ARG;
    }
    
    gpio_config_t cfg = {0};

    cfg.pin_bit_mask = (1ULL << gpio->sda) | (1ULL << gpio->scl) | (1ULL << gpio->dc);

    if (has_cs) {
        cfg.pin_bit_mask |= (1ULL << gpio->cs);
    }

    if (has_rst) {
        cfg.pin_bit_mask |= (1ULL << gpio->rst);
    }
    
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;

    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化引脚状态
    gpio_set_level(gpio->scl, 1);
    gpio_set_level(gpio->dc, 0);
    gpio_set_level(gpio->sda, 0);
    if (has_cs) {
        gpio_set_level(gpio->cs, 1);
    }
    if (has_rst) {
        gpio_set_level(gpio->rst, 1);
    }
    
    ESP_LOGI(TAG, "GPIO SPI pins initialized: sda=%d, scl=%d, dc=%d, cs=%d, rst=%d", 
             gpio->sda, gpio->scl, gpio->dc, gpio->cs, gpio->rst);
    
    return ESP_OK;
}

/**
 * @brief 初始化硬件SPI
 * 
 * @param config SPI配置
 * @param device 设备句柄
 * @return esp_err_t 
 */
static esp_err_t init_hardware_spi(const lcd_spi_config_t *config, lcd_spi_device_t *device)
{
    esp_err_t ret;
    
    // 初始化SPI总线
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config->gpio.sda,
        .miso_io_num = -1,
        .sclk_io_num = config->gpio.scl,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->max_transfer_sz > 0 ? config->max_transfer_sz : 4096,
    };

    ret = spi_bus_initialize(config->host_id, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置SPI设备
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = config->clock_speed_hz > 0 ? config->clock_speed_hz : 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = config->gpio.cs,
        .queue_size = 7,
        .flags = 0,
        .pre_cb = NULL,
    };

    ret = spi_bus_add_device(config->host_id, &dev_cfg, &device->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus add device failed: %s", esp_err_to_name(ret));
        spi_bus_free(config->host_id);
        return ret;
    }

    // 初始化DC和RST引脚（这些需要单独控制）
    if (config->gpio.dc < 0) {
        ESP_LOGE(TAG, "DC pin is required for hardware SPI");
        spi_bus_remove_device(device->handle);
        spi_bus_free(config->host_id);
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->gpio.dc),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    if (config->gpio.rst >= 0) {
        io_conf.pin_bit_mask |= (1ULL << config->gpio.rst);
    }

    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DC/RST gpio_config failed: %s", esp_err_to_name(ret));
        spi_bus_remove_device(device->handle);
        spi_bus_free(config->host_id);
        return ret;
    }

    // 初始化引脚状态
    gpio_set_level(config->gpio.dc, 0);
    if (config->gpio.rst >= 0) {
        gpio_set_level(config->gpio.rst, 1);
    }

    ESP_LOGI(TAG, "Hardware SPI initialized: host=%d, freq=%d Hz, dc=%d, rst=%d, cs=%d", 
             config->host_id, config->clock_speed_hz, config->gpio.dc, config->gpio.rst, config->gpio.cs);

    return ESP_OK;
}

/**
 * @brief 初始化SPI配置
 * 
 * @param configs SPI配置
 * @param num SPI数量
 * @return esp_err_t 
 */
esp_err_t lcd_spi_config_init(const lcd_spi_config_t *configs, int num)
{
    if (configs == NULL || num <= 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (num > CONFIG_LCD_MAX_SPI_DRIVER_NUM) {
        ESP_LOGE(TAG, "Too many SPI configs: %d, max: %d", num, CONFIG_LCD_MAX_SPI_DRIVER_NUM);
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < num; i++) {
        const lcd_spi_config_t *cfg = &configs[i];
        
        // 查找可用的设备槽位
        lcd_spi_device_t *device = NULL;
        for (int j = 0; j < CONFIG_LCD_MAX_SPI_DRIVER_NUM; j++) {
            if (!s_lcd_spi_devices[j].in_use) {
                device = &s_lcd_spi_devices[j];
                break;
            }
        }

        if (device == NULL) {
            ESP_LOGE(TAG, "No available device slot for config[%d]", i);
            return ESP_ERR_NO_MEM;
        }

        // 初始化设备
        device->user_id = cfg->user_id;
        device->config = cfg;
        device->handle = NULL;
        device->in_use = true;

        esp_err_t ret = ESP_OK;

        if (cfg->mode == LCD_SPI_MODE_SIMULATION) {
            // GPIO模拟SPI
            ret = init_gpio_spi_pins(&cfg->gpio);
            if (ret != ESP_OK) {
                device->in_use = false;
                return ret;
            }
            ESP_LOGI(TAG, "Config[%d]: GPIO simulation SPI initialized, user_id=%d", i, cfg->user_id);
        } else if (cfg->mode == LCD_SPI_MODE_HARDWARE) {
            // 硬件SPI
            ret = init_hardware_spi(cfg, device);
            if (ret != ESP_OK) {
                device->in_use = false;
                return ret;
            }
            ESP_LOGI(TAG, "Config[%d]: Hardware SPI initialized, user_id=%d", i, cfg->user_id);
        } else {
            ESP_LOGE(TAG, "Unknown SPI mode: %d", cfg->mode);
            device->in_use = false;
            return ESP_ERR_INVALID_ARG;
        }
    }

    ESP_LOGI(TAG, "Initialized %d SPI device(s)", num);
    return ESP_OK;
}


//=============================================================================
// GPIO模拟SPI的静态私有函数
//=============================================================================

/**
 * @brief GPIO模拟SPI - 写一个字节
 * 
 * @param gpio GPIO配置
 * @param cmd true为命令，false为数据
 * @param data 要写入的字节
 */
static inline void gpio_spi_write_byte(const lcd_spi_gpio_config_t *gpio, bool cmd, uint8_t data)
{
    // 决定写的是命令还是数据 
    gpio_set_level(gpio->dc, cmd ? 0 : 1);

    for (int i = 7; i >= 0; i--) {
        gpio_set_level(gpio->sda, (data >> i) & 0x01);
        gpio_set_level(gpio->scl, 0);
        udelay(1);
        gpio_set_level(gpio->scl, 1);
        udelay(1);
    }
}

/**
 * @brief GPIO模拟SPI - 写命令
 * 
 * @param gpio GPIO配置
 * @param data 命令数据
 * @param size 数据大小
 */
static void gpio_spi_write_command(const lcd_spi_gpio_config_t *gpio, const uint8_t *data, uint16_t size)
{
    if (gpio->cs >= 0) {
        gpio_set_level(gpio->cs, 0);
    }

    for (int i = 0; i < size; i++) {
        gpio_spi_write_byte(gpio, true, data[i]);
    }

    if (gpio->cs >= 0) {
        gpio_set_level(gpio->cs, 1);
    }
}

/**
 * @brief GPIO模拟SPI - 写数据
 * 
 * @param gpio GPIO配置
 * @param data 显存数据
 * @param size 数据大小
 */
static void gpio_spi_write_dram_data(const lcd_spi_gpio_config_t *gpio, const uint8_t *data, uint16_t size)
{
    if (gpio->cs >= 0) {
        gpio_set_level(gpio->cs, 0);
    }

    for (int i = 0; i < size; i++) {
        gpio_spi_write_byte(gpio, false, data[i]);
    }

    if (gpio->cs >= 0) {
        gpio_set_level(gpio->cs, 1);
    }
}

/**
 * @brief GPIO模拟SPI - 复位
 * 
 * @param gpio GPIO配置
 */
static void gpio_spi_reset(const lcd_spi_gpio_config_t *gpio)
{
    if (gpio->rst >= 0) {
        gpio_set_level(gpio->rst, 1);
        mdelay(10);
        gpio_set_level(gpio->rst, 0);
        mdelay(100);
        gpio_set_level(gpio->rst, 1);
        mdelay(10);
    }

    if (gpio->cs >= 0) {
        // 拉高CS，释放SPI总线
        gpio_set_level(gpio->cs, 1);
    }

    gpio_set_level(gpio->dc, 0);
    gpio_set_level(gpio->scl, 1);  // 时钟是高电平开始
    gpio_set_level(gpio->sda, 0);
}

//=============================================================================
// 硬件SPI的静态私有函数
//=============================================================================

/**
 * @brief 硬件SPI - 写命令
 * 
 * @param device SPI设备
 * @param data 命令数据
 * @param size 数据大小
 */
static void hw_spi_write_command(lcd_spi_device_t *device, const uint8_t *data, uint16_t size)
{
    if (device->handle == NULL) {
        ESP_LOGE(TAG, "Hardware SPI handle is NULL");
        return;
    }

    // DC引脚设置为低，表示命令
    gpio_set_level(device->config->gpio.dc, 0);

    spi_transaction_t trans = {
        .length = size * 8,
        .tx_buffer = data,
    };

    esp_err_t ret = spi_device_polling_transmit(device->handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware SPI write command failed: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 硬件SPI - 写数据
 * 
 * @param device SPI设备
 * @param data 显存数据
 * @param size 数据大小
 */
static void hw_spi_write_dram_data(lcd_spi_device_t *device, const uint8_t *data, uint16_t size)
{
    if (device->handle == NULL) {
        ESP_LOGE(TAG, "Hardware SPI handle is NULL");
        return;
    }

    // DC引脚设置为高，表示数据
    gpio_set_level(device->config->gpio.dc, 1);

    spi_transaction_t trans = {
        .length = size * 8,
        .tx_buffer = data,
    };

    esp_err_t ret = spi_device_polling_transmit(device->handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware SPI write data failed: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 硬件SPI - 复位
 * 
 * @param device SPI设备
 */
static void hw_spi_reset(lcd_spi_device_t *device)
{
    const lcd_spi_gpio_config_t *gpio = &device->config->gpio;
    
    if (gpio->rst >= 0) {
        gpio_set_level(gpio->rst, 1);
        mdelay(10);
        gpio_set_level(gpio->rst, 0);
        mdelay(100);
        gpio_set_level(gpio->rst, 1);
        mdelay(10);
    }

    gpio_set_level(gpio->dc, 0);
}

//=============================================================================
// 统一的OPS接口函数
//=============================================================================

/**
 * @brief SPI初始化（统一接口）
 * 
 * @param drv lcd_spi_data_t指针
 */
void lcd_ops_spi_init(const void *drv)
{
    const lcd_spi_data_t *spi_data = (const lcd_spi_data_t *)drv;
    lcd_spi_device_t *device = find_spi_device_by_id(spi_data->user_id);
    
    if (device == NULL) {
        ESP_LOGE(TAG, "SPI device not found, user_id=%d", spi_data->user_id);
        return;
    }

    ESP_LOGI(TAG, "SPI init, user_id=%d, mode=%s", 
             spi_data->user_id, 
             device->config->mode == LCD_SPI_MODE_HARDWARE ? "Hardware" : "GPIO Simulation");
}

/**
 * @brief SPI写命令（统一接口）
 * 
 * @param drv lcd_spi_data_t指针
 * @param data 命令数据
 * @param size 数据大小
 */
void lcd_ops_spi_write_command(const void *drv, const uint8_t *data, uint16_t size)
{
    const lcd_spi_data_t *spi_data = (const lcd_spi_data_t *)drv;
    lcd_spi_device_t *device = find_spi_device_by_id(spi_data->user_id);
    
    if (device == NULL) {
        ESP_LOGE(TAG, "SPI device not found, user_id=%d", spi_data->user_id);
        return;
    }

    if (device->config->mode == LCD_SPI_MODE_HARDWARE) {
        hw_spi_write_command(device, data, size);
    } else {
        gpio_spi_write_command(&device->config->gpio, data, size);
    }
}

/**
 * @brief SPI写数据（统一接口）
 * 
 * @param drv lcd_spi_data_t指针
 * @param data 显存数据
 * @param size 数据大小
 */
void lcd_ops_spi_write_dram_data(const void *drv, const uint8_t *data, uint16_t size)
{
    const lcd_spi_data_t *spi_data = (const lcd_spi_data_t *)drv;
    lcd_spi_device_t *device = find_spi_device_by_id(spi_data->user_id);
    
    if (device == NULL) {
        ESP_LOGE(TAG, "SPI device not found, user_id=%d", spi_data->user_id);
        return;
    }

    if (device->config->mode == LCD_SPI_MODE_HARDWARE) {
        hw_spi_write_dram_data(device, data, size);
    } else {
        gpio_spi_write_dram_data(&device->config->gpio, data, size);
    }
}

/**
 * @brief SPI复位（统一接口）
 * 
 * @param drv lcd_spi_data_t指针
 */
void lcd_ops_spi_reset(const void *drv)
{
    const lcd_spi_data_t *spi_data = (const lcd_spi_data_t *)drv;
    lcd_spi_device_t *device = find_spi_device_by_id(spi_data->user_id);
    
    if (device == NULL) {
        ESP_LOGE(TAG, "SPI device not found, user_id=%d", spi_data->user_id);
        return;
    }

    if (device->config->mode == LCD_SPI_MODE_HARDWARE) {
        hw_spi_reset(device);
    } else {
        gpio_spi_reset(&device->config->gpio);
    }
}
