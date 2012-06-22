#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TickitStringPos pos, limit;

  plan_tests(48);

  is_int(tickit_string_count("hello", &pos, NULL), 5, "tickit_string_count ASCII");
  is_int(pos.bytes,      5, "tickit_string_count ASCII bytes");
  is_int(pos.codepoints, 5, "tickit_string_count ASCII codepoints");
  is_int(pos.graphemes,  5, "tickit_string_count ASCII graphemes");
  is_int(pos.columns,    5, "tickit_string_count ASCII columns");

  /* U+00E9 - LATIN SMALL LETTER E WITH ACUTE
   * 0xc3 0xa9
   */
  is_int(tickit_string_count("caf\xc3\xa9", &pos, NULL), 5, "tickit_string_count UTF-8");
  is_int(pos.bytes,      5, "tickit_string_count UTF-8 bytes");
  is_int(pos.codepoints, 4, "tickit_string_count UTF-8 codepoints");
  is_int(pos.graphemes,  4, "tickit_string_count UTF-8 graphemes");
  is_int(pos.columns,    4, "tickit_string_count UTF-8 columns");

  /* U+0301 - COMBINING ACUTE ACCENT
   * 0xcc 0x81
   */
  is_int(tickit_string_count("cafe\xcc\x81", &pos, NULL), 6, "tickit_string_count UTF-8 combining");
  is_int(pos.bytes,      6, "tickit_string_count UTF-8 combining bytes");
  is_int(pos.codepoints, 5, "tickit_string_count UTF-8 combining codepoints");
  is_int(pos.graphemes,  4, "tickit_string_count UTF-8 combining graphemes");
  is_int(pos.columns,    4, "tickit_string_count UTF-8 combining columns");

  /* U+FF21 - FULLWIDTH LATIN CAPITAL LETTER A
   * 0xef 0xbc 0xa1
   */
  is_int(tickit_string_count("\xef\xbc\xa1", &pos, NULL), 3, "tickit_string_count UTF-8 fullwidth");
  is_int(pos.bytes,      3, "tickit_string_count UTF-8 fullwidth bytes");
  is_int(pos.codepoints, 1, "tickit_string_count UTF-8 fullwidth codepoints");
  is_int(pos.graphemes,  1, "tickit_string_count UTF-8 fullwidth graphemes");
  is_int(pos.columns,    2, "tickit_string_count UTF-8 fullwidth columns");

  /* Now with some limits */

  tickit_stringpos_limit_bytes(&limit, 5);

  is_int(tickit_string_count("hello world", &pos, &limit), 5, "tickit_string_count byte-limit");
  is_int(pos.bytes,      5, "tickit_string_count byte-limit bytes");
  is_int(pos.codepoints, 5, "tickit_string_count byte-limit codepoints");
  is_int(pos.graphemes,  5, "tickit_string_count byte-limit graphemes");
  is_int(pos.columns,    5, "tickit_string_count byte-limit columns");

  /* check byte limit never chops UTF-8 codepoints */
  limit.bytes = 4;
  is_int(tickit_string_count("caf\xc3\xa9", &pos, &limit), 3, "tickit_string_count byte-limit split");
  is_int(pos.bytes,      3, "tickit_string_count byte-limit split bytes");

  tickit_stringpos_limit_codepoints(&limit, 3);

  is_int(tickit_string_count("hello world", &pos, &limit), 3, "tickit_string_count char-limit");
  is_int(pos.bytes,      3, "tickit_string_count char-limit bytes");
  is_int(pos.codepoints, 3, "tickit_string_count char-limit codepoints");
  is_int(pos.graphemes,  3, "tickit_string_count char-limit graphemes");
  is_int(pos.columns,    3, "tickit_string_count char-limit columns");

  /* check char limit never chops graphemes */
  limit.codepoints = 4;
  is_int(tickit_string_count("cafe\xcc\x81", &pos, &limit), 3, "tickit_string_count char-limit split");
  is_int(pos.codepoints, 3, "tickit_string_count char-limit split codepoints");

  tickit_stringpos_limit_graphemes(&limit, 4);

  is_int(tickit_string_count("hello world", &pos, &limit), 4, "tickit_string_count grapheme-limit");
  is_int(pos.bytes,      4, "tickit_string_count grapheme-limit bytes");
  is_int(pos.codepoints, 4, "tickit_string_count grapheme-limit codepoints");
  is_int(pos.graphemes,  4, "tickit_string_count grapheme-limit graphemes");
  is_int(pos.columns,    4, "tickit_string_count grapheme-limit columns");

  tickit_stringpos_limit_columns(&limit, 6);

  is_int(tickit_string_count("hello world", &pos, &limit), 6, "tickit_string_count column-limit");
  is_int(pos.bytes,      6, "tickit_string_count column-limit bytes");
  is_int(pos.codepoints, 6, "tickit_string_count column-limit codepoints");
  is_int(pos.graphemes,  6, "tickit_string_count column-limit graphemes");
  is_int(pos.columns,    6, "tickit_string_count column-limit columns");

  /* check column limit never chops graphemes */
  limit.columns = 2;
  is_int(tickit_string_count("A\xef\xbc\xa1", &pos, &limit), 1, "tickit_string_count column-limit split");
  is_int(pos.columns,    1, "tickit_string_count column-limit split grapheme");

  /* C0 and C1 controls are errors */
  tickit_stringpos_limit_bytes(&limit, -1);

  is_int(tickit_string_count("\x1b", &pos, &limit), -1, "tickit_string_count -1 for C0");
  is_int(tickit_string_count("\x9b", &pos, &limit), -1, "tickit_string_count -1 for C0");

  return exit_status();
}
