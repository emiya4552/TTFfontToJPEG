#include"jpg.h"

#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


#define JPEG_DCT_SIZE		8
#define JPEG_MCU_SIZE		16
#define JPEG_CHROMA_SCALE	2
#define JPEG_BLOCK_LENGTH	64
#define JPEG_COMPONENT_COUNT	3
#define JPEG_HUFFMAN_TABLE_COUNT	4
#define JPEG_PI			3.14159265358979323846

#define JPEG_COMPONENT_Y	0
#define JPEG_COMPONENT_CB	1
#define JPEG_COMPONENT_CR	2

#define JPEG_HUFFMAN_DC_Y	0
#define JPEG_HUFFMAN_AC_Y	1
#define JPEG_HUFFMAN_DC_C	2
#define JPEG_HUFFMAN_AC_C	3


typedef uint16_t	u16;
typedef uint32_t	u32;
typedef int16_t		s16;


typedef struct{
	u16	code[256];
	u8	size[256];
}jpeg_huffman_table;


typedef struct{
	FILE*	file;
	u32	bit_buffer;
	int	bit_count;
	int	last_dc[JPEG_COMPONENT_COUNT];
	int	error;
}jpeg_writer;


static const u8 base_quantization_y[JPEG_BLOCK_LENGTH] = {
	16, 11, 10, 16, 24, 40, 51, 61,
	12, 12, 14, 19, 26, 58, 60, 55,
	14, 13, 16, 24, 40, 57, 69, 56,
	14, 17, 22, 29, 51, 87, 80, 62,
	18, 22, 37, 56, 68, 109, 103, 77,
	24, 35, 55, 64, 81, 104, 113, 92,
	49, 64, 78, 87, 103, 121, 120, 101,
	72, 92, 95, 98, 112, 100, 103, 99
};


static const u8 base_quantization_c[JPEG_BLOCK_LENGTH] = {
	17, 18, 24, 47, 99, 99, 99, 99,
	18, 21, 26, 66, 99, 99, 99, 99,
	24, 26, 56, 99, 99, 99, 99, 99,
	47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99
};


static const u8 zigzag_order[JPEG_BLOCK_LENGTH] = {
	0, 1, 8, 16, 9, 2, 3, 10,
	17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};


static const u8 bits_dc_y[17] = {
	0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};


static const u8 values_dc_y[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};


static const u8 bits_dc_c[17] = {
	0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};


static const u8 values_dc_c[12] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};


static const u8 bits_ac_y[17] = {
	0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
};


static const u8 values_ac_y[162] = {
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
	0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
	0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
	0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
	0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
	0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
	0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};


static const u8 bits_ac_c[17] = {
	0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77
};


static const u8 values_ac_c[162] = {
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
	0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
	0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
	0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
	0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
	0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
	0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};


// 功能：向 JPEG 文件写入一个原始字节
static void write_byte(jpeg_writer* writer, u8 value)
{
	if(writer->error != 0){
		return;
	}
	if(fwrite(&value, sizeof(u8), 1, writer->file) != 1){
		writer->error = -1;
	}
}


// 功能：按照 JPEG 大端字节序写入双字节整数
static void write_word(jpeg_writer* writer, u16 value)
{
	write_byte(writer, (u8)(value>>8));
	write_byte(writer, (u8)(value&0xff));
}


// 功能：写入 JPEG 标记
static void write_marker(jpeg_writer* writer, u16 marker)
{
	write_word(writer, marker);
}


// 功能：写入熵编码字节并处理 0xff 字节填充
static void write_entropy_byte(jpeg_writer* writer, u8 value)
{
	write_byte(writer, value);
	if(value == 0xff){
		write_byte(writer, 0x00);
	}
}


