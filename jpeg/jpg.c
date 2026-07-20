#include"jpg.h"




// get rgb data from file
void get_BMP(char* infile, u8** p_out_data, int* p_width, int* p_height)
{
	FILE*			fp;
	BMP_FILE_HEADER		file_header;
	BMP_INFO_HEADER		imformation_header;
	u8*			color_data;
	int			width;
	int			height;
	int			row_size;
	u8*			row_buffer;
	int			i;
	int			j;
	int			k;

	if((fp=fopen(infile, "rb")) == NULL){
		printf("open file error\n");
		return;
	}	

	fread(&file_header, sizeof(file_header), 1, fp);
	fread(&imformation_header, sizeof(imformation_header), 1, fp);
	
	// check bitBitCount
	if(imformation_header.biBitCount/8 != PER_PIXEL_BYTE){
		printf("not 24bit per pixel\n");
		return;
	}
	// initialize
	width		= imformation_header.biWidth;
	height		= fabs(imformation_header.biHeight);
	row_size	= ((width*24+31)/32)*4;
	row_buffer	= malloc(row_size);


	// jump to color_data
	fseek(fp, file_header.bf0ffBits, SEEK_SET);

	// initialize color_data space
	color_data 	= (u8*)malloc(sizeof(u8)*width*height*PER_PIXEL_BYTE);

	// write color_data
	if(imformation_header.biHeight > 0){
		for(i=height-1; i>=0; i--){
			fread(row_buffer, 1, row_size, fp);
			memcpy(&COLOR_DATA(i,0,0), row_buffer, width*3);
			
		}
	}else{
		for(i=0; i<height; i++){
			fread(row_buffer, 1, row_size, fp);
			memcpy(&COLOR_DATA(i,0,0), row_buffer, width*3);

		}
	}
	// adjust
	for(i=0; i<height; i++){
		for(j=0; j<width; j++){
			u8	temp		= COLOR_DATA(i,j,0);
			COLOR_DATA(i,j,0)	= COLOR_DATA(i,j,2);
			COLOR_DATA(i,j,2)	= temp;
		}
	}



	*p_width	= width;
	*p_height	= height;
	*p_out_data	= color_data;

	fclose(fp);
	free(row_buffer);
}		



void rgb_to_ycc(u8* color_data, u8** p_out_data, int height, int width)
{
	int 	i;
	int	j;

	u8*	ycc_data;

	// initialize space
	ycc_data	= (u8*)malloc(sizeof(u8)*height*width*PER_PIXEL_BYTE);
	*p_out_data	= ycc_data;

	// write data
	for(i=0; i<height; i++){
		for(j=0; j<width; j++){
			// Y
			YCC_DATA(i,j,0)	= 0.299*COLOR_DATA(i,j,0)+0.587*COLOR_DATA(i,j,1)+0.114*COLOR_DATA(i,j,2);
			// Cr
			YCC_DATA(i,j,2)	= 0.5*COLOR_DATA(i,j,0)-0.4187*COLOR_DATA(i,j,1)-0.0813*COLOR_DATA(i,j,2)+128;
			// Cb
			YCC_DATA(i,j,1)	= -0.1687*COLOR_DATA(i,j,0)-0.3313*COLOR_DATA(i,j,1)+0.5*COLOR_DATA(i,j,2)+128;
		}
	}
	free(color_data);
}




