# `ttf.c/.h` 代码实现思路

本文解释当前 `font/ttf.c` 与 `font/ttf.h` 的真实代码实现。重点不是重复介绍 TTF 文件格式，而是说明代码如何组织数据、函数如何调用、指针如何移动、内存由谁持有，以及 Unicode 最终如何变成可绘制的轮廓点。

TTF 表本身的字段定义与磁盘布局，可配合 [TTF 字模数据提取思路](TTF提取思路.md) 阅读。

## 1. 模块职责与边界

`ttf` 模块负责完成三件事：

1. 打开字体并解析后续查询需要的公共数据。
2. 根据 Unicode 找到 Glyph Index，再读取对应 `glyf` 原始数据与 `hmtx` 前进宽度。
3. 将简单字形的压缩数据解码为 `point[]` 轮廓点。

它不负责字符串拆分、缩放、排版、曲线采样和像素填充。这些工作由 `encoding` 与 `font_draw` 完成。

```text
ttf_font_open()
  ├─读取六张关键表
  ├─解析 head / hhea
  ├─选择并解析 cmap
  └─解析 loca

ttf_font_load_glyph(unicode)
  ├─cmap: Unicode → Glyph Index
  ├─loca: Glyph Index → glyf 偏移和长度
  ├─glyf: 按需读取原始字形
  └─hmtx: 读取 advanceWidth

glyph_to_point()
  └─glyf 原始数据 → 可绘制 point[]
```

## 2. `ttf.h` 暴露的数据类型

### 2.1 基础整数类型

| 类型 | 当前定义 | 主要用途 |
| --- | --- | --- |
| `u8` | `unsigned char` | 原始字节、Flag、字形缓冲区 |
| `s8` | `char` | 有符号单字节数据 |
| `u16` | `unsigned short` | TTF 的 16 位无符号字段 |
| `s16` | `short` | 坐标增量、字体边界等有符号字段 |
| `u32` | `unsigned int` | 表偏移、Unicode、Format 12 字段 |
| `s32` | `int` | 32 位有符号字段 |
| `u64` | `uint64_t` | `head` 表时间字段 |

当前实现假定 `short` 为 16 位、`int` 与 `unsigned int` 为 32 位，并主要面向常见小端 Windows/Linux 环境。

### 2.2 `point`

```c
typedef struct{
	float x;
	float y;
	int   locate;
}point;
```

| 字段 | 含义 |
| --- | --- |
| `x`、`y` | 解压后的字体坐标，绘制前可以乘缩放比例 |
| `locate == 0` | 当前点参与二次贝塞尔控制点组合 |
| `locate == 1` | 普通曲上点 |
| `locate == NEXT` | 当前点是一个轮廓的最后一点 |

`NEXT` 的值为 10000。绘制层通过它分割多个闭合轮廓，而不是额外保存轮廓数组。

### 2.3 `glyph_point_data`

| 字段 | 含义 | 所有权 |
| --- | --- | --- |
| `unicode` | 此条目对应的 Unicode | 普通值 |
| `glyph_data` | 从 `glyf` 表按需复制的原始字节 | 条目持有，使用 `ttf_glyph_free()` 释放 |
| `glyph_length` | 原始字形字节数 | 普通值 |
| `advance_width` | 从 `hmtx` 得到的水平前进宽度 | 普通值 |

该结构保存的是“可延迟解码的字形”。加载阶段只复制原始 `glyf` 数据，真正绘制时才调用 `glyph_to_point()` 生成临时轮廓点。

### 2.4 `font_box`

| 字段 | 当前赋值来源 |
| --- | --- |
| `x_min` | 固定为 0 |
| `x_max` | `head.unit_per_Em` |
| `y_min` | `hhea.descent` |
| `y_max` | `hhea.ascent` |

它是统一排版框，不是某个字形的实际包围盒，也不是直接复制 `head.xMin~yMax`。绘制层使用统一排版框计算目标字高和纵向中心，因此不同字符可以保持相同基线体系。

### 2.5 `font_glyph_source`

```c
typedef glyph_point_data* (*font_glyph_getter)(void* context, u32 unicode);

typedef struct{
	void*             context;
	font_box          box;
	font_glyph_getter get_glyph;
}font_glyph_source;
```

该结构是解析层、缓存层、预加载层与绘制层之间的统一接口。绘制层只调用 `get_glyph(context, unicode)`，不需要知道字形来自文件、缓存还是数组。

