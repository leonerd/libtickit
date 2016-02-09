#include "tickit.h"

struct TickitEventHook;

typedef int TickitEventFn(void *owner, TickitEventType ev, void *info, void *data);

void tickit_hooklist_run_event(struct TickitEventHook *hooks, void *owner, TickitEventType ev, void *info);
int  tickit_hooklist_run_event_whilefalse(struct TickitEventHook *hooks, void *owner, TickitEventType ev, void *info);

int  tickit_hooklist_bind_event(struct TickitEventHook **hooklist, void *owner, TickitEventType ev, TickitBindFlags flags,
    TickitEventFn *fn, void *data);
void tickit_hooklist_unbind_event_id(struct TickitEventHook **hooklist, void *owner, int id);

void tickit_hooklist_unbind_and_destroy(struct TickitEventHook *hooks, void *owner);

#define DEFINE_HOOKLIST_FUNCS(NAME,OWNER,EVENTFN)                               \
  int tickit_##NAME##_bind_event(OWNER *owner, TickitEventType ev,              \
      TickitBindFlags flags, EVENTFN *fn, void *data)                           \
  {                                                                             \
    return tickit_hooklist_bind_event(&owner->hooks, owner,                     \
        ev, flags, (TickitEventFn*)fn, data);                                   \
  }                                                                             \
                                                                                \
  void tickit_##NAME##_unbind_event_id(OWNER *owner, int id)                    \
  {                                                                             \
    tickit_hooklist_unbind_event_id(&owner->hooks, owner, id);                  \
  }                                                                             \
                                                                                \
  static inline void run_events(OWNER *owner, TickitEventType ev,               \
      void *info)                                                               \
  {                                                                             \
    tickit_hooklist_run_event(owner->hooks, owner, ev, info);                   \
  }                                                                             \
                                                                                \
  static inline int run_events_whilefalse(OWNER *owner, TickitEventType ev,     \
      void *info)                                                               \
  {                                                                             \
    return tickit_hooklist_run_event_whilefalse(owner->hooks, owner, ev, info); \
  }
