#include"ttf.h"
#include"bmp.h"


#pragma pack(1)
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
	float	x1;
	float	y1;
	float	x2;
	float	y2;
}glyph_edge;

typedef struct{
	float	x;
	int	direction;
}glyph_intersection;


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
void read_ttf_data(char* file, table** pp_table)
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
	k = 	0;

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
void read_head_table(u8* head_table_data, head_table* head_table_struct)
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



// 功能：从 cmap 表选择并复制可用的字符映射子表
u16 get_cmap_subtable_data(u8* cmap_table_data, u8** pp_subtable_data)
{


	int			i;
	int			j;
	u16			number_of_subtable;
	cmap_subtable_header*	header;			// 使用后需要释放
	u16			subtable_length_format_4;
	u32			subtable_length_format_12;
	u32			subtable_length;
	u16			subtable_format;
	u8*			pointer;
	int			current_subtable;
	static int		ever_read_format4;
	

	// 初始化表指针和候选子表长度
	i			= 0;
	j			= 0;
	pointer			= cmap_table_data;
	subtable_format		= 0;
	
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


	// 优先记录 Format 4，存在 Format 12 时改用 Format 12
	for(i=0;i<number_of_subtable;i++){

		if(header[i].platform_id	== PLATFORM_ID_WINDOWS && 
		header[i].encoding_id  		== ENCODING_ID_3_BMP &&
		ever_read_format4		== 0){
			current_subtable	= i;
			ever_read_format4++;	
			break;
		}
		if(header[i].platform_id	== PLATFORM_ID_WINDOWS && 
		header[i].encoding_id  		== ENCODING_ID_3_UNICODE_FULL &&
		ever_read_format4		== 1){
			current_subtable = i;
			break;
		}else if(i == number_of_subtable-1){
			return -1;
		}
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
		return subtable_format;
	}
	// 为完整子表申请独立缓冲区
	*pp_subtable_data = (u8*)malloc(sizeof(u8) * subtable_length);
	// 回到子表起始位置
	if(subtable_format == 12){
		pointer -= 2*sizeof(u16);
	}else{
		pointer -= sizeof(u16);
	}
	// 复制完整子表数据
	memcpy(*pp_subtable_data, pointer, subtable_length);

	// 释放临时子表目录
	free(header);

	return subtable_format;
}






// 功能：解析 cmap Format 4 子表
u16 read_cmap_format4_subtable(u8* cmap_subtable_data, cmap_format4_data* p_data)
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
		pointer += sizeof(u16)*(segment_count);
		memcpy(p_data->glyph_index_array, pointer, sizeof(u16)*glyph_index_array_length);
		for(i=0;i<glyph_index_array_length;i++){
			convert((p_data->glyph_index_array)[i], sizeof(u16)*BIT_PER_BYTE);
		}
	}

	return segment_count;
}


// 功能：通过 cmap Format 4 获取字形索引
int get_glyph_index_format4(cmap_format4_data format4, u32 unicode, u16 segment_count)
{
	int 	i;
	int 	glyph_index;
	int	offset;
	u16	current_segment;

	glyph_index	= 0;
	
	// 查找 Unicode 所属的编码分段
	for(i=0;;i++){
		if(unicode < format4.end_code[i]){
			current_segment = i;
			break;
		}
	}
	


	// 根据范围偏移或增量计算字形索引
	if(format4.id_range_offset[current_segment] == 0){
		glyph_index = unicode + format4.id_delta[current_segment];
		glyph_index %= 65536;
	}else{
		offset		= format4.id_range_offset[current_segment]/2 + unicode - format4.start_code[current_segment] - (segment_count - 1 - current_segment);
		glyph_index	= format4.glyph_index_array[offset-1];
		glyph_index %= 65536;	
	}

	return glyph_index;
}


