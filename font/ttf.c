#include"ttf.h"
#include"bmp.h"




// get key table data
void read_ttf_data(char* file, table** pp_table)
{
	FILE*		fp;
	file_header	ttf_header;
	table_entry*	p_table_entry_array;	// need to free
	int		i;
	int		j;
	int 		k;
	int		current_table;
	char*		key_table_name[4];

	// open file
	fp = fopen(file,"rb");
	if(fp == NULL){
		printf("open file error\n");
	}

	// initialize
	i =	0;
	j =	0;
	k = 	0;

	// initialize key table name
	for(i=0; i<4; i++){
		key_table_name[i] = (char*)malloc(sizeof(char)*5);
	}
	key_table_name[0]	= "head";
	key_table_name[1]	= "cmap";
	key_table_name[2]	= "loca";
	key_table_name[3]	= "glyf";

	// initialize *pp_table space 
	*pp_table	= (table*)malloc(sizeof(table)*4);

	// get file header
	fread(&ttf_header, sizeof(file_header), 1, fp);

	convert(ttf_header.version, 		sizeof(ttf_header.version)		*BIT_PER_BYTE); 	
	convert(ttf_header.table_number, 	sizeof(ttf_header.table_number)		*BIT_PER_BYTE);
	convert(ttf_header.search_range, 	sizeof(ttf_header.search_range)		*BIT_PER_BYTE);
	convert(ttf_header.entry_selector,	sizeof(ttf_header.entry_selector)	*BIT_PER_BYTE);
	convert(ttf_header.range_shift,		sizeof(ttf_header.range_shift)		*BIT_PER_BYTE);

	// get table entry array
	p_table_entry_array	= (table_entry*)malloc(sizeof(table_entry)*ttf_header.table_number);
	fread(p_table_entry_array, sizeof(table_entry), ttf_header.table_number, fp);
	for(i=0;i<ttf_header.table_number;i++){
		convert(((p_table_entry_array)[i]).check_sum,	sizeof((p_table_entry_array[i]).check_sum)	*BIT_PER_BYTE);
		convert(((p_table_entry_array)[i]).offset,	sizeof((p_table_entry_array[i]).offset)		*BIT_PER_BYTE);
		convert(((p_table_entry_array)[i]).length,	sizeof((p_table_entry_array[i]).length)		*BIT_PER_BYTE);
	}

	// print table entry array to test
	for(i=0;i<ttf_header.table_number;i++){
		printf("%c %c %c %c\n", p_table_entry_array[i].tag[0],
					p_table_entry_array[i].tag[1],
					p_table_entry_array[i].tag[2],
					p_table_entry_array[i].tag[3]);
		printf("%0x\n",((p_table_entry_array)[i]).check_sum);
		printf("%0x\n",((p_table_entry_array)[i]).offset);
		printf("%0x\n",((p_table_entry_array)[i]).length);	
	}

	// get table data
	for(i=0; i<4; i++){
		// locate to table
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
			}
		}
		// write data
		// write table name
		for(k=0; k<4; k++){
			(*pp_table)[current_table].name[k] = p_table_entry_array[j].tag[k];
		}
		// write table length
		(*pp_table)[current_table].length = p_table_entry_array[j].length;
		// locate table data
		fseek(fp, p_table_entry_array[j].offset, SEEK_SET);
		// initialize table data space
		(*pp_table)[current_table].data = (u8*)malloc(p_table_entry_array[j].length);
		// write table data
		fread((*pp_table)[current_table].data, sizeof(u8), p_table_entry_array[j].length, fp);
		j++;
	}

	// free
	free(p_table_entry_array);

	fclose(fp);
}



