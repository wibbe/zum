
#ifndef STR_H
#define STR_H

#include <stdint.h>

#define STRING_LEN 512
typedef uint32_t String[STRING_LEN];

extern String EMPTY_STR;

int string_len(String str);
void string_append(String dest, String source);
void string_append_char(String dest, uint32_t character);

void string_insert_char(String dest, uint32_t pos, uint32_t character);
void string_erase_char(String dest, uint32_t pos);

void string_set(String dest, const char * source);
void string_clear(String dest);
void string_copy(String dest, String source);
void string_fmt(String dest, const char * fmt, ...);

void string_utf8(char * dest, String source);

#endif