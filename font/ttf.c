#include"ttf.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>


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

#define BIT_PER_BYTE 		8

// glyf 简单字形点标志
#define GLYF_FLAG_ON_CURVE	0x01
#define GLYF_FLAG_X_SHORT	0x02
#define GLYF_FLAG_Y_SHORT	0x04
#define GLYF_FLAG_REPEAT	0x08
#define GLYF_FLAG_X_SAME	0x10
#define GLYF_FLAG_Y_SAME	0x20


#pragma pack(1)
// TTF 表目录项
typedef struct{
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
	u32	offset;
	u32	length;
	u8*	data;
}table;

// head 表数据
typedef struct{
	u32	version;
	u32	font_revision;
	u32	check_sum_adjustment;
	u32	magic_number;
	u16	flags;
	u16	unit_per_Em;
	u64	created;
	u64	modified;
	s16	xMin;
	s16	yMin;
	s16	xMax;
	s16	yMax;
	u16	mac_style;
	u16	lowest_RecPPRM;
	s16	font_direction_hint;
	s16	index_to_loca_format;
	s16	glyph_data_format;
}head_table;

// cmap 子表头
typedef struct{
	u16	platform_id;
	u16	encoding_id;
	u32	offset;
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
	s16 	number_of_contours;
	s16	x_min;
	s16	y_min;
	s16	x_max;
	s16	y_max;
}glyph_data_header;


typedef struct{
	u32	version;
	s16	ascent;
	s16	descent;
	s16	line_gap;
	u8	other_data[24];
	u16	number_of_h_metrics;
}hhea_header;
#pragma pack()

typedef struct{
	u16*	end_code;
	u16*	start_code;
	u16*	id_delta;
	u16*	id_range_offset;
	u16*	glyph_index_array;
	u16	glyph_index_array_length;
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
	sequential_map_group*	groups;
}cmap_format12_data;

// 当前实际使用的 cmap 字符映射数据
typedef struct{
	u16	format;
	u16	segment_count;
	union{
		cmap_format4_data	format4;
		cmap_format12_data	format12;
	}data;
}cmap_data;

struct ttf_font{
	FILE*		file;
	table*		table_array;
	cmap_data	cmap;
	u32*		loca_array;
	int		loca_length;
	u16		number_of_h_metrics;
	font_box	box;
};

// 功能：解析 hhea 表中排版所需的头部字段
static void read_hhea_header(u8* data, hhea_header* header)
{
	memcpy(header, data, sizeof(hhea_header));

	convert(header->version,	sizeof(header->version)*BIT_PER_BYTE);
	convert(header->ascent,		sizeof(header->ascent)*BIT_PER_BYTE);
	convert(header->descent,	sizeof(header->descent)*BIT_PER_BYTE);
	convert(header->line_gap,	sizeof(header->line_gap)*BIT_PER_BYTE);
	convert(header->number_of_h_metrics, sizeof(header->number_of_h_metrics)*BIT_PER_BYTE);
}


// 功能：从 hmtx 表获取指定字形的水平前进宽度
static u16 get_advance_width(u8* hmtx_data, u16 number_of_h_metrics, int glyph_index)
{
	u16	advance_width;
	int	metric_index;

	if(number_of_h_metrics == 0 || glyph_index < 0){
		return 0;
	}

	// 超出完整度量项范围的字形复用最后一个前进宽度
	metric_index = (glyph_index < number_of_h_metrics) ? glyph_index : number_of_h_metrics-1;
	memcpy(&advance_width, hmtx_data+metric_index*4, sizeof(u16));
	convert(advance_width, sizeof(advance_width)*BIT_PER_BYTE);

	return advance_width;
}




// 功能：释放关键 TTF 表数组及其中的数据
static void free_ttf_tables(table* table_array)
{
	int	i;

	if(table_array == NULL){
		return;
	}

	for(i=0; i<6; i++){
		free(table_array[i].data);
	}
	free(table_array);
}


