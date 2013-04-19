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

  plan_tests(5);

  tt = tickit_term_new_for_termtype("screen");

  tickit_term_set_output_func(tt, output, buffer);

  buffer[0] = 0;
  tickit_term_erasech(tt, 1, 0);
  is_str_escape(buffer, " \e[D", "buffer after tickit_term_erasech 1 nomove");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 0);
  is_str_escape(buffer, "   \e[3D", "buffer after tickit_term_erasech 3 nomove");

  buffer[0] = 0;
  tickit_term_erasech(tt, 1, 1);
  is_str_escape(buffer, " ", "buffer after tickit_term_erasech 1 move");

  buffer[0] = 0;
  tickit_term_erasech(tt, 3, 1);
  is_str_escape(buffer, "   ", "buffer after tickit_term_erasech 3 move");

  buffer[0] = 0;
  tickit_term_erasech(tt, 10, 1);

  is_str_escape(buffer, "          ", "buffer after tickit_term_erasech 10 move");

  tickit_term_destroy(tt);

  return exit_status();
}
