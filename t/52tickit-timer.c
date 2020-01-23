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

struct State {
  int *counterp;
  int capture;
};

static int on_call_capture(Tickit *t, TickitEventFlags flags, void *info, void *user)
{
  struct State *state = user;

  state->capture = *state->counterp;
  (*state->counterp)--;

  if(!(*state->counterp))
    tickit_stop(t);

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));

  /* tickit_watch_timer_after_msec */
  {
    int called = 0;
    tickit_watch_timer_after_msec(t, 10, 0, &on_call_incr, &called);

    tickit_run(t);

    is_int(called, 1, "tickit_watch_timer_after_msec invokes callback");
  }

  /* tickit_watch_timer_at_tv */
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    tv.tv_usec += 1000;
    if(tv.tv_usec >= 1000000)
      tv.tv_sec++, tv.tv_usec -= 1000000;

    int called = 0;
    tickit_watch_timer_at_tv(t, &tv, 0, &on_call_incr, &called);

    tickit_run(t);

    is_int(called, 1, "tickit_watch_timer_at_tv invokes callback");
  }

  /* two timers with ordering */
  {
    int counter = 2;
    struct State
      state_a = { .counterp = &counter },
      state_b = { .counterp = &counter };

    tickit_watch_timer_after_msec(t, 10, 0, &on_call_capture, &state_a);
    tickit_watch_timer_after_msec(t, 20, 0, &on_call_capture, &state_b);

    tickit_run(t);

    is_int(state_a.capture, 2, "tickit_watch_timer_after_msec first capture");
    is_int(state_b.capture, 1, "tickit_watch_timer_after_msec second capture");
  }

  /* timer cancellation */
  {
    void *watch = tickit_watch_timer_after_msec(t, 5, TICKIT_BIND_UNBIND, &on_call_incr, NULL);
    tickit_watch_cancel(t, watch);
    pass("immediate tickit_watch_cancel does not segfault");
  }
  {
    int called = 0;
    tickit_watch_timer_after_msec(t, 10, 0, &on_call_incr, &called);
    int not_called = 0;
    void *watch = tickit_watch_timer_after_msec(t, 5, TICKIT_BIND_UNBIND, &on_call_incr, &not_called);

    unbound_count = 0;
    tickit_watch_cancel(t, watch);
    is_int(unbound_count, 1, "unbound_count after tickit_watch_cancel");

    tickit_run(t);

    ok(!not_called, "tickit_watch_cancel prevents invocation");
  }

  /* object destruction */
  {
    tickit_watch_timer_after_msec(t, 10, TICKIT_BIND_DESTROY, &on_call_incr, NULL);

    unbound_count = 0;

    tickit_unref(t);

    is_int(unbound_count, 1, "unbound_count after tickit_unref");
  }

  return exit_status();
}