// 功能：从 TTF 文件读取渲染所需的六个关键表
static int read_ttf_data(const char* file, FILE** pp_file, table** pp_table)
{
	FILE*		fp;
	file_header	ttf_header;
	table_entry*	table_entry_array;
	const char*	key_table_name[6];
	table*		table_array;
	int		current_entry;
	int		i;
	int		j;

	if(file == NULL || pp_file == NULL || pp_table == NULL){
		return -1;
	}
	*pp_file = NULL;
	*pp_table = NULL;

	// 打开字体文件并读取表目录头
	fp = fopen(file, "rb");
	if(fp == NULL){
		printf("[TTF][ERROR] open font failed: file=%s\n", file);
		return -1;
	}
	if(fread(&ttf_header, sizeof(file_header), 1, fp) != 1){
		fclose(fp);
		return -1;
	}

	convert(ttf_header.version, 		sizeof(ttf_header.version)*BIT_PER_BYTE);
	convert(ttf_header.table_number, 	sizeof(ttf_header.table_number)*BIT_PER_BYTE);
	convert(ttf_header.search_range, 	sizeof(ttf_header.search_range)*BIT_PER_BYTE);
	convert(ttf_header.entry_selector,	sizeof(ttf_header.entry_selector)*BIT_PER_BYTE);
	convert(ttf_header.range_shift,		sizeof(ttf_header.range_shift)*BIT_PER_BYTE);
	if(ttf_header.table_number == 0){
		fclose(fp);
		return -1;
	}

	// 读取并转换全部表目录项
	table_entry_array = (table_entry*)malloc(sizeof(table_entry)*ttf_header.table_number);
	if(table_entry_array == NULL ||
	   fread(table_entry_array, sizeof(table_entry), ttf_header.table_number, fp) != ttf_header.table_number){
		free(table_entry_array);
		fclose(fp);
		return -1;
	}
	for(i=0; i<ttf_header.table_number; i++){
		convert(table_entry_array[i].check_sum, sizeof(table_entry_array[i].check_sum)*BIT_PER_BYTE);
		convert(table_entry_array[i].offset, sizeof(table_entry_array[i].offset)*BIT_PER_BYTE);
		convert(table_entry_array[i].length, sizeof(table_entry_array[i].length)*BIT_PER_BYTE);
	}

	key_table_name[0] = "head";
	key_table_name[1] = "cmap";
	key_table_name[2] = "loca";
	key_table_name[3] = "glyf";
	key_table_name[4] = "hhea";
	key_table_name[5] = "hmtx";
	table_array = (table*)calloc(6, sizeof(table));
	if(table_array == NULL){
		free(table_entry_array);
		fclose(fp);
		return -1;
	}

	// 每个关键表独立查找，避免依赖字体表目录的排列顺序
	for(i=0; i<6; i++){
		current_entry = -1;
		for(j=0; j<ttf_header.table_number; j++){
			if(memcmp(key_table_name[i], table_entry_array[j].tag, 4) == 0){
				current_entry = j;
				break;
			}
		}
		if(current_entry < 0 || table_entry_array[current_entry].length == 0){
			free(table_entry_array);
			fclose(fp);
			free_ttf_tables(table_array);
			return -1;
		}

		memcpy(table_array[i].name, table_entry_array[current_entry].tag, 4);
		table_array[i].offset = table_entry_array[current_entry].offset;
		table_array[i].length = table_entry_array[current_entry].length;

		// glyf 表体积最大，只保存文件位置并在请求字形时按需读取
		if(i == 3){
			continue;
		}
		table_array[i].data = (u8*)malloc(table_array[i].length);
		if(table_array[i].data == NULL ||
		   fseek(fp, table_entry_array[current_entry].offset, SEEK_SET) != 0 ||
		   fread(table_array[i].data, sizeof(u8), table_array[i].length, fp) != table_array[i].length){
			free(table_entry_array);
			fclose(fp);
			free_ttf_tables(table_array);
			return -1;
		}
	}

	free(table_entry_array);
	*pp_file = fp;
	*pp_table = table_array;
	return 0;
}



// 功能：解析 head 表中的字体全局信息
static void read_head_table(u8* head_table_data, head_table* head_table_struct)
{
	// 复制结构并将所有多字节字段转换为本机字节序
	memcpy(head_table_struct, head_table_data, sizeof(head_table));

	convert(head_table_struct->version,			sizeof(head_table_struct->version)			*BIT_PER_BYTE);
	convert(head_table_struct->font_revision,		sizeof(head_table_struct->font_revision)		*BIT_PER_BYTE);
	convert(head_table_struct->check_sum_adjustment,	sizeof(head_table_struct->check_sum_adjustment)		*BIT_PER_BYTE);
	convert(head_table_struct->magic_number,		sizeof(head_table_struct->magic_number)			*BIT_PER_BYTE);
	convert(head_table_struct->flags,			sizeof(head_table_struct->flags)			*BIT_PER_BYTE);
	convert(head_table_struct->unit_per_Em,			sizeof(head_table_struct->unit_per_Em)			*BIT_PER_BYTE);
	convert(head_table_struct->created,			sizeof(head_table_struct->created)			*BIT_PER_BYTE);
	convert(head_table_struct->modified,			sizeof(head_table_struct->modified)			*BIT_PER_BYTE);
	convert(head_table_struct->xMin,			sizeof(head_table_struct->xMin)				*BIT_PER_BYTE);
	convert(head_table_struct->yMin,			sizeof(head_table_struct->yMin)				*BIT_PER_BYTE);
	convert(head_table_struct->xMax,			sizeof(head_table_struct->xMax)				*BIT_PER_BYTE);
	convert(head_table_struct->yMax,			sizeof(head_table_struct->yMax)				*BIT_PER_BYTE);
	convert(head_table_struct->mac_style,			sizeof(head_table_struct->mac_style)			*BIT_PER_BYTE);
	convert(head_table_struct->lowest_RecPPRM,		sizeof(head_table_struct->lowest_RecPPRM)		*BIT_PER_BYTE);
	convert(head_table_struct->font_direction_hint,		sizeof(head_table_struct->font_direction_hint)		*BIT_PER_BYTE);
	convert(head_table_struct->index_to_loca_format,	sizeof(head_table_struct->index_to_loca_format)		*BIT_PER_BYTE);
	convert(head_table_struct->glyph_data_format,		sizeof(head_table_struct->glyph_data_format)		*BIT_PER_BYTE);
}



