#ifndef JPG_H
#define JPG_H


#include<stdint.h>


typedef uint8_t	u8;


#define JPEG_MIN_QUALITY	1
#define JPEG_MAX_QUALITY	100


// 功能：将 RGB 像素缓冲区编码为 4:2:0 Baseline JPEG 文件
int jpeg_generate(const char* file, const u8* color_data, int width, int height, int quality);


#endif
