#include "tickit.h"
#include "tickit-evloop.h"

#include <poll.h>

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
    int msec = tickit_evloop_next_timer_msec(evdata->t);

    int pollret = poll(evdata->pollfds, evdata->nfds, msec);

    tickit_evloop_invoke_timers(evdata->t);

    if(pollret > 0) {
      for(int idx = 0; idx < evdata->nfds; idx++) {
        if(evdata->pollfds[idx].fd == -1)
          continue;

        if(evdata->pollfds[idx].revents & (POLLIN|POLLHUP|POLLERR))
          tickit_evloop_invoke_watch(evdata->pollwatches[idx], TICKIT_EV_FIRE);
      }
    }
  }
}

static void evloop_stop(void *data)
{
  EventLoopData *evdata = data;

  evdata->still_running = 0;
}

static bool evloop_io_read(void *data, int fd, TickitBindFlags flags, Watch *watch)
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
    Watch **newpollwatches = realloc(evdata->pollwatches,
        sizeof(Watch *) * evdata->alloc_fds * 2);
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
  evdata->pollfds[idx].events = POLLIN;

  evdata->pollwatches[idx] = watch;

  tickit_evloop_set_watch_data(watch, (void*)(intptr_t)idx);

  return true;
}

static void evloop_cancel_io(void *data, Watch *watch)
{
  EventLoopData *evdata = data;

  int idx = (intptr_t)tickit_evloop_get_watch_data(watch);

  evdata->pollfds[idx].fd = -1;
  evdata->pollwatches[idx] = NULL;
}

TickitEventHooks tickit_evloop_default = {
  .init      = evloop_init,
  .destroy   = evloop_destroy,
  .run       = evloop_run,
  .stop      = evloop_stop,
  .io_read   = evloop_io_read,
  .cancel_io = evloop_cancel_io,
};
