#include"bmp.h"

// 功能：将 RGB 像素缓冲区写入 24 位 BMP 文件
int bmp_generate(char* file, unsigned char* color_data, int width, int height)
{
	FILE*		fp;
	int		image_size;
	unsigned char*	bmp_data;
	int		i;
	int		j;
	int		k;
	int		pixel_count;
	int		bit_count;
	int		supplement;
	int		header_length;

	fp	= fopen(file,"wb+");
	if(fp == NULL){
		printf("open file error\n");
		return -1;
	}

	if(width <= 0){
		printf("width error\n");
		return -2;
	}

	header_length	= 64;

	// 计算每行像素数据的四字节对齐补位
	supplement	= 0;
	while((width * PER_PIXEL_BYTE + supplement) % 4 != 0){
		supplement++;
	}

	// 计算完整像素区大小并申请输出缓冲区
	image_size	= width * fabs(height) * PER_PIXEL_BYTE + supplement * fabs(height);
	bmp_data	= (unsigned char*)calloc(1,image_size);

	// 初始化 BMP 文件头和信息头
	BMP_FILE_HEADER file_header;
	BMP_INFO_HEADER info_header;

	file_header.bfType[0]		= 'B';
	file_header.bfType[1]		= 'M';
	file_header.bfSize		= image_size + header_length;
	file_header.bfReserved1		= 0;
	file_header.bfReserved2		= 0;
	file_header.bf0ffBits		= 64;

	info_header.biSize		= 40;
	info_header.biWidth		= width;
	info_header.biHeight		= height;
	info_header.biPlane		= 1;
	info_header.biBitCount		= 24;
	info_header.biCompression	= 0;
	info_header.biSizeImage		= image_size;
	info_header.biXPelsPerMeter	= 0;
	info_header.biYPelsPerMeter	= 0;
	info_header.biClrUsed		= 0;
	info_header.biClrImportant	= 0;

	// 写入 BMP 文件头和信息头
	fwrite(&file_header, sizeof(BMP_FILE_HEADER), 1, fp);
	fwrite(&info_header, sizeof(BMP_INFO_HEADER), 1, fp);

	// 根据 BMP 高度方向转换行顺序和颜色通道
	if(height < 0){
		for(i=0, j=0, pixel_count=0, bit_count=PER_PIXEL_BYTE-1; i<image_size && pixel_count<image_size;){
			bmp_data[i] = color_data[pixel_count + bit_count];
			i++;
			bit_count--;
			if(bit_count < 0){
				bit_count = PER_PIXEL_BYTE - 1;
				pixel_count += PER_PIXEL_BYTE;
			}
		}
	}else{
		for(i=0, j=0, k=height-1, pixel_count=k*width*3, bit_count=PER_PIXEL_BYTE-1; i<image_size;){
			bmp_data[i++]	= color_data[pixel_count + bit_count];
			if(--bit_count < 0){
				bit_count	= PER_PIXEL_BYTE - 1;
			}
			if(bit_count == PER_PIXEL_BYTE - 1){
				pixel_count += PER_PIXEL_BYTE;
			}
			if(++j == width * PER_PIXEL_BYTE){
				j		= 0;
				i += supplement;
				k--;
				pixel_count	= k * width * 3;
			}
		}
	}

	fseek(fp, header_length, SEEK_SET);

	fwrite(bmp_data, 1, image_size, fp);

	fclose(fp);

	return 0;
}

