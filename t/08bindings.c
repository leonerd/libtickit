#include "tickit.h"
#include "taplib.h"

#include "../src/bindings.h"

struct TickitBindings bindings = { NULL };

int incr(void *owner, TickitEventFlags flags, void *info, void *data)
{
  ((int *)data)[0]++;
  return 0;
}

int delete(void *owner, TickitEventFlags flags, void *info, void *data)
{
  tickit_bindings_unbind_event_id(&bindings, NULL, *(int *)info);
  return 0;
}

int main(int argc, char *argv[])
{
  {
    int id = tickit_bindings_bind_event(&bindings, NULL, 1<<0, 0, &delete, NULL);

    int count = 0;
    tickit_bindings_bind_event(&bindings, NULL, 1<<0, 0, &incr, &count);

    tickit_bindings_run_event(&bindings, NULL, 1<<0, &id);

    is_int(count, 1, "binding after self-removal still invoked");

    tickit_bindings_unbind_and_destroy(&bindings, NULL);
    bindings.first = NULL;
  }

  {
    tickit_bindings_bind_event(&bindings, NULL, 1<<1, 0, &delete, NULL);

    int count2 = 0;
    int id2 = tickit_bindings_bind_event(&bindings, NULL, 1<<1, 0, &incr, &count2);

    int count3 = 0;
    tickit_bindings_bind_event(&bindings, NULL, 1<<1, 0, &incr, &count3);

    tickit_bindings_run_event(&bindings, NULL, 1<<1, &id2);

    is_int(count2, 0, "removed binding is not invoked");
    is_int(count3, 1, "binding after removed one still invoked");

    tickit_bindings_unbind_and_destroy(&bindings, NULL);
    bindings.first = NULL;
  }

  return exit_status();
}
