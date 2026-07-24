#include"font_renderer.h"

#include"bmp.h"
#include"font_draw.h"
#include"ttf_cache.h"

#include<limits.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


struct font_renderer{
	font_render_options	options;
	ttf_font*		font;
	ttf_cache*		cache;
	ttf_preload		preload;
	font_glyph_source	source;
};


// 功能：配置英文、通用标点、中文标点、常用汉字和全角字符默认范围
static void set_default_preload_ranges(unicode_range range_array[TTF_PRELOAD_DEFAULT_RANGE_COUNT])
{
	range_array[0].begin	= TTF_PRELOAD_ASCII_BEGIN;
	range_array[0].end	= TTF_PRELOAD_ASCII_END;
	range_array[1].begin	= TTF_PRELOAD_GENERAL_PUNCTUATION_BEGIN;
	range_array[1].end	= TTF_PRELOAD_GENERAL_PUNCTUATION_END;
	range_array[2].begin	= TTF_PRELOAD_CJK_PUNCTUATION_BEGIN;
	range_array[2].end	= TTF_PRELOAD_CJK_PUNCTUATION_END;
	range_array[3].begin	= TTF_PRELOAD_COMMON_CJK_BEGIN;
	range_array[3].end	= TTF_PRELOAD_COMMON_CJK_END;
	range_array[4].begin	= TTF_PRELOAD_FULLWIDTH_BEGIN;
	range_array[4].end	= TTF_PRELOAD_FULLWIDTH_END;
}


// 功能：检查高层字体渲染配置是否合法
static int check_render_options(const font_render_options* options)
{
	if(options == NULL || options->font_file == NULL || options->font_file[0] == '\0' ||
	   options->output_file == NULL || options->output_file[0] == '\0' ||
	   options->canvas_width <= 0 || options->canvas_height <= 0 ||
	   options->target_height <= 0){
		return -1;
	}
	if(options->encoding != WORD_ENCODING_UTF8 &&
	   options->encoding != WORD_ENCODING_GBK &&
	   options->encoding != WORD_ENCODING_SYSTEM){
		return -1;
	}
	if(options->load_strategy == FONT_LOAD_CACHE){
		return (options->cache_capacity > 0)?0:-1;
	}
	if(options->load_strategy == FONT_LOAD_PRELOAD){
		if(options->preload_range_array == NULL && options->preload_range_count == 0){
			return 0;
		}
		return (options->preload_range_array != NULL &&
			options->preload_range_count > 0)?0:-1;
	}

	return -1;
}


// 功能：初始化字体渲染器的默认配置
void init_font_render_options(font_render_options* options)
{
	if(options == NULL){
		return;
	}

	memset(options, 0, sizeof(font_render_options));
	options->font_file	= FONT_RENDER_DEFAULT_FILE;
	options->output_file	= FONT_RENDER_DEFAULT_OUTPUT;
	options->canvas_width	= FONT_RENDER_DEFAULT_CANVAS_WIDTH;
	options->canvas_height	= FONT_RENDER_DEFAULT_CANVAS_HEIGHT;
	options->center_x	= FONT_RENDER_AUTO_POSITION;
	options->center_y	= FONT_RENDER_AUTO_POSITION;
	options->target_height	= FONT_RENDER_DEFAULT_TARGET_HEIGHT;
	options->color[0]	= 0xff;
	options->color[1]	= 0x00;
	options->color[2]	= 0x77;
	options->encoding	= WORD_ENCODING_SYSTEM;
	options->load_strategy	= FONT_LOAD_CACHE;
	options->cache_capacity	= FONT_RENDER_DEFAULT_CACHE_CAPACITY;
}