### 2.6 `ttf_font`

头文件只声明：

```c
typedef struct ttf_font ttf_font;
```

调用者只能持有指针，不能访问内部字段。这使表解析细节留在 `ttf.c` 内，同时强制调用者通过 `ttf_font_open()`、`ttf_font_load_glyph()` 和 `ttf_font_close()` 管理生命周期。

## 3. `ttf.c` 的内部结构

### 3.1 表目录与表缓冲区

| 结构 | 关键字段 | 作用 |
| --- | --- | --- |
| `file_header` | `version`、`table_number` 等 | 对应 SFNT Offset Table |
| `table_entry` | `tag`、`check_sum`、`offset`、`length` | 对应每个表目录项 |
| `table` | `name`、`offset`、`length`、`data` | 保存代码真正关心的表 |

`table_entry` 是磁盘目录项，读取完成后会释放。`table` 是运行期描述，保存关键表的位置及可选的内存副本。

### 3.2 表头结构

| 结构 | 用途 |
| --- | --- |
| `head_table` | 读取 `unit_per_Em`、`index_to_loca_format` 和全局信息 |
| `hhea_header` | 读取 `ascent`、`descent`、`number_of_h_metrics` |
| `glyph_data_header` | 读取轮廓数与单字形边界 |
| `cmap_subtable_header` | 描述平台、编码和子表偏移 |
| `cmap_format4_header` | 读取 Format 4 的段数量和总长度 |

这些结构使用 `#pragma pack(1)`，目的是让结构体字段紧贴 TTF 磁盘布局。代码先 `memcpy()`，再逐字段执行大小端转换。

### 3.3 解析后的 cmap 数据

| 结构 | 内容 |
| --- | --- |
| `cmap_format4_data` | `end_code`、`start_code`、`id_delta`、`id_range_offset` 和 `glyph_index_array` |
| `sequential_map_group` | Format 12 的起始字符、结束字符和起始 Glyph ID |
| `cmap_format12_data` | Format 12 头部及 `groups[]` |
| `cmap_data` | 当前启用格式与 Format 4/12 联合体 |

`cmap_data` 一次只持有一种格式。选中 Format 12 后不会继续解析或缓存 Format 4。

### 3.4 `struct ttf_font`

| 字段 | 保存内容 | 关闭时如何处理 |
| --- | --- | --- |
| `file` | 保持打开的字体文件 | `fclose()` |
| `table_array` | 六张关键表的运行期描述 | `free_ttf_tables()` |
| `cmap` | 已解析的 Format 4 或 Format 12 | `free_cmap()` |
| `loca_array` | Glyph Index 对应的 `glyf` 偏移 | `free()` |
| `loca_length` | `loca` 项数，字形数为它减一 | 普通值 |
| `number_of_h_metrics` | 完整水平度量项数量 | 普通值 |
| `box` | 绘制层使用的统一排版框 | 普通值 |

字体文件保持打开，是因为 `glyf` 没有整体载入内存。每次缓存未命中时，解析器仍需 `fseek()` 到字体文件中读取一个字形。

## 4. 大端字段如何转换

TTF 多字节整数使用大端序。代码先由 `convert(A, B)` 根据位数选择 `BIG_TO_LITTLE_16/32/64`，再通过 `CONVERT(A)` 自动使用目标变量自身的位宽。

典型读取方式如下：

```c
memcpy(&value, pointer, sizeof(value));
CONVERT(value);
```

`memcpy()` 避免直接把任意字节地址强制转换为多字节指针。转换完成后，后续算法都使用本机整数，不再反复处理字节序。

当前宏执行的是无条件字节交换，所以适合项目当前的小端主机目标。如果未来支持大端机器，需要按主机字节序决定是否交换。

## 5. 六张关键表的加载策略

`read_ttf_data()` 只查找以下六张表：

| `table_array` 索引 | 表名 | 加载方式 | 后续用途 |
| --- | --- | --- | --- |
| 0 | `head` | 整表读取 | `unitsPerEm`、`loca` 格式 |
| 1 | `cmap` | 整表读取 | Unicode 映射 |
| 2 | `loca` | 整表读取 | 字形偏移数组 |
| 3 | `glyf` | 只保存偏移和长度 | 按字形读取 |
| 4 | `hhea` | 整表读取 | 排版上下界、度量数量 |
| 5 | `hmtx` | 整表读取并长期保留 | 每个字形的前进宽度 |