int devide_block(u8* ycc_data, int width, int height, u8_block** p_block_array)
{
	int		i;
	int		j;
	int		m;
	int		n;
	int		a;
	int		b;

	int		width_block_number;
	int		height_block_number;
	u8_block*	block_array;
	u8		right_edge_data[MCUSIZE];
	u8		down_edge_data[MCUSIZE];

	// initialize 
	width_block_number	= (width+15)/MCUSIZE;
	height_block_number	= (height+15)/MCUSIZE;

	// initialize space
	block_array		= (u8_block*)malloc(sizeof(u8_block)*width_block_number*height_block_number);
	*p_block_array		= block_array;

	for(i=0; i<height_block_number; i++){
		for(j=0; j<width_block_number; j++){
			// write Y
			// expand right edge firstly and then expend bottom
			// expand is copy last data
			for(a=0; a<2; a++){
				for(b=0; b<2; b++){
					for(m=0; m<DCTSIZE; m++){
						for(n=0; n<DCTSIZE; n++){
							if((m+i*MCUSIZE+a*DCTSIZE)<height && (n+j*MCUSIZE+b*DCTSIZE)<width){
								(BLOCK_ARRAY(i,j)).Y_data[a*2+b][m][n]	= YCC_DATA(m+i*MCUSIZE+a*DCTSIZE, n+j*MCUSIZE+b*DCTSIZE, 0);
							}else if((n+j*MCUSIZE+b*DCTSIZE)>=width){
								(BLOCK_ARRAY(i,j)).Y_data[a*2+b][m][n]  =(BLOCK_ARRAY(i,j)).Y_data[a*2+b][m][n-1];
							}else if((m+i*MCUSIZE+a*DCTSIZE)>=height){
								(BLOCK_ARRAY(i,j)).Y_data[a*2+b][m][n]  =(BLOCK_ARRAY(i,j)).Y_data[a*2+b][m-1][n];
							}
						}
					}
				}
			}
			// wirte Cb
			for(m=0; m<MCUSIZE; m++){
				for(n=0; n<MCUSIZE; n++){
					if((m+i*MCUSIZE)<height && (n+j*MCUSIZE)<width){
						(BLOCK_ARRAY(i,j)).Cb_data[m][n]	= YCC_DATA(m+i*MCUSIZE, n+j*MCUSIZE, 1);
					}else if(n+j*MCUSIZE >= width){
						(BLOCK_ARRAY(i,j)).Cb_data[m][n]	= (BLOCK_ARRAY(i,j)).Cb_data[m][n-1];
					}else if(m+i*MCUSIZE >= height){
						(BLOCK_ARRAY(i,j)).Cb_data[m][n]	= (BLOCK_ARRAY(i,j)).Cb_data[m-1][n];
					}
				}
			}
			// wirte Cr
			for(m=0; m<MCUSIZE; m++){
				for(n=0; n<MCUSIZE; n++){
					if((m+i*MCUSIZE)<height && (n+j*MCUSIZE)<width){
						(BLOCK_ARRAY(i,j)).Cr_data[m][n]	= YCC_DATA(m+i*MCUSIZE, n+j*MCUSIZE, 2);
					}else if(n+j*MCUSIZE >= width){
						(BLOCK_ARRAY(i,j)).Cr_data[m][n]	= (BLOCK_ARRAY(i,j)).Cb_data[m][n-1];
					}else if(m+i*MCUSIZE >= height){
						(BLOCK_ARRAY(i,j)).Cr_data[m][n]	= (BLOCK_ARRAY(i,j)).Cb_data[m-1][n];
					}
				}
			}
		}
	}

	return width_block_number*height_block_number;
}

void multiply_matrix(double* matrix1, double* matrix2, double* out, int size)
{
	int 	i;
	int 	j;
	double	temporary;
	int	t;

	for(i=0; i<size; i++){
		for(t=0; t<size; t++){
			temporary	= 0.0;
			for(j=0; j<size; j++){
				temporary	+= matrix1[i*size+j]*matrix2[j*size+t];
			}
			out[i*size+t]	= temporary;
		}
	}
}

