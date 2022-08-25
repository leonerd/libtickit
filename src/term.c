/* We need C99 vsnprintf() semantics */
#ifdef __GLIBC__
/* We need C99 vsnprintf() semantics */
#  define _ISOC99_SOURCE
/* We need sigaction() and struct sigaction */
#  define _POSIX_C_SOURCE 199309L
/* We need strdup and va_copy */
#  define _GNU_SOURCE
#endif

#include "tickit.h"
#include "bindings.h"
#include "termdriver.h"

#include "xterm-palette.inc"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#define streq(a,b) (!strcmp(a,b))
#define strneq(a,b,n) (strncmp(a,b,n)==0)

/* unit multipliers for working in microseconds */
#define MSEC      1000
#define SECOND 1000000

#include <termkey.h>

static TickitTermDriverInfo *driver_infos[] = {
  &tickit_termdrv_info_xterm,
  &tickit_termdrv_info_ti,
  NULL,
};

struct TickitTerm {
  int                   outfd;
  TickitTermOutputFunc *outfunc;
  void                 *outfunc_user;

  int                   infd;
  TermKey              *termkey;
  struct timeval        input_timeout_at; /* absolute time */

  struct TickitTerminfoHook ti_hook;

  char *termtype;
  TickitMaybeBool is_utf8;

  char *outbuffer;
  size_t outbuffer_len; /* size of outbuffer */
  size_t outbuffer_cur; /* current fill level */

  char *tmpbuffer;
  size_t tmpbuffer_len;

  TickitTermDriver *driver;

  int lines;
  int cols;

  bool observe_winch;
  TickitTerm *next_sigwinch_observer;

  bool window_changed;

  enum { UNSTARTED, STARTING, STARTED } state;

  int colors;
  TickitPen *pen;

  int refcount;
  struct TickitBindings bindings;

  int mouse_buttons_held;
};

DEFINE_BINDINGS_FUNCS(term,TickitTerm,TickitTermEventFn)

static void *get_tmpbuffer(TickitTerm *tt, size_t len);

static const char *getstr_hook(const char *name, const char *value, void *_tt)
{
  TickitTerm *tt = _tt;

  if(streq(name, "key_backspace")) {
    /* Many terminfos lie about backspace. Rather than trust it even a little
     * tiny smidge, we'll interrogate what termios thinks of the VERASE char
     * and claim that is the backspace key. It's what neovim does
     *
     *   https://github.com/neovim/neovim/blob/1083c626b9a3fc858c552d38250c3c555cda4074/src/nvim/tui/tui.c#L1982
     */
    struct termios termios;

    if(tt->infd != -1 &&
       tcgetattr(tt->infd, &termios) == 0) {
      char *ret = get_tmpbuffer(tt, 2);
      ret[0] = termios.c_cc[VERASE];
      ret[1] = 0;

      value = ret;
    }
  }

  if(tt->ti_hook.getstr)
    value = (*tt->ti_hook.getstr)(name, value, tt->ti_hook.data);

  return value;
}

static TermKey *get_termkey(TickitTerm *tt)
{
  if(!tt->termkey) {
    int flags = 0;
    if(tt->is_utf8 == TICKIT_YES)
      flags |= TERMKEY_FLAG_UTF8;
    else if(tt->is_utf8 == TICKIT_NO)
      flags |= TERMKEY_FLAG_RAW;

    /* A horrible hack: termkey_new doesn't take a termtype;
     * termkey_new_abstract does but doesn't take an fd. When we migrate
     * libtermkey source into here this will be much neater
     */
    {
      const char *was_term = getenv("TERM");
      setenv("TERM", tt->termtype, true);

      tt->termkey = termkey_new(tt->infd, TERMKEY_FLAG_EINTR|TERMKEY_FLAG_NOSTART | flags);

      if(was_term)
        setenv("TERM", was_term, true);
      else
        unsetenv("TERM");
    }

    termkey_hook_terminfo_getstr(tt->termkey, getstr_hook, tt);
    termkey_start(tt->termkey);

    tt->is_utf8 = !!(termkey_get_flags(tt->termkey) & TERMKEY_FLAG_UTF8);
  }

  termkey_set_canonflags(tt->termkey,
      termkey_get_canonflags(tt->termkey) | TERMKEY_CANON_DELBS);

  return tt->termkey;
}

static TickitTermDriver *tickit_term_build_driver(struct TickitTermBuilder *builder)
{
  if(builder->driver)
    return builder->driver;

  TickitTermProbeArgs args = {
    .termtype = builder->termtype,
    .ti_hook  = builder->ti_hook,
  };

  for(int i = 0; driver_infos[i]; i++) {
    TickitTermDriver *driver = (*driver_infos[i]->new)(&args);
    if(driver)
      return driver;
  }

  errno = ENOENT;
  return NULL;
}

