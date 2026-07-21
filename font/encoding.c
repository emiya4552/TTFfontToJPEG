#include"encoding.h"


#include<stddef.h>
#include<stdlib.h>
#include<string.h>

#define WORD_BUFFER_LENGTH	8

#ifdef _WIN32
#include<windows.h>

#define GBK_CODE_PAGE		936
#define WIDE_BUFFER_LENGTH	3
#else
#include<iconv.h>
#include<locale.h>
#include<wchar.h>

#define UTF8_BUFFER_LENGTH	8
#endif


// 功能：检查单字符转换的输入参数
static int check_input(const char* word, unicode_t* unicode)
{
	if(word == NULL || unicode == NULL || word[0] == '\0'){
		return -1;
	}

	return 0;
}


// 功能：根据 UTF-8 首字节判断字符字节数
static int get_utf8_byte_length(const char* string)
{
	const unsigned char*	data;

	data = (const unsigned char*)string;
	if(data[0] < 0x80){
		return 1;
	}
	if((data[0]&0xE0) == 0xC0){
		return 2;
	}
	if((data[0]&0xF0) == 0xE0){
		return 3;
	}
	if((data[0]&0xF8) == 0xF0){
		return 4;
	}

	return -1;
}


// 功能：根据 GBK 首字节判断字符字节数
static int get_gbk_byte_length(const char* string)
{
	const unsigned char*	data;

	data = (const unsigned char*)string;
	if(data[0] < 0x80){
		return 1;
	}
	if(data[0] >= 0x81 && data[0] <= 0xFE){
		return 2;
	}

	return -1;
}


#ifdef _WIN32
// 功能：获取 Windows 当前系统编码下的首字符字节数
static int get_system_byte_length(const char* string)
{
	unsigned int	code_page;

	code_page = GetACP();
	if(code_page == CP_UTF8){
		return get_utf8_byte_length(string);
	}
	if(IsDBCSLeadByteEx(code_page, (unsigned char)string[0]) != 0){
		return 2;
	}

	return 1;
}
#else
// 功能：获取当前 Linux Locale 下的首字符字节数
static int get_system_byte_length(const char* string)
{
	mbstate_t	state;
	wchar_t		wide;
	size_t		length;

	if(setlocale(LC_CTYPE, "") == NULL){
		return -1;
	}

	memset(&state, 0, sizeof(state));
	length = mbrtowc(&wide, string, MB_CUR_MAX, &state);
	if(length == (size_t)-1 || length == (size_t)-2 || length == 0){
		return -1;
	}

	return (int)length;
}
#endif


// 功能：将单个 UTF-8 字符转换为 Unicode
static int utf8_to_unicode(const char* word, unicode_t* unicode)
{
	const unsigned char*	data;
	unicode_t		result;
	size_t			word_length;
	int			length;
	int			i;

	data	= (const unsigned char*)word;
	result	= 0;
	word_length	= strlen(word);
	length	= 0;

	// 根据首字节解析 UTF-8 字符长度和初始值
	if(data[0] < 0x80){
		length	= 1;
		result	= data[0];
	}else if((data[0]&0xE0) == 0xC0){
		length	= 2;
		result	= data[0]&0x1F;
	}else if((data[0]&0xF0) == 0xE0){
		length	= 3;
		result	= data[0]&0x0F;
	}else if((data[0]&0xF8) == 0xF0){
		length	= 4;
		result	= data[0]&0x07;
	}else{
		return -2;
	}

	if(word_length < (size_t)length){
		return -2;
	}
	if(word_length > (size_t)length){
		return -3;
	}

	// 合并 UTF-8 后续字节中的有效位
	for(i=1; i<length; i++){
		if((data[i]&0xC0) != 0x80){
			return -2;
		}
		result	= (result<<6)|(data[i]&0x3F);
	}

	// 排除过长编码、代理项和超出范围的码点
	if((length == 2 && result < 0x80) ||
	   (length == 3 && result < 0x800) ||
	   (length == 4 && result < 0x10000) ||
	   (result >= 0xD800 && result <= 0xDFFF) ||
	   result > 0x10FFFF){
		return -4;
	}

	*unicode = result;

	return 0;
}


