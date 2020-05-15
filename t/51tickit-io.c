#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

#include <unistd.h>

int               captured_fd;
TickitIOCondition captured_cond;

static int on_call_incr(Tickit *t, TickitEventFlags flags, void *_info, void *user)
{
  TickitIOWatchInfo *info = _info;

  if(flags & TICKIT_EV_FIRE) {
    int *ip = user;
    (*ip)++;

    captured_fd   = info->fd;
    captured_cond = info->cond;

    tickit_stop(t);
  }

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));

  int fds[2];
  if(pipe(fds) != 0) {
    perror("pipe");
    exit(1);
  }

  /* TICKIT_IO_IN */
  {
    int counter = 0;

    void *watch = tickit_watch_io(t, fds[0], TICKIT_IO_IN, 0, &on_call_incr, &counter);

    write(fds[1], "OUT\n", 4);

    tickit_run(t);

    is_int(counter, 1, "tickit_watch_io cond=IN invokes callback");

    is_int(captured_fd, fds[0], "invoked callback receives fd in info");
    is_int(captured_cond, TICKIT_IO_IN, "invoked callback receives cond=IN");

    tickit_watch_cancel(t, watch);

    tickit_tick(t, TICKIT_RUN_NOHANG);

    is_int(counter, 1, "tickit_cancel_watch removes iowatch");
  }

  /* TICKIT_IO_OUT */
  {
    int counter = 0;

    void *watch = tickit_watch_io(t, fds[1], TICKIT_IO_OUT, 0, &on_call_incr, &counter);

    tickit_run(t);

    is_int(counter, 1, "tickit_watch_io cond=OUT invokes callback");

    is_int(captured_fd, fds[1], "invoked callback receives fd in info");
    is_int(captured_cond, TICKIT_IO_OUT, "invoked callback receives cond=OUT");

    tickit_watch_cancel(t, watch);
  }

  close(fds[1]);

  /* TICKIT_IO_HUP */
  {
    int counter = 0;

    void *watch = tickit_watch_io(t, fds[0], TICKIT_IO_HUP, 0, &on_call_incr, &counter);

    tickit_run(t);

    is_int(counter, 1, "tickit_watch_io cond=HUP invokes callback");

    is_int(captured_fd, fds[0], "invoked callback receives fd in info");
    is_int(captured_cond, TICKIT_IO_HUP, "invoked callback receives cond=HUP");

    tickit_watch_cancel(t, watch);
  }

  close(fds[0]);

  tickit_unref(t);

  return exit_status();
}
