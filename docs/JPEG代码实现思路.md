# JPEG 代码实现思路

本文结合当前 `jpeg/jpg.c`、`jpeg/jpg.h`、`jpeg/bmp_reader.c`、两个命令行入口及测试代码，说明项目如何把 RGB 像素编码为 4:2:0 Baseline JPEG。

重点不只是介绍 JPEG 标准，而是把标准概念对应到当前函数、数组、循环、文件字段和内存所有权。阅读后应能沿代码追踪一个 RGB 像素如何进入 MCU，并最终变成 JPEG 熵编码字节。

## 1. 模块目标与边界

当前 JPEG 部分实现的是一个独立的编码器，不依赖 libjpeg 等第三方库。编码器接收从上到下排列的 24 位 RGB 内存，输出可被常规图片查看器读取的 JFIF Baseline JPEG 文件。

模块分成“输入适配”和“JPEG 编码”两层：

```text
24 位未压缩 BMP
  ↓ bmp_read_rgb()
顶行优先 RGB 缓冲区
  ↓ jpeg_generate()
4:2:0 Baseline JPEG
```

根目录字体程序不需要先生成 BMP 再读取。字体绘制层已经持有相同布局的 RGB 画布，因此可以直接调用 `jpeg_generate()`：

```text
TTF 字形轮廓
  ↓ font_renderer_render()
顶行优先 RGB 画布
  ├─bmp_generate()  → BMP
  └─jpeg_generate() → JPEG
```

`bmp_reader.c` 只负责 BMP 文件格式适配，不参与 JPEG 压缩。`jpg.c` 只接受规范化 RGB 数据，不知道像素来自 BMP、字体渲染器还是其他图像来源。

## 2. 相关文件及职责

| 文件 | 职责 |
| --- | --- |
| `jpeg/jpg.h` | 声明质量范围和唯一公共编码接口 `jpeg_generate()` |
| `jpeg/jpg.c` | 写入 JPEG 文件段，完成 4:2:0、DCT、量化、Zigzag 与 Huffman 编码 |
| `jpeg/bmp_reader.h` | 定义 BMP 读取结果 `bmp_rgb_image` 及其释放接口 |
| `jpeg/bmp_reader.c` | 读取未压缩 24 位 BMP，处理行方向、补齐与 BGR→RGB |
| `jpeg/main.c` | 独立 BMP→JPEG 命令行程序 |
| `main.c` | 字体渲染总入口，可直接把 RGB 画布输出为 BMP、JPEG 或两者 |
| `tests/test_jpeg.c` | 验证 JPEG 标记、尺寸、4:2:0 采样和各种边界尺寸 |
| `tests/test_bmp_reader.c` | 验证 BMP 行补齐、上下方向及 BGR→RGB |

## 3. 三种使用路径

### 3.1 直接调用编码器

`jpg.h` 暴露的接口为：

```c
int jpeg_generate(
	const char* file,
	const u8* color_data,
	int width,
	int height,
	int quality
);
```

| 参数 | 含义 |
| --- | --- |
| `file` | 输出 JPEG 路径，必须是非空字符串 |
| `color_data` | RGB 连续缓冲区，由调用者持有 |
| `width`、`height` | 图像像素尺寸，范围为 1～65535 |
| `quality` | 质量参数，范围为 1～100 |
| 返回值 | 成功返回 0，失败返回 -1 |

每个像素固定占三个字节，排列为 R、G、B。像素行从图像顶部开始，位置计算方式为：

```c
position = ((size_t)y*width+x)*3;
red   = color_data[position];
green = color_data[position+1];
blue  = color_data[position+2];
```

编码器只读取 `color_data`，不会修改或释放它。函数返回后，缓冲区仍由调用者管理。

### 3.2 使用 `jpeg/main.c` 转换 BMP

独立入口的命令格式为：

```text
main <input.bmp> [output.jpg] [quality]
```

输出路径缺省为 `out.jpg`，质量缺省为 90。`parse_quality()` 使用 `strtol()` 解析完整十进制字符串，并拒绝越界、尾随字符及转换溢出。

实际调用链为：

```text
main()
  ├─parse_quality()
  ├─bmp_read_rgb()
  ├─jpeg_generate()
  └─bmp_rgb_image_free()
```

### 3.3 使用根目录字体程序

根目录 `main.c` 先把文字渲染到 RGB 画布，再由 `write_outputs()` 根据输出模式调用 `bmp_generate()`、`jpeg_generate()` 或两者。

JPEG 路径直接使用内存中的 `color_data`：

```c
jpeg_generate(
	options->jpeg_output,
	color_data,
	options->render_options.canvas_width,
	options->render_options.canvas_height,
	options->jpeg_quality
);
```

这条路径避免了“先保存 BMP，再重新读取 BMP”的冗余磁盘读写，也说明 JPEG 编码器本身并不依赖 BMP。

## 4. BMP 输入适配

### 4.1 `bmp_rgb_image`

```c
typedef struct{
	uint8_t* color_data;
	int      width;
	int      height;
}bmp_rgb_image;
```

| 字段 | 含义 | 所有权 |
| --- | --- | --- |
| `color_data` | 顶行优先的连续 RGB 像素 | `bmp_read_rgb()` 分配，调用者通过 `bmp_rgb_image_free()` 释放 |
| `width` | 图像宽度 | 普通值 |
| `height` | 转换后的正图像高度 | 普通值 |

`bmp_rgb_image_free()` 可以接收空结构。它释放 `color_data` 后，还会把指针、宽度和高度清零，降低重复释放和误用旧尺寸的风险。

### 4.2 手动读取小端整数

BMP 字段使用小端字节序。代码没有把文件内容直接强制转换为 C 结构体，而是通过三个函数逐字节读取：

| 函数 | 结果类型 | 作用 |
| --- | --- | --- |
| `read_u16_little()` | `uint16_t` | 读取两个字节并组合为无符号整数 |
| `read_u32_little()` | `uint32_t` | 读取四个字节并组合为无符号整数 |
| `read_s32_little()` | `int32_t` | 先读无符号值，再按相同比特解释为有符号整数 |

