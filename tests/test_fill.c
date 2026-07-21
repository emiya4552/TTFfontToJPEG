#include "../font/ttf.h"

// 功能：验证实心填充、内部孔洞和轮廓外区域
int main(void)
{
	u8	color_data[10*10*3] = {0};
	u8	color[3] = {0x11, 0x22, 0x33};
	point	point_data[8] = {
		{1.0f, 1.0f, 1},
		{8.0f, 1.0f, 1},
		{8.0f, 8.0f, 1},
		{1.0f, 8.0f, NEXT},
		{3.0f, 3.0f, 1},
		{3.0f, 6.0f, 1},
		{6.0f, 6.0f, 1},
		{6.0f, 3.0f, NEXT}
	};
	int	interior;
	int	hole;
	int	outside;

	// 绘制一个包含反向内轮廓的方框
	draw_filled_word_from_point(10, 10, color_data, color, point_data, 8, 0, 0);

	interior	= ((10-1-2)*10+2)*3;
	hole		= ((10-1-4)*10+4)*3;
	outside		= ((10-1-0)*10+0)*3;

	// 分别检查实心区域、孔洞区域和轮廓外区域
	if(color_data[interior] != color[0] ||
	   color_data[interior+1] != color[1] ||
	   color_data[interior+2] != color[2]){
		fprintf(stderr, "filled interior error\n");
		return 1;
	}
	if(color_data[hole] != 0 || color_data[hole+1] != 0 || color_data[hole+2] != 0){
		fprintf(stderr, "filled hole error\n");
		return 2;
	}
	if(color_data[outside] != 0 || color_data[outside+1] != 0 || color_data[outside+2] != 0){
		fprintf(stderr, "filled outside error\n");
		return 3;
	}

	return 0;
}
