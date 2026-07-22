# TTF 字模数据提取与当前代码实现

本项目使用 C 语言直接读取 TrueType 字体文件，在不依赖 FreeType、HarfBuzz 等字体库的情况下，完成字符映射、字形定位、轮廓提取及基础排版数据读取。

本文以旧版 `TTF.pdf` 笔记为基础，并按照当前 `font/ttf.c`、`font/ttf.h` 的实现进行补充。重点说明如何从一个字符的 Unicode 编码得到 Glyph Index，再从字体文件中得到可绘制的轮廓点。绘制、扫描线填充和图片编码不属于本文重点，但会说明字模提取结果如何交给后续模块。

## 1. 整体解析链路

从字符到轮廓点的核心数据链如下：

```text
Unicode 字符
    ↓ cmap
Glyph Index
    ↓ loca
glyf 表中的字形偏移量与长度
    ↓ glyf
轮廓结束点、Flags、压缩坐标
    ↓ 坐标解压与轮廓重建
结构化 point 数组
```

当前代码实际读取六张关键表：

| 表名 | 当前用途 | 主要输出 |
| --- | --- | --- |
| `head` | 获取字体全局参数及 `loca` 偏移格式 | `unitsPerEm`、`indexToLocFormat` |
| `cmap` | 将 Unicode 映射为 Glyph Index | 当前字符对应的字形编号 |
| `loca` | 按 Glyph Index 定位 `glyf` 数据 | 字形偏移量和长度 |
| `glyf` | 保存 TrueType 字形轮廓 | Flags、坐标增量和轮廓点 |
| `hhea` | 获取水平排版的纵向度量及 `hmtx` 项数 | `ascent`、`descent`、`numberOfHMetrics` |
| `hmtx` | 获取每个字形的水平前进宽度 | `advanceWidth` |

`head`、`hhea` 和 `hmtx` 不直接决定轮廓形状，但它们决定统一字框、字符串字距和缩放基准，因此已经成为当前解析流程的一部分。

## 2. 基础类型、字节序与结构体对齐

### 2.1 项目基础整数类型

`ttf.h` 使用固定含义的类型别名表达 TTF 字段宽度：

| 项目类型 | 实际 C 类型 | 位数 | 典型用途 |
| --- | --- | ---: | --- |
| `u8` | `unsigned char` | 8 | 原始字节、Flags、指令流 |
| `s8` | `char` | 8 | 有符号单字节数据 |
| `u16` | `unsigned short` | 16 | 数量、索引、短偏移 |
| `s16` | `short` | 16 | 字体坐标、带符号增量 |
| `u32` | `unsigned int` | 32 | 长度、偏移、Unicode、Glyph Index |
| `s32` | `int` | 32 | 带符号 32 位数据 |
| `u64` | `uint64_t` | 64 | 字体创建和修改时间 |

### 2.2 大端字节序

TTF/SFNT 文件中的多字节整数采用大端字节序。当前开发环境常见的 x86、x64 主机采用小端字节序，因此代码读取 `u16`、`s16`、`u32`、`u64` 后，通过 `convert` 宏完成转换。

四字节表名如 `head`、`cmap` 是字节数组，不进行整数转换。

### 2.3 `#pragma pack(1)`

磁盘结构体位于 `#pragma pack(1)` 范围内，目的是禁止编译器在字段之间插入对齐填充，使 `memcpy` 后的字段位置与文件格式一致。包含动态指针的运行时结构体位于该范围之外，因为它们不是 TTF 文件的直接映像。

## 3. SFNT 文件头与表目录

TrueType 字体使用 SFNT 容器。文件开头是 Offset Table，随后是 `numTables` 个表目录项，再由各目录项指向真正的表数据。

### 3.1 `file_header`：SFNT Offset Table

| 代码字段 | TTF 标准名称 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `version` | `sfntVersion` | `u32` | 4 | TrueType 轮廓通常为 `0x00010000`；若为 `OTTO`，通常表示 CFF 轮廓，不能按 `glyf` 解析 |
| `table_number` | `numTables` | `u16` | 2 | 字体文件包含的表数量 |
| `search_range` | `searchRange` | `u16` | 2 | 不大于 `numTables` 的最大 2 次幂乘 16，用于目录二分查找 |
| `entry_selector` | `entrySelector` | `u16` | 2 | `log2(searchRange / 16)` |
| `range_shift` | `rangeShift` | `u16` | 2 | `numTables * 16 - searchRange` |

