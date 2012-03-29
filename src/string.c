#include "tickit.h"

#include <stdint.h>

#include "../src/unicode.inc"

static int next_utf8(const char *str, uint32_t *cp)
{
  unsigned char b0 = (str++)[0];
  int nbytes;

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

  for(int i = 1; i < nbytes; i++) {
    b0 = (str++)[0];
    if(!b0)
      return -1;

    *cp <<= 6;
    *cp |= b0 & 0x3f;
  }

  return nbytes;
}

size_t tickit_string_count(const char *str, TickitStringPos *pos, const TickitStringPos *limit)
{
  pos->bytes = 0;
  pos->chars = 0;
  pos->graphemes = 0;
  pos->columns = 0;
  return tickit_string_countmore(str, pos, limit);
}

size_t tickit_string_countmore(const char *str, TickitStringPos *pos, const TickitStringPos *limit)
{
  while(str[pos->bytes]) {
    uint32_t cp;
    int bytes = next_utf8(str + pos->bytes, &cp);
    if(bytes == -1)
      return -1;

    /* Abort on C0 or C1 controls */
    if(cp < 0x20 || (cp >= 0x80 && cp < 0xa0))
      return -1;

    int width = mk_wcwidth(cp);
    if(width == -1)
      return -1;

    pos->bytes += bytes;
    pos->chars += 1;
    if(width)
      pos->graphemes += 1;
    pos->columns += width;;
  }

  return pos->bytes;
}
