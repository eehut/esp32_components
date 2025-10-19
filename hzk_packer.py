#!/usr/bin/env python3
"""
汉字库文件打包工具
用于在汉字库文件头部添加hzk_header_t结构
"""

import os
import sys
import struct
import argparse
import zlib
from typing import Tuple

# hzk_header_t 结构定义
class HzkHeader:
    """汉字库头信息结构"""
    
    # 魔数标识
    MAGIC = 0x314B5A48  # "HZK1" in little-endian
    
    def __init__(self, font_width: int, font_height: int, font_code_size: int, 
                 font_data_offset: int, font_data_size: int):
        """
        初始化汉字库头信息
        
        Args:
            font_width: 字体宽度
            font_height: 字体高度  
            font_code_size: 单个字符编码大小
            font_data_offset: 字体数据偏移量
            font_data_size: 字体数据大小
        """
        self.magic = self.MAGIC
        self.font_width = font_width
        self.font_height = font_height
        self.font_code_size = font_code_size
        self.font_data_offset = font_data_offset
        self.font_data_size = font_data_size
        self.font_data_checksum = 0
        self.reserved = [0] * 8  # 8个保留字段
        self.header_checksum = 0
    
    def calculate_data_checksum(self, data: bytes) -> int:
        """计算字体数据的CRC32校验和"""
        return zlib.crc32(data) & 0xFFFFFFFF
    
    def calculate_header_checksum(self) -> int:
        """计算头部的CRC32校验和"""
        # 将除了header_checksum字段外的所有字段打包
        header_data = struct.pack('<IIIIII', 
                                self.magic,
                                self.font_width,
                                self.font_height,
                                self.font_code_size,
                                self.font_data_offset,
                                self.font_data_size)
        
        # 添加font_data_checksum
        header_data += struct.pack('<I', self.font_data_checksum)
        
        # 添加reserved字段
        for reserved_val in self.reserved:
            header_data += struct.pack('<I', reserved_val)
        
        # 计算CRC32校验和
        return zlib.crc32(header_data) & 0xFFFFFFFF
    
    def serialize(self) -> bytes:
        """序列化为小端格式的字节数据"""
        # 计算校验和
        self.header_checksum = self.calculate_header_checksum()
        
        # 打包所有字段（小端格式）
        data = struct.pack('<IIIIII', 
                          self.magic,
                          self.font_width,
                          self.font_height,
                          self.font_code_size,
                          self.font_data_offset,
                          self.font_data_size)
        
        # 添加font_data_checksum（4字节）
        data += struct.pack('<I', self.font_data_checksum)
        
        # 添加reserved字段（8个uint32）
        for reserved_val in self.reserved:
            data += struct.pack('<I', reserved_val)
        
        # 添加header_checksum（4字节）
        data += struct.pack('<I', self.header_checksum)
        
        return data
    
    @classmethod
    def deserialize(cls, data: bytes) -> 'HzkHeader':
        """从字节数据反序列化"""
        if len(data) < 64:
            raise ValueError("Header data too short")
        
        # 解析基本字段
        magic, font_width, font_height, font_code_size, font_data_offset, font_data_size = \
            struct.unpack('<IIIIII', data[:24])
        
        if magic != cls.MAGIC:
            raise ValueError(f"Invalid magic number: 0x{magic:08X}")
        
        # 创建对象
        header = cls(font_width, font_height, font_code_size, font_data_offset, font_data_size)
        
        # 解析校验和字段
        header.font_data_checksum = struct.unpack('<I', data[24:28])[0]
        header.reserved = list(struct.unpack('<8I', data[28:60]))
        header.header_checksum = struct.unpack('<I', data[60:64])[0]
        
        return header
    
    def __str__(self) -> str:
        return f"""HzkHeader:
  Magic: 0x{self.magic:08X}
  Font Size: {self.font_width}x{self.font_height}
  Code Size: {self.font_code_size} bytes
  Data Offset: {self.font_data_offset}
  Data Size: {self.font_data_size} bytes
  Data Checksum: 0x{self.font_data_checksum:08X}
  Header Checksum: 0x{self.header_checksum:08X}"""