TickitTerm *tickit_term_build(const struct TickitTermBuilder *_builder)
{
  struct TickitTermBuilder builder = { 0 };
  if(_builder)
    builder = *_builder;

  if(!builder.termtype)
    builder.termtype = getenv("TERM");
  if(!builder.termtype)
    builder.termtype = "xterm";

  TickitTerm *tt = malloc(sizeof(TickitTerm));
  if(!tt)
    return NULL;

  if(builder.ti_hook)
    tt->ti_hook = *builder.ti_hook;
  else
    tt->ti_hook = (struct TickitTerminfoHook){ 0 };

  TickitTermDriver *driver = tickit_term_build_driver(&builder);
  if(!driver)
    return NULL;

  tt->outfd   = -1;
  tt->outfunc = NULL;

  tt->infd    = -1;
  tt->termkey = NULL;
  tt->input_timeout_at.tv_sec = -1;

  tt->outbuffer = NULL;
  tt->outbuffer_len = 0;
  tt->outbuffer_cur = 0;

  tt->tmpbuffer = NULL;
  tt->tmpbuffer_len = 0;

  tt->is_utf8 = TICKIT_MAYBE;

  /* Initially; the driver may provide a more accurate value */
  tt->lines = 25;
  tt->cols  = 80;

  tt->observe_winch = false;
  tt->next_sigwinch_observer = NULL;
  tt->window_changed = false;

  tt->refcount = 1;
  tt->bindings = (struct TickitBindings){ NULL };

  tt->mouse_buttons_held = 0;

  /* Initially empty because we don't necessarily know the initial state
   * of the terminal
   */
  tt->pen = tickit_pen_new();

  if(builder.termtype)
    tt->termtype = strdup(builder.termtype);
  else
    tt->termtype = NULL;

  tt->driver = driver;
  tt->driver->tt = tt;

  if(tt->driver->vtable->attach)
    (*tt->driver->vtable->attach)(tt->driver, tt);

  tickit_term_getctl_int(tt, TICKIT_TERMCTL_COLORS, &tt->colors);

  // Can't 'start' yet until we have an output method
  tt->state = UNSTARTED;

  int fd_in = -1, fd_out = -1;

  switch(builder.open) {
    case TICKIT_NO_OPEN:
      break;

    case TICKIT_OPEN_FDS:
      fd_in  = builder.input_fd;
      fd_out = builder.output_fd;
      break;

    case TICKIT_OPEN_STDIO:
      fd_in  = STDIN_FILENO;
      fd_out = STDOUT_FILENO;
      break;

    case TICKIT_OPEN_STDTTY:
      for(fd_in = 0; fd_in <= 2; fd_in++) {
        if(isatty(fd_in))
          break;
      }
      if(fd_in > 2) {
        fprintf(stderr, "Cannot find a TTY filehandle\n");
        abort();
      }
      fd_out = fd_in;
      break;
  }

  if(fd_in != -1)
    tickit_term_set_input_fd(tt, fd_in);
  if(fd_out != -1)
    tickit_term_set_output_fd(tt, fd_out);
  if(builder.output_func)
    tickit_term_set_output_func(tt, builder.output_func, builder.output_func_user);

  if(builder.output_buffersize)
    tickit_term_set_output_buffer(tt, builder.output_buffersize);

  return tt;
}

TickitTerm *tickit_term_new(void)
{
  return tickit_term_build(NULL);
}

TickitTerm *tickit_term_new_for_termtype(const char *termtype)
{
  return tickit_term_build(&(struct TickitTermBuilder){
    .termtype = termtype,
  });
}

TickitTerm *tickit_term_open_stdio(void)
{
  TickitTerm *tt = tickit_term_build(&(const struct TickitTermBuilder){
    .open = TICKIT_OPEN_STDIO,
  });
  if(!tt)
    return NULL;

  tickit_term_observe_sigwinch(tt, true);

  return tt;
}

void tickit_term_teardown(TickitTerm *tt)
{
  if(tt->driver && tt->state != UNSTARTED) {
    if(tt->driver->vtable->stop)
      (*tt->driver->vtable->stop)(tt->driver);

    tt->state = UNSTARTED;
  }

  if(tt->termkey)
    termkey_stop(tt->termkey);

  tickit_term_flush(tt);
}