当前 `read_ttf_data` 顺序读取全部目录项，并未使用三个二分查找参数；这些字段仍属于标准文件头，必须正确越过。

### 3.2 `table_entry`：表目录项

| 代码字段 | TTF 标准名称 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `tag[4]` | `tableTag` | `char[4]` | 4 | 四字节表名，例如 `head`、`cmap`、`loca`、`glyf` |
| `check_sum` | `checkSum` | `u32` | 4 | 当前表按 32 位大端整数累加得到的校验和 |
| `offset` | `offset` | `u32` | 4 | 表数据相对于整个字体文件开头的字节偏移 |
| `length` | `length` | `u32` | 4 | 表的真实字节长度，不包含对齐填充 |

表通常按 4 字节边界存放。校验和计算可能包含补零后的对齐数据，但目录中的 `length` 仍是实际长度。

### 3.3 `table`：代码中的表缓冲区

该结构不是 TTF 磁盘结构，而是 `read_ttf_data` 读取关键表后建立的运行时对象。

| 代码字段 | 类型 | 含义 | 所有权 |
| --- | --- | --- | --- |
| `name[4]` | `char[4]` | 表名副本 | 结构体自身保存 |
| `length` | `u32` | 表数据长度 | 普通数值 |
| `data` | `u8*` | 独立申请的表数据缓冲区 | 使用完成后必须 `free` |

当前六张表在 `ttf_table` 数组中的位置如下：

| 数组下标 | 表名 | 使用函数 |
| ---: | --- | --- |
| 0 | `head` | `read_head_table` |
| 1 | `cmap` | `read_cmap` |
| 2 | `loca` | `read_loca_table` |
| 3 | `glyf` | `get_glyph_data` |
| 4 | `hhea` | `read_hhea_header` |
| 5 | `hmtx` | `get_advance_width` |

## 4. `head` 表：字体全局信息

`head_table` 对应 TrueType `head` 表的完整标准头部。旧笔记将边界字段写成了无符号数，实际规范和当前代码均使用 `s16`。

### 4.1 `head_table` 字段

| 代码字段 | 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `version` | `majorVersion/minorVersion` | `u32` | 4 | `head` 表版本，通常为 `0x00010000` |
| `font_revision` | `fontRevision` | `u32` | 4 | 16.16 定点格式的字体修订版本 |
| `check_sum_adjustment` | `checksumAdjustment` | `u32` | 4 | 用于使整个字体校验和满足标准常量 |
| `magic_number` | `magicNumber` | `u32` | 4 | 固定为 `0x5F0F3CF5` |
| `flags` | `flags` | `u16` | 2 | 字体基线、缩放、整数坐标等全局标志 |
| `unit_per_Em` | `unitsPerEm` | `u16` | 2 | 一个 Em 在字体设计坐标中的单位数，通常为 512、1024 或 2048 |
| `created` | `created` | `u64` | 8 | 从 1904-01-01 00:00:00 起计算的创建时间 |
| `modified` | `modified` | `u64` | 8 | 从同一纪元起计算的最后修改时间 |
| `xMin` | `xMin` | `s16` | 2 | 所有字形全局包围盒最小 X |
| `yMin` | `yMin` | `s16` | 2 | 所有字形全局包围盒最小 Y |
| `xMax` | `xMax` | `s16` | 2 | 所有字形全局包围盒最大 X |
| `yMax` | `yMax` | `s16` | 2 | 所有字形全局包围盒最大 Y |
| `mac_style` | `macStyle` | `u16` | 2 | 粗体、斜体、下划线、轮廓等样式位 |
| `lowest_RecPPRM` | `lowestRecPPEM` | `u16` | 2 | 建议仍可辨认的最小 Pixels Per Em |
| `font_direction_hint` | `fontDirectionHint` | `s16` | 2 | 历史方向提示字段，现代字体中已弃用 |
| `index_to_loca_format` | `indexToLocFormat` | `s16` | 2 | 0 表示 `loca` 短格式，1 表示长格式 |
| `glyph_data_format` | `glyphDataFormat` | `s16` | 2 | TrueType `glyf` 当前要求为 0 |

当前代码主要使用 `unit_per_Em` 和 `index_to_loca_format`。`unit_per_Em` 参与建立统一横向字框和缩放比例，`index_to_loca_format` 决定 `loca` 的解析方式。

## 5. `hhea` 与 `hmtx`：水平排版度量

