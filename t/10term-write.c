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
  char   buffer[1024] = { 0 };
  size_t len;

  /* Output via filehandle */
  {
    int    fd[2];
    /* We'll need a real filehandle we can write/read.
     * pipe() can make us one */
    pipe(fd);

    TickitTerm *tt = tickit_term_build(&(struct TickitTermBuilder){
      .termtype  = "xterm",
      .open      = TICKIT_OPEN_FDS,
      .input_fd  = -1,
      .output_fd = fd[1],
    });

    ok(!!tt, "tickit_term_build");

    is_int(tickit_term_get_output_fd(tt), fd[1], "tickit_term_get_output_fd");

    /* Already it should have written its DECSLRM probe string */
    len = read(fd[0], buffer, sizeof buffer);
    buffer[len] = 0;

    is_str_escape(buffer,
        "\e[?69h\e[?69$p\e[?25$p\e[?12$p\eP$q q\e\\"
          "\e[38;5;255m\e[38:2:0:1:2m\eP$qm\e\\\e[m"
          "\e[G\e[K",
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

    tickit_term_unref(tt);
  }

  /* Output by function */
  {
    TickitTerm *tt = tickit_term_build(&(struct TickitTermBuilder){
      .termtype  = "xterm",
      .output_func      = output,
      .output_func_user = buffer,
    });

    /* Ignore startup string */
    buffer[0] = 0;

    tickit_term_print(tt, "Hello, world");

    is_str(buffer, "Hello, world", "buffer after print using output func");

    tickit_term_unref(tt);

    ok(1, "tickit_term_unref");

    ok(output_eof, "output func receives EOF indication after tickit_term_unref");
  }

  return exit_status();
}
