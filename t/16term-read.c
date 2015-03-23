#define _XOPEN_SOURCE 500 // usleep

#include "tickit.h"
#include "taplib.h"

#include <string.h>
#include <unistd.h>

TickitKeyEventType keytype;
char               keystr[16];

int on_key(TickitTerm *tt, TickitEventType ev, TickitEventInfo *args, void *data)
{
  keytype = args->type;
  strncpy(keystr, args->str, sizeof(keystr)-1); keystr[sizeof(keystr)-1] = 0;

  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  int    fd[2];

  /* We'll need a real filehandle we can write/read.
   * pipe() can make us one */
  pipe(fd);

  tt = tickit_term_new_for_termtype("xterm");

  tickit_term_set_input_fd(tt, fd[0]);

  is_int(tickit_term_get_input_fd(tt), fd[0], "tickit_term_get_input_fd");

  tickit_term_bind_event(tt, TICKIT_EV_KEY, on_key, NULL);

  write(fd[1], "A", 1);
  tickit_term_input_readable(tt);

  is_int(keytype, TICKIT_KEYEV_TEXT, "keytype after write A");
  is_str(keystr,  "A",               "keystr after write A");

  is_int(tickit_term_input_check_timeout_msec(tt), -1, "term has no timeout after A");

  keytype = -1; keystr[0] = 0;
  write(fd[1], "\e", 1);
  tickit_term_input_readable(tt);

  is_int(keytype, -1, "keytype not set after write Escape");

  int msec = tickit_term_input_check_timeout_msec(tt);
  ok(msec > 0, "term has timeout after Escape");

  /* Add an extra milisecond timing grace */
  usleep((msec+1) * 1000);

  tickit_term_input_check_timeout_msec(tt);

  is_int(keytype, TICKIT_KEYEV_KEY, "keytype after write completed Escape");
  is_str(keystr,  "Escape",         "keystr after write completed Escape");

  is_int(tickit_term_input_check_timeout_msec(tt), -1, "term has no timeout after completed Escape");

  tickit_term_destroy(tt);

  return exit_status();
}