void block_DCT(u8* block_data, s8* out, int size)
{
	int	i;
	int	j;
	double	sum;
	double*	matrix;
	double*	Tmatrix;
	double*	temporary_matrix;
	double*	input;
	double*	out_data;
	int	n;
	int	quantization;
	int	temporary;

	// initialize space
	matrix			= (double*)malloc(sizeof(double)*size*size);
	Tmatrix			= (double*)malloc(sizeof(double)*size*size);
	temporary_matrix	= (double*)malloc(sizeof(double)*size*size);
	input			= (double*)malloc(sizeof(double)*size*size);
	out_data		= (double*)malloc(sizeof(double)*size*size);
	n			= size==DCTSIZE?8:4;

	// prepare
	// get
	for(i=0; i<size; i++){
		double	c = i==0?sqrt(1.0/size):sqrt(2.0/size);
		for(j=0; j<size; j++){
			input[i*size+j]		= block_data[i*size+j] - CENTERJSAMPLE;
			matrix[i*size+j]	= c*cos((j+0.5)*PI/size*i);
			Tmatrix[j*size+i]	= matrix[i*size+j];
		}
	}

	// DCT
	multiply_matrix(matrix, input, temporary_matrix, size);
	multiply_matrix(temporary_matrix, Tmatrix, out_data, size);
	
	// quantization
	for(i=0; i<DCTSIZE; i++){
		for(j=0; j<DCTSIZE; j++){
			quantization	= size==DCTSIZE?quantization_table_Y[i*DCTSIZE+j]:quantization_table_C[i*DCTSIZE+j];
			temporary	= out_data[i*size+j]*n;
			if(temporary > 0){
				temporary	+= quantization>>1;
				DIVIDE(temporary, quantization);
			}else{
				temporary	= -temporary;
				temporary	+= quantization>>1;
				DIVIDE(temporary, quantization);
				temporary	= -temporary;
			}
			out[i*DCTSIZE+j]	= temporary;
		}
	}


	// free
	free(matrix);
	free(Tmatrix);
	free(temporary_matrix);
	free(input);
	free(out_data);
}


void all_DCT(u8_block* in_block_array, int block_number, s8_block** p_block_array)
{
	s8_block*	block_array;
	int		i;
	int		j;


	// initialize space
	block_array	= (s8_block*)malloc(sizeof(s8_block)*block_number);
	*p_block_array	= block_array;

	// DCT
	for(i=0; i<block_number; i++){
		block_DCT((u8*)(in_block_array[i]).Cb_data, (s8*)(block_array[i]).Cb_data, MCUSIZE);
		block_DCT((u8*)(in_block_array[i]).Cr_data, (s8*)(block_array[i]).Cr_data, MCUSIZE);
		for(j=0; j<4; j++){
			block_DCT((u8*)((in_block_array[i]).Y_data[j]), (s8*)((block_array[i]).Y_data[j]), DCTSIZE);
		}
	}
}
	

void get_huff_table(huff_table*	p_huff,const u8* bits_array, const u8* value_array)
{
	int	i;
	int	p;
	int	sum;
	bool	first;
	int	j;
	int	interval;
	int	l;
	int	lastp;
	int	si;
	u32	code;

	s8	huffsize[257];
	u32	huffcode[257];

	p	= 0;
	for(l=1; l<=16; l++){
		i = (int)bits_array[l];
		while(i--){
			huffsize[p++]	= (char)l;
		}
	}
	huffsize[p]	= 0;
	lastp		= p;

	code	= 0;
	si	= huffsize[0];
	p	= 0;
	while(huffsize[p]){
		while(((int)huffsize[p]) == si){
			huffcode[p++]	= code;
			code ++;
		}
		code <<= 1;
		si++;
	}

	(*p_huff).size	= (u32*)malloc(sizeof(u32)*300);
	(*p_huff).value	= (u32*)malloc(sizeof(u32)*300);

	for(p=0; p<lastp; p++){
		i			= value_array[p];
		(*p_huff).value[i]	= huffcode[p];
		(*p_huff).size[i]	= huffsize[p];
	}

}

void initialize_huff(huff_table** p_huff_array)
{
	*p_huff_array	= (huff_table*)malloc(sizeof(huff_table)*HUFF_TABLE_NUM);

	get_huff_table(&((*p_huff_array)[INDEX_DC_Y]), bits_dc_luminance, val_dc_luminance);
	get_huff_table(&((*p_huff_array)[INDEX_AC_Y]), bits_ac_luminance, val_ac_luminance);
	get_huff_table(&((*p_huff_array)[INDEX_DC_C]), bits_dc_chrominance, val_dc_chrominance);
	get_huff_table(&((*p_huff_array)[INDEX_AC_C]), bits_ac_chrominance, val_ac_chrominance);
}

