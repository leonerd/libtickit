#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

static int on_call_incr(Tickit *t, TickitEventFlags flags, void *user)
{
  if(flags & TICKIT_EV_FIRE) {
    int *ip = user;
    (*ip)++;
  }

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));

  // RUN_NOHANG with nothing
  {
    tickit_tick(t, TICKIT_RUN_NOHANG);
    pass("tickit_tick TICKIT_RUN_NOHANG stops immediately");
  }

  // RUN_ONCE runs once
  {
    int called = 0;
    tickit_watch_later(t, 0, &on_call_incr, &called);

    tickit_tick(t, TICKIT_RUN_ONCE);

    is_int(called, 1, "tickit_tick TICKIT_RUN_ONCE stops after invocation");
  }

  return exit_status();
}
