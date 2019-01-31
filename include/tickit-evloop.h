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

typedef struct TickitWatch TickitWatch; /* opaque */

typedef struct {
  void *(*init)(Tickit *t);
  void  (*destroy)(void *data);
  void  (*run)(void *data);
  void  (*stop)(void *data);
  bool  (*io_read)(void *data, int fd, TickitBindFlags flags, TickitWatch *watch);
  void  (*cancel_io)(void *data, TickitWatch *watch);
  /* Below here is optional */
  bool  (*timer)(void *data, const struct timeval *at, TickitBindFlags flags, TickitWatch *watch);
  void  (*cancel_timer)(void *data, TickitWatch *watch);
  bool  (*later)(void *data, TickitBindFlags flags, TickitWatch *watch);
  void  (*cancel_later)(void *data, TickitWatch *watch);
} TickitEventHooks;

/* Helper functions for eventloop implementations */

int tickit_evloop_next_timer_msec(Tickit *t);
void tickit_evloop_invoke_timers(Tickit *t);

void *tickit_evloop_get_watch_data(TickitWatch *watch);
void  tickit_evloop_set_watch_data(TickitWatch *watch, void *data);

int  tickit_evloop_get_watch_data_int(TickitWatch *watch);
void tickit_evloop_set_watch_data_int(TickitWatch *watch, int data);

void tickit_evloop_invoke_watch(TickitWatch *watch, TickitEventFlags flags);

#endif

#ifdef __cplusplus
}
#endif