这种方式不依赖结构体对齐和主机字节序，也避免 `#pragma pack`。任意字段遇到 EOF 时立即返回 -1，随后由上层进入统一清理路径。

### 4.3 当前读取的 BMP 头字段

| 字段 | 代码中的变量 | 用途 |
| --- | --- | --- |
| 文件签名 | `signature` | 必须为 `0x4D42`，即磁盘上的 `BM` |
| 文件大小 | `ignored` | 读取以移动文件指针，当前不依赖该值 |
| 两个保留字段 | `reserved` | 读取但不使用 |
| 像素偏移 | `pixel_offset` | 定位像素数组起点 |
| DIB 头大小 | `info_header_size` | 必须不小于 40 字节 |
| 宽度 | `width_value` | 必须为正 |
| 高度 | `height_value` | 可正可负，但不能为 0 或 `INT32_MIN` |
| 颜色平面数 | `planes` | 必须为 1 |
| 位深 | `bit_count` | 必须为 24 |
| 压缩方式 | `compression` | 必须为 0，即 `BI_RGB` |

当前适配器接受 BITMAPINFOHEADER 或更大的 DIB 头，但只读取编码所需字段。`pixel_offset` 必须位于完整文件头与 DIB 头之后，否则文件布局被判为非法。

### 4.4 BMP 行补齐

24 位 BMP 的一行像素占 `width * 3` 字节，但磁盘中的每行必须补齐到四字节边界。代码计算：

```c
row_color_length = (size_t)width_value*3;
row_file_length  = (row_color_length+3)&~(size_t)3;
```

`row_data` 按 `row_file_length` 分配，因此一次 `fread()` 会同时读入有效像素和行尾填充。转换像素时只遍历真实宽度，不会把填充字节写入 RGB 缓冲区。

### 4.5 行方向与颜色通道

BMP 的正高度表示磁盘第一行是图像底行，负高度表示磁盘第一行就是图像顶行。代码用以下关系统一为顶行优先：

```c
target_y = (height_value>0)?(image_height-1-file_y):file_y;
```

BMP 像素按 B、G、R 保存，而编码器要求 R、G、B。因此每个像素会执行通道反转：

```c
target[0] = source[2];
target[1] = source[1];
target[2] = source[0];
```

### 4.6 溢出与失败清理

分配前会检查 `width * 3`、行补齐加法以及 `row_color_length * height` 是否超出 `size_t`。`pixel_offset` 还必须能安全传给当前使用的 `fseek(long)`。

函数采用单一 `cleanup` 出口。无论头部非法、分配失败、定位失败还是像素截断，都会释放两个临时缓冲区并关闭文件；只有成功时才把 `color_data` 所有权转交给结果结构。

## 5. JPEG 编码器的常量和内部状态

### 5.1 关键常量

| 常量 | 值 | 作用 |
| --- | ---: | --- |
| `JPEG_DCT_SIZE` | 8 | 每个 DCT 数据块的边长 |
| `JPEG_MCU_SIZE` | 16 | 4:2:0 下每个 MCU 覆盖的亮度区域边长 |
| `JPEG_CHROMA_SCALE` | 2 | 色度在水平和垂直方向各缩小两倍 |
| `JPEG_BLOCK_LENGTH` | 64 | 一个 8×8 数据块的元素数 |
| `JPEG_COMPONENT_COUNT` | 3 | Y、Cb、Cr 三个分量 |
| `JPEG_HUFFMAN_TABLE_COUNT` | 4 | 亮度/色度各一张 DC 表和一张 AC 表 |
| `JPEG_PI` | π 的双精度常量 | DCT 余弦计算 |

分量索引 `JPEG_COMPONENT_Y/CB/CR` 分别为 0、1、2。它们既用于选择 Huffman 表，也用于访问三个独立的 DC 预测值。

Huffman 表索引分别是 `DC_Y`、`AC_Y`、`DC_C`、`AC_C`。Cb 与 Cr 共用色度表，但 DC 历史值彼此独立。

### 5.2 `jpeg_huffman_table`

```c
typedef struct{
	u16 code[256];
	u8  size[256];
}jpeg_huffman_table;
```

| 字段 | 下标 | 内容 |
| --- | --- | --- |
| `code` | Huffman 符号值 | 该符号对应的规范 Huffman 码字 |
| `size` | Huffman 符号值 | 码字有效位数，0 表示该符号未定义 |

JPEG 文件中的 DHT 表按“每个码长的符号数量 + 符号顺序”保存，不适合编码时反复查找。该结构把它预展开为按符号直接索引的编码表。

### 5.3 `jpeg_writer`

```c
typedef struct{
	FILE* file;
	u32  bit_buffer;
	int  bit_count;
	int  last_dc[3];
	int  error;
}jpeg_writer;
```

| 字段 | 作用 |
| --- | --- |
| `file` | 当前输出文件 |
| `bit_buffer` | 尚未凑满一个字节的熵编码位 |
| `bit_count` | `bit_buffer` 中当前有效位数 |
| `last_dc[3]` | Y、Cb、Cr 各自上一个块的 DC 系数 |
| `error` | 贯穿所有写入层级的失败状态，0 表示正常，-1 表示失败 |

`jpeg_writer` 把普通文件输出、位输出、DC 预测和错误传播集中在一个上下文中。内部函数无需层层返回错误码，只要检查或设置 `writer->error`。

## 6. JPEG 文件的整体布局

`jpeg_generate()` 按以下顺序写出文件：

```text
SOI
APP0 / JFIF
DQT
SOF0
DHT
SOS
熵编码扫描数据
EOI
```

