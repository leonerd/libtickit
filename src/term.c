#include "tickit.h"

#include "hooklists.h"
#include "termdriver.h"

/* We need C99 vsnprintf() semantics */
#define _ISOC99_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/time.h>

/* unit multipliers for working in microseconds */
#define MSEC      1000
#define SECOND 1000000

#ifdef HAVE_UNIBILIUM
# include "unibilium.h"
#else
# include <curses.h>
# include <term.h>

/* term.h has defined 'lines' as a macro. Eugh. We'd really rather prefer it
 * didn't pollute our namespace so we'll provide some functions here and then
 * #undef the name pollution
 */
static inline int terminfo_bce(void)     { return back_color_erase; }
static inline int terminfo_lines(void)   { return lines; }
static inline int terminfo_columns(void) { return columns; }

# undef back_color_erase
# undef lines
# undef columns
#endif

#include <termkey.h>

struct TickitTerm {
  int                   outfd;
  TickitTermOutputFunc *outfunc;
  void                 *outfunc_user;

  int                   infd;
  int                   termkey_flags;
  TermKey              *termkey;
  struct timeval        input_timeout_at; /* absolute time */

  char *outbuffer;
  size_t outbuffer_len; /* size of outbuffer */
  size_t outbuffer_cur; /* current fill level */

  char *tmpbuffer;
  size_t tmpbuffer_len;

  TickitTermDriver *driver;

  int lines;
  int cols;

  TickitPen *pen;

  struct TickitEventHook *hooks;
};

DEFINE_HOOKLIST_FUNCS(term,TickitTerm,TickitTermEventFn)

static TermKey *get_termkey(TickitTerm *tt)
{
  if(!tt->termkey) {
    tt->termkey = termkey_new(tt->infd, TERMKEY_FLAG_EINTR | tt->termkey_flags);
    tt->termkey_flags = termkey_get_flags(tt->termkey);
  }

  termkey_set_canonflags(tt->termkey,
      termkey_get_canonflags(tt->termkey) | TERMKEY_CANON_DELBS);

  return tt->termkey;
}

TickitTerm *tickit_term_new(void)
{
  const char *termtype = getenv("TERM");
  if(!termtype)
    termtype = "xterm";

  return tickit_term_new_for_termtype(termtype);
}

void tickit_term_free(TickitTerm *tt);

TickitTerm *tickit_term_new_for_termtype(const char *termtype)
{
  TickitTerm *tt = malloc(sizeof(TickitTerm));
  if(!tt)
    return NULL;

  tt->outfd   = -1;
  tt->outfunc = NULL;

  tt->infd    = -1;
  tt->termkey = NULL;
  tt->termkey_flags = 0;
  tt->input_timeout_at.tv_sec = -1;

  tt->outbuffer = NULL;
  tt->outbuffer_len = 0;
  tt->outbuffer_cur = 0;

  tt->tmpbuffer = NULL;
  tt->tmpbuffer_len = 0;

  /* TODO: driver integration */
  tt->driver = (*xterm_probe.new)(tt);

  tt->driver->mode.altscreen = 0;
  tt->driver->mode.cursorvis = 1;
  tt->driver->mode.mouse     = 0;
  /* /TODO */

  tt->driver->cap.bce = 1;
  tt->lines = 25;
  tt->cols  = 80;

#ifdef HAVE_UNIBILIUM
  {
    unibi_term *ut = unibi_from_term(termtype);
    if(ut) {
      tt->driver->cap.bce = unibi_get_bool(ut, unibi_back_color_erase);

      tt->lines = unibi_get_num(ut, unibi_lines);
      tt->cols  = unibi_get_num(ut, unibi_columns);

      unibi_destroy(ut);
    }
  }
#else
  {
    int err;
    if(setupterm((char*)termtype, 1, &err) == OK) {
      tt->driver->cap.bce = terminfo_bce();

      tt->lines = terminfo_lines();
      tt->cols  = terminfo_columns();
    }
  }
#endif

  /* Initially empty because we don't necessarily know the initial state
   * of the terminal
   */
  tt->pen = tickit_pen_new();

  tt->hooks = NULL;

  return tt;
}

void tickit_term_free(TickitTerm *tt)
{
  tickit_hooklist_unbind_and_destroy(tt->hooks, tt);
  tickit_pen_destroy(tt->pen);

  if(tt->driver)
    (*tt->driver->vtable->destroy)(tt->driver);

  if(tt->termkey)
    termkey_destroy(tt->termkey);

  if(tt->outbuffer)
    free(tt->outbuffer);

  if(tt->tmpbuffer)
    free(tt->tmpbuffer);

  free(tt);
}

