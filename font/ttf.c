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




// 功能：从 TTF 文件读取渲染所需的关键表
static void read_ttf_data(char* file, table** pp_table)
{
	FILE*		fp;
	file_header	ttf_header;
	table_entry*	p_table_entry_array;	// 使用后需要释放
	int		i;
	int		j;
	int 		k;
	int		current_table;
	const char*	key_table_name[6];

	// 打开字体文件
	fp = fopen(file,"rb");
	if(fp == NULL){
		printf("open file error\n");
	}

	// 初始化遍历状态
	i =	0;
	j =	0;
	k = 0;

	// 设置需要读取的关键表名称
	key_table_name[0]	= "head";
	key_table_name[1]	= "cmap";
	key_table_name[2]	= "loca";
	key_table_name[3]	= "glyf";
	key_table_name[4]	= "hhea";
	key_table_name[5]	= "hmtx";

	// 为关键表数组申请空间
	*pp_table	= (table*)malloc(sizeof(table)*6);

	// 读取并转换 TTF 文件头
	fread(&ttf_header, sizeof(file_header), 1, fp);

	convert(ttf_header.version, 		sizeof(ttf_header.version)		*BIT_PER_BYTE); 	
	convert(ttf_header.table_number, 	sizeof(ttf_header.table_number)		*BIT_PER_BYTE);
	convert(ttf_header.search_range, 	sizeof(ttf_header.search_range)		*BIT_PER_BYTE);
	convert(ttf_header.entry_selector,	sizeof(ttf_header.entry_selector)	*BIT_PER_BYTE);
	convert(ttf_header.range_shift,		sizeof(ttf_header.range_shift)		*BIT_PER_BYTE);

	// 读取并转换全部表目录项
	p_table_entry_array	= (table_entry*)malloc(sizeof(table_entry)*ttf_header.table_number);
	fread(p_table_entry_array, sizeof(table_entry), ttf_header.table_number, fp);
	for(i=0;i<ttf_header.table_number;i++){
		convert(((p_table_entry_array)[i]).check_sum,	sizeof((p_table_entry_array[i]).check_sum)	*BIT_PER_BYTE);
		convert(((p_table_entry_array)[i]).offset,	sizeof((p_table_entry_array[i]).offset)		*BIT_PER_BYTE);
		convert(((p_table_entry_array)[i]).length,	sizeof((p_table_entry_array[i]).length)		*BIT_PER_BYTE);
	}

	// 输出表目录调试信息
	for(i=0;i<ttf_header.table_number;i++){
		printf("%c %c %c %c\n", p_table_entry_array[i].tag[0],
					p_table_entry_array[i].tag[1],
					p_table_entry_array[i].tag[2],
					p_table_entry_array[i].tag[3]);
		printf("%0x\n",((p_table_entry_array)[i]).check_sum);
		printf("%0x\n",((p_table_entry_array)[i]).offset);
		printf("%0x\n",((p_table_entry_array)[i]).length);	
	}

	// 按表目录顺序查找并读取六个关键表
	for(i=0; i<6; i++){
		// 定位下一个需要加载的表
		// 每次比较会比较所有关键表名，所以此处不需要j=0初始化
		current_table = 0;
		for(; j<ttf_header.table_number; j++){
			if(!memcmp(key_table_name[0], (p_table_entry_array[j]).tag, 4)){
				current_table = 0;
				break;
			}else if(!memcmp(key_table_name[1], (p_table_entry_array[j]).tag, 4)){
				current_table = 1;
				break;
			}else if(!memcmp(key_table_name[2], (p_table_entry_array[j]).tag, 4)){
				current_table = 2;
				break;
			}else if(!memcmp(key_table_name[3], (p_table_entry_array[j]).tag, 4)){
				current_table = 3;
				break;
			}else if(!memcmp(key_table_name[4], (p_table_entry_array[j]).tag, 4)){
				current_table = 4;
				break;
			}else if(!memcmp(key_table_name[5], (p_table_entry_array[j]).tag, 4)){
				current_table = 5;
				break;
			}
		}
		// 保存表名称和长度
		for(k=0; k<4; k++){
			(*pp_table)[current_table].name[k] = p_table_entry_array[j].tag[k];
		}
		(*pp_table)[current_table].length = p_table_entry_array[j].length;
		// 定位表数据并复制到独立缓冲区
		fseek(fp, p_table_entry_array[j].offset, SEEK_SET);
		(*pp_table)[current_table].data = (u8*)malloc(p_table_entry_array[j].length);
		fread((*pp_table)[current_table].data, sizeof(u8), p_table_entry_array[j].length, fp);
		j++;
	}

	// 释放临时表目录并关闭文件
	free(p_table_entry_array);

	fclose(fp);
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
	printf("glyph_index_array_length = %d\n", glyph_index_array_length);

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
static void read_cmap_format12_subtable(u8* cmap_subtable_data, cmap_format12_data* p_data)
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
	p_data->groups = (sequential_map_group*)malloc(sizeof(sequential_map_group)*(p_data->groups_number));
	
	// 读取并转换全部连续映射分组
	memcpy(p_data->groups, pointer, sizeof(sequential_map_group)*(p_data->groups_number));
	for(i=0; i<p_data->groups_number; i++){
		convert((p_data->groups)[i].start_char_code, 	sizeof(u32)*BIT_PER_BYTE);
		convert((p_data->groups)[i].end_char_code, 	sizeof(u32)*BIT_PER_BYTE);
		convert((p_data->groups)[i].start_glyph_id, 	sizeof(u32)*BIT_PER_BYTE);
	}
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
		read_cmap_format12_subtable(cmap_subtable_data, &(p_cmap->data.format12));
		free(cmap_subtable_data);
		p_cmap->format = 12;
		return 12;
	}
	
	// 字体没有 Format 12 时使用 BMP 范围的 Format 4
	format = get_cmap_subtable_data(cmap_table_data, 4, &cmap_subtable_data);
	if(format == 4){
		p_cmap->segment_count = read_cmap_format4_subtable(cmap_subtable_data, &(p_cmap->data.format4));
		free(cmap_subtable_data);
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
	// 按短格式或长格式读取并转换全部偏移
	if(index_to_loca_format == 0){
		for(i=0; i<loca_length; i++){
			u16	temp;
			memcpy(&temp, pointer+i*sizeof(u16), sizeof(u16));
			convert(temp, sizeof(u16)*BIT_PER_BYTE);
			(*pp_result)[i]	= temp;
		}
	}else{
		for(i=0; i<loca_length; i++){
			memcpy(&((*pp_result)[i]), pointer+i*sizeof(u32), sizeof(u32));
			convert((*pp_result)[i], sizeof(u32)*BIT_PER_BYTE);
		}
	}
	
	return loca_length;
}

