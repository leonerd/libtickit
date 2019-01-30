#include "tickit.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/time.h>

/* INTERNAL */
TickitWindow* tickit_window_new_root2(Tickit *t, TickitTerm *term);

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
  void *evdata;

  union {
    struct {
      int fd;
    } io;

    struct {
      struct timeval at;
    } timer;
  };
};

typedef struct {
  void *(*init)(Tickit *t);
  void  (*destroy)(void *data);
  void  (*run)(void *data);
  void  (*stop)(void *data);
  void  (*io_read)(void *data, int fd, TickitBindFlags flags, Watch *watch);
  void  (*cancel_io)(void *data, Watch *watch);
  void  (*timer)(void *data, const struct timeval *at, TickitBindFlags flags, Watch *watch);
  void  (*cancel_timer)(void *data, Watch *watch);
  void  (*later)(void *data, TickitBindFlags flags, Watch *watch);
  void  (*cancel_later)(void *data, Watch *watch);
  } TickitEventHooks;

static TickitEventHooks default_event_loop;

struct Tickit {
  int refcount;

  TickitTerm   *term;
  TickitWindow *rootwin;

  Watch *iowatches, *timers, *laters;

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

static void invoke_watch(Tickit *t, Watch *watch, TickitEventFlags flags)
{
  (*watch->fn)(t, flags, watch->user);
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

Tickit *tickit_new_stdio(void)
{
  return tickit_new_for_term(NULL);
}

static void destroy_watchlist(Tickit *t, Watch *watches)
{
  Watch *this, *next;
  for(this = watches; this; this = next) {
    next = this->next;

    if(this->flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY))
      invoke_watch(t, this, TICKIT_EV_UNBIND|TICKIT_EV_DESTROY);

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
    destroy_watchlist(t, t->iowatches);
  if(t->timers)
    destroy_watchlist(t, t->timers);
  if(t->laters)
    destroy_watchlist(t, t->laters);

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
  Watch *watch = malloc(sizeof(Watch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->type = WATCH_IO;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_UNBIND);
  watch->fn = fn;
  watch->user = user;

  watch->io.fd = fd;

  /*TODO errcheck */
  (*t->evhooks->io_read)(t->evdata, fd, flags, watch);

  Watch **prevp = &t->iowatches;
  while(*prevp)
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;
}

/* static for now until we decide how to expose it */
static void *tickit_watch_timer_at(Tickit *t, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  Watch *watch = malloc(sizeof(Watch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->type = WATCH_TIMER;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  watch->timer.at = *at;

  /* TODO: errcheck */
  (*t->evhooks->timer)(t->evdata, at, flags, watch);

  Watch **prevp = &t->timers;
  /* Try to insert in-order at matching timestamp */
  while(*prevp && !timercmp(&(*prevp)->timer.at, at, >))
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;
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
  Watch *watch = malloc(sizeof(Watch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->type = WATCH_LATER;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  /* TODO: errcheck */
  (*t->evhooks->later)(t->evdata, flags, watch);

  Watch **prevp = &t->laters;
  while(*prevp)
    prevp = &(*prevp)->next;

  watch->next = *prevp;
  *prevp = watch;

  return watch;
}

void tickit_watch_cancel(Tickit *t, void *cookie)
{
  Watch *watch = cookie;

  Watch **prevp;
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
    Watch *this = *prevp;
    if(this == cookie) {
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

static int next_timer_msec(Tickit *t)
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

static void invoke_timers(Tickit *t)
{
  /* detach the later queue before running any events */
  Watch *later = t->laters;
  t->laters = NULL;

  if(t->timers) {
    struct timeval now;
    gettimeofday(&now, NULL);

    /* timer queue is stored ordered, so we can just eat a prefix
     * of it
     */

    Watch *this = t->timers;
    while(this) {
      if(timercmp(&this->timer.at, &now, >))
        break;

      invoke_watch(t, this, TICKIT_EV_FIRE|TICKIT_EV_UNBIND);

      Watch *next = this->next;
      free(this);
      this = next;
    }

    t->timers = this;
  }

  while(later) {
    invoke_watch(t, later, TICKIT_EV_FIRE|TICKIT_EV_UNBIND);

    Watch *next = later->next;
    free(later);
    later = next;
  }
}

/*
 * Internal default event loop implementation
 */

typedef struct {
  Tickit *t;

  volatile int still_running;

  int alloc_fds;
  int nfds;
  struct pollfd *pollfds;
  Watch **pollwatches;
} EventLoopData;

static void *evloop_init(Tickit *t)
{
  EventLoopData *evdata = malloc(sizeof(*evdata));
  if(!evdata)
    return NULL;

  evdata->t = t;

  evdata->alloc_fds = 4; /* most programs probably won't use more than 1 FD anyway */
  evdata->nfds = 0;

  evdata->pollfds     = malloc(sizeof(struct pollfd) * evdata->alloc_fds);
  evdata->pollwatches = malloc(sizeof(Watch *) * evdata->alloc_fds);

  return evdata;
}

static void evloop_destroy(void *data)
{
  EventLoopData *evdata = data;

  if(evdata->pollfds)
    free(evdata->pollfds);
  if(evdata->pollwatches)
    free(evdata->pollwatches);
}

static void evloop_run(void *data)
{
  EventLoopData *evdata = data;

  evdata->still_running = 1;

  while(evdata->still_running) {
    int msec = next_timer_msec(evdata->t);

    int pollret = poll(evdata->pollfds, evdata->nfds, msec);

    invoke_timers(evdata->t);

    if(pollret > 0) {
      for(int idx = 0; idx < evdata->nfds; idx++) {
        if(evdata->pollfds[idx].fd == -1)
          continue;

        if(evdata->pollfds[idx].revents & (POLLIN|POLLHUP|POLLERR))
          invoke_watch(evdata->t, evdata->pollwatches[idx], TICKIT_EV_FIRE);
      }
    }
  }
}

static void evloop_stop(void *data)
{
  EventLoopData *evdata = data;

  evdata->still_running = 0;
}

static void evloop_io_read(void *data, int fd, TickitBindFlags flags, Watch *watch)
{
  EventLoopData *evdata = data;

  int idx;
  for(idx = 0; idx < evdata->nfds; idx++)
    if(evdata->pollfds[idx].fd == -1)
      goto reuse_idx;

  if(idx == evdata->nfds && evdata->nfds == evdata->alloc_fds) {
    /* Grow the pollfd array */
    struct pollfd *newpollfds = realloc(evdata->pollfds,
        sizeof(struct pollfd) * evdata->alloc_fds * 2);
    if(!newpollfds)
      return;
    Watch **newpollwatches = realloc(evdata->pollwatches,
        sizeof(Watch *) * evdata->alloc_fds * 2);
    if(!newpollwatches)
      return;

    evdata->alloc_fds *= 2;
    evdata->pollfds     = newpollfds;
    evdata->pollwatches = newpollwatches;
  }

  idx = evdata->nfds;
  evdata->nfds++;

reuse_idx:
  evdata->pollfds[idx].fd = fd;
  evdata->pollfds[idx].events = POLLIN;

  evdata->pollwatches[idx] = watch;

  watch->evdata = (void *)(intptr_t)idx;
}

static void evloop_cancel_io(void *data, Watch *watch)
{
  EventLoopData *evdata = data;

  int idx = (intptr_t)(watch->evdata);

  evdata->pollfds[idx].fd = -1;
  evdata->pollwatches[idx] = NULL;
}

static void evloop_timer(void *data, const struct timeval *at, TickitBindFlags flags, Watch *watch)
{
  // nothing to do
}

static void evloop_later(void *data, TickitBindFlags flags, Watch *watch)
{
  // nothing to do
}

static TickitEventHooks default_event_loop = {
  .init      = evloop_init,
  .destroy   = evloop_destroy,
  .run       = evloop_run,
  .stop      = evloop_stop,
  .io_read   = evloop_io_read,
  .cancel_io = evloop_cancel_io,
  .timer     = evloop_timer,
  .later     = evloop_later,
};