void tickit_term_destroy(TickitTerm *tt)
{
  tickit_term_free(tt);
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

    TickitEvent args = { .lines = lines, .cols = cols };
    run_events(tt, TICKIT_EV_RESIZE, &args);
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

void tickit_term_set_output_fd(TickitTerm *tt, int fd)
{
  tt->outfd = fd;

  tickit_term_refresh_size(tt);
}

int tickit_term_get_output_fd(const TickitTerm *tt)
{
  return tt->outfd;
}

void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user)
{
  tt->outfunc      = fn;
  tt->outfunc_user = user;
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

int tickit_term_get_utf8(const TickitTerm *tt)
{
  return tt->termkey_flags & TERMKEY_FLAG_UTF8;
}

void tickit_term_set_utf8(TickitTerm *tt, int utf8)
{
  /* TODO: See what we can think of for the output side */
  if(utf8) {
    tt->termkey_flags |=  TERMKEY_FLAG_UTF8;
    tt->termkey_flags &= ~TERMKEY_FLAG_RAW;
  }
  else {
    tt->termkey_flags |=  TERMKEY_FLAG_RAW;
    tt->termkey_flags &= ~TERMKEY_FLAG_UTF8;
  }

  if(tt->termkey)
    termkey_set_flags(tt->termkey, tt->termkey_flags);
}

static void got_key(TickitTerm *tt, TermKey *tk, TermKeyKey *key)
{
  TickitEvent args;

  if(key->type == TERMKEY_TYPE_MOUSE) {
    TermKeyMouseEvent ev;
    termkey_interpret_mouse(tk, key, &ev, &args.button, &args.line, &args.col);
    /* TermKey is 1-based, Tickit is 0-based for position */
    args.line--; args.col--;
    switch(ev) {
    case TERMKEY_MOUSE_PRESS:   args.type = TICKIT_MOUSEEV_PRESS;   break;
    case TERMKEY_MOUSE_DRAG:    args.type = TICKIT_MOUSEEV_DRAG;    break;
    case TERMKEY_MOUSE_RELEASE: args.type = TICKIT_MOUSEEV_RELEASE; break;
    default:                    args.type = -1; break;
    }

    /* Translate PRESS of buttons >= 4 into wheel events */
    if(ev == TERMKEY_MOUSE_PRESS && args.button >= 4) {
      args.type = TICKIT_MOUSEEV_WHEEL;
      args.button -= (4 - TICKIT_MOUSEWHEEL_UP);
    }

    run_events(tt, TICKIT_EV_MOUSE, &args);
  }
  else if(key->type == TERMKEY_TYPE_UNICODE && !key->modifiers) {
    /* Unmodified unicode */
    args.type = TICKIT_KEYEV_TEXT;
    args.str  = key->utf8;

    run_events(tt, TICKIT_EV_KEY, &args);
  }
  else {
    char buffer[64]; // TODO: should be long enough
    termkey_strfkey(tk, buffer, sizeof buffer, key, TERMKEY_FORMAT_ALTISMETA);

    args.type = TICKIT_KEYEV_KEY;
    args.str  = buffer;

    run_events(tt, TICKIT_EV_KEY, &args);
  }
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
  TermKey *tk = get_termkey(tt);
  termkey_push_bytes(tk, bytes, len);

  get_keys(tt, tk);
}

void tickit_term_input_readable(TickitTerm *tt)
{
  TermKey *tk = get_termkey(tt);
  termkey_advisereadable(tk);

  get_keys(tt, tk);
}

int tickit_term_input_check_timeout(TickitTerm *tt)
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

  TermKey *tk = get_termkey(tt);

  TermKeyKey key;
  if(termkey_getkey_force(tk, &key) == TERMKEY_RES_KEY) {
    got_key(tt, tk, &key);
  }

  tt->input_timeout_at.tv_sec = -1;
  return -1;
}

void tickit_term_input_wait(TickitTerm *tt)
{
  TermKey *tk = get_termkey(tt);
  TermKeyKey key;

  termkey_waitkey(tk, &key);
  got_key(tt, tk, &key);

  /* Might as well get any more that are ready */
  while(termkey_getkey(tk, &key) == TERMKEY_RES_KEY) {
    got_key(tt, tk, &key);
  }
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
  size_t len = vsnprintf(buffer, sizeof buffer, fmt, args);

  if(len < 64) {
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
  (*tt->driver->vtable->print)(tt->driver, str);
}

void tickit_term_goto(TickitTerm *tt, int line, int col)
{
  (*tt->driver->vtable->goto_abs)(tt->driver, line, col);
}

void tickit_term_move(TickitTerm *tt, int downward, int rightward)
{
  (*tt->driver->vtable->move_rel)(tt->driver, downward, rightward);
}

int tickit_term_scrollrect(TickitTerm *tt, int top, int left, int lines, int cols, int downward, int rightward)
{
  return (*tt->driver->vtable->scrollrect)(tt->driver, top, left, lines, cols, downward, rightward);
}

void tickit_term_chpen(TickitTerm *tt, const TickitPen *pen)
{
  TickitPen *delta = tickit_pen_new();

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(pen, attr))
      continue;

    if(tickit_pen_has_attr(tt->pen, attr) && tickit_pen_equiv_attr(tt->pen, pen, attr))
      continue;

    tickit_pen_copy_attr(tt->pen, pen, attr);
    tickit_pen_copy_attr(delta, pen, attr);
  }

  (*tt->driver->vtable->chpen)(tt->driver, delta, tt->pen);

  tickit_pen_destroy(delta);
}

void tickit_term_setpen(TickitTerm *tt, const TickitPen *pen)
{
  TickitPen *delta = tickit_pen_new();

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(tickit_pen_has_attr(tt->pen, attr) && tickit_pen_equiv_attr(tt->pen, pen, attr))
      continue;

    tickit_pen_copy_attr(tt->pen, pen, attr);
    tickit_pen_copy_attr(delta, pen, attr);
  }

  (*tt->driver->vtable->chpen)(tt->driver, delta, tt->pen);

  tickit_pen_destroy(delta);
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

void tickit_term_erasech(TickitTerm *tt, int count, int moveend)
{
  (*tt->driver->vtable->erasech)(tt->driver, count, moveend);
}

void tickit_term_set_mode_altscreen(TickitTerm *tt, int on)
{
  (*tt->driver->vtable->set_mode)(tt->driver, TICKIT_TERMMODE_ALTSCREEN, on);
}

void tickit_term_set_mode_cursorvis(TickitTerm *tt, int on)
{
  (*tt->driver->vtable->set_mode)(tt->driver, TICKIT_TERMMODE_CURSORVIS, on);
}

void tickit_term_set_mode_mouse(TickitTerm *tt, int on)
{
  (*tt->driver->vtable->set_mode)(tt->driver, TICKIT_TERMMODE_MOUSE, on);
}