void emit(u8** p_data, int size, unsigned int code)
{

	static u32	buffer		= 0;
	u32		new_code;
	static int	bits		= 0;
	static int	emit_time	= 0;
	static int	expand_time	= 0;

	static bool	ii		= true;


	

	new_code	= ((int)code)&((((int)1)<<size)-1);
	bits += size;
	
	new_code <<= 24-bits;

	buffer	|= new_code;

	while(bits >= 8){
		u8 emit_data	= (u8)((buffer>>16)&0xFF);
		(*p_data)[emit_time] = emit_data;
		emit_time++;
		
		if(emit_data == 0xFF){
			(*p_data)[emit_time]	= 0;
			emit_time++;
		}
		buffer <<= 8;
		bits -= 8;
	}


	if(emit_time>10000 && ii){
		FILE*	fp = fopen("eee", "w");
		for(int i=0 ; i<1000; i++){
			fprintf(fp, "%x\n", (*p_data)[i]);
		}
		fclose(fp);
		ii 	= false;
	}

}



void encode_block(s8* block_data, huff_table* huff_array, u8** p_data, bool is_y, bool is_cb)
{
	
	register int	temp1;
	register int	temp2;
	register int	bits;
	register int	i;
	register int 	zero_count;
	static int	last_dc_y	= 0;
	static int	last_dc_cr	= 0;
	static int	last_dc_cb	= 0;
	int		index_dc;
	int		index_ac;

	FILE*		file 	= fopen("encode", "a");
	FILE*		fp 	= fopen("encode2", "a");


	zero_count	= 0;
	if(is_y){
		temp1 		= temp2 	= block_data[0] - last_dc_y;
		last_dc_y	= block_data[0];
	}else if(is_cb){
		temp1		= temp2		= block_data[0] - last_dc_cb;
		last_dc_cb	= block_data[0];
	}else{
		temp1		= temp2		= block_data[0] - last_dc_cr;
		last_dc_cr	= block_data[0];
	}



	if(is_y){
		index_dc	= INDEX_DC_Y;
		index_ac	= INDEX_AC_Y;
	}else{
		index_ac	= INDEX_AC_C;
		index_dc	= INDEX_DC_C;
	}

	if(temp1<0){
		temp1 = -temp1;
		temp2--;
	}

	bits	= 0;
	while(temp1){
		bits ++;
		temp1 >>= 1;
	}
	fprintf(file, "dc1:%x\t%x\t",huff_array[index_dc].size[bits], huff_array[index_dc].value[bits]);
	emit(p_data, huff_array[index_dc].size[bits], huff_array[index_dc].value[bits]);

	if(bits){
		fprintf(file, "dc2:%x\t%x\t",bits, (u32)temp2);
		emit(p_data, bits, (u32)temp2);
	}
	
	for(i=1; i<DCTSIZE2; i++){
		if((temp2 = block_data[natural_order[i]]) == 0){
			zero_count ++;
		}else{
			
			fprintf(fp, "%d\t%d\n", i, natural_order[i]);
	
			while(zero_count > 15){
		
				fprintf(file, "160: %x\t%x\t", huff_array[index_ac].size[0xF0], huff_array[index_ac].value[0xF0]);
				emit(p_data, huff_array[index_ac].size[0xF0], huff_array[index_ac].value[0xF0]);
				zero_count -= 16;
			}
			
			temp1 = temp2;
			if(temp1 < 0){
				temp1 = -temp1;
				temp2 --;
			}
			bits = 1;
			while((temp1 >>= 1)){
				bits++;
			}

			temp1 = (zero_count << 4) + bits;

			fprintf(file, "ac1:%x\t%x\t%x\t",huff_array[index_ac].size[temp1], huff_array[index_ac].value[temp1],temp1);
			
			emit(p_data, huff_array[index_ac].size[temp1], huff_array[index_ac].value[temp1]);
			fprintf(file, "ac2:%x\t%x\n",bits, (u32)temp2);
			emit(p_data, bits, (u32)temp2);


			zero_count = 0;

		}
	}

	fclose(file);
	fclose(fp);
	if(zero_count > 0)
		emit(p_data, huff_array[index_ac].size[0], huff_array[index_ac].value[0]);

}

void encode(huff_table* huff_array, s8_block* block_array, u8** p_data, int block_number)
{
	u8*	data;
	int	space;
	int	i;
	int	j;
	

	space	= 3000000;
	data	= *p_data;
	data	= (u8*)malloc(sizeof(u8)*space);


	for(i=0; i<block_number; i++){

		for(j=0; j<4; j++){
			encode_block(block_array[i].Y_data[j], huff_array, &data, true, false);
		}
		encode_block(block_array[i].Cb_data, huff_array, &data, false, true);
		encode_block(block_array[i].Cr_data, huff_array, &data, false, false);
	}

}