函数首先读取 `file_header`，根据 `table_number` 申请完整目录数组，再对每个目录项转换校验和、偏移和长度。

随后针对六个固定表名分别遍历目录。这样不依赖字体内的表排列顺序，也不要求六张表相邻。

`glyf` 可能很大，因此索引 3 不申请 `data`。其他五张表会 `malloc(length)`、定位到表偏移并完整读取。

任何关键表缺失、长度为零、分配失败或读取不足，函数都会释放已经建立的数据并关闭文件。

## 6. `ttf_font_open()` 的初始化顺序

`ttf_font_open()` 是整个解析上下文的入口，实际顺序如下：

```text
申请 ttf_font
  ↓
read_ttf_data()
  ↓
read_head_table() + read_hhea_header()
  ↓
read_cmap()
  ↓
read_loca_table()
  ↓
建立 font_box 与 number_of_h_metrics
  ↓
释放不再需要的 head/cmap/loca/hhea 原始副本
```

### 6.1 解析 `head` 与 `hhea`

`read_head_table()` 复制完整 `head_table`，逐字段转换字节序。当前初始化真正使用的是 `unit_per_Em` 和 `index_to_loca_format`。

`read_hhea_header()` 使用相同方式读取 `ascent`、`descent`、`line_gap` 和 `number_of_h_metrics`。

### 6.2 解析 cmap 与 loca

`read_cmap()` 必须返回 12 或 4；`read_loca_table()` 必须得到至少两个偏移项。任一条件不满足，初始化立即调用 `ttf_font_close()` 清理半成品。

### 6.3 建立排版框

```c
box.x_min = 0;
box.x_max = head.unit_per_Em;
box.y_min = hhea.descent;
box.y_max = hhea.ascent;
```

这个框表达一个 Em 宽度和字体推荐的纵向排版范围，后续缩放使用 `box.y_max - box.y_min` 作为原始字体高度。

### 6.4 丢弃临时原始表

当 `head`、`cmap`、`loca`、`hhea` 已转化为结构化数据后，它们的原始缓冲区被释放并置空。

运行期只长期保留：解析后的 cmap、`loca_array`、完整 `hmtx`、`glyf` 文件位置以及打开的文件句柄。

## 7. cmap 子表选择

### 7.1 `get_cmap_subtable_data()`

该函数接收目标格式 12 或 4，先读取 cmap 版本后的子表数量，再把所有编码记录复制到 `cmap_subtable_header[]`。

选择逻辑分两层：

1. 找到格式正确的子表时，先将第一个匹配项作为候选。
2. 如果同一格式存在 Windows 平台的首选编码，则替换候选并停止搜索。

Format 12 的首选记录是平台 3、编码 10；Format 4 的首选记录是平台 3、编码 1。

因此“格式”决定能否解析，“平台和编码”决定多个同格式子表中优先使用哪一个。即使没有 Windows 首选项，第一个格式匹配项仍可作为回退。

选中后，Format 4 从第二个 `u16` 读取长度；Format 12 跳过保留字段，从后续 `u32` 读取长度。函数复制完整子表，调用者负责释放。

### 7.2 `read_cmap()`

`read_cmap()` 先请求 Format 12。只要成功解析，就设置 `p_cmap->format = 12` 并立即返回。

只有字体完全没有可用 Format 12 时，才请求 Format 4。这样同一字体不会同时持有两套 Unicode 映射。

## 8. Format 12 的解析与查询

### 8.1 `read_cmap_format12_subtable()`

函数按顺序读取 `format`、`reserved`、`length`、`language` 和 `groups_number`，再申请 `groups_number` 个 `sequential_map_group`。

每个分组表示：

```text
Unicode: [start_char_code, end_char_code]
Glyph:   start_glyph_id + (unicode - start_char_code)
```

所有分组复制完成后，代码逐组转换三个 32 位字段。

### 8.2 `get_glyph_index_format12()`

Format 12 分组按字符编码递增。函数使用左闭右开的二分查找区间 `[left, right)`。

若 Unicode 小于当前组起点，则收缩右边界；大于当前组终点，则移动左边界；落在组内时按顺序映射公式计算 Glyph Index。

查询复杂度为 `O(log groups_number)`，适合包含大量 Unicode 分组的字体。

## 9. Format 4 的解析与查询

### 9.1 `read_cmap_format4_subtable()`