// 功能：将指定长度的码字追加到熵编码 Bit 流
static void write_bits(jpeg_writer* writer, u16 value, int bit_length)
{
	int	bit;

	if(writer->error != 0 || bit_length<0 || bit_length>16){
		writer->error = -1;
		return;
	}

	// 从高位到低位逐位写入，满八位后立即输出并执行字节填充
	for(bit=bit_length-1; bit>=0; bit--){
		writer->bit_buffer = (writer->bit_buffer<<1)|((value>>bit)&1);
		writer->bit_count++;
		if(writer->bit_count == 8){
			write_entropy_byte(writer, (u8)writer->bit_buffer);
			writer->bit_buffer = 0;
			writer->bit_count = 0;
		}
	}
}


// 功能：使用全一填充完成最后一个不足八位的熵编码字节
static void flush_bits(jpeg_writer* writer)
{
	int	padding;
	u8	value;

	if(writer->bit_count <= 0 || writer->error != 0){
		return;
	}
	padding = 8-writer->bit_count;
	value = (u8)((writer->bit_buffer<<padding)|((1u<<padding)-1));
	write_entropy_byte(writer, value);
	writer->bit_buffer = 0;
	writer->bit_count = 0;
}


// 功能：统计 Huffman 码长表包含的符号数量
static int get_huffman_value_count(const u8 bits[17])
{
	int	count;
	int	i;

	count = 0;
	for(i=1; i<=16; i++){
		count += bits[i];
	}

	return count;
}


// 功能：根据码长数量与符号顺序生成规范 Huffman 码表
static int build_huffman_table(const u8 bits[17], const u8* values, int value_count, jpeg_huffman_table* table)
{
	u16	code;
	int	length;
	int	position;
	int	i;

	if(values==NULL || table==NULL || get_huffman_value_count(bits)!=value_count){
		return -1;
	}
	memset(table, 0, sizeof(jpeg_huffman_table));
	code = 0;
	position = 0;

	// JPEG 按码长递增顺序生成规范码，并映射到对应符号
	for(length=1; length<=16; length++){
		for(i=0; i<bits[length]; i++){
			table->code[values[position]] = code;
			table->size[values[position]] = (u8)length;
			code++;
			position++;
		}
		code <<= 1;
	}

	return 0;
}


// 功能：根据质量参数生成亮度和色度量化表
static void build_quantization_tables(int quality, u8 table_y[JPEG_BLOCK_LENGTH], u8 table_c[JPEG_BLOCK_LENGTH])
{
	int	scale;
	int	value_y;
	int	value_c;
	int	i;

	scale = (quality<50)?(5000/quality):(200-quality*2);
	for(i=0; i<JPEG_BLOCK_LENGTH; i++){
		value_y = (base_quantization_y[i]*scale+50)/100;
		value_c = (base_quantization_c[i]*scale+50)/100;
		if(value_y < 1){
			value_y = 1;
		}else if(value_y > 255){
			value_y = 255;
		}
		if(value_c < 1){
			value_c = 1;
		}else if(value_c > 255){
			value_c = 255;
		}
		table_y[i] = (u8)value_y;
		table_c[i] = (u8)value_c;
	}
}


// 功能：写入 JFIF APP0 段
static void write_app0(jpeg_writer* writer)
{
	static const u8 jfif_name[5] = {'J', 'F', 'I', 'F', 0};
	int	i;

	write_marker(writer, 0xffe0);
	write_word(writer, 16);
	for(i=0; i<5; i++){
		write_byte(writer, jfif_name[i]);
	}
	write_byte(writer, 1);
	write_byte(writer, 1);
	write_byte(writer, 0);
	write_word(writer, 1);
	write_word(writer, 1);
	write_byte(writer, 0);
	write_byte(writer, 0);
}


// 功能：按 Zigzag 顺序写入亮度和色度量化表
static void write_quantization_tables(jpeg_writer* writer, const u8 table_y[JPEG_BLOCK_LENGTH], const u8 table_c[JPEG_BLOCK_LENGTH])
{
	int	i;

	write_marker(writer, 0xffdb);
	write_word(writer, 2+65*2);
	write_byte(writer, 0x00);
	for(i=0; i<JPEG_BLOCK_LENGTH; i++){
		write_byte(writer, table_y[zigzag_order[i]]);
	}
	write_byte(writer, 0x01);
	for(i=0; i<JPEG_BLOCK_LENGTH; i++){
		write_byte(writer, table_c[zigzag_order[i]]);
	}
}


