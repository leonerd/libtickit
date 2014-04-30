#include "tickit.h"

#include <stdint.h>

#include "unicode.h"

static int next_utf8(const char *str, size_t len, uint32_t *cp)
{
  unsigned char b0 = (str++)[0];
  int nbytes;

  if(!len)
    return -1;

  if(!b0)
    return -1;
  else if(b0 < 0x80) { // ASCII
    *cp = b0; return 1;
  }
  else if(b0 < 0xc0) // C1 or continuation
    return -1;
  else if(b0 < 0xe0) {
    nbytes = 2; *cp = b0 & 0x1f;
  }
  else if(b0 < 0xf0) {
    nbytes = 3; *cp = b0 & 0x0f;
  }
  else if(b0 < 0xf8) {
    nbytes = 4; *cp = b0 & 0x07;
  }
  else
    return -1;

  if(len < nbytes)
    return -1;

  for(int i = 1; i < nbytes; i++) {
    b0 = (str++)[0];
    if(!b0)
      return -1;

    *cp <<= 6;
    *cp |= b0 & 0x3f;
  }

  return nbytes;
}

int tickit_string_seqlen(long codepoint)
{
  if(codepoint < 0x0000080) return 1;
  if(codepoint < 0x0000800) return 2;
  if(codepoint < 0x0010000) return 3;
  if(codepoint < 0x0200000) return 4;
  if(codepoint < 0x4000000) return 5;
  return 6;
}

size_t tickit_string_putchar(char *str, size_t len, long codepoint)
{
  int nbytes = tickit_string_seqlen(codepoint);
  if(len < nbytes)
    return -1;

  // This is easier done backwards
  int b = nbytes;
  while(b > 1) {
    b--;
    str[b] = 0x80 | (codepoint & 0x3f);
    codepoint >>= 6;
  }

  switch(nbytes) {
    case 1: str[0] =        (codepoint & 0x7f); break;
    case 2: str[0] = 0xc0 | (codepoint & 0x1f); break;
    case 3: str[0] = 0xe0 | (codepoint & 0x0f); break;
    case 4: str[0] = 0xf0 | (codepoint & 0x07); break;
    case 5: str[0] = 0xf8 | (codepoint & 0x03); break;
    case 6: str[0] = 0xfc | (codepoint & 0x01); break;
  }

  return nbytes;
}

size_t tickit_string_count(const char *str, TickitStringPos *pos, const TickitStringPos *limit)
{
  tickit_stringpos_zero(pos);
  return tickit_string_ncountmore(str, (size_t)-1, pos, limit);
}

size_t tickit_string_countmore(const char *str, TickitStringPos *pos, const TickitStringPos *limit)
{
  return tickit_string_ncountmore(str, (size_t)-1, pos, limit);
}

size_t tickit_string_ncount(const char *str, size_t len, TickitStringPos *pos, const TickitStringPos *limit)
{
  tickit_stringpos_zero(pos);
  return tickit_string_ncountmore(str, len, pos, limit);
}

size_t tickit_string_ncountmore(const char *str, size_t len, TickitStringPos *pos, const TickitStringPos *limit)
{
  TickitStringPos here = *pos;
  size_t start_bytes = pos->bytes;

  str += pos->bytes;
  if(len != (size_t)-1)
    len -= pos->bytes;

  while((len == (size_t)-1) ? *str : (len > 0)) {
    uint32_t cp;
    int bytes = next_utf8(str, len, &cp);
    if(bytes == -1)
      return -1;

    /* Abort on C0 or C1 controls */
    if(cp < 0x20 || (cp >= 0x80 && cp < 0xa0))
      return -1;

    int width = mk_wcwidth(cp);
    if(width == -1)
      return -1;

    int is_grapheme = (width > 0) ? 1 : 0;
    if(is_grapheme) // Commit on the previous grapheme
      *pos = here;

    if(limit && limit->bytes != -1 && here.bytes + bytes > limit->bytes)
      break;
    if(limit && limit->codepoints != -1 && here.codepoints + 1 > limit->codepoints)
      break;
    if(limit && limit->graphemes != -1 && here.graphemes + is_grapheme > limit->graphemes)
      break;
    if(limit && limit->columns != -1 && here.columns + width > limit->columns)
      break;

    str += bytes;
    if(len != (size_t)-1)
      len -= bytes;

    here.bytes += bytes;
    here.codepoints += 1;
    here.graphemes += is_grapheme;
    here.columns += width;
  }

  if(len == 0 || *str == 0) // Commit on the final grapheme
    *pos = here;

  return pos->bytes - start_bytes;
}

int tickit_string_mbswidth(const char *str)
{
  TickitStringPos pos;
  tickit_string_count(str, &pos, NULL);
  return pos.columns;
}

int tickit_string_byte2col(const char *str, size_t byte)
{
  TickitStringPos limit = INIT_TICKIT_STRINGPOS_LIMIT_BYTES(byte);
  TickitStringPos pos;
  tickit_string_count(str, &pos, &limit);
  return pos.columns;
}

size_t tickit_string_col2byte(const char *str, int col)
{
  TickitStringPos limit = INIT_TICKIT_STRINGPOS_LIMIT_COLUMNS(col);
  TickitStringPos pos;
  tickit_string_count(str, &pos, &limit);
  return pos.bytes;
}