函数读取 `cmap_format4_header` 后，通过 `segment_count_X2 / 2` 得到段数量。

Format 4 的数组布局为：

```text
endCode[segCount]
reservedPad
startCode[segCount]
idDelta[segCount]
idRangeOffset[segCount]
glyphIndexArray[]
```

代码为四个等长段数组分别申请内存。尾部 `glyphIndexArray` 长度由子表总长度减去头部、保留字段和四组段数组后计算。

各数组先整体复制，再逐项做 16 位字节序转换。

### 9.2 `get_glyph_index_format4()`

Format 4 只能处理 `U+0000~U+FFFF`。Unicode 超过 `0xffff` 时直接返回 0。

函数顺序遍历段，找到满足以下条件的段：

```text
startCode[i] <= unicode <= endCode[i]
```

若 `idRangeOffset == 0`：

```text
glyph_index = (unicode + idDelta) % 65536
```

若 `idRangeOffset != 0`，代码先把规范中“相对当前 `idRangeOffset` 元素地址”的偏移换算成 `glyph_index_array` 下标：

```text
offset = idRangeOffset / 2
       + unicode - startCode
       + currentSegment - segmentCount
```

下标越界时返回 0。取出的 Glyph Index 非零时，还要加 `idDelta` 并对 65536 取模。

Format 4 当前使用顺序段查找，复杂度为 `O(segment_count)`；Format 12 则使用二分查找。

## 10. `loca` 如何变成偏移数组

`read_loca_table()` 根据 `head.index_to_loca_format` 选择两种读取方式。

| 格式 | 磁盘项宽度 | 代码处理 |
| --- | --- | --- |
| 0 | `u16` | 大端转换后乘 2，得到真实 `glyf` 字节偏移 |
| 非 0 | `u32` | 大端转换后直接保存 |

`loca_length` 是偏移项数量，不是字形数量。第 `i` 个字形的范围为：

```text
offset = loca[i]
length = loca[i + 1] - loca[i]
```

因此可索引的字形数为 `loca_length - 1`。最后一个 `loca` 项只用于计算最后一个字形的长度。

两个相邻偏移相等时，`glyph_length == 0`，表示空轮廓字形。空格通常属于这种情况，但仍然可能具有有效的 `advance_width`。

## 11. Unicode 字形加载链路

### 11.1 `ttf_font_load_glyph()`

公共函数先把输出 `glyph_point_data` 清零，再把上下文中的文件、`glyf` 描述、cmap、loca、hmtx 和度量数量传给 `load_glyph_entry()`。

输出条目的 `glyph_data` 是独立分配的副本，所以缓存或预加载模块可以在之后持有它。

### 11.2 `load_glyph_entry()`

函数先调用 `get_glyph_index()`。若索引为负数或无法取得 `loca[index + 1]`，则回退到 Glyph 0。

Glyph 0 通常是 `.notdef`。cmap 查询返回 0 时也会自然得到该字形，因此“没有映射”和“映射到 Glyph 0”在当前接口上都表现为索引 0。

随后从 `loca` 计算偏移和长度，并填充：

```c
glyph->unicode       = unicode;
glyph->glyph_length  = glyph_length;
glyph->advance_width = get_advance_width(...);
```

最后调用 `read_glyph_data()` 读取原始字形。

### 11.3 `get_advance_width()`

`hmtx` 的前 `number_of_h_metrics` 项每项占 4 字节，前两个字节是 `advanceWidth`，后两个字节是左侧边距。

当前代码只读取 `advanceWidth`。若 Glyph Index 超过完整度量项数量，会复用最后一个完整度量项的前进宽度，这符合 `hmtx` 后续字形共享宽度的布局。

### 11.4 `read_glyph_data()`

函数检查 `glyph_data_offset` 与 `glyph_length` 是否落在 `glyf` 表长度内，再申请恰好 `glyph_length` 字节。

实际文件位置为：

```text
glyf_table.offset + glyph_data_offset
```

定位或读取失败时会释放刚申请的缓冲区并将输出指针恢复为 `NULL`。

## 12. `glyph_to_point()` 的简单字形解压

这是 `ttf.c` 中最核心、边界检查最多的函数。

### 12.1 输入和临时指针

`pointer` 指向当前读取位置，`glyph_end = glyph_data + glyph_length` 指向合法范围末尾。

每次读取可变长度内容前，代码都比较剩余字节数。错误通过 `error_reason` 记录，统一跳转到 `cleanup`。

