#include"font_draw.h"

#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


#define COLOR_DATA(x, y, z) color_data[x*width*3 + y*3 + z]
#define MAX_CONTROL_POINT 	10
#define OUT_COUNT 		1000
#define ORIGINAL_FONT_HEIGHT	-1
#define DRAW_OUTLINE		0
#define DRAW_FILLED		1


typedef struct{
	float 	x;
	float	y;
}bezier_point;

typedef struct{
	float	x1;
	float	y1;
	float	x2;
	float	y2;
}glyph_edge;

typedef struct{
	float	x;
	int	direction;
}glyph_intersection;

typedef struct{
	glyph_point_data*	glyph_array;
	int			glyph_count;
}array_glyph_source;


// 功能：根据 Unicode 查找已经加载的字形位置
static int find_glyph_target(glyph_point_data* glyph_array, int glyph_count, unicode_t unicode)
{
	int	i;

	for(i=0; i<glyph_count; i++){
		if(glyph_array[i].unicode == unicode){
			return i;
		}
	}

	return -1;
}


// 功能：通过数组字形来源按 Unicode 返回已经加载的字形
static glyph_point_data* get_array_glyph(void* context, u32 unicode)
{
	array_glyph_source*	source;
	int			target;

	source = (array_glyph_source*)context;
	target = find_glyph_target(source->glyph_array, source->glyph_count, unicode);
	if(target < 0){
		return NULL;
	}

	return &(source->glyph_array[target]);
}


// 功能：计算贝塞尔曲线上指定参数位置的点
static bezier_point bezier_data_get(float t, point* points, int count)
{
	bezier_point 	temporary_points[MAX_CONTROL_POINT];
	int		i;
	int		j;

	// 逐层插值并将控制点收敛为曲线上的目标点
	for(i=1; i<count; i++){
		for(j=0; j<count-i; j++){
			if(i == 1){
				temporary_points[j].x = points[j].x*(1-t) + points[j+1].x*t;
				temporary_points[j].y = points[j].y*(1-t) + points[j+1].y*t;
				continue;
			}
			temporary_points[j].x = temporary_points[j].x*(1-t) + temporary_points[j+1].x*t;
			temporary_points[j].y = temporary_points[j].y*(1-t) + temporary_points[j+1].y*t;
		}
	}

	return temporary_points[0];
}

// 功能：对一段贝塞尔曲线进行采样
static void get_bezier(point* in, int count, bezier_point** pp_out)
{
	float 	step;
	float 	t;
	int	i;

	step	= (1.0) / (OUT_COUNT);
	t	= 0;
	*pp_out	= (bezier_point*)malloc(sizeof(bezier_point)*OUT_COUNT);

	// 按固定步长生成整段曲线的采样点
	for(i=0; i<OUT_COUNT; i++){
		bezier_point temporary_point	= bezier_data_get(t, in, count);
		t += step;
		(*pp_out)[i].x	= temporary_point.x;
		(*pp_out)[i].y	= temporary_point.y;
	}
}

// 功能：将一段贝塞尔曲线绘制到 BMP 像素缓冲区
void draw_bezier_to_bitmap(int width, int height, u8* color_data, u8 color[3], point* point_data, int count, int offset_x, int offset_y)
{
	int		i;
	int		x;
	int		y;
	int		row;
	int		column;
	bezier_point*	draw_point_data;

	i		= 0;

	// 复用贝塞尔采样函数生成曲线点
	get_bezier(point_data, count, &draw_point_data);
	
	// 将每个字体坐标采样点转换为 BMP 像素
	for(i=0; i<OUT_COUNT; i++){
		x	= (int)(draw_point_data[i]).x;
		y	= (int)(draw_point_data[i]).y;

		// 将左下原点字体坐标转换为左上原点 BMP 行列
		column	= x + offset_x;
		row	= height - 1 - (y + offset_y);

		// 跳过画布范围外的采样点
		if(column < 0 || column >= width || row < 0 || row >= height){
			continue;
		}

		COLOR_DATA(row, column, 0) = color[0];
		COLOR_DATA(row, column, 1) = color[1];
		COLOR_DATA(row, column, 2) = color[2];
	}

	free(draw_point_data);

}


