#include"bmp_reader.h"
#include"jpg.h"

#include<errno.h>
#include<limits.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


#define JPEG_MAIN_DEFAULT_OUTPUT	"out.jpg"
#define JPEG_MAIN_DEFAULT_QUALITY	90


// 功能：输出 BMP 转 JPEG 命令行入口的参数说明
static void print_usage(const char* program)
{
	printf("[JPEG-MAIN][INFO] usage: %s <input.bmp> [output.jpg] [quality]\n",
		program);
	printf("[JPEG-MAIN][INFO] default output: %s\n", JPEG_MAIN_DEFAULT_OUTPUT);
	printf("[JPEG-MAIN][INFO] quality range: %d-%d, default: %d\n",
		JPEG_MIN_QUALITY, JPEG_MAX_QUALITY, JPEG_MAIN_DEFAULT_QUALITY);
}


// 功能：将完整的十进制质量参数转换为 int
static int parse_quality(const char* string, int* quality)
{
	char*	end;
	long	value;

	if(string==NULL || string[0]=='\0' || quality==NULL){
		return -1;
	}
	errno = 0;
	value = strtol(string, &end, 10);
	if(errno==ERANGE || *end!='\0' || value<JPEG_MIN_QUALITY ||
	   value>JPEG_MAX_QUALITY || value>INT_MAX){
		return -1;
	}
	*quality = (int)value;

	return 0;
}


// 功能：读取 24 位 BMP 并将其编码为 4:2:0 Baseline JPEG
int main(int argc, char* argv[])
{
	bmp_rgb_image	image;
	const char*	input_file;
	const char*	output_file;
	int		quality;
	int		result;

	if(argc==2 && (strcmp(argv[1], "--help")==0 || strcmp(argv[1], "-h")==0)){
		print_usage(argv[0]);
		return 0;
	}
	if(argc<2 || argc>4){
		print_usage(argv[0]);
		return -1;
	}
	input_file = argv[1];
	output_file = (argc>=3)?argv[2]:JPEG_MAIN_DEFAULT_OUTPUT;
	quality = JPEG_MAIN_DEFAULT_QUALITY;
	if(argc==4 && parse_quality(argv[3], &quality)!=0){
		printf("[JPEG-MAIN][ERROR] invalid quality: value=%s\n", argv[3]);
		return -1;
	}

	printf("[JPEG-MAIN][INFO] convert request: input=%s output=%s quality=%d\n",
		input_file, output_file, quality);
	if(bmp_read_rgb(input_file, &image) != 0){
		printf("[JPEG-MAIN][ERROR] BMP read failed: file=%s\n", input_file);
		return -1;
	}
	result = jpeg_generate(output_file, image.color_data, image.width,
		image.height, quality);
	bmp_rgb_image_free(&image);
	if(result != 0){
		printf("[JPEG-MAIN][ERROR] JPEG generation failed: file=%s\n", output_file);
		return -1;
	}

	printf("[JPEG-MAIN][INFO] conversion completed: output=%s\n", output_file);
	return 0;
}
