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

typedef struct{
	u32	begin;
	u32	end;
}unicode_range;

typedef struct{
	glyph_point_data*	glyph_array;
	int			glyph_count;
	font_box		box;
}ttf_font_data;


// 功能：将简单字形原始数据解析为轮廓点数组
int glyph_to_point(u8* glyph_data, point** pp_point_data, int glyph_length);
// 返回解析后的点数量

// 功能：加载字体中的中文 BMP 字形数据及统一排版框
void load_ttf_BMP(char* file, glyph_point_data** glyph_array, font_box* box);

// 功能：按照多个 Unicode 范围加载字形数据和统一排版框
int load_ttf_ranges(char* file, const unicode_range* range_array, int range_count, ttf_font_data* font);

// 功能：释放范围加载得到的全部字形数据
void free_ttf_font_data(ttf_font_data* font);


#endif
