# ESP32 公共组件库

这是一个为ESP32项目设计的公共组件库，提供了常用的硬件驱动、网络通信、显示控制等功能模块。所有组件都经过精心设计，具有良好的模块化结构和易用性。

## 📋 组件列表

### 🔧 系统组件

#### 1. app_event_loop - 应用事件循环
通用的事件循环组件，提供统一的事件管理机制。

**主要功能：**
- 事件循环初始化和配置
- 事件处理器注册和注销
- 事件发送和分发
- 支持自定义事件队列大小和任务优先级

**API接口：**
```c
esp_err_t app_event_loop_init(int queue_size, int task_priority);
esp_err_t app_event_handler_register(esp_event_base_t event_base, int32_t event_id, 
                                   esp_event_handler_t event_handler, void *event_handler_arg);
esp_err_t app_event_post(esp_event_base_t event_base, int32_t event_id, 
                        void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
```

#### 2. uptime - 系统时间抽象
提供系统时间相关的实用函数。

**主要功能：**
- 获取系统运行时间（tick）
- 微秒级和毫秒级延时函数
- 时间比较宏定义

**API接口：**
```c
sys_tick_t uptime(void);
void udelay(uint32_t us);
void mdelay(uint32_t ms);
```

### 🌐 网络通信组件

#### 3. wifi_station - WiFi客户端
功能完整的WiFi Station组件，支持自动连接和网络管理。

**主要功能：**
- NVS存储最多8个WiFi连接记录
- 自动连接到信号最强的已保存网络
- 后台网络扫描和自动重连
- 连接状态监控和事件回调
- 支持同步和异步网络扫描

**API接口：**
```c
esp_err_t wifi_station_init(wifi_station_event_callback_t event_callback, void *user_ctx);
esp_err_t wifi_station_connect(const char *ssid, const char *password);
esp_err_t wifi_station_get_status(wifi_connection_status_t *status);
esp_err_t wifi_station_scan_networks_async(wifi_network_info_t *networks, uint16_t *count, int timeout_ms);
```

#### 4. tcp_server - TCP服务器
独立的多客户端TCP服务器组件。

**主要功能：**
- 支持多客户端并发连接
- 非阻塞I/O操作
- 线程安全设计
- 回调函数机制处理客户端事件
- 支持单播和广播数据发送

**API接口：**
```c
tcp_server_handle_t tcp_server_create(const tcp_server_config_t *config, esp_err_t *err);
esp_err_t tcp_server_start(tcp_server_handle_t server_handle);
esp_err_t tcp_server_send_to_client(tcp_server_handle_t server_handle, tcp_client_t *client, 
                                   const uint8_t *data, size_t len);
esp_err_t tcp_server_broadcast(tcp_server_handle_t server_handle, const uint8_t *data, size_t len);
```

### 🔌 硬件接口组件

#### 5. bus_manager - 总线管理器
统一管理I2C和UART总线资源。

**主要功能：**
- I2C总线初始化和句柄管理
- UART硬件配置管理
- 支持多总线配置
- 资源自动分配和释放

**API接口：**
```c
esp_err_t i2c_bus_init(i2c_bus_t bus_id, const i2c_bus_config_t *config);
i2c_master_bus_handle_t i2c_bus_get_handle(i2c_bus_t bus_id);
esp_err_t uart_hw_config_add(uint8_t user_id, const uart_hw_config_t *config);
```

#### 6. ext_gpio - 扩展GPIO驱动
高级GPIO控制组件，支持LED闪烁和按键检测。

**主要功能：**
- GPIO逻辑索引管理
- LED闪烁控制（支持复杂闪烁模式）
- 按键检测和事件处理
- 可配置的GPIO数量限制

**API接口：**
```c
int ext_gpio_config(const ext_gpio_config_t *configs, int num);
int ext_gpio_set(uint16_t id, int value);
int ext_led_flash(uint16_t id, uint32_t control, uint32_t mask);
int ext_gpio_start(void);
```

### 🖥️ 显示组件

#### 7. lcd_display - LCD显示驱动
通用的LCD/OLED显示驱动组件。

**主要功能：**
- 支持多种LCD驱动和显示模型
- 字符和字符串显示
- 位图显示支持
- 基本图形绘制（线条、矩形）
- 支持屏幕旋转
- 部分显示优化

