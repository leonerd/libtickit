#include "tickit.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/time.h>

/* INTERNAL */
TickitWindow* tickit_window_new_root2(Tickit *t, TickitTerm *term);

typedef struct {
  void *(*init)(Tickit *t);
  void  (*destroy)(void *data);
  void  (*run)(void *data);
  void  (*stop)(void *data);
  void *(*io_read)(void *data, int fd, TickitBindFlags flags, TickitCallbackFn *fn, void *user);
  void *(*timer)(void *data, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user);
  void *(*later)(void *data, TickitBindFlags flags, TickitCallbackFn *fn, void *user);
  void  (*cancel)(void *data, void *cookie);
} TickitEventHooks;

static TickitEventHooks default_event_loop;

struct Tickit {
  int refcount;

  TickitTerm   *term;
  TickitWindow *rootwin;

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

Tickit *tickit_new_for_term(TickitTerm *tt)
{
  Tickit *t = malloc(sizeof(Tickit));
  if(!t)
    return NULL;

  t->refcount = 1;

  t->term = NULL;
  t->rootwin = NULL;

  t->evhooks = &default_event_loop;
  t->evdata  = (*t->evhooks->init)(t);
  if(!t->evdata)
    goto abort;

  if(tt)
    setterm(t, tt);

  return t;

abort:
  free(t);
  return NULL;
}

Tickit *tickit_new_stdio(void)
{
  return tickit_new_for_term(NULL);
}

static void tickit_destroy(Tickit *t)
{
  if(t->rootwin)
    tickit_window_unref(t->rootwin);
  if(t->term)
    tickit_term_unref(t->term);

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
  return (*t->evhooks->io_read)(t->evdata, fd, flags, fn, user);
}

/* static for now until we decide how to expose it */
static void *tickit_watch_timer_at(Tickit *t, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  return (*t->evhooks->timer)(t->evdata, at, flags, fn, user);
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
  return (*t->evhooks->later)(t->evdata, flags, fn, user);

}

void tickit_watch_cancel(Tickit *t, void *cookie)
{
  (*t->evhooks->cancel)(t->evdata, cookie);
}

/*
 * Internal default event loop implementation
 */

typedef struct Watch Watch;
struct Watch {
  Watch *next;

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
    struct {
      int idx;
      int fd;
    } io;

    struct {
      struct timeval at;
    } timer;
  };
};

typedef struct {
  Tickit *t;

  volatile int still_running;

  Watch *iowatches, *timers, *laters;

  struct pollfd *fds;
  int nfds;
  int alloc_fds;
} EventLoopData;

static void *evloop_init(Tickit *t)
{
  EventLoopData *evdata = malloc(sizeof(*evdata));
  if(!evdata)
    return NULL;

  evdata->t = t;

  evdata->iowatches = NULL;
  evdata->timers    = NULL;
  evdata->laters    = NULL;

  evdata->alloc_fds = 4; /* most programs probably won't use more than 1 FD anyway */
  evdata->fds = malloc(sizeof(struct pollfd) * evdata->alloc_fds);
  evdata->nfds = 0;

  return evdata;
}

static void destroy_watchlist(EventLoopData *evdata, Watch *watches)
{
  Watch *this, *next;
  for(this = watches; this; this = next) {
    next = this->next;

    if(this->flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY))
      (*this->fn)(evdata->t, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY, this->user);

    free(this);
  }
}

static void evloop_destroy(void *data)
{
  EventLoopData *evdata = data;

  if(evdata->iowatches)
    destroy_watchlist(evdata, evdata->iowatches);
  if(evdata->timers)
    destroy_watchlist(evdata, evdata->timers);
  if(evdata->laters)
    destroy_watchlist(evdata, evdata->laters);
}

