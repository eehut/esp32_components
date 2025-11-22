# LCD字体组件使用说明

本组件提供了多种ASCII字体和汉字库支持，可在应用工程中按需启用。

## 第一部分：使用ASCII字体

组件提供了多种ASCII字体，默认情况下已启用 `ascii_8x8` 和 `ascii_8x16` 两种基础字体。如需启用其他字体，需要在应用的 `CMakeLists.txt` 中设置相应的编译宏。

### 可用的ASCII字体

| 字体名称 | 尺寸 | 配置宏 | 默认状态 |
|---------|------|--------|---------|
| ascii_8x8 | 8×8 | `CONFIG_LCD_FONT_ASCII_8X8` | ✅ 已启用 |
| ascii_8x16 | 8×16 | `CONFIG_LCD_FONT_ASCII_8X16` | ✅ 已启用 |
| ascii_10x18 | 10×18 | `CONFIG_LCD_FONT_ASCII_10X18` | ❌ 未启用 |
| sun_ascii_12x22 | 12×22 | `CONFIG_LCD_FONT_SUN_ASCII_12X22` | ❌ 未启用 |
| ter_ascii_16x32 | 16×32 | `CONFIG_LCD_FONT_TER_ASCII_16X32` | ❌ 未启用 |
| acorn_ascii_8x8 | 8×8 | `CONFIG_LCD_FONT_ACORN_ASCII_8X8` | ❌ 未启用 |
| console_number_32x48 | 32×48 | `CONFIG_LCD_FONT_CONSOLE_NUMBER_32X48` | ❌ 未启用 |

### 在应用中启用ASCII字体

在应用的 `CMakeLists.txt` 中，使用 `target_compile_definitions` 为 `lcd_font` 组件设置编译宏：

```cmake
# 获取lcd_font组件的库目标
idf_component_get_property(lcd_font lcd_font COMPONENT_LIB)

# 启用所需的字体
target_compile_definitions(${lcd_font} PRIVATE
    CONFIG_LCD_FONT_ASCII_10X18=1
    CONFIG_LCD_FONT_SUN_ASCII_12X22=1
    CONFIG_LCD_FONT_TER_ASCII_16X32=1
    CONFIG_LCD_FONT_ACORN_ASCII_8X8=1
    CONFIG_LCD_FONT_CONSOLE_NUMBER_32X48=1
)
```

## 第二部分：使用汉字库

组件支持从Flash分区加载汉字库，支持16×16和24×24两种尺寸的HZK字体。

### 烧录汉字库到Flash

汉字库需要烧录到Flash的指定地址。使用 `esptool.py` 工具进行烧录：

```bash
esptool.py write_flash 0x380000 hzk16.bin
```

**注意：**
- 烧录地址 `0x380000` 需要与分区表中的 `hzk16` 分区起始地址一致
- 确保分区表中有对应的 `hzk16` 或 `hzk24` 分区
- 汉字库文件需要符合组件要求的格式（包含header信息）

### 启用汉字库

**默认情况下，汉字库功能未启用**，需要在应用中显式指定编译宏来启用。

在应用的 `CMakeLists.txt` 中设置编译宏：

```cmake
# 获取lcd_font组件的库目标
idf_component_get_property(lcd_font lcd_font COMPONENT_LIB)

# 启用HZK16汉字库
target_compile_definitions(${lcd_font} PRIVATE
    CONFIG_LCD_FONT_HZK_16=1
    CONFIG_GB2312_ENCODE_HZ3500=1
)
```

### 配置说明

- **`CONFIG_LCD_FONT_HZK_16=1`**: 启用16×16汉字库
- **`CONFIG_LCD_FONT_HZK_24=1`**: 启用24×24汉字库（如果使用24×24字体）
- **`CONFIG_GB2312_ENCODE_HZ3500=1`**: 选择GB2312编码表
  - 设置为 `1` 时使用 `hz3500` 编码表（支持3500个常用汉字）
  - 不设置或设置为 `0` 时使用 `hz2000` 编码表（支持2000个常用汉字）

### 初始化汉字库

在应用启动时，需要调用 `lcd_font_init()` 函数来初始化汉字库：

```c
#include "lcd_fonts.h"

void app_main(void)
{
    // ... 其他初始化代码 ...
    
    // 初始化字体（包括汉字库）
    lcd_font_init();
    
    // ... 其他代码 ...
}
```

### 完整示例

以下是一个完整的 `CMakeLists.txt` 示例，展示了如何同时启用ASCII字体和汉字库：

```cmake
idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "include"
    REQUIRES lcd_font
)

# 为lcd_font组件设置编译宏
idf_component_get_property(lcd_font lcd_font COMPONENT_LIB)
target_compile_definitions(${lcd_font} PRIVATE
    # 启用ASCII字体
    CONFIG_LCD_FONT_ASCII_10X18=1
    CONFIG_LCD_FONT_CONSOLE_NUMBER_32X48=1
    # 启用汉字库
    CONFIG_LCD_FONT_HZK_16=1
    CONFIG_GB2312_ENCODE_HZ3500=1
)
```

