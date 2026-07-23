# TTF 字体解析与轻量渲染

本项目使用 C 语言直接解析 TrueType 字体文件，在不依赖 FreeType、HarfBuzz 或 stb_truetype 等字体库的情况下，将中英文字符串渲染为实心 BMP 图像。

项目覆盖从 Unicode 映射、字形定位、轮廓解压，到缩放、排版、扫描线填充和 BMP 输出的完整链路，并提供按需缓存与批量预加载两种字形管理策略。

```text
字符串
  ↓ 编码转换
Unicode
  ↓ cmap
Glyph Index
  ↓ loca + glyf + hmtx
轮廓点与前进宽度
  ↓ 缩放、排版、贝塞尔曲线与扫描线填充
24 位 BMP
```

## 简要亮点

- 使用基础 C 库自行解析 TTF，不依赖第三方字体引擎。
- 支持 `cmap Format 12`，不存在时回退到 `Format 4`。
- 支持英文、中文、常用标点及中英文混合字符串。
- 正确展开 `glyf` 重复 Flag、相对坐标和隐式曲上点。
- 根据 `hmtx` 前进宽度排版，而不是使用固定字符间距。
- 支持目标字高缩放、字符串整体居中、画布裁剪和实心填充。
- 提供 `cache` 按需缓存与 `preload` 批量预加载两种策略。
- 提供高层渲染接口和带缺省值的命令行入口。
- Windows 与 Linux 使用独立编码适配分支。

## 快速启动

### Windows

需要安装 MinGW-w64 或其他提供 GCC 的 C 编译环境，并确保 `gcc` 已加入 `PATH`。

在 PowerShell 中进入 `font` 目录：

```powershell
cd font
gcc -std=c11 -O2 main.c font_renderer.c ttf.c ttf_cache.c ttf_preload.c font_draw.c encoding.c bmp.c -o main.exe
```

只传入待绘制文本即可启动：

```powershell
.\main.exe "Hello, World 你好，世界"
```

默认使用当前目录下的 `simhei.ttf`，生成 `out.bmp`。默认画布为 `2000 × 2000`，目标字高为 100，字符串位于画布中心。

选择预加载策略：

```powershell
.\main.exe "Hello 你好" --strategy preload
```

自定义字体、输出文件和画布：

```powershell
.\main.exe "Hello 你好" --font simhei.ttf --output result.bmp --font-height 160 --canvas-width 1600 --canvas-height 1200
```

### Linux

项目源码具备 Linux 适配分支。需要 GCC、标准 C 运行库、数学库，以及系统提供的 `iconv` 和 Locale 支持。

Debian、Ubuntu 等使用 glibc 的发行版通常不需要额外链接 `libiconv`。进入 `font` 目录后执行：

```bash
cd font
gcc -std=c11 -O2 main.c font_renderer.c ttf.c ttf_cache.c ttf_preload.c font_draw.c encoding.c bmp.c -o main -lm
```

确保终端使用 UTF-8 Locale，再运行程序：

```bash
LANG=C.UTF-8 ./main "Hello, World 你好，世界"
```

如果系统把 `iconv` 作为独立库提供，链接失败时可在编译命令末尾增加 `-liconv`。

#### Linux 支持分析

字体文件读取、内存管理、字符串处理和 BMP 输出均使用标准 C 接口，没有硬编码 Windows 路径。

Windows 专用的 `windows.h` 和代码页转换被限制在 `_WIN32` 分支内。Linux 分支使用 `setlocale`、`mbrtowc` 和 `iconv` 完成系统编码及 GBK 转换。

因此，项目可以在常见的 x86-64、ARM64 小端 Linux 环境中编译运行，但需要注意以下条件：

- 系统 Locale 应设置为 UTF-8，否则中文命令行输入可能无法解析。
- Linux 文件名区分大小写，传入的字体路径必须与实际文件名一致。
- `font_draw.c` 和 `bmp.c` 使用数学函数，Linux 链接时需要 `-lm`。
- 当前大小端转换按常见小端主机设计，不建议直接用于大端 Linux 系统。

## 命令行参数

```text
main <text> [options]
```

`text` 是唯一必填参数。包含空格时，需要使用引号包裹。

| 参数 | 作用 | 缺省值 |
| --- | --- | --- |
| `--font <file>` | 指定 TTF 文件 | `simhei.ttf` |
| `--output <file>` | 指定 BMP 输出文件 | `out.bmp` |
| `--font-height <number>` | 指定缩放后的目标字高 | `100` |
| `--canvas-width <number>` | 指定画布宽度 | `2000` |
| `--canvas-height <number>` | 指定画布高度 | `2000` |
| `--center-x <number>` | 指定字符串中心的横坐标 | 画布水平中心 |
| `--center-y <number>` | 指定字符串中心的纵坐标 | 画布垂直中心 |
| `--strategy <cache\|preload>` | 选择字形加载策略 | `cache` |
| `--cache-capacity <number>` | 指定缓存容量 | `64` |
| `--help` | 显示帮助信息 | — |

完整示例：

```powershell
.\main.exe "字体渲染 Font Rendering" --font simhei.ttf --output result.bmp --font-height 180 --canvas-width 1600 --canvas-height 1200 --center-x 800 --center-y 600 --strategy cache --cache-capacity 128
```

## 项目结构

