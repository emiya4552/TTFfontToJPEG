# 纯 C TTF 字体渲染与 JPEG 编码

本项目使用 C 语言直接解析 TrueType 字体，将中英文字符串绘制为实心 RGB 位图，并输出为 24 位 BMP 或 Baseline JPEG。

字体解析、轮廓还原、字符串排版、扫描线填充、DCT、量化和 Huffman 编码均由项目自行实现，不依赖 FreeType、HarfBuzz、stb_truetype 或 libjpeg 等库。

项目提供三个可独立运行的入口：

1. `font/main.c`：演示 TTF 解析与 BMP 输出。
2. `jpeg/main.c`：将未压缩 24 位 BMP 转换为 4:2:0 JPEG。
3. 根目录 `main.c`：完成文本渲染并直接输出 BMP、JPEG 或两种格式。

完整数据链路如下：

```text
文本输入
  ↓ UTF-8 / GBK / 系统编码转换
Unicode
  ↓ cmap Format 12 / Format 4
Glyph Index
  ↓ loca + glyf + hmtx
字形轮廓与前进宽度
  ↓ 缩放、字符串排版、贝塞尔曲线、扫描线填充
RGB 像素缓冲区
  ├─→ 24 位 BMP
  └─→ YCbCr + 4:2:0 + DCT + 量化 + Huffman → JPEG
```

## 项目亮点

- 使用基础 C 库自行解析 TTF 核心表，不依赖第三方字体引擎。
- 优先使用 `cmap Format 12`，不存在时回退到 `Format 4`。
- 支持中文、英文、常用标点及中英文混合字符串。
- 根据 `hmtx` 前进宽度排版，不使用固定字符间距。
- 支持目标字高缩放、字符串整体居中、画布裁剪与实心填充。
- 提供按需 Cache 与批量 Preload 两种字形加载策略。
- 自行实现 Baseline JPEG 4:2:0 编码及标准 Huffman 熵编码。
- 提供三个不同层级的入口，便于分别验证字体、JPEG 和完整流水线。
- 提供 TTF 提取、TTF 代码、字体绘制和 JPEG 编码四份实现文档。
- Windows 和 Linux 使用独立编码适配分支，主体代码保持平台无关。

## 快速启动

### 环境要求

Windows 推荐使用 MinGW-w64 GCC，并确保 `gcc` 已加入 `PATH`。Linux 需要 GCC、标准 C 运行库、数学库、Locale 和 `iconv` 支持。

以下命令均假设当前仓库根目录为 `TTF/main`。

### Main 1：TTF 渲染为 BMP

入口位于 `font/main.c`，用于独立演示字体解析、字形加载、缩放排版、实心绘制和 BMP 输出。

该程序默认从当前目录读取 `simhei.ttf`，因此需要进入 `font` 目录运行。

#### Windows

```powershell
cd font
gcc -std=c11 -O2 main.c font_renderer.c ttf.c ttf_cache.c ttf_preload.c font_draw.c encoding.c bmp.c -o main.exe -lm
.\main.exe "Hello, World 你好，世界"
```

默认生成 `font/out.bmp`，画布为 2000×2000，目标字高为 100，字符串位于画布中心。

自定义字体、字号和画布：

```powershell
.\main.exe "字体渲染 Font" --font fonts\NotoSerifSC-VF.ttf --output result.bmp --font-height 160 --canvas-width 1600 --canvas-height 1200
```

选择批量预加载策略：

```powershell
.\main.exe "Hello 你好" --strategy preload
```

#### Linux

```bash
cd font
gcc -std=c11 -O2 main.c font_renderer.c ttf.c ttf_cache.c ttf_preload.c font_draw.c encoding.c bmp.c -o main -lm
LANG=C.UTF-8 ./main "Hello, World 你好，世界"
```

如果系统将 `iconv` 作为独立库提供，可在编译命令末尾增加 `-liconv`。