// 功能：按指定格式从 cmap 表选择并复制字符映射子表
static u16 get_cmap_subtable_data(u8* cmap_table_data, u16 target_format, u8** pp_subtable_data)
{
	int			i;
	u16			number_of_subtable;
	cmap_subtable_header*	header;			// 使用后需要释放
	u16			subtable_length_format_4;
	u32			subtable_length_format_12;
	u32			subtable_length;
	u16			subtable_format;
	u16			preferred_encoding;
	u8*			pointer;
	int			current_subtable;

	// 初始化输出和子表选择状态
	i			= 0;
	pointer			= cmap_table_data;
	subtable_format		= 0;
	subtable_length		= 0;
	current_subtable	= -1;
	*pp_subtable_data	= NULL;
	preferred_encoding	= (target_format == 12) ? ENCODING_ID_3_UNICODE_FULL : ENCODING_ID_3_BMP;
	
	// 跳过版本号并读取子表数量
	pointer	+= sizeof(u16);	// 跳过版本号
	memcpy(&number_of_subtable, pointer, sizeof(u16));
	convert(number_of_subtable, sizeof(number_of_subtable) * BIT_PER_BYTE);

	// 为子表头申请空间并移动到首个目录项
	pointer += sizeof(u16); // 跳过子表数量
	header	= (cmap_subtable_header*)malloc(sizeof(cmap_subtable_header)*number_of_subtable);
	if(number_of_subtable > 0 && header == NULL){
		return 0;
	}

	// 读取并转换全部 cmap 子表目录项
	for(i=0; i<number_of_subtable; i++){
		memcpy(&(header[i]), pointer, sizeof(cmap_subtable_header));

		convert(header[i].platform_id, 	sizeof(header[i].platform_id)	* BIT_PER_BYTE);
		convert(header[i].encoding_id,	sizeof(header[i].encoding_id)	* BIT_PER_BYTE);
		convert(header[i].offset, 	sizeof(header[i].offset)	* BIT_PER_BYTE);
		pointer += sizeof(cmap_subtable_header);
	}


	// 查找目标格式，并优先使用 Windows 平台对应的 Unicode 编码子表
	for(i=0;i<number_of_subtable;i++){
		pointer = cmap_table_data + header[i].offset;
		memcpy(&subtable_format, pointer, sizeof(u16));
		convert(subtable_format, sizeof(subtable_format) * BIT_PER_BYTE);

		if(subtable_format != target_format){
			continue;
		}
		if(current_subtable == -1){
			current_subtable = i;
		}
		if(header[i].platform_id == PLATFORM_ID_WINDOWS &&
			header[i].encoding_id == preferred_encoding){
			current_subtable = i;
			break;
		}
	}

	// 当前字体不存在指定格式时返回 0
	if(current_subtable == -1){
		free(header);
		return 0;
	}

	// 定位选中的子表
	pointer	= cmap_table_data + header[current_subtable].offset;
	// 读取子表格式和长度
	memcpy(&subtable_format, pointer, sizeof(u16));
	convert(subtable_format, sizeof(subtable_format) * BIT_PER_BYTE);
	pointer	+= sizeof(u16);		// 跳过格式字段
	if(subtable_format == 4){
		memcpy(&subtable_length_format_4, pointer, sizeof(u16));
		convert(subtable_length_format_4, sizeof(u16) * BIT_PER_BYTE);
		subtable_length = subtable_length_format_4;
	}else if(subtable_format == 12){
		pointer += sizeof(u16);	// 跳过保留字段
		memcpy(&subtable_length_format_12, pointer, sizeof(u32));
		convert(subtable_length_format_12, sizeof(u32) * BIT_PER_BYTE);
		subtable_length = subtable_length_format_12;
	}else{
		free(header);
		return 0;
	}

	// 为完整子表申请独立缓冲区
	*pp_subtable_data = (u8*)malloc(sizeof(u8) * subtable_length);
	if(*pp_subtable_data == NULL){
		free(header);
		return 0;
	}

	// 复制完整子表数据
	pointer = cmap_table_data + header[current_subtable].offset;
	memcpy(*pp_subtable_data, pointer, subtable_length);

	// 释放临时子表目录
	free(header);

	return subtable_format;
}