| 顺序 | 标记 | 十六进制 | 当前实现内容 |
| ---: | --- | --- | --- |
| 1 | SOI | `FFD8` | 图像开始 |
| 2 | APP0 | `FFE0` | JFIF 1.01 标识和基本密度信息 |
| 3 | DQT | `FFDB` | 亮度表 0、色度表 1 |
| 4 | SOF0 | `FFC0` | 8 位 Baseline 帧、尺寸和 4:2:0 采样 |
| 5 | DHT | `FFC4` | 四张标准 Huffman 表 |
| 6 | SOS | `FFDA` | 三分量单次顺序扫描参数 |
| 7 | 数据 | 无固定标记 | DC/AC Huffman 熵编码流 |
| 8 | EOI | `FFD9` | 图像结束 |

SOI 和 EOI 只有两字节标记。其余段的长度字段包含长度字段自身的两个字节，但不包含段前面的两字节 Marker。

## 7. 基础字节与 Bit 写入

### 7.1 原始字节和大端整数

`write_byte()` 是最底层文件写函数。只要之前已失败，它就直接返回；若 `fwrite()` 没写入一个字节，则把 `error` 设为 -1。

JPEG 多字节字段使用大端序。`write_word()` 依次写高八位和低八位，`write_marker()` 则用它写入完整的 `0xFFxx` 标记。

### 7.2 熵编码中的 `0xFF` 转义

JPEG 扫描数据中的 `0xFF` 可能被解码器误认为下一个 Marker。`write_entropy_byte()` 在任何数据字节 `0xFF` 后自动补写 `0x00`：

```text
熵编码原始字节 FF
文件实际写入     FF 00
```

这种 byte stuffing 只用于熵编码数据。文件段 Marker 本身通过 `write_marker()` 写入，不能追加 `0x00`。

### 7.3 从高位到低位写码字

`write_bits()` 接受一个最多 16 位的码字。循环从 `bit_length - 1` 到 0，逐位把最高有效位先放入缓冲区；凑满八位后调用 `write_entropy_byte()`。

虽然 `bit_buffer` 是 32 位，但当前逻辑每满八位就清零。因此它实际上承担“正在拼装的一个字节”，而不是长期保存多字节位流。

### 7.4 扫描结尾补位

Huffman 数据通常不会恰好结束在字节边界。`flush_bits()` 将剩余位左移，并按 JPEG 约定使用全 1 补足低位，再通过熵字节函数输出。

补位后会把 `bit_buffer` 和 `bit_count` 清零。这样随后的 EOI Marker 一定从新的字节边界开始。

## 8. 各 JPEG 文件段的具体实现

### 8.1 APP0 / JFIF

`write_app0()` 写入长度 16 的 JFIF APP0 段。

| 字段 | 当前值 | 含义 |
| --- | --- | --- |
| 标识符 | `JFIF\0` | 声明 JFIF 文件 |
| 主版本 | 1 | JFIF 版本 1 |
| 次版本 | 1 | JFIF 1.01 |
| 密度单位 | 0 | 仅表示宽高比，不声明 DPI 单位 |
| X/Y 密度 | 1/1 | 像素宽高比为 1:1 |
| 缩略图宽高 | 0/0 | APP0 中不附带缩略图 |

### 8.2 DQT 量化表段

`write_quantization_tables()` 在一个 DQT 段中连续写入两张 8 位量化表。段长为：

```text
2 + (1 + 64) + (1 + 64) = 132
```

第一张表的表信息字节为 `0x00`，表示 8 位精度、表号 0；第二张为 `0x01`，表示 8 位精度、表号 1。

内存中的量化表保持普通二维行优先顺序，但 DQT 文件段要求 Zigzag 顺序，因此写入值为 `table[zigzag_order[i]]`。

### 8.3 SOF0 帧头

`write_frame_header()` 写入长度 17 的 Baseline DCT 帧头。

| 字段 | 当前值 |
| --- | --- |
| 样本精度 | 8 位 |
| 图像高度、宽度 | `jpeg_generate()` 的输入尺寸 |
| 分量数 | 3 |
| Y 分量 | ID 1，采样 `0x22`，量化表 0 |
| Cb 分量 | ID 2，采样 `0x11`，量化表 1 |
| Cr 分量 | ID 3，采样 `0x11`，量化表 1 |

采样字节高四位是水平因子，低四位是垂直因子。Y 的 `0x22` 表示 2×2，Cb/Cr 的 `0x11` 表示 1×1，因此当前输出固定为 4:2:0。

### 8.4 DHT Huffman 表段

`write_huffman_tables()` 把四组标准表写入同一个 DHT 段：

| 表信息字节 | 表类型 | 表号 | 用途 |
| --- | --- | --- | --- |
| `0x00` | DC | 0 | Y 的 DC |
| `0x10` | AC | 0 | Y 的 AC |
| `0x01` | DC | 1 | Cb/Cr 的 DC |
| `0x11` | AC | 1 | Cb/Cr 的 AC |

表信息字节的高四位表示类别，0 为 DC、1 为 AC；低四位表示表号。每张表随后写 16 个码长计数，再写这些码长对应的全部符号。

`get_huffman_value_count()` 汇总 `bits[1]` 到 `bits[16]`。DHT 段长由实际符号数计算，而不是硬编码，因此数组和段长度保持一致。

### 8.5 SOS 扫描头

`write_scan_header()` 写入长度 12 的单次顺序扫描头。

| 分量 | 选择字节 | 含义 |
| --- | --- | --- |
| Y | `0x00` | DC 表 0、AC 表 0 |
| Cb | `0x11` | DC 表 1、AC 表 1 |
| Cr | `0x11` | DC 表 1、AC 表 1 |

随后写入 `Ss=0`、`Se=63`、`Ah/Al=0`，表示一个扫描内编码每个块的全部 64 个系数，并且不是渐进式逐次逼近。

## 9. 质量参数与量化表

### 9.1 基础量化表

`base_quantization_y[64]` 和 `base_quantization_c[64]` 分别保存常用的亮度、色度基础量化表。亮度表保留更多视觉敏感信息，色度表的多数高频项更大，允许更强的色彩细节损失。

