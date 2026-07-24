#include"font/font_renderer.h"
#include"font/bmp.h"
#include"jpeg/jpg.h"

#include<errno.h>
#include<limits.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


#define MAIN_DEFAULT_FONT_FILE		"font/simhei.ttf"
#define MAIN_DEFAULT_BMP_OUTPUT		"out.bmp"
#define MAIN_DEFAULT_JPEG_OUTPUT	"out.jpg"
#define MAIN_DEFAULT_JPEG_QUALITY	90


typedef enum{
	MAIN_OUTPUT_BMP,
	MAIN_OUTPUT_JPEG,
	MAIN_OUTPUT_BOTH
}main_output_format;


typedef struct{
	font_render_options	render_options;
	const char*		bmp_output;
	const char*		jpeg_output;
	int			jpeg_quality;
	main_output_format	output_format;
}main_options;


// 功能：初始化根目录程序的字体渲染与图像输出缺省配置
static void init_main_options(main_options* options)
{
	if(options == NULL){
		return;
	}

	init_font_render_options(&(options->render_options));
	options->render_options.font_file	= MAIN_DEFAULT_FONT_FILE;
	options->render_options.output_file	= MAIN_DEFAULT_BMP_OUTPUT;
	options->bmp_output			= MAIN_DEFAULT_BMP_OUTPUT;
	options->jpeg_output			= MAIN_DEFAULT_JPEG_OUTPUT;
	options->jpeg_quality			= MAIN_DEFAULT_JPEG_QUALITY;
	options->output_format			= MAIN_OUTPUT_BOTH;
}


// 功能：输出根目录程序的命令行参数说明
static void print_usage(const char* program)
{
	printf("[MAIN][INFO] usage: %s <text> [options]\n", program);
	printf("[MAIN][INFO] options:\n");
	printf("  --font <file>              TTF file, default: %s\n",
		MAIN_DEFAULT_FONT_FILE);
	printf("  --format <bmp|jpeg|both>   output format, default: both\n");
	printf("  --bmp-output <file>        BMP output, default: %s\n",
		MAIN_DEFAULT_BMP_OUTPUT);
	printf("  --jpeg-output <file>       JPEG output, default: %s\n",
		MAIN_DEFAULT_JPEG_OUTPUT);
	printf("  --quality <number>         JPEG quality 1-100, default: %d\n",
		MAIN_DEFAULT_JPEG_QUALITY);
	printf("  --font-height <number>     target font height, default: %d\n",
		FONT_RENDER_DEFAULT_TARGET_HEIGHT);
	printf("  --canvas-width <number>    canvas width, default: %d\n",
		FONT_RENDER_DEFAULT_CANVAS_WIDTH);
	printf("  --canvas-height <number>   canvas height, default: %d\n",
		FONT_RENDER_DEFAULT_CANVAS_HEIGHT);
	printf("  --center-x <number>        horizontal center, default: canvas center\n");
	printf("  --center-y <number>        vertical center, default: canvas center\n");
	printf("  --strategy <cache|preload> glyph loading strategy, default: cache\n");
	printf("  --cache-capacity <number>  cache capacity, default: %d\n",
		FONT_RENDER_DEFAULT_CACHE_CAPACITY);
	printf("  --help                     show this help\n");
}


// 功能：将完整的十进制参数转换为 int
static int parse_integer(const char* string, int* result)
{
	char*	end;
	long	value;

	if(string == NULL || string[0] == '\0' || result == NULL){
		return -1;
	}
	errno = 0;
	value = strtol(string, &end, 10);
	if(errno == ERANGE || *end != '\0' || value<INT_MIN || value>INT_MAX){
		return -1;
	}

	*result = (int)value;
	return 0;
}


// 功能：读取需要携带值的具名参数
static int get_option_value(int argc, char* argv[], int* position, const char** value)
{
	if(argv==NULL || position==NULL || value==NULL){
		return -1;
	}
	*value = NULL;
	if(*position+1 >= argc){
		printf("[MAIN][ERROR] option value missing: option=%s\n", argv[*position]);
		return -1;
	}

	(*position)++;
	*value = argv[*position];
	return 0;
}