### 12.2 读取字形头

函数复制 `glyph_data_header`，转换轮廓数和四个边界字段。

`number_of_contours <= 0` 时直接返回 0。负数表示复合字形；零通常表示无轮廓字形。当前函数只处理正轮廓数的简单字形。

### 12.3 读取轮廓终点

`endPtsOfContours[]` 保存每个轮廓最后一个逻辑点的索引。函数要求这些索引严格递增。

最后一个终点加一就是原始逻辑点总数：

```text
raw_point_length = endPtsOfContours[numberOfContours - 1] + 1
```

### 12.4 跳过 Hinting 指令

轮廓终点后是 `instructionLength`。函数读取长度并确认剩余数据足够，然后直接跳过指令字节。

当前渲染器不执行 TrueType Hinting，但必须正确跳过，否则后续指针无法定位到 Flag 流。

### 12.5 展开重复 Flag

代码申请 `raw_point_length` 个 Flag。每读取一个 Flag，先写入当前逻辑点。

若 Flag 含 `GLYF_FLAG_REPEAT`，下一个字节是额外重复次数。代码检查重复次数不能超过剩余逻辑点，再把同一 Flag 填入后续位置。

展开完成后，`flag[i]` 与逻辑点 `i` 一一对应，后续坐标解压不再关心压缩形式。

### 12.6 解码 x 坐标

x 坐标是相对增量，`current_x` 从 0 开始累加。

| Flag 组合 | x 增量读取方式 |
| --- | --- |
| `X_SHORT=1`、`X_SAME=1` | 读取 1 字节正增量 |
| `X_SHORT=1`、`X_SAME=0` | 读取 1 字节负增量 |
| `X_SHORT=0`、`X_SAME=1` | 增量为 0，不读取字节 |
| `X_SHORT=0`、`X_SAME=0` | 读取大端 `s16` 增量 |

解码后的累计值写入 `raw_point_data[i].x`。

### 12.7 解码 y 坐标和轮廓标记

y 坐标使用相同规则，但对应 `Y_SHORT` 与 `Y_SAME`。

每个点同时根据 `ON_CURVE` 写入 `locate`：曲上点为 1，曲外控制点为 0。

若当前逻辑点索引等于某个轮廓终点，代码把 `locate` 覆盖为 `NEXT`，并移动到下一个轮廓终点。

因此 `NEXT` 同时承担“非零端点”和“轮廓结束”两种作用。绘制层把任何非零 `locate` 当作普通端点，并额外用 `NEXT` 分割轮廓。

### 12.8 `process_point()` 插入隐式中点

TrueType 允许两个连续曲外控制点之间隐含一个中点。函数先统计相邻 `locate == 0` 的组合数量，再申请扩展后的点数组。

每遇到连续两个控制点，就插入：

```text
x = (previous.x + current.x) / 2
y = (previous.y + current.y) / 2
```

当前代码将插入点的 `locate` 仍写为 0。绘制代码依赖点序列和 `i += 2` 的步进把它作为相邻二次曲线的连接点，而不是只依赖 `locate` 判断第三个端点。

这是一处理解代码时容易误判的细节：`locate` 在当前实现中既有点类型含义，也参与轮廓分段；真正的曲线组合还依赖数组位置。

### 12.9 统一清理

成功时返回处理后的点数量，输出 `point[]` 由调用者 `free()`。

错误时打印原因、字形长度和当前偏移，并释放轮廓终点、Flag、原始点及已经生成的输出点。返回值为 0。

## 13. 内存所有权

| 数据 | 创建者 | 使用者 | 释放方式 |
| --- | --- | --- | --- |
| `ttf_font` | `ttf_font_open()` | cache/preload/调用者 | `ttf_font_close()` |
| `table_array` | `read_ttf_data()` | `ttf_font` | `free_ttf_tables()` |
| cmap 数组 | `read_cmap_*()` | `ttf_font` | `free_cmap()` |
| `loca_array` | `read_loca_table()` | `ttf_font` | `free()` |
| `glyph_data` | `read_glyph_data()` | 字形条目 | `ttf_glyph_free()` |
| `point[]` | `glyph_to_point()` | 单次绘制 | `free()` |

缓存策略要求 `ttf_font` 在缓存生命周期内保持打开，因为未命中字形需要继续读取字体文件。

预加载策略会先把目标字形全部复制成独立 `glyph_data`，随后可以关闭 `ttf_font`，只保留预加载数组。