旧笔记只关注轮廓提取，但当前字符串排版还需要统一纵向字框和每个字形的水平前进宽度。

### 5.1 当前 `hhea_header` 结构

| 代码字段 | 类型 | 字节数 | 含义 |
| --- | --- | ---: | --- |
| `version` | `u32` | 4 | `hhea` 表版本，通常为 `0x00010000` |
| `ascent` | `s16` | 2 | 推荐的水平排版上边界 |
| `descent` | `s16` | 2 | 推荐的水平排版下边界，通常为负数 |
| `line_gap` | `s16` | 2 | 建议额外行间距 |
| `other_data[24]` | `u8[24]` | 24 | 当前解析不使用的中间标准字段 |
| `number_of_h_metrics` | `u16` | 2 | `hmtx` 中完整水平度量项的数量 |

### 5.2 `other_data[24]` 对应的标准字段

当前代码将这些字段整体跳过，但理解表布局时仍需要明确其内容。

| 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | ---: | --- |
| `advanceWidthMax` | `u16` | 2 | 所有字形中最大的前进宽度 |
| `minLeftSideBearing` | `s16` | 2 | 最小左侧边距 |
| `minRightSideBearing` | `s16` | 2 | 最小右侧边距 |
| `xMaxExtent` | `s16` | 2 | 最大水平范围，通常为 `lsb + (xMax - xMin)` |
| `caretSlopeRise` | `s16` | 2 | 文本光标斜率的纵向分量 |
| `caretSlopeRun` | `s16` | 2 | 文本光标斜率的横向分量 |
| `caretOffset` | `s16` | 2 | 倾斜字体的光标偏移 |
| `reserved[4]` | `s16[4]` | 8 | 保留字段，必须为 0 |
| `metricDataFormat` | `s16` | 2 | 当前要求为 0 |

当前统一字框通过以下字段建立：

```text
xMin = 0
xMax = head.unitsPerEm
yMin = hhea.descent
yMax = hhea.ascent
```

因此缩放功能输入的目标字高对应 `ascent - descent` 缩放后的像素高度，而不是某个字形着色像素的紧包围盒高度。

### 5.3 `hmtx` 表布局

`hmtx` 没有独立表头，其长度和分段方式由 `hhea.numberOfHMetrics` 与字体字形总数共同决定。

完整水平度量项 `longHorMetric` 的结构为：

| 字段 | 类型 | 字节数 | 含义 |
| --- | --- | ---: | --- |
| `advanceWidth` | `u16` | 2 | 排版笔移动到下一个字形原点的水平距离 |
| `leftSideBearing` | `s16` | 2 | 字形包围盒左边缘相对当前原点的距离 |

当 Glyph Index 小于 `numberOfHMetrics` 时，可以直接读取对应的 `longHorMetric`。其后的字形共用最后一个 `advanceWidth`，但各自仍可能有独立的 `leftSideBearing`：

| 尾部字段 | 类型 | 数量 | 含义 |
| --- | --- | --- | --- |
| `leftSideBearing[]` | `s16[]` | `numGlyphs - numberOfHMetrics` | 后续字形各自的左侧边距 |

当前 `get_advance_width` 只读取 `advanceWidth`，不使用 `leftSideBearing`。当 Glyph Index 超出完整度量项数量时，代码复用最后一个 `advanceWidth`，符合 `hmtx` 的宽度继承规则。

## 6. `cmap`：Unicode 到 Glyph Index

`cmap` 表由一个简短表头、若干子表目录项和实际映射子表组成。当前代码支持 Format 12 和 Format 4，并优先使用 Format 12。

### 6.1 `cmap` 表头

当前代码直接用指针读取这两个字段，没有单独定义 C 结构体。

| 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | ---: | --- |
| `version` | `u16` | 2 | `cmap` 表版本，当前为 0 |
| `numTables` | `u16` | 2 | 后续编码记录的数量 |

### 6.2 `cmap_subtable_header`：编码记录

| 代码字段 | 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `platform_id` | `platformID` | `u16` | 2 | 平台编号，例如 0 为 Unicode，3 为 Windows |
| `encoding_id` | `encodingID` | `u16` | 2 | 平台内部的编码编号 |
| `offset` | `subtableOffset` | `u32` | 4 | 子表相对于 `cmap` 表开头的偏移 |

当前代码重点识别的 Windows 组合如下：