// 功能：从 glyf 表复制指定字形的原始数据
static void get_glyph_data(u8* glyf_data, u32 glyph_data_offset, u8** pp_glyph_data, int glyph_length)
{
	u8*	pointer;
	
	*pp_glyph_data = NULL;
	if(glyph_length <= 0){
		return;
	}

	// 为单个字形数据申请独立缓冲区
	pointer		= glyf_data + glyph_data_offset;
	*pp_glyph_data	= (u8*)malloc(sizeof(u8)*glyph_length);
	if(*pp_glyph_data == NULL){
		return;
	}
	
	// 从 glyf 表指定偏移复制字形数据
	memcpy(*pp_glyph_data, pointer, glyph_length*sizeof(u8));
}
			
// 功能：补充连续控制点之间的隐式点并生成可绘制点数组
static int process_point(point* raw_point_data, point** pp_point_data, int raw_point_length)
{
	int 	point_length;
	int	i;
	int 	j;

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

	int	bit_0;
	int	bit_1;
	int	bit_2;
	int	bit_3;
	int	bit_4;
	int	bit_5;

	int	i;
	int	j;
	int	k;
	int	m;
	int	n;
	u8*	pointer;
	u8*	pointer_x;
	u8*	pointer_y;
	int	repeat_time;

	glyph_data_header	header;
	s16*			end_pointer_of_contours;
	u16			instruction_length;
	u8*			instructions;
	int			flag_length;
	int			point_length;
	u8*			flag;
	int			x_length;
	int			y_length;
	u8*			x_data;
	u8*			y_data;

	point*			raw_point_data;
	int			raw_point_length;

	i		= 0;
	pointer 	= glyph_data;
	point_length	= 0;		

	repeat_time	= 0;

	// 读取并转换简单字形头部
	memcpy(&header, pointer, sizeof(header));
	// 转换字形头部中的大端字段
	convert(header.number_of_contours, 	sizeof(s16) * BIT_PER_BYTE);
	convert(header.x_min, 			sizeof(s16) * BIT_PER_BYTE);
	convert(header.y_min, 			sizeof(s16) * BIT_PER_BYTE);
	convert(header.x_max, 			sizeof(s16) * BIT_PER_BYTE);
	convert(header.y_max, 			sizeof(s16) * BIT_PER_BYTE);
	// 移动到轮廓结束点数组
	pointer += sizeof(header);

	// 为各轮廓结束点索引申请空间
	end_pointer_of_contours	= (s16*)malloc(sizeof(s16)*header.number_of_contours);
	// 读取并转换各轮廓结束点索引
	memcpy(end_pointer_of_contours, pointer, sizeof(s16)*header.number_of_contours);
	pointer += sizeof(s16)*header.number_of_contours;
	for(i=0; i<header.number_of_contours; i++){
		convert(end_pointer_of_contours[i], sizeof(s16) * BIT_PER_BYTE);
	}


	// 读取并转换提示指令长度
	memcpy(&instruction_length, pointer, sizeof(u16));
	pointer += sizeof(u16);
	convert(instruction_length, sizeof(u16) * BIT_PER_BYTE);
	// 保存提示指令并移动到点标志数据
	if(instruction_length>0){
		instructions = (u8*)malloc(sizeof(u8)*instruction_length);
		memcpy(instructions, pointer, sizeof(u8)*instruction_length);
		pointer += sizeof(u8)*instruction_length;
	}

	// 初始化标志流和坐标流位置
	flag_length	= end_pointer_of_contours[header.number_of_contours-1] + 1;
	pointer_x 	= pointer + flag_length*sizeof(u8);

	// 为压缩标志数据申请空间
	flag		= (u8*)malloc(sizeof(u8)*flag_length);
	// 复制字形点标志数据
	memcpy(flag, pointer, sizeof(u8)*flag_length);


	// 展开重复标志并计算原始点数量与 y 坐标位置
	pointer	= pointer_x;
	for(i=0; i<flag_length;){
		// 根据重复标志累计实际点数量
		if(repeat_time == 0){
			bit_3	= flag[i]&0x08;
			bit_1	= flag[i]&0x02;
			bit_4	= flag[i]&0x10;
			if(bit_3 == 0){
				point_length++;
			}else{
				point_length += (flag[i+1] + 1);
				repeat_time = flag[i+1];
			}
		}
		// 根据 x 坐标标志累计 x 数据长度
		if(bit_1 != 0){
			pointer += sizeof(u8);

		}else if(bit_1==0 && bit_4 == 0){
			pointer += sizeof(u8)*2;
		}
		// 处理重复标志剩余次数并移动标志索引
		if(repeat_time > 0){
			repeat_time--;
			if(repeat_time == 0){
				i += 2;
			}
		}else{
			i++;
		}
	}
	pointer_y = pointer;


	// 根据总数据长度计算 x、y 坐标数据长度
	x_length	= pointer_y - pointer_x;
	y_length	= &(glyph_data[glyph_length-1]) - pointer_y + 1;

	// 为 x、y 坐标压缩数据申请空间
	x_data	= (u8*)malloc(sizeof(u8)*x_length);
	y_data 	= (u8*)malloc(sizeof(u8)*y_length);
	// 分别复制 x、y 坐标压缩数据
	memcpy(x_data, pointer_x, x_length);
	memcpy(y_data, pointer_y, y_length);


	// 为展开后的原始轮廓点申请空间
	raw_point_length	= point_length;
	raw_point_data		= (point*)malloc(sizeof(point)*raw_point_length);

	// 逐点展开标志、坐标增量和轮廓结束标记
	j	= 0;	// x 坐标索引
	k	= 0;	// y 坐标索引
	m	= 0;	// 轮廓结束点索引
	n	= 0;	// 当前点索引
	for(i=0; i<flag_length; n++){
		// 拆分当前点标志位
		if(repeat_time == 0){
			bit_0	= flag[i]&0x01;
			bit_1	= flag[i]&0x02;
			bit_2	= flag[i]&0x04;
			bit_3	= flag[i]&0x08;
			bit_4	= flag[i]&0x10;
			bit_5	= flag[i]&0x20;
			if(bit_3 == 0){
				point_length++;
			}else{
				printf("repeat\n");
				point_length += (flag[i+1] + 1);
				repeat_time = flag[i+1];
			}
		}

		// 记录在线点、离线控制点和轮廓结束点
		if(bit_0 == 0){
			(raw_point_data[n]).locate = 0;
		}else{
			(raw_point_data[n]).locate = 1;
		}
		if(i == end_pointer_of_contours[m]){
 			(raw_point_data[n]).locate = NEXT;
			m++;
		}

		// 根据标志位解码并累计 x 坐标增量
		if(bit_1 != 0){
			float delta_x		= (float)((bit_4==0)?(-x_data[j]):(x_data[j]));
			(raw_point_data[n]).x 	= (n==0)?(delta_x):((raw_point_data[n-1]).x+delta_x);
			j++;
		}else if(bit_4 != 0){
			(raw_point_data[n]).x 	= (raw_point_data[n-1]).x;
		}else{
			float delta_x		= (float)((x_data[j++]<<8)|(x_data[j++]));
			(raw_point_data[n]).x 	= (n==0)?(delta_x):((raw_point_data[n-1]).x+delta_x);
		}

		// 根据标志位解码并累计 y 坐标增量
		if(bit_2 != 0){
			float delta_y		= (float)((bit_5==0)?(-y_data[k]):(y_data[k]));
			(raw_point_data[n]).y 	= (n==0)?(delta_y):((raw_point_data[n-1]).y+delta_y);
			k++;
		}else if(bit_5 != 0){
			(raw_point_data[n]).y 	= (raw_point_data[n-1]).y;
		}else{
			float delta_y		= (float)((y_data[k++]<<8)|(y_data[k++]));
			(raw_point_data[n]).y 	= (n==0)?(delta_y):((raw_point_data[n-1]).y+delta_y);
		}

		// 处理重复标志剩余次数并移动标志索引
		if(repeat_time > 0){
			repeat_time--;
			if(repeat_time == 0){
				i += 2;
			}
		}else{
			i++;
		}
	}



	point_length = process_point(raw_point_data, pp_point_data, raw_point_length);
	// 释放解析过程中的临时数组
	free(end_pointer_of_contours);
	free(flag);
	free(x_data);
	free(y_data);
	free(raw_point_data);
	
	return point_length;
}


