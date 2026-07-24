#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H


#include"encoding.h"
#include"ttf_preload.h"


#define FONT_RENDER_DEFAULT_FILE		"simhei.ttf"
#define FONT_RENDER_DEFAULT_OUTPUT		"out.bmp"
#define FONT_RENDER_DEFAULT_CANVAS_WIDTH	2000
#define FONT_RENDER_DEFAULT_CANVAS_HEIGHT	2000
#define FONT_RENDER_DEFAULT_TARGET_HEIGHT	100
#define FONT_RENDER_DEFAULT_CACHE_CAPACITY	64
#define FONT_RENDER_AUTO_POSITION		-1


typedef enum{
	FONT_LOAD_CACHE,
	FONT_LOAD_PRELOAD
}font_load_strategy;


typedef struct{
	const char*		font_file;
	const char*		output_file;
	int			canvas_width;
	int			canvas_height;
	int			center_x;
	int			center_y;
	int			target_height;
	u8			color[3];
	word_encoding		encoding;
	font_load_strategy	load_strategy;
	int			cache_capacity;
	const unicode_range*	preload_range_array;
	int			preload_range_count;
}font_render_options;


typedef struct font_renderer font_renderer;


// 功能：初始化字体渲染器的默认配置
void init_font_render_options(font_render_options* options);

// 功能：根据配置打开可复用的高层字体渲染器
int font_renderer_open(const font_render_options* options, font_renderer** pp_renderer);

// 功能：将字符串绘制到新分配的 RGB 缓冲区，缓冲区由调用者使用 free 释放
int font_renderer_render_rgb(font_renderer* renderer, const char* text, u8** pp_color_data);

// 功能：使用已经打开的字体渲染器将字符串输出为实心 BMP
int font_renderer_render(font_renderer* renderer, const char* text);

// 功能：关闭字体渲染器并释放缓存或预加载资源
void font_renderer_close(font_renderer* renderer);


#endif
