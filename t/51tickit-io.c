#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

#include <unistd.h>

static int on_call_incr(Tickit *t, TickitEventFlags flags, void *user)
{
  if(flags & TICKIT_EV_FIRE) {
    int *ip = user;
    (*ip)++;

    tickit_stop(t);
  }

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));

  {
    int fds[2];
    if(pipe(fds) != 0) {
      perror("pipe");
      exit(1);
    }

    int counter = 0;

    void *watch = tickit_watch_io_read(t, fds[0], 0, &on_call_incr, &counter);

    write(fds[1], "OUT\n", 4);

    tickit_run(t);

    is_int(counter, 1, "tickit_watch_io_read invokes callback");

    tickit_watch_cancel(t, watch);

    /* TODO: fds[0] is still readready currently but lets just throw it away */
  }

  tickit_unref(t);

  return exit_status();
}