// 功能：解析 cmap Format 4 子表
static u16 read_cmap_format4_subtable(u8* cmap_subtable_data, cmap_format4_data* p_data)
{
	u8*			pointer;
	cmap_format4_header	header;
	u16			segment_count;
	u16			glyph_index_array_length;
	int			i;

	
	pointer	= cmap_subtable_data;
	i	= 0;

	// 读取并转换 Format 4 头部字段
	memcpy(&header, pointer, sizeof(cmap_format4_header));
	pointer += sizeof(cmap_format4_header);

	convert(header.format,			sizeof(header.format) 		*BIT_PER_BYTE);
	convert(header.length, 			sizeof(header.length) 		*BIT_PER_BYTE);
	convert(header.language,		sizeof(header.language) 	*BIT_PER_BYTE);
	convert(header.segment_count_X2, 	sizeof(header.segment_count_X2)	*BIT_PER_BYTE);
	convert(header.search_range, 		sizeof(header.search_range) 	*BIT_PER_BYTE);
	convert(header.entry_selector, 		sizeof(header.entry_selector) 	*BIT_PER_BYTE);
	convert(header.range_shift, 		sizeof(header.range_shift) 	*BIT_PER_BYTE);
	
	segment_count	= header.segment_count_X2 / 2;

	// 计算尾部字形索引数组长度
	glyph_index_array_length	= header.length-sizeof(cmap_format4_header)-sizeof(u16)-segment_count*sizeof(u16)*4;
	glyph_index_array_length /=2;
	// 为各分段数组申请空间
	p_data->end_code	= (u16*)malloc(sizeof(u16)*segment_count);
	p_data->start_code	= (u16*)malloc(sizeof(u16)*segment_count);
	p_data->id_delta	= (u16*)malloc(sizeof(u16)*segment_count);
	p_data->id_range_offset	= (u16*)malloc(sizeof(u16)*segment_count);
	if(glyph_index_array_length > 0){
		p_data->glyph_index_array	= (u16*)malloc(sizeof(u16)*glyph_index_array_length);
	}else{
		p_data->glyph_index_array	= NULL;
	}
	if(p_data->end_code == NULL || p_data->start_code == NULL ||
	   p_data->id_delta == NULL || p_data->id_range_offset == NULL ||
	   (glyph_index_array_length > 0 && p_data->glyph_index_array == NULL)){
		free(p_data->end_code);
		free(p_data->start_code);
		free(p_data->id_delta);
		free(p_data->id_range_offset);
		free(p_data->glyph_index_array);
		memset(p_data, 0, sizeof(cmap_format4_data));
		return 0;
	}
	p_data->glyph_index_array_length = glyph_index_array_length;
	// 按 Format 4 布局依次读取各分段数组
	memcpy(p_data->end_code, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->end_code)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count+1);	// 跳过保留字段和 end_code 数组
	
	memcpy(p_data->start_code, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->start_code)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count);	// 跳过 start_code 数组
	
	memcpy(p_data->id_delta, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->id_delta)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count);	// 跳过 id_delta 数组

	memcpy(p_data->id_range_offset, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->id_range_offset)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count);	// 跳过 id_range_offset 数组

	if(glyph_index_array_length > 0){
		memcpy(p_data->glyph_index_array, pointer, sizeof(u16)*glyph_index_array_length);
		for(i=0;i<glyph_index_array_length;i++){
			convert((p_data->glyph_index_array)[i], sizeof(u16)*BIT_PER_BYTE);
		}
	}

	return segment_count;
}


// 功能：通过 cmap Format 4 获取字形索引
static int get_glyph_index_format4(cmap_format4_data format4, u32 unicode, u16 segment_count)
{
	int 	i;
	int 	glyph_index;
	int	offset;
	int	current_segment;

	glyph_index	= 0;
	current_segment	= -1;

	if(unicode > 0xffff){
		return 0;
	}
	
	// 查找 Unicode 所属的编码分段
	for(i=0;i<segment_count;i++){
		if(unicode < format4.start_code[i]){
			return 0;
		}
		if(unicode <= format4.end_code[i]){
			current_segment = i;
			break;
		}
	}

	if(current_segment == -1){
		return 0;
	}

	// 根据范围偏移或增量计算字形索引
	if(format4.id_range_offset[current_segment] == 0){
		glyph_index = unicode + format4.id_delta[current_segment];
		glyph_index %= 65536;
	}else{
		offset = format4.id_range_offset[current_segment]/2 + unicode -
			format4.start_code[current_segment] + current_segment - segment_count;
		if(offset < 0 || offset >= format4.glyph_index_array_length){
			return 0;
		}
		glyph_index = format4.glyph_index_array[offset];
		if(glyph_index != 0){
			glyph_index += format4.id_delta[current_segment];
			glyph_index %= 65536;
		}
	}

	return glyph_index;
}


// 功能：解析 cmap Format 12 子表
static int read_cmap_format12_subtable(u8* cmap_subtable_data, cmap_format12_data* p_data)
{
	u8*	pointer;
	u32	i;
	
	i	= 0;
	pointer	= cmap_subtable_data;
	
	// 读取并转换 Format 12 头部字段
	memcpy(&(p_data->format), pointer, sizeof(u16));
	pointer += sizeof(u16);
	memcpy(&(p_data->reserved), pointer, sizeof(u16));
	pointer += sizeof(u16);
	memcpy(&(p_data->length), pointer, sizeof(u32));
	pointer += sizeof(u32);
	memcpy(&(p_data->language), pointer, sizeof(u32));
	pointer += sizeof(u32);
	memcpy(&(p_data->groups_number), pointer, sizeof(u32));
	pointer += sizeof(u32);

	convert(p_data->format, 	sizeof(u16)*BIT_PER_BYTE);
	convert(p_data->reserved, 	sizeof(u16)*BIT_PER_BYTE);
	convert(p_data->length, 	sizeof(u32)*BIT_PER_BYTE);
	convert(p_data->language, 	sizeof(u32)*BIT_PER_BYTE);
	convert(p_data->groups_number, 	sizeof(u32)*BIT_PER_BYTE);


	// 为连续映射分组申请空间
	if(p_data->groups_number == 0){
		return -1;
	}
	p_data->groups = (sequential_map_group*)malloc(sizeof(sequential_map_group)*(p_data->groups_number));
	if(p_data->groups == NULL){
		return -1;
	}
	
	// 读取并转换全部连续映射分组
	memcpy(p_data->groups, pointer, sizeof(sequential_map_group)*(p_data->groups_number));
	for(i=0; i<p_data->groups_number; i++){
		convert((p_data->groups)[i].start_char_code, 	sizeof(u32)*BIT_PER_BYTE);
		convert((p_data->groups)[i].end_char_code, 	sizeof(u32)*BIT_PER_BYTE);
		convert((p_data->groups)[i].start_glyph_id, 	sizeof(u32)*BIT_PER_BYTE);
	}

	return 0;
}