// 功能：根据轮廓点绘制空心字形
void draw_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y)
{
	int 	start;
	int 	end;
	int	i;
	point*	draw_point;
	int	first;

	if(color_data == NULL || color == NULL || point_data == NULL || point_length <= 0){
		return;
	}

	// 从首个点开始逐个查找闭合轮廓
	start	= 0;

	// 为直线或二次贝塞尔控制点申请空间
	draw_point = (point*)malloc(sizeof(point)*3);
	if(draw_point == NULL){
		printf("[DRAW][ERROR] cannot allocate contour buffer\n");
		return;
	}

	while(start < point_length){
		end = -1;
		// 查找当前轮廓的结束标记
		for(i=start; i<point_length; i++){
			if(point_data[i].locate == NEXT){
				end 	= i;
				break;
			}
		}
		// 数据损坏时停止绘制，避免继续使用上一个轮廓终点
		if(end < 0){
			printf("[DRAW][ERROR] contour end not found: start=%d points=%d\n",
				start, point_length);
			break;
		}



		// 按在线点和离线控制点组合逐段绘制轮廓
		for(i=start, first=0; ;){
			if(first == 0){
				draw_point[0] = point_data[end];
				first	= 1;
				i--;
			}else{
				draw_point[0] = point_data[i];
			}
			draw_point[1]	= point_data[i+1];
			if(i+1>end){
				break;
			}

			if(draw_point[0].locate!=0 && draw_point[1].locate!=0){
				draw_bezier_to_bitmap(width, height, color_data, color, draw_point, 2, offset_x, offset_y);

				i++;
				continue;
			}
			if(i+2>end){
				break;
			}

			draw_point[2]	= point_data[i+2];
			draw_bezier_to_bitmap(width, height, color_data, color, draw_point, 3, offset_x, offset_y);

				i += 2;
		}
		start = end+1;
	}

	// 释放轮廓分段绘制使用的控制点缓冲区
	free(draw_point);
}


// 功能：将一段贝塞尔曲线转换为有方向的短边并追加到边数组
static int append_bezier_edges(point* point_data, int count, glyph_edge** edge_array, int* edge_length, int* edge_capacity)
{
	bezier_point*	bezier_array;
	glyph_edge*	temporary_array;
	float		x1;
	float		y1;
	float		x2;
	float		y2;
	int		new_capacity;
	int		i;

	// 当前容量不足时按一次曲线的最大边数扩展数组
	if(*edge_length+OUT_COUNT > *edge_capacity){
		new_capacity = *edge_capacity+OUT_COUNT;
		temporary_array = (glyph_edge*)realloc(*edge_array, sizeof(glyph_edge)*new_capacity);
		if(temporary_array == NULL){
			return -1;
		}
		*edge_array	= temporary_array;
		*edge_capacity	= new_capacity;
	}

	// 复用现有曲线采样并连接相邻采样点
	get_bezier(point_data, count, &bezier_array);
	for(i=0; i<OUT_COUNT; i++){
		x1 = bezier_array[i].x;
		y1 = bezier_array[i].y;
		if(i == OUT_COUNT-1){
			x2 = point_data[count-1].x;
			y2 = point_data[count-1].y;
		}else{
			x2 = bezier_array[i+1].x;
			y2 = bezier_array[i+1].y;
		}

		// 水平边不影响扫描线环绕值
		if(y1 == y2){
			continue;
		}
		(*edge_array)[*edge_length].x1 = x1;
		(*edge_array)[*edge_length].y1 = y1;
		(*edge_array)[*edge_length].x2 = x2;
		(*edge_array)[*edge_length].y2 = y2;
		(*edge_length)++;
	}
	free(bezier_array);

	return 0;
}


