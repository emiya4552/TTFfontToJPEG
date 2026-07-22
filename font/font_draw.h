#ifndef FONT_DRAW_H
#define FONT_DRAW_H


#include"ttf.h"
#include"ttf_preload.h"
#include"encoding.h"


typedef struct{
	char*	name;
	u8* 	color_data;
	u8*	color;
	int	width;
	int 	height;
}bmp_data;


// 功能：将一段贝塞尔曲线绘制到 BMP 像素缓冲区
void draw_bezier_to_bitmap(int width, int height, u8* color_data, u8 color[3], point* point_data, int count, int offset_x, int offset_y);

// 功能：根据轮廓点绘制空心字形
void draw_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y);

// 功能：根据轮廓点绘制实心字形
void draw_filled_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y);

// 功能：以指定排版框中心绘制单个空心字符
// center_x 距画布左侧，center_y 距画布底部
void draw_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：以指定排版框中心绘制单个实心字符
void draw_filled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：按目标字框高度在指定中心绘制单个空心字符
void draw_scaled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：按目标字框高度在指定中心绘制单个实心字符
void draw_scaled_filled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：以指定位置为整体中心从左向右绘制空心字符串
void draw_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：以指定位置为整体中心从左向右绘制实心字符串
void draw_filled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y);

// 功能：按目标字框高度在指定中心绘制空心字符串
void draw_scaled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：按目标字框高度在指定中心绘制实心字符串
void draw_scaled_filled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：使用变长字体集合按目标字框高度绘制空心字符串
void draw_font_scaled_string(ttf_font_data* font, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：使用变长字体集合按目标字框高度绘制实心字符串
void draw_font_scaled_filled_string(ttf_font_data* font, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：通过统一字形来源按目标字框高度绘制空心字符串
void draw_source_scaled_string(font_glyph_source* source, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);

// 功能：通过统一字形来源按目标字框高度绘制实心字符串
void draw_source_scaled_filled_string(font_glyph_source* source, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height);


#endif