| Platform ID | Encoding ID | 常见含义 | 当前用途 |
| ---: | ---: | --- | --- |
| 3 | 10 | Unicode full repertoire，32 位字符范围 | Format 12 首选记录 |
| 3 | 1 | Unicode BMP，基本多文种平面 | Format 4 首选记录 |

`get_cmap_subtable_data` 会先根据子表自身的 `format` 查找目标格式，并优先选取上述 Windows 记录；如果没有首选组合，则保留第一个相同 Format 的记录作为候选。

### 6.3 `cmap_data`：当前活动映射

该结构是运行时对象，不是磁盘表头。它保证当前只解析并保存一种 Format。

| 代码字段 | 类型 | 含义 |
| --- | --- | --- |
| `format` | `u16` | 当前活动格式，取 12、4 或失败时的 0 |
| `segment_count` | `u16` | Format 4 的 Segment 数量；Format 12 不使用 |
| `data.format4` | `cmap_format4_data` | 当 `format == 4` 时有效 |
| `data.format12` | `cmap_format12_data` | 当 `format == 12` 时有效 |

`data` 是联合体，因此 Format 12 存在时不会继续解析或缓存 Format 4。`free_cmap` 也只释放当前活动格式所申请的数组。

### 6.4 Format 选择顺序

`read_cmap` 的选择顺序为：

```text
查找 Format 12
    ├─ 找到：解析 Format 12 并立即返回 12
    └─ 未找到：继续查找 Format 4
                  ├─ 找到：解析 Format 4 并返回 4
                  └─ 未找到：返回 0
```

Format 12 能覆盖 BMP 和辅助平面，因此选择 Format 12 后，BMP 中文同样通过 Format 12 查找，不再依赖 Format 4。

## 7. `cmap` Format 4

Format 4 使用按字符编码排序的 Segment 压缩 BMP 范围映射。

### 7.1 `cmap_format4_header`

| 代码字段 | 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `format` | `format` | `u16` | 2 | 固定为 4 |
| `length` | `length` | `u16` | 2 | 整个 Format 4 子表的字节长度 |
| `language` | `language` | `u16` | 2 | 语言代码，Unicode 映射通常为 0 |
| `segment_count_X2` | `segCountX2` | `u16` | 2 | Segment 数量乘 2 |
| `search_range` | `searchRange` | `u16` | 2 | `2 * 2^floor(log2(segCount))` |
| `entry_selector` | `entrySelector` | `u16` | 2 | `log2(searchRange / 2)` |
| `range_shift` | `rangeShift` | `u16` | 2 | `2 * segCount - searchRange` |

头部之后的变长数组按固定顺序排列：

| 磁盘字段 | 类型 | 数量 | 含义 |
| --- | --- | --- | --- |
| `endCode[]` | `u16[]` | `segCount` | 每个 Segment 的结束字符编码 |
| `reservedPad` | `u16` | 1 | 保留值，必须为 0 |
| `startCode[]` | `u16[]` | `segCount` | 每个 Segment 的开始字符编码 |
| `idDelta[]` | `s16[]` | `segCount` | 字形编号增量；代码以 `u16` 保存并进行模 65536 运算 |
| `idRangeOffset[]` | `u16[]` | `segCount` | 指向字形索引数据的相对偏移 |
| `glyphIdArray[]` | `u16[]` | 变长 | 不能只用增量表达时的显式 Glyph Index |

最后一个哨兵 Segment 通常以 `0xFFFF` 结束，用于保证查找能够终止。

### 7.2 `cmap_format4_data`：解析后的数组

| 代码字段 | 类型 | 含义 | 内存管理 |
| --- | --- | --- | --- |
| `end_code` | `u16*` | `endCode[]` 的独立数组 | `free_cmap` 释放 |
| `start_code` | `u16*` | `startCode[]` 的独立数组 | `free_cmap` 释放 |
| `id_delta` | `u16*` | `idDelta[]` 的独立数组 | `free_cmap` 释放 |
| `id_range_offset` | `u16*` | `idRangeOffset[]` 的独立数组 | `free_cmap` 释放 |
| `glyph_index_array` | `u16*` | `glyphIdArray[]` 的独立数组 | `free_cmap` 释放 |
| `glyph_index_array_length` | `u16` | `glyph_index_array` 的元素数量 | 用于查询边界检查 |

### 7.3 Format 4 查询

首先查找满足以下条件的 Segment：

```text
startCode[i] <= unicode <= endCode[i]
```

当 `idRangeOffset[i] == 0` 时：

```text
glyphIndex = (unicode + idDelta[i]) mod 65536
```