## 14. 公共接口的典型使用

```c
ttf_font* font;
glyph_point_data glyph;
font_box box;
point* points;
int point_count;

font = NULL;
if(ttf_font_open("font/simhei.ttf", &font) != 0){
	return -1;
}

ttf_font_get_box(font, &box);
if(ttf_font_load_glyph(font, 0x9A6C, &glyph) == 0){
	points = NULL;
	point_count = glyph_to_point(glyph.glyph_data, &points, glyph.glyph_length);
	free(points);
	ttf_glyph_free(&glyph);
}

ttf_font_close(font);
```

实际高层代码通常不会直接调用这一整套流程，而是让 `ttf_cache` 或 `ttf_preload` 管理字形，再通过 `font_glyph_source` 交给绘制层。

## 15. 函数调用关系

```text
ttf_font_open
  ├─ read_ttf_data
  │    └─ free_ttf_tables（失败清理）
  ├─ read_head_table
  ├─ read_hhea_header
  ├─ read_cmap
  │    ├─ get_cmap_subtable_data
  │    ├─ read_cmap_format12_subtable
  │    └─ read_cmap_format4_subtable
  └─ read_loca_table

ttf_font_load_glyph
  └─ load_glyph_entry
       ├─ get_glyph_index
       │    ├─ get_glyph_index_format12
       │    └─ get_glyph_index_format4
       ├─ get_advance_width
       └─ read_glyph_data

glyph_to_point
  └─ process_point

ttf_font_close
  ├─ free_cmap
  └─ free_ttf_tables
```

## 16. 函数索引

| 函数 | 层级 | 作用 |
| --- | --- | --- |
| `read_hhea_header` | 内部 | 解析字体纵向度量和水平度量数量 |
| `get_advance_width` | 内部 | 读取指定 Glyph 的前进宽度 |
| `free_ttf_tables` | 内部 | 释放六张关键表描述及缓冲区 |
| `read_ttf_data` | 内部 | 读取 SFNT 目录与六张关键表 |
| `read_head_table` | 内部 | 解析 `head` 表 |
| `get_cmap_subtable_data` | 内部 | 选择并复制指定格式的 cmap 子表 |
| `read_cmap_format4_subtable` | 内部 | 展开 Format 4 数组 |
| `get_glyph_index_format4` | 内部 | 查询 BMP Unicode 映射 |
| `read_cmap_format12_subtable` | 内部 | 读取 Format 12 分组 |
| `get_glyph_index_format12` | 内部 | 二分查询完整 Unicode 映射 |
| `read_cmap` | 内部 | 选择 Format 12 或回退 Format 4 |
| `get_glyph_index` | 内部 | 分发到当前 cmap 查询函数 |
| `free_cmap` | 内部 | 释放当前 cmap 格式的数据 |
| `read_loca_table` | 内部 | 建立 Glyph 偏移数组 |
| `read_glyph_data` | 内部 | 从 `glyf` 按需复制一个字形 |
| `process_point` | 内部 | 插入连续控制点间的隐式中点 |
| `load_glyph_entry` | 内部 | 组合 cmap、loca、glyf、hmtx |
| `ttf_font_open` | 公共 | 创建可复用字体上下文 |
| `ttf_font_close` | 公共 | 关闭上下文并释放资源 |
| `ttf_font_get_box` | 公共 | 获取统一排版框 |
| `ttf_font_load_glyph` | 公共 | 按 Unicode 加载独立字形 |
| `ttf_glyph_free` | 公共 | 释放单个字形原始数据 |
| `glyph_to_point` | 公共 | 把简单字形解码为轮廓点 |

## 17. 当前实现边界

- 只解析 TrueType `glyf` 简单字形，不展开复合字形。
- 不执行 TrueType Hinting，仅跳过提示指令。
- 不解析 CFF/CFF2 轮廓、可变字体轴、Kerning、GPOS 或 GSUB。
- Format 4 使用顺序段查找；极大字体可以进一步改为二分查询。
- 表目录与 cmap 子表的部分长度关系仍依赖字体文件基本合法，简单字形内部的边界检查更完整。
- `ttf_font` 本身不缓存字形，缓存与批量预加载由独立模块完成。

理解这些边界后，可以把本模块准确描述为：一个面向 TrueType 简单字形、支持 Unicode Format 12/4、采用按需 `glyf` 读取的轻量解析核心。
