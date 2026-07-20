#ifndef BMP_H

#define BMP_H

#include<stdio.h>
#include<math.h>
#include<stdlib.h>
#include<string.h>

#pragma pack(1)

#define PER_PIXEL_BYTE 		3


typedef struct{
	unsigned char		bfType[2];
	unsigned int		bfSize;
	unsigned short int	bfReserved1;
	unsigned short int	bfReserved2;
	unsigned int		bf0ffBits;
}BMP_FILE_HEADER;

typedef struct{
	unsigned int		biSize;
	int			biWidth;
	int			biHeight;
	unsigned short int	biPlane;
	unsigned short int	biBitCount;
	unsigned int		biCompression;
	unsigned int		biSizeImage;
	int			biXPelsPerMeter;
	int			biYPelsPerMeter;
	unsigned int		biClrUsed;
	unsigned int		biClrImportant;
}BMP_INFO_HEADER;


int bmp_generate(char* file, unsigned char* color_data, int width, int height);

#endif
