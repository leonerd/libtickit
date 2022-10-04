/* We need sigaction() and struct sigaction */
#ifdef __GLIBC__
#  define _POSIX_C_SOURCE 199309L
#endif

#ifdef __linux__
#  define HAVE_PPOLL 1
#  define _GNU_SOURCE
#endif

/* TODO: Rumour has some FreeBSDs have ppoll() now as well; we should support
 * that somehow
 */

#ifndef HAVE_PPOLL
#  define HAVE_PPOLL 0
#endif

#include "tickit.h"
#include "tickit-evloop.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>

typedef struct {
  Tickit *t;

  volatile int still_running;

  int alloc_fds;
  int nfds;
  struct pollfd *pollfds;
  TickitWatch **pollwatches;

#if HAVE_PPOLL
  int alloc_signals;
  int nsignals;
  int *signums;
  sigset_t defmask;
  sigset_t watched_signals;
#endif

  sigset_t pending_signals;
} EventLoopData;

/* TODO: For now this only allows one toplevel instance
 */

static EventLoopData *signal_observer;

static void sighandler(int signum)
{
  if(signal_observer) {
    sigaddset(&signal_observer->pending_signals, signum);
  }
}

static void dispatch_signals(EventLoopData *evdata)
{
  sigset_t pending;
  {
    sigset_t orig;
    sigset_t block;
#if HAVE_PPOLL
    block = evdata->watched_signals;
#endif
    sigprocmask(SIG_BLOCK, &block, &orig);

    pending = evdata->pending_signals;
    sigemptyset(&evdata->pending_signals);

    sigprocmask(SIG_SETMASK, &orig, NULL);
  }

#if HAVE_PPOLL
  for(int signum = 1; signum < NSIG; signum++) {
    if(!sigismember(&pending, signum))
      continue;
    if(!sigismember(&evdata->watched_signals, signum))
      continue;

    tickit_evloop_invoke_sigwatches(evdata->t, signum);
  }
#endif
}

static void *evloop_init(Tickit *t, void *initdata)
{
  EventLoopData *evdata = malloc(sizeof(*evdata));
  if(!evdata)
    return NULL;

  evdata->t = t;

  evdata->alloc_fds = 4; /* most programs probably won't use more than 1 FD anyway */
  evdata->nfds = 0;

  evdata->pollfds     = malloc(sizeof(struct pollfd) * evdata->alloc_fds);
  evdata->pollwatches = malloc(sizeof(TickitWatch *) * evdata->alloc_fds);

#if HAVE_PPOLL
  evdata->alloc_signals = 2; /* most programs probably won't use more than 2 anyway */
  evdata->nsignals = 0;

  evdata->signums = malloc(sizeof(int) * evdata->alloc_signals);

  sigemptyset(&evdata->defmask);
  sigemptyset(&evdata->watched_signals);
#endif

  if(!signal_observer)
    signal_observer = evdata;

  return evdata;
}

static void evloop_destroy(void *data)
{
  EventLoopData *evdata = data;

  if(evdata->pollfds)
    free(evdata->pollfds);
  if(evdata->pollwatches)
    free(evdata->pollwatches);
  if(evdata->signums)
    free(evdata->signums);

  if(signal_observer == evdata)
    signal_observer = NULL;

  free(evdata);
}

static void evloop_run(void *data, TickitRunFlags flags)
{
  EventLoopData *evdata = data;

  evdata->still_running = 1;

  while(evdata->still_running) {
    int msec = tickit_evloop_next_timer_msec(evdata->t);

    if(flags & TICKIT_RUN_NOHANG)
      msec = 0;

    int pollret;
#if HAVE_PPOLL
    struct timespec timeout;
    if(msec > -1) {
      timeout.tv_sec  = (msec / 1000);
      timeout.tv_nsec = (msec % 1000) * 1000000;
    }
    pollret = ppoll(evdata->pollfds, evdata->nfds, msec > -1 ? &timeout : NULL, &evdata->defmask);
#else
    pollret = poll(evdata->pollfds, evdata->nfds, msec);
#endif

    tickit_evloop_invoke_timers(evdata->t);

    if(pollret > 0) {
      for(int idx = 0; idx < evdata->nfds; idx++) {
        if(evdata->pollfds[idx].fd == -1)
          continue;

        int revents = evdata->pollfds[idx].revents;
        /* If there are any revents we presume them interesting */
        if(!revents)
          continue;

        TickitIOCondition cond = 0;
        if(revents & POLLIN)
          cond |= TICKIT_IO_IN;
        if(revents & POLLOUT)
          cond |= TICKIT_IO_OUT;
        if(revents & POLLHUP)
          cond |= TICKIT_IO_HUP;
        if(revents & POLLERR)
          cond |= TICKIT_IO_ERR;
        if(revents & POLLNVAL)
          cond |= TICKIT_IO_INVAL;

        tickit_evloop_invoke_iowatch(evdata->pollwatches[idx], TICKIT_EV_FIRE, cond);
      }
    }
    else if(pollret < 0 && errno == EINTR) {
      dispatch_signals(evdata);
    }

    if(flags & (TICKIT_RUN_ONCE|TICKIT_RUN_NOHANG))
      return;
  }
}