// 功能：写入 Baseline JPEG 帧头并配置 4:2:0 采样
static void write_frame_header(jpeg_writer* writer, int width, int height)
{
	write_marker(writer, 0xffc0);
	write_word(writer, 17);
	write_byte(writer, 8);
	write_word(writer, (u16)height);
	write_word(writer, (u16)width);
	write_byte(writer, JPEG_COMPONENT_COUNT);

	write_byte(writer, 1);
	write_byte(writer, 0x22);
	write_byte(writer, 0);
	write_byte(writer, 2);
	write_byte(writer, 0x11);
	write_byte(writer, 1);
	write_byte(writer, 3);
	write_byte(writer, 0x11);
	write_byte(writer, 1);
}


// 功能：将一组 Huffman 码长和符号写入 DHT 段
static void write_huffman_definition(jpeg_writer* writer, u8 table_information, const u8 bits[17], const u8* values)
{
	int	value_count;
	int	i;

	value_count = get_huffman_value_count(bits);
	write_byte(writer, table_information);
	for(i=1; i<=16; i++){
		write_byte(writer, bits[i]);
	}
	for(i=0; i<value_count; i++){
		write_byte(writer, values[i]);
	}
}


// 功能：写入亮度和色度的四组标准 Huffman 表
static void write_huffman_tables(jpeg_writer* writer)
{
	int	segment_length;

	segment_length = 2+
		1+16+get_huffman_value_count(bits_dc_y)+
		1+16+get_huffman_value_count(bits_ac_y)+
		1+16+get_huffman_value_count(bits_dc_c)+
		1+16+get_huffman_value_count(bits_ac_c);
	write_marker(writer, 0xffc4);
	write_word(writer, (u16)segment_length);
	write_huffman_definition(writer, 0x00, bits_dc_y, values_dc_y);
	write_huffman_definition(writer, 0x10, bits_ac_y, values_ac_y);
	write_huffman_definition(writer, 0x01, bits_dc_c, values_dc_c);
	write_huffman_definition(writer, 0x11, bits_ac_c, values_ac_c);
}


// 功能：写入单次顺序扫描的 SOS 段
static void write_scan_header(jpeg_writer* writer)
{
	write_marker(writer, 0xffda);
	write_word(writer, 12);
	write_byte(writer, JPEG_COMPONENT_COUNT);
	write_byte(writer, 1);
	write_byte(writer, 0x00);
	write_byte(writer, 2);
	write_byte(writer, 0x11);
	write_byte(writer, 3);
	write_byte(writer, 0x11);
	write_byte(writer, 0);
	write_byte(writer, 63);
	write_byte(writer, 0);
}


// 功能：从 RGB 画布提取一个保持完整分辨率的 8×8 亮度块
static void get_luma_block(const u8* color_data, int width, int height, int block_x, int block_y, double block[JPEG_BLOCK_LENGTH])
{
	size_t	position;
	double	red;
	double	green;
	double	blue;
	int	source_x;
	int	source_y;
	int	x;
	int	y;
	int	index;

	// 超过图像右侧或底部的亮度采样点复制最近边缘像素
	for(y=0; y<JPEG_DCT_SIZE; y++){
		source_y = block_y+y;
		if(source_y >= height){
			source_y = height-1;
		}
		for(x=0; x<JPEG_DCT_SIZE; x++){
			source_x = block_x+x;
			if(source_x >= width){
				source_x = width-1;
			}
			position = ((size_t)source_y*width+source_x)*3;
			red = color_data[position];
			green = color_data[position+1];
			blue = color_data[position+2];
			index = y*JPEG_DCT_SIZE+x;
			block[index] = 0.299*red+0.587*green+0.114*blue;
		}
	}
}