**API接口：**
```c
lcd_handle_t lcd_display_create(const lcd_driver_ops_t *driver, const lcd_model_t *model, 
                               lcd_rotation_t rotation, uint8_t *static_mem, uint32_t mem_size);
int lcd_display_string(lcd_handle_t disp, int x, int y, const char *text, 
                      const lcd_font_t *font, bool reverse);
int lcd_draw_rectangle(lcd_handle_t disp, int start_x, int start_y, int end_x, int end_y, 
                      int width, bool reverse);
```

#### 8. lcd_font - LCD字体库
提供多种尺寸的字体支持。

**支持的字体：**
- ASCII 8x8, 8x16, 10x18, 12x22, 16x32
- Acorn 8x8 字体
- Console Number 32x48 大号数字字体

**API接口：**
```c
// 字体声明宏
LCD_FONT_DECLARE(ascii_8x8);
LCD_FONT_DECLARE(ascii_8x16);
LCD_FONT_DECLARE(console_number_32x48);
```

### 🛠️ 工具组件

#### 9. misc_utils - 杂项工具
包含各种实用工具函数。

**主要功能：**
- 十六进制数据打印（hex_dump）
- 标准格式的数据调试输出
- 支持行前缀和ASCII字符显示

**API接口：**
```c
void hex_dump(const void *data, size_t len, const char *prefix);
```

## 🚀 快速开始

### 1. 集成到项目

将整个`components`目录复制到你的ESP32项目的`components`目录中。

### 2. 在CMakeLists.txt中添加依赖

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_event_loop wifi_station tcp_server lcd_display ext_gpio bus_manager misc_utils
)
```

### 3. 基本使用示例

```c
#include "wifi_station.h"
#include "tcp_server.h"
#include "lcd_display.h"
#include "ext_gpio.h"

void app_main(void)
{
    // 初始化NVS和事件循环
    nvs_flash_init();
    esp_event_loop_create_default();
    
    // 初始化WiFi
    wifi_station_init(NULL, NULL);
    
    // 配置TCP服务器
    tcp_server_config_t tcp_config = {
        .port = 8080,
        .max_clients = 5,
        .recv_callback = on_data_received,
        .user_ctx = NULL
    };
    
    tcp_server_handle_t server;
    esp_err_t ret = tcp_server_create(&tcp_config, &server);
    if (ret == ESP_OK) {
        tcp_server_start(server);
    }
    
    // 配置GPIO
    ext_gpio_config_t gpio_configs[] = {
        {.id = 0, .name = "LED1", .pin = GPIO_NUM_2, .mode = EXT_GPIO_MODE_OUTPUT},
        {.id = 1, .name = "BUTTON1", .pin = GPIO_NUM_0, .mode = EXT_GPIO_MODE_INPUT}
    };
    ext_gpio_config(gpio_configs, 2);
    ext_gpio_start();
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## ⚙️ 配置选项

### WiFi Station配置
```c
// 在sdkconfig.h或menuconfig中配置
#define CONFIG_WIFI_STATION_MAX_RECORDS 8
#define CONFIG_WIFI_STATION_SSID_LEN 64
#define CONFIG_WIFI_STATION_PASSWORD_LEN 64
```

### 扩展GPIO配置
```c
#define CONFIG_EXT_GPIO_MAX_NUM 8
#define CONFIG_EXT_BUTTON_MAX_NUM 2
#define CONFIG_EXT_GPIO_CACHE_SIZE 8
```

### 总线管理器配置
```c
#define CONFIG_BUS_MANAGER_UART_MAX_NUM 3
#define CONFIG_BUS_MANAGER_I2C_BUS_MAX_NUM 2
```

### LCD字体配置
```c
#define CONFIG_LCD_FONT_ASCII_8X8 1
#define CONFIG_LCD_FONT_ASCII_8X16 1
#define CONFIG_LCD_FONT_CONSOLE_NUMBER_32X48 0
```

## 📚 详细文档

每个组件都有独立的README文档，包含：
- 详细的API参考
- 使用示例和最佳实践
- 配置选项说明
- 错误处理指南

## 🔧 构建要求

- ESP-IDF 4.4 或更高版本
- ESP32 系列芯片
- FreeRTOS
- LWIP网络栈

## 📝 许可证

本组件库遵循与ESP-IDF相同的许可证。

## 🤝 贡献

欢迎提交Issue和Pull Request来改进这个组件库。

## 📞 联系方式

如有问题或建议，请联系：179712066@qq.com

---

**注意：** 使用前请确保已正确配置ESP-IDF环境，并根据实际硬件需求调整配置参数。