void tickit_term_destroy(TickitTerm *tt)
{
  if(tt->observe_winch)
    tickit_term_observe_sigwinch(tt, false);

  if(tt->driver) {
    tickit_term_teardown(tt);

    (*tt->driver->vtable->destroy)(tt->driver);
  }

  tickit_term_flush(tt);

  if(tt->outfunc)
    (*tt->outfunc)(tt, NULL, 0, tt->outfunc_user);

  tickit_bindings_unbind_and_destroy(&tt->bindings, tt);
  tickit_pen_unref(tt->pen);

  if(tt->termkey)
    termkey_destroy(tt->termkey);

  if(tt->outbuffer)
    free(tt->outbuffer);

  if(tt->tmpbuffer)
    free(tt->tmpbuffer);

  if(tt->termtype)
    free(tt->termtype);

  free(tt);
}

TickitTerm *tickit_term_ref(TickitTerm *tt)
{
  tt->refcount++;
  return tt;
}

void tickit_term_unref(TickitTerm *tt)
{
  if(tt->refcount < 1) {
    fprintf(stderr, "tickit_term_unref: invalid refcount %d\n", tt->refcount);
    abort();
  }
  tt->refcount--;
  if(!tt->refcount)
    tickit_term_destroy(tt);
}

const char *tickit_term_get_termtype(TickitTerm *tt)
{
  return tt->termtype;
}

const char *tickit_term_get_drivername(TickitTerm *tt)
{
  return tt->driver->name;
}

static TickitTermDriverInfo *driverinfo(TickitTerm *tt)
{
  for(int i = 0; driver_infos[i]; i++)
    /* These pointers are copied directly */
    if(driver_infos[i]->name == tt->driver->name)
      return driver_infos[i];

  return NULL;
}

TickitTermDriver *tickit_term_get_driver(TickitTerm *tt)
{
  return tt->driver;
}

int tickit_term_get_driverctl_range(TickitTerm *tt)
{
  TickitTermDriverInfo *info = driverinfo(tt);
  if(info)
    return info->privatectl;
  return 0;
}

static void *get_tmpbuffer(TickitTerm *tt, size_t len)
{
  if(tt->tmpbuffer_len < len) {
    if(tt->tmpbuffer)
      free(tt->tmpbuffer);
    tt->tmpbuffer = malloc(len);
    tt->tmpbuffer_len = len;
  }

  return tt->tmpbuffer;
}

/* Driver API */
void *tickit_termdrv_get_tmpbuffer(TickitTermDriver *ttd, size_t len)
{
  return get_tmpbuffer(ttd->tt, len);
}

void tickit_term_get_size(const TickitTerm *tt, int *lines, int *cols)
{
  if(lines)
    *lines = tt->lines;
  if(cols)
    *cols  = tt->cols;
}

void tickit_term_set_size(TickitTerm *tt, int lines, int cols)
{
  if(tt->lines != lines || tt->cols != cols) {
    tt->lines = lines;
    tt->cols  = cols;

    TickitResizeEventInfo info = { .lines = lines, .cols = cols };
    run_events(tt, TICKIT_TERM_ON_RESIZE, &info);
  }
}

void tickit_term_refresh_size(TickitTerm *tt)
{
  if(tt->outfd == -1)
    return;

  struct winsize ws = { 0, 0, 0, 0 };
  if(ioctl(tt->outfd, TIOCGWINSZ, &ws) == -1)
    return;

  tickit_term_set_size(tt, ws.ws_row, ws.ws_col);
}

static TickitTerm *first_sigwinch_observer;

static void sigwinch(int signum)
{
  for(TickitTerm *tt = first_sigwinch_observer; tt; tt = tt->next_sigwinch_observer)
    tt->window_changed = 1;
}

void tickit_term_observe_sigwinch(TickitTerm *tt, bool observe)
{
  sigset_t newset;
  sigset_t oldset;

  sigemptyset(&newset);
  sigaddset(&newset, SIGWINCH);

  sigprocmask(SIG_BLOCK, &newset, &oldset);

  if(observe && !tt->observe_winch) {
    tt->observe_winch = true;

    if(!first_sigwinch_observer)
      sigaction(SIGWINCH, &(struct sigaction){ .sa_handler = sigwinch }, NULL);

    TickitTerm **tailp = &first_sigwinch_observer;
    while(*tailp)
      tailp = &(*tailp)->next_sigwinch_observer;
    *tailp = tt;
  }
  else if(!observe && tt->observe_winch) {
    TickitTerm **tailp = &first_sigwinch_observer;
    while(tailp && *tailp != tt)
      tailp = &(*tailp)->next_sigwinch_observer;
    if(tailp)
      *tailp = (*tailp)->next_sigwinch_observer;

    if(!first_sigwinch_observer)
      sigaction(SIGWINCH, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);

    tt->observe_winch = false;
  }

  sigprocmask(SIG_SETMASK, &oldset, NULL);
}

