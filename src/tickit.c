#include "tickit.h"
#include "tickit-evloop.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define streq(a,b) (!strcmp(a,b))

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
      TickitIOCondition cond;
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

  const TickitEventHooks *evhooks;
  void                   *evdata;

  unsigned int done_setup    : 1,
               use_altscreen : 1;
};

static int on_term_timeout(Tickit *t, TickitEventFlags flags, void *info, void *user);
static int on_term_timeout(Tickit *t, TickitEventFlags flags, void *info, void *user)
{
  int timeout = tickit_term_input_check_timeout_msec(t->term);
  if(timeout > -1)
    tickit_watch_timer_after_msec(t, timeout, 0, on_term_timeout, NULL);

  return 0;
}

static int on_term_readable(Tickit *t, TickitEventFlags flags, void *info, void *user)
{
  tickit_term_input_readable(t->term);

  on_term_timeout(t, TICKIT_EV_FIRE, NULL, NULL);
  return 0;
}

static void setupterm(Tickit *t)
{
  TickitTerm *tt = t->term;

  tickit_term_await_started_msec(tt, 50);

  if(t->use_altscreen)
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_DRAG);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_KEYPAD_APP, 1);

  tickit_term_clear(tt);
  tickit_term_flush(tt);

  t->done_setup = true;
}

static void teardownterm(Tickit *t)
{
  t->done_setup = false;
}

Tickit *tickit_build(const struct TickitBuilder *builder)
{
  Tickit *t = malloc(sizeof(Tickit));
  if(!t)
    return NULL;

  t->refcount = 1;

  t->term = NULL;
  t->rootwin = NULL;

  t->evhooks = builder->evhooks;
  if(!t->evhooks)
    t->evhooks = &tickit_evloop_default;
  t->evdata  = (*t->evhooks->init)(t, builder->evinitdata);
  if(!t->evdata)
    goto abort;

  t->iowatches = NULL;
  t->timers    = NULL;
  t->laters    = NULL;

  t->done_setup = false;

  t->use_altscreen = true;

  TickitTerm *tt = builder->tt;
  if(!tt) {
    struct TickitTermBuilder term_builder = builder->term_builder;
    if(!term_builder.output_buffersize)
      term_builder.output_buffersize = 4096;

    tt = tickit_term_build(&term_builder);
    if(!tt)
      goto abort;
  }
  t->term = tt; /* take ownership */

  tickit_watch_io_read(t, tickit_term_get_input_fd(tt), 0, on_term_readable, NULL);

  return t;

abort:
  free(t);
  return NULL;
}

Tickit *tickit_new_with_evloop(TickitTerm *tt, TickitEventHooks *evhooks, void *initdata)
{
  return tickit_build(&(struct TickitBuilder){
    .tt = tt,
    /* In case tt==NULL */
    .term_builder.open = TICKIT_OPEN_STDTTY,

    .evhooks = evhooks,
    .evinitdata = initdata,
  });
}

Tickit *tickit_new_for_term(TickitTerm *tt)
{
  return tickit_build(&(struct TickitBuilder){
    .tt = tt,
  });
}

Tickit *tickit_new_stdio(void)
{
  return tickit_build(&(const struct TickitBuilder){
    .term_builder.open = TICKIT_OPEN_STDIO,
  });
}

Tickit *tickit_new_stdtty(void)
{
  return tickit_build(&(const struct TickitBuilder){
    .term_builder.open = TICKIT_OPEN_STDTTY,
  });
}

static void destroy_watchlist(Tickit *t, TickitWatch *watches, void (*cancelfunc)(void *data, TickitWatch *watch))
{
  TickitWatch *this, *next;
  for(this = watches; this; this = next) {
    next = this->next;

    if(this->flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY))
      (*this->fn)(this->t, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY, NULL, this->user);

    if(cancelfunc)
      (*cancelfunc)(t->evdata, this);

    free(this);
  }
}

static void tickit_destroy(Tickit *t)
{
  if(t->done_setup)
    teardownterm(t);

  if(t->rootwin)
    tickit_window_unref(t->rootwin);
  if(t->term)
    tickit_term_unref(t->term);

  if(t->iowatches)
    destroy_watchlist(t, t->iowatches, t->evhooks->cancel_io);
  if(t->timers)
    destroy_watchlist(t, t->timers, t->evhooks->cancel_timer);
  if(t->laters)
    destroy_watchlist(t, t->laters, t->evhooks->cancel_later);

  (*t->evhooks->destroy)(t->evdata);

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
  return t->term;
}

TickitWindow *tickit_get_rootwin(Tickit *t)
{
  if(!t->rootwin) {
    t->rootwin = tickit_window_new_root2(t, t->term);
  }

  return t->rootwin;
}

bool tickit_getctl_int(Tickit *t, TickitCtl ctl, int *value)
{
  switch(ctl) {
    case TICKIT_CTL_USE_ALTSCREEN:
      *value = t->use_altscreen;
      return true;

    case TICKIT_N_CTLS:
      ;
  }
  return false;
}