查看全部参数：

```powershell
.\main.exe --help
```

### Main 2：BMP 转换为 JPEG

入口位于 `jpeg/main.c`，用于独立验证 BMP 读取和 JPEG 编码。输入必须是未压缩的 24 位 BMP，输出为 Baseline JPEG 4:2:0。

命令格式：

```text
main <input.bmp> [output.jpg] [quality]
```

`output.jpg` 缺省为 `out.jpg`，质量范围为 1～100，缺省质量为 90。

#### Windows

先使用 Main 1 生成 `font/out.bmp`，再进入 `jpeg` 目录：

```powershell
cd jpeg
gcc -std=c11 -O2 main.c bmp_reader.c jpg.c -o main.exe -lm
.\main.exe ..\font\out.bmp out.jpg 90
```

#### Linux

```bash
cd jpeg
gcc -std=c11 -O2 main.c bmp_reader.c jpg.c -o main -lm
./main ../font/out.bmp out.jpg 90
```

该入口会根据 BMP 像素偏移读取像素区，处理四字节行补齐、正负高度、上下行方向及 BGR→RGB 通道转换。

### Main 3：完整文本渲染流水线

入口位于根目录 `main.c`，用于组合字体渲染器、BMP 写入器和 JPEG 编码器。它直接复用同一份 RGB 缓冲区生成两种格式，不会把 BMP 再从磁盘读回。

#### Windows

```powershell
gcc -std=c11 -O2 main.c font\font_renderer.c font\ttf.c font\ttf_cache.c font\ttf_preload.c font\font_draw.c font\encoding.c font\bmp.c jpeg\jpg.c -I font -I jpeg -o main.exe -lm
.\main.exe "Hello, World 你好，世界"
```

默认读取 `font/simhei.ttf`，同时生成根目录下的 `out.bmp` 和 `out.jpg`。JPEG 缺省质量为 90。

只生成 JPEG：

```powershell
.\main.exe "Hello 你好" --format jpeg --jpeg-output result.jpg --quality 85
```

同时指定字体、画布和加载策略：

```powershell
.\main.exe "字体渲染 Font Rendering" --font font\simhei.ttf --format both --bmp-output result.bmp --jpeg-output result.jpg --quality 90 --font-height 180 --canvas-width 1600 --canvas-height 1200 --strategy cache --cache-capacity 128
```

#### Linux

```bash
gcc -std=c11 -O2 main.c font/font_renderer.c font/ttf.c font/ttf_cache.c font/ttf_preload.c font/font_draw.c font/encoding.c font/bmp.c jpeg/jpg.c -I font -I jpeg -o main -lm
LANG=C.UTF-8 ./main "Hello, World 你好，世界"
```

根目录入口的主要参数如下：

| 参数 | 作用 | 缺省值 |
| --- | --- | --- |
| `--font <file>` | 指定 TTF 文件 | `font/simhei.ttf` |
| `--format <bmp\|jpeg\|both>` | 选择输出格式 | `both` |
| `--bmp-output <file>` | 指定 BMP 输出路径 | `out.bmp` |
| `--jpeg-output <file>` | 指定 JPEG 输出路径 | `out.jpg` |
| `--quality <1-100>` | 指定 JPEG 质量 | `90` |
| `--font-height <number>` | 指定缩放后的目标字高 | `100` |
| `--canvas-width <number>` | 指定画布宽度 | `2000` |
| `--canvas-height <number>` | 指定画布高度 | `2000` |
| `--center-x <number>` | 指定字符串中心横坐标 | 画布中心 |
| `--center-y <number>` | 指定字符串中心纵坐标 | 画布中心 |
| `--strategy <cache\|preload>` | 选择字形加载策略 | `cache` |
| `--cache-capacity <number>` | 指定缓存容量 | `64` |

## 项目目录结构

