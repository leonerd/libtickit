#include "tickit.h"

struct TickitEventHook;

typedef void TickitEventFn(void *owner, TickitEventType ev, TickitEvent *args, void *data);

void tickit_hooklist_run_event(struct TickitEventHook *hooks, void *owner, TickitEventType ev, TickitEvent *args);

int  tickit_hooklist_bind_event(struct TickitEventHook **hooklist, void *owner, TickitEventType ev, TickitEventFn *fn, void *data);
void tickit_hooklist_unbind_event_id(struct TickitEventHook **hooklist, void *owner, int id);

void tickit_hooklist_unbind_and_destroy(struct TickitEventHook *hooks, void *owner);

#define DEFINE_HOOKLIST_FUNCS(NAME,OWNER,EVENTFN)                      \
  int tickit_##NAME##_bind_event(OWNER *owner, TickitEventType ev,     \
      EVENTFN *fn, void *data)                                         \
  {                                                                    \
    return tickit_hooklist_bind_event(&owner->hooks, owner,            \
        ev, (TickitEventFn*)fn, data);                                 \
  }                                                                    \
                                                                       \
  void tickit_##NAME##_unbind_event_id(OWNER *owner, int id)           \
  {                                                                    \
    tickit_hooklist_unbind_event_id(&owner->hooks, owner, id);         \
  }                                                                    \
                                                                       \
  static void run_events(OWNER *owner, TickitEventType ev,             \
      TickitEvent *args)                                               \
  {                                                                    \
    tickit_hooklist_run_event(owner->hooks, owner, ev, args);          \
  }