```text
.
├── font/                       TTF 解析与字体渲染主模块
│   ├── main.c                  命令行入口与可选参数解析
│   ├── font_renderer.c/.h      高层渲染流程与资源生命周期管理
│   ├── ttf.c/.h                TTF 表解析、Unicode 映射与字形读取
│   ├── ttf_cache.c/.h          固定容量的按需字形缓存
│   ├── ttf_preload.c/.h        指定 Unicode 范围的批量预加载
│   ├── font_draw.c/.h          缩放、排版、贝塞尔绘制和实心填充
│   ├── encoding.c/.h           UTF-8、GBK 与系统编码转换
│   ├── bmp.c/.h                24 位 BMP 文件输出
│   └── simhei.ttf              默认测试字体
├── docs/
│   └── TTF提取思路.md          TTF 表结构与字形提取过程说明
├── tests/                       编码、解析、缩放、填充和缓存回归测试
├── jpeg/                        尚未完成的 JPEG 实验代码
└── README.md                    项目说明与快速启动文档
```

更详细的字体表和轮廓提取过程参见 [TTF 提取思路](docs/TTF提取思路.md)。

## 项目亮点详解

### 1. 自行完成 TTF 核心表解析

程序先读取 SFNT 文件头和表目录，再解析字体渲染所需的 `head`、`cmap`、`loca`、`glyf`、`hhea` 和 `hmtx` 表。

`glyf` 表通常体积最大，程序只保留其文件位置，在真正请求某个字形时才按偏移读取对应数据，避免启动时把整个字形表复制到内存。

### 2. 优先使用完整 Unicode 映射

程序优先选择 `cmap Format 12`，用于处理 32 位 Unicode；字体不存在 Format 12 时，再回退到适用于基本多文种平面的 Format 4。

Format 12 的分组按照字符编码排序，查找时使用二分搜索。Format 4 则按照 Segment、`idDelta`、`idRangeOffset` 和 `glyphIndexArray` 还原字形索引。

### 3. 正确解压 TrueType 简单字形

简单字形中的 Flag 使用重复压缩，坐标则保存为相对增量。程序先把压缩 Flag 展开为与逻辑点数量一致的数组，再依次解码 x、y 坐标流。

轮廓恢复过程同时处理短坐标、正负增量、不变坐标、轮廓终点，以及两个连续曲外控制点之间的隐式中点。

对于当前尚未支持的复合字形，程序会安全拒绝，避免错误解析、越界访问或绘制死循环。

### 4. 从矢量轮廓生成实心位图

TrueType 轮廓由直线和二次贝塞尔曲线组成。程序将轮廓分段采样为有方向的边，再计算每条扫描线与边的交点。

实心区域按照非零环绕规则填充，因此可以同时处理外轮廓、内部孔洞和方向相反的子轮廓。坐标写入 BMP 前还会完成字体坐标系与位图行方向的转换。

### 5. 使用真实字体度量进行字符串排版

程序从 `hmtx` 获取每个字形的 `advanceWidth`，使用真实前进宽度排列字符串，不假设所有字符拥有相同宽度。

输入坐标表示整个字符串的中心。程序先计算缩放后的总宽度，再确定首字符位置；超出画布可用宽度的尾部字符会被安全截断。

目标字号不是缩放比例，而是缩放后的字体排版框高度。调用者可以直接使用 50、100、200 等像素高度，程序内部负责计算缩放系数。

### 6. 支持多种文本编码

UTF-8 由项目代码自行校验并转换为 Unicode。Windows 下通过当前系统代码页处理控制台输入，并支持 GBK 代码页兜底。

Linux 下通过 Locale 解析系统输入，并使用 `iconv` 完成 GBK 到 UTF-8、Unicode 的转换。编码层与 TTF 解析层分离，避免平台补丁进入字体表解析逻辑。

### 7. Cache 与 Preload 双策略

`cache` 是默认策略。它使用固定容量数组按需加载字形，命中时直接复用；容量满时按照 FIFO 顺序淘汰，适合内存有限或文本字符集合不固定的场景。

`preload` 会一次加载指定 Unicode 范围。缺省范围包含 ASCII、通用标点、中文标点、常用汉字和全角字符，适合字体固定并需要重复绘制大量文本的场景。

两种实现都被包装为统一的 `font_glyph_source`。绘制层只依赖统一接口，不需要感知字形来自缓存还是预加载集合。

### 8. 高层渲染模块隐藏资源管理

`font_renderer.c` 负责打开字体、初始化策略、创建画布、调用绘制层、生成 BMP，并在结束时统一释放字体、缓存或预加载资源。

命令行入口只负责读取用户参数。除了文本以外，其余参数都有缺省值，既方便快速演示，也便于后续将高层接口接入显示设备或其他 C 程序。

### 9. 面向异常输入的保护

解析过程检查表长度、字形偏移、轮廓终点、提示指令长度、Flag 重复次数、坐标流边界和内存分配结果。

绘制层在找不到轮廓终点时会停止当前字形，不再重复使用旧轮廓位置，从而避免损坏字体数据导致死循环。


## JPEG 部分状态

`jpeg/` 目录保存早期的 BMP 读取、YCbCr 转换、分块、DCT、量化和 Huffman 编码实验代码，但 JPEG 文件头、熵编码输出和完整资源管理尚未整理完成。

JPEG 模块目前不能作为稳定功能使用，也没有接入 `font_renderer` 或 `main`。当前可运行且经过测试的输出格式只有 24 位 BMP。