// 功能：遍历字形全部轮廓并生成扫描线填充所需的边数组
static int get_glyph_edges(point* point_data, int point_length, glyph_edge** edge_array, int* edge_length)
{
	point	draw_point[3];
	int	edge_capacity;
	int	start;
	int	end;
	int	first;
	int	i;

	*edge_array	= NULL;
	*edge_length	= 0;
	edge_capacity	= 0;
	start		= 0;

	// 以 NEXT 标记为边界逐个处理闭合轮廓
	while(start < point_length){
		end = -1;
		for(i=start; i<point_length; i++){
			if(point_data[i].locate == NEXT){
				end = i;
				break;
			}
		}
		if(end < 0){
			free(*edge_array);
			return -1;
		}

		// 按在线点与离线控制点组合生成直线或二次贝塞尔边
		for(i=start, first=0; ;){
			if(first == 0){
				draw_point[0] = point_data[end];
				first = 1;
				i--;
			}else{
				draw_point[0] = point_data[i];
			}
			draw_point[1] = point_data[i+1];
			if(i+1>end){
				break;
			}

			if(draw_point[0].locate!=0 && draw_point[1].locate!=0){
				if(append_bezier_edges(draw_point, 2, edge_array, edge_length, &edge_capacity) != 0){
					free(*edge_array);
					return -1;
				}
				i++;
				continue;
			}
			if(i+2>end){
				break;
			}

			draw_point[2] = point_data[i+2];
			if(append_bezier_edges(draw_point, 3, edge_array, edge_length, &edge_capacity) != 0){
				free(*edge_array);
				return -1;
			}
			i += 2;
		}
		start = end+1;
	}

	return 0;
}


// 功能：使用插入排序按横坐标排列扫描线交点
static void sort_intersection(glyph_intersection* intersection_array, int intersection_length)
{
	glyph_intersection	temporary;
	int			i;
	int			j;

	// 交点数量较少，直接使用简单稳定的插入排序
	for(i=1; i<intersection_length; i++){
		temporary = intersection_array[i];
		j = i-1;
		while(j>=0 && intersection_array[j].x>temporary.x){
			intersection_array[j+1] = intersection_array[j];
			j--;
		}
		intersection_array[j+1] = temporary;
	}
}


// 功能：根据有序交点和非零环绕规则填充一条扫描线
static void fill_scanline(int width, int height, u8* color_data, u8 color[3], glyph_intersection* intersection_array, int intersection_length, int canvas_y, int offset_x)
{
	float	start_x;
	float	current_x;
	int	direction;
	int	old_winding;
	int	winding;
	int	start_column;
	int	end_column;
	int	row;
	int	i;
	int	j;
	int	column;

	sort_intersection(intersection_array, intersection_length);
	start_x	= 0;
	winding	= 0;
	row	= height-1-canvas_y;

	// 合并同一位置的交点并累计轮廓环绕值
	for(i=0; i<intersection_length;){
		current_x	= intersection_array[i].x;
		direction	= 0;
		for(j=i; j<intersection_length && fabs(intersection_array[j].x-current_x)<0.0001; j++){
			direction += intersection_array[j].direction;
		}

		old_winding	= winding;
		winding		+= direction;
		// 环绕值在零与非零之间切换时确定实心区间
		if(old_winding == 0 && winding != 0){
			start_x = current_x;
		}else if(old_winding != 0 && winding == 0){
			start_column	= (int)ceil(start_x+offset_x-0.5);
			end_column	= (int)floor(current_x+offset_x-0.5);
			if(start_column < 0){
				start_column = 0;
			}
			if(end_column >= width){
				end_column = width-1;
			}
			// 将实心区间裁剪后写入当前 BMP 行
			for(column=start_column; column<=end_column; column++){
				COLOR_DATA(row, column, 0) = color[0];
				COLOR_DATA(row, column, 1) = color[1];
				COLOR_DATA(row, column, 2) = color[2];
			}
		}
		i = j;
	}
}


