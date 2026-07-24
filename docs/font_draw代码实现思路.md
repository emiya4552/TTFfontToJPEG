# `font_draw.c/.h` 代码实现思路

本文解释 `font/font_draw.c` 与 `font/font_draw.h` 如何把 TTF 轮廓转换成 RGB 像素，并完成缩放、单字定位、字符串居中、画布裁剪与实心填充。

阅读前应先了解 `glyph_point_data`、`font_box`、`font_glyph_source` 和 `glyph_to_point()`。这些内容在 [TTF 代码实现思路](TTF代码实现思路.md) 中有详细说明。

## 1. 模块输入与输出

`font_draw` 不读取字体文件。它接收已经加载的 `glyph_point_data`，需要时调用 `glyph_to_point()` 解压轮廓，然后将颜色直接写入调用者提供的 RGB 缓冲区。

```text
文本或单字符
  ↓ encoding：字符边界与 Unicode
font_glyph_source：Unicode → glyph_point_data
  ↓ glyph_to_point
point[] 轮廓
  ↓ 缩放与定位
直线 / 二次贝塞尔曲线
  ├─曲线采样 → 空心轮廓
  └─有向边 + 扫描线 + 非零环绕 → 实心区域
  ↓
RGB color_data
```

该模块不负责创建或保存 BMP 文件。`bmp_data` 中的 `color_data` 只是内存画布，最终文件写入由 `bmp_generate()` 完成。

## 2. 头文件中的公共数据

### 2.1 `bmp_data`

| 字段 | 作用 |
| --- | --- |
| `name` | 兼容旧接口保留的文件名，绘制代码本身不使用 |
| `color_data` | 连续 RGB 画布，长度至少为 `width * height * 3` |
| `color` | 当前绘制颜色，顺序为 R、G、B |
| `width`、`height` | 画布像素尺寸 |

### 2.2 公共接口的四组层级

| 接口组 | 输入来源 | 缩放 | 对象 |
| --- | --- | --- | --- |
| `draw_word`、`draw_filled_word` | 固定字形数组 | 原始高度 | 单字 |
| `draw_scaled_*` | 固定字形数组 | 目标高度 | 单字或字符串 |
| `draw_font_scaled_*` | `ttf_font_data` | 目标高度 | 字符串 |
| `draw_source_scaled_*` | `font_glyph_source` | 目标高度 | 字符串 |

高层渲染器实际使用最后一组。旧数组接口继续保留，用于兼容早期代码和测试。

## 3. 内部常量与数据结构

### 3.1 常量

| 常量 | 值 | 作用 |
| --- | --- | --- |
| `MAX_CONTROL_POINT` | 10 | de Casteljau 临时控制点容量 |
| `OUT_COUNT` | 1000 | 每段曲线的采样数 |
| `ORIGINAL_FONT_HEIGHT` | -1 | 表示不缩放的内部值 |
| `DRAW_OUTLINE` | 0 | 选择空心绘制 |
| `DRAW_FILLED` | 1 | 选择实心绘制 |

当前 TTF 只产生直线和二次贝塞尔曲线，实际控制点数只会是 2 或 3。

### 3.2 内部结构

| 结构 | 字段 | 作用 |
| --- | --- | --- |
| `bezier_point` | `x`、`y` | 曲线采样后的浮点坐标 |
| `glyph_edge` | `x1/y1/x2/y2` | 扫描线使用的有向短边 |
| `glyph_intersection` | `x`、`direction` | 当前扫描线交点与方向 |
| `array_glyph_source` | `glyph_array`、`glyph_count` | 把普通数组适配为统一字形来源 |

边的方向必须保留，因为非零环绕填充需要区分向上和向下穿过扫描线。

## 4. RGB 画布和坐标系

### 4.1 RGB 内存布局

`COLOR_DATA(x, y, z)` 展开为：

```c
color_data[x * width * 3 + y * 3 + z]
```

宏中的 `x` 实际表示行号，`y` 表示列号，`z` 表示 RGB 通道。连续三个字节分别为红、绿、蓝。

### 4.2 坐标转换

TTF 使用左下原点，x 向右、y 向上。RGB 缓冲区使用左上原点，行号向下增大。

