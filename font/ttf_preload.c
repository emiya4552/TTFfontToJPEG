#include"ttf_preload.h"

#include<stdlib.h>
#include<string.h>


// 功能：通过通用上下文在批量预加载集合中查找字形
static glyph_point_data* get_source_glyph(void* context, u32 unicode)
{
	ttf_preload*	preload;
	int		i;

	preload = (ttf_preload*)context;
	for(i=0; i<preload->glyph_count; i++){
		if(preload->glyph_array[i].unicode == unicode){
			return &(preload->glyph_array[i]);
		}
	}

	return NULL;
}


// 功能：统计多个 Unicode 范围包含的字符总数
static int get_range_glyph_count(const unicode_range* range_array, int range_count, int* glyph_count)
{
	u64	total_count;
	int	i;

	if(range_array == NULL || range_count <= 0 || glyph_count == NULL){
		return -1;
	}
	total_count = 0;

	// 累加全部范围，并拒绝逆序或超过有符号整数上限的配置
	for(i=0; i<range_count; i++){
		if(range_array[i].begin > range_array[i].end){
			return -1;
		}
		total_count += (u64)range_array[i].end-range_array[i].begin+1;
		if(total_count > 0x7fffffff){
			return -1;
		}
	}

	*glyph_count = (int)total_count;
	return 0;
}


// 功能：通过已经打开的字体按多个 Unicode 范围批量加载字形
int ttf_preload_ranges(ttf_font* ttf, const unicode_range* range_array, int range_count, ttf_preload* preload)
{
	u32	unicode;
	int	glyph_count;
	int	glyph_position;
	int	i;

	if(ttf == NULL || preload == NULL ||
	   get_range_glyph_count(range_array, range_count, &glyph_count) != 0){
		return -1;
	}
	preload->glyph_array = NULL;
	preload->glyph_count = 0;
	memset(&(preload->box), 0, sizeof(font_box));

	if(ttf_font_get_box(ttf, &(preload->box)) != 0){
		return -1;
	}
	preload->glyph_array = (glyph_point_data*)calloc((size_t)glyph_count, sizeof(glyph_point_data));
	if(preload->glyph_array == NULL){
		return -1;
	}
	preload->glyph_count = glyph_count;

	// 按范围顺序加载每个 Unicode，空轮廓仍保留水平前进宽度
	glyph_position = 0;
	for(i=0; i<range_count; i++){
		for(unicode=range_array[i].begin; ; unicode++){
			if(ttf_font_load_glyph(ttf, unicode, &(preload->glyph_array[glyph_position])) != 0){
				ttf_preload_free(preload);
				return -1;
			}
			glyph_position++;
			if(unicode == range_array[i].end){
				break;
			}
		}
	}

	return 0;
}


// 功能：释放批量预加载得到的全部字形数据
void ttf_preload_free(ttf_preload* preload)
{
	int	i;

	if(preload == NULL){
		return;
	}

	for(i=0; i<preload->glyph_count; i++){
		ttf_glyph_free(&(preload->glyph_array[i]));
	}
	free(preload->glyph_array);
	preload->glyph_array = NULL;
	preload->glyph_count = 0;
	memset(&(preload->box), 0, sizeof(font_box));
}


// 功能：打开字体并按照多个 Unicode 范围批量加载字形
int load_ttf_ranges(const char* file, const unicode_range* range_array, int range_count, ttf_font_data* font)
{
	ttf_font*	ttf;
	int	result;

	ttf = NULL;
	if(ttf_font_open(file, &ttf) != 0){
		return -1;
	}

	result = ttf_preload_ranges(ttf, range_array, range_count, font);
	ttf_font_close(ttf);
	return result;
}


// 功能：释放批量预加载得到的全部字形数据
void free_ttf_font_data(ttf_font_data* font)
{
	ttf_preload_free(font);
}


// 功能：加载字体中的中文 BMP 字形数据及统一排版框
void load_ttf_BMP(char* file, glyph_point_data** glyph_array, font_box* box)
{
	unicode_range	range;
	ttf_font_data	font;

	if(glyph_array == NULL || box == NULL){
		return;
	}
	range.begin = UNICODE_BMP_CHINA_BEGIN;
	range.end = UNICODE_BMP_CHINA_END;
	*glyph_array = NULL;
	memset(box, 0, sizeof(font_box));

	if(load_ttf_ranges(file, &range, 1, &font) != 0){
		return;
	}

	*glyph_array = font.glyph_array;
	*box = font.box;
}


// 功能：将批量预加载结果包装为绘制层使用的统一字形来源
int ttf_preload_get_source(ttf_preload* preload, font_glyph_source* source)
{
	if(preload == NULL || preload->glyph_array == NULL || preload->glyph_count <= 0 || source == NULL){
		return -1;
	}

	source->context = preload;
	source->box = preload->box;
	source->get_glyph = get_source_glyph;
	return 0;
}