// 功能：通过 cmap Format 12 获取字形索引
static int get_glyph_index_format12(cmap_format12_data format12, u32 unicode)
{
	u32	left;
	u32	right;
	u32	current_group;
	int 	glyph_index;

	if(format12.groups_number == 0){
		return 0;
	}

	// Format 12 分组按字符编码递增，使用二分查找定位目标分组
	left	= 0;
	right	= format12.groups_number;
	while(left < right){
		current_group = left+(right-left)/2;
		if(unicode < format12.groups[current_group].start_char_code){
			right = current_group;
		}else if(unicode > format12.groups[current_group].end_char_code){
			left = current_group+1;
		}else{
			glyph_index = format12.groups[current_group].start_glyph_id;
			glyph_index += unicode-format12.groups[current_group].start_char_code;
			return glyph_index;
		}
	}

	return 0;
}




// 功能：优先解析 Format 12，不存在时回退解析 Format 4
static u16 read_cmap(u8* cmap_table_data, cmap_data* p_cmap)
{
	u16		format;
	u8*		cmap_subtable_data;

	format	= 0;
	cmap_subtable_data = NULL;
	memset(p_cmap, 0, sizeof(cmap_data));

	// Format 12 能覆盖完整 Unicode，存在时不再解析 Format 4
	format = get_cmap_subtable_data(cmap_table_data, 12, &cmap_subtable_data);
	if(format == 12){
		if(read_cmap_format12_subtable(cmap_subtable_data, &(p_cmap->data.format12)) != 0){
			free(cmap_subtable_data);
			return 0;
		}
		free(cmap_subtable_data);
		p_cmap->format = 12;
		return 12;
	}
	
	// 字体没有 Format 12 时使用 BMP 范围的 Format 4
	format = get_cmap_subtable_data(cmap_table_data, 4, &cmap_subtable_data);
	if(format == 4){
		p_cmap->segment_count = read_cmap_format4_subtable(cmap_subtable_data, &(p_cmap->data.format4));
		free(cmap_subtable_data);
		if(p_cmap->segment_count == 0){
			return 0;
		}
		p_cmap->format = 4;
		return 4;
	}

	return 0;
}

// 功能：根据 cmap 格式获取 Unicode 对应的字形索引
static int get_glyph_index(cmap_data* p_cmap, u32 unicode)
{
	if(p_cmap->format == 12){
		return get_glyph_index_format12(p_cmap->data.format12, unicode);
	}
	if(p_cmap->format == 4){
		return get_glyph_index_format4(p_cmap->data.format4, unicode, p_cmap->segment_count);
	}

	return 0;
}

// 功能：释放当前实际解析的 cmap 字符映射数据
static void free_cmap(cmap_data* p_cmap)
{
	if(p_cmap->format == 12){
		free(p_cmap->data.format12.groups);
	}else if(p_cmap->format == 4){
		free(p_cmap->data.format4.end_code);
		free(p_cmap->data.format4.start_code);
		free(p_cmap->data.format4.id_delta);
		free(p_cmap->data.format4.id_range_offset);
		free(p_cmap->data.format4.glyph_index_array);
	}

	memset(p_cmap, 0, sizeof(cmap_data));
}
		





// 功能：解析 loca 表并生成字形偏移数组
static int read_loca_table(u8* data, u16 index_to_loca_format, u32** pp_result, u32 table_length)
{
	int 	loca_length;
	int	i;
	u8*	pointer;

	i		= 0;
	loca_length	= 0;
	pointer		= data;
	

	// 根据 head 表指定的格式计算偏移项数量
	if(index_to_loca_format == 0){
		loca_length	= table_length/2;
	}else{
		loca_length 	= table_length/4;
	}

	// 为字形偏移数组申请空间
	*pp_result	= (u32*)malloc(sizeof(u32)*loca_length);
	if(*pp_result == NULL){
		return 0;
	}
	// 按短格式或长格式读取并转换全部偏移
	if(index_to_loca_format == 0){
		for(i=0; i<loca_length; i++){
			u16	temp;
			memcpy(&temp, pointer+i*sizeof(u16), sizeof(u16));
			convert(temp, sizeof(u16)*BIT_PER_BYTE);
			(*pp_result)[i]	= (u32)temp*2;
		}
	}else{
		for(i=0; i<loca_length; i++){
			memcpy(&((*pp_result)[i]), pointer+i*sizeof(u32), sizeof(u32));
			convert((*pp_result)[i], sizeof(u32)*BIT_PER_BYTE);
		}
	}
	
	return loca_length;
}