bool tickit_setctl_int(Tickit *t, TickitCtl ctl, int value)
{
  switch(ctl) {
    case TICKIT_CTL_USE_ALTSCREEN:
      t->use_altscreen = value;
      return true;

    case TICKIT_N_CTLS:
      ;
  }
  return false;
}

// TODO: copy the entire SIGWINCH-like structure from term.c
// For now we only handle atmost-one running Tickit instance

static Tickit *running_tickit;

static void sigint(int sig)
{
  if(running_tickit)
    tickit_stop(running_tickit);
}

void tickit_tick(Tickit *t, TickitRunFlags flags)
{
  if(!t->done_setup && !(flags & TICKIT_RUN_NOSETUP))
    setupterm(t);

  (*t->evhooks->run)(t->evdata, TICKIT_RUN_ONCE | flags);
}

void tickit_run(Tickit *t)
{
  running_tickit = t;
  signal(SIGINT, sigint);

  if(!t->done_setup)
    setupterm(t);

  (*t->evhooks->run)(t->evdata, TICKIT_RUN_DEFAULT);

  running_tickit = NULL;
}

void tickit_stop(Tickit *t)
{
  return (*t->evhooks->stop)(t->evdata);
}

void *tickit_watch_io(Tickit *t, int fd, TickitIOCondition cond, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
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

  watch->io.fd   = fd;
  watch->io.cond = cond;

  if(!(*t->evhooks->io)(t->evdata, fd, cond, flags, watch))
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

void *tickit_watch_io_read(Tickit *t, int fd, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  return tickit_watch_io(t, fd, TICKIT_IO_IN|TICKIT_IO_HUP, flags, fn, user);
}

void *tickit_watch_timer_at_tv(Tickit *t, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
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

void *tickit_watch_timer_at_epoch(Tickit *t, time_t at, TickitBindFlags flags, TickitCallbackFn *func, void *user)
{
  return tickit_watch_timer_at_tv(t, &(struct timeval){
      .tv_sec = at,
      .tv_usec = 0,
    }, flags, func, user);
}

void *tickit_watch_timer_after_tv(Tickit *t, const struct timeval *after, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  struct timeval at;
  gettimeofday(&at, NULL);

  /* at + after ==> at */
  timeradd(&at, after, &at);

  return tickit_watch_timer_at_tv(t, &at, flags, fn, user);
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

  TickitWatch **thisp;
  switch(watch->type) {
    case WATCH_IO:
      thisp = &t->iowatches;
      break;
    case WATCH_TIMER:
      thisp = &t->timers;
      break;
    case WATCH_LATER:
      thisp = &t->laters;
      break;

    default:
      return;
  }

  while(*thisp) {
    TickitWatch *this = *thisp;
    if(this == watch) {
      *thisp = this->next;

      if(this->flags & TICKIT_BIND_UNBIND)
        (*this->fn)(t, TICKIT_EV_UNBIND, NULL, this->user);

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

    if(!thisp || !*thisp)
      break;

    thisp = &(*thisp)->next;
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

      /* TODO: consider what info might point at */
      (*this->fn)(this->t, TICKIT_EV_FIRE|TICKIT_EV_UNBIND, NULL, this->user);

      TickitWatch *next = this->next;
      free(this);
      this = next;
    }

    t->timers = this;
  }

  while(later) {
    (*later->fn)(later->t, TICKIT_EV_FIRE|TICKIT_EV_UNBIND, NULL, later->user);

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

static void invoke_watch(TickitWatch *watch, TickitEventFlags flags, void *info)
{
  (*watch->fn)(watch->t, flags, info, watch->user);

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

void tickit_evloop_invoke_watch(TickitWatch *watch, TickitEventFlags flags)
{
  invoke_watch(watch, flags, NULL);
}

void tickit_evloop_invoke_iowatch(TickitWatch *watch, TickitEventFlags flags, TickitIOCondition cond)
{
  invoke_watch(watch, flags, &(TickitIOWatchInfo){
    .fd   = watch->io.fd,
    .cond = cond,
  });
}

void tickit_evloop_sigwinch(Tickit *t)
{
  if(!t->term)
    return;

  tickit_term_refresh_size(t->term);
}

const char *tickit_ctlname(TickitCtl ctl)
{
  switch(ctl) {
    case TICKIT_CTL_USE_ALTSCREEN: return "use-altscreen";

    case TICKIT_N_CTLS: ;
  }
  return NULL;
}

TickitCtl tickit_lookup_ctl(const char *name)
{
  const char *s;

  for(TickitCtl ctl = 1; ctl < TICKIT_N_CTLS; ctl++)
    if((s = tickit_ctlname(ctl)) && streq(name, s))
      return ctl;

  return -1;
}

TickitType tickit_ctltype(TickitCtl ctl)
{
  switch(ctl) {
    case TICKIT_CTL_USE_ALTSCREEN:
      return TICKIT_TYPE_BOOL;

    case TICKIT_N_CTLS:
      ;
  }
  return TICKIT_TYPE_NONE;
}

int tickit_version_major(void) { return TICKIT_VERSION_MAJOR; }
int tickit_version_minor(void) { return TICKIT_VERSION_MINOR; }
int tickit_version_patch(void) { return TICKIT_VERSION_PATCH; }
