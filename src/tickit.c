#include "tickit.h"
#include "tickit-evloop.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

/* POSIX doesn't actually define an NSIG macro, but if it did, almost every
 * known platform would set it to 32
 */
#ifndef NSIG
#  define NSIG 32
#endif

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
    WATCH_SIGNAL,
    WATCH_PROCESS,
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

    struct {
      int signum;
    } signal;

    struct {
      pid_t pid;
      int wstatus; /* in case of pre-exited process */
    } process;
  };
};

extern TickitEventHooks tickit_evloop_default;

struct Tickit {
  int refcount;

  TickitTerm   *term;
  TickitWindow *rootwin;

  TickitWatch *iowatches, *timers, *laters, *signals, *processes;

  const TickitEventHooks *evhooks;
  void                   *evdata;

  struct {
    int pipefds[2];
    void *pipewatch;
    sigset_t watched;
    sigset_t pending;
  } signal;

  void *sigchldwatch;

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

static Tickit *signal_observer;

static void sighandler(int signum)
{
  if(signal_observer) {
    sigaddset(&signal_observer->signal.pending, signum);
    write(signal_observer->signal.pipefds[1], "\0", 1);
  }
}

static int on_sigpipe_readable(Tickit *t, TickitEventFlags flags, void *info, void *user)
{
  char buf[1];
  read(t->signal.pipefds[0], &buf, 1);

  sigset_t pending;
  {
    sigset_t orig;
    sigprocmask(SIG_BLOCK, &t->signal.watched, &orig);

    pending = t->signal.pending;
    sigemptyset(&t->signal.pending);

    sigprocmask(SIG_SETMASK, &orig, NULL);
  }

  TickitWatch *this;
  for(this = t->signals; this; this = this->next) {
    if(sigismember(&pending, this->signal.signum))
      (*this->fn)(this->t, TICKIT_EV_FIRE, NULL, this->user);
  }

  return 0;
}

static int on_sigwinch(Tickit *t, TickitEventFlags flags, void *info, void *user)
{
  if(!t->term)
    return 0;

  tickit_term_refresh_size(t->term);
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
  t->signals   = NULL;
  t->processes = NULL;

  t->signal.pipefds[0] = -1;
  t->signal.pipewatch = NULL;
  sigemptyset(&t->signal.watched);
  sigemptyset(&t->signal.pending);

  t->sigchldwatch = NULL;

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

  tickit_watch_io(t, tickit_term_get_input_fd(tt), TICKIT_IO_IN,
      0, on_term_readable, NULL);

  tickit_watch_signal(t, SIGWINCH, 0, on_sigwinch, NULL);

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

static void insert_watch(TickitWatch **watchesptr, TickitBindFlags flags, TickitWatch *new)
{
  if(!(flags & TICKIT_BIND_FIRST)) {
    while(*watchesptr)
      watchesptr = &(*watchesptr)->next;
  }

  new->next = *watchesptr;
  *watchesptr = new;
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

static void invoke_watch(TickitWatch *watch, TickitEventFlags flags, void *info)
{
  (*watch->fn)(watch->t, flags, info, watch->user);

  /* Remove oneshot watches from the list */
  TickitWatch **prevp;
  switch(watch->type) {
    case WATCH_NONE:
    case WATCH_IO:
    case WATCH_SIGNAL:
      return;

    case WATCH_TIMER:
      prevp = &watch->t->timers;
      break;

    case WATCH_LATER:
      prevp = &watch->t->laters;
      break;

    case WATCH_PROCESS:
      prevp = &watch->t->processes;
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

static void tickit_destroy(Tickit *t)
{
  if(t->done_setup)
    teardownterm(t);

  if(t->rootwin)
    tickit_window_unref(t->rootwin);
  if(t->term) {
    tickit_term_teardown(t->term);
    tickit_term_unref(t->term);
  }

  if(t->sigchldwatch)
    tickit_watch_cancel(t, t->sigchldwatch);

  if(t->signal.pipewatch)
    tickit_watch_cancel(t, t->signal.pipewatch);
  for(int signum = 1; signum < NSIG; signum++) {
    if(sigismember(&t->signal.watched, signum))
      sigaction(signum, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);
  }
  if(signal_observer == t)
    signal_observer = NULL;

  if(t->iowatches)
    destroy_watchlist(t, t->iowatches, t->evhooks->cancel_io);
  if(t->timers)
    destroy_watchlist(t, t->timers, t->evhooks->cancel_timer);
  if(t->laters)
    destroy_watchlist(t, t->laters, t->evhooks->cancel_later);
  if(t->signals)
    destroy_watchlist(t, t->signals, t->evhooks->cancel_signal);
  if(t->processes)
    destroy_watchlist(t, t->processes, t->evhooks->cancel_process);

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

static int on_sigint(Tickit *t, TickitEventFlags flags, void *info, void *data)
{
  tickit_stop(t);
  return 0;
}

void tickit_tick(Tickit *t, TickitRunFlags flags)
{
  if(!t->done_setup && !(flags & TICKIT_RUN_NOSETUP))
    setupterm(t);

  (*t->evhooks->run)(t->evdata, TICKIT_RUN_ONCE | flags);
}

void tickit_run(Tickit *t)
{
  void *sigint_watch = tickit_watch_signal(t, SIGINT, 0, &on_sigint, NULL);

  if(!t->done_setup)
    setupterm(t);

  (*t->evhooks->run)(t->evdata, TICKIT_RUN_DEFAULT);

  tickit_watch_cancel(t, sigint_watch);
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

  insert_watch(&t->iowatches, flags, watch);

  return watch;

fail:
  free(watch);
  return NULL;
}

void *tickit_watch_io_read(Tickit *t, int fd, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  return tickit_watch_io(t, fd, TICKIT_IO_IN, flags, fn, user);
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
  /* Try to insert in-order at matching timestamp; don't use insert_watch() */
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

  insert_watch(&t->laters, flags, watch);

  return watch;

fail:
  free(watch);
  return NULL;
}

static void watch_signal(Tickit *t, int signum, TickitWatch *watch)
{
  if(t->signal.pipefds[0] == -1) {
    pipe(t->signal.pipefds);
    t->signal.pipewatch = tickit_watch_io(t, t->signal.pipefds[0], TICKIT_IO_IN, 0, &on_sigpipe_readable, t);
  }

  if(sigismember(&t->signal.watched, signum))
    return;

  sigaction(signum, &(struct sigaction){ .sa_handler = sighandler }, NULL);
  sigaddset(&t->signal.watched, signum);

  if(!signal_observer)
    signal_observer = t;
}

static void unwatch_signal(Tickit *t, TickitWatch *watch)
{
  int signum = watch->signal.signum;

  /* watch has already been removed from t->signals */
  TickitWatch *this;
  for(this = t->signals; this; this = this->next) {
    if(this->signal.signum == signum)
      return;
  }

  sigdelset(&t->signal.watched, signum);
  sigaction(signum, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);
}

void *tickit_watch_signal(Tickit *t, int signum, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  TickitWatch *watch = malloc(sizeof(TickitWatch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->t    = t;
  watch->type = WATCH_SIGNAL;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  watch->signal.signum = signum;

  if(!t->evhooks->signal ||
      !(*t->evhooks->signal)(t->evdata, signum, flags, watch))
    watch_signal(t, signum, watch);

  insert_watch(&t->signals, flags, watch);

  return watch;
}

static int on_sigchld(Tickit *t, TickitEventFlags flags, void *info, void *data)
{
  TickitWatch *this, *next;
  for(this = t->processes; this; this = next) {
    next = this->next;

    TickitProcessWatchInfo info;
    if(waitpid(this->process.pid, &info.wstatus, WNOHANG) <= 0)
      continue;

    info.pid = this->process.pid;
    invoke_watch(this, TICKIT_EV_FIRE, &info);
  }
  return 0;
}

static int process_notify(Tickit *t, TickitEventFlags flags, void *_info, void *data)
{
  TickitWatch *watch = data;

  tickit_evloop_invoke_processwatch(watch, TICKIT_EV_FIRE, watch->process.wstatus);

  return 0;
}

void *tickit_watch_process(Tickit *t, pid_t pid, TickitBindFlags flags, TickitCallbackFn *fn, void *user)
{
  TickitWatch *watch = malloc(sizeof(TickitWatch));
  if(!watch)
    return NULL;

  watch->next = NULL;
  watch->t    = t;
  watch->type = WATCH_PROCESS;

  watch->flags = flags & (TICKIT_BIND_UNBIND|TICKIT_BIND_DESTROY);
  watch->fn = fn;
  watch->user = user;

  watch->process.pid = pid;

  if(!t->evhooks->process ||
      !(*t->evhooks->process)(t->evdata, pid, flags, watch)) {
    if(!t->sigchldwatch)
      t->sigchldwatch = tickit_watch_signal(t, SIGCHLD, 0, &on_sigchld, NULL);

    if(waitpid(pid, &watch->process.wstatus, WNOHANG) > 0) {
      /* Process already exited, so SIGCHLD won't see it. We can't invoke
       * callback immediately as user will be expecting it to only be called via
       * tickit_run(). We'll install a later handler for it
       */
      tickit_watch_later(t, 0, process_notify, watch);

      return watch;
    }
  }

  insert_watch(&t->processes, flags, watch);

  return watch;
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
    case WATCH_SIGNAL:
      thisp = &t->signals;
      break;
    case WATCH_PROCESS:
      thisp = &t->processes;
      break;

    case WATCH_NONE:
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
        case WATCH_SIGNAL:
          if(t->evhooks->cancel_signal)
            (*t->evhooks->cancel_signal)(t->evdata, this);
          else
            unwatch_signal(t, this);
          break;
        case WATCH_PROCESS:
          if(t->evhooks->cancel_process)
            (*t->evhooks->cancel_process)(t->evdata, this);
          break;

        case WATCH_NONE:
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

void tickit_evloop_invoke_processwatch(TickitWatch *watch, TickitEventFlags flags, int wstatus)
{
  invoke_watch(watch, flags, &(TickitProcessWatchInfo){
    .pid     = watch->process.pid,
    .wstatus = wstatus,
  });
}

void tickit_evloop_invoke_sigwatches(Tickit *t, int signum)
{
  TickitWatch *this;
  for(this = t->signals; this; this = this->next) {
    if(this->signal.signum == signum)
      (*this->fn)(this->t, TICKIT_EV_FIRE, NULL, this->user);
  }
}

void tickit_evloop_sigwinch(Tickit *t)
{
  /* No longer need to do anything, as on_sigwinch() has already handled it
   * But this function needs to remain for ABI reasons
   */
}

const char *tickit_ctl_name(TickitCtl ctl)
{
  switch(ctl) {
    case TICKIT_CTL_USE_ALTSCREEN: return "use-altscreen";

    case TICKIT_N_CTLS: ;
  }
  return NULL;
}

TickitCtl tickit_ctl_lookup(const char *name)
{
  const char *s;

  for(TickitCtl ctl = 1; ctl < TICKIT_N_CTLS; ctl++)
    if((s = tickit_ctl_name(ctl)) && streq(name, s))
      return ctl;

  return -1;
}

TickitType tickit_ctl_type(TickitCtl ctl)
{
  switch(ctl) {
    case TICKIT_CTL_USE_ALTSCREEN:
      return TICKIT_TYPE_BOOL;

    case TICKIT_N_CTLS:
      ;
  }
  return TICKIT_TYPE_NONE;
}

const char *tickit_ctlname(TickitCtl ctl) { return tickit_ctl_name(ctl); }
TickitCtl tickit_lookup_ctl(const char *name) { return tickit_ctl_lookup(name); }
TickitType tickit_ctltype(TickitCtl ctl) { return tickit_ctl_type(ctl); }

int tickit_version_major(void) { return TICKIT_VERSION_MAJOR; }
int tickit_version_minor(void) { return TICKIT_VERSION_MINOR; }
int tickit_version_patch(void) { return TICKIT_VERSION_PATCH; }