// 功能：从字体文件的 glyf 表按偏移读取一个独立字形
static int read_glyph_data(FILE* file, table* glyf_table, u32 glyph_data_offset, u8** pp_glyph_data, int glyph_length)
{
	*pp_glyph_data = NULL;
	if(glyph_length <= 0){
		return 0;
	}
	if(file == NULL || glyf_table == NULL ||
	   glyph_data_offset > glyf_table->length ||
	   (u32)glyph_length > glyf_table->length-glyph_data_offset){
		return -1;
	}

	// 只为当前字形申请空间，并定位到 glyf 表内的对应片段
	*pp_glyph_data = (u8*)malloc(sizeof(u8)*glyph_length);
	if(*pp_glyph_data == NULL){
		return -1;
	}
	if(fseek(file, glyf_table->offset+glyph_data_offset, SEEK_SET) != 0 ||
	   fread(*pp_glyph_data, sizeof(u8), glyph_length, file) != (size_t)glyph_length){
		free(*pp_glyph_data);
		*pp_glyph_data = NULL;
		return -1;
	}

	return 0;
}
			
// 功能：补充连续控制点之间的隐式点并生成可绘制点数组
static int process_point(point* raw_point_data, point** pp_point_data, int raw_point_length)
{
	int 	point_length;
	int	i;
	int 	j;

	if(raw_point_data == NULL || pp_point_data == NULL || raw_point_length <= 0){
		return 0;
	}
	*pp_point_data = NULL;
	point_length	= 1;
	i		= 0;
	j		= 0;
	
	// 统计连续离线控制点之间需要插入的隐式点
	for(i=1; i<raw_point_length; i++,point_length++){
		if(raw_point_data[i].locate == 0 && raw_point_data[i-1].locate == 0){

			point_length++;
		}
	}

	// 为处理后的轮廓点申请空间
	*pp_point_data	= (point*)malloc(sizeof(point)*point_length);
	if(*pp_point_data == NULL){
		return 0;
	}

	// 保持轮廓顺序并插入隐式中点
	(*pp_point_data)[j].x		= raw_point_data[0].x;
	(*pp_point_data)[j].y		= raw_point_data[0].y;
	(*pp_point_data)[j].locate	= raw_point_data[0].locate;
	j++;
	for(i=1; i<raw_point_length; i++,j++){
		if(raw_point_data[i].locate == 0 && raw_point_data[i-1].locate == 0){
			(*pp_point_data)[j].x		= (raw_point_data[i].x + raw_point_data[i-1].x)/2;
			(*pp_point_data)[j].y		= (raw_point_data[i].y + raw_point_data[i-1].y)/2;
			(*pp_point_data)[j].locate	= 0;
			j++;
		}
		(*pp_point_data)[j].x		= raw_point_data[i].x;
		(*pp_point_data)[j].y		= raw_point_data[i].y;
		(*pp_point_data)[j].locate	= raw_point_data[i].locate;
	}	


	return point_length;
}