// 功能：将字形轮廓转换为边并逐行执行实心填充
static int fill_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y)
{
	glyph_edge*		edge_array;
	glyph_intersection*	intersection_array;
	float			min_y;
	float			max_y;
	float			scan_y;
	float			x;
	int			edge_length;
	int			intersection_length;
	int			start_y;
	int			end_y;
	int			canvas_y;
	int			i;

	if(get_glyph_edges(point_data, point_length, &edge_array, &edge_length) != 0){
		return -1;
	}
	if(edge_length == 0){
		free(edge_array);
		return -1;
	}

	intersection_array = (glyph_intersection*)malloc(sizeof(glyph_intersection)*edge_length);
	if(intersection_array == NULL){
		free(edge_array);
		return -1;
	}

	// 计算字形边数组的纵向扫描范围
	min_y = edge_array[0].y1;
	max_y = edge_array[0].y1;
	for(i=0; i<edge_length; i++){
		if(edge_array[i].y1 < min_y){
			min_y = edge_array[i].y1;
		}
		if(edge_array[i].y2 < min_y){
			min_y = edge_array[i].y2;
		}
		if(edge_array[i].y1 > max_y){
			max_y = edge_array[i].y1;
		}
		if(edge_array[i].y2 > max_y){
			max_y = edge_array[i].y2;
		}
	}

	// 将字体纵坐标范围转换并裁剪到画布范围
	start_y = (int)floor(min_y+offset_y);
	end_y = (int)ceil(max_y+offset_y);
	if(start_y < 0){
		start_y = 0;
	}
	if(end_y >= height){
		end_y = height-1;
	}

	// 逐行收集交点，并调用非零环绕扫描线填充
	for(canvas_y=start_y; canvas_y<=end_y; canvas_y++){
		scan_y = canvas_y+0.5-offset_y;
		intersection_length = 0;
		// 使用半开区间规则避免轮廓顶点被重复计数
		for(i=0; i<edge_length; i++){
			if((edge_array[i].y1<=scan_y && scan_y<edge_array[i].y2) ||
			   (edge_array[i].y2<=scan_y && scan_y<edge_array[i].y1)){
				x = edge_array[i].x1+
					(scan_y-edge_array[i].y1)*(edge_array[i].x2-edge_array[i].x1)/
					(edge_array[i].y2-edge_array[i].y1);
				intersection_array[intersection_length].x = x;
				intersection_array[intersection_length].direction =
					(edge_array[i].y2>edge_array[i].y1) ? 1 : -1;
				intersection_length++;
			}
		}
		if(intersection_length > 1){
			fill_scanline(width, height, color_data, color, intersection_array, intersection_length, canvas_y, offset_x);
		}
	}

	free(intersection_array);
	free(edge_array);

	return 0;
}


// 功能：根据轮廓点绘制边界清晰的实心字形
void draw_filled_word_from_point(int width, int height, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y)
{
	// 先复用旧函数绘制轮廓，再填充轮廓内部
	draw_word_from_point(width, height, color_data, color, point_data, point_length, offset_x, offset_y);
	if(fill_word_from_point(width, height, color_data, color, point_data, point_length, offset_x, offset_y) != 0){
		printf("[DRAW][ERROR] fill failed: points=%d offset=(%d,%d)\n",
			point_length, offset_x, offset_y);
	}
}


// 功能：按照缩放比例将字体坐标转换为像素坐标
static int scale_coordinate(int coordinate, float scale)
{
	float	result;

	result = coordinate*scale;
	if(result >= 0){
		return (int)(result+0.5f);
	}
	return (int)(result-0.5f);
}


