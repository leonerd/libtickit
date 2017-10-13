#include "tickit.h"
#include "taplib.h"

#include "../src/hooklists.h"

struct TickitHooklist hooks = { NULL };

int incr(void *owner, TickitEventFlags flags, void *info, void *data)
{
  ((int *)data)[0]++;
}

int delete(void *owner, TickitEventFlags flags, void *info, void *data)
{
  tickit_hooklist_unbind_event_id(&hooks, NULL, *(int *)info);
}

int main(int argc, char *argv[])
{
  {
    int id = tickit_hooklist_bind_event(&hooks, NULL, 1<<0, 0, &delete, NULL);

    int count = 0;
    tickit_hooklist_bind_event(&hooks, NULL, 1<<0, 0, &incr, &count);

    tickit_hooklist_run_event(&hooks, NULL, 1<<0, &id);

    is_int(count, 1, "hook after self-removal still invoked");

    tickit_hooklist_unbind_and_destroy(&hooks, NULL);
    hooks.hooks = NULL;
  }

  {
    int id1 = tickit_hooklist_bind_event(&hooks, NULL, 1<<1, 0, &delete, NULL);

    int count2 = 0;
    int id2 = tickit_hooklist_bind_event(&hooks, NULL, 1<<1, 0, &incr, &count2);

    int count3 = 0;
    tickit_hooklist_bind_event(&hooks, NULL, 1<<1, 0, &incr, &count3);

    tickit_hooklist_run_event(&hooks, NULL, 1<<1, &id2);

    is_int(count2, 0, "removed hook is not invoked");
    is_int(count3, 1, "hook after removed one still invoked");

    tickit_hooklist_unbind_and_destroy(&hooks, NULL);
    hooks.hooks = NULL;
  }

  return exit_status();
}
