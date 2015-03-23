#include "hooklists.h"

#include <stdlib.h>

struct TickitEventHook {
  struct TickitEventHook *next;
  TickitEventType         ev;
  TickitEventFn          *fn;
  void                   *data;
  int                     id;
};

void tickit_hooklist_run_event(struct TickitEventHook *hooks, void *owner, TickitEventType ev, TickitEventInfo *args)
{
  for(struct TickitEventHook *hook = hooks; hook; hook = hook->next)
    if(hook->ev & ev)
      (*hook->fn)(owner, ev, args, hook->data);
}

int tickit_hooklist_run_event_whilefalse(struct TickitEventHook *hooks, void *owner, TickitEventType ev, TickitEventInfo *args)
{
  for(struct TickitEventHook *hook = hooks; hook; hook = hook->next)
    if(hook->ev & ev) {
      int ret = (*hook->fn)(owner, ev, args, hook->data);
      if(ret)
        return ret;
    }

  return 0;
}

int tickit_hooklist_bind_event(struct TickitEventHook **hooklist, void *owner, TickitEventType ev, TickitEventFn *fn, void *data)
{
  int max_id = 0;

  /* Find the end of a linked list, and find the highest ID in use while we're
   * at it
   */
  struct TickitEventHook **newhook = hooklist;
  for(; *newhook; newhook = &(*newhook)->next)
    if((*newhook)->id > max_id)
      max_id = (*newhook)->id;

  *newhook = malloc(sizeof(struct TickitEventHook)); // TODO: malloc failure

  (*newhook)->next = NULL;
  (*newhook)->ev = ev;
  (*newhook)->fn = fn;
  (*newhook)->data = data;

  return (*newhook)->id = max_id + 1;
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
