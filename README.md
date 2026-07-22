# TTF 字模数据提取

本项目使用 C 语言直接读取 TTF 字体文件，在不依赖字体解析库的情况下，完成字符映射、字形定位和轮廓数据提取。

本文仅整理 TTF 字模数据的提取思路，重点说明如何从一个字符的 Unicode 编码得到对应的字形轮廓点。绘制、排版、缩放和图片编码将在后续继续补充。

## 解析目标

TTF 文件解析的核心任务，可以概括为下面这条数据链：

```text
Unicode 字符
    ↓ cmap
Glyph Index
    ↓ loca
字形在 glyf 表中的偏移量和长度
    ↓ glyf
轮廓、标志和压缩坐标
    ↓ 轮廓重建
结构化字形轮廓点
```

字模提取主要读取以下四张表：

| 表名 | 作用 |
| --- | --- |
| `head` | 获取字体全局信息以及 `loca` 表的偏移格式 |
| `cmap` | 将 Unicode 编码映射为 Glyph Index |
| `loca` | 根据 Glyph Index 定位 `glyf` 中的字形数据 |
| `glyf` | 保存字形边界、轮廓、标志和压缩坐标 |

完成单个字形提取的主链路是：

```text
head → cmap → loca → glyf
```

## 1. 读取 TTF 文件头和表目录

TTF 使用 SFNT 容器组织数据。文件开头是一个全局文件头，随后是一组表目录项。

文件头中比较重要的字段包括：

| 字段 | 作用 |
| --- | --- |
| `version` | SFNT 版本 |
| `numTables` | 字体文件中包含的表数量 |
| `searchRange` | 表目录二分查找参数 |
| `entrySelector` | 表目录二分查找参数 |
| `rangeShift` | 表目录二分查找参数 |

每个表目录项包含：

| 字段 | 作用 |
| --- | --- |
| `tag[4]` | 四字节表名，例如 `head`、`cmap`、`loca`、`glyf` |
| `checkSum` | 表校验和 |
| `offset` | 表数据相对于文件开头的偏移量 |
| `length` | 表数据长度 |

解析时先读取全部目录项，再根据四字节表名找到需要的表，通过 `offset` 和 `length` 将表数据读取到独立缓冲区。

### 字节序处理

TTF 中的多字节整数使用大端字节序。常见的 x86 和 x64 环境使用小端字节序，因此读取 `u16`、`s16`、`u32`、`u64` 等字段后，需要先转换字节序，再参与长度、偏移和坐标计算。

四字节表名本身是字符数组，不需要进行字节序转换。

## 2. 解析 `head` 表

`head` 表保存字体的全局信息。本项目主要使用以下字段：

| 字段 | 类型 | 作用 |
| --- | --- | --- |
| `magicNumber` | `u32` | 固定值 `0x5F0F3CF5`，可用于检查表格式 |
| `unitsPerEm` | `u16` | 字体设计坐标系中一个 Em 的单位数 |
| `xMin`、`yMin` | `s16` | 字体全局边界的左下角 |
| `xMax`、`yMax` | `s16` | 字体全局边界的右上角 |
| `indexToLocFormat` | `s16` | 指定 `loca` 使用短偏移还是长偏移 |
| `glyphDataFormat` | `s16` | 当前 TrueType 字形数据格式应为 0 |

`unitsPerEm` 描述的是字体内部坐标单位，并不代表最终像素大小。解析得到轮廓后，可以根据目标字号建立“字体单位到像素”的缩放关系。

`indexToLocFormat` 决定下一步如何读取 `loca`：

- `0`：短偏移，每项为 `u16`，实际偏移量为读取值乘以 2。
- `1`：长偏移，每项为 `u32`，读取值就是实际偏移量。

## 3. 通过 `cmap` 将 Unicode 映射为 Glyph Index

`cmap` 表负责回答一个问题：某个 Unicode 字符在当前字体中对应哪个字形？

`cmap` 由表头、子表目录和多个映射子表组成：

```text
cmap header
cmap subtable records[]
cmap subtables[]
```

每个子表目录项由 `platformID`、`encodingID` 和 `offset` 组成。`platformID` 与 `encodingID` 共同说明该子表使用的字符编码范围，`offset` 指向实际映射数据。

本项目解析 `Format 4` 和 `Format 12` 两种子表。

### Format 4

Format 4 面向基本多文种平面，使用多个 Segment 压缩字符映射。每个 Segment 由下面四项共同描述：

```text
startCode[i]
endCode[i]
idDelta[i]
idRangeOffset[i]
```

查找步骤如下：

1. 找到满足 `startCode[i] <= unicode <= endCode[i]` 的 Segment。
2. 如果没有匹配区间，则返回 Glyph Index 0，表示缺失字形。
3. 如果 `idRangeOffset[i] == 0`，直接计算：

```text
glyphIndex = (unicode + idDelta[i]) mod 65536
```

4. 如果 `idRangeOffset[i] != 0`，则利用当前 Segment 的范围偏移，在 `glyphIndexArray` 中找到对应项。
5. 从 `glyphIndexArray` 取出的值非 0 时，还需要结合该 Segment 的 `idDelta` 得到最终 Glyph Index。

Format 4 的最后一个 `endCode` 通常为 `0xFFFF`，用于保证分段搜索能够结束。

代码中将 `idRangeOffset` 数组和 `glyphIndexArray` 分别保存，因此需要显式计算目标项相对于 `glyphIndexArray` 开头的下标，而不是依赖数组在原始文件中的连续地址进行越界访问。

### Format 12

Format 12 使用 32 位字符编码，可以描述基本多文种平面以外的字符。它由若干连续映射组组成：

```text
startCharCode
endCharCode
startGlyphID
```