两张基础表和 DCT 输出使用相同的自然二维顺序。只有写 DQT 和扫描 AC 系数时，才通过 `zigzag_order[]` 改变访问顺序。

### 9.2 质量到缩放值

`build_quantization_tables()` 将 1～100 的质量参数转换为缩放系数：

```text
quality < 50 : scale = 5000 / quality
quality ≥ 50 : scale = 200 - 2 × quality
```

低质量区使用反比例，使量化值快速增大；高质量区使用线性下降。质量 50 得到 `scale=100`，量化表接近基础表。

### 9.3 生成最终量化项

每个量化项按以下方式计算：

```text
Q = (base × scale + 50) / 100
Q = clamp(Q, 1, 255)
```

加 50 用于整数除法的四舍五入。量化值不能为 0，否则 DCT 系数会除零；Baseline 当前采用 8 位量化表，因此最大限制为 255。

质量 100 时 `scale=0`，公式初值为 0，但最终会被限制为 1。这表示最弱量化，不代表数学意义上的完全无损。

量化值越大，更多 DCT 系数会被舍入为 0，压缩率通常越高、细节损失越大。质量参数只影响量化表，不改变 4:2:0 采样和 Huffman 表。

## 10. 标准 Huffman 表的建立

### 10.1 源数组的含义

每张 Huffman 表由两个数组描述：

| 数组 | 含义 |
| --- | --- |
| `bits[1..16]` | 每一种码长分别有多少个符号；下标 0 不使用 |
| `values[]` | 先按码长递增，再按该码长中的规范顺序排列符号 |

DC 表的符号是幅值 Category 0～11。AC 表的符号高四位表示前导零数量，低四位表示非零系数的幅值 Category。

代码保存的是 JPEG 常用标准亮度和色度表，不会先统计当前图像频率并生成图像专用最优表。这减少了实现复杂度，但文件通常不是理论上的最小尺寸。

### 10.2 规范 Huffman 码生成

`build_huffman_table()` 首先确认 `bits[]` 的总符号数等于 `value_count`，然后清空目标表。

生成过程从码字 0 和码长 1 开始：

```text
对 length = 1..16：
  为 bits[length] 个符号依次分配当前 code
  每分配一个符号，code 加 1
  处理完当前长度后，code 左移 1 位
```

这就是规范 Huffman 编码。文件只需要保存码长分布和符号顺序，编码器与解码器便能独立重建相同码字。

生成结果写入 `table.code[symbol]` 和 `table.size[symbol]`。后续编码时可以用符号值 O(1) 查到码字与长度。

### 10.3 四张运行时表

`initialize_huffman_tables()` 依次构建：

1. 亮度 DC：12 个符号。
2. 亮度 AC：162 个符号。
3. 色度 DC：12 个符号。
4. 色度 AC：162 个符号。

任意一张表的数量校验失败，初始化就返回 -1，JPEG 文件尚未打开。写入 DHT 使用的原始数组与运行时编码表来自同一组数据，保证文件声明和实际扫描码流一致。

## 11. RGB 到 YCbCr

JPEG 不直接对 R、G、B 三通道编码，而是转换为亮度 Y 和两个色度分量 Cb、Cr。这样可以利用人眼对亮度细节更敏感、对色彩空间细节相对不敏感的特点。

### 11.1 亮度计算

`get_luma_block()` 对每个 RGB 像素计算：

```text
Y = 0.299R + 0.587G + 0.114B
```

函数一次提取一个完整分辨率的 8×8 Y 块。`block_x`、`block_y` 是该块左上角在原图中的位置。

### 11.2 色度计算

`get_chroma_blocks()` 使用：

```text
Cb = -0.168736R - 0.331264G + 0.5B + 128
Cr =  0.5R - 0.418688G - 0.081312B + 128
```

`+128` 把原本以 0 为中心的色差信号平移到约 0～255 范围。DCT 阶段还会统一减 128，使所有分量重新以 0 为中心。

### 11.3 为什么先平均 RGB

每个色度样本对应原图 2×2 像素。代码先分别累加这四个像素的 R、G、B，再除以 4，最后只执行一次 RGB→CbCr。

颜色变换是线性运算，因此“先转换四次再平均”和“先平均再转换一次”等价。当前写法减少了重复浮点乘法。

## 12. 4:2:0 采样与 MCU

### 12.1 采样关系

SOF0 中 Y 的采样因子是 2×2，Cb 和 Cr 是 1×1。这表示在同一空间区域内：

```text
Y  : 2 × 2 个 8×8 块
Cb : 1 × 1 个 8×8 块
Cr : 1 × 1 个 8×8 块
```

一个 MCU 因此覆盖原图 16×16 像素，共编码六个 8×8 块。

### 12.2 MCU 中的块顺序

`write_scan_data()` 按从左到右、从上到下遍历 16×16 MCU。每个 MCU 内的编码顺序是：

```text
Y0：左上 8×8
Y1：右上 8×8
Y2：左下 8×8
Y3：右下 8×8
Cb：整个 16×16 区域下采样后的 8×8
Cr：整个 16×16 区域下采样后的 8×8
```

亮度块的两个嵌套偏移循环都从 0 递增到 8，因此实际顺序与上面一致。色度块只有在四个亮度块成功编码后才处理。

### 12.3 MCU 数量

循环每次令 `mcu_x`、`mcu_y` 增加 16，因此 MCU 数量等价于：

```text
horizontal_mcu = ceil(width / 16)
vertical_mcu   = ceil(height / 16)
total_mcu      = horizontal_mcu × vertical_mcu
```

图像宽高不要求是 16 的倍数。测试中的 1×1、13×11、17×19 等尺寸就是为了覆盖不完整 MCU。

### 12.4 边缘复制

当采样坐标超过图像右边或下边时，`get_luma_block()` 和 `get_chroma_blocks()` 都使用最后一个合法坐标：

```c
if(source_x >= width){
	source_x = width-1;
}
if(source_y >= height){
	source_y = height-1;
}
```

