#include "bindings.h"

#include <stdlib.h>

struct TickitBinding {
  struct TickitBinding *next;
  int                     id;
  int                     evindex;
  TickitBindFlags         flags;
  TickitEventFn          *fn;
  void                   *data;
};

#define BINDING_ID_TOMBSTONE -1

static void cleanup(struct TickitBindings *bindings)
{
  for(struct TickitBinding **bindp = &bindings->first; *bindp; /**/) {
    struct TickitBinding *bind = *bindp;
    if(bind->id != BINDING_ID_TOMBSTONE) {
      bindp = &(*bindp)->next;
      continue;
    }

    *bindp = bind->next;
    bind->next = NULL;

    free(bind);
  }

  bindings->needs_delete = false;
}

void tickit_bindings_run_event(struct TickitBindings *bindings, void *owner, int evindex, void *info)
{
  int was_iterating = bindings->is_iterating;
  bindings->is_iterating = true;

  for(struct TickitBinding *bind = bindings->first; bind; bind = bind->next)
    if(bind->evindex == evindex)
      (*bind->fn)(owner, TICKIT_EV_FIRE, info, bind->data);

  bindings->is_iterating = was_iterating;
  if(!was_iterating && bindings->needs_delete)
    cleanup(bindings);
}

int tickit_bindings_run_event_whilefalse(struct TickitBindings *bindings, void *owner, int evindex, void *info)
{
  int was_iterating = bindings->is_iterating;
  bindings->is_iterating = true;

  int ret = 0;

  for(struct TickitBinding *bind = bindings->first; bind; bind = bind->next)
    if(bind->evindex == evindex) {
      ret = (*bind->fn)(owner, TICKIT_EV_FIRE, info, bind->data);
      if(ret)
        goto exit;
    }

exit:
  bindings->is_iterating = was_iterating;
  if(!was_iterating && bindings->needs_delete)
    cleanup(bindings);
  return ret;
}

int tickit_bindings_bind_event(struct TickitBindings *bindings, void *owner, int evindex, TickitBindFlags flags,
    TickitEventFn *fn, void *data)
{
  int max_id = 0;

  struct TickitBinding **newp = &bindings->first;
  struct TickitBinding *next = NULL;

  if(flags & TICKIT_BIND_FIRST) {
    next = bindings->first;
    for(struct TickitBinding *bind = *newp; bind; bind = bind->next)
      if(bind->id > max_id)
        max_id = bind->id;
  }
  else {
    for(; *newp; newp = &(*newp)->next)
      if((*newp)->id > max_id)
        max_id = (*newp)->id;
  }

  *newp = malloc(sizeof(struct TickitBinding)); // TODO: malloc failure

  (*newp)->next = next;
  (*newp)->evindex = evindex;
  (*newp)->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  (*newp)->fn = fn;
  (*newp)->data = data;

  return (*newp)->id = max_id + 1;
}

void tickit_bindings_unbind_event_id(struct TickitBindings *bindings, void *owner, int id)
{
  for(struct TickitBinding **bindp = &bindings->first; *bindp; ) {
    struct TickitBinding *bind = *bindp;
    if(bind->id != id) {
      bindp = &(bind->next);
      continue;
    }

    if(bind->flags & TICKIT_EV_UNBIND)
      (*bind->fn)(owner, TICKIT_EV_UNBIND, NULL, bind->data);

    // zero out the structure
    bind->evindex = -1;
    bind->fn = NULL;

    if(!bindings->is_iterating) {
      *bindp = bind->next;
      bind->next = NULL;

      free(bind);
      /* no bindp update */
    }
    else {
      bindings->needs_delete = true;

      bind->id = BINDING_ID_TOMBSTONE;
      bindp = &(bind->next);
    }
  }
}

void tickit_bindings_unbind_and_destroy(struct TickitBindings *bindings, void *owner)
{
  /* TICKIT_EV_DESTROY events need to run in reverse order. Since the bindings
   * is singly-linked it is easiest just to reverse it then iterate.
   */
  struct TickitBinding *revbinds = NULL;
  for(struct TickitBinding *bind = bindings->first; bind; /**/) {
    struct TickitBinding *this = bind;
    bind = bind->next;

    this->next = revbinds;
    revbinds = this;
  }

  for(struct TickitBinding *bind = revbinds; bind;) {
    struct TickitBinding *next = bind->next;
    if(bind->evindex == 0 ||
        bind->flags & (TICKIT_EV_UNBIND|TICKIT_EV_DESTROY))
      (*bind->fn)(owner, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY, NULL, bind->data);
    free(bind);
    bind = next;
  }
}
