#include "tickit.h"
#include "taplib.h"

#include <string.h>

void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;
  strcat(buffer, bytes);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  char buffer[1024];

  plan_tests(19);

  tt = tickit_term_new();

  ok(!!tt, "tickit_term_new");

  tickit_term_set_output_func(tt, output, buffer);

  buffer[0] = 0;
  tickit_term_print(tt, "Hello world!");

  is_str(buffer, "Hello world!", "buffer after tickit_term_print");

  buffer[0] = 0;
  tickit_term_goto(tt, 2, 5);

  is_str(buffer, "\e[3;6H", "buffer after tickit_term_goto line+col");

  buffer[0] = 0;
  tickit_term_goto(tt, 4, -1);

  is_str(buffer, "\e[5H", "buffer after tickit_term_goto line");

  buffer[0] = 0;
  tickit_term_goto(tt, -1, 10);

  is_str(buffer, "\e[11G", "buffer after tickit_term_goto col");

  buffer[0] = 0;
  tickit_term_move(tt, 1, 0);

  is_str(buffer, "\e[B", "buffer after tickit_term_move down 1");

  buffer[0] = 0;
  tickit_term_move(tt, 2, 0);

  is_str(buffer, "\e[2B", "buffer after tickit_term_move down 2");

  buffer[0] = 0;
  tickit_term_move(tt, -1, 0);

  is_str(buffer, "\e[A", "buffer after tickit_term_move down 1");

  buffer[0] = 0;
  tickit_term_move(tt, -2, 0);

  is_str(buffer, "\e[2A", "buffer after tickit_term_move down 2");

  buffer[0] = 0;
  tickit_term_move(tt, 0, 1);

  is_str(buffer, "\e[D", "buffer after tickit_term_move right 1");

  buffer[0] = 0;
  tickit_term_move(tt, 0, 2);

  is_str(buffer, "\e[2D", "buffer after tickit_term_move right 2");

  buffer[0] = 0;
  tickit_term_move(tt, 0, -1);

  is_str(buffer, "\e[C", "buffer after tickit_term_move left 1");

  buffer[0] = 0;
  tickit_term_move(tt, 0, -2);

  is_str(buffer, "\e[2C", "buffer after tickit_term_move left 2");

  buffer[0] = 0;
  tickit_term_clear(tt);

  is_str(buffer, "\e[2J", "buffer after tickit_term_clear");

  buffer[0] = 0;
  tickit_term_insertch(tt, 1);

  is_str(buffer, "\e[@", "buffer after tickit_term_insertch 1");

  buffer[0] = 0;
  tickit_term_insertch(tt, 5);

  is_str(buffer, "\e[5@", "buffer after tickit_term_insertch 5");

  buffer[0] = 0;
  tickit_term_deletech(tt, 1);

  is_str(buffer, "\e[P", "buffer after tickit_term_deletech 1");

  buffer[0] = 0;
  tickit_term_deletech(tt, 5);

  is_str(buffer, "\e[5P", "buffer after tickit_term_deletech 5");

  tickit_term_free(tt);

  ok(1, "tickit_term_free");

  return exit_status();
}
