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
  TickitPen *pen;

  plan_tests(11);

  tt = tickit_term_new_for_termtype("xterm");
  tickit_term_set_output_func(tt, output, buffer);

  pen = tickit_pen_new();

  buffer[0] = 0;
  tickit_term_setpen(tt, pen);

  is_str_escape(buffer, "\e[m", "buffer after chpen empty");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 1);

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "\e[1m", "buffer contains SGR 1 for chpen bold");

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "", "chpen again is a no-op");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 0);

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "\e[m", "chpen disables bold");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 1);
  tickit_pen_set_bool_attr(pen, TICKIT_PEN_UNDER, 1);

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "\e[1;4m", "chpen enables bold and under");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 0);
  tickit_pen_clear_attr(pen, TICKIT_PEN_UNDER);

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "\e[22m", "chpen disables bold");

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "", "chpen disable bold again is no-op");

  tickit_pen_clear_attr(pen, TICKIT_PEN_BOLD);
  tickit_pen_set_bool_attr(pen, TICKIT_PEN_UNDER, 0);

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "\e[m", "chpen disable under is reset");

  tickit_pen_clear_attr(pen, TICKIT_PEN_UNDER);

  tickit_pen_set_int_attr(pen, TICKIT_PEN_FG, 1);
  tickit_pen_set_int_attr(pen, TICKIT_PEN_BG, 5);

  buffer[0] = 0;
  tickit_term_setpen(tt, pen);

  is_str_escape(buffer, "\e[31;45m", "chpen foreground+background");

  tickit_pen_set_int_attr(pen, TICKIT_PEN_FG, 9);
  tickit_pen_clear_attr(pen, TICKIT_PEN_BG);

  buffer[0] = 0;
  tickit_term_chpen(tt, pen);

  is_str_escape(buffer, "\e[91m", "chpen foreground high");

  tickit_pen_clear_attr(pen, TICKIT_PEN_FG);
  tickit_pen_set_bool_attr(pen, TICKIT_PEN_UNDER, 1);

  buffer[0] = 0;
  tickit_term_setpen(tt, pen);

  is_str_escape(buffer, "\e[39;49;4m", "setpen resets colours, enables under");

  tickit_term_destroy(tt);
  return exit_status();
}