static void check_resize(TickitTerm *tt)
{
  if(!tt->window_changed)
    return;

  tt->window_changed = 0;
  tickit_term_refresh_size(tt);
}

void tickit_term_set_output_fd(TickitTerm *tt, int fd)
{
  tt->outfd = fd;

  tickit_term_refresh_size(tt);

  if(tt->state == UNSTARTED) {
    if(tt->driver->vtable->start)
      (*tt->driver->vtable->start)(tt->driver);
    tt->state = STARTING;
  }
}

int tickit_term_get_output_fd(const TickitTerm *tt)
{
  return tt->outfd;
}

void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user)
{
  if(tt->outfunc)
    (*tt->outfunc)(tt, NULL, 0, tt->outfunc_user);

  tt->outfunc      = fn;
  tt->outfunc_user = user;

  if(tt->state == UNSTARTED) {
    if(tt->driver->vtable->start)
      (*tt->driver->vtable->start)(tt->driver);
    tt->state = STARTING;
  }
}

void tickit_term_set_output_buffer(TickitTerm *tt, size_t len)
{
  void *buffer = len ? malloc(len) : NULL;

  if(tt->outbuffer)
    free(tt->outbuffer);

  tt->outbuffer = buffer;
  tt->outbuffer_len = len;
  tt->outbuffer_cur = 0;
}

void tickit_term_set_input_fd(TickitTerm *tt, int fd)
{
  if(tt->termkey)
    termkey_destroy(tt->termkey);

  tt->infd = fd;
  (void)get_termkey(tt);
}

int tickit_term_get_input_fd(const TickitTerm *tt)
{
  return tt->infd;
}

TickitMaybeBool tickit_term_get_utf8(const TickitTerm *tt)
{
  return tt->is_utf8;
}

void tickit_term_set_utf8(TickitTerm *tt, bool utf8)
{
  tt->is_utf8 = !!utf8;

  /* TODO: See what we can think of for the output side */

  if(tt->termkey) {
    int flags = termkey_get_flags(tt->termkey) & ~(TERMKEY_FLAG_UTF8|TERMKEY_FLAG_RAW);

    if(utf8)
      flags |= TERMKEY_FLAG_UTF8;
    else
      flags |= TERMKEY_FLAG_RAW;

    termkey_set_flags(tt->termkey, flags);
  }
}

void tickit_term_await_started_msec(TickitTerm *tt, long msec)
{
  if(msec > -1)
    tickit_term_await_started_tv(tt, &(struct timeval){
        .tv_sec  = msec / 1000,
        .tv_usec = (msec % 1000) * 1000,
    });
  else
    tickit_term_await_started_tv(tt, NULL);
}

void tickit_term_await_started_tv(TickitTerm *tt, const struct timeval *timeout)
{
  if(tt->state == STARTED)
    return;

  struct timeval until;
  gettimeofday(&until, NULL);

  // until += timeout
  if(until.tv_usec + timeout->tv_usec >= 1E6) {
    until.tv_sec  += timeout->tv_sec + 1;
    until.tv_usec += timeout->tv_usec - 1E6;
  }
  else {
    until.tv_sec  += timeout->tv_sec;
    until.tv_usec += timeout->tv_usec;
  }

  while(tt->state != STARTED) {
    if(!tt->driver->vtable->started ||
       (*tt->driver->vtable->started)(tt->driver))
      break;

    struct timeval timeout;
    gettimeofday(&timeout, NULL);

    // timeout = until - timeout
    if(until.tv_usec < timeout.tv_usec) {
      timeout.tv_sec  = until.tv_sec  - timeout.tv_sec - 1;
      timeout.tv_usec = until.tv_usec - timeout.tv_usec + 1E6;
    }
    else {
      timeout.tv_sec  = until.tv_sec  - timeout.tv_sec;
      timeout.tv_usec = until.tv_usec - timeout.tv_usec;
    }

    if(timeout.tv_sec < 0)
      break;

    tickit_term_input_wait_tv(tt, &timeout);
  }

  tt->state = STARTED;
}

