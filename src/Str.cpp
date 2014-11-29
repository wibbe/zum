
#include "Str.h"
#include "termbox.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include <algorithm>

String EMPTY_STR = { 0 };

int string_len(String str)
{
  int len = 0;
  for (; str[len] != 0 && len < STRING_LEN; ++len)
  { }

  return len;
}

void string_append(String dest, String source)
{
  int start = 0;
  while (dest[start] != 0 && start < STRING_LEN)
    start++;

  int i = 0;
  while (source[i] != 0 && start < STRING_LEN)
  {
    dest[start] = source[i];
    ++i;
    ++start;
  }

  dest[start] = 0;
}

void string_append_char(String dest, uint32_t character)
{
  int start = 0;
  while (dest[start] != 0 && start < STRING_LEN)
    start++;

  dest[start] = character;
  dest[start + 1] = 0;
}

void string_insert_char(String dest, uint32_t pos, uint32_t character)
{
  assert(pos < STRING_LEN);

  for (int i = STRING_LEN - 2; i >= 0; --i)
  {
    dest[i + 1] = dest[i];

    if (i == pos)
    {
      dest[i] = character;
      break;
    }
  }
}

void string_erase_char(String dest, uint32_t pos)
{
  for (uint32_t i = pos; i < (STRING_LEN - 1); ++i)
    dest[i] = dest[i + 1];
}

void string_copy(String dest, String source)
{
  for (int i = 0; i < STRING_LEN; ++i)
    dest[i] = source[i];
}

void string_set(String dest, const char * source)
{
  const char * sit = source;
  uint32_t * dit = dest;

  while (*sit)
  {
    tb_utf8_char_to_unicode(dit, sit);
    ++sit;
    ++dit;
  }
  *dit = 0;
}

void string_clear(String dest)
{
  for (int i = 0; i < STRING_LEN; ++i)
    dest[i] = 0;
}

void string_fmt(String dest, const char * fmt, ...)
{
  char buffer[STRING_LEN];

  va_list argp;
  va_start(argp, fmt);
  vsnprintf(buffer, STRING_LEN, fmt, argp);
  va_end(argp);

  string_set(dest, buffer);
}

void string_utf8(char * dest, String source)
{
  int i = 0;
  while (source[i] != 0)
  {
    dest += tb_utf8_unicode_to_char(dest, source[i]);
    ++i;
  }
  *dest = '\0';
}
