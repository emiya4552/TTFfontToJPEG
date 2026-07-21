#include "../font/encoding.h"
#include<stdio.h>
#include<string.h>

// 功能：验证单字符转换和多编码字符串拆分
int main(int argc, char* argv[])
{
	char	utf8_word[] = "\xE4\xBF\x9D";
	char	gbk_word[] = "\xC2\xED";
	char	gbk_words[] = "\xC2\xED\xB1\xA3";
	char	words[] = "\xE4\xBF\x9D\xE5\xAD\x97";
	char	broken_utf8[] = "\xE4\xBF";
	unicode_t	unicode;

	// 验证 UTF-8、GBK 单字符转换和多字符拒绝行为
	if(word_to_unicode(utf8_word, WORD_ENCODING_UTF8, &unicode) != 0 || unicode != 0x4FDD){
		fprintf(stderr, "single character decode failed\n");
		return 1;
	}

	if(word_to_unicode(gbk_word, WORD_ENCODING_GBK, &unicode) != 0 || unicode != 0x9A6C){
		fprintf(stderr, "GBK character decode failed\n");
		return 2;
	}

	if(word_to_unicode(words, WORD_ENCODING_UTF8, &unicode) == 0){
		fprintf(stderr, "multiple characters should be rejected\n");
		return 3;
	}

	if(word_to_unicode(broken_utf8, WORD_ENCODING_UTF8, &unicode) == 0){
		fprintf(stderr, "incomplete UTF-8 should be rejected\n");
		return 4;
	}

	// 验证 UTF-8 与 GBK 字符串能够按字符边界拆分
	if(get_character_byte_length(words, WORD_ENCODING_UTF8) != 3 ||
	   get_character_byte_length(words+3, WORD_ENCODING_UTF8) != 3){
		fprintf(stderr, "UTF-8 string split failed\n");
		return 5;
	}
	if(get_character_byte_length(gbk_words, WORD_ENCODING_GBK) != 2 ||
	   get_character_byte_length(gbk_words+2, WORD_ENCODING_GBK) != 2){
		fprintf(stderr, "GBK string split failed\n");
		return 6;
	}

	// 传入命令行字符时额外验证当前系统编码
	if(argc == 2){
		if(word_to_unicode(argv[1], WORD_ENCODING_SYSTEM, &unicode) != 0 || unicode != 0x9A6C){
			fprintf(stderr, "system character decode failed\n");
			return 7;
		}
		if(get_character_byte_length(argv[1], WORD_ENCODING_SYSTEM) != (int)strlen(argv[1])){
			fprintf(stderr, "system string split failed\n");
			return 8;
		}
	}

	return 0;
}
