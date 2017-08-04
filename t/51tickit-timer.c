#include "tickit.h"
#include "taplib.h"

static void on_call_incr(Tickit *t, void *user)
{
  int *ip = user;
  (*ip)++;

  tickit_stop(t);
}

struct State {
  int *counterp;
  int capture;
};

static void on_call_capture(Tickit *t, void *user)
{
  struct State *state = user;

  state->capture = *state->counterp;
  (*state->counterp)--;

  if(!(*state->counterp))
    tickit_stop(t);
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new();

  {
    int called = 0;
    tickit_timer_after_msec(t, 10, &on_call_incr, &called);

    tickit_run(t);

    is_int(called, 1, "tickit_timer_after_msec invokes callback");
  }

  /* two timers with ordering */
  {
    int counter = 2;
    struct State
      state_a = { .counterp = &counter },
      state_b = { .counterp = &counter };

    tickit_timer_after_msec(t, 10, &on_call_capture, &state_a);
    tickit_timer_after_msec(t, 20, &on_call_capture, &state_b);

    tickit_run(t);

    is_int(state_a.capture, 2, "tickit_timer_after_msec first capture");
    is_int(state_b.capture, 1, "tickit_timer_after_msec second capture");
  }

  tickit_unref(t);

  return exit_status();
}