def pack_hzk_file(input_file: str, output_file: str, font_width: int, font_height: int) -> bool:
    """
    打包汉字库文件，在头部添加hzk_header_t结构
    
    Args:
        input_file: 输入汉字库文件路径
        output_file: 输出文件路径
        font_width: 字体宽度
        font_height: 字体高度
        
    Returns:
        bool: 是否成功
    """
    try:
        # 检查输入文件
        if not os.path.exists(input_file):
            print(f"Error: Input file '{input_file}' not found!")
            return False
        
        # 读取原始字体数据
        with open(input_file, 'rb') as f:
            font_data = f.read()
        
        if not font_data:
            print(f"Error: Input file '{input_file}' is empty!")
            return False
        
        # 计算字体参数
        font_data_size = len(font_data)
        font_data_offset = 64  # hzk_header_t 结构大小
        font_code_size = (font_width * font_height + 7) // 8  # 按位计算，不足8位算1字节
        
        print(f"Font data size: {font_data_size} bytes")
        print(f"Font code size: {font_code_size} bytes per character")
        print(f"Estimated character count: {font_data_size // font_code_size}")
        
        # 创建头部结构
        header = HzkHeader(font_width, font_height, font_code_size, 
                          font_data_offset, font_data_size)
        
        # 计算字体数据校验和
        header.font_data_checksum = header.calculate_data_checksum(font_data)
        
        # 序列化头部
        header_data = header.serialize()
        
        # 写入输出文件
        with open(output_file, 'wb') as f:
            f.write(header_data)  # 写入头部
            f.write(font_data)    # 写入字体数据
        
        print(f"Successfully packed font file:")
        print(f"  Input: {input_file} ({font_data_size} bytes)")
        print(f"  Output: {output_file} ({len(header_data) + font_data_size} bytes)")
        print(f"  Header size: {len(header_data)} bytes")
        print(header)
        
        return True
        
    except Exception as e:
        print(f"Error packing font file: {e}")
        return False


def dump_hzk_file(input_file: str) -> bool:
    """
    解析汉字库文件头部信息并检查校验和
    
    Args:
        input_file: 输入打包文件路径
        
    Returns:
        bool: 是否成功
    """
    try:
        # 检查输入文件
        if not os.path.exists(input_file):
            print(f"Error: Input file '{input_file}' not found!")
            return False
        
        # 读取文件
        with open(input_file, 'rb') as f:
            file_data = f.read()
        
        if len(file_data) < 64:
            print(f"Error: File too small to contain header! (got {len(file_data)} bytes)")
            return False
        
        # 解析头部
        header = HzkHeader.deserialize(file_data[:64])
        
        print(f"=== HZK File Header Analysis ===")
        print(f"File: {input_file}")
        print(f"File size: {len(file_data)} bytes")
        print()
        print(f"Header Information:")
        print(header)
        print()
        
        # 验证文件大小
        expected_file_size = header.font_data_offset + header.font_data_size
        if len(file_data) == expected_file_size:
            print(f"✓ File size is correct: {len(file_data)} bytes")
        else:
            print(f"✗ File size mismatch!")
            print(f"  Expected: {expected_file_size} bytes")
            print(f"  Actual: {len(file_data)} bytes")
        
        print()
        
        # 提取字体数据
        if len(file_data) >= header.font_data_offset + header.font_data_size:
            font_data = file_data[header.font_data_offset:header.font_data_offset + header.font_data_size]
            
            # 验证数据校验和
            calculated_data_checksum = header.calculate_data_checksum(font_data)
            print(f"Data Checksum Validation:")
            print(f"  Stored:    0x{header.font_data_checksum:08X}")
            print(f"  Calculated: 0x{calculated_data_checksum:08X}")
            
            if calculated_data_checksum == header.font_data_checksum:
                print(f"  ✓ Data checksum is correct")
            else:
                print(f"  ✗ Data checksum mismatch!")
            
            print()
            
            # 验证头部校验和
            calculated_header_checksum = header.calculate_header_checksum()
            print(f"Header Checksum Validation:")
            print(f"  Stored:    0x{header.header_checksum:08X}")
            print(f"  Calculated: 0x{calculated_header_checksum:08X}")
            
            if calculated_header_checksum == header.header_checksum:
                print(f"  ✓ Header checksum is correct")
            else:
                print(f"  ✗ Header checksum mismatch!")
            
            print()
            
            # 字体信息分析
            print(f"Font Information:")
            print(f"  Font size: {header.font_width}x{header.font_height} pixels")
            print(f"  Code size: {header.font_code_size} bytes per character")
            print(f"  Data size: {header.font_data_size} bytes")
            print(f"  Character count: {header.font_data_size // header.font_code_size}")
            
            # 检查字符数量是否合理
            if header.font_data_size % header.font_code_size == 0:
                print(f"  ✓ Character count is valid (no padding)")
            else:
                padding = header.font_data_size % header.font_code_size
                print(f"  ⚠ Warning: {padding} bytes of padding detected")
            
            print()
            
            # 总体状态
            data_ok = calculated_data_checksum == header.font_data_checksum
            header_ok = calculated_header_checksum == header.header_checksum
            size_ok = len(file_data) == expected_file_size
            
            if data_ok and header_ok and size_ok:
                print(f"=== Overall Status: ✓ VALID ===")
                return True
            else:
                print(f"=== Overall Status: ✗ INVALID ===")
                return False
        else:
            print(f"✗ File truncated! Cannot extract font data")
            return False
        
    except Exception as e:
        print(f"Error analyzing file: {e}")
        return False