这相当于复制最近边缘像素，补齐最后一个 MCU。它不会读取缓冲区外内存，也比补黑色更少在图像边缘制造高频突变。

## 13. 二维 DCT 与量化

### 13.1 输入中心化

`transform_block()` 同时负责 DCT 与量化。所有输入分量原本约在 0～255，计算时先减 128，将直流中心移动到 0：

```text
sample = input(x,y) - 128
```

中心化后，DC 系数主要表示该块相对中灰值的平均偏移，AC 系数表示块内变化。

### 13.2 当前二维 DCT 公式

代码直接计算 8×8 DCT：

```text
F(u,v) = 1/4 × C(u) × C(v)
         × Σx Σy [sample(x,y)
         × cos((2x+1)uπ/16)
         × cos((2y+1)vπ/16)]
```

其中：

```text
C(0) = 1 / √2
C(n) = 1，n > 0
```

外层遍历 `v`、`u` 生成 64 个频域系数，内层遍历 `y`、`x` 汇总 64 个像素。输出位置仍为 `v*8+u` 的自然二维顺序。

### 13.3 DC 与 AC

`output[0]` 对应 `u=0,v=0`，称为 DC 系数，主要描述整个块的平均亮度或色度。

其余 63 项称为 AC 系数。越靠近二维矩阵右下角，水平和垂直频率越高，通常越容易在量化后变为 0。

### 13.4 量化与舍入

DCT 结果在同一个函数中除以对应量化项：

```text
coefficient = DCT(u,v) / quantization(u,v)
```

正数加 0.5、负数减 0.5 后转换为整数，从而实现远离 0 的对称四舍五入。结果保存到 `s16 coefficient[64]`。

量化是 JPEG 的主要有损步骤。除数越大，系数绝对值越小，更多高频项变成 0，后续零游程与 Huffman 编码就越有效。

当前实现直接调用 `cos()` 计算二维公式，意图清晰，适合展示算法；但它没有预计算余弦值，也没有采用两次一维快速 DCT，因此是编码阶段最明显的性能热点。

## 14. Zigzag 扫描

DCT 会把低频集中在左上角，高频分布到右下角。若按普通行顺序扫描，不容易形成最长的连续零区间。

`zigzag_order[64]` 从 DC 开始，沿对角线大致由低频走向高频。它在当前代码中有两个用途：

1. 将量化表按 JPEG DQT 要求的 Zigzag 顺序写入文件。
2. 在 `encode_block()` 中按 Zigzag 顺序读取 63 个 AC 系数。

量化缓冲区本身没有原地重排。访问时使用 `block[zigzag_order[i]]`，既保留自然二维布局，也避免额外复制数组。

## 15. 系数幅值表示

### 15.1 Category 位数

`get_value_size()` 计算绝对值需要多少二进制位。JPEG 把这个位数称为 Category 或 Size。

| 系数范围 | Size |
| --- | ---: |
| 0 | 0 |
| -1、1 | 1 |
| -3～-2、2～3 | 2 |
| -7～-4、4～7 | 3 |

该函数对绝对值不断右移，直到为 0。它只返回位数，不返回实际附加位。

### 15.2 正负数附加位

`get_value_bits()` 把有符号系数转换为 JPEG 幅值附加位。正数直接使用其二进制值，负数使用：

```c
value + ((1 << size) - 1)
```

例如 `+5` 的 Size 为 3，附加位为 `101`；`-5` 也为 Size 3，附加位为 `010`。负数编码相当于同位数正幅值的逐位取反形式。

Huffman 码只描述 Size 或“零游程+Size”，真正数值由紧跟其后的附加位给出。

## 16. DC 差分编码

相邻图像块的平均值通常接近，因此 JPEG 不直接编码每个 DC，而是编码当前 DC 与同分量前一个 DC 的差：

```text
difference = current_dc - last_dc[component]
last_dc[component] = current_dc
```

Y、Cb、Cr 各有独立预测历史。虽然 Cb 和 Cr 使用同一套色度 Huffman 表，但不能共用前一个 DC 值。

DC 编码步骤为：

1. 计算差值的 Size。
2. 使用 Size 作为符号，从对应 DC Huffman 表写出码字。
3. Size 不为 0 时，再写差值的幅值附加位。

Baseline 标准 DC 表支持的 Size 最大为 11。若计算结果超出 11，或对应表项不存在，`encode_block()` 将编码状态置为失败。

## 17. AC 零游程编码

### 17.1 普通非零系数

AC 系数按 Zigzag 顺序遍历。遇到 0 时增加 `zero_count`，遇到非零值时生成：

```text
symbol = (zero_count << 4) | size
```

高四位记录该非零值前面的零数量，低四位记录非零值的 Size。写出该符号的 AC Huffman 码后，再写非零系数的幅值附加位。

例如三个 0 后出现 Size 3 的 `-5`，符号为 `0x33`。码流由 `0x33` 对应的 Huffman 码和 `-5` 的三位附加值 `010` 组成。

### 17.2 ZRL

一个 AC 符号的高四位最多表示 15 个前导零。每累计至少 16 个零，代码先写特殊符号 `0xF0`，即 ZRL，表示连续 16 个零，然后从计数中减去 16。

如果后面还有非零系数，剩余不足 16 个零会进入普通 `(run,size)` 符号。

### 17.3 EOB

若扫描完 63 个 AC 系数后仍有尾部零，代码写特殊符号 `0x00`，即 EOB，表示这个块的剩余系数全部为 0。

如果最后一个 AC 系数刚好非零，`zero_count` 为 0，不需要额外 EOB，因为解码器已经得到完整 63 项。

Baseline 标准 AC 表支持的非零 Size 最大为 10。超出范围时当前实现将整个编码标记为失败。

## 18. 一个数据块的完整编码

`encode_block()` 是频域系数进入位流的统一入口。它先根据 `component` 选择亮度或色度 Huffman 表，再按以下顺序处理：