static void got_key(TickitTerm *tt, TermKey *tk, TermKeyKey *key)
{
  if(key->type == TERMKEY_TYPE_MOUSE) {
    TermKeyMouseEvent ev;
    TickitMouseEventInfo info;
    termkey_interpret_mouse(tk, key, &ev, &info.button, &info.line, &info.col);
    /* TermKey is 1-based, Tickit is 0-based for position */
    info.line--; info.col--;
    switch(ev) {
    case TERMKEY_MOUSE_PRESS:   info.type = TICKIT_MOUSEEV_PRESS;   break;
    case TERMKEY_MOUSE_DRAG:    info.type = TICKIT_MOUSEEV_DRAG;    break;
    case TERMKEY_MOUSE_RELEASE: info.type = TICKIT_MOUSEEV_RELEASE; break;
    default:                    info.type = -1; break;
    }

    /* Translate PRESS of buttons >= 4 into wheel events */
    if(ev == TERMKEY_MOUSE_PRESS && info.button >= 4) {
      info.type = TICKIT_MOUSEEV_WHEEL;
      info.button -= (4 - TICKIT_MOUSEWHEEL_UP);
    }

    info.mod = key->modifiers;

    if(info.type == TICKIT_MOUSEEV_PRESS || info.type == TICKIT_MOUSEEV_DRAG) {
      tt->mouse_buttons_held |= (1 << info.button);
    }
    else if(info.type == TICKIT_MOUSEEV_RELEASE && info.button) {
      tt->mouse_buttons_held &= ~(1 << info.button);
    }
    else if(info.type == TICKIT_MOUSEEV_RELEASE) {
      /* X10 cannot report which button was released. Just report that they 
       * all were */
      for(info.button = 1; tt->mouse_buttons_held; info.button++)
        if(tt->mouse_buttons_held & (1 << info.button)) {
          run_events_whilefalse(tt, TICKIT_TERM_ON_MOUSE, &info);
          tt->mouse_buttons_held &= ~(1 << info.button);
        }
      return; // Buttons have been handled
    }

    run_events_whilefalse(tt, TICKIT_TERM_ON_MOUSE, &info);
  }
  else if(key->type == TERMKEY_TYPE_UNICODE && !key->modifiers) {
    /* Unmodified unicode */
    TickitKeyEventInfo info = {
      .type = TICKIT_KEYEV_TEXT,
      .str  = key->utf8,
      .mod  = key->modifiers,
    };

    run_events_whilefalse(tt, TICKIT_TERM_ON_KEY, &info);
  }
  else if(key->type == TERMKEY_TYPE_UNICODE ||
          key->type == TERMKEY_TYPE_FUNCTION ||
          key->type == TERMKEY_TYPE_KEYSYM) {
    char buffer[64]; // TODO: should be long enough
    termkey_strfkey(tk, buffer, sizeof buffer, key, TERMKEY_FORMAT_ALTISMETA);

    TickitKeyEventInfo info = {
      .type = TICKIT_KEYEV_KEY,
      .str  = buffer,
      .mod  = key->modifiers,
    };

    run_events_whilefalse(tt, TICKIT_TERM_ON_KEY, &info);
  }
  else if(key->type == TERMKEY_TYPE_MODEREPORT) {
    if(tt->driver->vtable->on_modereport) {
      int initial, mode, value;
      termkey_interpret_modereport(tk, key, &initial, &mode, &value);

      (tt->driver->vtable->on_modereport)(tt->driver, initial, mode, value);
    }
  }
  else if(key->type == TERMKEY_TYPE_DCS) {
    const char *dcs;
    if(termkey_interpret_string(tk, key, &dcs) != TERMKEY_RES_KEY)
      return;

    if(strneq(dcs, "1$r", 3)) { // Successful DECRQSS
      if(tt->driver->vtable->on_decrqss)
        (tt->driver->vtable->on_decrqss)(tt->driver, dcs + 3, strlen(dcs + 3));
    }
  }
}

void tickit_term_emit_key(TickitTerm *tt, TickitKeyEventInfo *info)
{
  run_events_whilefalse(tt, TICKIT_TERM_ON_KEY, info);
}

void tickit_term_emit_mouse(TickitTerm *tt, TickitMouseEventInfo *info)
{
  run_events_whilefalse(tt, TICKIT_TERM_ON_MOUSE, info);
}

static void get_keys(TickitTerm *tt, TermKey *tk)
{
  TermKeyResult res;
  TermKeyKey key;
  while((res = termkey_getkey(tk, &key)) == TERMKEY_RES_KEY) {
    got_key(tt, tk, &key);
  }

  if(res == TERMKEY_RES_AGAIN) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    /* tv += waittime in MSEC */
    int new_usec = tv.tv_usec + (termkey_get_waittime(tk) * MSEC);
    if(new_usec >= SECOND) {
      tv.tv_sec++;
      new_usec -= SECOND;
    }
    tv.tv_usec = new_usec;

    tt->input_timeout_at = tv;
  }
  else {
    tt->input_timeout_at.tv_sec = -1;
  }
}

void tickit_term_input_push_bytes(TickitTerm *tt, const char *bytes, size_t len)
{
  check_resize(tt);

  TermKey *tk = get_termkey(tt);
  termkey_push_bytes(tk, bytes, len);

  get_keys(tt, tk);
}

