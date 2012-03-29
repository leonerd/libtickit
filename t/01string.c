#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TickitStringPos pos, limit;

  plan_tests(20);

  is_int(tickit_string_count("hello", &pos, NULL), 5, "tickit_string_count ASCII");
  is_int(pos.bytes,     5, "tickit_string_count ASCII bytes");
  is_int(pos.chars,     5, "tickit_string_count ASCII chars");
  is_int(pos.graphemes, 5, "tickit_string_count ASCII graphemes");
  is_int(pos.columns,   5, "tickit_string_count ASCII columns");

  /* U+00E9 - LATIN SMALL LETTER E WITH ACUTE
   * 0xc3 0xa9
   */
  is_int(tickit_string_count("caf\xc3\xa9", &pos, NULL), 5, "tickit_string_count UTF-8");
  is_int(pos.bytes,     5, "tickit_string_count UTF-8 bytes");
  is_int(pos.chars,     4, "tickit_string_count UTF-8 chars");
  is_int(pos.graphemes, 4, "tickit_string_count UTF-8 graphemes");
  is_int(pos.columns,   4, "tickit_string_count UTF-8 columns");

  /* U+0301 - COMBINING ACUTE ACCENT
   * 0xcc 0x81
   */
  is_int(tickit_string_count("cafe\xcc\x81", &pos, NULL), 6, "tickit_string_count UTF-8 combining");
  is_int(pos.bytes,     6, "tickit_string_count UTF-8 combining bytes");
  is_int(pos.chars,     5, "tickit_string_count UTF-8 combining chars");
  is_int(pos.graphemes, 4, "tickit_string_count UTF-8 combining graphemes");
  is_int(pos.columns,   4, "tickit_string_count UTF-8 combining columns");

  /* U+FF21 - FULLWIDTH LATIN CAPITAL LETTER A
   * 0xef 0xbc 0xa1
   */
  is_int(tickit_string_count("\xef\xbc\xa1", &pos, NULL), 3, "tickit_string_count UTF-8 fullwidth");
  is_int(pos.bytes,     3, "tickit_string_count UTF-8 fullwidth bytes");
  is_int(pos.chars,     1, "tickit_string_count UTF-8 fullwidth chars");
  is_int(pos.graphemes, 1, "tickit_string_count UTF-8 fullwidth graphemes");
  is_int(pos.columns,   2, "tickit_string_count UTF-8 fullwidth columns");

  return exit_status();
}
