# 汉字库文件打包工具 (hzk_packer.py)

这个工具用于在汉字库文件头部添加 `hzk_header_t` 结构，支持打包、解包和分析操作。

## 功能特性

- ✅ 在汉字库文件头部添加 `hzk_header_t` 结构（64字节）
- ✅ 使用小端格式存储数据
- ✅ 支持数据校验和验证（CRC32）
- ✅ 支持头部校验和验证（CRC32）
- ✅ 支持打包、解包和分析操作
- ✅ 命令行接口，易于使用
- ✅ 完整的文件完整性验证

## hzk_header_t 结构

```c
typedef struct 
{
    uint32_t magic;              // 魔数标识 "HZK1" (0x314B5A48)
    uint32_t font_width;         // 字体宽度
    uint32_t font_height;        // 字体高度
    uint32_t font_code_size;     // 单个字符编码大小
    uint32_t font_data_offset;   // 字体数据偏移量 (固定64)
    uint32_t font_data_size;     // 字体数据大小
    uint32_t font_data_checksum; // 字体数据校验和 (CRC32)
    uint32_t reserved[8];        // 保留字段
    uint32_t header_checksum;    // 头部校验和 (CRC32)
} hzk_header_t;
```

## 使用方法

### 打包模式

将原始汉字库文件打包，在头部添加 `hzk_header_t` 结构：

```bash
python3 hzk_packer.py --pack --input <原始字体文件> --output <打包文件> --width <字体宽度> --height <字体高度>
```

**示例：**
```bash
# 打包16x16字体文件
python3 components/hzk_packer.py --pack --input font_16x16.bin --output font_16x16.hzk --width 16 --height 16

# 打包24x24字体文件  
python3 components/hzk_packer.py --pack --input font_24x24.bin --output font_24x24.hzk --width 24 --height 24
```

### 解包模式

从打包文件中提取原始字体数据：

```bash
python3 hzk_packer.py --unpack --input <打包文件> --output <原始字体文件>
```

**示例：**
```bash
python3 components/hzk_packer.py --unpack --input font_16x16.hzk --output font_16x16.bin
```

### 分析模式

分析汉字库文件头部信息并验证校验和：

```bash
python3 components/hzk_packer.py --dump --input <打包文件>
```

**示例：**
```bash
python3 components/hzk_packer.py --dump --input font_16x16.hzk
```

**输出示例：**
```
=== HZK File Header Analysis ===
File: font_16x16.hzk
File size: 160 bytes

Header Information:
HzkHeader:
  Magic: 0x314B5A48
  Font Size: 16x16
  Code Size: 32 bytes
  Data Offset: 64
  Data Size: 96 bytes
  Data Checksum: 0xF00C4CD8
  Header Checksum: 0x3D218146

✓ File size is correct: 160 bytes

Data Checksum Validation:
  Stored:    0xF00C4CD8
  Calculated: 0xF00C4CD8
  ✓ Data checksum is correct

Header Checksum Validation:
  Stored:    0x3D218146
  Calculated: 0x3D218146
  ✓ Header checksum is correct

Font Information:
  Font size: 16x16 pixels
  Code size: 32 bytes per character
  Data size: 96 bytes
  Character count: 3
  ✓ Character count is valid (no padding)

=== Overall Status: ✓ VALID ===
```

## 参数说明

- `--pack, -p`: 打包模式
- `--unpack, -u`: 解包模式
- `--dump, -d`: 分析模式（不需要输出文件）
- `--input, -i`: 输入文件路径
- `--output, -o`: 输出文件路径（分析模式不需要）
- `--width, -W`: 字体宽度（仅打包模式需要）
- `--height, -H`: 字体高度（仅打包模式需要）

## 文件格式

### 打包文件结构

```
[64字节 hzk_header_t] + [原始字体数据]
```

### 字体数据计算

- `font_code_size = (font_width * font_height + 7) // 8`
- `font_data_offset = 64` (固定值)
- `font_data_size = 原始文件大小`

## 校验和机制

1. **数据校验和**: 对原始字体数据计算CRC32
2. **头部校验和**: 对头部除 `header_checksum` 外的所有字段计算CRC32

### CRC32优势

- **MCU友好**: CRC32算法简单，适合在微控制器中实现
- **计算效率高**: 比MD5计算更快，资源消耗更少
- **标准算法**: CRC32是广泛使用的标准校验算法
- **32位整数**: 直接存储为32位无符号整数，便于处理

## 测试示例

工具包含测试功能，可以创建测试字体文件：

```bash
# 创建测试字体文件
python3 create_test_font.py

# 打包测试文件
python3 components/hzk_packer.py --pack --input test_font_16x16.bin --output test_font_packed.hzk --width 16 --height 16

# 分析打包文件
python3 components/hzk_packer.py --dump --input test_font_packed.hzk

# 解包测试文件
python3 components/hzk_packer.py --unpack --input test_font_packed.hzk --output test_font_unpacked.bin

# 验证文件一致性
diff test_font_16x16.bin test_font_unpacked.bin
```

## 错误处理

- 输入文件不存在或为空
- 文件大小不匹配
- 魔数验证失败
- CRC32校验和不匹配
- 文件截断检测
- 参数错误验证

## 注意事项

1. 字体数据大小必须是 `font_code_size` 的整数倍
2. 打包后的文件大小 = 64字节 + 原始文件大小
3. 所有数据使用小端格式存储
4. CRC32校验和不匹配时会显示错误信息
5. 分析模式不需要指定输出文件
6. 支持三种操作模式：打包、解包、分析

## 使用场景

- **开发阶段**: 使用 `--dump` 分析字体文件完整性
- **生产部署**: 使用 `--pack` 打包字体文件
- **调试恢复**: 使用 `--unpack` 提取原始字体数据
- **质量保证**: 验证CRC32校验和确保数据完整性


## 烧录到ESP32

假设分区表如下所示：

```sh
# Name,     Type, SubType, Offset,   Size,    Flags
nvs,        data, nvs,     0x9000,   0x6000,
otadata,    data, ota,     0xf000,   0x2000,
phy_init,   data, phy,     0x11000,  0x1000,
factory,    app,  factory, 0x20000,  0x120000,
ota_0,      app,  ota_0,   0x140000, 0x120000,
ota_1,      app,  ota_1,   0x260000, 0x120000,
hzk16,      data, 0x40,    0x380000, 0x60000,
```

现在需要将做好的字体文件`hzk16.bin`写入到分区`hzk16`, 命令如下：

```sh
# 指定芯片类型及接口
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x380000 hzk16.bin

# 简单方式
esptool.py write_flash 0x380000 hzk16.bin
```
