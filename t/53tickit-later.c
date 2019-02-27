#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

static int unbound_count;
static int on_call_incr(Tickit *t, TickitEventFlags flags, void *info, void *user)
{
  if(flags & TICKIT_EV_FIRE) {
    int *ip = user;
    (*ip)++;

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
    int called = 0;
    tickit_watch_later(t, 0, &on_call_incr, &called);

    tickit_run(t);

    is_int(called, 1, "tickit_watch_later invokes callback");
  }

  /* object destruction */
  {
    tickit_watch_later(t, TICKIT_BIND_DESTROY, &on_call_incr, NULL);

    unbound_count = 0;

    tickit_unref(t);

    is_int(unbound_count, 1, "unbound_count after tickit_unref");
  }

  return exit_status();
}
