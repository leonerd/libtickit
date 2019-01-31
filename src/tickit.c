#include "tickit.h"
#include "tickit-evloop.h"

#include <errno.h>
#include <signal.h>
#include <sys/time.h>

/* INTERNAL */
TickitWindow* tickit_window_new_root2(Tickit *t, TickitTerm *term);

struct TickitWatch {
  TickitWatch *next;

  Tickit *t; // uncounted

  enum {
    WATCH_NONE,
    WATCH_IO,
    WATCH_TIMER,
    WATCH_LATER,
  } type;

  TickitBindFlags flags;
  TickitCallbackFn *fn;
  void *user;

  union {
    void *ptr;
    int   i;
  } evdata;

  union {
    struct {
      int fd;
    } io;

    struct {
      struct timeval at;
    } timer;
  };
};

extern TickitEventHooks tickit_evloop_default;

struct Tickit {
  int refcount;

  TickitTerm   *term;
  TickitWindow *rootwin;

  TickitWatch *iowatches, *timers, *laters;

  TickitEventHooks *evhooks;
  void             *evdata;
};

static int on_term_timeout(Tickit *t, TickitEventFlags flags, void *user);
static int on_term_timeout(Tickit *t, TickitEventFlags flags, void *user)
{
  int timeout = tickit_term_input_check_timeout_msec(t->term);
  if(timeout > -1)
    tickit_watch_timer_after_msec(t, timeout, 0, on_term_timeout, NULL);

  return 0;
}

static int on_term_readable(Tickit *t, TickitEventFlags flags, void *user)
{
  tickit_term_input_readable(t->term);

  on_term_timeout(t, TICKIT_EV_FIRE, NULL);
  return 0;
}

static void setterm(Tickit *t, TickitTerm *tt);
static void setterm(Tickit *t, TickitTerm *tt)
{
  t->term = tt; /* take ownership */

  tickit_watch_io_read(t, tickit_term_get_input_fd(tt), 0, on_term_readable, NULL);
}

Tickit *tickit_new_with_evloop(TickitTerm *tt, TickitEventHooks *evhooks)
{
  Tickit *t = malloc(sizeof(Tickit));
  if(!t)
    return NULL;

  t->refcount = 1;

  t->term = NULL;
  t->rootwin = NULL;

  t->evhooks = evhooks;
  t->evdata  = (*t->evhooks->init)(t);
  if(!t->evdata)
    goto abort;

  t->iowatches = NULL;
  t->timers    = NULL;
  t->laters    = NULL;

  if(tt)
    setterm(t, tt);

  return t;

abort:
  free(t);
  return NULL;
}

Tickit *tickit_new_for_term(TickitTerm *tt)
{
  return tickit_new_with_evloop(tt, &tickit_evloop_default);
}

Tickit *tickit_new_stdio(void)
{
  return tickit_new_for_term(NULL);
}

static void destroy_watchlist(Tickit *t, TickitWatch *watches, void (*cancelfunc)(void *data, TickitWatch *watch))
{
  TickitWatch *this, *next;
  for(this = watches; this; this = next) {
    next = this->next;

    if(this->flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY))
      (*this->fn)(this->t, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY, this->user);

    if(cancelfunc)
      (*cancelfunc)(t->evdata, this);

    free(this);
  }
}

static void tickit_destroy(Tickit *t)
{
  if(t->rootwin)
    tickit_window_unref(t->rootwin);
  if(t->term)
    tickit_term_unref(t->term);

  (*t->evhooks->destroy)(t->evdata);

  if(t->iowatches)
    destroy_watchlist(t, t->iowatches, t->evhooks->cancel_io);
  if(t->timers)
    destroy_watchlist(t, t->timers, t->evhooks->cancel_timer);
  if(t->laters)
    destroy_watchlist(t, t->laters, t->evhooks->cancel_later);

  free(t);
}

Tickit *tickit_ref(Tickit *t)
{
  t->refcount++;
  return t;
}

void tickit_unref(Tickit *t)
{
  t->refcount--;
  if(!t->refcount)
    tickit_destroy(t);
}

TickitTerm *tickit_get_term(Tickit *t)
{
  if(!t->term) {
    TickitTerm *tt = tickit_term_open_stdio();
    if(!tt)
      return NULL;

    setterm(t, tt);
  }

  return t->term;
}

static void setupterm(Tickit *t)
{
  TickitTerm *tt = tickit_get_term(t);

  tickit_term_await_started_msec(tt, 50);

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_DRAG);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_KEYPAD_APP, 1);

  tickit_term_clear(tt);
}

static void teardownterm(Tickit *t)
{
}

TickitWindow *tickit_get_rootwin(Tickit *t)
{
  if(!t->rootwin) {
    TickitTerm *tt = tickit_get_term(t);
    if(!tt)
      return NULL;

    t->rootwin = tickit_window_new_root2(t, tt);
  }

  return t->rootwin;
}

// TODO: copy the entire SIGWINCH-like structure from term.c
// For now we only handle atmost-one running Tickit instance

static Tickit *running_tickit;

static void sigint(int sig)
{
  if(running_tickit)
    tickit_stop(running_tickit);
}

void tickit_run(Tickit *t)
{
  running_tickit = t;
  signal(SIGINT, sigint);

  setupterm(t);

  (*t->evhooks->run)(t->evdata);

  teardownterm(t);

  running_tickit = NULL;
}

void tickit_stop(Tickit *t)
{
  return (*t->evhooks->stop)(t->evdata);
}