// ===================================================================================
// START: New functions to write JPEG headers
// ===================================================================================

void write_byte(FILE *fp, u8 value) {
    fwrite(&value, 1, 1, fp);
}

void write_word(FILE *fp, u16 value) {
    u8 high = (value >> 8) & 0xFF;
    u8 low = value & 0xFF;
    write_byte(fp, high);
    write_byte(fp, low);
}

void write_marker(FILE *fp, u16 marker) {
    write_word(fp, marker);
}

// SOI: Start of Image
void write_SOI(FILE *fp) {
    write_marker(fp, 0xFFD8);
}

// EOI: End of Image
void write_EOI(FILE *fp) {
    write_marker(fp, 0xFFD9);
}

// APP0: JFIF Header
void write_APP0(FILE *fp) {
    write_marker(fp, 0xFFE0); // APP0 marker
    write_word(fp, 16);       // Length of segment
    fwrite("JFIF\0", 5, 1, fp); // JFIF identifier
    write_byte(fp, 1);        // Major version
    write_byte(fp, 1);        // Minor version
    write_byte(fp, 0);        // Density units (0=aspect ratio)
    write_word(fp, 1);        // X density
    write_word(fp, 1);        // Y density
    write_byte(fp, 0);        // Thumbnail width
    write_byte(fp, 0);        // Thumbnail height
}

// DQT: Define Quantization Tables
void write_DQTs(FILE *fp) {
    // Luminance Table
    write_marker(fp, 0xFFDB);
    write_word(fp, 67);       // Length
    write_byte(fp, 0x00);     // Precision (8-bit) and ID (0)
    fwrite(quantization_table_Y, 64, 1, fp);

    // Chrominance Table
    write_marker(fp, 0xFFDB);
    write_word(fp, 67);       // Length
    write_byte(fp, 0x01);     // Precision (8-bit) and ID (1)
    fwrite(quantization_table_C, 64, 1, fp);
}

// SOF0: Start of Frame (Baseline DCT)
void write_SOF0(FILE *fp, int width, int height) {
    write_marker(fp, 0xFFC0);
    write_word(fp, 17);       // Length
    write_byte(fp, 8);        // Sample precision (8-bit)
    write_word(fp, height);
    write_word(fp, width);
    write_byte(fp, 3);        // Number of components (Y, Cb, Cr)

    // Y component
    write_byte(fp, 1);        // Component ID
    write_byte(fp, 0x22);     // Sampling factors (H=2, V=2)
    write_byte(fp, 0);        // Quantization table ID

    // Cb component
    write_byte(fp, 2);        // Component ID
    write_byte(fp, 0x11);     // Sampling factors (H=1, V=1)
    write_byte(fp, 1);        // Quantization table ID

    // Cr component
    write_byte(fp, 3);        // Component ID
    write_byte(fp, 0x11);     // Sampling factors (H=1, V=1)
    write_byte(fp, 1);        // Quantization table ID
}

void write_huff_table_segment(FILE* fp, u8 table_class_id, const u8* bits, const u8* values) {
    write_byte(fp, table_class_id);
    fwrite(bits + 1, 16, 1, fp); // Write 16 bytes for bit counts
    int count = 0;
    for (int i = 1; i <= 16; i++) {
        count += bits[i];
    }
    fwrite(values, count, 1, fp);
}

// DHT: Define Huffman Tables
void write_DHT(FILE *fp) {
    int len_dc_y = 0, len_ac_y = 0, len_dc_c = 0, len_ac_c = 0;
    for (int i=1; i<=16; i++) {
        len_dc_y += bits_dc_luminance[i];
        len_ac_y += bits_ac_luminance[i];
        len_dc_c += bits_dc_chrominance[i];
        len_ac_c += bits_ac_chrominance[i];
    }

    write_marker(fp, 0xFFC4);
    // Total length = 2 (length field) + sum of (1 (ID) + 16 (bits) + values_count) for each table
    write_word(fp, 2 + (1+16+len_dc_y) + (1+16+len_ac_y) + (1+16+len_dc_c) + (1+16+len_ac_c));

    write_huff_table_segment(fp, 0x00, bits_dc_luminance, val_dc_luminance);   // DC, Y
    write_huff_table_segment(fp, 0x10, bits_ac_luminance, val_ac_luminance);   // AC, Y
    write_huff_table_segment(fp, 0x01, bits_dc_chrominance, val_dc_chrominance); // DC, C
    write_huff_table_segment(fp, 0x11, bits_ac_chrominance, val_ac_chrominance); // AC, C
}


