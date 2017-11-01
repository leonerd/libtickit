#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

static int on_call_incr(Tickit *t, TickitEventFlags flags, void *user)
{
  int *ip = user;
  (*ip)++;

  tickit_stop(t);
  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));

  {
    int called = 0;
    tickit_later(t, &on_call_incr, &called);

    tickit_run(t);

    is_int(called, 1, "tickit_later invokes callback");
  }

  tickit_unref(t);

  return exit_status();
}
