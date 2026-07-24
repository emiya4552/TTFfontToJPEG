#ifndef BMP_READER_H
#define BMP_READER_H


#include<stdint.h>


typedef struct{
	uint8_t*	color_data;
	int		width;
	int		height;
}bmp_rgb_image;


// 功能：读取未压缩 24 位 BMP 并转换为从上到下排列的 RGB 缓冲区
int bmp_read_rgb(const char* file, bmp_rgb_image* image);

// 功能：释放 BMP RGB 图像缓冲区并清空结构体
void bmp_rgb_image_free(bmp_rgb_image* image);


#endif
