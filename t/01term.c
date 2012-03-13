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

  plan_tests(3);

  tt = tickit_term_new();

  ok(!!tt, "tickit_term_new");

  tickit_term_set_output_func(tt, output, buffer);

  buffer[0] = 0;

  tickit_term_print(tt, "Hello world!");

  is_str(buffer, "Hello world!", "buffer after tickit_term_print");

  tickit_term_free(tt);

  ok(1, "tickit_term_free");

  return exit_status();
}