// 功能：解析 BMP、JPEG 与字形加载策略的字符串参数
static int parse_text_option(const char* option, const char* value, main_options* options)
{
	if(strcmp(option, "--font") == 0){
		options->render_options.font_file = value;
	}else if(strcmp(option, "--bmp-output") == 0){
		options->bmp_output = value;
		options->render_options.output_file = value;
	}else if(strcmp(option, "--jpeg-output") == 0){
		options->jpeg_output = value;
	}else if(strcmp(option, "--strategy") == 0){
		if(strcmp(value, "cache") == 0){
			options->render_options.load_strategy = FONT_LOAD_CACHE;
		}else if(strcmp(value, "preload") == 0){
			options->render_options.load_strategy = FONT_LOAD_PRELOAD;
		}else{
			printf("[MAIN][ERROR] invalid strategy: value=%s\n", value);
			return -1;
		}
	}else if(strcmp(option, "--format") == 0){
		if(strcmp(value, "bmp") == 0){
			options->output_format = MAIN_OUTPUT_BMP;
		}else if(strcmp(value, "jpeg") == 0){
			options->output_format = MAIN_OUTPUT_JPEG;
		}else if(strcmp(value, "both") == 0){
			options->output_format = MAIN_OUTPUT_BOTH;
		}else{
			printf("[MAIN][ERROR] invalid output format: value=%s\n", value);
			return -1;
		}
	}else{
		return -1;
	}

	return 0;
}


// 功能：把已校验的整数参数写入对应程序配置
static int set_integer_option(const char* option, int number, main_options* options)
{
	// 中心点允许位于画布之外，其他尺寸、容量和质量必须为正数
	if(strcmp(option, "--center-x") == 0){
		options->render_options.center_x = number;
	}else if(strcmp(option, "--center-y") == 0){
		options->render_options.center_y = number;
	}else if(number <= 0){
		printf("[MAIN][ERROR] option must be positive: option=%s value=%d\n",
			option, number);
		return -1;
	}else if(strcmp(option, "--font-height") == 0){
		options->render_options.target_height = number;
	}else if(strcmp(option, "--canvas-width") == 0){
		options->render_options.canvas_width = number;
	}else if(strcmp(option, "--canvas-height") == 0){
		options->render_options.canvas_height = number;
	}else if(strcmp(option, "--cache-capacity") == 0){
		options->render_options.cache_capacity = number;
	}else if(strcmp(option, "--quality") == 0){
		if(number>JPEG_MAX_QUALITY){
			printf("[MAIN][ERROR] JPEG quality out of range: value=%d\n", number);
			return -1;
		}
		options->jpeg_quality = number;
	}else{
		return -1;
	}

	return 0;
}


// 功能：解析必填文本以及可选的具名输出和渲染参数
static int parse_arguments(int argc, char* argv[], const char** text, main_options* options)
{
	const char*	option;
	const char*	value;
	int		position;
	int		number;

	if(argc<2 || text==NULL || options==NULL){
		return -1;
	}
	if(strcmp(argv[1], "--help")==0 || strcmp(argv[1], "-h")==0){
		return 1;
	}
	if(argv[1][0] == '\0'){
		printf("[MAIN][ERROR] text cannot be empty\n");
		return -1;
	}
	*text = argv[1];

	// 逐项解析具名参数，未出现的配置继续使用缺省值
	for(position=2; position<argc; position++){
		option = argv[position];
		if(strcmp(option, "--help") == 0){
			return 1;
		}
		if(get_option_value(argc, argv, &position, &value) != 0){
			return -1;
		}

		// 字符串参数与整数参数分别交给独立函数处理
		if(strcmp(option, "--font")==0 || strcmp(option, "--bmp-output")==0 ||
		   strcmp(option, "--jpeg-output")==0 || strcmp(option, "--strategy")==0 ||
		   strcmp(option, "--format")==0){
			if(parse_text_option(option, value, options) != 0){
				return -1;
			}
		}else if(strcmp(option, "--font-height")==0 ||
			 strcmp(option, "--canvas-width")==0 ||
			 strcmp(option, "--canvas-height")==0 ||
			 strcmp(option, "--center-x")==0 ||
			 strcmp(option, "--center-y")==0 ||
			 strcmp(option, "--cache-capacity")==0 ||
			 strcmp(option, "--quality")==0){
			if(parse_integer(value, &number) != 0){
				printf("[MAIN][ERROR] invalid integer option: option=%s value=%s\n",
					option, value);
				return -1;
			}
			if(set_integer_option(option, number, options) != 0){
				return -1;
			}
		}else{
			printf("[MAIN][ERROR] unknown option: option=%s\n", option);
			return -1;
		}
	}

	return 0;
}


