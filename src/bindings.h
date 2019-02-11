#include "tickit.h"

struct TickitBinding;

struct TickitBindings {
  struct TickitBinding *first;
  int is_iterating : 1;
  int needs_delete : 1;
};

typedef int TickitEventFn(void *owner, TickitEventFlags flags, void *info, void *data);

void tickit_bindings_run_event(struct TickitBindings *bindings, void *owner, int evindex, void *info);
int  tickit_bindings_run_event_whilefalse(struct TickitBindings *bindings, void *owner, int evindex, void *info);

int  tickit_bindings_bind_event(struct TickitBindings *bindings, void *owner, int evindex, TickitBindFlags flags,
    TickitEventFn *fn, void *data);
void tickit_bindings_unbind_event_id(struct TickitBindings *bindings, void *owner, int id);

void tickit_bindings_unbind_and_destroy(struct TickitBindings *bindings, void *owner);

#define DEFINE_BINDINGS_FUNCS(NAME,OWNER,EVENTFN)                                \
  int tickit_##NAME##_bind_event(OWNER *owner, OWNER##Event evindex,             \
      TickitBindFlags flags, EVENTFN *fn, void *data)                            \
  {                                                                              \
    return tickit_bindings_bind_event(&owner->bindings, owner,                      \
        evindex, flags, (TickitEventFn*)fn, data);                               \
  }                                                                              \
                                                                                 \
  void tickit_##NAME##_unbind_event_id(OWNER *owner, int id)                     \
  {                                                                              \
    tickit_bindings_unbind_event_id(&owner->bindings, owner, id);                   \
  }                                                                              \
                                                                                 \
  static inline void run_events(OWNER *owner, int evindex,                       \
      void *info)                                                                \
  {                                                                              \
    tickit_bindings_run_event(&owner->bindings, owner, evindex, info);              \
  }                                                                              \
                                                                                 \
  static inline int run_events_whilefalse(OWNER *owner, int evindex,             \
      void *info)                                                                \
  {                                                                              \
    return tickit_bindings_run_event_whilefalse(&owner->bindings, owner, evindex,   \
        info);                                                                   \
  }
