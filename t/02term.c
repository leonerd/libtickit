#include "tickit.h"
#include "taplib.h"

#include <string.h>

void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;
  strncat(buffer, bytes, len);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  char buffer[1024];
  int lines, cols;

  plan_tests(36);

  tt = tickit_term_new_for_termtype("xterm");
  ok(!!tt, "tickit_term_new_for_termtype");

  tickit_term_set_output_func(tt, output, buffer);

  tickit_term_set_size(tt, 24, 80);

  tickit_term_get_size(tt, &lines, &cols);
  is_int(lines, 24, "get_size lines");
  is_int(cols,  80, "get_size cols");

  buffer[0] = 0;
  tickit_term_print(tt, "Hello world!");
  is_str_escape(buffer, "Hello world!", "buffer after tickit_term_print");

  buffer[0] = 0;
  tickit_term_goto(tt, 2, 5);
  is_str_escape(buffer, "\e[3;6H", "buffer after tickit_term_goto line+col");

  buffer[0] = 0;
  tickit_term_goto(tt, 3, 0);
  is_str_escape(buffer, "\e[4H", "buffer after tickit_term_goto line+col0");

  buffer[0] = 0;
  tickit_term_goto(tt, 4, -1);
  is_str_escape(buffer, "\e[5d", "buffer after tickit_term_goto line");

  buffer[0] = 0;
  tickit_term_goto(tt, -1, 10);
  is_str_escape(buffer, "\e[11G", "buffer after tickit_term_goto col");

  buffer[0] = 0;
  tickit_term_goto(tt, -1, 0);
  is_str_escape(buffer, "\e[G", "buffer after tickit_term_goto col0");

  buffer[0] = 0;
  tickit_term_move(tt, 1, 0);
  is_str_escape(buffer, "\e[B", "buffer after tickit_term_move down 1");

  buffer[0] = 0;
  tickit_term_move(tt, 2, 0);
  is_str_escape(buffer, "\e[2B", "buffer after tickit_term_move down 2");

  buffer[0] = 0;
  tickit_term_move(tt, -1, 0);
  is_str_escape(buffer, "\e[A", "buffer after tickit_term_move down 1");

  buffer[0] = 0;
  tickit_term_move(tt, -2, 0);
  is_str_escape(buffer, "\e[2A", "buffer after tickit_term_move down 2");

  buffer[0] = 0;
  tickit_term_move(tt, 0, 1);
  is_str_escape(buffer, "\e[D", "buffer after tickit_term_move right 1");

  buffer[0] = 0;
  tickit_term_move(tt, 0, 2);
  is_str_escape(buffer, "\e[2D", "buffer after tickit_term_move right 2");

  buffer[0] = 0;
  tickit_term_move(tt, 0, -1);
  is_str_escape(buffer, "\e[C", "buffer after tickit_term_move left 1");

  buffer[0] = 0;
  tickit_term_move(tt, 0, -2);
  is_str_escape(buffer, "\e[2C", "buffer after tickit_term_move left 2");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 3, 0, 7, 80, 2, 0);
  is_str_escape(buffer, "\e[4;10r\e[10d\n\n\e[r", "buffer after tickit_term_scroll lines 3-9 2 down");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 3, 0, 15, 80, 8, 0);
  is_str_escape(buffer, "\e[4;18r\e[18d\e[8T\e[r", "buffer after tickit_term_scroll lines 3-17 8 down");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 3, 0, 7, 80, -2, 0);
  is_str_escape(buffer, "\e[4;10r\e[4d\eM\eM\e[r", "buffer after tickit_term_scroll lines 3-9 2 up");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 3, 0, 15, 80, -8, 0);
  is_str_escape(buffer, "\e[4;18r\e[4d\e[8S\e[r", "buffer after tickit_term_scroll lines 3-17 8 up");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 5, 0, 1, 80, 0, 3);
  is_str_escape(buffer, "\e[6H\e[3@", "buffer after tickit_term_scrollrect line 5 3 right");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 6, 10, 2, 70, 0, 5);
  is_str_escape(buffer, "\e[7;11H\e[5@\e[8;11H\e[5@", "buffer after tickit_term_scrollrect lines 6-7 cols 10-80 5 right");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 5, 0, 1, 80, 0, -3);
  is_str_escape(buffer, "\e[6H\e[3P", "buffer after tickit_term_scrollrect line 5 3 left");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, 6, 10, 2, 70, 0, -5);
  is_str_escape(buffer, "\e[7;11H\e[5P\e[8;11H\e[5P", "buffer after tickit_term_scrollrect lines 6-7 cols 10-80 5 left");

  buffer[0] = 0;
  tickit_term_clear(tt);
  is_str_escape(buffer, "\e[2J", "buffer after tickit_term_clear");

  buffer[0] = 0;
  tickit_term_erasech(tt, 1, 0);
  is_str_escape(buffer, "\e[X", "buffer after tickit_term_erasech 1 nomove");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 0);
  is_str_escape(buffer, "\e[3X", "buffer after tickit_term_erasech 3 nomove");

  buffer[0] = 0;
  tickit_term_erasech(tt, 1, 1);
  is_str_escape(buffer, "\e[X\e[D", "buffer after tickit_term_erasech 1 move");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 1);
  is_str_escape(buffer, "\e[3X\e[3D", "buffer after tickit_term_erasech 3 move");

  tickit_term_destroy(tt);
  ok(1, "tickit_term_destroy");

  /* Test erasech without bce */
  tt = tickit_term_new_for_termtype("screen");
  tickit_term_set_output_func(tt, output, buffer);

  buffer[0] = 0;
  tickit_term_erasech(tt, 1, 0);
  is_str_escape(buffer, " \e[C", "buffer after tickit_term_erasech 1 nomove");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 0);
  is_str_escape(buffer, "   \e[3C", "buffer after tickit_term_erasech 3 nomove");

  buffer[0] = 0;
  tickit_term_erasech(tt, 1, 1);
  is_str_escape(buffer, " ", "buffer after tickit_term_erasech 1 move");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 1);
  is_str_escape(buffer, "   ", "buffer after tickit_term_erasech 3 move");

  /* also test write_str_rep() */
  buffer[0] = 0;
  tickit_term_erasech(tt, 10, 1);

  is_str_escape(buffer, "          ", "buffer after tickit_term_erasech 10 move");

  tickit_term_destroy(tt);

  return exit_status();
}