// 功能：从已经解析的字体表中加载一个 Unicode 对应的字形数据
static int load_glyph_entry(cmap_data* cmap, u32* loca_array, int loca_length, u8* glyf_data, u8* hmtx_data, u16 number_of_h_metrics, u32 unicode, glyph_point_data* glyph)
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
	get_glyph_data(glyf_data, glyph_offset, &(glyph->glyph_data), glyph_length);
	if(glyph_length > 0 && glyph->glyph_data == NULL){
		return -1;
	}

	return 0;
}


// 功能：按照多个 Unicode 范围加载字形数据和统一排版框
int load_ttf_ranges(char* file, const unicode_range* range_array, int range_count, ttf_font_data* font)
{
	table*		ttf_table;
	head_table	head;
	hhea_header	hhea;
	cmap_data	cmap;
	u32*		loca_array;
	u64		total_count;
	u32		unicode;
	u16		cmap_format;
	int		loca_length;
	int		glyph_position;
	int		i;
	int		j;

	if(file == NULL || range_array == NULL || range_count <= 0 || font == NULL){
		return -1;
	}

	font->glyph_array = NULL;
	font->glyph_count = 0;
	memset(&(font->box), 0, sizeof(font_box));
	total_count = 0;

	// 统计所有范围的字符总数，并拒绝逆序或过大的范围
	for(i=0; i<range_count; i++){
		if(range_array[i].begin > range_array[i].end){
			return -1;
		}
		total_count += (u64)range_array[i].end-range_array[i].begin+1;
		if(total_count > 0x7fffffff){
			return -1;
		}
	}

	read_ttf_data(file, &ttf_table);
	read_head_table(ttf_table[0].data, &head);
	read_hhea_header(ttf_table[4].data, &hhea);
	font->box.x_min = 0;
	font->box.y_min = hhea.descent;
	font->box.x_max = head.unit_per_Em;
	font->box.y_max = hhea.ascent;

	// 字符映射和字形偏移只解析一次，全部范围共享解析结果
	cmap_format = read_cmap(ttf_table[1].data, &cmap);
	loca_length = read_loca_table(ttf_table[2].data, head.index_to_loca_format, &loca_array, ttf_table[2].length);
	if((cmap_format != 12 && cmap_format != 4) || loca_array == NULL){
		free_cmap(&cmap);
		for(i=0; i<6; i++){
			free(ttf_table[i].data);
		}
		free(ttf_table);
		return -1;
	}

	font->glyph_array = (glyph_point_data*)calloc((size_t)total_count, sizeof(glyph_point_data));
	if(font->glyph_array == NULL){
		free(loca_array);
		free_cmap(&cmap);
		for(i=0; i<6; i++){
			free(ttf_table[i].data);
		}
		free(ttf_table);
		return -1;
	}
	font->glyph_count = (int)total_count;

	// 按范围顺序加载每个 Unicode，空轮廓仍保留 advanceWidth
	glyph_position = 0;
	for(i=0; i<range_count; i++){
		for(unicode=range_array[i].begin; ; unicode++){
			if(load_glyph_entry(&cmap, loca_array, loca_length, ttf_table[3].data,
				ttf_table[5].data, hhea.number_of_h_metrics, unicode,
				&(font->glyph_array[glyph_position])) != 0){
				for(j=0; j<glyph_position; j++){
					free(font->glyph_array[j].glyph_data);
				}
				free(font->glyph_array);
				font->glyph_array = NULL;
				font->glyph_count = 0;
				free(loca_array);
				free_cmap(&cmap);
				for(j=0; j<6; j++){
					free(ttf_table[j].data);
				}
				free(ttf_table);
				return -1;
			}
			glyph_position++;
			if(unicode == range_array[i].end){
				break;
			}
		}
	}

	free(loca_array);
	free_cmap(&cmap);
	for(i=0; i<6; i++){
		free(ttf_table[i].data);
	}
	free(ttf_table);

	return 0;
}


// 功能：释放范围加载得到的全部字形数据
void free_ttf_font_data(ttf_font_data* font)
{
	int	i;

	if(font == NULL){
		return;
	}

	for(i=0; i<font->glyph_count; i++){
		free(font->glyph_array[i].glyph_data);
	}
	free(font->glyph_array);
	font->glyph_array = NULL;
	font->glyph_count = 0;
	memset(&(font->box), 0, sizeof(font_box));
}


// 功能：加载字体中的中文 BMP 字形数据及统一排版框
void load_ttf_BMP(char* file, glyph_point_data** glyph_array, font_box* box)
{
	unicode_range	range;
	ttf_font_data	font;

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
