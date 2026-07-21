# TTF 字体渲染与 JPEG 编码核心源码

这个目录最初从旧项目中按文件粒度抽离，目前已作为独立源码目录继续开发。

整理原则：

- 不修改旧项目中的任何文件。
- 初始 7 个核心文件的来源和原始哈希保留在本文末尾，`main` 中的字体代码已在此基础上继续修改。
- 所有新增和修改只发生在 `TTF/main`，旧目录 `TTF/ttf_3` 和 `TTF/jpg/jpeg_test` 不修改。
- 不包含 IJG/libjpeg 源码、头文件、静态库或构建产物。
- 不包含旧测试目录中的中间数据、可执行文件、字体文件和图片样例。

## 目录结构

```text
main/
├─ font/
│  ├─ main.c       # TTF 渲染示例入口
│  ├─ encoding.c   # UTF-8、GBK及系统编码转换
│  ├─ encoding.h   # 字符编码类型与转换接口
│  ├─ ttf.c        # TTF 表解析、字形映射、轮廓解析和贝塞尔绘制
│  ├─ ttf.h        # TTF 数据结构与函数声明
│  ├─ bmp.c        # 24 位 BMP 文件生成
│  └─ bmp.h        # BMP 结构与函数声明
├─ jpeg/
│  ├─ jpg.c        # BMP 读取、JPEG 编码流程及当前命令行入口
│  └─ jpg.h        # JPEG 数据结构、量化表、Huffman 表与函数声明
├─ tests/
│  ├─ test_character_input.c
│  └─ test_orientation.c
├─ .gitignore
└─ README.md
```

## 初始原文件映射

| 新路径 | 原路径 | 用途 |
| --- | --- | --- |
| `main/font/main.c` | `TTF/ttf_3/main.c` | TTF 渲染示例入口 |
| `main/font/ttf.c` | `TTF/ttf_3/ttf.c` | TTF 核心解析与轮廓绘制 |
| `main/font/ttf.h` | `TTF/ttf_3/ttf.h` | TTF 类型及公开函数 |
| `main/font/bmp.c` | `TTF/ttf_3/bmp.c` | BMP 生成 |
| `main/font/bmp.h` | `TTF/ttf_3/bmp.h` | BMP 文件结构及声明 |
| `main/jpeg/jpg.c` | `TTF/jpg/jpeg_test/test5/jpg.c` | BMP 读取与 JPEG 编码核心流程 |
| `main/jpeg/jpg.h` | `TTF/jpg/jpeg_test/test5/jpg.h` | JPEG 编码数据与声明 |

## 当前包含的能力

### TTF → BMP

- 读取 TTF/SFNT 表目录及大端字段。
- 解析 `head`、`cmap`、`loca`、`glyf` 等关键表。
- 支持 `cmap Format 4/12` 的 Unicode 到 Glyph ID 映射。
- 解析简单字形轮廓、坐标增量和 on-curve/off-curve 点。
- 进行贝塞尔曲线采样并生成 24 位 BMP。
- 读取 `head.unitsPerEm` 与 `hhea` 垂直度量，使用字体统一排版方框定位；SimHei 的汉字方框为 256×256。
- `center_x` 表示方框中心距画布左侧的距离，`center_y` 表示方框中心距画布底部的距离；绘制时会转换为 BMP 的行列坐标。
- `draw_word` 接收单个字符及其编码，例如 `draw_word(glyph_array, box, "保", WORD_ENCODING_UTF8, bmp, 500, 500)`；支持 UTF-8、GBK 和系统编码，空输入、非法编码和多个字符会被拒绝。
- `draw_string` 手动解析 `hmtx.advanceWidth`，从首字符方框中心开始横向排版，并逐字调用 `draw_word`；下一个完整方框超出 BMP 时直接截断剩余字符。
- `draw_filled_string` 复用字形轮廓与字符串排版，通过非零环绕扫描线生成实心字形，并保留汉字内部孔洞。

### BMP → JPEG

- 读取 24 位 BMP 像素数据。
- RGB 转 YCbCr、MCU/数据块组织、DCT 和量化。
- Zig-Zag、DC 差分、AC 游程和 Huffman 编码。
- JPEG 标记段与压缩数据写入代码。

