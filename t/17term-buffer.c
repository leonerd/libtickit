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
  int    fd[2];
  char   buffer[1024] = { 0 };

  tt = tickit_term_new_for_termtype("xterm");

  ok(!!tt, "tickit_term_new_for_termtype");

  tickit_term_set_output_func(tt, output, buffer);
  tickit_term_set_output_buffer(tt, 4096);

  buffer[0] = 0;

  tickit_term_print(tt, "Hello ");
  is_str_escape(buffer, "", "buffer empty after print");

  tickit_term_print(tt, "world!");
  is_str_escape(buffer, "", "buffer still empty after second print");

  tickit_term_flush(tt);
  is_str_escape(buffer, "Hello world!", "buffer contains output after flush");

  tickit_term_destroy(tt);

  return exit_status();
}