// 功能：根据目标字框高度计算缩放比例和缩放后的排版框
static int get_scaled_box(font_box box, int target_height, font_box* scaled_box, float* scale)
{
	int	font_height;

	// 内部使用 -1 表示保持原始排版框，供旧绘制接口兼容调用
	if(target_height == ORIGINAL_FONT_HEIGHT){
		*scaled_box = box;
		*scale = 1.0f;
		return 0;
	}

	font_height = box.y_max-box.y_min;
	if(target_height <= 0 || font_height <= 0){
		printf("[DRAW][ERROR] invalid scale: target_height=%d font_height=%d\n",
			target_height, font_height);
		return -1;
	}

	*scale = (float)target_height/font_height;
	scaled_box->x_min = scale_coordinate(box.x_min, *scale);
	scaled_box->x_max = scale_coordinate(box.x_max, *scale);
	scaled_box->y_min = scale_coordinate(box.y_min, *scale);
	scaled_box->y_max = scaled_box->y_min+target_height;

	return 0;
}


// 功能：按照统一比例缩放字形的全部轮廓点
static void scale_point_data(point* point_data, int point_length, float scale)
{
	int	i;

	for(i=0; i<point_length; i++){
		point_data[i].x *= scale;
		point_data[i].y *= scale;
	}
}


// 功能：按目标字框高度解析并绘制已经取得的单个字形
static void draw_glyph_by_height(glyph_point_data* glyph, font_box box, bmp_data bmp, int center_x, int center_y, int target_height, int filled)
{
	point*		point_data;
	font_box	scaled_box;
	float		scale;
	int		point_length;
	int		offset_x;
	int		offset_y;

	if(glyph == NULL || glyph->glyph_length <= 0){
		return;
	}
	if(get_scaled_box(box, target_height, &scaled_box, &scale) != 0){
		return;
	}

	// 解析并缩放轮廓点，再计算缩放后排版框的原点偏移
	point_data = NULL;
	point_length = glyph_to_point(glyph->glyph_data, &point_data, glyph->glyph_length);
	if(point_length <= 0 || point_data == NULL){
		printf("[DRAW][ERROR] glyph decode failed: unicode=U+%04X glyph_length=%d\n",
			(unsigned int)glyph->unicode, glyph->glyph_length);
		free(point_data);
		return;
	}
	scale_point_data(point_data, point_length, scale);
	offset_x = center_x-(scaled_box.x_min+scaled_box.x_max)/2;
	offset_y = center_y-(scaled_box.y_min+scaled_box.y_max)/2;

	// 根据调用类型绘制空心或实心字形
	if(filled == 0){
		draw_word_from_point(bmp.width, bmp.height, bmp.color_data, bmp.color,
			point_data, point_length, offset_x, offset_y);
	}else{
		draw_filled_word_from_point(bmp.width, bmp.height, bmp.color_data, bmp.color,
			point_data, point_length, offset_x, offset_y);
	}

	free(point_data);
}


// 功能：通过统一字形来源按目标字框高度绘制单个字符
static void draw_word_by_height(font_glyph_source* source, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height, int filled)
{
	glyph_point_data*	glyph;
	unicode_t	unicode;

	if(source == NULL || source->get_glyph == NULL){
		printf("[DRAW][ERROR] invalid glyph source\n");
		return;
	}
	if(word_to_unicode(word, encoding, &unicode) != 0){
		printf("[DRAW][ERROR] character decode failed: encoding=%d\n", encoding);
		return;
	}

	glyph = source->get_glyph(source->context, unicode);
	if(glyph == NULL){
		printf("[DRAW][ERROR] glyph not found: unicode=U+%04X\n", (unsigned int)unicode);
		return;
	}

	draw_glyph_by_height(glyph, source->box, bmp, center_x, center_y, target_height, filled);
}