// 功能：根据配置打开可复用的高层字体渲染器
int font_renderer_open(const font_render_options* options, font_renderer** pp_renderer)
{
	font_renderer*	renderer;
	unicode_range	default_range_array[TTF_PRELOAD_DEFAULT_RANGE_COUNT];
	const unicode_range*	range_array;
	int		range_count;

	if(pp_renderer == NULL){
		return -1;
	}
	*pp_renderer = NULL;
	if(check_render_options(options) != 0){
		printf("[RENDERER][ERROR] invalid render options\n");
		return -1;
	}

	renderer = (font_renderer*)calloc(1, sizeof(font_renderer));
	if(renderer == NULL){
		printf("[RENDERER][ERROR] renderer allocation failed\n");
		return -1;
	}
	renderer->options = *options;

	if(ttf_font_open(options->font_file, &(renderer->font)) != 0){
		printf("[RENDERER][ERROR] font initialization failed: file=%s\n",
			options->font_file);
		font_renderer_close(renderer);
		return -1;
	}

	// 缓存策略需要在渲染器生命周期内保持字体文件处于打开状态
	if(options->load_strategy == FONT_LOAD_CACHE){
		if(ttf_cache_init(renderer->font, options->cache_capacity, &(renderer->cache)) != 0 ||
		   ttf_cache_get_source(renderer->cache, &(renderer->source)) != 0){
			printf("[RENDERER][ERROR] cache strategy initialization failed: file=%s\n",
				options->font_file);
			font_renderer_close(renderer);
			return -1;
		}
		printf("[RENDERER][INFO] opened: strategy=cache font=%s cache_capacity=%d\n",
			options->font_file, options->cache_capacity);
	}else{
		// 未指定范围时使用项目已有的五段默认预加载范围
		range_array = options->preload_range_array;
		range_count = options->preload_range_count;
		if(range_array == NULL){
			set_default_preload_ranges(default_range_array);
			range_array = default_range_array;
			range_count = TTF_PRELOAD_DEFAULT_RANGE_COUNT;
		}
		if(ttf_preload_ranges(renderer->font, range_array, range_count,
		   &(renderer->preload)) != 0 ||
		   ttf_preload_get_source(&(renderer->preload), &(renderer->source)) != 0){
			printf("[RENDERER][ERROR] preload strategy initialization failed: file=%s\n",
				options->font_file);
			font_renderer_close(renderer);
			return -1;
		}

		// 预加载数据独立持有字形，后续绘制不再需要保持字体文件打开
		ttf_font_close(renderer->font);
		renderer->font = NULL;
		printf("[RENDERER][INFO] opened: strategy=preload font=%s ranges=%d glyphs=%d\n",
			options->font_file, range_count, renderer->preload.glyph_count);
	}

	if(renderer->source.get_glyph == NULL){
		printf("[RENDERER][ERROR] glyph source initialization failed: file=%s\n",
			options->font_file);
		font_renderer_close(renderer);
		return -1;
	}
	*pp_renderer = renderer;
	return 0;
}


// 功能：将字符串绘制到新分配的 RGB 缓冲区，缓冲区由调用者使用 free 释放
int font_renderer_render_rgb(font_renderer* renderer, const char* text, u8** pp_color_data)
{
	bmp_data	bmp;
	u8*		color_data;
	size_t		pixel_count;
	size_t		color_length;
	int		center_x;
	int		center_y;

	if(pp_color_data == NULL){
		printf("[RENDERER][ERROR] invalid render request\n");
		return -1;
	}
	*pp_color_data = NULL;
	if(renderer==NULL || text==NULL || text[0]=='\0'){
		printf("[RENDERER][ERROR] invalid render request\n");
		return -1;
	}

	// 使用 size_t 检查画布像素数量和三通道缓冲区长度是否溢出
	if((size_t)renderer->options.canvas_width >
	   (size_t)-1/(size_t)renderer->options.canvas_height){
		printf("[RENDERER][ERROR] canvas size overflow\n");
		return -1;
	}
	pixel_count = (size_t)renderer->options.canvas_width*renderer->options.canvas_height;
	if(pixel_count > (size_t)-1/3 || pixel_count>(size_t)INT_MAX/3){
		printf("[RENDERER][ERROR] color buffer size overflow\n");
		return -1;
	}
	color_length = pixel_count*3;
	color_data = (u8*)calloc(color_length, sizeof(u8));
	if(color_data == NULL){
		printf("[RENDERER][ERROR] color buffer allocation failed: bytes=%lu\n",
			(unsigned long)color_length);
		return -1;
	}

	center_x = renderer->options.center_x;
	center_y = renderer->options.center_y;
	if(center_x == FONT_RENDER_AUTO_POSITION){
		center_x = renderer->options.canvas_width/2;
	}
	if(center_y == FONT_RENDER_AUTO_POSITION){
		center_y = renderer->options.canvas_height/2;
	}

	bmp.name	= (char*)renderer->options.output_file;
	bmp.color_data	= color_data;
	bmp.color	= renderer->options.color;
	bmp.width	= renderer->options.canvas_width;
	bmp.height	= renderer->options.canvas_height;
	printf("[RENDERER][INFO] render: text=\"%s\" center=(%d,%d) target_height=%d\n",
		text, center_x, center_y, renderer->options.target_height);

	// 绘制层只使用统一字形来源，不感知当前采用的加载策略
	draw_source_scaled_filled_string(&(renderer->source), text, renderer->options.encoding,
		bmp, center_x, center_y, renderer->options.target_height);
	*pp_color_data = color_data;
	return 0;
}


// 功能：使用已经打开的字体渲染器将字符串输出为实心 BMP
int font_renderer_render(font_renderer* renderer, const char* text)
{
	u8*	color_data;
	int	result;

	color_data = NULL;
	if(font_renderer_render_rgb(renderer, text, &color_data) != 0){
		return -1;
	}
	result = bmp_generate((char*)renderer->options.output_file, color_data,
		renderer->options.canvas_width, renderer->options.canvas_height);
	free(color_data);
	if(result != 0){
		printf("[RENDERER][ERROR] bitmap generation failed: output=%s\n",
			renderer->options.output_file);
		return -1;
	}

	return 0;
}


// 功能：关闭字体渲染器并释放缓存或预加载资源
void font_renderer_close(font_renderer* renderer)
{
	if(renderer == NULL){
		return;
	}

	ttf_cache_free(renderer->cache);
	ttf_preload_free(&(renderer->preload));
	ttf_font_close(renderer->font);
	free(renderer);
}