```text
.
├── main.c                         Main 3：完整文本渲染与图像输出入口
├── font/
│   ├── main.c                     Main 1：TTF 渲染为 BMP
│   ├── font_renderer.c/.h         高层渲染流程与资源生命周期
│   ├── ttf.c/.h                   TTF 表解析、Unicode 映射与字形读取
│   ├── ttf_cache.c/.h             固定容量按需字形缓存
│   ├── ttf_preload.c/.h           指定 Unicode 范围批量预加载
│   ├── font_draw.c/.h             缩放、排版、贝塞尔绘制与实心填充
│   ├── encoding.c/.h              UTF-8、GBK 与系统编码转换
│   ├── bmp.c/.h                   24 位 BMP 输出
│   ├── simhei.ttf                 缺省测试字体
│   └── fonts/                     其他测试字体
├── jpeg/
│   ├── main.c                     Main 2：24 位 BMP 转 JPEG
│   ├── bmp_reader.c/.h            BMP 头解析与 RGB 像素读取
│   └── jpg.c/.h                   Baseline JPEG 4:2:0 编码器
├── docs/
│   ├── TTF提取思路.md             TTF 文件结构与字模数据提取原理
│   ├── ttf代码实现思路.md         ttf.c/.h 的解析、查询与字形解码流程
│   ├── font_draw代码实现思路.md   缩放、排版、轮廓绘制与实心填充流程
│   └── JPEG代码实现思路.md        BMP 适配与 JPEG 编码完整流程
├── tests/                          字体、绘制、缓存、BMP 和 JPEG 测试
├── AGENTS.md                       项目代码规范
└── README.md                       项目说明
```

## 实现文档

README 用于介绍项目能力和启动方式。`docs/` 中的文档进一步解释文件格式、代码结构、数据流、内存所有权和关键算法。

| 文档 | 建议阅读场景 |
| --- | --- |
| [TTF 提取思路](docs/TTF提取思路.md) | 了解 SFNT 表目录、关键 TTF 表结构和字模提取原理 |
| [`ttf.c/.h` 代码实现思路](docs/ttf代码实现思路.md) | 沿代码理解字体打开、Unicode 映射、`loca/glyf/hmtx` 查询和字形解码 |
| [`font_draw.c/.h` 代码实现思路](docs/font_draw代码实现思路.md) | 理解缩放、字符串居中、贝塞尔采样、扫描线填充和画布裁剪 |
| [JPEG 代码实现思路](docs/JPEG代码实现思路.md) | 理解 BMP→RGB、4:2:0 MCU、DCT、量化、Zigzag、Huffman 和 Bit 流 |

推荐按“TTF 提取思路 → TTF 代码实现 → font_draw 代码实现 → JPEG 代码实现”的顺序阅读。前三份文档解释文字如何变成 RGB，最后一份解释 RGB 如何压缩为 JPEG。

## 项目亮点详解

### 1. 自行解析 TTF 核心表

程序读取 SFNT 文件头和表目录，再解析字体渲染需要的 `head`、`cmap`、`loca`、`glyf`、`hhea` 与 `hmtx` 表。

`glyf` 通常是字体中体积最大的表。程序保留它在文件中的位置，并在请求字形时按 `loca` 偏移读取，避免启动阶段复制完整字形表。

### 2. 优先使用完整 Unicode 映射

程序优先选择 `cmap Format 12`，支持 32 位 Unicode；字体不存在 Format 12 时，再回退到基本多文种平面的 Format 4。

Format 12 使用分组和二分搜索定位 Glyph Index。Format 4 则根据 Segment、`idDelta`、`idRangeOffset` 与 `glyphIndexArray` 还原映射。

### 3. 解压 TrueType 简单字形

简单字形的 Flag 使用重复压缩，坐标保存为相对增量。程序展开重复 Flag，再分别解码 x、y 坐标流并累加为真实位置。