// 功能：将字形数组包装为统一字形来源并绘制单个字符
static void draw_array_word_by_height(glyph_point_data* glyph_array, int glyph_count, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height, int filled)
{
	array_glyph_source	array_source;
	font_glyph_source	source;

	array_source.glyph_array = glyph_array;
	array_source.glyph_count = glyph_count;
	source.context = &array_source;
	source.box = box;
	source.get_glyph = get_array_glyph;
	draw_word_by_height(&source, word, encoding, bmp, center_x, center_y, target_height, filled);
}


// 功能：以指定排版框中心绘制单个空心字符
void draw_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	draw_array_word_by_height(glyph_array, UNICODE_BMP_NUMBER, box, word, encoding, bmp, center_x, center_y, ORIGINAL_FONT_HEIGHT, DRAW_OUTLINE);
}


// 功能：以指定排版框中心绘制单个实心字符
void draw_filled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	draw_array_word_by_height(glyph_array, UNICODE_BMP_NUMBER, box, word, encoding, bmp, center_x, center_y, ORIGINAL_FONT_HEIGHT, DRAW_FILLED);
}


// 功能：按目标字框高度在指定中心绘制单个空心字符
void draw_scaled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	draw_array_word_by_height(glyph_array, UNICODE_BMP_NUMBER, box, word, encoding, bmp, center_x, center_y, target_height, DRAW_OUTLINE);
}


// 功能：按目标字框高度在指定中心绘制单个实心字符
void draw_scaled_filled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	draw_array_word_by_height(glyph_array, UNICODE_BMP_NUMBER, box, word, encoding, bmp, center_x, center_y, target_height, DRAW_FILLED);
}


// 功能：计算缩放后能够以指定位置为中心完整绘制的字符串前缀及其宽度
static int get_centered_string_range(font_glyph_source* source, const char* string, word_encoding encoding, int bmp_width, int center_x, float scale, int* draw_length, int* draw_width)
{
	const char*	current;
	char*		word;
	glyph_point_data*	glyph;
	unicode_t	unicode;
	int		string_length;
	int		word_length;
	int		advance_width;
	int		available_width;

	*draw_length	= 0;
	*draw_width	= 0;

	// 取中心点左右两侧的较短距离，保证最终排版框能够保持居中
	available_width = center_x;
	if(bmp_width-center_x < available_width){
		available_width = bmp_width-center_x;
	}
	available_width *= 2;
	if(available_width <= 0){
		return 0;
	}

	string_length	= strlen(string);
	current		= string;
	word		= (char*)malloc(string_length+1);
	if(word == NULL){
		printf("[DRAW][ERROR] string buffer allocation failed: bytes=%d\n", string_length);
		return -1;
	}

	// 逐字符累计前进宽度，遇到非法字符或下一个完整字框越界时停止
	while(current[0] != '\0'){
		word_length = get_character_byte_length(current, encoding);
		if(word_length < 0){
			printf("[DRAW][ERROR] invalid character boundary: byte=%d encoding=%d\n",
				(int)(current-string), encoding);
			break;
		}

		memcpy(word, current, word_length);
		word[word_length] = '\0';
		if(word_to_unicode(word, encoding, &unicode) != 0){
			printf("[DRAW][ERROR] character decode failed: byte=%d encoding=%d\n",
				(int)(current-string), encoding);
			break;
		}

		// 通过统一来源获取当前字符的水平前进宽度
		glyph = source->get_glyph(source->context, unicode);
		if(glyph == NULL){
			printf("[DRAW][ERROR] glyph not found: unicode=U+%04X\n", (unsigned int)unicode);
			break;
		}

		advance_width = scale_coordinate(glyph->advance_width, scale);
		if(advance_width <= 0){
			printf("[DRAW][ERROR] invalid advance width: unicode=U+%04X width=%d\n",
				(unsigned int)unicode, advance_width);
			break;
		}
		if(*draw_width+advance_width > available_width){
			printf("[DRAW][INFO] string prefix truncated: byte=%d required_width=%d available_width=%d\n",
				(int)(current-string), *draw_width+advance_width, available_width);
			break;
		}

		*draw_width	+= advance_width;
		*draw_length	+= word_length;
		current		+= word_length;
	}

	free(word);
	return 0;
}


