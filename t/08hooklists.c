#include "tickit.h"
#include "taplib.h"

#include "../src/hooklists.h"

struct TickitEventHook *hooks = NULL;

int incr(void *owner, TickitEventType ev, void *info, void *data)
{
  ((int *)data)[0]++;
}

int delete(void *owner, TickitEventType ev, void *info, void *data)
{
  tickit_hooklist_unbind_event_id(&hooks, NULL, *(int *)info);
}

int main(int argc, char *argv[])
{
  {
    int id = tickit_hooklist_bind_event(&hooks, NULL, 1<<0, 0, &delete, NULL);

    int count = 0;
    tickit_hooklist_bind_event(&hooks, NULL, 1<<0, 0, &incr, &count);

    tickit_hooklist_run_event(hooks, NULL, 1<<0, &id);

    is_int(count, 1, "hook after self-removal still invoked");
  }

  return exit_status();
}
