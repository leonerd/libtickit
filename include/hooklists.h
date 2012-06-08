#include "tickit.h"

struct TickitEventHook;

typedef void TickitEventFn(void *owner, TickitEventType ev, TickitEvent *args, void *data);

void tickit_hooklist_run_event(struct TickitEventHook *hooks, void *owner, TickitEventType ev, TickitEvent *args);

int  tickit_hooklist_bind_event(struct TickitEventHook **hooklist, void *owner, TickitEventType ev, TickitEventFn *fn, void *data);
void tickit_hooklist_unbind_event_id(struct TickitEventHook **hooklist, void *owner, int id);

void tickit_hooklist_unbind_and_destroy(struct TickitEventHook *hooks, void *owner);
