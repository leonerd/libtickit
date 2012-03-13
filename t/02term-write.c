#include "tickit.h"
#include "taplib.h"

#include <string.h>

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  int    fd[2];
  char   buffer[1024];
  size_t len;

  /* We'll need a real filehandle we can write/read.
   * pipe() can make us one */
  pipe(fd);

  plan_tests(4);

  tt = tickit_term_new();

  ok(!!tt, "tickit_term_new");

  tickit_term_set_output_fd(tt, fd[1]);

  buffer[0] = 0;

  tickit_term_print(tt, "Hello world!");

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_int(len, 12, "read() length after tickit_term_print");
  is_str(buffer, "Hello world!", "buffer after tickit_term_print");

  tickit_term_free(tt);

  ok(1, "tickit_term_free");

  return exit_status();
}
