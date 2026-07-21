
#ifndef TTF_H
#define TTF_H



#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<locale.h>
#include<stddef.h>
#include<wchar.h>
#include<math.h>
#include<stddef.h>
#include"encoding.h"

typedef	unsigned int 	u32;
typedef unsigned short 	u16;
typedef int		s32;
typedef short		s16;
typedef uint64_t	u64;
typedef char		s8;
typedef unsigned char	u8;

// 大端读取成小端

#define BIG_TO_LITTLE_32(A)	((( (A)&0xFF000000) >>24)	|\
				 (( (A)&0x00FF0000) >> 8)	|\
				 (( (A)&0x0000FF00) << 8)  	|\
				 (( (A)&0x000000FF) <<24))
 
#define BIG_TO_LITTLE_16(A)	((((A)&0xFF00)>>8)|(((A)&0x00FF)<<8))

#define BIG_TO_LITTLE_64(A)	((( (A) & 0xFF00000000000000)>>56)	|\
				 (( (A) & 0x00FF000000000000)>>40) 	|\
				 (( (A) & 0x0000FF0000000000)>>24) 	|\
				 (( (A) & 0x000000FF00000000)>> 8) 	|\
				 (( (A) & 0x00000000FF000000)<< 8) 	|\
				 (( (A) & 0x0000000000FF0000)<<24) 	|\
				 (( (A) & 0x000000000000FF00)<<40) 	|\
				 (( (A) & 0x00000000000000FF)<<56))

// 根据位数选择对应的大小端转换方式
#define convert(A,B)	((A) = 	(B == 16) ? BIG_TO_LITTLE_16(A) : \
				((B == 32) ? BIG_TO_LITTLE_32(A) : \
				((B == 64) ? BIG_TO_LITTLE_64(A) : (A))))

// cmap 表的平台编号与编码编号
#define PLATFORM_ID_WINDOWS 		3
#define ENCODING_ID_3_BMP 		1
#define ENCODING_ID_3_UNICODE_FULL 	10


#define COLOR_DATA(x, y, z) color_data[x*width*3 + y*3 + z]


#define BIT_PER_BYTE 		8
#define NEXT 			10000
#define MAX_CONTROL_POINT 	10
#define OUT_COUNT 		1000
#define UNICODE_BMP_CHINA_BEGIN	0x4E00
#define	UNICODE_BMP_CHINA_END	0x9FA4

#define UNICODE_BMP_NUMBER	(UNICODE_BMP_CHINA_END-UNICODE_BMP_CHINA_BEGIN+1)


#pragma pack(1)
// TTF 表目录项
typedef	struct{
	char	tag[4];
	u32	check_sum;
	u32	offset;
	u32	length;
}table_entry;

// TTF 文件头
typedef struct{
	u32	version;
	u16	table_number;
	u16	search_range;
	u16	entry_selector;
	u16	range_shift;
}file_header;

// TTF 表数据
typedef struct{
	char 	name[4];
	u32	length;
	u8*	data;
} table;

// head 表数据
typedef struct{
	u32		version;
	u32		font_revision;
	u32		check_sum_adjustment;
	u32		magic_number;
	u16		flags;
	u16		unit_per_Em;
	u64		created;
	u64		modified;
	s16		xMin;
	s16		yMin;
	s16		xMax;
	s16		yMax;
	u16		mac_style;
	u16		lowest_RecPPRM;
	s16		font_direction_hint;
	s16		index_to_loca_format;
	s16		glyph_data_format;
}head_table;
	


// cmap 子表头
typedef struct{
	u16		platform_id;
	u16		encoding_id;
	u32		offset;
}cmap_subtable_header;

// cmap Format 4 子表头
typedef struct{
	u16	format;
	u16	length;
	u16	language;
	u16	segment_count_X2;
	u16	search_range;
	u16	entry_selector;
	u16	range_shift;
}cmap_format4_header;

typedef struct{
	u16*	end_code;
	u16*	start_code;
	u16*	id_delta;
	u16*	id_range_offset;
	u16*	glyph_index_array;
}cmap_format4_data;