最终写像素时执行：

```text
column = glyph_x + offset_x
row    = height - 1 - (glyph_y + offset_y)
```

`center_y` 和 `offset_y` 都按距画布底部理解，只有访问 RGB 行时才翻转 y。

## 5. 字形来源抽象

### 5.1 `find_glyph_target()`

函数顺序遍历 `glyph_point_data[]` 并比较 Unicode。找到时返回数组下标，找不到返回 -1，复杂度为 `O(glyph_count)`。

### 5.2 `get_array_glyph()`

函数把 `void* context` 转回 `array_glyph_source*`，调用线性查找，再返回字形条目地址。

### 5.3 `font_glyph_source` 的作用

绘制代码统一写成：

```c
glyph = source->get_glyph(source->context, unicode);
```

Cache 可以在未命中时读文件，Preload 可以从数组返回。两种策略对绘制层表现为同一个调用。

`source->box` 同时携带统一排版框，使查询接口与字体度量作为一个整体传递。

## 6. 贝塞尔曲线采样

### 6.1 `bezier_data_get()`

函数使用 de Casteljau 算法计算参数 `t` 对应的曲线点。

以二次贝塞尔的 `P0`、`P1`、`P2` 为例：

```text
Q0 = P0 × (1-t) + P1 × t
Q1 = P1 × (1-t) + P2 × t
P  = Q0 × (1-t) + Q1 × t
```

两层循环逐层插值，所以同一函数也能处理两个点表示的直线。

### 6.2 `get_bezier()`

函数申请 `OUT_COUNT` 个点，步长为 `1.0 / OUT_COUNT`，从 `t=0` 采样到接近 1。

固定 1000 点实现简单，但不根据曲线长度和弯曲程度自适应。短曲线会产生重复像素，超长曲线仍可能有离散误差。

## 7. 空心轮廓绘制

### 7.1 `draw_bezier_to_bitmap()`

函数先取得曲线采样点，再把浮点坐标截断为整数。经过左下到左上的坐标转换后，画布外的点被跳过，合法点写入三个 RGB 通道。

它没有抗锯齿，一个采样点最终只覆盖一个完整像素。

### 7.2 `draw_word_from_point()` 分割轮廓

输入 `point[]` 可以包含多个闭合轮廓。函数从 `start` 开始寻找第一个 `locate == NEXT` 的点作为 `end`。

找不到结束标记时打印错误并停止，避免复用上一次轮廓终点造成越界或死循环。

### 7.3 闭合首段

每个轮廓首先把 `point_data[end]` 作为当前点，再读取 `point_data[start]`。因此第一段连接“轮廓最后一点→第一点”，点数组不需要重复保存起点。

### 7.4 直线与二次曲线

若当前点和下一点的 `locate` 都非零，使用两个点绘制直线，并执行 `i++`。

否则再读取第三个点，用三个点绘制二次贝塞尔，并执行 `i += 2`。

```text
端点 + 端点                 → 直线
连接点 + 曲外控制点 + 连接点 → 二次贝塞尔
```

曲线组合同时依赖 `locate` 和数组位置。`glyph_to_point()` 插入的隐式中点位于相邻二次曲线之间。

## 8. 从曲线生成有向边

### 8.1 `append_bezier_edges()`

边数组容量不足时，函数每次增加 `OUT_COUNT`，并使用 `realloc()` 扩展。

它连接相邻采样点，最后一个采样点再连接曲线真实终点，避免填充边界留下开口。

水平边不改变扫描线环绕值，因此 `y1 == y2` 时跳过。其他短边保留方向并追加到 `glyph_edge[]`。

### 8.2 `get_glyph_edges()`

该函数与空心绘制使用相同的 `NEXT` 分割、闭合首段和直线/曲线组合规则。

区别是它不写像素，而是把全部轮廓转换为动态边数组，供所有扫描线重复查询。

## 9. 扫描线与非零环绕填充

### 9.1 总体过程

```text
point[]
  ↓ get_glyph_edges
glyph_edge[]
  ↓ 计算 minY / maxY
逐像素行建立 scan_y
  ↓ 收集边与扫描线的交点
glyph_intersection[]
  ↓ 排序、合并、累计 winding
填充 winding != 0 的区间
```

