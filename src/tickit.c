#include "tickit.h"

#include <signal.h>

struct Tickit {
  int refcount;

  TickitTerm   *term;
  TickitWindow *rootwin;
};

Tickit *tickit_new(void)
{
  Tickit *t = malloc(sizeof(Tickit));
  if(!t)
    return NULL;

  t->refcount = 1;

  t->term = NULL;
  t->rootwin = NULL;

  return t;
}

static void tickit_destroy(Tickit *t)
{
  if(t->rootwin)
    tickit_window_unref(t->rootwin);
  if(t->term)
    tickit_term_unref(t->term);

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

    tickit_term_await_started_msec(tt, 50);

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_DRAG);
    tickit_term_setctl_int(tt, TICKIT_TERMCTL_KEYPAD_APP, 1);

    tickit_term_clear(tt);

    t->term = tt;
  }

  return t->term;
}

TickitWindow *tickit_get_rootwin(Tickit *t)
{
  if(!t->rootwin) {
    TickitTerm *tt = tickit_get_term(t);
    if(!tt)
      return NULL;

    t->rootwin = tickit_window_new_root(tt);
  }

  return t->rootwin;
}

// TODO: copy the entire SIGWINCH-like structure from term.c
int still_running;

static void sigint(int sig)
{
  still_running = 0;
}

void tickit_run(Tickit *t)
{
  still_running = 1;
  signal(SIGINT, sigint);

  while(still_running) {
    if(t->rootwin)
      tickit_window_flush(t->rootwin);
    if(t->term)
      tickit_term_input_wait_msec(t->term, -1);
  }
}
