/* We need sigaction() and struct sigaction */
#ifdef __GLIBC__
#  define _POSIX_C_SOURCE 199309L
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

  unsigned int pending_sigwinch : 1;
} EventLoopData;

/* TODO: For now this only allows one toplevel instance
 */

static EventLoopData *sigwinch_observer;

static void sigwinch(int signum)
{
  if(sigwinch_observer)
    sigwinch_observer->pending_sigwinch = true;
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

  sigwinch_observer = evdata;
  sigaction(SIGWINCH, &(struct sigaction){ .sa_handler = sigwinch }, NULL);

  return evdata;
}

static void evloop_destroy(void *data)
{
  EventLoopData *evdata = data;

  if(evdata->pollfds)
    free(evdata->pollfds);
  if(evdata->pollwatches)
    free(evdata->pollwatches);

  sigaction(SIGWINCH, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);
  sigwinch_observer = NULL;

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

    int pollret = poll(evdata->pollfds, evdata->nfds, msec);

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
      /* Check the signal flags */
      if(evdata->pending_sigwinch) {
        evdata->pending_sigwinch = false;
        tickit_evloop_sigwinch(evdata->t);
      }
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

TickitEventHooks tickit_evloop_default = {
  .init      = evloop_init,
  .destroy   = evloop_destroy,
  .run       = evloop_run,
  .stop      = evloop_stop,
  .io        = evloop_io,
  .cancel_io = evloop_cancel_io,
};
