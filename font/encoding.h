#ifndef ENCODING_H
#define ENCODING_H

#include<stdint.h>

typedef uint32_t unicode_t;

typedef enum{
	WORD_ENCODING_UTF8,
	WORD_ENCODING_GBK,
	WORD_ENCODING_SYSTEM
}word_encoding;

// 功能：将指定编码的单个字符转换为 Unicode
int word_to_unicode(const char* word, word_encoding encoding, unicode_t* unicode);

// 功能：获取字符串首字符在指定编码下占用的字节数
int get_character_byte_length(const char* string, word_encoding encoding);

#endif