// 功能：将简单字形原始数据解析为轮廓点数组
int glyph_to_point(u8* glyph_data, point** pp_point_data, int glyph_length)
{
	int	contour_position;
	int	i;
	int	j;
	int	point_length;
	int	raw_point_length;
	int	result;
	s16	raw_delta;
	u16	instruction_length;
	u16*	end_pointer_of_contours;
	u8	current_flag;
	u8	repeat_count;
	u8*	flag;
	u8*	glyph_end;
	u8*	pointer;
	float	current_x;
	float	current_y;
	point*	raw_point_data;
	glyph_data_header	header;
	const char*	error_reason;

	if(pp_point_data == NULL){
		return 0;
	}
	*pp_point_data = NULL;
	if(glyph_data == NULL || glyph_length < (int)sizeof(glyph_data_header)){
		return 0;
	}

	end_pointer_of_contours	= NULL;
	flag			= NULL;
	raw_point_data		= NULL;
	point_length		= 0;
	result			= 0;
	error_reason		= NULL;
	pointer			= glyph_data;
	glyph_end		= glyph_data+glyph_length;

	// 读取并转换简单字形头部
	memcpy(&header, pointer, sizeof(header));
	pointer += sizeof(header);
	convert(header.number_of_contours, sizeof(s16)*BIT_PER_BYTE);
	convert(header.x_min, sizeof(s16)*BIT_PER_BYTE);
	convert(header.y_min, sizeof(s16)*BIT_PER_BYTE);
	convert(header.x_max, sizeof(s16)*BIT_PER_BYTE);
	convert(header.y_max, sizeof(s16)*BIT_PER_BYTE);

	// 负轮廓数表示复合字形，当前函数只负责简单字形
	if(header.number_of_contours <= 0){
		return 0;
	}

	// 读取每个轮廓的最后一个逻辑点索引
	if(glyph_end-pointer < (int)(sizeof(u16)*header.number_of_contours)){
		error_reason = "contour endpoints are incomplete";
		goto cleanup;
	}
	end_pointer_of_contours = (u16*)malloc(sizeof(u16)*header.number_of_contours);
	if(end_pointer_of_contours == NULL){
		error_reason = "cannot allocate contour endpoints";
		goto cleanup;
	}
	for(i=0; i<header.number_of_contours; i++){
		memcpy(&end_pointer_of_contours[i], pointer, sizeof(u16));
		pointer += sizeof(u16);
		convert(end_pointer_of_contours[i], sizeof(u16)*BIT_PER_BYTE);
		if(i>0 && end_pointer_of_contours[i] <= end_pointer_of_contours[i-1]){
			error_reason = "contour endpoints are not increasing";
			goto cleanup;
		}
	}
	raw_point_length = end_pointer_of_contours[header.number_of_contours-1]+1;

	// 跳过提示指令，指针随后指向压缩 flag 数据
	if(glyph_end-pointer < (int)sizeof(u16)){
		error_reason = "instruction length is missing";
		goto cleanup;
	}
	memcpy(&instruction_length, pointer, sizeof(u16));
	pointer += sizeof(u16);
	convert(instruction_length, sizeof(u16)*BIT_PER_BYTE);
	if(glyph_end-pointer < instruction_length){
		error_reason = "instructions are incomplete";
		goto cleanup;
	}
	pointer += instruction_length;

	// 将重复压缩的 flag 展开为与逻辑点数量相同的数组
	flag = (u8*)malloc(sizeof(u8)*raw_point_length);
	if(flag == NULL){
		error_reason = "cannot allocate flags";
		goto cleanup;
	}
	for(i=0; i<raw_point_length;){
		if(pointer >= glyph_end){
			error_reason = "flags are incomplete";
			goto cleanup;
		}
		current_flag = *pointer;
		pointer++;
		flag[i] = current_flag;
		i++;

		// repeatCount 表示当前 flag 还需额外重复的逻辑点数量
		if((current_flag&GLYF_FLAG_REPEAT) != 0){
			if(pointer >= glyph_end){
				error_reason = "flag repeat count is missing";
				goto cleanup;
			}
			repeat_count = *pointer;
			pointer++;
			if(repeat_count > raw_point_length-i){
				error_reason = "flag repeat count exceeds point count";
				goto cleanup;
			}
			for(j=0; j<repeat_count; j++){
				flag[i] = current_flag;
				i++;
			}
		}
	}

	// 为所有原始逻辑点申请空间，并按 flag 解码 x 坐标增量
	raw_point_data = (point*)malloc(sizeof(point)*raw_point_length);
	if(raw_point_data == NULL){
		error_reason = "cannot allocate glyph points";
		goto cleanup;
	}
	current_x = 0;
	for(i=0; i<raw_point_length; i++){
		if((flag[i]&GLYF_FLAG_X_SHORT) != 0){
			if(pointer >= glyph_end){
				error_reason = "x coordinates are incomplete";
				goto cleanup;
			}
			if((flag[i]&GLYF_FLAG_X_SAME) != 0){
				current_x += *pointer;
			}else{
				current_x -= *pointer;
			}
			pointer++;
		}else if((flag[i]&GLYF_FLAG_X_SAME) == 0){
			if(glyph_end-pointer < (int)sizeof(s16)){
				error_reason = "x coordinates are incomplete";
				goto cleanup;
			}
			raw_delta = (s16)(((u16)pointer[0]<<8)|pointer[1]);
			current_x += raw_delta;
			pointer += sizeof(s16);
		}
		raw_point_data[i].x = current_x;
	}

	// x 坐标流结束后紧接 y 坐标流，继续按逻辑点逐项解码
	current_y = 0;
	contour_position = 0;
	for(i=0; i<raw_point_length; i++){
		if((flag[i]&GLYF_FLAG_Y_SHORT) != 0){
			if(pointer >= glyph_end){
				error_reason = "y coordinates are incomplete";
				goto cleanup;
			}
			if((flag[i]&GLYF_FLAG_Y_SAME) != 0){
				current_y += *pointer;
			}else{
				current_y -= *pointer;
			}
			pointer++;
		}else if((flag[i]&GLYF_FLAG_Y_SAME) == 0){
			if(glyph_end-pointer < (int)sizeof(s16)){
				error_reason = "y coordinates are incomplete";
				goto cleanup;
			}
			raw_delta = (s16)(((u16)pointer[0]<<8)|pointer[1]);
			current_y += raw_delta;
			pointer += sizeof(s16);
		}
		raw_point_data[i].y = current_y;
		raw_point_data[i].locate = ((flag[i]&GLYF_FLAG_ON_CURVE) != 0)?1:0;

		// 轮廓终点使用展开后的逻辑点索引进行比较
		if(contour_position<header.number_of_contours &&
		   i == end_pointer_of_contours[contour_position]){
			raw_point_data[i].locate = NEXT;
			contour_position++;
		}
	}
	if(contour_position != header.number_of_contours){
		error_reason = "contour endpoint was not reached";
		goto cleanup;
	}

	point_length = process_point(raw_point_data, pp_point_data, raw_point_length);
	if(point_length <= 0){
		error_reason = "cannot process glyph points";
		goto cleanup;
	}
	result = point_length;

cleanup:
	if(error_reason != NULL){
		printf("[TTF][ERROR] malformed simple glyph: %s length=%d offset=%d\n",
			error_reason, glyph_length, (int)(pointer-glyph_data));
		free(*pp_point_data);
		*pp_point_data = NULL;
		result = 0;
	}
	free(end_pointer_of_contours);
	free(flag);
	free(raw_point_data);

	return result;
}