typedef struct{
	u32	start_char_code;
	u32	end_char_code;
	u32	start_glyph_id;
}sequential_map_group;

typedef struct{
	u16			format;
	u16			reserved;
	u32			length;
	u32			language;
	u32			groups_number;
	sequential_map_group* 	groups;
}cmap_format12_data;

typedef struct{
	s16 	number_of_contours;
	s16	x_min;
	s16  	y_min;
	s16	x_max;
	s16	y_max;
}glyph_data_header;

typedef struct{
	float 	x;
	float 	y;
	int 	locate;
}point;

typedef struct{
	float 	x;
	float	y;
}bezier_point;

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
	char*	name;
	u8* 	color_data;
	u8*	color;
	int	width;
	int 	height;
}bmp_data;

#pragma pack()

// 功能：从 TTF 文件读取渲染所需的关键表
void read_ttf_data(char* file, table** pp_data);

// 功能：解析 head 表中的字体全局信息
void read_head_table(u8* head_table_data, head_table* head_table_struct);

// 功能：从 cmap 表选择并复制可用的字符映射子表
u16 get_cmap_subtable_data(u8* cmap_table_data, u8** pp_cmap_subtable); 

// 功能：解析 cmap Format 4 子表
u16 read_cmap_format4_subtable(u8* cmap_subtable_data, cmap_format4_data* p_data);
// 返回 Format 4 的分段数量

// 功能：通过 cmap Format 4 获取字形索引
int get_glyph_index_format4(cmap_format4_data format4, u32 unicode, u16 segment_count);

// 功能：解析 cmap Format 12 子表
void read_cmap_format12_subtable(u8* cmap_subtable_data, cmap_format12_data* p_data);

// 功能：通过 cmap Format 12 获取字形索引
int get_glyph_index_format12(cmap_format12_data format12, u32 unicode);

// 功能：读取 cmap 表并解析选中的字符映射格式
int read_cmap(u8* cmap_table_data, cmap_format4_data* p_format4, cmap_format12_data* p_format12, u16* segment_count);

// 功能：根据 cmap 格式获取 Unicode 对应的字形索引
int get_glyph_index(cmap_format4_data format4, cmap_format12_data format, u16 segment_count, u32 unicode, int get_format_condition);

// 功能：解析 loca 表并生成字形偏移数组
int read_loca_table(u8* data, u16 index_to_loca_format, u32** pp_result, u32 table_length);

// 功能：从 glyf 表复制指定字形的原始数据
void get_glyph_data(u8* glyf_data, u32 glyph_data_offset, u8** pp_glyph_data, int glyph_length);

// 功能：补充连续控制点之间的隐式点并生成可绘制点数组
int process_point(point* raw_point_data, point** pp_point_data, int raw_point_length);
// 返回处理后的点数量

// 功能：将简单字形原始数据解析为轮廓点数组
int glyph_to_point(u8* glyph_data, point** pp_point_data, int glyph_length);
// 返回解析后的点数量

// 功能：计算贝塞尔曲线上指定参数位置的点
bezier_point bezier_data_get(float t, point* points, int count);

// 功能：对一段贝塞尔曲线进行采样
void get_bezier(point* in, int count, bezier_point** pp_out);

// 功能：将一段贝塞尔曲线绘制到 BMP 像素缓冲区
void draw_bezier_to_bitmap(int width, int height, u8* color_data, u8 color[3], point* point_data, int count, int offset_x, int offset_y);

// 功能：根据轮廓点绘制空心字形
void draw_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y);

// 功能：根据轮廓点绘制实心字形
void draw_filled_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y);

// 功能：加载字体中的中文 BMP 字形数据及统一排版框
void load_ttf_BMP(char* file, glyph_point_data** glyph_array, font_box* box);

// 功能：以指定排版框中心绘制单个空心字符
// center_x 距画布左侧，center_y 距画布底部
void draw_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：以指定排版框中心绘制单个实心字符
void draw_filled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：从左向右绘制空心字符串
void draw_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：从左向右绘制实心字符串
void draw_filled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y);
#endif