当 `idRangeOffset[i] != 0` 时，当前代码已经把各数组拆开保存，因此需要计算相对于 `glyph_index_array` 开头的下标：

```text
offset = idRangeOffset[i] / 2
       + unicode - startCode[i]
       + i - segmentCount

glyphIndex = glyphIndexArray[offset]
```

如果数组中的 `glyphIndex` 非 0，还需要执行：

```text
glyphIndex = (glyphIndex + idDelta[i]) mod 65536
```

Glyph Index 0 表示字体的缺失字形 `.notdef`。

## 8. `cmap` Format 12

Format 12 使用 32 位字符编码，通过若干连续映射组覆盖完整 Unicode 范围。

### 8.1 `cmap_format12_data`

该结构既保存磁盘头部字段，也保存解析后申请的分组数组指针。

| 代码字段 | 标准字段 | 类型 | 字节数/性质 | 含义 |
| --- | --- | --- | --- | --- |
| `format` | `format` | `u16` | 2 | 固定为 12 |
| `reserved` | `reserved` | `u16` | 2 | 保留字段，必须为 0 |
| `length` | `length` | `u32` | 4 | 整个 Format 12 子表的字节长度 |
| `language` | `language` | `u32` | 4 | 语言代码，通常为 0 |
| `groups_number` | `numGroups` | `u32` | 4 | 连续映射组数量 |
| `groups` | - | `sequential_map_group*` | 运行时指针 | 解析后保存全部分组，由 `free_cmap` 释放 |

### 8.2 `sequential_map_group`

| 代码字段 | 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `start_char_code` | `startCharCode` | `u32` | 4 | 当前组的起始 Unicode |
| `end_char_code` | `endCharCode` | `u32` | 4 | 当前组的结束 Unicode |
| `start_glyph_id` | `startGlyphID` | `u32` | 4 | 起始 Unicode 对应的 Glyph Index |

当 Unicode 位于某个组内时：

```text
glyphIndex = startGlyphID + (unicode - startCharCode)
```

分组按字符编码递增排列，当前 `get_glyph_index_format12` 使用二分查找定位分组。若没有任何分组覆盖目标 Unicode，则返回 Glyph Index 0。

## 9. `loca`：Glyph Index 到字形偏移

`loca` 没有独立表头，本质上是一组相对于 `glyf` 表开头的偏移量。数组元素格式由 `head.indexToLocFormat` 决定。

| `indexToLocFormat` | 磁盘元素类型 | 磁盘值含义 | 真实偏移计算 |
| ---: | --- | --- | --- |
| 0 | `u16` | 真实偏移量除以 2 | `offset = value * 2` |
| 1 | `u32` | 真实字节偏移量 | `offset = value` |

Glyph Index 对应字形的数据范围由连续两个元素确定：

```text
glyphOffset = loca[glyphIndex]
nextOffset  = loca[glyphIndex + 1]
glyphLength = nextOffset - glyphOffset
```

因此 `loca` 元素数量通常是字体字形数量加 1。`glyphLength == 0` 表示该字形没有独立轮廓数据，例如空格可能出现这种情况。

需要特别注意：`glyphOffset` 是相对于 `glyf` 表开头的偏移，而不是相对于整个字体文件的偏移。

当前实现注意事项：TrueType 规范要求短格式乘 2；当前 `read_loca_table` 的短格式分支仍将读取值直接写入 `u32` 数组。若使用 `indexToLocFormat == 0` 的字体，应在代码中补上乘 2 处理。

## 10. `glyf`：字形轮廓数据

`glyf` 表没有统一表头，而是由多个字形数据块连续组成。`loca` 提供每个数据块的开始位置和结束位置。

### 10.1 `glyph_data_header`：字形公共头部

| 代码字段 | 标准字段 | 类型 | 字节数 | 含义 |
| --- | --- | --- | ---: | --- |
| `number_of_contours` | `numberOfContours` | `s16` | 2 | 大于等于 0 表示简单字形；小于 0 表示复合字形 |
| `x_min` | `xMin` | `s16` | 2 | 当前字形包围盒最小 X |
| `y_min` | `yMin` | `s16` | 2 | 当前字形包围盒最小 Y |
| `x_max` | `xMax` | `s16` | 2 | 当前字形包围盒最大 X |
| `y_max` | `yMax` | `s16` | 2 | 当前字形包围盒最大 Y |