### 9.2 扫描范围与像素中心

`fill_word_from_point()` 从所有边端点求纵向范围，加 `offset_y` 后使用 `floor/ceil` 转成画布行，并裁剪到合法高度。

每行扫描位置为：

```text
scan_y = canvas_y + 0.5 - offset_y
```

`+0.5` 表示扫描线穿过像素中心。

### 9.3 半开区间规则

边参与扫描的条件为“扫描线包含较低端点但不包含较高端点”。代码分别处理正向边和反向边。

该规则让共享顶点只被一条相邻边计数，避免扫描线穿过轮廓顶点时得到重复交点。

### 9.4 交点与方向

交点使用线性插值：

```text
x = x1 + (scan_y-y1) × (x2-x1) / (y2-y1)
```

边向上时 `direction=1`，向下时为 -1。

### 9.5 `sort_intersection()`

交点使用插入排序按 x 递增排列。单个字形每行交点通常较少，简单稳定的排序符合当前基础 C 实现风格。

### 9.6 `fill_scanline()`

横坐标差小于 `0.0001` 的交点被视为同一位置，并先累加方向。

函数维护 `winding`：

- `0 → 非0`：进入实心区域，记录起点。
- `非0 → 0`：离开实心区域，填充起点到当前交点。
- 非零之间变化：仍位于轮廓内部。

这就是非零环绕规则。外轮廓和孔洞方向相反时，孔洞区域的环绕值会回到 0。

实心区间使用像素中心公式转换为列：

```text
start = ceil(start_x + offset_x - 0.5)
end   = floor(end_x + offset_x - 0.5)
```

列范围裁剪到画布宽度后，再逐像素写入 RGB。

## 10. 实心绘制组合

`draw_filled_word_from_point()` 先调用 `draw_word_from_point()` 绘制边界，再调用 `fill_word_from_point()` 填充内部。

扫描线只覆盖像素中心位于内部的区域。先画轮廓可以补足边界附近像素，使结果更完整。

代价是实心字形会对同一批曲线采样两次：一次画轮廓，一次生成填充边。

## 11. 缩放实现

### 11.1 `scale_coordinate()`

坐标乘浮点比例后，正数加 0.5、负数减 0.5，再转换为整数，实现远离零方向的四舍五入。

该函数主要处理排版框和 `advance_width`。轮廓点本身由 `scale_point_data()` 保持为浮点数。

### 11.2 `get_scaled_box()`

原始字体高度和缩放比例为：

```text
font_height = box.y_max - box.y_min
scale       = target_height / font_height
```

缩放后的 x 和 y 下界按比例取整。`scaled_box.y_max` 直接设为 `scaled_box.y_min + target_height`，确保排版框高度严格等于输入值。

内部值 `ORIGINAL_FONT_HEIGHT == -1` 表示比例 1，用于旧的不缩放接口。

### 11.3 `scale_point_data()`

函数遍历轮廓点，对 x、y 原地乘相同比例。`locate` 不变，因此缩放不会改变轮廓结构。

## 12. 单字定位与绘制

### 12.1 `draw_glyph_by_height()`

该函数是单字几何绘制核心，执行顺序如下：

1. 计算目标排版框和比例。
2. 调用 `glyph_to_point()` 解码原始字形。
3. 缩放全部轮廓点。
4. 根据排版框中心计算平移。
5. 调用空心或实心底层函数。
6. 释放临时 `point[]`。

偏移公式为：

```text
offset_x = center_x - (scaled_x_min + scaled_x_max) / 2
offset_y = center_y - (scaled_y_min + scaled_y_max) / 2
```

所以输入坐标代表传入排版框的中心，不是黑色轮廓实际包围盒的中心。

### 12.2 `draw_word_by_height()`

函数通过 `word_to_unicode()` 把单字符字符串转换为 Unicode，再通过统一来源取得字形。

编码失败或找不到字形时只打印错误，不进入轮廓解码。

### 12.3 `draw_array_word_by_height()`

函数建立栈上的 `array_glyph_source` 和临时 `font_glyph_source`，最终复用 `draw_word_by_height()`。

因此数组接口不是另一套绘制算法，只是统一来源接口的适配层。

