#include"ttf_cache.h"

#include<stdlib.h>


struct ttf_cache{
	ttf_font*		font;
	glyph_point_data*	glyph_array;
	int			capacity;
	int			glyph_count;
	int			next_replace;
};


// 功能：通过通用上下文调用缓存字形查询
static glyph_point_data* get_source_glyph(void* context, u32 unicode)
{
	return ttf_cache_get((ttf_cache*)context, unicode);
}


// 功能：在缓存数组中查找指定 Unicode 的位置
static int find_cache_glyph(ttf_cache* cache, u32 unicode)
{
	int	i;

	for(i=0; i<cache->glyph_count; i++){
		if(cache->glyph_array[i].unicode == unicode){
			return i;
		}
	}

	return -1;
}


// 功能：为已经打开的字体创建固定容量的按需字形缓存
int ttf_cache_init(ttf_font* font, int capacity, ttf_cache** pp_cache)
{
	ttf_cache*	cache;

	if(font == NULL || capacity <= 0 || pp_cache == NULL){
		return -1;
	}
	*pp_cache = NULL;
	cache = (ttf_cache*)calloc(1, sizeof(ttf_cache));
	if(cache == NULL){
		return -1;
	}

	cache->glyph_array = (glyph_point_data*)calloc((size_t)capacity, sizeof(glyph_point_data));
	if(cache->glyph_array == NULL){
		free(cache);
		return -1;
	}
	cache->font = font;
	cache->capacity = capacity;
	*pp_cache = cache;

	return 0;
}


// 功能：释放缓存持有的字形，但不关闭外部字体解析上下文
void ttf_cache_free(ttf_cache* cache)
{
	int	i;

	if(cache == NULL){
		return;
	}

	for(i=0; i<cache->glyph_count; i++){
		ttf_glyph_free(&(cache->glyph_array[i]));
	}
	free(cache->glyph_array);
	free(cache);
}


// 功能：从缓存获取字形，未命中时从字体解析模块加载并执行 FIFO 淘汰
glyph_point_data* ttf_cache_get(ttf_cache* cache, u32 unicode)
{
	glyph_point_data	glyph;
	int			target;

	if(cache == NULL){
		return NULL;
	}
	target = find_cache_glyph(cache, unicode);
	if(target >= 0){
		return &(cache->glyph_array[target]);
	}

	// 先完整加载新字形，失败时不破坏缓存中的已有条目
	if(ttf_font_load_glyph(cache->font, unicode, &glyph) != 0){
		return NULL;
	}
	if(cache->glyph_count < cache->capacity){
		target = cache->glyph_count;
		cache->glyph_count++;
	}else{
		target = cache->next_replace;
		ttf_glyph_free(&(cache->glyph_array[target]));
		cache->next_replace++;
		if(cache->next_replace >= cache->capacity){
			cache->next_replace = 0;
		}
	}
	cache->glyph_array[target] = glyph;

	return &(cache->glyph_array[target]);
}


// 功能：判断指定 Unicode 是否已经存在于缓存中
int ttf_cache_contains(ttf_cache* cache, u32 unicode)
{
	if(cache == NULL){
		return 0;
	}

	return find_cache_glyph(cache, unicode) >= 0;
}


// 功能：将按需缓存包装为绘制层使用的统一字形来源
int ttf_cache_get_source(ttf_cache* cache, font_glyph_source* source)
{
	if(cache == NULL || source == NULL ||
	   ttf_font_get_box(cache->font, &(source->box)) != 0){
		return -1;
	}

	source->context = cache;
	source->get_glyph = get_source_glyph;
	return 0;
}