```text
量化后的自然顺序系数 block[64]
  ├─block[0]
  │   ├─同分量 DC 差分
  │   ├─DC Size Huffman 码
  │   └─DC 幅值附加位
  └─block[zigzag_order[1..63]]
      ├─累计连续零
      ├─必要时写 ZRL
      ├─写 (run,size) Huffman 码和幅值位
      └─尾部零写 EOB
```

该函数不会执行 DCT，也不会决定 MCU 顺序。它只关心一个已经量化的数据块和该块所属分量。

## 19. 整张图像的扫描数据

`write_scan_data()` 组合前面的像素提取、颜色转换、DCT、量化和 Huffman 编码：

```text
遍历每个 16×16 MCU
  ├─提取 Y 左上块 → DCT/量化 → encode_block(Y)
  ├─提取 Y 右上块 → DCT/量化 → encode_block(Y)
  ├─提取 Y 左下块 → DCT/量化 → encode_block(Y)
  ├─提取 Y 右下块 → DCT/量化 → encode_block(Y)
  ├─2×2 平均得到 Cb/Cr
  ├─Cb DCT/量化 → encode_block(Cb)
  └─Cr DCT/量化 → encode_block(Cr)

完成所有 MCU
  └─flush_bits()
```

三个 `double[64]` 分量块和一个 `s16[64]` 系数块都位于栈上。亮度块缓冲区会重复使用，Cb 和 Cr 先同时生成，再依次复用同一个系数缓冲区。

两个 MCU 循环和两个亮度块循环都把 `writer->error==0` 放在条件中。一旦底层文件写入或系数范围检查失败，后续 MCU 会立即停止。

即使循环中失败，函数最后仍调用 `flush_bits()`；但该函数检测到 `error != 0` 后会直接返回，不再产生额外数据。

## 20. `jpeg_generate()` 完整生命周期

`jpeg_generate()` 是模块唯一公共编码入口，完整顺序如下：

```text
check_jpeg_input()
  ↓
initialize_huffman_tables()
  ↓
build_quantization_tables()
  ↓
清零 jpeg_writer 并 fopen("wb")
  ↓
SOI → APP0 → DQT → SOF0 → DHT → SOS
  ↓
write_scan_data()
  ↓
EOI
  ↓
fclose()
  ├─成功：保留文件并返回 0
  └─失败：remove(file) 并返回 -1
```

### 20.1 输入检查

`check_jpeg_input()` 拒绝：

- 空路径或空 RGB 指针；
- 非正宽高；
- 超过 65535 的宽高；
- 不在 1～100 内的质量；
- `width * height * 3` 会溢出 `size_t` 的尺寸。

65535 限制来自 SOF0 中宽高字段各为 16 位。溢出检查虽然不分配内存，但能保证后续像素位置计算所依据的缓冲区长度可由 `size_t` 表示。

### 20.2 初始化顺序

Huffman 表和量化表都在打开输出文件前准备。若标准表数据自身不一致，函数会在产生任何输出文件之前失败。

`memset(&writer, 0, ...)` 同时把位缓冲、三个 DC 预测值和错误状态归零。第一个 Y、Cb、Cr 块因此都使用 0 作为 DC 初始预测值。

### 20.3 文件失败保护

任意 `fwrite()` 失败都会设置 `writer.error`。`fclose()` 失败也会把最终结果设为 -1。

只要编码过程或关闭文件失败，代码就调用 `remove(file)` 删除不完整输出，避免调用者误把只有部分段或部分扫描数据的文件当作成功结果。

## 21. 数据与资源所有权

| 资源 | 创建位置 | 释放位置 | 说明 |
| --- | --- | --- | --- |
| BMP `row_data` | `bmp_read_rgb()` | 同函数 `cleanup` | 仅保存一行磁盘数据 |
| BMP `color_data` | `bmp_read_rgb()` | `bmp_rgb_image_free()` | 成功后转交调用者 |
| 根程序 RGB 画布 | 字体渲染层 | 根程序清理逻辑 | JPEG 只读借用 |
| 输出 `FILE*` | `jpeg_generate()` | 同函数 `fclose()` | 不向外暴露 |
| Huffman 运行时表 | `jpeg_generate()` 栈 | 函数返回自动释放 | 四张查找表 |
| 量化表与块缓冲 | 编码函数栈 | 函数返回自动释放 | 与图像面积无关 |

`jpg.c` 本身不为整幅图像申请额外堆内存。除调用者提供的 RGB 缓冲区外，编码时只保留当前 MCU 所需的块和固定大小表，因此辅助内存基本为常量级。

## 22. 函数调用关系

```text
jpeg_generate
  ├─check_jpeg_input
  ├─initialize_huffman_tables
  │   └─build_huffman_table ×4
  │       └─get_huffman_value_count
  ├─build_quantization_tables
  ├─write_marker(SOI)
  ├─write_app0
  ├─write_quantization_tables
  ├─write_frame_header
  ├─write_huffman_tables
  │   ├─get_huffman_value_count
  │   └─write_huffman_definition ×4
  ├─write_scan_header
  ├─write_scan_data
  │   ├─get_luma_block ×4/MCU
  │   ├─get_chroma_blocks ×1/MCU
  │   ├─transform_block ×6/MCU
  │   ├─encode_block ×6/MCU
  │   │   ├─get_value_size
  │   │   ├─get_value_bits
  │   │   └─write_bits
  │   └─flush_bits
  └─write_marker(EOI)
```

所有文件段写函数最终都落到 `write_byte()`。所有扫描数据码字最终都落到 `write_bits()`，满字节后通过 `write_entropy_byte()` 执行 `0xFF` 填充。

## 23. 从一个 RGB 像素到 JPEG 字节

把前面的模块串起来，一个像素并不是独立压缩，而是参与所在 MCU 的多级变换：