static void evloop_stop(void *data)
{
  EventLoopData *evdata = data;

  evdata->still_running = 0;
}

static bool evloop_io(void *data, int fd, TickitIOCondition cond, TickitBindFlags flags, TickitWatch *watch)
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
      return false;
    TickitWatch **newpollwatches = realloc(evdata->pollwatches,
        sizeof(TickitWatch *) * evdata->alloc_fds * 2);
    if(!newpollwatches)
      return false;

    evdata->alloc_fds *= 2;
    evdata->pollfds     = newpollfds;
    evdata->pollwatches = newpollwatches;
  }

  idx = evdata->nfds;
  evdata->nfds++;

reuse_idx:
  evdata->pollfds[idx].fd = fd;
  int events = 0;
  if(cond & TICKIT_IO_IN)
    events |= POLLIN;
  if(cond & TICKIT_IO_OUT)
    events |= POLLOUT;
  if(cond & TICKIT_IO_HUP)
    events |= POLLHUP;
  evdata->pollfds[idx].events = events;

  evdata->pollwatches[idx] = watch;

  tickit_evloop_set_watch_data_int(watch, idx);

  return true;
}

static void evloop_cancel_io(void *data, TickitWatch *watch)
{
  EventLoopData *evdata = data;

  int idx = tickit_evloop_get_watch_data_int(watch);

  evdata->pollfds[idx].fd = -1;
  evdata->pollwatches[idx] = NULL;
}

#if HAVE_PPOLL

static bool evloop_signal(void *data, int signum, TickitBindFlags flags, TickitWatch *watch)
{
  EventLoopData *evdata = data;

  int idx;
  for(idx = 0; idx < evdata->nsignals; idx++)
    if(evdata->signums[idx] == 0)
      goto reuse_idx;

  if(idx == evdata->nsignals && evdata->nsignals == evdata->alloc_signals) {
    /* Grow the signals arrray */
    int *newsignums = realloc(evdata->signums,
        sizeof(int) * evdata->alloc_signals * 2);
    if(!newsignums)
      return false;

    evdata->alloc_signals *= 2;
    evdata->signums    = newsignums;
  }

  idx = evdata->nsignals;
  evdata->nsignals++;

reuse_idx:
  evdata->signums[idx] = signum;

  tickit_evloop_set_watch_data_int(watch, idx);

  if(sigismember(&evdata->watched_signals, signum))
    return true;

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signum);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  sigaction(signum, &(struct sigaction){ .sa_handler = sighandler }, NULL);
  sigaddset(&evdata->watched_signals, signum);

  return true;
}

static void evloop_cancel_signal(void *data, TickitWatch *watch)
{
  EventLoopData *evdata = data;

  int idx = tickit_evloop_get_watch_data_int(watch);

  int signum = evdata->signums[idx];

  evdata->signums[idx] = 0;

  for(idx = 0; idx < evdata->nsignals; idx++) {
    if(evdata->signums[idx] == 0)
      continue;
    if(evdata->signums[idx] == signum)
      return;
  }

  sigdelset(&evdata->watched_signals, signum);
  sigaction(signum, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signum);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

#endif /* HAVE_PPOLL */

TickitEventHooks tickit_evloop_default = {
  .init      = evloop_init,
  .destroy   = evloop_destroy,
  .run       = evloop_run,
  .stop      = evloop_stop,
  .io        = evloop_io,
  .cancel_io = evloop_cancel_io,
#if HAVE_PPOLL
  .signal        = evloop_signal,
  .cancel_signal = evloop_cancel_signal,
#endif
};
