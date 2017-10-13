#include "hooklists.h"

#include <stdlib.h>

struct TickitEventHook {
  struct TickitEventHook *next;
  int                     id;
  int                     evindex;
  TickitBindFlags         flags;
  TickitEventFn          *fn;
  void                   *data;
};

#define HOOK_ID_TOMBSTONE -1

static void cleanup(struct TickitHooklist *hooklist)
{
  for(struct TickitEventHook **hookp = &hooklist->hooks; *hookp; /**/) {
    struct TickitEventHook *hook = *hookp;
    if(hook->id != HOOK_ID_TOMBSTONE) {
      hookp = &(*hookp)->next;
      continue;
    }

    *hookp = hook->next;
    hook->next = NULL;

    free(hook);
  }

  hooklist->needs_delete = false;
}

void tickit_hooklist_run_event(struct TickitHooklist *hooklist, void *owner, int evindex, void *info)
{
  int was_iterating = hooklist->is_iterating;
  hooklist->is_iterating = true;

  for(struct TickitEventHook *hook = hooklist->hooks; hook; hook = hook->next)
    if(hook->evindex == evindex)
      (*hook->fn)(owner, TICKIT_EV_FIRE, info, hook->data);

  hooklist->is_iterating = was_iterating;
  if(!was_iterating && hooklist->needs_delete)
    cleanup(hooklist);
}

int tickit_hooklist_run_event_whilefalse(struct TickitHooklist *hooklist, void *owner, int evindex, void *info)
{
  int was_iterating = hooklist->is_iterating;
  hooklist->is_iterating = true;

  int ret = 0;

  for(struct TickitEventHook *hook = hooklist->hooks; hook; hook = hook->next)
    if(hook->evindex == evindex) {
      ret = (*hook->fn)(owner, TICKIT_EV_FIRE, info, hook->data);
      if(ret)
        goto exit;
    }

exit:
  hooklist->is_iterating = was_iterating;
  if(!was_iterating && hooklist->needs_delete)
    cleanup(hooklist);
  return ret;
}

int tickit_hooklist_bind_event(struct TickitHooklist *hooklist, void *owner, int evindex, TickitBindFlags flags,
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
  (*newhookp)->evindex = evindex;
  (*newhookp)->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
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

    if(hook->flags & TICKIT_EV_UNBIND)
      (*hook->fn)(owner, TICKIT_EV_UNBIND, NULL, hook->data);

    // zero out the structure
    hook->evindex = -1;
    hook->fn = NULL;

    if(!hooklist->is_iterating) {
      *hookp = hook->next;
      hook->next = NULL;

      free(hook);
      /* no hookp update */
    }
    else {
      hooklist->needs_delete = true;

      hook->id = HOOK_ID_TOMBSTONE;
      hookp = &(hook->next);
    }
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
    if(hook->evindex == 0 ||
        hook->flags & (TICKIT_EV_UNBIND|TICKIT_EV_DESTROY))
      (*hook->fn)(owner, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY, NULL, hook->data);
    free(hook);
    hook = next;
  }
}
