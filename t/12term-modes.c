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
  char buffer[1024] = { 0 };
  int value;

  tt = tickit_term_new_for_termtype("xterm");

  ok(!!tt, "tickit_term_new_for_termtype");

  tickit_term_set_output_func(tt, output, buffer);

  is_str(tickit_term_ctlname(TICKIT_TERMCTL_ALTSCREEN), "altscreen",
      "tickit_term_ctlname on TICKIT_TERMCTL_ALTSCREEN");
  is_int(tickit_term_lookup_ctl("cursorvis"), TICKIT_TERMCTL_CURSORVIS,
      "tickit_term_lookup_ctl on cursorvis");

  buffer[0] = 0;
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);

  is_str_escape(buffer, "\e[?1049h", "buffer after set_mode_altscreen on");

  tickit_term_getctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, &value);
  is_int(value, 1, "get_mode_altscreen returns value");

  buffer[0] = 0;
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);

  is_str_escape(buffer, "", "set_mode_altscreen a second time is idempotent");

  buffer[0] = 0;
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);

  is_str_escape(buffer, "\e[?25l", "buffer after set_mode_cursorvis off");

  buffer[0] = 0;
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_CLICK);

  is_str_escape(buffer, "\e[?1000h\e[?1006h", "buffer after set_mode_mouse to click");

  buffer[0] = 0;
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_DRAG);

  is_str_escape(buffer, "\e[?1002h\e[?1006h", "buffer after set_mode_mouse to drag");

  buffer[0] = 0;
  tickit_term_setctl_str(tt, TICKIT_TERMCTL_TITLE_TEXT, "title here");

  is_str_escape(buffer, "\e]2;title here\e\\", "buffer after set title");

  buffer[0] = 0;
  tickit_term_unref(tt);

  ok(1, "tickit_term_unref");

  is_str_escape(buffer, "\e[?1002l\e[?1006l\e[?25h\e[?1049l\e[m", "buffer after termkey_term_unref resets modes");

  return exit_status();
}