void tickit_term_input_readable(TickitTerm *tt)
{
  check_resize(tt);

  TermKey *tk = get_termkey(tt);
  termkey_advisereadable(tk);

  get_keys(tt, tk);
}

static int get_timeout(TickitTerm *tt)
{
  if(tt->input_timeout_at.tv_sec == -1)
    return -1;

  struct timeval tv;
  gettimeofday(&tv, NULL);

  /* tv = tt->input_timeout_at - tv */
  int new_usec = tt->input_timeout_at.tv_usec - tv.tv_usec;
  tv.tv_sec    = tt->input_timeout_at.tv_sec  - tv.tv_sec;
  if(new_usec < 0) {
    tv.tv_sec--;
    tv.tv_usec = new_usec + SECOND;
  }
  else {
    tv.tv_usec = new_usec;
  }

  if(tv.tv_sec > 0 || (tv.tv_sec == 0 && tv.tv_usec > 0))
    return tv.tv_sec * 1000 + (tv.tv_usec+MSEC-1)/MSEC;

  return 0;
}

static void timedout(TickitTerm *tt)
{
  TermKey *tk = get_termkey(tt);

  TermKeyKey key;
  if(termkey_getkey_force(tk, &key) == TERMKEY_RES_KEY) {
    got_key(tt, tk, &key);
  }

  tt->input_timeout_at.tv_sec = -1;
}

int tickit_term_input_check_timeout_msec(TickitTerm *tt)
{
  check_resize(tt);

  int msec = get_timeout(tt);

  if(msec != 0)
    return msec;

  timedout(tt);
  return -1;
}

void tickit_term_input_wait_msec(TickitTerm *tt, long msec)
{
  TermKey *tk = get_termkey(tt);

  int maxwait = get_timeout(tt);
  if(maxwait > -1) {
    if(msec == -1 || maxwait < msec)
      msec = maxwait;
  }

  struct timeval timeout;
  if(msec > -1) {
    timeout.tv_sec = msec / 1000;
    timeout.tv_usec = (msec % 1000) * 1000;
  }

  fd_set readfds;
  FD_ZERO(&readfds);

  int fd = termkey_get_fd(tk);
  if (fd < 0 || fd >= FD_SETSIZE)
    return;

  FD_SET(fd, &readfds);
  int ret = select(fd + 1, &readfds, NULL, NULL, msec > -1 ? &timeout : NULL);

  if(ret == 0)
    timedout(tt);
  else if(ret > 0)
    termkey_advisereadable(tk);

  check_resize(tt);

  get_keys(tt, tk);
}

void tickit_term_input_wait_tv(TickitTerm *tt, const struct timeval *timeout)
{
  if(timeout)
    tickit_term_input_wait_msec(tt, (long)(timeout->tv_sec) + (timeout->tv_usec / 1000));
  else
    tickit_term_input_wait_msec(tt, -1);
}

void tickit_term_flush(TickitTerm *tt)
{
  if(tt->outbuffer_cur == 0)
    return;

  if(tt->outfunc)
    (*tt->outfunc)(tt, tt->outbuffer, tt->outbuffer_cur, tt->outfunc_user);
  else if(tt->outfd != -1) {
    write(tt->outfd, tt->outbuffer, tt->outbuffer_cur);
  }

  tt->outbuffer_cur = 0;
}

static void write_str(TickitTerm *tt, const char *str, size_t len)
{
  if(len == 0)
    len = strlen(str);

  if(tt->outbuffer) {
    while(len > 0) {
      size_t space = tt->outbuffer_len - tt->outbuffer_cur;
      if(len < space)
        space = len;
      memcpy(tt->outbuffer + tt->outbuffer_cur, str, space);
      tt->outbuffer_cur += space;
      str += space;
      len -= space;
      if(tt->outbuffer_cur >= tt->outbuffer_len)
        tickit_term_flush(tt);
    }
  }
  else if(tt->outfunc) {
    (*tt->outfunc)(tt, str, len, tt->outfunc_user);
  }
  else if(tt->outfd != -1) {
    write(tt->outfd, str, len);
  }
}
/* Driver API */
void tickit_termdrv_write_str(TickitTermDriver *ttd, const char *str, size_t len)
{
  write_str(ttd->tt, str, len);
}

static void write_vstrf(TickitTerm *tt, const char *fmt, va_list args)
{
  /* It's likely the output will fit in, say, 64 bytes */
  char buffer[64];
  size_t len;
  {
    va_list args_for_size;
    va_copy(args_for_size, args);

    len = vsnprintf(buffer, sizeof buffer, fmt, args_for_size);

    va_end(args_for_size);
  }

  if(len < sizeof buffer) {
    write_str(tt, buffer, len);
    return;
  }

  char *morebuffer = get_tmpbuffer(tt, len + 1);
  vsnprintf(morebuffer, len + 1, fmt, args);

  write_str(tt, morebuffer, len);
}