def unpack_hzk_file(input_file: str, output_file: str) -> bool:
    """
    解包汉字库文件，提取字体数据
    
    Args:
        input_file: 输入打包文件路径
        output_file: 输出字体文件路径
        
    Returns:
        bool: 是否成功
    """
    try:
        # 检查输入文件
        if not os.path.exists(input_file):
            print(f"Error: Input file '{input_file}' not found!")
            return False
        
        # 读取文件
        with open(input_file, 'rb') as f:
            file_data = f.read()
        
        if len(file_data) < 64:
            print(f"Error: File too small to contain header!")
            return False
        
        # 解析头部
        header = HzkHeader.deserialize(file_data[:64])
        
        print(f"Parsed header:")
        print(header)
        
        # 验证数据大小
        if len(file_data) < header.font_data_offset + header.font_data_size:
            print(f"Error: File truncated! Expected {header.font_data_offset + header.font_data_size} bytes, got {len(file_data)}")
            return False
        
        # 提取字体数据
        font_data = file_data[header.font_data_offset:header.font_data_offset + header.font_data_size]
        
        # 验证数据校验和
        calculated_checksum = header.calculate_data_checksum(font_data)
        if calculated_checksum != header.font_data_checksum:
            print(f"Warning: Data checksum mismatch!")
            print(f"  Expected: 0x{header.font_data_checksum:08X}")
            print(f"  Calculated: 0x{calculated_checksum:08X}")
        
        # 写入输出文件
        with open(output_file, 'wb') as f:
            f.write(font_data)
        
        print(f"Successfully unpacked font file:")
        print(f"  Input: {input_file} ({len(file_data)} bytes)")
        print(f"  Output: {output_file} ({len(font_data)} bytes)")
        
        return True
        
    except Exception as e:
        print(f"Error unpacking font file: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description='HZK Font File Packer/Unpacker')
    parser.add_argument('--pack', '-p', action='store_true', help='Pack mode: add header to font file')
    parser.add_argument('--unpack', '-u', action='store_true', help='Unpack mode: extract font data')
    parser.add_argument('--dump', '-d', action='store_true', help='Dump mode: analyze header and verify checksums')
    parser.add_argument('--input', '-i', required=True, help='Input file path')
    parser.add_argument('--output', '-o', help='Output file path (not required for dump mode)')
    parser.add_argument('--width', '-W', type=int, help='Font width (for pack mode)')
    parser.add_argument('--height', '-H', type=int, help='Font height (for pack mode)')
    
    args = parser.parse_args()
    
    # 检查模式参数
    modes = [args.pack, args.unpack, args.dump]
    mode_count = sum(modes)
    
    if mode_count == 0:
        print("Error: Must specify one of --pack, --unpack, or --dump")
        sys.exit(1)
    elif mode_count > 1:
        print("Error: Cannot specify multiple modes at once")
        sys.exit(1)
    
    # 检查必需参数
    if args.pack:
        if not args.width or not args.height:
            print("Error: --width and --height are required for pack mode")
            sys.exit(1)
        if not args.output:
            print("Error: --output is required for pack mode")
            sys.exit(1)
    elif args.unpack:
        if not args.output:
            print("Error: --output is required for unpack mode")
            sys.exit(1)
    # dump mode doesn't need --output
    
    # 执行相应操作
    if args.pack:
        success = pack_hzk_file(args.input, args.output, args.width, args.height)
    elif args.unpack:
        success = unpack_hzk_file(args.input, args.output)
    else:  # dump
        success = dump_hzk_file(args.input)
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