void read_head_table(u8* head_table_data, head_table* head_table_struct)
{
	// get head_table data
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



// get cmap_subtable_data from cmap_table
u16 get_cmap_subtable_data(u8* cmap_table_data, u8** pp_subtable_data)
{


	int			i;
	int			j;
	u16			number_of_subtable;
	cmap_subtable_header*	header;			// need to free
	u16			subtable_length_format_4;
	u32			subtable_length_format_12;
	u32			subtable_length;
	u16			subtable_format;
	u8*			pointer;
	int			current_subtable;
	static int		ever_read_format4;
	

	// initialize
	i			= 0;
	j			= 0;
	pointer			= cmap_table_data;
	subtable_format		= 0;
	
	// get number of subtable
	pointer	+= sizeof(u16);	// jump version
	memcpy(&number_of_subtable, pointer, sizeof(u16));
	convert(number_of_subtable, sizeof(number_of_subtable) * BIT_PER_BYTE);

	// initialize subtable header space and pointer
	pointer += sizeof(u16); // jump number_of_subtable
	header	= (cmap_subtable_header*)malloc(sizeof(cmap_subtable_header)*number_of_subtable);

	// get cmap subtable header	
	for(i=0; i<number_of_subtable; i++){
		memcpy(&(header[i]), pointer, sizeof(cmap_subtable_header));

		convert(header[i].platform_id, 	sizeof(header[i].platform_id)	* BIT_PER_BYTE);
		convert(header[i].encoding_id,	sizeof(header[i].encoding_id)	* BIT_PER_BYTE);
		convert(header[i].offset, 	sizeof(header[i].offset)	* BIT_PER_BYTE);
		pointer += sizeof(cmap_subtable_header);
	}


	// find correct subtable
	// find format4 first, choose format12 if it exsists
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
	// locate
	pointer	= cmap_table_data + header[current_subtable].offset;
	// get subtable format
	memcpy(&subtable_format, pointer, sizeof(u16));
	convert(subtable_format, sizeof(subtable_format) * BIT_PER_BYTE);
	pointer	+= sizeof(u16);		// jump format
	// get subtable length
	if(subtable_format == 4){
		memcpy(&subtable_length_format_4, pointer, sizeof(u16));
		convert(subtable_length_format_4, sizeof(u16) * BIT_PER_BYTE);
		subtable_length = subtable_length_format_4;
	}else if(subtable_format == 12){
		pointer += sizeof(u16);	//jump reserved
		memcpy(&subtable_length_format_12, pointer, sizeof(u32));
		convert(subtable_length_format_12, sizeof(u32) * BIT_PER_BYTE);
		subtable_length = subtable_length_format_12;
	}else{
		return subtable_format;
	}
	// initialize *pp_subtable_data space
	*pp_subtable_data = (u8*)malloc(sizeof(u8) * subtable_length);
	// back to subtable head
	if(subtable_format == 12){
		pointer -= 2*sizeof(u16);
	}else{
		pointer -= sizeof(u16);
	}
	// get subtable data
	memcpy(*pp_subtable_data, pointer, subtable_length);

	// free
	free(header);

	return subtable_format;
}






// process format4 data to get glyph_index easier
u16 read_cmap_format4_subtable(u8* cmap_subtable_data, cmap_format4_data* p_data)
{
	u8*			pointer;
	cmap_format4_header	header;
	u16			segment_count;
	u16			glyph_index_array_length;
	int			i;

	
	pointer	= cmap_subtable_data;
	i	= 0;

	// get format4 header
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

	// calculate glyph_index_array_length
	glyph_index_array_length	= header.length-sizeof(cmap_format4_header)-sizeof(u16)-segment_count*sizeof(u16)*4;
	glyph_index_array_length /=2;
	printf("glyph_index_array_length = %d\n", glyph_index_array_length);

	// initialize space
	p_data->end_code	= (u16*)malloc(sizeof(u16)*segment_count);
	p_data->start_code	= (u16*)malloc(sizeof(u16)*segment_count);
	p_data->id_delta	= (u16*)malloc(sizeof(u16)*segment_count);
	p_data->id_range_offset	= (u16*)malloc(sizeof(u16)*segment_count);
	if(glyph_index_array_length > 0){
		p_data->glyph_index_array	= (u16*)malloc(sizeof(u16)*glyph_index_array_length);
	}else{
		p_data->glyph_index_array	= NULL;
	}
	// write format4 data
	memcpy(p_data->end_code, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->end_code)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count+1);	// jump reserved and end_code[]
	
	memcpy(p_data->start_code, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->start_code)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count);	// jump start_code[]
	
	memcpy(p_data->id_delta, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->id_delta)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count);	// jump id_delta[]

	memcpy(p_data->id_range_offset, pointer, sizeof(u16)*segment_count);
	for(i=0;i<segment_count;i++){
		convert((p_data->id_range_offset)[i], sizeof(u16)*BIT_PER_BYTE);
	}
	pointer += sizeof(u16)*(segment_count);	// jump id_range_offset[]

	if(glyph_index_array_length > 0){
		pointer += sizeof(u16)*(segment_count);
		memcpy(p_data->glyph_index_array, pointer, sizeof(u16)*glyph_index_array_length);
		for(i=0;i<glyph_index_array_length;i++){
			convert((p_data->glyph_index_array)[i], sizeof(u16)*BIT_PER_BYTE);
		}
	}

	//printf("glyph_index_array\n");
	//for(i=0; i<glyph_index_array_length; i++){
	//	printf("%x  ", (p_data->glyph_index_array)[i]);
	//}

	return segment_count;
}


