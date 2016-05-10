#include "tickit.h"
#include "taplib.h"

#include <string.h>
#include <unistd.h>

bool output_eof = false;

void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;

  if(bytes)
    strncat(buffer, bytes, len);
  else
    output_eof = true;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  int    fd[2];
  char   buffer[1024] = { 0 };
  size_t len;

  /* We'll need a real filehandle we can write/read.
   * pipe() can make us one */
  pipe(fd);

  tt = tickit_term_new_for_termtype("xterm");

  ok(!!tt, "tickit_term_new_for_termtype");

  tickit_term_set_output_fd(tt, fd[1]);

  is_int(tickit_term_get_output_fd(tt), fd[1], "tickit_term_get_output_fd");

  /* Already it should have written its DECSLRM probe string */
  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_str_escape(buffer, "\e[?69h\e[?69$p\e[?25$p\e[?12$p\eP$q q\e\\\e[G\e[K",
      "buffer after initialisation contains DECSLRM and cursor status probes");

  tickit_term_print(tt, "Hello world!");

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_int(len, 12, "read() length after tickit_term_print");
  is_str_escape(buffer, "Hello world!", "buffer after tickit_term_print");

  tickit_term_printn(tt, "another string here", 7);

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_int(len, 7, "read() length after tickit_term_printn");
  is_str_escape(buffer, "another", "buffer after tickit_term_printn");

  tickit_term_printf(tt, "%s %s!", "More", "messages");

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_int(len, 14, "read() length after tickit_term_printf");
  is_str_escape(buffer, "More messages!", "buffer after tickit_term_printf");

  tickit_term_goto(tt, 2, 5);

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_str_escape(buffer, "\e[3;6H", "buffer after tickit_term_goto line+col");

  tickit_term_goto(tt, 4, -1);

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_str_escape(buffer, "\e[5d", "buffer after tickit_term_goto line");

  tickit_term_goto(tt, -1, 10);

  len = read(fd[0], buffer, sizeof buffer);
  buffer[len] = 0;

  is_str_escape(buffer, "\e[11G", "buffer after tickit_term_goto col");

  buffer[0] = 0;

  tickit_term_set_output_func(tt, output, buffer);

  tickit_term_print(tt, "Hello, world");

  is_str(buffer, "Hello, world", "buffer after print using output func");

  tickit_term_destroy(tt);

  ok(1, "tickit_term_destroy");

  ok(output_eof, "output func receives EOF indication after tickit_term_destroy");

  return exit_status();
}
