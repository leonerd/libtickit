#include "tickit.h"
#include "taplib.h"

#include <string.h>

#define RECT(t,l,li,co)  (TickitRect){ .top = t, .left = l, .lines = li, .cols = co }

void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;
  strncat(buffer, bytes, len);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  char buffer[1024] = { 0 };
  int lines, cols;

  tt = tickit_term_build(&(struct TickitTermBuilder){
    .termtype  = "xterm",
    .output_func      = output,
    .output_func_user = buffer,
  });
  ok(!!tt, "tickit_term_build");

  is_str(tickit_term_get_termtype(tt), "xterm", "tickit_term_get_termtype");
  is_str(tickit_term_get_drivername(tt), "xterm", "tickit_term_get_drivername");

  is_int(tickit_term_get_output_fd(tt), -1, "tickit_term_get_output_fd");

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
  is_str_escape(buffer, "\e[C", "buffer after tickit_term_move right 1");

  buffer[0] = 0;
  tickit_term_move(tt, 0, 2);
  is_str_escape(buffer, "\e[2C", "buffer after tickit_term_move right 2");

  buffer[0] = 0;
  tickit_term_move(tt, 0, -1);
  is_str_escape(buffer, "\e[D", "buffer after tickit_term_move left 1");

  buffer[0] = 0;
  tickit_term_move(tt, 0, -2);
  is_str_escape(buffer, "\e[2D", "buffer after tickit_term_move left 2");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(3,0,7,80), 1, 0);
  is_str_escape(buffer, "\e[4;10r\e[4H\e[M\e[r", "buffer after tickit_term_scrollrect lines 3-9 1 down");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(3,0,15,80), 8, 0);
  is_str_escape(buffer, "\e[4;18r\e[4H\e[8M\e[r", "buffer after tickit_term_scrollrect lines 3-17 8 down");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(3,0,7,80), -1, 0);
  is_str_escape(buffer, "\e[4;10r\e[4H\e[L\e[r", "buffer after tickit_term_scrollrect lines 3-9 1 up");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(3,0,15,80), -8, 0);
  is_str_escape(buffer, "\e[4;18r\e[4H\e[8L\e[r", "buffer after tickit_term_scrollrect lines 3-17 8 up");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(5,0,1,80), 0, 3);
  is_str_escape(buffer, "\e[6H\e[3P", "buffer after tickit_term_scrollrect line 5 3 right");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(6,10,2,70), 0, 5);
  is_str_escape(buffer, "\e[7;11H\e[5P\e[8;11H\e[5P", "buffer after tickit_term_scrollrect lines 6-7 cols 10-80 5 right");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(5,0,1,80), 0, -3);
  is_str_escape(buffer, "\e[6H\e[3@", "buffer after tickit_term_scrollrect line 5 3 left");

  buffer[0] = 0;
  tickit_term_scrollrect(tt, RECT(6,10,2,70), 0, -5);
  is_str_escape(buffer, "\e[7;11H\e[5@\e[8;11H\e[5@", "buffer after tickit_term_scrollrect lines 6-7 cols 10-80 5 left");

  is_int(tickit_term_scrollrect(tt, RECT(3,10,5,60), 1, 0), 0, "tickit_term cannot scroll partial lines vertically");

  is_int(tickit_term_scrollrect(tt, RECT(3,10,5,60), 0, 1), 0, "tickit_term cannot scroll partial lines horizontally");

  /* Now (belatedly) respond to the DECSLRM probe to enable more scrollrect options */
  tickit_term_input_push_bytes(tt, "\e[?69;1$y", 9);

  {
    int b;
    ok(tickit_term_getctl_int(tt, tickit_termctl_lookup("xterm.cap_slrm"), &b), "tickit_term can get xterm.cap_srlm");
    ok(b, "tickit_term has xterm.cap_slrm true");
  }

  buffer[0] = 0;
  is_int(tickit_term_scrollrect(tt, RECT(3,10,5,60), 1, 0), 1, "tickit_term can scroll partial lines vertically with DECSLRM enabled");
  is_str_escape(buffer, "\e[4;8r\e[11;70s\e[4;11H\e[M\e[r\e[s", "buffer after tickit_term_scroll lines 3-7 cols 10-69 down");

  buffer[0] = 0;
  is_int(tickit_term_scrollrect(tt, RECT(3,10,5,60), 0, 1), 1, "tickit_term can scroll partial lines horizontally with DECSLRM enabled");
  is_str_escape(buffer, "\e[4;8r\e[11;70s\e[4;11H\e['~\e[r\e[s", "buffer after tickit_term_scroll lines 3-7 cols 10-69 right");

  buffer[0] = 0;
  is_int(tickit_term_scrollrect(tt, RECT(3,10,1,60), 0, 1), 1, "tickit_term can scroll partial lines horizontally with DECSLRM enabled");
  is_str_escape(buffer, "\e[;70s\e[4;11H\e[P\e[s", "buffer after tickit_term_scroll line 3 cols 10-69 right");

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
  is_str_escape(buffer, "\e[X\e[C", "buffer after tickit_term_erasech 1 move");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 1);
  is_str_escape(buffer, "\e[3X\e[3C", "buffer after tickit_term_erasech 3 move");

  tickit_term_unref(tt);
  pass("tickit_term_unref");

  return exit_status();
}
