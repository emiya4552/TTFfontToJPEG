#ifndef TTF_H
#define TTF_H


#include<stdint.h>


typedef unsigned int 	u32;
typedef unsigned short 	u16;
typedef int		s32;
typedef short		s16;
typedef uint64_t	u64;
typedef char		s8;
typedef unsigned char	u8;


#define NEXT 			10000
#define UNICODE_BMP_CHINA_BEGIN	0x4E00
#define UNICODE_BMP_CHINA_END	0x9FA4
#define UNICODE_BMP_NUMBER	(UNICODE_BMP_CHINA_END-UNICODE_BMP_CHINA_BEGIN+1)


typedef struct{
	float 	x;
	float 	y;
	int 	locate;
}point;

typedef struct{
	u32 	unicode;
	u8*	glyph_data;
	int	glyph_length;
	u16	advance_width;
}glyph_point_data;

typedef struct{
	int	x_min;
	int	y_min;
	int	x_max;
	int	y_max;
}font_box;

typedef glyph_point_data* (*font_glyph_getter)(void* context, u32 unicode);

typedef struct{
	void*			context;
	font_box		box;
	font_glyph_getter	get_glyph;
}font_glyph_source;

typedef struct ttf_font ttf_font;


// 功能：打开字体文件并建立可复用的 TTF 解析上下文
int ttf_font_open(const char* file, ttf_font** pp_font);

// 功能：关闭 TTF 解析上下文并释放其持有的数据
void ttf_font_close(ttf_font* font);

// 功能：读取字体的统一排版框
int ttf_font_get_box(ttf_font* font, font_box* box);

// 功能：按 Unicode 从已经打开的字体中加载一个独立字形
int ttf_font_load_glyph(ttf_font* font, u32 unicode, glyph_point_data* glyph);

// 功能：释放单个独立字形持有的原始数据
void ttf_glyph_free(glyph_point_data* glyph);


// 功能：将简单字形原始数据解析为轮廓点数组
int glyph_to_point(u8* glyph_data, point** pp_point_data, int glyph_length);
// 返回解析后的点数量


// 保留旧代码只包含 ttf.h 时对预加载接口的可见性
#include"ttf_preload.h"


#endif