// 功能：返回便于日志输出的图像格式名称
static const char* get_output_format_name(main_output_format format)
{
	if(format == MAIN_OUTPUT_BMP){
		return "bmp";
	}
	if(format == MAIN_OUTPUT_JPEG){
		return "jpeg";
	}

	return "both";
}


// 功能：根据配置将 RGB 画布写入 BMP、JPEG 或两种文件
static int generate_output_files(const main_options* options, const u8* color_data)
{
	int	result;

	result = 0;
	if(options->output_format==MAIN_OUTPUT_BMP ||
	   options->output_format==MAIN_OUTPUT_BOTH){
		if(bmp_generate((char*)options->bmp_output, (u8*)color_data,
		   options->render_options.canvas_width,
		   options->render_options.canvas_height) != 0){
			result = -1;
		}
	}
	if(options->output_format==MAIN_OUTPUT_JPEG ||
	   options->output_format==MAIN_OUTPUT_BOTH){
		if(jpeg_generate(options->jpeg_output, color_data,
		   options->render_options.canvas_width,
		   options->render_options.canvas_height,
		   options->jpeg_quality) != 0){
			result = -1;
		}
	}

	return result;
}


// 功能：解析命令行并完成 TTF 渲染、BMP 生成和 JPEG 编码
int main(int argc, char* argv[])
{
	main_options	options;
	font_renderer*	renderer;
	u8*		color_data;
	const char*	text;
	const char*	strategy;
	int		center_x;
	int		center_y;
	int		result;

	init_main_options(&options);
	result = parse_arguments(argc, argv, &text, &options);
	if(result != 0){
		print_usage(argv[0]);
		return (result>0)?0:-1;
	}

	// JPEG 帧头只能保存 16 位宽高，编码前直接拒绝越界画布
	if(options.output_format!=MAIN_OUTPUT_BMP &&
	   (options.render_options.canvas_width>65535 ||
	    options.render_options.canvas_height>65535)){
		printf("[MAIN][ERROR] JPEG canvas exceeds 65535 pixels\n");
		return -1;
	}

	center_x = (options.render_options.center_x==FONT_RENDER_AUTO_POSITION)?
		options.render_options.canvas_width/2:options.render_options.center_x;
	center_y = (options.render_options.center_y==FONT_RENDER_AUTO_POSITION)?
		options.render_options.canvas_height/2:options.render_options.center_y;
	strategy = (options.render_options.load_strategy==FONT_LOAD_CACHE)?"cache":"preload";
	printf("[MAIN][INFO] render request: text=\"%s\" strategy=%s canvas=(%d,%d) "
		"center=(%d,%d) target_height=%d format=%s quality=%d font=%s\n",
		text, strategy, options.render_options.canvas_width,
		options.render_options.canvas_height, center_x, center_y,
		options.render_options.target_height,
		get_output_format_name(options.output_format), options.jpeg_quality,
		options.render_options.font_file);

	renderer = NULL;
	color_data = NULL;
	if(font_renderer_open(&(options.render_options), &renderer) != 0 ||
	   font_renderer_render_rgb(renderer, text, &color_data) != 0 ||
	   generate_output_files(&options, color_data) != 0){
		printf("[MAIN][ERROR] render or output generation failed\n");
		free(color_data);
		font_renderer_close(renderer);
		return -1;
	}
	free(color_data);
	font_renderer_close(renderer);
	if(options.output_format == MAIN_OUTPUT_BMP){
		printf("[MAIN][INFO] completed: format=bmp output=%s\n",
			options.bmp_output);
	}else if(options.output_format == MAIN_OUTPUT_JPEG){
		printf("[MAIN][INFO] completed: format=jpeg output=%s\n",
			options.jpeg_output);
	}else{
		printf("[MAIN][INFO] completed: format=both bmp=%s jpeg=%s\n",
			options.bmp_output, options.jpeg_output);
	}

	return 0;
}
