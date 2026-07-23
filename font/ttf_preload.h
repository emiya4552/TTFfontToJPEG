#ifndef TTF_PRELOAD_H
#define TTF_PRELOAD_H


#include"ttf.h"


// 默认预加载范围数量
#define TTF_PRELOAD_DEFAULT_RANGE_COUNT		5

// 基本英文与 ASCII 字符范围
#define TTF_PRELOAD_ASCII_BEGIN			0x0020
#define TTF_PRELOAD_ASCII_END			0x007E

// 通用标点字符范围
#define TTF_PRELOAD_GENERAL_PUNCTUATION_BEGIN	0x2000
#define TTF_PRELOAD_GENERAL_PUNCTUATION_END	0x206F

// 中文标点字符范围
#define TTF_PRELOAD_CJK_PUNCTUATION_BEGIN	0x3000
#define TTF_PRELOAD_CJK_PUNCTUATION_END		0x303F

// 常用汉字范围
#define TTF_PRELOAD_COMMON_CJK_BEGIN		0x4E00
#define TTF_PRELOAD_COMMON_CJK_END		0x9FA4

// 全角与半角字符范围
#define TTF_PRELOAD_FULLWIDTH_BEGIN		0xFF00
#define TTF_PRELOAD_FULLWIDTH_END		0xFFEF


typedef struct{
	u32	begin;
	u32	end;
}unicode_range;

typedef struct{
	glyph_point_data*	glyph_array;
	int			glyph_count;
	font_box		box;
}ttf_preload;

// 旧名称保留用于兼容已有绘制接口
typedef ttf_preload ttf_font_data;


// 功能：通过已经打开的字体按多个 Unicode 范围批量加载字形
int ttf_preload_ranges(ttf_font* ttf, const unicode_range* range_array, int range_count, ttf_preload* preload);

// 功能：释放批量预加载得到的全部字形数据
void ttf_preload_free(ttf_preload* preload);

// 功能：打开字体并按照多个 Unicode 范围批量加载字形
int load_ttf_ranges(const char* file, const unicode_range* range_array, int range_count, ttf_font_data* font);

// 功能：释放批量预加载得到的全部字形数据
void free_ttf_font_data(ttf_font_data* font);

// 功能：加载字体中的中文 BMP 字形数据及统一排版框
void load_ttf_BMP(char* file, glyph_point_data** glyph_array, font_box* box);

// 功能：将批量预加载结果包装为绘制层使用的统一字形来源
int ttf_preload_get_source(ttf_preload* preload, font_glyph_source* source);


#endif