```text
RGB 像素
  ↓
按位置进入一个 8×8 Y 块
同时参与一个 2×2 平均，形成 Cb/Cr 样本
  ↓
Y/Cb/Cr 各自组成 8×8 数据块
  ↓ 每项减 128
二维 DCT
  ↓ 除量化表并舍入
一个 DC + 63 个 AC
  ↓
DC：同分量差分
AC：Zigzag + 零游程
  ↓
标准 Huffman 码 + 幅值附加位
  ↓
按位拼接，FF 后插入 00
  ↓
JPEG 扫描数据字节
```

空间域的一个像素会影响整个 8×8 块的多个 DCT 系数，因此 JPEG 不是“逐像素压缩”。解码时也必须先恢复一个完整块的频域系数，再做反量化与 IDCT。

## 24. 测试如何验证实现

### 24.1 JPEG 结构测试

`tests/test_jpeg.c` 构造 RGB 测试图并调用 `jpeg_generate()`。有效用例覆盖：

| 尺寸或参数 | 验证目的 |
| --- | --- |
| 1×1 | 最小图像和大量边缘复制 |
| 13×11 | 宽高都不是 MCU 倍数 |
| 16×16 | 恰好一个完整 4:2:0 MCU |
| 17×19 | 跨越水平、垂直 MCU 边界 |
| 质量 1、50、90、100 | 两段质量公式及量化上下界 |
| 连续多次编码 | 检查状态是否局部初始化、能否重复调用 |

测试会读取文件起止标记，确认 SOI 与 EOI 存在；还会定位 SOF0，核对图像宽高、三个分量、量化表号和 `0x22/0x11/0x11` 采样因子。

非法用例覆盖空像素指针、零尺寸、超出 16 位范围的尺寸，以及质量 0 和 101。它们应在创建有效 JPEG 前返回失败。

### 24.2 BMP 适配测试

`tests/test_bmp_reader.c` 使用已知顶行优先 RGB 数据生成 BMP，再读取并逐字节比较。测试图宽度会触发行补齐，从而同时验证：

- BMP 磁盘 BGR 是否正确转为 RGB；
- 正高度 BMP 是否从底行翻转为顶行；
- 行尾补齐是否没有混入输出像素；
- 宽高是否与源图一致。

### 24.3 外部解码验证

结构测试能证明段字段与采样声明正确，但无法完整证明熵数据能被所有解码器恢复。实践中还应使用独立图片库或系统查看器打开生成结果。

独立解码验证尤其能发现 DHT 与实际码流不一致、Bit 补齐错误、`0xFF` 未填充、DC 历史顺序错误或 MCU 块顺序错误等问题。

## 25. 调试时应关注的层级

如果生成 JPEG 失败，可按以下顺序定位：

1. `check_jpeg_input()` 是否拒绝路径、尺寸或质量。
2. `fopen()` 是否成功创建输出文件。
3. `writer.error` 是否在文件段写入阶段置为 -1。
4. `encode_block()` 是否因 DC Size>11 或 AC Size>10 失败。
5. `fclose()` 是否失败，失败文件会被删除。

如果函数返回成功但图片颜色错误，应优先检查输入是否真的是顶行优先 RGB，而不是 BMP 的 BGR 或底行优先数据。

如果图片可打开但边缘异常，应检查最后一个 MCU 的坐标、源宽高和边缘复制。若色彩块状明显，则还要区分这是 4:2:0 的正常色度损失，还是通道顺序错误。

文件头问题可从 SOI 开始逐段解析，扫描数据问题则重点检查 DHT、SOS、MCU 顺序、DC 预测、Bit stuffing 和结尾补位。

## 26. 当前实现的优点

### 26.1 算法链完整且独立

编码器自行完成颜色变换、4:2:0、DCT、量化、Zigzag、DC/AC 编码、规范 Huffman 表、Bit 流和 JFIF 文件封装，没有把核心压缩步骤交给第三方库。

### 26.2 接口边界清晰

`jpeg_generate()` 只依赖标准 RGB 缓冲区，因此既可服务 BMP 转换器，也能直接承接字体渲染结果。输入适配与压缩算法不会互相污染。

### 26.3 辅助内存固定

编码过程以 MCU 为单位流式处理，不保存整幅 YCbCr 图像和全部 DCT 系数。除输入 RGB 外，主要工作缓冲都是固定的 8×8 数组。

### 26.4 失败不会留下半成品

底层写错误通过 `jpeg_writer.error` 统一传播，关闭失败也计入最终结果。失败输出会被删除，而不是留下带正确扩展名但数据不完整的文件。

### 26.5 支持任意合法尺寸

边缘复制允许宽高不是 8 或 16 的倍数。最小 1×1 到 SOF0 可表示的 65535×65535 都有明确输入检查和 MCU 补齐策略。

## 27. 性能与压缩取舍

### 27.1 时间复杂度

图像包含约 `ceil(width/16) × ceil(height/16)` 个 MCU，每个 MCU 进行六次 DCT。

当前每次二维 DCT 为 64 个输出系数遍历 64 个输入点，并在内层调用余弦函数。因此总体随像素数线性增长，但常数明显大于快速整数 DCT 实现。

### 27.2 主要性能热点

`transform_block()` 是主要热点，原因包括：

- 对每个块直接计算二维 DCT；
- 相同的余弦值没有预计算；
- 没有把二维 DCT 分解为行、列两次一维变换；
- 全程使用 `double` 和 `cos()`。

后续若优化，可先增加固定的 `cos_table[8][8]`，再考虑可分离 DCT或整数快速 DCT。优化时应保留现有版本作为正确性参考。

### 27.3 4:2:0 的效果

4:2:0 将每个 16×16 区域的 Cb、Cr 各缩减为 8×8，即色度样本数量降到全分辨率的四分之一。总采样量从 3×256 降到 `256+64+64=384`。

相较 4:4:4 的 768 个样本，进入 DCT 的原始分量样本数减半。代价是小尺寸彩色文字、锐利彩边和高饱和细线可能出现色度模糊。

### 27.4 固定 Huffman 表的取舍