// 功能：按目标字框高度从左向右绘制字符串，并在完整字框越界时截断
static void draw_string_by_height(font_glyph_source* source, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height, int filled)
{
	const char*	current;
	char*		word;
	glyph_point_data*	glyph;
	font_box	word_box;
	font_box	scaled_box;
	unicode_t	unicode;
	float		scale;
	int		string_length;
	int		draw_length;
	int		draw_width;
	int		word_length;
	int		advance_width;
	int		pen_x;
	int		word_center_x;
	int		offset_y;

	if(source == NULL || source->get_glyph == NULL){
		printf("[DRAW][ERROR] invalid glyph source\n");
		return;
	}
	if(string == NULL || string[0] == '\0'){
		printf("[DRAW][ERROR] empty string\n");
		return;
	}
	if(get_scaled_box(source->box, target_height, &scaled_box, &scale) != 0){
		return;
	}

	// 先按缩放后的前进宽度确定可绘制前缀，再将其整体中心对齐到输入坐标
	string_length	= strlen(string);
	if(get_centered_string_range(source, string, encoding, bmp.width, center_x, scale, &draw_length, &draw_width) != 0 ||
	   draw_length == 0){
		printf("[DRAW][ERROR] no drawable string prefix\n");
		return;
	}
	printf("[DRAW][INFO] layout: input_bytes=%d draw_bytes=%d width=%d center=(%d,%d) target_height=%d\n",
		string_length, draw_length, draw_width, center_x, center_y, target_height);

	// 为每次截取的单字符申请可复用缓冲区
	current		= string;
	word		= (char*)malloc(string_length+1);
	offset_y	= center_y-(scaled_box.y_min+scaled_box.y_max)/2;
	pen_x		= center_x-draw_width/2;

	if(word == NULL){
		printf("[DRAW][ERROR] string buffer allocation failed: bytes=%d\n", string_length);
		return;
	}

	// 逐字符解析、定位并调用旧的单字符绘制函数
	while(current < string+draw_length){
		word_length = get_character_byte_length(current, encoding);
		if(word_length < 0){
			printf("[DRAW][ERROR] invalid character boundary: byte=%d encoding=%d\n",
				(int)(current-string), encoding);
			break;
		}

		// 复制当前编码字符并转换为 Unicode
		memcpy(word, current, word_length);
		word[word_length] = '\0';
		if(word_to_unicode(word, encoding, &unicode) != 0){
			printf("[DRAW][ERROR] character decode failed: byte=%d encoding=%d\n",
				(int)(current-string), encoding);
			break;
		}

		// 通过统一来源获取当前字符和水平前进宽度
		glyph = source->get_glyph(source->context, unicode);
		if(glyph == NULL){
			printf("[DRAW][ERROR] glyph not found: unicode=U+%04X\n", (unsigned int)unicode);
			break;
		}

		advance_width = scale_coordinate(glyph->advance_width, scale);
		if(advance_width <= 0){
			printf("[DRAW][ERROR] invalid advance width: unicode=U+%04X width=%d\n",
				(unsigned int)unicode, advance_width);
			break;
		}

		// 当前完整排版框越界时截断剩余字符串
		if(pen_x < 0 || pen_x+advance_width > bmp.width ||
		   offset_y+scaled_box.y_min < 0 || offset_y+scaled_box.y_max > bmp.height){
			printf("[DRAW][INFO] drawing stopped at byte=%d: pen_x=%d advance=%d vertical=(%d,%d)\n",
				(int)(current-string), pen_x, advance_width,
				offset_y+scaled_box.y_min, offset_y+scaled_box.y_max);
			break;
		}

		// 空格等无轮廓字形只移动排版笔，不进入轮廓解析和绘制
		if(glyph->glyph_length > 0){
			word_center_x	= pen_x+advance_width/2;
			word_box	= source->box;
			word_box.x_min	= 0;
			word_box.x_max	= glyph->advance_width;
			draw_glyph_by_height(glyph, word_box, bmp, word_center_x, center_y,
				target_height, filled);
		}

		pen_x	+= advance_width;
		current	+= word_length;
	}
	printf("[DRAW][INFO] rendered: bytes=%d/%d width=%d mode=%s\n",
		(int)(current-string), string_length, pen_x-(center_x-draw_width/2),
		(filled == 0)?("outline"):("filled"));

	free(word);
}