// 功能：解析 cmap Format 12 子表
void read_cmap_format12_subtable(u8* cmap_subtable_data, cmap_format12_data* p_data)
{
	u8*	pointer;
	int	i;
	
	i	= 0;
	pointer	= cmap_subtable_data;
	
	// 读取并转换 Format 12 头部字段
	memcpy(p_data, cmap_subtable_data, sizeof(cmap_format12_data));
	pointer += sizeof(cmap_format12_data) - sizeof(u32*);

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
int get_glyph_index_format12(cmap_format12_data format12, u32 unicode)
{
	int	i;
	int	current_group;
	int 	glyph_index;

	// 查找 Unicode 所属的连续映射分组
	i		= 0;
	current_group	= 0;
	for(i=0; i<format12.groups_number; i++){
		if(unicode>=(format12.groups)[i].start_char_code &&
			unicode<=(format12.groups)[i].end_char_code){
			current_group = i;
			break;
		}
		// 已超过最后一个可能匹配的分组
		if(i == format12.groups_number-1){
			return 0;
		}
	}

	// 通过分组起始字形编号计算最终索引
	glyph_index	= (format12.groups)[current_group].start_glyph_id;
	glyph_index += (unicode-(format12.groups)[current_group].start_char_code);
	return glyph_index;
}




// 功能：读取 cmap 表并解析选中的字符映射格式
int read_cmap(u8* cmap_table_data, cmap_format4_data* p_format4, cmap_format12_data* p_format12, u16* segment_count)
{
	u16		format;
	u8*		cmap_subtable_data;
	int		i;
	int		result;		// 记录当前使用的 cmap 格式

	result 	= 0;
	format	= 0;

	// 第一次读取并解析 Format 4 子表
	format =	get_cmap_subtable_data(cmap_table_data, &cmap_subtable_data);

	if(format == 4){
		*segment_count	= read_cmap_format4_subtable(cmap_subtable_data, p_format4);
		free(cmap_subtable_data);
		result++;
	}else{
		printf("not find format4\n");
		return 0;
	}
	
	// 第二次读取并尝试解析 Format 12 子表
	format =	get_cmap_subtable_data(cmap_table_data, &cmap_subtable_data);
	printf("\n format2    %d\n",format);
	if(format == 12){
		read_cmap_format12_subtable(cmap_subtable_data, p_format12);
		free(cmap_subtable_data);
		result++;
	}
	return result;
}

// 功能：根据 cmap 格式获取 Unicode 对应的字形索引
int get_glyph_index(cmap_format4_data format4, cmap_format12_data format12, u16 segment_count, u32 unicode, int get_format_condition)
{
	int 	glyph_index;

	// 根据 Unicode 范围和已解析格式选择对应查找函数
	if(unicode>=UNICODE_BMP_CHINA_BEGIN && unicode<UNICODE_BMP_CHINA_END && get_format_condition>=1){
		glyph_index	= get_glyph_index_format4(format4, unicode, segment_count);
	}else if(unicode>=0x20000 && get_format_condition==2){
		glyph_index	= get_glyph_index_format12(format12, unicode);
	}else{
		glyph_index = 0;
	}

	return glyph_index;
}
		





// 功能：解析 loca 表并生成字形偏移数组
int read_loca_table(u8* data, u16 index_to_loca_format, u32** pp_result, u32 table_length)
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
void get_glyph_data(u8* glyf_data, u32 glyph_data_offset, u8** pp_glyph_data, int glyph_length)
{	
	u8*	pointer;
	
	// 为单个字形数据申请独立缓冲区
	pointer		= glyf_data + glyph_data_offset;
	*pp_glyph_data	= (u8*)malloc(sizeof(u8)*glyph_length);
	
	// 从 glyf 表指定偏移复制字形数据
	memcpy(*pp_glyph_data, pointer, glyph_length*sizeof(u8));
}
			
// 功能：补充连续控制点之间的隐式点并生成可绘制点数组
int process_point(point* raw_point_data, point** pp_point_data, int raw_point_length)
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


// 功能：计算贝塞尔曲线上指定参数位置的点
bezier_point bezier_data_get(float t, point* points, int count)
{
	bezier_point 	temporary_points[MAX_CONTROL_POINT];
	int		i;
	int		j;

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
void get_bezier(point* in, int count, bezier_point** pp_out)
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

	// 初始化首个轮廓结束位置
	end	= 1;

	printf("hellop\n");
	// 为直线或二次贝塞尔控制点申请空间
	draw_point = (point*)malloc(sizeof(point)*3);
	printf("hellop\n");

	while(1){
		// 根据上一个轮廓确定当前轮廓起点
		if(end	== 1){
			start	= 0;
		}else{
			start	= end+1;
		}
		// 查找当前轮廓的结束标记
		for(i=start; i<point_length; i++){
			if(point_data[i].locate == NEXT){
				end 	= i;
				break;
			}
		}
		// 所有轮廓处理完成后退出
		if(start >= point_length){
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
	}

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
		printf("fill word error\n");
	}
}

// 功能：加载字体中的中文 BMP 字形数据及统一排版框
void load_ttf_BMP(char* file, glyph_point_data** glyph_array, font_box* box)
{
	int 			i;
	u32			unicode;

	table*			ttf_table;
	// head 与 hhea 表数据
	head_table		head;
	hhea_header		hhea;
	// cmap 表数据
	cmap_format4_data 	format4;
	cmap_format12_data	format12;
	int			get_format_condition;
	int 			glyph_index;
	u16			segment_count;
	// loca 表数据
	u32*			loca_array;
	u32			glyph_offset;
	int			loca_length;
	int			glyph_length;
	// glyf 表数据
	int			point_length;


	i	= 0;

	read_ttf_data(file, &ttf_table);
	
	// 解析 head 表并释放原始表数据
	read_head_table(ttf_table[0].data, &head);
	free(ttf_table[0].data);

	// 根据 unitsPerEm、ascent 和 descent 建立统一排版框
	read_hhea_header(ttf_table[4].data, &hhea);
	box->x_min	= 0;
	box->y_min	= hhea.descent;
	box->x_max	= head.unit_per_Em;
	box->y_max	= hhea.ascent;
	free(ttf_table[4].data);

	// 解析 cmap 字符映射表
	get_format_condition	= read_cmap(ttf_table[1].data, &format4, &format12, &segment_count);
	// cmap 原始表数据由现有流程继续持有

	
	printf("hello\n");
	// 解析 loca 字形偏移表
	loca_length	= read_loca_table(ttf_table[2].data, head.index_to_loca_format, &loca_array, ttf_table[2].length);
	printf("loca_length = %x\n", loca_length);
	free(ttf_table[2].data);

	printf("hello\n");

	// 遍历中文 BMP 范围，复制字形数据和水平前进宽度
	if(get_format_condition == 1){
		// 为中文字形数组申请空间
		*glyph_array		= (glyph_point_data*)malloc(sizeof(glyph_point_data)*(UNICODE_BMP_CHINA_END-UNICODE_BMP_CHINA_BEGIN+1));
		for(unicode=UNICODE_BMP_CHINA_BEGIN, i=0; unicode<=UNICODE_BMP_CHINA_END; unicode++, i++){
			// 获取当前 Unicode 对应的字形偏移与长度
			glyph_index 	= get_glyph_index(format4, format12, segment_count, unicode, get_format_condition);
			glyph_offset	= loca_array[glyph_index];
			glyph_length	= loca_array[glyph_index+1] - loca_array[glyph_index];
			// 保存字形原始数据、Unicode 和水平度量
			if(unicode == 0x4fdd){
				printf("glyph_index  = %x\n",glyph_index);
				printf("glyph_offset = %x\n",glyph_offset);
				printf("glyph_length = %x\n",glyph_length);
			}

			get_glyph_data(ttf_table[3].data, glyph_offset, &(*glyph_array)[i].glyph_data, glyph_length);
			(*glyph_array)[i].unicode	= unicode;
			(*glyph_array)[i].glyph_length	= glyph_length;
			(*glyph_array)[i].advance_width	= get_advance_width(ttf_table[5].data, hhea.number_of_h_metrics, glyph_index);
		}
	}
	free(loca_array);
	free(format4.end_code);
	free(format4.start_code);
	free(format4.id_delta);
	free(format4.id_range_offset);
	free(ttf_table[3].data);
	free(ttf_table[5].data);
}


// 功能：以指定排版框中心绘制单个空心字符
void draw_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	int	i;
	int	target;
	unicode_t	unicode;
	point*	point_data;
	int 	point_length;
	int	offset_x;
	int	offset_y;

	if(word_to_unicode(word, encoding, &unicode) != 0){
		printf("word error\n");
		return;
	}

	// 查找 Unicode 对应的已加载字形
	target	= -1;
	for(i=0; i<UNICODE_BMP_NUMBER; i++){
		if(glyph_array[i].unicode == unicode){
			target	= i;
			break;
		}
	}
	if(target < 0){
		printf("not find correct glyph_data\n");
		return;
	}

	// 解析轮廓点并计算排版框原点偏移
	point_length	= glyph_to_point(glyph_array[target].glyph_data, &point_data, glyph_array[target].glyph_length);
	offset_x	= center_x-(box.x_min+box.x_max)/2;
	offset_y	= center_y-(box.y_min+box.y_max)/2;

	// 输出轮廓点调试信息
	for(i=0; i<point_length; i++){
		printf("i %d   x %f    y %f    locate %d\n", i, point_data[i].x, point_data[i].y, point_data[i].locate);
	}


	// 调用旧轮廓绘制函数生成空心字形
	draw_word_from_point(bmp.width, bmp.height, bmp.color_data, bmp.color, point_data, point_length, offset_x, offset_y);
}


