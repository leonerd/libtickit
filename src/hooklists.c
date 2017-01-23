#include "hooklists.h"

#include <stdlib.h>

struct TickitEventHook {
  struct TickitEventHook *next;
  int                     id;
  TickitEventType         ev;
  TickitEventFn          *fn;
  void                   *data;
};

void tickit_hooklist_run_event(struct TickitHooklist *hooklist, void *owner, TickitEventType ev, void *info)
{
  for(struct TickitEventHook *hook = hooklist->hooks; hook; /**/) {
    struct TickitEventHook *next = hook->next;
    if(hook->ev & ev)
      (*hook->fn)(owner, ev, info, hook->data);
    hook = next;
  }
}

int tickit_hooklist_run_event_whilefalse(struct TickitHooklist *hooklist, void *owner, TickitEventType ev, void *info)
{
  for(struct TickitEventHook *hook = hooklist->hooks; hook; /**/) {
    struct TickitEventHook *next = hook->next;
    if(hook->ev & ev) {
      int ret = (*hook->fn)(owner, ev, info, hook->data);
      if(ret)
        return ret;
    }
    hook = next;
  }

  return 0;
}

int tickit_hooklist_bind_event(struct TickitHooklist *hooklist, void *owner, TickitEventType ev, TickitBindFlags flags,
    TickitEventFn *fn, void *data)
{
  int max_id = 0;

  struct TickitEventHook **newhookp = &hooklist->hooks;
  struct TickitEventHook *next = NULL;

  if(flags & TICKIT_BIND_FIRST) {
    next = hooklist->hooks;
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

void tickit_hooklist_unbind_event_id(struct TickitHooklist *hooklist, void *owner, int id)
{
  for(struct TickitEventHook **hookp = &hooklist->hooks; *hookp; ) {
    struct TickitEventHook *hook = *hookp;
    if(hook->id != id) {
      hookp = &(hook->next);
      continue;
    }

    if(hook->ev & TICKIT_EV_UNBIND)
      (*hook->fn)(owner, TICKIT_EV_UNBIND, NULL, hook->data);

    *hookp = hook->next;

    // zero out the structure
    hook->next = NULL;
    hook->ev = 0;
    hook->fn = NULL;

    free(hook);
    /* no hookp update */
  }
}

void tickit_hooklist_unbind_and_destroy(struct TickitHooklist *hooklist, void *owner)
{
  /* TICKIT_EV_DESTROY events need to run in reverse order. Since the hooklist
   * is singly-linked it is easiest just to reverse it then iterate.
   */
  struct TickitEventHook *revhooks = NULL;
  for(struct TickitEventHook *hook = hooklist->hooks; hook; /**/) {
    struct TickitEventHook *this = hook;
    hook = hook->next;

    this->next = revhooks;
    revhooks = this;
  }

  for(struct TickitEventHook *hook = revhooks; hook;) {
    struct TickitEventHook *next = hook->next;
    if(hook->ev & (TICKIT_EV_UNBIND|TICKIT_EV_DESTROY))
      (*hook->fn)(owner, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY, NULL, hook->data);
    free(hook);
    hook = next;
  }
}