// 功能：对 16×16 MCU 的 RGB 像素执行 2×2 平均并生成两个 8×8 色度块
static void get_chroma_blocks(const u8* color_data, int width, int height, int mcu_x, int mcu_y, double block_cb[JPEG_BLOCK_LENGTH], double block_cr[JPEG_BLOCK_LENGTH])
{
	size_t	position;
	double	red;
	double	green;
	double	blue;
	int	source_x;
	int	source_y;
	int	offset_x;
	int	offset_y;
	int	x;
	int	y;
	int	index;

	// 每个色度采样点由对应的 2×2 RGB 区域平均得到
	for(y=0; y<JPEG_DCT_SIZE; y++){
		for(x=0; x<JPEG_DCT_SIZE; x++){
			red = 0;
			green = 0;
			blue = 0;
			for(offset_y=0; offset_y<JPEG_CHROMA_SCALE; offset_y++){
				source_y = mcu_y+y*JPEG_CHROMA_SCALE+offset_y;
				if(source_y >= height){
					source_y = height-1;
				}
				for(offset_x=0; offset_x<JPEG_CHROMA_SCALE; offset_x++){
					source_x = mcu_x+x*JPEG_CHROMA_SCALE+offset_x;
					if(source_x >= width){
						source_x = width-1;
					}
					position = ((size_t)source_y*width+source_x)*3;
					red += color_data[position];
					green += color_data[position+1];
					blue += color_data[position+2];
				}
			}

			// RGB 到 YCbCr 为线性变换，先平均 RGB 可减少重复计算
			red /= JPEG_CHROMA_SCALE*JPEG_CHROMA_SCALE;
			green /= JPEG_CHROMA_SCALE*JPEG_CHROMA_SCALE;
			blue /= JPEG_CHROMA_SCALE*JPEG_CHROMA_SCALE;
			index = y*JPEG_DCT_SIZE+x;
			block_cb[index] = -0.168736*red-0.331264*green+0.5*blue+128;
			block_cr[index] = 0.5*red-0.418688*green-0.081312*blue+128;
		}
	}
}


// 功能：对一个 8×8 分量块执行 DCT 和量化
static void transform_block(const double input[JPEG_BLOCK_LENGTH], const u8 quantization[JPEG_BLOCK_LENGTH], s16 output[JPEG_BLOCK_LENGTH])
{
	double	coefficient;
	double	factor_u;
	double	factor_v;
	double	sum;
	int	quantized;
	int	u;
	int	v;
	int	x;
	int	y;

	// 使用二维 DCT 公式逐项计算频域系数，再按照对应位置量化
	for(v=0; v<JPEG_DCT_SIZE; v++){
		factor_v = (v==0)?(1.0/sqrt(2.0)):1.0;
		for(u=0; u<JPEG_DCT_SIZE; u++){
			factor_u = (u==0)?(1.0/sqrt(2.0)):1.0;
			sum = 0;
			for(y=0; y<JPEG_DCT_SIZE; y++){
				for(x=0; x<JPEG_DCT_SIZE; x++){
					sum += (input[y*JPEG_DCT_SIZE+x]-128.0)*
						cos((2*x+1)*u*JPEG_PI/16.0)*
						cos((2*y+1)*v*JPEG_PI/16.0);
				}
			}
			coefficient = 0.25*factor_u*factor_v*sum/quantization[v*JPEG_DCT_SIZE+u];
			quantized = (coefficient>=0)?(int)(coefficient+0.5):(int)(coefficient-0.5);
			output[v*JPEG_DCT_SIZE+u] = (s16)quantized;
		}
	}
}


// 功能：计算一个有符号系数的 JPEG 幅值位数
static int get_value_size(int value)
{
	unsigned int	absolute;
	int		size;

	absolute = (value<0)?(unsigned int)(-value):(unsigned int)value;
	size = 0;
	while(absolute > 0){
		absolute >>= 1;
		size++;
	}

	return size;
}


// 功能：将有符号系数转换为 JPEG 幅值附加位
static u16 get_value_bits(int value, int size)
{
	if(value >= 0){
		return (u16)value;
	}

	return (u16)(value+((1<<size)-1));
}


