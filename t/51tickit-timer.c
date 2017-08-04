#include "tickit.h"
#include "taplib.h"

static void on_call_incr(Tickit *t, void *user)
{
  int *ip = user;
  (*ip)++;

  tickit_stop(t);
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new();

  {
    int called = 0;
    tickit_timer_after_msec(t, 10, &on_call_incr, &called);

    tickit_run(t);

    is_int(called, 1, "tickit_timer_after_ms invokes callback");
  }

  tickit_unref(t);

  return exit_status();
}