## 13. 字符串整体居中

### 13.1 为什么先预排版

输入坐标表示整个字符串中心。代码必须先知道最终可绘制前缀的总前进宽度，才能确定第一个字符的位置。

字符串绘制因此分为“宽度计算”和“实际绘制”两遍。

### 13.2 `get_centered_string_range()`

中心点两侧可用距离取较小值：

```text
available_width = 2 × min(center_x, bmp_width - center_x)
```

这样最终前缀可以保持以输入位置为中心，不会因为某一侧空间更大而改变中心。

函数逐字符执行：

1. `get_character_byte_length()` 取得当前字符字节数。
2. 复制成以 `\0` 结尾的单字符字符串。
3. `word_to_unicode()` 转为 Unicode。
4. 通过字形来源取得 `advance_width`。
5. 按统一比例缩放前进宽度。
6. 判断加入字符后是否超过可用宽度。

`draw_length` 是可绘制前缀的字节数，`draw_width` 是它的缩放后总前进宽度。

遇到非法字符、字形缺失、无效宽度或画布不足时停止，后续字符直接截断。

## 14. 字符串逐字绘制

### 14.1 初始化排版笔

`draw_string_by_height()` 取得缩放框并完成预排版后，计算：

```text
pen_x    = center_x - draw_width / 2
offset_y = center_y - scaled_box_center_y
```

`pen_x` 表示当前字符排版框的左侧。

### 14.2 再次拆分字符串

实际循环再次按编码边界拆字符，并重新查询 Unicode 和字形。第一遍只确定完整前缀和宽度，第二遍才产生像素。

### 14.3 完整字框裁剪

每个字符绘制前检查：

```text
0 <= pen_x
pen_x + advance_width <= bmp.width
0 <= offset_y + scaled_box.y_min
offset_y + scaled_box.y_max <= bmp.height
```

任一方向无法容纳完整排版框时，当前字符和剩余字符串都不会绘制。

### 14.4 单字符排版框

字符串中的单字使用：

```text
word_box.x_min = 0
word_box.x_max = glyph.advance_width
word_center_x  = pen_x + advance_width / 2
```

定位依据真实前进宽度，而不是轮廓 `xMin/xMax`。这样可以保留左右留白并连续推进排版笔。

### 14.5 空格

若 `glyph_length == 0`，代码不调用 `glyph_to_point()`，但仍执行 `pen_x += advance_width`。

因此空格参与总宽度和居中，却不会产生字形解码失败。

## 15. 三类字形集合如何复用

### 15.1 固定数组接口

`draw_word()`、`draw_string()` 及 scaled/filled 版本，把数组包装为 `font_glyph_source`。

旧单字接口固定使用 `UNICODE_BMP_NUMBER` 作为数组长度，适用于早期完整中文数组。

### 15.2 `ttf_font_data` 接口

`draw_font_scaled_string()` 与 `draw_font_scaled_filled_string()` 使用预加载结构中的真实 `glyph_count` 和 `box`。

它们检查预加载数据后，仍然调用数组适配层。

### 15.3 统一来源接口

`draw_source_scaled_string()` 和 `draw_source_scaled_filled_string()` 直接调用 `draw_string_by_height()`。

这是当前推荐入口。`font_renderer` 使用它在 Cache 与 Preload 之间切换，而不改变绘制代码。

## 16. 高层实际调用链

```text
font_renderer_render_rgb
  ↓
draw_source_scaled_filled_string
  ↓
draw_string_by_height
  ├─ get_scaled_box
  ├─ get_centered_string_range
  └─ 对每个字符：draw_glyph_by_height
       ├─ glyph_to_point
       ├─ scale_point_data
       └─ draw_filled_word_from_point
            ├─ draw_word_from_point
            │    └─ draw_bezier_to_bitmap
            └─ fill_word_from_point
                 ├─ get_glyph_edges
                 │    └─ append_bezier_edges
                 └─ fill_scanline
```

高层管理画布和加载策略，`font_draw` 只处理字符、轮廓几何与 RGB 写入。

## 17. 内存所有权

