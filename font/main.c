#include"ttf.h"
#include"ttf_preload.h"
#include"font_draw.h"
#include"bmp.h"


u8	color_data[2000][2000][3];

// 功能：加载字体并将命令行字符串渲染为实心 BMP
int main(int argc, char* argv[])
{
	char 	file[] 		= "simhei.ttf";
	char	bmp_name[]	= "out.bmp";
	unicode_range		range_array[5];
	ttf_font*		ttf;
	ttf_preload		preload;
	font_glyph_source	source;
	u8			color[3];
	bmp_data		bmp;
	int 		center_x;
	int 		center_y;
	int		target_height;

	// 检查字符串、中心点和目标字高参数
	if(argc != 5){
		printf("usage: %s <string> <center_x> <center_y> <target_height>\n", argv[0]);
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
	target_height	= atoi(argv[4]);
	if(target_height <= 0){
		printf("target height error\n");
		return -1;
	}

	// 配置英文、通用标点、中文标点、常用汉字和全角字符范围
	range_array[0].begin	= 0x0020;
	range_array[0].end	= 0x007E;
	range_array[1].begin	= 0x2000;
	range_array[1].end	= 0x206F;
	range_array[2].begin	= 0x3000;
	range_array[2].end	= 0x303F;
	range_array[3].begin	= 0x4E00;
	range_array[3].end	= 0x9FA4;
	range_array[4].begin	= 0xFF00;
	range_array[4].end	= 0xFFEF;

	// 打开一次字体解析上下文并批量预加载全部配置范围
	ttf = NULL;
	if(ttf_font_open(file, &ttf) != 0 ||
	   ttf_preload_ranges(ttf, range_array, 5, &preload) != 0){
		printf("load font error\n");
		ttf_font_close(ttf);
		return -1;
	}
	ttf_font_close(ttf);
	if(ttf_preload_get_source(&preload, &source) != 0){
		printf("font source error\n");
		ttf_preload_free(&preload);
		return -1;
	}

	// 从指定中心点按目标字框高度绘制实心字符串
	draw_source_scaled_filled_string(&source, argv[1], WORD_ENCODING_SYSTEM, bmp,
		center_x, center_y, target_height);

 	bmp_generate(bmp.name, (u8*) color_data, bmp.width, bmp.height);
	ttf_preload_free(&preload);

	return 0;
}
