#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TICKIT_EVLOOP_H__
#define __TICKIT_EVLOOP_H__

/*
 * The contents of this file should be considered entirely experimental, and
 * subject to any change at any time. We make no API or ABI stability
 * guarantees at this time.
 */

typedef struct Watch Watch; /* opaque */

typedef struct {
  void *(*init)(Tickit *t);
  void  (*destroy)(void *data);
  void  (*run)(void *data);
  void  (*stop)(void *data);
  bool  (*io_read)(void *data, int fd, TickitBindFlags flags, Watch *watch);
  void  (*cancel_io)(void *data, Watch *watch);
  /* Below here is optional */
  bool  (*timer)(void *data, const struct timeval *at, TickitBindFlags flags, Watch *watch);
  void  (*cancel_timer)(void *data, Watch *watch);
  bool  (*later)(void *data, TickitBindFlags flags, Watch *watch);
  void  (*cancel_later)(void *data, Watch *watch);
} TickitEventHooks;

/* Helper functions for eventloop implementations */

int tickit_evloop_next_timer_msec(Tickit *t);
void tickit_evloop_invoke_timers(Tickit *t);

void *tickit_evloop_get_watch_data(Watch *watch);
void  tickit_evloop_set_watch_data(Watch *watch, void *data);

void tickit_evloop_invoke_watch(Watch *watch, TickitEventFlags flags);

#endif

#ifdef __cplusplus
}
#endif