| 数据 | 创建位置 | 释放位置 |
| --- | --- | --- |
| `bezier_point[]` | `get_bezier()` | 曲线绘制或边生成函数 |
| 空心绘制控制点缓冲区 | `draw_word_from_point()` | 同一函数结束前 |
| `glyph_edge[]` | `get_glyph_edges()` | `fill_word_from_point()` |
| `glyph_intersection[]` | `fill_word_from_point()` | 同一函数结束前 |
| 解码后的 `point[]` | `glyph_to_point()` | `draw_glyph_by_height()` |
| 单字符字符串缓冲区 | 预排版和字符串绘制函数 | 各自结束前 |

字形来源返回的 `glyph_point_data*` 不由绘制层释放。Cache 或 Preload 决定其有效期。

## 18. 关键错误保护

- 画布、颜色或点数组为空时，底层绘制直接返回。
- 轮廓缺少 `NEXT` 时停止，避免死循环。
- `malloc/realloc` 失败时释放已经建立的数据。
- 编码无效或字形不存在时停止当前单字或剩余字符串。
- 目标高度和原始字体高度必须大于零。
- 扫描范围、填充列和采样点都会裁剪到画布。
- 空轮廓字形在字符串中只推进排版笔。

多数公共绘制接口返回 `void`，错误通过日志体现。调用者目前无法只凭返回值判断某个字符是否被跳过。

## 19. 性能特征

### 19.1 固定曲线采样

每段曲线固定生成 1000 点。字形轮廓段越多，采样和边数组越大。

实心绘制先画轮廓再填充，因此同一曲线会采样两遍。

### 19.2 扫描线复杂度

每条像素扫描线都会遍历全部非水平短边，再对交点执行插入排序。

该实现适合作为算法展示。若追求大字号或高帧率，可使用活动边表减少重复检查。

### 19.3 字形查找

普通数组来源按 Unicode 线性查找。Cache 或 Preload 可以在各自模块中替换查询结构，绘制层无需改变。

## 20. 当前实现边界

- 像素为硬覆盖，没有灰度或亚像素抗锯齿。
- 曲线固定采样 1000 点，没有自适应细分。
- 不执行 Hinting、Kerning、GPOS、GSUB、双向文本或复杂文字整形。
- 字符串只进行单行、从左向右排版。
- 截断以完整前进宽度排版框为单位，不绘制半个字符。
- 复合字形在 `glyph_to_point()` 阶段被拒绝。
- 输入中心表示排版框中心，不保证等于黑色轮廓的视觉重心。

## 21. 函数索引

| 函数 | 作用 |
| --- | --- |
| `find_glyph_target` | 在线性数组中查找 Unicode |
| `get_array_glyph` | 数组来源查询回调 |
| `bezier_data_get` | 使用 de Casteljau 计算曲线点 |
| `get_bezier` | 固定步长采样曲线 |
| `draw_bezier_to_bitmap` | 将采样点写入 RGB 画布 |
| `draw_word_from_point` | 分割轮廓并绘制空心字形 |
| `append_bezier_edges` | 把曲线转换为有向短边 |
| `get_glyph_edges` | 建立整个字形的边数组 |
| `sort_intersection` | 按 x 排序扫描线交点 |
| `fill_scanline` | 依据非零环绕值填充一行 |
| `fill_word_from_point` | 遍历扫描线填充字形内部 |
| `draw_filled_word_from_point` | 组合轮廓和内部填充 |
| `scale_coordinate` | 缩放并取整整数度量 |
| `get_scaled_box` | 计算比例与目标排版框 |
| `scale_point_data` | 缩放全部轮廓点 |
| `draw_glyph_by_height` | 解码、缩放、定位并绘制单字 |
| `draw_word_by_height` | 解码单字符并查询字形 |
| `draw_array_word_by_height` | 把数组适配为单字来源 |
| `get_centered_string_range` | 计算可居中绘制的字符串前缀 |
| `draw_string_by_height` | 完成字符串排版、截断和绘制 |
| `draw_array_string_by_height` | 把数组适配为字符串来源 |
| `draw_*` 公共包装 | 选择来源、缩放和空心/实心模式 |

理解本模块的关键，是把代码分成三层：底层曲线与像素、中层缩放与单字定位、高层编码拆分与字符串排版。三层通过统一字形来源和 RGB 画布连接起来。