// get glyph_index from format4
int get_glyph_index_format4(cmap_format4_data format4, u32 unicode, u16 segment_count)
{
	int 	i;
	int 	glyph_index;
	int	offset;
	u16	current_segment;

	glyph_index	= 0;
	
	// find segment
	for(i=0;;i++){
		if(unicode < format4.end_code[i]){
			current_segment = i;
			break;
		}
	}
	


	// get glyph index
	if(format4.id_range_offset[current_segment] == 0){
		glyph_index = unicode + format4.id_delta[current_segment];
		glyph_index %= 65536;
	}else{
		//printf("id_range_offset != 0\n");
		offset		= format4.id_range_offset[current_segment]/2 + unicode - format4.start_code[current_segment] - (segment_count - 1 - current_segment);
		glyph_index	= format4.glyph_index_array[offset-1];
		glyph_index %= 65536;	
	}

	return glyph_index;
}


void read_cmap_format12_subtable(u8* cmap_subtable_data, cmap_format12_data* p_data)
{
	u8*	pointer;
	int	i;
	
	i	= 0;
	pointer	= cmap_subtable_data;
	
	// read header data
	memcpy(p_data, cmap_subtable_data, sizeof(cmap_format12_data));
	pointer += sizeof(cmap_format12_data) - sizeof(u32*);

	convert(p_data->format, 	sizeof(u16)*BIT_PER_BYTE);
	convert(p_data->reserved, 	sizeof(u16)*BIT_PER_BYTE);
	convert(p_data->length, 	sizeof(u32)*BIT_PER_BYTE);
	convert(p_data->language, 	sizeof(u32)*BIT_PER_BYTE);
	convert(p_data->groups_number, 	sizeof(u32)*BIT_PER_BYTE);


	// initialize groups space
	p_data->groups = (sequential_map_group*)malloc(sizeof(sequential_map_group)*(p_data->groups_number));
	
	// write groups data
	memcpy(p_data->groups, pointer, sizeof(sequential_map_group)*(p_data->groups_number));
	for(i=0; i<p_data->groups_number; i++){
		convert((p_data->groups)[i].start_char_code, 	sizeof(u32)*BIT_PER_BYTE);
		convert((p_data->groups)[i].end_char_code, 	sizeof(u32)*BIT_PER_BYTE);
		convert((p_data->groups)[i].start_glyph_id, 	sizeof(u32)*BIT_PER_BYTE);
	}
}

// get glyph_index from format12 subtable
int get_glyph_index_format12(cmap_format12_data format12, u32 unicode)
{
	int	i;
	int	current_group;
	int 	glyph_index;

	// find correct group
	i		= 0;
	current_group	= 0;
	for(i=0; i<format12.groups_number; i++){
		if(unicode>=(format12.groups)[i].start_char_code &&
			unicode<=(format12.groups)[i].end_char_code){
			current_group = i;
			break;
		}
		// not find
		if(i == format12.groups_number-1){
			return 0;
		}
	}

	// get glyph index
	glyph_index	= (format12.groups)[current_group].start_glyph_id;
	glyph_index += (unicode-(format12.groups)[current_group].start_char_code);
	return glyph_index;
}