// 功能：从已经解析的字体表中加载一个 Unicode 对应的字形数据
static int load_glyph_entry(FILE* file, table* glyf_table, cmap_data* cmap, u32* loca_array, int loca_length, u8* hmtx_data, u16 number_of_h_metrics, u32 unicode, glyph_point_data* glyph)
{
	int	glyph_index;
	u32	glyph_offset;
	int	glyph_length;

	glyph_index = get_glyph_index(cmap, unicode);
	if(glyph_index < 0 || glyph_index+1 >= loca_length){
		glyph_index = 0;
	}

	glyph_offset = loca_array[glyph_index];
	glyph_length = loca_array[glyph_index+1]-loca_array[glyph_index];

	glyph->unicode = unicode;
	glyph->glyph_length = glyph_length;
	glyph->advance_width = get_advance_width(hmtx_data, number_of_h_metrics, glyph_index);
	if(read_glyph_data(file, glyf_table, glyph_offset, &(glyph->glyph_data), glyph_length) != 0){
		return -1;
	}

	return 0;
}


// 功能：打开字体文件并建立可复用的 TTF 解析上下文
int ttf_font_open(const char* file, ttf_font** pp_font)
{
	ttf_font*	font;
	head_table	head;
	hhea_header	hhea;
	u16		cmap_format;

	if(file == NULL || pp_font == NULL){
		printf("[TTF][ERROR] invalid open arguments\n");
		return -1;
	}
	printf("[TTF][INFO] opening: file=%s\n", file);
	*pp_font = NULL;
	font = (ttf_font*)calloc(1, sizeof(ttf_font));
	if(font == NULL){
		return -1;
	}

	// 一次读取并解析后续单字形加载共享的关键表
	if(read_ttf_data(file, &(font->file), &(font->table_array)) != 0){
		free(font);
		return -1;
	}
	read_head_table(font->table_array[0].data, &head);
	read_hhea_header(font->table_array[4].data, &hhea);
	cmap_format = read_cmap(font->table_array[1].data, &(font->cmap));
	font->loca_length = read_loca_table(font->table_array[2].data,
		head.index_to_loca_format, &(font->loca_array), font->table_array[2].length);
	if((cmap_format != 12 && cmap_format != 4) ||
	   font->loca_length <= 1 || font->loca_array == NULL){
		printf("[TTF][ERROR] required table parse failed: cmap=%u loca_entries=%d\n",
			cmap_format, font->loca_length);
		ttf_font_close(font);
		return -1;
	}

	font->number_of_h_metrics = hhea.number_of_h_metrics;
	font->box.x_min = 0;
	font->box.y_min = hhea.descent;
	font->box.x_max = head.unit_per_Em;
	font->box.y_max = hhea.ascent;

	// 解析完成后释放不再使用的原始表，仅保留 hmtx 和 glyf 文件位置
	free(font->table_array[0].data);
	free(font->table_array[1].data);
	free(font->table_array[2].data);
	free(font->table_array[4].data);
	font->table_array[0].data = NULL;
	font->table_array[1].data = NULL;
	font->table_array[2].data = NULL;
	font->table_array[4].data = NULL;
	*pp_font = font;
	printf("[TTF][INFO] opened: cmap=%u glyphs=%d units_per_em=%u box=(%d,%d,%d,%d) hmetrics=%u\n",
		cmap_format, font->loca_length-1, head.unit_per_Em,
		font->box.x_min, font->box.y_min, font->box.x_max, font->box.y_max,
		font->number_of_h_metrics);

	return 0;
}


// 功能：关闭 TTF 解析上下文并释放其持有的数据
void ttf_font_close(ttf_font* font)
{
	if(font == NULL){
		return;
	}

	free(font->loca_array);
	free_cmap(&(font->cmap));
	free_ttf_tables(font->table_array);
	if(font->file != NULL){
		fclose(font->file);
	}
	free(font);
}


// 功能：读取字体的统一排版框
int ttf_font_get_box(ttf_font* font, font_box* box)
{
	if(font == NULL || box == NULL){
		return -1;
	}

	*box = font->box;
	return 0;
}


// 功能：按 Unicode 从已经打开的字体中加载一个独立字形
int ttf_font_load_glyph(ttf_font* font, u32 unicode, glyph_point_data* glyph)
{
	if(font == NULL || glyph == NULL){
		return -1;
	}
	memset(glyph, 0, sizeof(glyph_point_data));

	return load_glyph_entry(font->file, &(font->table_array[3]), &(font->cmap),
		font->loca_array, font->loca_length, font->table_array[5].data,
		font->number_of_h_metrics, unicode, glyph);
}


// 功能：释放单个独立字形持有的原始数据
void ttf_glyph_free(glyph_point_data* glyph)
{
	if(glyph == NULL){
		return;
	}

	free(glyph->glyph_data);
	memset(glyph, 0, sizeof(glyph_point_data));
}
