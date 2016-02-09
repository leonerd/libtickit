#include "hooklists.h"

#include <stdlib.h>

struct TickitEventHook {
  struct TickitEventHook *next;
  TickitEventType         ev;
  TickitEventFn          *fn;
  void                   *data;
  int                     id;
};

void tickit_hooklist_run_event(struct TickitEventHook *hooks, void *owner, TickitEventType ev, void *info)
{
  for(struct TickitEventHook *hook = hooks; hook; hook = hook->next)
    if(hook->ev & ev)
      (*hook->fn)(owner, ev, info, hook->data);
}

int tickit_hooklist_run_event_whilefalse(struct TickitEventHook *hooks, void *owner, TickitEventType ev, void *info)
{
  for(struct TickitEventHook *hook = hooks; hook; hook = hook->next)
    if(hook->ev & ev) {
      int ret = (*hook->fn)(owner, ev, info, hook->data);
      if(ret)
        return ret;
    }

  return 0;
}

int tickit_hooklist_bind_event(struct TickitEventHook **hooklist, void *owner, TickitEventType ev, TickitBindFlags flags,
    TickitEventFn *fn, void *data)
{
  int max_id = 0;

  struct TickitEventHook **newhookp = hooklist;
  struct TickitEventHook *next = NULL;

  if(flags & TICKIT_BIND_FIRST) {
    next = *hooklist;
    for(struct TickitEventHook *hook = *newhookp; hook; hook = hook->next)
      if(hook->id > max_id)
        max_id = hook->id;
  }
  else {
    for(; *newhookp; newhookp = &(*newhookp)->next)
      if((*newhookp)->id > max_id)
        max_id = (*newhookp)->id;
  }

  *newhookp = malloc(sizeof(struct TickitEventHook)); // TODO: malloc failure

  (*newhookp)->next = next;
  (*newhookp)->ev = ev;
  (*newhookp)->fn = fn;
  (*newhookp)->data = data;

  return (*newhookp)->id = max_id + 1;
}

void tickit_hooklist_unbind_event_id(struct TickitEventHook **hooklist, void *owner, int id)
{
  struct TickitEventHook **link = hooklist;
  for(struct TickitEventHook *hook = *hooklist; hook;) {
    if(hook->id == id) {
      *link = hook->next;
      if(hook->ev & TICKIT_EV_UNBIND)
        (*hook->fn)(owner, TICKIT_EV_UNBIND, NULL, hook->data);
      free(hook);
      hook = *link;
    }
    else
      hook = hook->next;
  }
}

void tickit_hooklist_unbind_and_destroy(struct TickitEventHook *hooks, void *owner)
{
  for(struct TickitEventHook *hook = hooks; hook;) {
    struct TickitEventHook *next = hook->next;
    if(hook->ev & TICKIT_EV_UNBIND)
      (*hook->fn)(owner, TICKIT_EV_UNBIND, NULL, hook->data);
    free(hook);
    hook = next;
  }
}