当 Unicode 落在某个分组内时，Glyph Index 的计算方式为：

```text
glyphIndex = startGlyphID + (unicode - startCharCode)
```

因此 Format 12 的解析重点是读取全部映射组，并找到包含目标 Unicode 的区间。

## 4. 通过 `loca` 定位字形数据

`loca` 本质上是字形偏移数组。`cmap` 得到的 Glyph Index 可以直接作为数组下标。

一个字形需要连续两个偏移量才能确定数据范围：

```text
glyphOffset = loca[glyphIndex]
nextOffset  = loca[glyphIndex + 1]
glyphLength = nextOffset - glyphOffset
```

如果 `glyphLength` 为 0，说明该字形没有独立轮廓数据，例如空格可能出现这种情况。

根据 `head.indexToLocFormat` 处理偏移：

- 短格式读取 `u16`，并将值乘以 2。
- 长格式读取 `u32`，直接作为真实偏移。

最终的 `glyphOffset` 是相对于 `glyf` 表开头的偏移量，不是相对于整个 TTF 文件的偏移量。

## 5. 从 `glyf` 读取字形

`glyf` 表由多个字形数据块组成。通过 `loca` 得到偏移和长度后，即可复制目标字形的数据。

每个字形首先包含一个公共头部：

| 字段 | 类型 | 作用 |
| --- | --- | --- |
| `numberOfContours` | `s16` | 正数表示简单字形，负数表示复合字形 |
| `xMin`、`yMin` | `s16` | 当前字形边界左下角 |
| `xMax`、`yMax` | `s16` | 当前字形边界右上角 |

当前解析流程主要处理简单字形。简单字形头部之后的数据顺序为：

```text
endPtsOfContours[]
instructionLength
instructions[]
flags[]
xCoordinates[]
yCoordinates[]
```

### 确定轮廓和点数

`endPtsOfContours` 保存每条轮廓最后一个点的下标。因此：

```text
pointCount = endPtsOfContours[numberOfContours - 1] + 1
```

`instructionLength` 给出 TrueType Hinting 指令的字节数。本项目目前跳过指令内容，继续读取后面的标志和坐标数据。

### 展开 Flags

每个点对应一个 Flag，Flag 决定该点是否位于曲线上，以及 x、y 坐标各占多少字节。

| 位 | 名称 | 作用 |
| --- | --- | --- |
| 0 | On Curve | 1 表示曲上点，0 表示曲外控制点 |
| 1 | X Short Vector | x 坐标增量使用 1 字节 |
| 2 | Y Short Vector | y 坐标增量使用 1 字节 |
| 3 | Repeat | 下一字节表示当前 Flag 还要重复多少次 |
| 4 | X Is Same / Positive X Short | 与位 1 组合，表示 x 增量的符号或 x 不变 |
| 5 | Y Is Same / Positive Y Short | 与位 2 组合，表示 y 增量的符号或 y 不变 |
| 6-7 | Reserved | 保留位 |

由于 Repeat 可以压缩连续相同的 Flag，文件中的 Flag 字节数不一定等于点数。解析坐标前必须先展开重复标志，才能确定 x 坐标流和 y 坐标流的起始位置。

### 还原相对坐标

TTF 保存的是相对于前一个点的坐标增量，而不是每个点的绝对坐标。解析时需要根据 Flag 判断增量的长度和符号，再逐点累加：

```text
x[i] = x[i - 1] + deltaX[i]
y[i] = y[i - 1] + deltaY[i]
```

第一个点以坐标原点为累计起点。完成累加后，才能得到字体设计坐标系中的真实轮廓点。

## 6. 重建可绘制轮廓

一个字形可以包含多条闭合轮廓，例如汉字外框、内部笔画和孔洞。`endPtsOfContours` 用于划分这些轮廓，每条轮廓的最后一个点还需要与第一个点闭合。

TrueType 轮廓使用直线和二次贝塞尔曲线：

- 两个相邻曲上点之间绘制直线。
- 曲上点、曲外点、曲上点组成一段二次贝塞尔曲线。
- 两个连续曲外点之间存在一个隐式曲上点，其坐标是两个曲外点的中点。

因此，在输出字模数据前，需要遍历原始点数组，在连续曲外点之间补入中点，并保留每条轮廓的结束标记。至此，压缩的 `glyf` 数据已经被还原成结构化轮廓点数组，后续绘制逻辑可以直接使用该数组。

## 7. 项目中的实现对应关系

| 解析阶段 | 主要函数 |
| --- | --- |
| 读取文件头、表目录和关键表 | `read_ttf_data` |
| 解析字体全局信息 | `read_head_table` |
| 选择并读取 cmap 子表 | `get_cmap_subtable_data`、`read_cmap` |
| 解析 Format 4 | `read_cmap_format4_subtable`、`get_glyph_index_format4` |
| 解析 Format 12 | `read_cmap_format12_subtable`、`get_glyph_index_format12` |
| 解析 loca 偏移数组 | `read_loca_table` |
| 提取目标字形数据 | `get_glyph_data` |
| 解压 Flag 和坐标 | `glyph_to_point` |
| 补充隐式轮廓点 | `process_point` |

字模提取的核心源码位于：

```text
font/
├── main.c
├── ttf.c
├── ttf.h
├── encoding.c
└── encoding.h
```

## 本次整理范围

- 当前流程以 `glyf` 中的简单字形解析为主。
- 复合字形需要继续解析组件 Glyph Index、仿射变换和组件偏移。
- TrueType Hinting 指令目前只跳过，不执行字节码解释。
- 本文的终点是得到字形轮廓点数组，不展开 BMP 绘制、字符串排版、字体缩放和 JPEG 编码。
