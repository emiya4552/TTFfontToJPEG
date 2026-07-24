#include"bmp_reader.h"

#include<limits.h>
#include<stdio.h>
#include<stdlib.h>


#define BMP_FILE_HEADER_SIZE	14
#define BMP_INFO_HEADER_MIN_SIZE	40
#define BMP_RGB_BIT_COUNT	24
#define BMP_RGB_CHANNEL_COUNT	3
#define BMP_COMPRESSION_RGB	0


// 功能：从 BMP 文件读取一个小端双字节无符号整数
static int read_u16_little(FILE* file, uint16_t* value)
{
	int	byte_0;
	int	byte_1;

	byte_0 = fgetc(file);
	byte_1 = fgetc(file);
	if(byte_0==EOF || byte_1==EOF || value==NULL){
		return -1;
	}
	*value = (uint16_t)(byte_0|(byte_1<<8));

	return 0;
}


// 功能：从 BMP 文件读取一个小端四字节无符号整数
static int read_u32_little(FILE* file, uint32_t* value)
{
	int	byte_0;
	int	byte_1;
	int	byte_2;
	int	byte_3;

	byte_0 = fgetc(file);
	byte_1 = fgetc(file);
	byte_2 = fgetc(file);
	byte_3 = fgetc(file);
	if(byte_0==EOF || byte_1==EOF || byte_2==EOF || byte_3==EOF || value==NULL){
		return -1;
	}
	*value = (uint32_t)byte_0|
		((uint32_t)byte_1<<8)|
		((uint32_t)byte_2<<16)|
		((uint32_t)byte_3<<24);

	return 0;
}


// 功能：从 BMP 文件读取一个小端四字节有符号整数
static int read_s32_little(FILE* file, int32_t* value)
{
	uint32_t	unsigned_value;

	if(value==NULL || read_u32_little(file, &unsigned_value)!=0){
		return -1;
	}
	*value = (int32_t)unsigned_value;

	return 0;
}


// 功能：释放 BMP RGB 图像缓冲区并清空结构体
void bmp_rgb_image_free(bmp_rgb_image* image)
{
	if(image == NULL){
		return;
	}

	free(image->color_data);
	image->color_data	= NULL;
	image->width		= 0;
	image->height		= 0;
}


// 功能：读取未压缩 24 位 BMP 并转换为从上到下排列的 RGB 缓冲区
int bmp_read_rgb(const char* file, bmp_rgb_image* image)
{
	FILE*		input;
	uint8_t*	row_data;
	uint8_t*	color_data;
	uint16_t	signature;
	uint16_t	reserved;
	uint16_t	planes;
	uint16_t	bit_count;
	uint32_t	ignored;
	uint32_t	pixel_offset;
	uint32_t	info_header_size;
	uint32_t	compression;
	int32_t		width_value;
	int32_t		height_value;
	size_t		row_color_length;
	size_t		row_file_length;
	size_t		color_length;
	size_t		source_position;
	size_t		target_position;
	int		image_height;
	int		file_y;
	int		target_y;
	int		x;
	int		result;

	if(image == NULL){
		return -1;
	}
	image->color_data	= NULL;
	image->width		= 0;
	image->height		= 0;
	if(file==NULL || file[0]=='\0'){
		return -1;
	}

	input = fopen(file, "rb");
	if(input == NULL){
		printf("[BMP-READER][ERROR] input open failed: file=%s\n", file);
		return -1;
	}
	row_data = NULL;
	color_data = NULL;
	result = -1;

	// 读取 BMP 文件头及 DIB 头中与像素解析相关的字段
	if(read_u16_little(input, &signature)!=0 || signature!=0x4d42 ||
	   read_u32_little(input, &ignored)!=0 ||
	   read_u16_little(input, &reserved)!=0 ||
	   read_u16_little(input, &reserved)!=0 ||
	   read_u32_little(input, &pixel_offset)!=0 ||
	   read_u32_little(input, &info_header_size)!=0 ||
	   read_s32_little(input, &width_value)!=0 ||
	   read_s32_little(input, &height_value)!=0 ||
	   read_u16_little(input, &planes)!=0 ||
	   read_u16_little(input, &bit_count)!=0 ||
	   read_u32_little(input, &compression)!=0){
		printf("[BMP-READER][ERROR] invalid BMP header: file=%s\n", file);
		goto cleanup;
	}

	// 当前转换入口只接受带 BITMAPINFOHEADER 的未压缩 24 位 RGB BMP
	if(info_header_size<BMP_INFO_HEADER_MIN_SIZE ||
	   (uint64_t)BMP_FILE_HEADER_SIZE+info_header_size>pixel_offset ||
	   pixel_offset>(uint32_t)LONG_MAX || width_value<=0 || height_value==0 ||
	   height_value==INT32_MIN || planes!=1 || bit_count!=BMP_RGB_BIT_COUNT ||
	   compression!=BMP_COMPRESSION_RGB || width_value>INT_MAX){
		printf("[BMP-READER][ERROR] unsupported BMP: file=%s bit_count=%u compression=%lu\n",
			file, (unsigned int)bit_count, (unsigned long)compression);
		goto cleanup;
	}
	image_height = (height_value<0)?-(int)height_value:(int)height_value;

	// 检查 RGB 缓冲区和带四字节补齐的 BMP 行长度是否溢出
	if((size_t)width_value>((size_t)-1-3)/BMP_RGB_CHANNEL_COUNT){
		goto cleanup;
	}
	row_color_length = (size_t)width_value*BMP_RGB_CHANNEL_COUNT;
	row_file_length = (row_color_length+3)&~(size_t)3;
	if((size_t)image_height>(size_t)-1/row_color_length){
		goto cleanup;
	}
	color_length = row_color_length*image_height;
	row_data = (uint8_t*)malloc(row_file_length);
	color_data = (uint8_t*)malloc(color_length);
	if(row_data==NULL || color_data==NULL ||
	   fseek(input, (long)pixel_offset, SEEK_SET)!=0){
		printf("[BMP-READER][ERROR] pixel buffer initialization failed: file=%s\n", file);
		goto cleanup;
	}

	// BMP 正高度按从下到上存储，负高度按从上到下存储
	for(file_y=0; file_y<image_height; file_y++){
		if(fread(row_data, 1, row_file_length, input) != row_file_length){
			printf("[BMP-READER][ERROR] pixel data truncated: file=%s row=%d\n",
				file, file_y);
			goto cleanup;
		}
		target_y = (height_value>0)?(image_height-1-file_y):file_y;
		for(x=0; x<width_value; x++){
			source_position = (size_t)x*BMP_RGB_CHANNEL_COUNT;
			target_position = ((size_t)target_y*width_value+x)*BMP_RGB_CHANNEL_COUNT;
			color_data[target_position] = row_data[source_position+2];
			color_data[target_position+1] = row_data[source_position+1];
			color_data[target_position+2] = row_data[source_position];
		}
	}

	image->color_data	= color_data;
	image->width		= (int)width_value;
	image->height		= image_height;
	color_data = NULL;
	result = 0;
	printf("[BMP-READER][INFO] loaded: file=%s width=%d height=%d\n",
		file, image->width, image->height);

cleanup:
	free(row_data);
	free(color_data);
	fclose(input);
	return result;
}
