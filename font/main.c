#include"ttf.h"
#include"bmp.h"


u8	color_data[2000][2000][3];



/* 旧版单字测试入口，保留用于历史参考
int main()
{
	int 			i;
	wchar_t			c;
	u32			unicode;

	table*			ttf_table;
	// head 表数据
	head_table		head;
	// cmap 表数据
	u16			segment_count;
	cmap_format4_data 	format4;
	cmap_format12_data	format12;
	int			get_format_condition;
	int 			glyph_index;
	// loca 表数据
	u32*			loca_array;
	u32			glyph_offset;
	int			loca_length;
	int			glyph_length;

	// glyf 表数据
	u8*			glyph_data;
	point*			point_data;
	int			point_length;

	// BMP 输出参数
	int			width;
	int			height;
	u8			color[3];
	int			offset_x;
	int 			offset_y;
	char			file[] = "xxfang.bmp";


	c		= L'水';
	unicode		= (u32)c;
	width		= 2000;
	height		= 2000;
	offset_x	= 1000;
	offset_y	= 1000;

	color[0]	= 0xff;
	color[1]	= 0x88;
	color[2]	= 0x00;

	read_ttf_data("simfang.ttf", &ttf_table);
	printf("unicode = %x\n", unicode);

	// 解析 head 表
	read_head_table(ttf_table[0].data, &head);
	printf("%d %x\n", head.index_to_loca_format, head.magic_number);
	// 释放 head 表原始数据
	free(ttf_table[0].data);

	// 解析 cmap 表
	get_format_condition	= read_cmap(ttf_table[1].data, &format4, &format12, &segment_count);
	glyph_index 		= get_glyph_index(format4, format12, segment_count, unicode, get_format_condition);
	printf("\nget_format_condition %d   glyph_index %d\n", get_format_condition, glyph_index);
	
	// 解析 loca 表
	loca_length	= read_loca_table(ttf_table[2].data, head.index_to_loca_format, &loca_array, ttf_table[2].length);
	free(ttf_table[2].data);
	// 计算字形偏移与长度
	glyph_offset	= loca_array[glyph_index];
	glyph_length	= loca_array[glyph_index+1] - loca_array[glyph_index];
	
	// 输出 loca 调试信息
	printf("\n loca array\n");
	for(i=0; i<1000; i++){
		printf("%x  ", loca_array[i]);
	}
	printf("\nglyph offset = %d\n", glyph_offset);

	get_glyph_data(ttf_table[3].data, glyph_offset, &glyph_data, glyph_length);

	// 输出 glyf 调试信息
	printf("\nglyph_data\n");
	printf("glyph_length   %d\n", glyph_length);
	for(i=0; i<glyph_length; i++){
		printf("%x  ", glyph_data[i]);
	}
	
	point_length	= glyph_to_point(glyph_data, &point_data, glyph_length);

	draw_word_from_point(width, height, (u8*)color_data, color, point_data, point_length, offset_x, offset_y);
 	bmp_generate(file, (u8*) color_data, width, height);
	return 0;
}*/




// 功能：加载字体并将命令行字符串渲染为实心 BMP
int main(int argc, char* argv[])
{
	char 	file[] 		= "simhei.ttf";
	char	bmp_name[]	= "out.bmp";
	glyph_point_data*	glyph_array;
	font_box		box;
	u8			color[3];
	bmp_data		bmp;
	int 		center_x;
	int 		center_y;

	// 检查字符串和中心点参数
	if(argc != 4){
		printf("usage: %s <string> <center_x> <center_y>\n", argv[0]);
		return -1;
	}


	color[0]		= 0xff;
	color[1]		= 0;
	color[2]		= 0x77;

	bmp.name	= bmp_name;
	bmp.color_data	= (u8*)color_data;
	bmp.color	= color;
	bmp.width	= 2000;
	bmp.height	= 2000;

	center_x	= atoi(argv[2]);
	center_y	= atoi(argv[3]);




	// 读取字模数据
	load_ttf_BMP(file, &glyph_array, &box);

	// 从指定中心点绘制实心字符串
	draw_filled_string(glyph_array, box, argv[1], WORD_ENCODING_SYSTEM, bmp, center_x, center_y);

 	bmp_generate(bmp.name, (u8*) color_data, bmp.width, bmp.height);

	return 0;
}