static void evloop_run(void *data)
{
  EventLoopData *evdata = data;

  evdata->still_running = 1;

  while(evdata->still_running) {
    int msec = -1;
    if(evdata->timers) {
      struct timeval now, delay;
      gettimeofday(&now, NULL);

      /* timers->timer.at - now ==> delay */
      timersub(&evdata->timers->timer.at, &now, &delay);

      msec = (delay.tv_sec * 1000) + (delay.tv_usec / 1000);
      if(msec < 0)
        msec = 0;
    }

    int pollret;

    /* detach the later queue before running any events */
    Watch *later = evdata->laters;
    evdata->laters = NULL;

    if(later)
      msec = 0;

    pollret = poll(evdata->fds, evdata->nfds, msec);

    if(pollret > 0) {
      for(Watch *this = evdata->iowatches; this; this = this->next) {
        if(evdata->fds[this->io.idx].revents & (POLLIN|POLLHUP|POLLERR))
          (*this->fn)(evdata->t, TICKIT_EV_FIRE, this->user);
      }
    }

    if(evdata->timers) {
      struct timeval now;
      gettimeofday(&now, NULL);

      /* timer queue is stored ordered, so we can just eat a prefix
       * of it
       */

      Watch *this = evdata->timers;
      while(this) {
        if(timercmp(&this->timer.at, &now, >))
          break;

        (*this->fn)(evdata->t, TICKIT_EV_FIRE|TICKIT_EV_UNBIND, this->user);

        Watch *next = this->next;
        free(this);
        this = next;
      }

      evdata->timers = this;
    }

    while(later) {
      (*later->fn)(evdata->t, TICKIT_EV_FIRE|TICKIT_EV_UNBIND, later->user);

      Watch *next = later->next;
      free(later);
      later = next;
    }
  }
}

static void evloop_stop(void *data)
{
  EventLoopData *evdata = data;

  evdata->still_running = 0;
}

static void *evloop_io_read(void *data, int fd, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  EventLoopData *evdata = data;

  Watch *watch = malloc(sizeof(Watch));
  if(!watch)
    return NULL;

  int idx;
  for(idx = 0; idx < evdata->nfds; idx++)
    if(evdata->fds[idx].fd == -1)
      goto reuse_idx;

  if(idx == evdata->nfds && evdata->nfds == evdata->alloc_fds) {
    /* Allocate and reinitialise a larger pollfd array */
    struct pollfd *new = malloc(sizeof(struct pollfd) * evdata->alloc_fds * 2);
    if(!new)
      return NULL;

    for(Watch *this = evdata->iowatches; this; this = this->next) {
      new[this->io.idx].fd = this->io.fd;
      new[this->io.idx].events = POLLIN;
    }

    evdata->alloc_fds *= 2;
    free(evdata->fds);
    evdata->fds = new;
  }

  idx = evdata->nfds;
  evdata->nfds++;

reuse_idx:
  evdata->fds[idx].fd = fd;
  evdata->fds[idx].events = POLLIN;

  watch->next = NULL;
  watch->type = WATCH_IO;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_UNBIND);
  watch->fn = fn;
  watch->user = user;

  watch->io.idx = idx;
  watch->io.fd = fd;

  Watch **prevp = &evdata->iowatches;
  while(*prevp)
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;
}

static void *evloop_timer(void *data, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  EventLoopData *evdata = data;

  Watch *watch = malloc(sizeof(Watch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->type = WATCH_TIMER;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  watch->timer.at = *at;

  Watch **prevp = &evdata->timers;
  /* Try to insert in-order at matching timestamp */
  while(*prevp && !timercmp(&(*prevp)->timer.at, at, >))
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;
}

static void *evloop_later(void *data, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  EventLoopData *evdata = data;

  Watch *watch = malloc(sizeof(Watch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->type = WATCH_LATER;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  Watch **prevp = &evdata->laters;
  while(*prevp)
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;
}

static void evloop_cancel(void *data, void *cookie)
{
  EventLoopData *evdata = data;
  Watch *watch = cookie;

  Watch **prevp;
  switch(watch->type) {
    case WATCH_IO:
      prevp = &evdata->timers;
      break;
    case WATCH_TIMER:
      prevp = &evdata->timers;
      break;
    case WATCH_LATER:
      prevp = &evdata->laters;
      break;

    default:
      return;
  }

  while(*prevp) {
    Watch *this = *prevp;
    if(this == cookie) {
      *prevp = this->next;

      if(this->flags & TICKIT_BIND_UNBIND)
        (*this->fn)(evdata->t, TICKIT_EV_UNBIND, this->user);

      if(this->type == WATCH_IO) {
        evdata->fds[this->io.idx].fd = -1;
      }

      free(this);
    }

    prevp = &(*prevp)->next;
  }
}

static TickitEventHooks default_event_loop = {
  .init    = evloop_init,
  .destroy = evloop_destroy,
  .run     = evloop_run,
  .stop    = evloop_stop,
  .io_read = evloop_io_read,
  .timer   = evloop_timer,
  .later   = evloop_later,
  .cancel  = evloop_cancel,
};
