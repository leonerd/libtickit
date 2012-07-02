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

  struct {
    unsigned int bce:1;
  } cap;

  struct {
    unsigned int altscreen:1;
    unsigned int cursorvis:1;
    unsigned int mouse:1;
  } mode;

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
  tt->driver = malloc(sizeof(TickitTermDriver));
  tt->driver->vtable = &xterm_vtable;
  tt->driver->tt = tt;
  /* /TODO */

  tt->mode.altscreen = 0;
  tt->mode.cursorvis = 1;
  tt->mode.mouse     = 0;

  tt->cap.bce = 1;
  tt->lines = 25;
  tt->cols  = 80;

#ifdef HAVE_UNIBILIUM
  {
    unibi_term *ut = unibi_from_term(termtype);
    if(ut) {
      tt->cap.bce = unibi_get_bool(ut, unibi_back_color_erase);

      tt->lines = unibi_get_num(ut, unibi_lines);
      tt->cols  = unibi_get_num(ut, unibi_columns);

      unibi_destroy(ut);
    }
  }
#else
  {
    int err;
    if(setupterm((char*)termtype, 1, &err) == OK) {
      tt->cap.bce = terminfo_bce();

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

  if(tt->termkey)
    termkey_destroy(tt->termkey);

  if(tt->outbuffer)
    free(tt->outbuffer);

  if(tt->tmpbuffer)
    free(tt->tmpbuffer);

  if(tt->driver)
    (*tt->driver->vtable->destroy)(tt->driver);

  free(tt);
}

void tickit_term_destroy(TickitTerm *tt)
{
  if(tt->mode.mouse)
    tickit_term_set_mode_mouse(tt, 0);
  if(!tt->mode.cursorvis)
    tickit_term_set_mode_cursorvis(tt, 1);
  if(tt->mode.altscreen)
    tickit_term_set_mode_altscreen(tt, 0);

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

static void write_str_rep(TickitTerm *tt, const char *str, size_t len, int repeat)
{
  if(len == 0)
    len = strlen(str);

  if(repeat < 1)
    return;

  if(tt->outfunc && !tt->outbuffer) {
    char *buffer = get_tmpbuffer(tt, len * repeat + 1);
    char *s = buffer;
    for(int i = 0; i < repeat; i++) {
      strncpy(s, str, len);
      s += len;
    }
    (*tt->outfunc)(tt, buffer, len * repeat, tt->outfunc_user);
  }
  else {
    for(int i = 0; i < repeat; i++)
      write_str(tt, str, len);
  }
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

static void write_strf(TickitTerm *tt, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  write_vstrf(tt, fmt, args);
  va_end(args);
}

void tickit_term_print(TickitTerm *tt, const char *str)
{
  write_str(tt, str, strlen(str));
}

void tickit_term_goto(TickitTerm *tt, int line, int col)
{
  if(line != -1 && col > 0)
    write_strf(tt, "\e[%d;%dH", line+1, col+1);
  else if(line != -1 && col == 0)
    write_strf(tt, "\e[%dH", line+1);
  else if(line != -1)
    write_strf(tt, "\e[%dd", line+1);
  else if(col > 0)
    write_strf(tt, "\e[%dG", col+1);
  else if(col != -1)
    write_str(tt, "\e[G", 3);
}

void tickit_term_move(TickitTerm *tt, int downward, int rightward)
{
  if(downward > 1)
    write_strf(tt, "\e[%dB", downward);
  else if(downward == 1)
    write_str(tt, "\e[B", 3);
  else if(downward == -1)
    write_str(tt, "\e[A", 3);
  else if(downward < -1)
    write_strf(tt, "\e[%dA", -downward);

  if(rightward > 1)
    write_strf(tt, "\e[%dC", rightward);
  else if(rightward == 1)
    write_str(tt, "\e[C", 3);
  else if(rightward == -1)
    write_str(tt, "\e[D", 3);
  else if(rightward < -1)
    write_strf(tt, "\e[%dD", -rightward);
}

static void insertch(TickitTerm *tt, int count)
{
  if(count == 1)
    write_str(tt, "\e[@", 3);
  else if(count > 1)
    write_strf(tt, "\e[%d@", count);
}

static void deletech(TickitTerm *tt, int count)
{
  if(count == 1)
    write_str(tt, "\e[P", 3);
  else if(count > 1)
    write_strf(tt, "\e[%dP", count);
}

int tickit_term_scrollrect(TickitTerm *tt, int top, int left, int lines, int cols, int downward, int rightward)
{
  if(!downward && !rightward)
    return 1;

  if(left == 0 && cols == tt->cols && rightward == 0) {
    write_strf(tt, "\e[%d;%dr", top + 1, top + lines);
    tickit_term_goto(tt, top, left);
    if(downward > 0) {
      if(downward > 1)
        write_strf(tt, "\e[%dM", downward); /* DL */
      else
        write_str(tt, "\e[M", 3);
    }
    else {
      if(downward < -1)
        write_strf(tt, "\e[%dL", -downward); /* IL */
      else
        write_str(tt, "\e[L", 3);
    }
    write_str(tt, "\e[r", 3);
    return 1;
  }

  if(left + cols == tt->cols && downward == 0) {
    for(int line = top; line < top + lines; line++) {
      tickit_term_goto(tt, line, left);
      if(rightward > 0)
        insertch(tt,  rightward);
      else
        deletech(tt, -rightward);
    }
  }

  return 0;
}

struct SgrOnOff { int on, off; } sgr_onoff[] = {
  { 30, 39 }, /* fg */
  { 40, 49 }, /* bg */
  {  1, 22 }, /* bold */
  {  4, 24 }, /* under */
  {  3, 23 }, /* italic */
  {  7, 27 }, /* reverse */
  {  9, 29 }, /* strike */
  { 10, 10 }, /* altfont */
};

static void do_pen(TickitTerm *tt, const TickitPen *delta)
{
  /* There can be at most 12 SGR parameters; 3 from each of 2 colours, and
   * 6 single attributes
   */
  int params[12];
  int pindex = 0;

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(delta, attr))
      continue;

    struct SgrOnOff *onoff = &sgr_onoff[attr];

    int val;

    switch(attr) {
    case TICKIT_PEN_FG:
    case TICKIT_PEN_BG:
      val = tickit_pen_get_colour_attr(delta, attr);
      if(val < 0)
        params[pindex++] = onoff->off;
      else if(val < 8)
        params[pindex++] = onoff->on + val;
      else if(val < 16)
        params[pindex++] = onoff->on+60 + val-8;
      else {
        params[pindex++] = (onoff->on+8) | 0x80000000;
        params[pindex++] = 5 | 0x80000000;
        params[pindex++] = val;
      }
      break;

    case TICKIT_PEN_ALTFONT:
      val = tickit_pen_get_int_attr(delta, attr);
      if(val < 0 || val >= 10)
        params[pindex++] = onoff->off;
      else
        params[pindex++] = onoff->on + val;
      break;

    case TICKIT_PEN_BOLD:
    case TICKIT_PEN_UNDER:
    case TICKIT_PEN_ITALIC:
    case TICKIT_PEN_REVERSE:
    case TICKIT_PEN_STRIKE:
      val = tickit_pen_get_bool_attr(delta, attr);
      params[pindex++] = val ? onoff->on : onoff->off;
      break;

    case TICKIT_N_PEN_ATTRS:
      break;
    }
  }

  if(pindex == 0)
    return;

  /* If we're going to clear all the attributes then empty SGR is neater */
  if(!tickit_pen_is_nondefault(tt->pen))
    pindex = 0;

  /* Render params[] into a CSI string */

  size_t len = 3; /* ESC [ ... m */
  for(int i = 0; i < pindex; i++)
    len += snprintf(NULL, 0, "%d", params[i]&0x7fffffff) + 1;
  if(pindex > 0)
    len--; /* Last one has no final separator */

  char *buffer = get_tmpbuffer(tt, len + 1);
  char *s = buffer;

  s += sprintf(s, "\e[");
  for(int i = 0; i < pindex-1; i++)
    /* TODO: Work out what terminals support :s */
    s += sprintf(s, "%d%c", params[i]&0x7fffffff, ';');
  if(pindex > 0)
    s += sprintf(s, "%d", params[pindex-1]&0x7fffffff);
  sprintf(s, "m");

  write_str(tt, buffer, len);
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

  do_pen(tt, delta);

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

  do_pen(tt, delta);

  tickit_pen_destroy(delta);
}

void tickit_term_clear(TickitTerm *tt)
{
  write_strf(tt, "\e[2J", 4);
}

void tickit_term_erasech(TickitTerm *tt, int count, int moveend)
{
  if(count < 1)
    return;

  /* Even if the terminal can do bce, only use ECH if we're not in
   * reverse-video mode. Most terminals don't do rv+ECH properly
   */
  if(tt->cap.bce && !tickit_pen_get_bool_attr(tt->pen, TICKIT_PEN_REVERSE)) {
    if(count == 1)
      write_str(tt, "\e[X", 3);
    else
      write_strf(tt, "\e[%dX", count);

    if(moveend == 1)
      tickit_term_move(tt, 0, count);
  }
  else {
    /* For small counts this is probably more efficient than write_str_rep()
     * TODO: benchmark it and find out
     */
    if(count <= 8)
      write_str(tt, "        ", count);
    else
      write_str_rep(tt, " ", 1, count);

    if(moveend == 0)
      tickit_term_move(tt, 0, -count);
  }
}

void tickit_term_set_mode_altscreen(TickitTerm *tt, int on)
{
  if(!tt->mode.altscreen == !on)
    return;

  write_str(tt, on ? "\e[?1049h" : "\e[?1049l", 0);
  tt->mode.altscreen = !!on;
}

void tickit_term_set_mode_cursorvis(TickitTerm *tt, int on)
{
  if(!tt->mode.cursorvis == !on)
    return;

  write_str(tt, on ? "\e[?25h" : "\e[?25l", 0);
  tt->mode.cursorvis = !!on;
}

void tickit_term_set_mode_mouse(TickitTerm *tt, int on)
{
  if(!tt->mode.mouse == !on)
    return;

  write_str(tt, on ? "\e[?1002h" : "\e[?1002l", 0);
  tt->mode.mouse = !!on;
}