// 功能：将字形数组包装为统一字形来源并绘制字符串
static void draw_array_string_by_height(glyph_point_data* glyph_array, int glyph_count, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height, int filled)
{
	array_glyph_source	array_source;
	font_glyph_source	source;

	array_source.glyph_array = glyph_array;
	array_source.glyph_count = glyph_count;
	source.context = &array_source;
	source.box = box;
	source.get_glyph = get_array_glyph;
	draw_string_by_height(&source, string, encoding, bmp, center_x, center_y, target_height, filled);
}


// 功能：以指定位置为整体中心从左向右绘制空心字符串
void draw_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	draw_array_string_by_height(glyph_array, UNICODE_BMP_NUMBER, box, string, encoding, bmp, center_x, center_y, ORIGINAL_FONT_HEIGHT, DRAW_OUTLINE);
}


// 功能：以指定位置为整体中心从左向右绘制实心字符串
void draw_filled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	draw_array_string_by_height(glyph_array, UNICODE_BMP_NUMBER, box, string, encoding, bmp, center_x, center_y, ORIGINAL_FONT_HEIGHT, DRAW_FILLED);
}


// 功能：按目标字框高度在指定中心绘制空心字符串
void draw_scaled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	draw_array_string_by_height(glyph_array, UNICODE_BMP_NUMBER, box, string, encoding, bmp, center_x, center_y, target_height, DRAW_OUTLINE);
}


// 功能：按目标字框高度在指定中心绘制实心字符串
void draw_scaled_filled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	draw_array_string_by_height(glyph_array, UNICODE_BMP_NUMBER, box, string, encoding, bmp, center_x, center_y, target_height, DRAW_FILLED);
}


// 功能：使用变长字体集合按目标字框高度绘制空心字符串
void draw_font_scaled_string(ttf_font_data* font, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	if(font == NULL || font->glyph_array == NULL || font->glyph_count <= 0){
		printf("[DRAW][ERROR] invalid preloaded font data\n");
		return;
	}

	draw_array_string_by_height(font->glyph_array, font->glyph_count, font->box, string, encoding,
		bmp, center_x, center_y, target_height, DRAW_OUTLINE);
}


// 功能：使用变长字体集合按目标字框高度绘制实心字符串
void draw_font_scaled_filled_string(ttf_font_data* font, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	if(font == NULL || font->glyph_array == NULL || font->glyph_count <= 0){
		printf("[DRAW][ERROR] invalid preloaded font data\n");
		return;
	}

	draw_array_string_by_height(font->glyph_array, font->glyph_count, font->box, string, encoding,
		bmp, center_x, center_y, target_height, DRAW_FILLED);
}


// 功能：通过统一字形来源按目标字框高度绘制空心字符串
void draw_source_scaled_string(font_glyph_source* source, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	draw_string_by_height(source, string, encoding, bmp, center_x, center_y, target_height, DRAW_OUTLINE);
}


// 功能：通过统一字形来源按目标字框高度绘制实心字符串
void draw_source_scaled_filled_string(font_glyph_source* source, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y, int target_height)
{
	draw_string_by_height(source, string, encoding, bmp, center_x, center_y, target_height, DRAW_FILLED);
}