/* Driver API */
void tickit_termdrv_write_strf(TickitTermDriver *ttd, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  write_vstrf(ttd->tt, fmt, args);
  va_end(args);
}

void tickit_term_print(TickitTerm *tt, const char *str)
{
  (*tt->driver->vtable->print)(tt->driver, str, strlen(str));
}

void tickit_term_printn(TickitTerm *tt, const char *str, size_t len)
{
  (*tt->driver->vtable->print)(tt->driver, str, len);
}

void tickit_term_printf(TickitTerm *tt, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  tickit_term_vprintf(tt, fmt, args);
  va_end(args);
}

void tickit_term_vprintf(TickitTerm *tt, const char *fmt, va_list args)
{
  va_list args2;
  va_copy(args2, args);

  size_t len = vsnprintf(NULL, 0, fmt, args);
  char *buf = get_tmpbuffer(tt, len + 1);
  vsnprintf(buf, len + 1, fmt, args2);
  (*tt->driver->vtable->print)(tt->driver, buf, len);

  va_end(args2);
}

bool tickit_term_goto(TickitTerm *tt, int line, int col)
{
  return (*tt->driver->vtable->goto_abs)(tt->driver, line, col);
}

void tickit_term_move(TickitTerm *tt, int downward, int rightward)
{
  (*tt->driver->vtable->move_rel)(tt->driver, downward, rightward);
}

bool tickit_term_scrollrect(TickitTerm *tt, TickitRect rect, int downward, int rightward)
{
  return (*tt->driver->vtable->scrollrect)(tt->driver, &rect, downward, rightward);
}

static int convert_colour(int index, int colours)
{
  if(colours >= 16)
    return xterm256[index].as16;
  else
    return xterm256[index].as8;
}

void tickit_term_chpen(TickitTerm *tt, const TickitPen *pen)
{
  TickitPen *delta = tickit_pen_new();

  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(pen, attr))
      continue;

    if(tickit_pen_has_attr(tt->pen, attr) && tickit_pen_equiv_attr(tt->pen, pen, attr))
      continue;

    int index;
    if((attr == TICKIT_PEN_FG || attr == TICKIT_PEN_BG) &&
       (index = tickit_pen_get_colour_attr(pen, attr)) >= tt->colors) {
      index = convert_colour(index, tt->colors);
      tickit_pen_set_colour_attr(tt->pen, attr, index);
      tickit_pen_set_colour_attr(delta, attr, index);
    }
    else {
      tickit_pen_copy_attr(tt->pen, pen, attr);
      tickit_pen_copy_attr(delta, pen, attr);
    }
  }

  (*tt->driver->vtable->chpen)(tt->driver, delta, tt->pen);

  tickit_pen_unref(delta);
}

void tickit_term_setpen(TickitTerm *tt, const TickitPen *pen)
{
  TickitPen *delta = tickit_pen_new();

  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(tickit_pen_has_attr(tt->pen, attr) && tickit_pen_equiv_attr(tt->pen, pen, attr))
      continue;

    int index;
    if((attr == TICKIT_PEN_FG || attr == TICKIT_PEN_BG) &&
       (index = tickit_pen_get_colour_attr(pen, attr)) >= tt->colors) {
      index = convert_colour(index, tt->colors);
      tickit_pen_set_colour_attr(tt->pen, attr, index);
      tickit_pen_set_colour_attr(delta, attr, index);
    }
    else {
      tickit_pen_copy_attr(tt->pen, pen, attr);
      tickit_pen_copy_attr(delta, pen, attr);
    }
  }

  (*tt->driver->vtable->chpen)(tt->driver, delta, tt->pen);

  tickit_pen_unref(delta);
}

/* Driver API */
TickitPen *tickit_termdrv_current_pen(TickitTermDriver *ttd)
{
  return ttd->tt->pen;
}

void tickit_term_clear(TickitTerm *tt)
{
  (*tt->driver->vtable->clear)(tt->driver);
}

void tickit_term_erasech(TickitTerm *tt, int count, TickitMaybeBool moveend)
{
  (*tt->driver->vtable->erasech)(tt->driver, count, moveend);
}

bool tickit_term_getctl_int(TickitTerm *tt, TickitTermCtl ctl, int *value)
{
  return (*tt->driver->vtable->getctl_int)(tt->driver, ctl, value);
}

bool tickit_term_setctl_int(TickitTerm *tt, TickitTermCtl ctl, int value)
{
  return (*tt->driver->vtable->setctl_int)(tt->driver, ctl, value);
}