轮廓还原同时处理短坐标、正负增量、不变坐标、轮廓终点，以及两个连续曲外控制点之间的隐式中点。

当前尚未展开复合字形，但解析器会安全识别并拒绝，避免把复合字形数据误当作简单字形而产生越界或死循环。

### 4. 从矢量轮廓生成实心位图

TrueType 轮廓由直线和二次贝塞尔曲线组成。程序将轮廓采样为有方向的边，并计算每条扫描线与轮廓边的交点。

实心区域按照非零环绕规则填充，可处理外轮廓、内部孔洞和方向相反的子轮廓。写入画布前还会转换字体坐标系与位图行方向。

### 5. 使用真实字体度量排版

程序从 `hmtx` 获取每个字形的 `advanceWidth`，使用真实前进宽度排列字符串，不假设所有字符拥有相同宽度。

输入坐标表示整个字符串的中心。程序先计算缩放后的字符串总宽度，再确定首字符位置；超出画布可用宽度的尾部字符会被安全截断。

缩放参数表示最终字体排版框高度，而不是缩放比例。调用者可以直接使用 50、100 或 200 等目标像素高度。

### 6. Cache 与 Preload 双策略

Cache 策略使用固定容量数组按需加载字形，命中时直接复用，容量满时按照 FIFO 顺序淘汰，适合字符集合不固定或内存受限的场景。

Preload 策略一次加载指定 Unicode 范围。缺省范围覆盖 ASCII、通用标点、中文标点、常用汉字和全角字符，适合字体固定且需要重复绘制的场景。

两种策略都实现统一的 `font_glyph_source` 接口，绘制层不需要感知字形来自缓存还是预加载集合。

### 7. 自行实现 Baseline JPEG 4:2:0

JPEG 编码器首先将 RGB 转换为 YCbCr，再按 16×16 MCU 组织数据。每个 MCU 包含 4 个 8×8 Y 块、1 个 Cb 块和 1 个 Cr 块。

Cb、Cr 由对应 2×2 RGB 区域平均得到。图像尺寸不是 16 的倍数时，编码器复制最邻近的边缘像素完成 MCU 补齐。

各分量依次经过二维 DCT、质量参数量化、Zigzag 排列、DC 差分、AC 游程编码及标准 Huffman 编码。

输出包含 SOI、APP0、DQT、SOF0、DHT、SOS 和 EOI，并处理熵编码中的 `0xff` 字节填充。生成文件已经过标准图像解码器验证。

### 8. 三个入口对应三个验证层级

Main 1 只组合字体模块与 BMP 输出，适合排查 TTF 解析和绘制问题。Main 2 只组合 BMP 读取与 JPEG 编码，适合排查图像压缩问题。

Main 3 直接组合整个项目。字体渲染器返回一份由调用者管理的 RGB 缓冲区，BMP 和 JPEG 输出共享该缓冲区，避免中间文件读写与颜色通道重复转换。

### 9. 编码与跨平台边界分离

UTF-8 由项目代码自行校验并转换。Windows 分支使用系统代码页并提供 GBK 兜底，Linux 分支使用 Locale 与 `iconv`。

TTF 解析、轮廓绘制、BMP 输出和 JPEG 编码主体均使用标准 C 接口，没有硬编码 Windows 绝对路径。

Windows 已完成实际编译和回归测试。Linux 源码具备适配分支，使用 UTF-8 Locale，并在链接时加入 `-lm`；部分系统还需要 `-liconv`。

当前实现边界也应明确：TTF 解析器支持简单字形并安全拒绝尚未展开的复合字形；JPEG 编码器固定输出 8 位 4:2:0 Baseline JPEG，不支持 Progressive、EXIF、ICC 和自适应 Huffman 表。

这些限制不影响当前“中英文字符串 → RGB/BMP/JPEG”和“24 位 BMP → JPEG”两条完整主链。具体实现与后续扩展方向可继续参阅上面的四份实现文档。