JPEG 模块保留的是旧项目中最后一版自研编码器源码，适合作为继续开发的起点；本目录没有引入 `jpeg_source` 或 `libjpeg` 中的实现。

## 继续开发前需要知道

- `jpeg/jpg.c` 仍含 `encode`、`encode2`、`eee` 等旧调试文件写入逻辑，后续可在建立测试后删除。
- JPEG 编码数据缓冲区、有效字节数和入口调用方式尚需重新统一；本轮没有修复这些旧接口。
- 上述状态与 Windows/Linux 环境无关，README 仅记录后续再开发的优先切入点。

## 字体程序构建

Windows：

```powershell
gcc main.c ttf.c bmp.c encoding.c -o main.exe -lm
.\main.exe 马 1000 1000
```

Linux：

```text
gcc main.c ttf.c bmp.c encoding.c -o main -lm
./main 马 1000 1000
```

`WORD_ENCODING_SYSTEM` 会在中文 Windows 下使用当前 ANSI 代码页，在 Linux 下使用当前 Locale。Linux 显式转换 GBK 时使用 `iconv`；部分非 glibc 环境可能需要额外链接 `-liconv`。

## 没有迁入的内容

以下内容不是继续开发所必需，或属于第三方/构建/测试资料，因此没有复制：

- `TTF/jpg/jpeg_source/**`：IJG/libjpeg 源码及安装产物。
- `TTF/jpg/jpeg_test/test1/**`、`test2/**`：以 libjpeg 和分阶段实验为主的旧测试代码。
- `TTF/jpg/jpeg_test/test3/**`、`test4/**`：旧版本或过渡版本。
- `TTF/jpg/jpeg_test/test5/test.c`、`1.h`：调试入口和临时数据，不属于最终最小编码模块。
- `*.o`、`*.a`、旧可执行文件、Makefile：依赖旧目录布局或属于构建产物。
- `test_*.txt`、`encode*`、`emit_time`、`eee`：阶段性调试输出。
- `*.ttf`、`*.ttc`、`*.bmp`、`*.jpg`：初始抽离时未迁入；本地开发可自行放置，且已由 `.gitignore` 排除。

## 推荐的后续整理顺序

1. 保留本文的来源映射和旧目录哈希，便于追溯初始实现。
2. 将 `font/main.c` 和 `jpeg/jpg.c` 中的命令行入口移到单独的 `examples/` 或 `tools/`。
3. 为字体与 JPEG 模块分别设计一个小型公开接口，隐藏内部结构和临时缓冲区。
4. 增加统一的 CMake 构建，并在 Linux 下开启编译警告。
5. 增加少量固定样例和端到端测试，再逐步清理旧调试代码。

## 旧目录原始文件 SHA-256

| 原文件 | SHA-256 |
| --- | --- |
| `TTF/ttf_3/ttf.c` | `E2D201D92F47716B6B4F825E2578E7A573956E11E7E201FC2B970DB24FA35070` |
| `TTF/ttf_3/ttf.h` | `0742F9053C5C47C8F810176565C3CA890F7D365B4587605C76927D7B7A5FD061` |
| `TTF/ttf_3/bmp.c` | `CBB9ABB5AA3354664D9F666312C28E0B5BC78D6C92261BFA48D9339D9A3C1345` |
| `TTF/ttf_3/bmp.h` | `9F8093F78D64CD973DD7A7850479CB8F69242E6CC2F60809DDF2EBD1C2C2BA77` |
| `TTF/ttf_3/main.c` | `946B7CEA374C23F7AAB24D8D02B54AEF144F6ACFB36473C5F4E8A199DC3043A2` |
| `TTF/jpg/jpeg_test/test5/jpg.c` | `C3696E20A19D4650200C29F872995EE124769A21E4425879D021037DE048F6A7` |
| `TTF/jpg/jpeg_test/test5/jpg.h` | `35E342701BB6720B87704FB790E51B46F113DB0A8B6CF4652B3AD821C3BC1D9C` |