// SOS: Start of Scan
void write_SOS(FILE *fp) {
    write_marker(fp, 0xFFDA);
    write_word(fp, 12);       // Length
    write_byte(fp, 3);        // Number of components in scan

    // Y component
    write_byte(fp, 1);        // Component ID
    write_byte(fp, 0x00);     // Huffman tables (DC=0, AC=0)

    // Cb component
    write_byte(fp, 2);        // Component ID
    write_byte(fp, 0x11);     // Huffman tables (DC=1, AC=1)

    // Cr component
    write_byte(fp, 3);        // Component ID
    write_byte(fp, 0x11);     // Huffman tables (DC=1, AC=1)

    // Spectral selection and successive approx.
    write_byte(fp, 0);
    write_byte(fp, 63);
    write_byte(fp, 0);
}

// ===================================================================================
// END: New functions to write JPEG headers
// ===================================================================================


void get_jpg(char* file_name, int width, int height, u8* data, int data_size)
{
	FILE* fp = fopen(file_name, "wb");
    if (!fp) {
        printf("Error: Could not create output file %s\n", file_name);
        return;
    }

    // 1. Write headers
    write_SOI(fp);
    write_APP0(fp);
    write_DQTs(fp);
    write_SOF0(fp, width, height);
    write_DHT(fp);
    write_SOS(fp);

    // 2. Write compressed image data
    fwrite(data, 1, data_size, fp);

    // 3. Write end of image marker
    write_EOI(fp);

	fclose(fp);
}


// Main function to drive the conversion
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input.bmp> <output.jpg>\n", argv[0]);
        return 1;
    }

    char* infile = argv[1];
    char* outfile = argv[2];

    // Variables for image data and properties
    u8* rgb_data = NULL;
    u8* ycc_data = NULL;
    u8_block* u8_blocks = NULL;
    s8_block* s8_blocks = NULL;
    huff_table* huffman_tables = NULL;
    u8* encoded_data = NULL;
    int width, height;
    int block_count;
    int encoded_size;

    // STEP 1: Read BMP file
    printf("Reading BMP file: %s\n", infile);
    get_BMP(infile, &rgb_data, &width, &height);
    if (!rgb_data) {
        printf("Failed to read BMP file.\n");
        return 1;
    }
    printf("Image dimensions: %d x %d\n", width, height);

    // STEP 2: RGB to YCbCr conversion
    printf("Converting RGB to YCbCr...\n");
    rgb_to_ycc(rgb_data, &ycc_data, height, width);

    // STEP 3: Divide into 8x8 blocks (with 4:2:0 subsampling)
    printf("Dividing into blocks...\n");
    block_count = devide_block(ycc_data, width, height, &u8_blocks);

    // STEP 4: Apply DCT and Quantization
    printf("Applying DCT and Quantization...\n");
    all_DCT(u8_blocks, block_count, &s8_blocks);

    // STEP 5: Initialize Huffman Tables
    printf("Initializing Huffman Tables...\n");
    initialize_huff(&huffman_tables);

    // STEP 6: Huffman Encoding
    printf("Performing Huffman Encoding...\n");
    // Allocate a large buffer for the output. A safe bet is width*height, but could be smaller.
    encoded_data = (u8*)malloc(width * height * 3);
    if (!encoded_data) {
        printf("Failed to allocate memory for encoded data.\n");
        return 1;
    }
    encoded_size = encode(huffman_tables, s8_blocks, encoded_data, block_count);
    printf("Encoded data size: %d bytes\n", encoded_size);

    // STEP 7: Write to JPG file
    printf("Writing JPEG file: %s\n", outfile);
    get_jpg(outfile, width, height, encoded_data, encoded_size);

    // Cleanup
    free(encoded_data);
    printf("Conversion complete.\n");

    return 0;
}







	





	
	