// 功能：写入一个量化块的 DC 差分和 AC 游程 Huffman 数据
static void encode_block(jpeg_writer* writer, const s16 block[JPEG_BLOCK_LENGTH], int component, const jpeg_huffman_table tables[JPEG_HUFFMAN_TABLE_COUNT])
{
	const jpeg_huffman_table*	dc_table;
	const jpeg_huffman_table*	ac_table;
	int	value;
	int	difference;
	int	size;
	int	zero_count;
	int	symbol;
	int	i;

	if(component == JPEG_COMPONENT_Y){
		dc_table = &tables[JPEG_HUFFMAN_DC_Y];
		ac_table = &tables[JPEG_HUFFMAN_AC_Y];
	}else{
		dc_table = &tables[JPEG_HUFFMAN_DC_C];
		ac_table = &tables[JPEG_HUFFMAN_AC_C];
	}

	// DC 使用同一分量前一个块的 DC 系数作为预测值
	difference = block[0]-writer->last_dc[component];
	writer->last_dc[component] = block[0];
	size = get_value_size(difference);
	if(size>11 || dc_table->size[size]==0){
		writer->error = -1;
		return;
	}
	write_bits(writer, dc_table->code[size], dc_table->size[size]);
	if(size > 0){
		write_bits(writer, get_value_bits(difference, size), size);
	}

	// AC 按 Zigzag 顺序统计零游程，并写入 ZRL、普通符号或 EOB
	zero_count = 0;
	for(i=1; i<JPEG_BLOCK_LENGTH; i++){
		value = block[zigzag_order[i]];
		if(value == 0){
			zero_count++;
			continue;
		}
		while(zero_count >= 16){
			write_bits(writer, ac_table->code[0xf0], ac_table->size[0xf0]);
			zero_count -= 16;
		}
		size = get_value_size(value);
		if(size>10){
			writer->error = -1;
			return;
		}
		symbol = (zero_count<<4)|size;
		if(ac_table->size[symbol] == 0){
			writer->error = -1;
			return;
		}
		write_bits(writer, ac_table->code[symbol], ac_table->size[symbol]);
		write_bits(writer, get_value_bits(value, size), size);
		zero_count = 0;
	}
	if(zero_count > 0){
		write_bits(writer, ac_table->code[0x00], ac_table->size[0x00]);
	}
}


// 功能：初始化编码所需的四组标准 Huffman 码表
static int initialize_huffman_tables(jpeg_huffman_table tables[JPEG_HUFFMAN_TABLE_COUNT])
{
	if(build_huffman_table(bits_dc_y, values_dc_y, 12, &tables[JPEG_HUFFMAN_DC_Y]) != 0 ||
	   build_huffman_table(bits_ac_y, values_ac_y, 162, &tables[JPEG_HUFFMAN_AC_Y]) != 0 ||
	   build_huffman_table(bits_dc_c, values_dc_c, 12, &tables[JPEG_HUFFMAN_DC_C]) != 0 ||
	   build_huffman_table(bits_ac_c, values_ac_c, 162, &tables[JPEG_HUFFMAN_AC_C]) != 0){
		return -1;
	}

	return 0;
}