bool tickit_term_setctl_str(TickitTerm *tt, TickitTermCtl ctl, const char *value)
{
  return (*tt->driver->vtable->setctl_str)(tt->driver, ctl, value);
}

void tickit_term_pause(TickitTerm *tt)
{
  if(tt->driver->vtable->pause)
    (*tt->driver->vtable->pause)(tt->driver);

  if(tt->termkey)
    termkey_stop(tt->termkey);
}

void tickit_term_resume(TickitTerm *tt)
{
  if(tt->termkey)
    termkey_start(tt->termkey);

  if(tt->driver->vtable->resume)
    (*tt->driver->vtable->resume)(tt->driver);
}

const char *tickit_termctl_name(TickitTermCtl ctl)
{
  if(ctl & TICKIT_TERMCTL_PRIVATEMASK) {
    for(int i = 0; driver_infos[i]; i++)
      if((ctl & TICKIT_TERMCTL_PRIVATEMASK) == driver_infos[i]->privatectl)
        return (*driver_infos[i]->ctlname)(ctl);
    return NULL;
  }

  switch(ctl) {
    case TICKIT_TERMCTL_ALTSCREEN:      return "altscreen";
    case TICKIT_TERMCTL_CURSORVIS:      return "cursorvis";
    case TICKIT_TERMCTL_MOUSE:          return "mouse";
    case TICKIT_TERMCTL_CURSORBLINK:    return "cursorblink";
    case TICKIT_TERMCTL_CURSORSHAPE:    return "cursorshape";
    case TICKIT_TERMCTL_ICON_TEXT:      return "icon_text";
    case TICKIT_TERMCTL_TITLE_TEXT:     return "title_text";
    case TICKIT_TERMCTL_ICONTITLE_TEXT: return "icontitle_text";
    case TICKIT_TERMCTL_KEYPAD_APP:     return "keypad_app";
    case TICKIT_TERMCTL_COLORS:         return "colors";

    case TICKIT_N_TERMCTLS: ;
  }
  return NULL;
}

TickitTermCtl tickit_termctl_lookup(const char *name)
{
  const char *s;

  for(TickitTermCtl ctl = 1; ctl < TICKIT_N_TERMCTLS; ctl++)
    if((s = tickit_termctl_name(ctl)) && streq(name, s))
      return ctl;

  const char *dotat = strchr(name, '.');
  if(dotat) {
    size_t prefixlen = dotat - name;
    for(int i = 0; driver_infos[i]; i++) {
      if(strncmp(name, driver_infos[i]->name, prefixlen) != 0 ||
          driver_infos[i]->name[prefixlen] != 0)
        continue;

      for(int ctl = driver_infos[i]->privatectl + 1; ; ctl++) {
        const char *ctlname = (*driver_infos[i]->ctlname)(ctl);
        if(!ctlname)
          break;

        if(strcmp(name, ctlname) == 0)
          return ctl;
      }

      return -1;
    }
  }

  return -1;
}

TickitType tickit_termctl_type(TickitTermCtl ctl)
{
  if(ctl & TICKIT_TERMCTL_PRIVATEMASK) {
    for(int i = 0; driver_infos[i]; i++)
      if((ctl & TICKIT_TERMCTL_PRIVATEMASK) == driver_infos[i]->privatectl)
        return (*driver_infos[i]->ctltype)(ctl);
    return TICKIT_TYPE_NONE;
  }

  switch(ctl) {
    case TICKIT_TERMCTL_ALTSCREEN:
    case TICKIT_TERMCTL_CURSORVIS:
    case TICKIT_TERMCTL_CURSORBLINK:
    case TICKIT_TERMCTL_KEYPAD_APP:
      return TICKIT_TYPE_BOOL;

    case TICKIT_TERMCTL_COLORS:
    case TICKIT_TERMCTL_CURSORSHAPE:
    case TICKIT_TERMCTL_MOUSE:
      return TICKIT_TYPE_INT;

    case TICKIT_TERMCTL_ICON_TEXT:
    case TICKIT_TERMCTL_ICONTITLE_TEXT:
    case TICKIT_TERMCTL_TITLE_TEXT:
      return TICKIT_TYPE_STR;

    case TICKIT_N_TERMCTLS:
      ;
  }
  return TICKIT_TYPE_NONE;
}

const char *tickit_term_ctlname(TickitTermCtl ctl) { return tickit_termctl_name(ctl); }
TickitTermCtl tickit_term_lookup_ctl(const char *name) { return tickit_termctl_lookup(name); }
TickitType tickit_term_ctltype(TickitTermCtl ctl) { return tickit_termctl_type(ctl); }
