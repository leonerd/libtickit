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
  char   buffer[1024] = { 0 };

  tt = tickit_term_build(&(struct TickitTermBuilder){
    .termtype  = "xterm",
    .output_func      = output,
    .output_func_user = buffer,
  });

  ok(!!tt, "tickit_term_build");

  tickit_term_set_output_buffer(tt, 8);

  buffer[0] = 0;

  tickit_term_print(tt, "Hello ");
  is_str_escape(buffer, "", "buffer empty after print");

  tickit_term_print(tt, "world!");
  is_str_escape(buffer, "Hello wo", "buffer contains one spill after second print");

  tickit_term_flush(tt);
  is_str_escape(buffer, "Hello world!", "buffer contains output after flush");

  tickit_term_unref(tt);

  return exit_status();
}