int read_cmap(u8* cmap_table_data, cmap_format4_data* p_format4, cmap_format12_data* p_format12, u16* segment_count)
{
	u16		format;
	u8*		cmap_subtable_data;
	int		i;
	int		result;		// result means get_format_condition

	result 	= 0;
	format	= 0;

	// get subtable data
	format =	get_cmap_subtable_data(cmap_table_data, &cmap_subtable_data);

	// format4
	if(format == 4){
		*segment_count	= read_cmap_format4_subtable(cmap_subtable_data, p_format4);
		free(cmap_subtable_data);
		result++;
	}else{
		printf("not find format4\n");
		return 0;
	}
	
	// get subtable data 
	format =	get_cmap_subtable_data(cmap_table_data, &cmap_subtable_data);
	printf("\n format2    %d\n",format);
	// format12
	if(format == 12){
		read_cmap_format12_subtable(cmap_subtable_data, p_format12);
		free(cmap_subtable_data);
		result++;
	}
	return result;
}

int get_glyph_index(cmap_format4_data format4, cmap_format12_data format12, u16 segment_count, u32 unicode, int get_format_condition)
{
	int 	glyph_index;

	if(unicode>=UNICODE_BMP_CHINA_BEGIN && unicode<UNICODE_BMP_CHINA_END && get_format_condition>=1){
		glyph_index	= get_glyph_index_format4(format4, unicode, segment_count);
	}else if(unicode>=0x20000 && get_format_condition==2){
		glyph_index	= get_glyph_index_format12(format12, unicode);
	}else{
		glyph_index = 0;
	}

	return glyph_index;
}
		





