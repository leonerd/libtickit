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

  plan_tests(7);

  tt = tickit_term_new_for_termtype("xterm");

  ok(!!tt, "tickit_term_new_for_termtype");

  tickit_term_set_output_func(tt, output, buffer);

  buffer[0] = 0;
  tickit_term_set_mode_altscreen(tt, 1);

  is_str(buffer, "\e[?1049h", "buffer after set_mode_altscreen on");

  buffer[0] = 0;
  tickit_term_set_mode_altscreen(tt, 1);

  is_str(buffer, "", "set_mode_altscreen a second time is idempotent");

  buffer[0] = 0;
  tickit_term_set_mode_cursorvis(tt, 0);

  is_str(buffer, "\e[?25l", "buffer after set_mode_cursorvis off");

  buffer[0] = 0;
  tickit_term_set_mode_mouse(tt, 1);

  is_str(buffer, "\e[?1002h", "buffer after set_mode_mouse on");

  buffer[0] = 0;
  tickit_term_destroy(tt);

  ok(1, "tickit_term_destroy");

  is_str(buffer, "\e[?1002l\e[?25h\e[?1049l", "buffer after termkey_term_destroy resets modes");

  return exit_status();
}