#ifdef _WIN32
// 功能：通过 Windows 代码页将单个字符转换为 Unicode
static int windows_to_unicode(const char* word, unsigned int code_page, unicode_t* unicode)
{
	wchar_t	wide[WIDE_BUFFER_LENGTH];
	int	flags;
	int	length;

	flags	= (code_page == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0;
	length	= MultiByteToWideChar(
			code_page,
			flags,
			word,
			-1,
			wide,
			WIDE_BUFFER_LENGTH
		);

	// 处理基本多文种平面中的单个字符
	if(length == 2){
		if(wide[0] >= 0xD800 && wide[0] <= 0xDFFF){
			return -2;
		}
		*unicode = (unicode_t)wide[0];
		return 0;
	}

	// 合并辅助平面字符的 UTF-16 代理对
	if(length == 3 &&
	   wide[0] >= 0xD800 && wide[0] <= 0xDBFF &&
	   wide[1] >= 0xDC00 && wide[1] <= 0xDFFF){
		*unicode = 0x10000 +
			(((unicode_t)wide[0]-0xD800)<<10) +
			((unicode_t)wide[1]-0xDC00);
		return 0;
	}

	return -2;
}
#else
// 功能：通过当前 Linux Locale 将单个字符转换为 Unicode
static int locale_to_unicode(const char* word, unicode_t* unicode)
{
	mbstate_t	state;
	wchar_t		wide;
	size_t		length;

	if(setlocale(LC_CTYPE, "") == NULL){
		return -2;
	}

	memset(&state, 0, sizeof(state));
	length	= mbrtowc(&wide, word, MB_CUR_MAX, &state);

	if(length == (size_t)-1 || length == (size_t)-2 || length == 0){
		return -2;
	}
	if(word[length] != '\0'){
		return -3;
	}

	if((unicode_t)wide >= 0xD800 && (unicode_t)wide <= 0xDFFF){
		return -2;
	}
	if((unicode_t)wide > 0x10FFFF){
		return -2;
	}

	*unicode = (unicode_t)wide;

	return 0;
}


// 功能：通过 iconv 将单个 GBK 字符转换为 Unicode
static int gbk_to_unicode(const char* word, unicode_t* unicode)
{
	iconv_t	converter;
	char	utf8[UTF8_BUFFER_LENGTH];
	char*	in_pointer;
	char*	out_pointer;
	size_t	in_length;
	size_t	out_length;

	converter	= iconv_open("UTF-8", "GBK");
	if(converter == (iconv_t)-1){
		return -2;
	}

	in_pointer	= (char*)word;
	out_pointer	= utf8;
	in_length	= strlen(word);
	out_length	= sizeof(utf8)-1;

	if(iconv(converter, &in_pointer, &in_length, &out_pointer, &out_length) == (size_t)-1){
		iconv_close(converter);
		return -2;
	}
	iconv_close(converter);

	*out_pointer = '\0';

	return utf8_to_unicode(utf8, unicode);
}
#endif


// 功能：将指定编码的单个字符转换为 Unicode
int word_to_unicode(const char* word, word_encoding encoding, unicode_t* unicode)
{
	if(check_input(word, unicode) != 0){
		return -1;
	}

	// 根据调用方指定的编码选择对应转换方式
	switch(encoding){
	case WORD_ENCODING_UTF8:
		return utf8_to_unicode(word, unicode);
	case WORD_ENCODING_GBK:
#ifdef _WIN32
		return windows_to_unicode(word, GBK_CODE_PAGE, unicode);
#else
		return gbk_to_unicode(word, unicode);
#endif
	case WORD_ENCODING_SYSTEM:
#ifdef _WIN32
		return windows_to_unicode(word, GetACP(), unicode);
#else
		return locale_to_unicode(word, unicode);
#endif
	default:
		return -1;
	}
}


// 功能：获取字符串首字符在指定编码下占用的字节数
int get_character_byte_length(const char* string, word_encoding encoding)
{
	char		word[WORD_BUFFER_LENGTH];
	unicode_t	unicode;
	size_t		string_length;
	int		length;

	if(string == NULL || string[0] == '\0'){
		return -1;
	}

	// 先确定候选字节数，再调用单字符转换函数进行完整校验
	switch(encoding){
	case WORD_ENCODING_UTF8:
		length = get_utf8_byte_length(string);
		break;
	case WORD_ENCODING_GBK:
		length = get_gbk_byte_length(string);
		break;
	case WORD_ENCODING_SYSTEM:
		length = get_system_byte_length(string);
		break;
	default:
		return -1;
	}

	string_length = strlen(string);
	if(length <= 0 || length >= WORD_BUFFER_LENGTH || string_length < (size_t)length){
		return -1;
	}

	memcpy(word, string, length);
	word[length] = '\0';
	if(word_to_unicode(word, encoding, &unicode) != 0){
		return -1;
	}

	return length;
}