// read loca_table data
int read_loca_table(u8* data, u16 index_to_loca_format, u32** pp_result, u32 table_length)
{
	int 	loca_length;
	int	i;
	u8*	pointer;

	i		= 0;
	loca_length	= 0;
	pointer		= data;
	

	// calculate loca array length
	if(index_to_loca_format == 0){
		loca_length	= table_length/2;
	}else{
		loca_length 	= table_length/4;
	}

	// initialize array space
	*pp_result	= (u32*)malloc(sizeof(u32)*loca_length);
	// write data
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

// get glyph_data
void get_glyph_data(u8* glyf_data, u32 glyph_data_offset, u8** pp_glyph_data, int glyph_length)
{	
	u8*	pointer;
	
	// initialize
	pointer		= glyf_data + glyph_data_offset;
	*pp_glyph_data	= (u8*)malloc(sizeof(u8)*glyph_length);
	
	// get data
	memcpy(*pp_glyph_data, pointer, glyph_length*sizeof(u8));
}
			
// process point data to draw 
int process_point(point* raw_point_data, point** pp_point_data, int raw_point_length)
{
	int 	point_length;
	int	i;
	int 	j;

	point_length	= 1;
	i		= 0;
	j		= 0;
	
	// calculate length
	for(i=1; i<raw_point_length; i++,point_length++){
		if(raw_point_data[i].locate == 0 && raw_point_data[i-1].locate == 0){

			point_length++;
		}
	}

	// initialize point data
	*pp_point_data	= (point*)malloc(sizeof(point)*point_length);

	// write point data
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

// process glyph_data to point_data
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

	// get header data
	memcpy(&header, pointer, sizeof(header));
	// convert
	convert(header.number_of_contours, 	sizeof(s16) * BIT_PER_BYTE);
	convert(header.x_min, 			sizeof(s16) * BIT_PER_BYTE);
	convert(header.y_min, 			sizeof(s16) * BIT_PER_BYTE);
	convert(header.x_max, 			sizeof(s16) * BIT_PER_BYTE);
	convert(header.y_max, 			sizeof(s16) * BIT_PER_BYTE);
	// shift pointer
	pointer += sizeof(header);

	// initialize end_pointer_of_contours
	end_pointer_of_contours	= (s16*)malloc(sizeof(s16)*header.number_of_contours);
	// get end_pointer_of_contours
	memcpy(end_pointer_of_contours, pointer, sizeof(s16)*header.number_of_contours);
	pointer += sizeof(s16)*header.number_of_contours;
	for(i=0; i<header.number_of_contours; i++){
		convert(end_pointer_of_contours[i], sizeof(s16) * BIT_PER_BYTE);
	}


	// instruction length
	memcpy(&instruction_length, pointer, sizeof(u16));
	pointer += sizeof(u16);
	convert(instruction_length, sizeof(u16) * BIT_PER_BYTE);
	// instructions
	if(instruction_length>0){
		instructions = (u8*)malloc(sizeof(u8)*instruction_length);
		memcpy(instructions, pointer, sizeof(u8)*instruction_length);
		pointer += sizeof(u8)*instruction_length;
	}

	// initialize pointer_x and flag_length
	flag_length	= end_pointer_of_contours[header.number_of_contours-1] + 1;
	pointer_x 	= pointer + flag_length*sizeof(u8);

	// initialize flag space
	flag		= (u8*)malloc(sizeof(u8)*flag_length);
	// get flag data
	memcpy(flag, pointer, sizeof(u8)*flag_length);


	// get raw point length and pointer_y 
	pointer	= pointer_x;
	for(i=0; i<flag_length;){
		// calculate point length
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
		// calculate pointer_y
		if(bit_1 != 0){
			pointer += sizeof(u8);

		}else if(bit_1==0 && bit_4 == 0){
			pointer += sizeof(u8)*2;
		}
		// reduce repeat_time if repeat_time>0 and process it reduce to 0
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


	// get x_length and y_length
	x_length	= pointer_y - pointer_x;
	y_length	= &(glyph_data[glyph_length-1]) - pointer_y + 1;

	// initialize x_data and y_data space
	x_data	= (u8*)malloc(sizeof(u8)*x_length);
	y_data 	= (u8*)malloc(sizeof(u8)*y_length);
	// write x_data and y_data
	memcpy(x_data, pointer_x, x_length);
	memcpy(y_data, pointer_y, y_length);


	// initialize raw point data space
	raw_point_length	= point_length;
	raw_point_data		= (point*)malloc(sizeof(point)*raw_point_length);

	// get point data
	j	= 0;	// for x
	k	= 0;	// for y
	m	= 0;	// for end_pointer_of_contours
	n	= 0;	// for point
	for(i=0; i<flag_length; n++){
		// get bit data
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

		// locate
		if(bit_0 == 0){
			(raw_point_data[n]).locate = 0;
		}else{
			(raw_point_data[n]).locate = 1;
		}
		if(i == end_pointer_of_contours[m]){
 			(raw_point_data[n]).locate = NEXT;
			m++;
		}

		// x data
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

		// y_data
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

		// reduce repeat_time if repeat_time>0 and process it reduce to 0
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
	// free
	free(end_pointer_of_contours);
	free(flag);
	free(x_data);
	free(y_data);
	free(raw_point_data);
	
	return point_length;
}


// get bezier one point data
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

// get all bezier data
void get_bezier(point* in, int count, bezier_point** pp_out)
{
	float 	step;
	float 	t;
	int	i;

	step	= (1.0) / (OUT_COUNT);
	t	= 0;
	*pp_out	= (bezier_point*)malloc(sizeof(bezier_point)*OUT_COUNT);

	for(i=0; i<OUT_COUNT; i++){
		bezier_point temporary_point	= bezier_data_get(t, in, count);
		t += step;
		(*pp_out)[i].x	= temporary_point.x;
		(*pp_out)[i].y	= temporary_point.y;
	}
}

// draw bezier to color data
void draw_bezier(int width, u8* color_data, u8 color[3], point* point_data, int count, int offset_x, int offset_y)
{
	int		i;
	bezier_point*	draw_point_data;

	i		= 0;

	// get bezier point data
	get_bezier(point_data, count, &draw_point_data);
	
	// draw
	for(i=0; i<OUT_COUNT; i++){
		int	x = (draw_point_data[i]).x;
		int	y = (draw_point_data[i]).y;
		if(x + offset_x <0){
			printf("draw bezier error x<0\n");
			continue;
		}
		if(y + offset_y <0){
			printf("draw bezier error y<0\n");
			continue;
		}

		COLOR_DATA((x+offset_x), (y+offset_y), 0) = color[0];
		COLOR_DATA((x+offset_x), (y+offset_y), 1) = color[1];
		COLOR_DATA((x+offset_x), (y+offset_y), 2) = color[2];
	}

}


void draw_word_from_point(int width, u8* color_data, u8 color[3], point* point_data, int point_length, int offset_x, int offset_y)
{
	int 	start;
	int 	end;
	int	i;
	point*	draw_point;
	int	first;

	// begin condition
	end	= 1;

	printf("hellop\n");
	// initialize draw_point space
	draw_point = (point*)malloc(sizeof(point)*3);
	printf("hellop\n");

	while(1){
		// get contour start
		if(end	== 1){
			start	= 0;
		}else{
			start	= end+1;
		}
		// get contour end
		for(i=start; i<point_length; i++){
			if(point_data[i].locate == NEXT){
				end 	= i;
				break;
			}
		}
		// jump out
		if(start >= point_length){
			break;
		}



		// draw
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
				draw_bezier(width, color_data, color, draw_point, 2, offset_x, offset_y);

				i++;
				continue;
			}
			if(i+2>end){
				break;
			}

			draw_point[2]	= point_data[i+2];
			draw_bezier(width, color_data, color, draw_point, 3, offset_x, offset_y);

			i += 2;
		}
	}

}

// load ttf_data to glyph_array only BMP
void load_ttf_BMP(char* file, glyph_point_data** glyph_array)
{
	int 			i;
	u32			unicode;

	table*			ttf_table;
	// head
	head_table		head;
	// cmap
	cmap_format4_data 	format4;
	cmap_format12_data	format12;
	int			get_format_condition;
	int 			glyph_index;
	u16			segment_count;
	// loca
	u32*			loca_array;
	u32			glyph_offset;
	int			loca_length;
	int			glyph_length;
	// glyf
	int			point_length;


	i	= 0;

	read_ttf_data(file, &ttf_table);
	
	// head
	read_head_table(ttf_table[0].data, &head);
	// free head table data
	free(ttf_table[0].data);

	// cmap
	get_format_condition	= read_cmap(ttf_table[1].data, &format4, &format12, &segment_count);
	//free(ttf_table[1].data);

	
	printf("hello\n");
	// loca 
	loca_length	= read_loca_table(ttf_table[2].data, head.index_to_loca_format, &loca_array, ttf_table[2].length);
	printf("loca_length = %x\n", loca_length);
	free(ttf_table[2].data);

	printf("hello\n");

	if(get_format_condition == 1){
		// initialize glyph array space
		*glyph_array		= (glyph_point_data*)malloc(sizeof(glyph_point_data)*(UNICODE_BMP_CHINA_END-UNICODE_BMP_CHINA_BEGIN+1));
		for(unicode=UNICODE_BMP_CHINA_BEGIN, i=0; unicode<=UNICODE_BMP_CHINA_END; unicode++, i++){
			// get data
			glyph_index 	= get_glyph_index(format4, format12, segment_count, unicode, get_format_condition);
			//printf("glyph_index  = %x\n",glyph_index);
			glyph_offset	= loca_array[glyph_index];
			//printf("glyph_offset = %x\n",glyph_offset);
			glyph_length	= loca_array[glyph_index+1] - loca_array[glyph_index];
			//printf("glyph_length = %x\n",glyph_length);
			// write data	
			if(unicode == 0x4fdd){
				printf("glyph_index  = %x\n",glyph_index);
				printf("glyph_offset = %x\n",glyph_offset);
				printf("glyph_length = %x\n",glyph_length);
			}

			get_glyph_data(ttf_table[3].data, glyph_offset, &(*glyph_array)[i].glyph_data, glyph_length);
			(*glyph_array)[i].unicode	= unicode;
			(*glyph_array)[i].glyph_length	= glyph_length;	
		}
	}
	free(loca_array);
	free(format4.end_code);
	free(format4.start_code);
	free(format4.id_delta);
	free(format4.id_range_offset);
	free(ttf_table[3].data);
}


void draw_word(glyph_point_data* glyph_array, u32 unicode, bmp_data bmp, int offset_x, int offset_y)
{
	int	i;
	int	target;
	point*	point_data;
	int 	point_length;

	for(i=0; i<UNICODE_BMP_NUMBER; i++){
		if(glyph_array[i].unicode == unicode){
			target	= i;
			break;
		}else if(i == UNICODE_BMP_NUMBER){
			printf("not find correct glyph_data\n");
			return;
		}
	}
	// get point data
	point_length	= glyph_to_point(glyph_array[target].glyph_data, &point_data, glyph_array[target].glyph_length);

	for(i=0; i<point_length; i++){
		printf("i %d   x %f    y %f    locate %d\n", i, point_data[i].x, point_data[i].y, point_data[i].locate);
	}


	// draw
	draw_word_from_point(bmp.width, bmp.color_data, bmp.color, point_data, point_length, offset_x, offset_y);
}