字形包围盒是当前 Glyph 自己的紧边界；它不同于 `hhea.ascent/descent` 建立的统一排版框。

### 10.2 简单字形数据布局

当 `numberOfContours >= 0` 时，公共头部之后按以下顺序存放：

| 磁盘字段 | 类型 | 数量/长度 | 含义 |
| --- | --- | --- | --- |
| `endPtsOfContours[]` | `u16[]` | `numberOfContours` | 每条轮廓最后一个点的下标 |
| `instructionLength` | `u16` | 1 | TrueType Hinting 指令字节数 |
| `instructions[]` | `u8[]` | `instructionLength` | 字形提示指令 |
| `flags[]` | `u8[]` | 变长 | 点属性及坐标压缩方式，可使用 Repeat 压缩 |
| `xCoordinates[]` | 变长字节流 | 由 Flags 决定 | 所有点的 X 坐标增量 |
| `yCoordinates[]` | 变长字节流 | 由 Flags 决定 | 所有点的 Y 坐标增量 |

最后一个轮廓结束下标可以确定总点数：

```text
pointCount = endPtsOfContours[numberOfContours - 1] + 1
```

当前实现会读取并跳过 Hinting 指令，但不会执行 TrueType 字节码解释器。

### 10.3 简单字形 Flag

| 位 | 标准名称 | 值为 1 时的含义 | 值为 0 时的含义 |
| ---: | --- | --- | --- |
| 0 | `ON_CURVE_POINT` | 当前点是曲上点 | 当前点是二次贝塞尔曲外控制点 |
| 1 | `X_SHORT_VECTOR` | X 增量占 1 字节 | 结合位 4，X 增量为 0 或有符号 2 字节 |
| 2 | `Y_SHORT_VECTOR` | Y 增量占 1 字节 | 结合位 5，Y 增量为 0 或有符号 2 字节 |
| 3 | `REPEAT_FLAG` | 下一字节表示该 Flag 额外重复次数 | 当前 Flag 只描述一个点 |
| 4 | `X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR` | 位 1 为 1 时表示正增量；位 1 为 0 时表示 X 不变 | 位 1 为 1 时表示负增量；位 1 为 0 时读取有符号 2 字节增量 |
| 5 | `Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR` | 位 2 为 1 时表示正增量；位 2 为 0 时表示 Y 不变 | 位 2 为 1 时表示负增量；位 2 为 0 时读取有符号 2 字节增量 |
| 6 | `OVERLAP_SIMPLE` | 轮廓可能重叠，供栅格器参考 | 未声明重叠 |
| 7 | `RESERVED` | 保留 | 保留 |

旧笔记将位 6-7 都写作保留位。较新的 OpenType/TrueType 定义中，位 6 可表示简单字形轮廓重叠；当前代码没有使用该位。

### 10.4 Flags 展开和坐标解压

Flags 是变长流。若设置 Repeat 位，下一字节不是下一个点的 Flag，而是当前 Flag 还要额外重复的次数。因此必须先展开到“一点一个 Flag”，才能准确找到 X 坐标流的开始位置。

坐标是相对于前一个点的增量：

```text
x[i] = x[i - 1] + deltaX[i]
y[i] = y[i - 1] + deltaY[i]
```

第一个点以 `(0, 0)` 为累计起点。短向量只有 1 字节，符号由位 4 或位 5 决定；长向量是大端有符号 16 位整数；“Same”表示当前方向增量为 0。

当前实现注意事项：带 Repeat 的 Flag 流本身长度不等于点数。解析时应扫描变长 Flag 流后再确定 X 流起点，并对所有读取位置进行 `glyph_length` 边界检查。当前 `glyph_to_point` 已处理 Repeat 语义，但相关长度推导和异常输入检查仍可继续加强。

### 10.5 `point`：解压后的轮廓点

该结构是项目运行时表示，不是 TTF 磁盘结构。

| 代码字段 | 类型 | 含义 |
| --- | --- | --- |
| `x` | `float` | 解压并累计后的字体设计坐标 X |
| `y` | `float` | 解压并累计后的字体设计坐标 Y |
| `locate` | `int` | 0 表示曲外点，1 表示曲上点，`NEXT` 表示当前轮廓结束 |

`NEXT` 是项目自定义标记，用于让绘制模块区分多条闭合轮廓。当前表示把“曲上/曲外状态”和“轮廓结束状态”合并在同一字段中，后续若增强复合轮廓处理，可以拆成独立字段。

### 10.6 隐式曲上点与闭合轮廓