void *tickit_watch_io_read(Tickit *t, int fd, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  TickitWatch *watch = malloc(sizeof(TickitWatch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->t    = t;
  watch->type = WATCH_IO;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_UNBIND);
  watch->fn = fn;
  watch->user = user;

  watch->io.fd = fd;

  if(!(*t->evhooks->io_read)(t->evdata, fd, flags, watch))
    goto fail;

  TickitWatch **prevp = &t->iowatches;
  while(*prevp)
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;

fail:
  free(watch);
  return NULL;
}

/* static for now until we decide how to expose it */
static void *tickit_watch_timer_at(Tickit *t, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  TickitWatch *watch = malloc(sizeof(TickitWatch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->t    = t;
  watch->type = WATCH_TIMER;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  watch->timer.at = *at;

  if(t->evhooks->timer)
    if(!(*t->evhooks->timer)(t->evdata, at, flags, watch))
      goto fail;

  TickitWatch **prevp = &t->timers;
  /* Try to insert in-order at matching timestamp */
  while(*prevp && !timercmp(&(*prevp)->timer.at, at, >))
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;

fail:
  free(watch);
  return NULL;
}

void *tickit_watch_timer_after_tv(Tickit *t, const struct timeval *after, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  struct timeval at;
  gettimeofday(&at, NULL);

  /* at + after ==> at */
  timeradd(&at, after, &at);

  return tickit_watch_timer_at(t, &at, flags, fn, user);
}

void *tickit_watch_timer_after_msec(Tickit *t, int msec, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  return tickit_watch_timer_after_tv(t, &(struct timeval){
      .tv_sec = msec / 1000,
      .tv_usec = (msec % 1000) * 1000,
    }, flags, fn, user);
}

void *tickit_watch_later(Tickit *t, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  TickitWatch *watch = malloc(sizeof(TickitWatch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->t    = t;
  watch->type = WATCH_LATER;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  if(t->evhooks->later)
    if(!(*t->evhooks->later)(t->evdata, flags, watch))
      goto fail;

  TickitWatch **prevp = &t->laters;
  while(*prevp)
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;

fail:
  free(watch);
  return NULL;
}

void tickit_watch_cancel(Tickit *t, void *_watch)
{
  TickitWatch *watch = _watch;

  TickitWatch **prevp;
  switch(watch->type) {
    case WATCH_IO:
      prevp = &t->timers;
      break;
    case WATCH_TIMER:
      prevp = &t->timers;
      break;
    case WATCH_LATER:
      prevp = &t->laters;
      break;

    default:
      return;
  }

  while(*prevp) {
    TickitWatch *this = *prevp;
    if(this == watch) {
      *prevp = this->next;

      if(this->flags & TICKIT_BIND_UNBIND)
        (*this->fn)(t, TICKIT_EV_UNBIND, this->user);

      switch(this->type) {
        case WATCH_IO:
          (*t->evhooks->cancel_io)(t->evdata, this);
          break;
        case WATCH_TIMER:
          if(t->evhooks->cancel_timer)
            (*t->evhooks->cancel_timer)(t->evdata, this);
          break;
        case WATCH_LATER:
          if(t->evhooks->cancel_later)
            (*t->evhooks->cancel_later)(t->evdata, this);
          break;

        default:
          ;
      }

      free(this);
    }

    prevp = &(*prevp)->next;
  }
}

int tickit_evloop_next_timer_msec(Tickit *t)
{
  if(t->laters)
    return 0;

  if(!t->timers)
    return -1;

  struct timeval now, delay;
  gettimeofday(&now, NULL);

  /* timers->timer.at - now ==> delay */
  timersub(&t->timers->timer.at, &now, &delay);

  int msec = (delay.tv_sec * 1000) + (delay.tv_usec / 1000);
  if(msec < 0)
    msec = 0;

  return msec;
}

void tickit_evloop_invoke_timers(Tickit *t)
{
  /* detach the later queue before running any events */
  TickitWatch *later = t->laters;
  t->laters = NULL;

  if(t->timers) {
    struct timeval now;
    gettimeofday(&now, NULL);

    /* timer queue is stored ordered, so we can just eat a prefix
     * of it
     */

    TickitWatch *this = t->timers;
    while(this) {
      if(timercmp(&this->timer.at, &now, >))
        break;

      (*this->fn)(this->t, TICKIT_EV_FIRE|TICKIT_EV_UNBIND, this->user);

      TickitWatch *next = this->next;
      free(this);
      this = next;
    }

    t->timers = this;
  }

  while(later) {
    (*later->fn)(later->t, TICKIT_EV_FIRE|TICKIT_EV_UNBIND, later->user);

    TickitWatch *next = later->next;
    free(later);
    later = next;
  }
}

void *tickit_evloop_get_watch_data(TickitWatch *watch)
{
  return watch->evdata.ptr;
}

void tickit_evloop_set_watch_data(TickitWatch *watch, void *data)
{
  watch->evdata.ptr = data;
}

int  tickit_evloop_get_watch_data_int(TickitWatch *watch)
{
  return watch->evdata.i;
}

void tickit_evloop_set_watch_data_int(TickitWatch *watch, int data)
{
  watch->evdata.i = data;
}

void tickit_evloop_invoke_watch(TickitWatch *watch, TickitEventFlags flags)
{
  (*watch->fn)(watch->t, flags, watch->user);

  /* Remove oneshot watches from the list */
  TickitWatch **prevp;
  switch(watch->type) {
    case WATCH_NONE:
    case WATCH_IO:
      return;

    case WATCH_TIMER:
      prevp = &watch->t->timers;
      break;

    case WATCH_LATER:
      prevp = &watch->t->laters;
      break;
  }

  while(*prevp) {
    if(*prevp == watch) {
      *prevp = watch->next;
      watch->next = NULL;
      watch->type = WATCH_NONE;
      free(watch);
      return;
    }

    prevp = &(*prevp)->next;
  }
}