// 功能：以指定排版框中心绘制单个实心字符
void draw_filled_word(glyph_point_data* glyph_array, font_box box, const char* word, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	int	i;
	int	target;
	unicode_t	unicode;
	point*	point_data;
	int 	point_length;
	int	offset_x;
	int	offset_y;

	if(word_to_unicode(word, encoding, &unicode) != 0){
		printf("word error\n");
		return;
	}

	// 查找 Unicode 对应的已加载字形
	target = -1;
	for(i=0; i<UNICODE_BMP_NUMBER; i++){
		if(glyph_array[i].unicode == unicode){
			target = i;
			break;
		}
	}
	if(target < 0){
		printf("not find correct glyph_data\n");
		return;
	}

	// 解析轮廓点并计算排版框原点偏移
	point_length	= glyph_to_point(glyph_array[target].glyph_data, &point_data, glyph_array[target].glyph_length);
	offset_x	= center_x-(box.x_min+box.x_max)/2;
	offset_y	= center_y-(box.y_min+box.y_max)/2;

	// 绘制完成后释放本次解析得到的点数组
	draw_filled_word_from_point(bmp.width, bmp.height, bmp.color_data, bmp.color, point_data, point_length, offset_x, offset_y);
	free(point_data);
}


// 功能：从左向右绘制空心字符串，并在完整字框越界时截断
void draw_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	const char*	current;
	char*		word;
	font_box	word_box;
	unicode_t	unicode;
	int		string_length;
	int		word_length;
	int		target;
	int		advance_width;
	int		pen_x;
	int		word_center_x;
	int		offset_y;
	int		first;
	int		i;

	if(string == NULL || string[0] == '\0'){
		printf("string error\n");
		return;
	}

	// 为每次截取的单字符申请可复用缓冲区
	string_length	= strlen(string);
	current		= string;
	word		= (char*)malloc(string_length+1);
	offset_y	= center_y-(box.y_min+box.y_max)/2;
	first		= 1;
	pen_x		= 0;

	if(word == NULL){
		printf("string malloc error\n");
		return;
	}

	// 逐字符解析、定位并调用旧的单字符绘制函数
	while(current[0] != '\0'){
		word_length = get_character_byte_length(current, encoding);
		if(word_length < 0){
			printf("string error\n");
			break;
		}

		// 复制当前编码字符并转换为 Unicode
		memcpy(word, current, word_length);
		word[word_length] = '\0';
		if(word_to_unicode(word, encoding, &unicode) != 0){
			printf("word error\n");
			break;
		}

		// 查找当前字符对应的字形和水平前进宽度
		target = -1;
		for(i=0; i<UNICODE_BMP_NUMBER; i++){
			if(glyph_array[i].unicode == unicode){
				target = i;
				break;
			}
		}
		if(target < 0){
			printf("not find correct glyph_data\n");
			break;
		}

		advance_width = glyph_array[target].advance_width;
		if(advance_width <= 0){
			printf("advance width error\n");
			break;
		}

		// 首字符中心由输入指定，后续字符沿排版笔位置向右排列
		if(first != 0){
			pen_x = center_x-advance_width/2;
			first = 0;
		}
		// 当前完整排版框越界时截断剩余字符串
		if(pen_x < 0 || pen_x+advance_width > bmp.width ||
		   offset_y+box.y_min < 0 || offset_y+box.y_max > bmp.height){
			break;
		}

		// 使用当前字形宽度构造排版框并复用 draw_word
		word_center_x	= pen_x+advance_width/2;
		word_box	= box;
		word_box.x_min	= 0;
		word_box.x_max	= advance_width;
		draw_word(glyph_array, word_box, word, encoding, bmp, word_center_x, center_y);

		pen_x	+= advance_width;
		current	+= word_length;
	}

	free(word);
}