TrueType 使用直线和二次贝塞尔曲线：

- 相邻两个曲上点之间是直线；
- 曲上点、曲外点、曲上点组成二次贝塞尔曲线；
- 两个连续曲外点之间存在一个隐式曲上点，坐标是两点中点；
- 每条轮廓首尾相连，最后一个点必须连接回第一个点。

`process_point` 遍历原始点数组，在连续曲外点之间插入中点，生成绘制模块可以直接消费的新数组。

### 10.7 复合字形布局

当 `numberOfContours < 0` 时，字形由一个或多个已有 Glyph 组合而成。当前代码尚未解析复合字形，但标准数据通常包含以下字段：

| 字段 | 类型 | 是否重复 | 含义 |
| --- | --- | --- | --- |
| `flags` | `u16` | 每个组件一项 | 决定参数宽度、参数含义、变换类型及是否还有组件 |
| `glyphIndex` | `u16` | 每个组件一项 | 被引用的基础 Glyph Index |
| `argument1` | `s8/s16` 或 `u8/u16` | 每个组件一项 | X/Y 偏移或父子锚点编号 |
| `argument2` | `s8/s16` 或 `u8/u16` | 每个组件一项 | Y 偏移或第二个锚点编号 |
| `scale` | F2Dot14 | 可选 | X、Y 使用相同缩放 |
| `xScale/yScale` | F2Dot14 | 可选 | X、Y 分别缩放 |
| `scale01/scale10` | F2Dot14 | 可选 | 二维变换矩阵的非对角元素 |
| `instructionLength` | `u16` | 可选一次 | 复合字形 Hinting 指令长度 |
| `instructions[]` | `u8[]` | 可选一次 | 复合字形 Hinting 指令 |

完整支持复合字形需要递归读取被引用 Glyph，将组件坐标应用偏移和仿射变换后合并轮廓，并防止循环引用或递归过深。

## 11. 字形缓存与统一字框

### 11.1 `glyph_point_data`

虽然名称包含 `point`，当前结构缓存的实际上是尚未解压的单个 `glyf` 原始数据及排版宽度。

| 代码字段 | 类型 | 含义 | 所有权 |
| --- | --- | --- | --- |
| `unicode` | `u32` | 此缓存项对应的 Unicode | 普通数值 |
| `glyph_data` | `u8*` | 从 `glyf` 复制出的单字形原始数据 | 缓存销毁时释放 |
| `glyph_length` | `int` | `glyph_data` 的字节长度 | 普通数值 |
| `advance_width` | `u16` | 从 `hmtx` 得到的水平前进宽度 | 普通数值 |

当前 `load_ttf_BMP` 会预加载 `U+4E00` 到 `U+9FA4` 范围，并为每个字符保存一项。这样绘制时查询直接，但不适合内存受限设备；轻量显示场景更适合改为按需加载和小容量字形缓存。

### 11.2 `font_box`

该结构是统一排版框，不是单字形紧包围盒。

| 代码字段 | 类型 | 当前来源 | 含义 |
| --- | --- | --- | --- |
| `x_min` | `int` | 固定为 0 | 统一字框左边界 |
| `y_min` | `int` | `hhea.descent` | 统一字框下边界 |
| `x_max` | `int` | `head.unitsPerEm` | 统一字框右边界 |
| `y_max` | `int` | `hhea.ascent` | 统一字框上边界 |

单字和字符串都以该字框进行中心定位。缩放时使用：

```text
scale = targetHeight / (fontBox.yMax - fontBox.yMin)
```

随后轮廓点和 `advanceWidth` 使用同一比例，保证字形比例、字符串字距和基线一致。

## 12. 当前代码的完整执行顺序

`load_ttf_BMP` 的主要阶段如下：

| 阶段 | 输入 | 处理 | 输出 |
| ---: | --- | --- | --- |
| 1 | 字体文件路径 | `read_ttf_data` 读取六张关键表 | `table[6]` |
| 2 | `head` 数据 | 转换全部多字节字段 | `head_table` |
| 3 | `hhea` 数据 | 读取 `ascent/descent/numberOfHMetrics` | `hhea_header` 与 `font_box` |
| 4 | `cmap` 数据 | 优先 Format 12，否则 Format 4 | `cmap_data` |
| 5 | `loca` 数据 | 按短格式或长格式生成统一 `u32` 偏移数组 | `loca_array` |
| 6 | Unicode | 通过当前 `cmap` 查询 | Glyph Index |
| 7 | Glyph Index | 读取相邻两个 `loca` 偏移 | `glyphOffset`、`glyphLength` |
| 8 | `glyf` 数据 | 复制当前字形原始数据 | `glyph_data` |
| 9 | Glyph Index 与 `hmtx` | 读取或继承 `advanceWidth` | 水平前进宽度 |
| 10 | 所有中文缓存项 | 保存 Unicode、原始字形与宽度 | `glyph_point_data[]` |