标准表省去了频率统计、构造最优树和缓冲扫描数据的过程，适合轻量编码器。缺点是无法针对当前图像的符号分布取得最优熵编码长度。

## 28. 当前限制

当前实现有意聚焦最小可用的 Baseline JPEG 编码链，尚未覆盖以下能力：

| 限制 | 当前状态 |
| --- | --- |
| JPEG 类型 | 只支持 8 位 Baseline Sequential DCT |
| 采样模式 | 固定 4:2:0，不能选择 4:4:4 或 4:2:2 |
| 颜色分量 | 固定三分量 YCbCr，不输出灰度 JPEG |
| Huffman | 使用标准表，不按图像优化 |
| Progressive | 不支持渐进式多扫描 |
| Restart Marker | 不写 DRI/RST，损坏恢复能力有限 |
| 元数据 | 只写 JFIF APP0，不写 EXIF、ICC、注释和方向信息 |
| 透明度 | RGB 接口没有 Alpha，JPEG 本身也不保存透明通道 |
| BMP 输入 | 只支持未压缩 24 位 BMP，不支持 8/16/32 位及压缩 BMP |
| DCT 性能 | 使用直接双精度二维公式，尚未优化 |
| 错误详情 | 公共接口只返回 0/-1，未暴露细分错误码 |
| 解码 | 项目只编码 JPEG，不实现 JPEG→RGB/BMP |

这些限制不影响当前“字体渲染 RGB→JPEG”和“24 位 BMP→JPEG”的主目标，但应在后续扩展接口前明确，避免把现有函数误认为通用 JPEG 框架。

## 29. 可继续扩展的方向

扩展时建议保持 `jpeg_generate()` 作为简单缺省接口，再新增配置结构和高层接口，而不是让现有参数列表持续膨胀。

可以考虑：

1. 增加 `jpeg_options`，选择 4:4:4、4:2:2、4:2:0 或灰度。
2. 预计算 DCT 余弦表，之后替换为可分离或整数 DCT。
3. 增加按图像统计生成优化 Huffman 表的可选策略。
4. 增加 Restart Interval，以便分段解码和错误恢复。
5. 增加 APP1/EXIF 或 ICC Profile 写入接口。
6. 让 BMP 适配器支持 32 位 BGRA，并明确 Alpha 的合成背景。
7. 增加更具体的错误枚举，同时保留旧接口的 0/-1 兼容行为。

其中采样模式会改变 SOF0 采样因子、MCU 大小、每 MCU 块数和扫描顺序，属于结构性扩展。DCT 预计算只替换块变换内部算法，接口影响较小，适合优先实施。

## 30. 核心函数索引

| 函数 | 层级 | 主要作用 |
| --- | --- | --- |
| `jpeg_generate()` | 公共入口 | 验证输入并组织完整 JPEG 生命周期 |
| `check_jpeg_input()` | 参数层 | 检查路径、指针、尺寸、质量和乘法溢出 |
| `build_quantization_tables()` | 压缩配置 | 根据质量生成 Y/C 两张量化表 |
| `initialize_huffman_tables()` | 压缩配置 | 构建四张运行时标准 Huffman 表 |
| `get_huffman_value_count()` | Huffman 基础 | 统计指定码长数组中的符号总数 |
| `build_huffman_table()` | Huffman 基础 | 生成按符号索引的规范码表 |
| `write_app0()` | 文件封装 | 写 JFIF APP0 |
| `write_quantization_tables()` | 文件封装 | 写两张 DQT |
| `write_frame_header()` | 文件封装 | 写 SOF0 和 4:2:0 采样参数 |
| `write_huffman_definition()` | 文件封装 | 写一张 DHT 定义 |
| `write_huffman_tables()` | 文件封装 | 合并写四张 DHT |
| `write_scan_header()` | 文件封装 | 写三分量 SOS |
| `get_luma_block()` | 像素处理 | 提取完整分辨率 8×8 Y |
| `get_chroma_blocks()` | 像素处理 | 2×2 平均并生成 Cb/Cr 8×8 块 |
| `transform_block()` | 频域变换 | 中心化、二维 DCT、量化和舍入 |
| `get_value_size()` | 系数编码 | 计算有符号值的幅值 Category |
| `get_value_bits()` | 系数编码 | 生成正负数附加位 |
| `encode_block()` | 系数编码 | 编码 DC 差分、AC 游程、ZRL 和 EOB |
| `write_scan_data()` | 图像扫描 | 按 MCU 顺序编码整张图 |
| `write_byte()` | 文件基础 | 写原始字节并记录 I/O 错误 |
| `write_word()` | 文件基础 | 写 JPEG 大端 16 位值 |
| `write_marker()` | 文件基础 | 写两字节 Marker |
| `write_entropy_byte()` | Bit 流 | 写数据字节并执行 `FF 00` 填充 |
| `write_bits()` | Bit 流 | 从高位到低位拼接 Huffman 与附加位 |
| `flush_bits()` | Bit 流 | 用全 1 补齐最后一个字节 |
| `bmp_read_rgb()` | 输入适配 | BMP→顶行优先 RGB |
| `bmp_rgb_image_free()` | 输入适配 | 释放并清空 BMP 读取结果 |
| `parse_quality()` | 独立入口 | 严格解析命令行质量参数 |

## 31. 总结

当前 JPEG 代码已经形成完整的轻量编码闭环：输入适配器统一 RGB 内存布局，编码器按 16×16 MCU 完成 4:2:0 下采样，再对六个 8×8 块执行 DCT、量化和 Huffman 熵编码。

文件层则从 SOI 开始，依次声明 JFIF、量化表、Baseline 4:2:0 帧、标准 Huffman 表和单次扫描，最后处理 Bit 填充并写入 EOI。

这套实现的价值不仅是“能生成 JPEG”，还在于各压缩阶段都由 C 代码直接呈现，数据边界清晰，便于继续研究采样模式、DCT 优化、Huffman 优化和嵌入式图像输出。
