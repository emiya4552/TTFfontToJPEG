#include"font_renderer.h"

#include<errno.h>
#include<limits.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


// 功能：输出命令行参数说明
static void print_usage(const char* program)
{
	printf("[MAIN][INFO] usage: %s <text> [options]\n", program);
	printf("[MAIN][INFO] options:\n");
	printf("  --font <file>             TTF file, default: %s\n", FONT_RENDER_DEFAULT_FILE);
	printf("  --output <file>           BMP output, default: %s\n", FONT_RENDER_DEFAULT_OUTPUT);
	printf("  --font-height <number>    target font height, default: %d\n",
		FONT_RENDER_DEFAULT_TARGET_HEIGHT);
	printf("  --canvas-width <number>   canvas width, default: %d\n",
		FONT_RENDER_DEFAULT_CANVAS_WIDTH);
	printf("  --canvas-height <number>  canvas height, default: %d\n",
		FONT_RENDER_DEFAULT_CANVAS_HEIGHT);
	printf("  --center-x <number>       horizontal center, default: canvas center\n");
	printf("  --center-y <number>       vertical center, default: canvas center\n");
	printf("  --strategy <cache|preload> glyph loading strategy, default: cache\n");
	printf("  --cache-capacity <number> cache capacity, default: %d\n",
		FONT_RENDER_DEFAULT_CACHE_CAPACITY);
	printf("  --help                    show this help\n");
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

// 功能：解析必填文本以及可选的具名渲染参数
static int parse_arguments(int argc, char* argv[], const char** text, font_render_options* options)
{
	const char*	value;
	int		position;
	int		number;

	if(argc<2 || text==NULL || options==NULL){
		return -1;
	}
	value = NULL;
	if(strcmp(argv[1], "--help")==0 || strcmp(argv[1], "-h")==0){
		return 1;
	}
	if(argv[1][0] == '\0'){
		printf("[MAIN][ERROR] text cannot be empty\n");
		return -1;
	}
	*text = argv[1];

	// 新命令使用具名参数，未出现的配置保持默认值
	for(position=2; position<argc; position++){
		if(strcmp(argv[position], "--help") == 0){
			return 1;
		}else if(strcmp(argv[position], "--font") == 0){
			if(get_option_value(argc, argv, &position, &value) != 0){
				return -1;
			}
			options->font_file = value;
		}else if(strcmp(argv[position], "--output") == 0){
			if(get_option_value(argc, argv, &position, &value) != 0){
				return -1;
			}
			options->output_file = value;
		}else if(strcmp(argv[position], "--strategy") == 0){
			if(get_option_value(argc, argv, &position, &value) != 0){
				return -1;
			}
			if(strcmp(value, "cache") == 0){
				options->load_strategy = FONT_LOAD_CACHE;
			}else if(strcmp(value, "preload") == 0){
				options->load_strategy = FONT_LOAD_PRELOAD;
			}else{
				printf("[MAIN][ERROR] invalid strategy: value=%s\n", value);
				return -1;
			}
		}else if(strcmp(argv[position], "--font-height") == 0 ||
			 strcmp(argv[position], "--canvas-width") == 0 ||
			 strcmp(argv[position], "--canvas-height") == 0 ||
			 strcmp(argv[position], "--center-x") == 0 ||
			 strcmp(argv[position], "--center-y") == 0 ||
			 strcmp(argv[position], "--cache-capacity") == 0){
			if(get_option_value(argc, argv, &position, &value) != 0 ||
			   parse_integer(value, &number) != 0){
				printf("[MAIN][ERROR] invalid integer option: value=%s\n",
					(value==NULL)?"":value);
				return -1;
			}

			// 中心点允许画布外坐标，其余尺寸和容量必须为正数
			if(strcmp(argv[position-1], "--center-x") == 0){
				options->center_x = number;
			}else if(strcmp(argv[position-1], "--center-y") == 0){
				options->center_y = number;
			}else if(number <= 0){
				printf("[MAIN][ERROR] option must be positive: option=%s value=%d\n",
					argv[position-1], number);
				return -1;
			}else if(strcmp(argv[position-1], "--font-height") == 0){
				options->target_height = number;
			}else if(strcmp(argv[position-1], "--canvas-width") == 0){
				options->canvas_width = number;
			}else if(strcmp(argv[position-1], "--canvas-height") == 0){
				options->canvas_height = number;
			}else{
				options->cache_capacity = number;
			}
		}else{
			printf("[MAIN][ERROR] unknown option: option=%s\n", argv[position]);
			return -1;
		}
	}

	return 0;
}


// 功能：解析命令行配置并将字符串渲染为实心 BMP
int main(int argc, char* argv[])
{
	font_render_options	options;
	font_renderer*		renderer;
	const char*		text;
	const char*		strategy;
	int			center_x;
	int			center_y;
	int			result;

	init_font_render_options(&options);
	result = parse_arguments(argc, argv, &text, &options);
	if(result != 0){
		print_usage(argv[0]);
		return (result>0)?0:-1;
	}

	center_x = (options.center_x==FONT_RENDER_AUTO_POSITION)?
		options.canvas_width/2:options.center_x;
	center_y = (options.center_y==FONT_RENDER_AUTO_POSITION)?
		options.canvas_height/2:options.center_y;
	strategy = (options.load_strategy==FONT_LOAD_CACHE)?"cache":"preload";
	printf("[MAIN][INFO] render request: text=\"%s\" strategy=%s canvas=(%d,%d) "
		"center=(%d,%d) target_height=%d font=%s output=%s\n",
		text, strategy, options.canvas_width, options.canvas_height,
		center_x, center_y, options.target_height, options.font_file,
		options.output_file);

	renderer = NULL;
	if(font_renderer_open(&options, &renderer) != 0 ||
	   font_renderer_render(renderer, text) != 0){
		printf("[MAIN][ERROR] render failed\n");
		font_renderer_close(renderer);
		return -1;
	}
	font_renderer_close(renderer);
	printf("[MAIN][INFO] render completed: output=%s\n", options.output_file);

	return 0;
}