真正绘制某个字符时，`glyph_to_point` 才会把该缓存项的 `glyph_data` 解压成 `point[]`。

## 13. 函数与解析阶段对应关系

| 函数 | 所属阶段 | 作用 |
| --- | --- | --- |
| `read_ttf_data` | SFNT | 读取文件头、表目录和六张关键表 |
| `read_head_table` | `head` | 转换字体全局信息 |
| `read_hhea_header` | `hhea` | 提取统一纵向度量和水平度量项数 |
| `get_advance_width` | `hmtx` | 获取 Glyph 的水平前进宽度 |
| `get_cmap_subtable_data` | `cmap` | 按目标 Format 选择并复制子表 |
| `read_cmap_format4_subtable` | `cmap` Format 4 | 读取 Segment 数组 |
| `get_glyph_index_format4` | `cmap` Format 4 | 计算 BMP 字符的 Glyph Index |
| `read_cmap_format12_subtable` | `cmap` Format 12 | 读取连续映射组 |
| `get_glyph_index_format12` | `cmap` Format 12 | 二分查找完整 Unicode 映射 |
| `read_cmap` | `cmap` | 优先 Format 12，缺失时回退 Format 4 |
| `get_glyph_index` | `cmap` | 根据当前活动格式统一查询 |
| `free_cmap` | `cmap` | 只释放实际解析的格式数据 |
| `read_loca_table` | `loca` | 生成统一的 `u32` 偏移数组 |
| `get_glyph_data` | `glyf` | 复制单个字形的原始数据 |
| `glyph_to_point` | `glyf` | 读取头部、Flags 和压缩坐标 |
| `process_point` | 轮廓重建 | 插入连续曲外点之间的隐式中点 |
| `load_ttf_BMP` | 总入口 | 预加载中文范围字形和排版宽度 |

## 14. 当前支持范围与后续完善方向

| 项目 | 当前状态 | 后续方向 |
| --- | --- | --- |
| SFNT TrueType | 支持包含 `glyf/loca` 的字体 | 检查 `sfntVersion`，区分 CFF/OpenType |
| `cmap` Format 12 | 支持并优先使用 | 增加子表长度、偏移和分组有序性检查 |
| `cmap` Format 4 | 支持回退 | 增加畸形 Segment 和数组边界检查 |
| `loca` 长格式 | 支持 | 校验最后偏移不超过 `glyf` 长度 |
| `loca` 短格式 | 解析框架存在 | 按规范将读取值乘 2 |
| 简单字形 | 支持基础 Flags、坐标与隐式点 | 完善 Repeat 流边界及有符号长增量处理 |
| 复合字形 | 尚未支持 | 解析组件 Glyph、偏移和 F2Dot14 仿射变换 |
| Hinting | 只读取并跳过 | 若面向小字号显示，可实现解释器或明确禁用 |
| `hmtx.advanceWidth` | 支持 | 如需精确边界，继续使用 `leftSideBearing` |
| 字形加载 | 预加载固定中文范围 | 改为按需读取和 LRU/定长缓存 |
| 错误处理 | 以调试输出为主 | 增加文件长度、表存在性、申请失败和越界返回值 |

## 15. 模块边界

字模提取的核心源码位于：

```text
font/
├── ttf.c          TTF/SFNT 解析、字形定位和轮廓解压
├── ttf.h          公共字形、轮廓点和字框结构
├── font_draw.c    轮廓绘制、实心填充、字符串排版和缩放
├── font_draw.h    绘制模块公共接口
├── encoding.c     UTF-8、GBK 与 Unicode 转换
└── encoding.h     编码模块公共接口
```

`ttf` 模块的输出是 `glyph_point_data[]`、`font_box` 和按需解压得到的 `point[]`；`font_draw` 模块只消费这些结果，不需要理解 `cmap`、`loca` 或 `glyf` 的磁盘布局。这一边界便于后续将 BMP 输出替换为真实显示器 FrameBuffer，而不改动字体解析主链路。