// 功能：从左向右绘制实心字符串，并在完整字框越界时截断
void draw_filled_string(glyph_point_data* glyph_array, font_box box, const char* string, word_encoding encoding, bmp_data bmp, int center_x, int center_y)
{
	const char*	current;
	char*		word;
	font_box	word_box;
	unicode_t	unicode;
	int		string_length;
	int		word_length;
	int		target;
	int		advance_width;
	int		pen_x;
	int		word_center_x;
	int		offset_y;
	int		first;
	int		i;

	if(string == NULL || string[0] == '\0'){
		printf("string error\n");
		return;
	}

	// 为每次截取的单字符申请可复用缓冲区
	string_length	= strlen(string);
	current		= string;
	word		= (char*)malloc(string_length+1);
	offset_y	= center_y-(box.y_min+box.y_max)/2;
	first		= 1;
	pen_x		= 0;

	if(word == NULL){
		printf("string malloc error\n");
		return;
	}

	// 逐字符解析、定位并调用实心单字符绘制函数
	while(current[0] != '\0'){
		word_length = get_character_byte_length(current, encoding);
		if(word_length < 0){
			printf("string error\n");
			break;
		}

		// 复制当前编码字符并转换为 Unicode
		memcpy(word, current, word_length);
		word[word_length] = '\0';
		if(word_to_unicode(word, encoding, &unicode) != 0){
			printf("word error\n");
			break;
		}

		// 查找当前字符对应的字形和水平前进宽度
		target = -1;
		for(i=0; i<UNICODE_BMP_NUMBER; i++){
			if(glyph_array[i].unicode == unicode){
				target = i;
				break;
			}
		}
		if(target < 0){
			printf("not find correct glyph_data\n");
			break;
		}

		advance_width = glyph_array[target].advance_width;
		if(advance_width <= 0){
			printf("advance width error\n");
			break;
		}

		// 首字符中心由输入指定，后续字符沿排版笔位置向右排列
		if(first != 0){
			pen_x = center_x-advance_width/2;
			first = 0;
		}
		// 当前完整排版框越界时截断剩余字符串
		if(pen_x < 0 || pen_x+advance_width > bmp.width ||
		   offset_y+box.y_min < 0 || offset_y+box.y_max > bmp.height){
			break;
		}

		// 使用当前字形宽度构造排版框并调用实心单字函数
		word_center_x	= pen_x+advance_width/2;
		word_box	= box;
		word_box.x_min	= 0;
		word_box.x_max	= advance_width;
		draw_filled_word(glyph_array, word_box, word, encoding, bmp, word_center_x, center_y);

		pen_x	+= advance_width;
		current	+= word_length;
	}

	free(word);
}
