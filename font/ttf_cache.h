#ifndef TTF_CACHE_H
#define TTF_CACHE_H


#include"ttf.h"


typedef struct ttf_cache ttf_cache;


// 功能：为已经打开的字体创建固定容量的按需字形缓存
int ttf_cache_init(ttf_font* font, int capacity, ttf_cache** pp_cache);

// 功能：释放缓存持有的字形，但不关闭外部字体解析上下文
void ttf_cache_free(ttf_cache* cache);

// 功能：从缓存获取字形，未命中时从字体解析模块加载并执行 FIFO 淘汰
// 返回指针在对应缓存条目被后续未命中请求淘汰前有效
glyph_point_data* ttf_cache_get(ttf_cache* cache, u32 unicode);

// 功能：判断指定 Unicode 是否已经存在于缓存中
int ttf_cache_contains(ttf_cache* cache, u32 unicode);

// 功能：将按需缓存包装为绘制层使用的统一字形来源
int ttf_cache_get_source(ttf_cache* cache, font_glyph_source* source);


#endif