// 功能：按 4:2:0 MCU 顺序转换、量化并编码整张 RGB 图像
static void write_scan_data(jpeg_writer* writer, const u8* color_data, int width, int height, const u8 table_y[JPEG_BLOCK_LENGTH], const u8 table_c[JPEG_BLOCK_LENGTH], const jpeg_huffman_table tables[JPEG_HUFFMAN_TABLE_COUNT])
{
	double	block_y[JPEG_BLOCK_LENGTH];
	double	block_cb[JPEG_BLOCK_LENGTH];
	double	block_cr[JPEG_BLOCK_LENGTH];
	s16	coefficient[JPEG_BLOCK_LENGTH];
	int	mcu_x;
	int	mcu_y;
	int	block_offset_x;
	int	block_offset_y;

	for(mcu_y=0; mcu_y<height && writer->error==0; mcu_y+=JPEG_MCU_SIZE){
		for(mcu_x=0; mcu_x<width && writer->error==0; mcu_x+=JPEG_MCU_SIZE){
			// 每个 MCU 先按从左到右、从上到下的顺序编码四个亮度块
			for(block_offset_y=0; block_offset_y<JPEG_MCU_SIZE &&
			    writer->error==0; block_offset_y+=JPEG_DCT_SIZE){
				for(block_offset_x=0; block_offset_x<JPEG_MCU_SIZE &&
				    writer->error==0; block_offset_x+=JPEG_DCT_SIZE){
					get_luma_block(color_data, width, height,
						mcu_x+block_offset_x, mcu_y+block_offset_y, block_y);
					transform_block(block_y, table_y, coefficient);
					encode_block(writer, coefficient, JPEG_COMPONENT_Y, tables);
				}
			}

			// 四个亮度块之后依次编码一个 Cb 块和一个 Cr 块
			if(writer->error == 0){
				get_chroma_blocks(color_data, width, height, mcu_x, mcu_y,
					block_cb, block_cr);
				transform_block(block_cb, table_c, coefficient);
				encode_block(writer, coefficient, JPEG_COMPONENT_CB, tables);
				transform_block(block_cr, table_c, coefficient);
				encode_block(writer, coefficient, JPEG_COMPONENT_CR, tables);
			}
		}
	}
	flush_bits(writer);
}


// 功能：检查 JPEG 编码入口参数
static int check_jpeg_input(const char* file, const u8* color_data, int width, int height, int quality)
{
	if(file==NULL || file[0]=='\0' || color_data==NULL ||
	   width<=0 || height<=0 || width>65535 || height>65535 ||
	   quality<JPEG_MIN_QUALITY || quality>JPEG_MAX_QUALITY){
		return -1;
	}
	if((size_t)width>(size_t)-1/(size_t)height ||
	   (size_t)width*height>(size_t)-1/3){
		return -1;
	}

	return 0;
}


// 功能：将 RGB 像素缓冲区编码为 4:2:0 Baseline JPEG 文件
int jpeg_generate(const char* file, const u8* color_data, int width, int height, int quality)
{
	jpeg_huffman_table	tables[JPEG_HUFFMAN_TABLE_COUNT];
	jpeg_writer		writer;
	u8			table_y[JPEG_BLOCK_LENGTH];
	u8			table_c[JPEG_BLOCK_LENGTH];
	int			result;

	if(check_jpeg_input(file, color_data, width, height, quality) != 0){
		printf("[JPEG][ERROR] invalid input: width=%d height=%d quality=%d\n",
			width, height, quality);
		return -1;
	}
	if(initialize_huffman_tables(tables) != 0){
		printf("[JPEG][ERROR] Huffman table initialization failed\n");
		return -1;
	}
	build_quantization_tables(quality, table_y, table_c);
	memset(&writer, 0, sizeof(jpeg_writer));
	writer.file = fopen(file, "wb");
	if(writer.file == NULL){
		printf("[JPEG][ERROR] output open failed: file=%s\n", file);
		return -1;
	}

	// 按 JFIF Baseline 顺序写入文件头、扫描数据和结束标记
	write_marker(&writer, 0xffd8);
	write_app0(&writer);
	write_quantization_tables(&writer, table_y, table_c);
	write_frame_header(&writer, width, height);
	write_huffman_tables(&writer);
	write_scan_header(&writer);
	write_scan_data(&writer, color_data, width, height, table_y, table_c, tables);
	write_marker(&writer, 0xffd9);
	result = writer.error;
	if(fclose(writer.file) != 0){
		result = -1;
	}
	if(result != 0){
		remove(file);
		printf("[JPEG][ERROR] encoding failed: file=%s\n", file);
		return -1;
	}

	printf("[JPEG][INFO] generated: file=%s width=%d height=%d quality=%d sampling=4:2:0\n",
		file, width, height, quality);
	return 0;
}
