#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

#include <unistd.h>
#include <sys/wait.h>

static int called_count;
static int unbound_count;
static int on_call_capture_status(Tickit *t, TickitEventFlags flags, void *_info, void *user)
{
  if(flags & TICKIT_EV_FIRE) {
    TickitProcessWatchInfo *info = _info;

    int *ip = user;
    (*ip) = info->wstatus;

    called_count++;
    tickit_stop(t);
  }
  if(flags & TICKIT_EV_UNBIND)
    unbound_count++;

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));

  {
    int status = 0;
    pid_t kid = fork();
    if(kid == 0) {
      usleep(10000);
      _exit(5);
    }

    tickit_watch_process(t, kid, 0, &on_call_capture_status, &status);

    tickit_run(t);

    is_int(called_count, 1, "tickit_watch_process invokes callback");
    is_int(WEXITSTATUS(status), 5, "tickit_watch_process passes status to callback");
  }

  /* child pre-exit before watch */
  {
    int status = 0;
    pid_t kid = fork();
    if(kid == 0) {
      _exit(10);
    }

    usleep(10000);

    called_count = 0;

    tickit_watch_process(t, kid, 0, &on_call_capture_status, &status);

    tickit_run(t);

    is_int(called_count, 1, "tickit_watch_process invokes callback for pre-exit");
    is_int(WEXITSTATUS(status), 10, "tickit_watch_process passes status to callback for pre-exit");
  }

  /* object destruction */
  {
    /* We'll just make up a PID, it won't matter as we'll just cancel the
     * watch */
    tickit_watch_process(t, 1234, TICKIT_BIND_DESTROY, &on_call_capture_status, NULL);

    unbound_count = 0;

    tickit_unref(t);

    is_int(unbound_count, 1, "unbound_count after tickit_unref");
  }

  return exit_status();
}
