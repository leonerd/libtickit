#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

int on_event_incr_int(TickitWindow *window, TickitEventFlags flags, void *_info, void *data)
{
  (*(int*)data)++;
  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_for_term(tickit_mockterm_new(25, 80));
  TickitWindow *root;

  ok(!!t, "tickit_new_for_term()");

  /* TODO: add some real tests
   * There isn't a lot that can be checked in a unit-test script yet, until
   * more accessors, mutators, and maybe some construction options or a
   * builder happens
   */

  // Rootwin
  {
    root = tickit_get_rootwin(t);
    ok(!!root, "tickit_get_rootwin");
  }

  // DESTROY
  int destroyed = 0;
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_DESTROY, 0, &on_event_incr_int, &destroyed);

  tickit_unref(t);

  ok(destroyed, "TICKIT_WINDOW_ON_DESTROY invoked");

  return exit_status();
}
