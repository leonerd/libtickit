#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TICKIT_H__
#define __TICKIT_H__

/* We'd quite like the timer*() functions */
#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/time.h>

#ifdef __GNUC__
# define DEPRECATED __attribute__((deprecated))
#else
# define DEPRECATED
#endif

#define TICKIT_VERSION_MAJOR 0
#define TICKIT_VERSION_MINOR 4
#define TICKIT_VERSION_PATCH 1

int tickit_version_major(void);
int tickit_version_minor(void);
int tickit_version_patch(void);

/*
 * Top-level object / structure types
 */

typedef struct TickitPen TickitPen;
typedef struct TickitRectSet TickitRectSet;
typedef struct TickitRenderBuffer TickitRenderBuffer;
typedef struct TickitString TickitString;
typedef struct TickitTerm TickitTerm;
typedef struct TickitWindow TickitWindow;

typedef struct Tickit Tickit;

/* Opaque struct pointers required but not part of official API */
typedef struct TickitTermDriver TickitTermDriver;

/* Forward declaration to pointer defined by <tickit-evloop.h> */
typedef struct TickitEventHooks TickitEventHooks;

typedef struct {
  int top;
  int left;
  int lines;
  int cols;
} TickitRect;

/*
 * Enumerations
 */

typedef enum {
  TICKIT_BIND_FIRST   = 1<<0,
  TICKIT_BIND_UNBIND  = 1<<1,
  TICKIT_BIND_DESTROY = 1<<2,
  TICKIT_BIND_ONESHOT = 1<<3,
} TickitBindFlags;

typedef enum {
  TICKIT_CTL_USE_ALTSCREEN = 1,

  TICKIT_N_CTLS
} TickitCtl;

typedef enum {
  TICKIT_CURSORSHAPE_BLOCK = 1,
  TICKIT_CURSORSHAPE_UNDER,
  TICKIT_CURSORSHAPE_LEFT_BAR,
} TickitCursorShape;

typedef enum {
  TICKIT_IO_IN    = 1<<0,
  TICKIT_IO_OUT   = 1<<1,
  TICKIT_IO_HUP   = 1<<2,
  TICKIT_IO_ERR   = 1<<3,
  TICKIT_IO_INVAL = 1<<4,
} TickitIOCondition;

typedef struct {
  int fd;
  TickitIOCondition cond;
} TickitIOWatchInfo;

typedef enum {
  TICKIT_LINECAP_START = 0x01,
  TICKIT_LINECAP_END   = 0x02,
  TICKIT_LINECAP_BOTH  = 0x03,
} TickitLineCaps;

typedef enum {
  TICKIT_LINE_SINGLE = 1,
  TICKIT_LINE_DOUBLE = 2,
  TICKIT_LINE_THICK  = 3,
} TickitLineStyle;

typedef enum {
  TICKIT_NO    =  0,
  TICKIT_YES   =  1,
  TICKIT_MAYBE = -1,
} TickitMaybeBool;

typedef enum {
  TICKIT_PEN_FG = 1,     /* colour */
  TICKIT_PEN_BG,         /* colour */
  TICKIT_PEN_BOLD,       /* bool */
  TICKIT_PEN_UNDER,      /* number */
  TICKIT_PEN_ITALIC,     /* bool */
  TICKIT_PEN_REVERSE,    /* bool */
  TICKIT_PEN_STRIKE,     /* bool */
  TICKIT_PEN_ALTFONT,    /* number */
  TICKIT_PEN_BLINK,      /* bool */

  TICKIT_N_PEN_ATTRS
} TickitPenAttr;

/* additional attribute types recognised by tickit_pen_new_attrs */
enum {
  /* We're unlikely to ever have 256 attributes, so adding 0x100 should be safe */
  TICKIT_PEN_FG_DESC = 0x100 + TICKIT_PEN_FG,
  TICKIT_PEN_BG_DESC = 0x100 + TICKIT_PEN_BG,
};

typedef enum {
  TICKIT_PEN_UNDER_NONE,
  TICKIT_PEN_UNDER_SINGLE,
  TICKIT_PEN_UNDER_DOUBLE,
  TICKIT_PEN_UNDER_WAVY,

  TICKIT_N_PEN_UNDERS
} TickitPenUnderline;

typedef enum {
  TICKIT_RUN_DEFAULT = 0,
  TICKIT_RUN_ONCE    = 1<<0,
  TICKIT_RUN_NOHANG  = 1<<1,
  TICKIT_RUN_NOSETUP = 1<<2,
} TickitRunFlags;

typedef enum {
  /* This is part of the API so additions must go at the end only */
  TICKIT_TERMCTL_ALTSCREEN = 1,
  TICKIT_TERMCTL_CURSORVIS,
  TICKIT_TERMCTL_MOUSE,
  TICKIT_TERMCTL_CURSORBLINK,
  TICKIT_TERMCTL_CURSORSHAPE,
  TICKIT_TERMCTL_ICON_TEXT,
  TICKIT_TERMCTL_TITLE_TEXT,
  TICKIT_TERMCTL_ICONTITLE_TEXT,
  TICKIT_TERMCTL_KEYPAD_APP,
  TICKIT_TERMCTL_COLORS, // read-only

  TICKIT_N_TERMCTLS
} TickitTermCtl;

typedef enum {
  TICKIT_TERM_MOUSEMODE_OFF,
  TICKIT_TERM_MOUSEMODE_CLICK,
  TICKIT_TERM_MOUSEMODE_DRAG,
  TICKIT_TERM_MOUSEMODE_MOVE,
} TickitTermMouseMode;

typedef enum {
  TICKIT_TYPE_NONE,
  TICKIT_TYPE_BOOL,
  TICKIT_TYPE_INT,
  TICKIT_TYPE_STR,
  TICKIT_TYPE_COLOUR, // currently unused except by pen
} TickitType;

typedef enum {
  TICKIT_WINCTL_STEAL_INPUT = 1,
  TICKIT_WINCTL_FOCUS_CHILD_NOTIFY,
  TICKIT_WINCTL_CURSORVIS,
  TICKIT_WINCTL_CURSORBLINK,
  TICKIT_WINCTL_CURSORSHAPE,

  TICKIT_N_WINCTLS
} TickitWindowCtl;

typedef enum {
  TICKIT_WINDOW_HIDDEN      = 1<<0,
  TICKIT_WINDOW_LOWEST      = 1<<1,
  TICKIT_WINDOW_ROOT_PARENT = 1<<2,
  TICKIT_WINDOW_STEAL_INPUT = 1<<3,

  // Composite flag
  TICKIT_WINDOW_POPUP = TICKIT_WINDOW_ROOT_PARENT|TICKIT_WINDOW_STEAL_INPUT,
} TickitWindowFlags;

// TODO: this wants a name surely?
enum {
  TICKIT_MOD_SHIFT = 0x01,
  TICKIT_MOD_ALT   = 0x02,
  TICKIT_MOD_CTRL  = 0x04,
};

/* back-compat name */
typedef enum {
  TICKIT_PENTYPE_BOOL   = TICKIT_TYPE_BOOL,
  TICKIT_PENTYPE_INT    = TICKIT_TYPE_INT,
  TICKIT_PENTYPE_COLOUR = TICKIT_TYPE_COLOUR,
} TickitPenAttrType;

/*
 * Secondary structures
 */

typedef struct {
  size_t bytes;
  int    codepoints;
  int    graphemes;
  int    columns;
} TickitStringPos;

/*
 * Event types
 */

typedef enum {
  TICKIT_EV_FIRE = (1 << 0),
  TICKIT_EV_UNBIND = (1 << 1),
  TICKIT_EV_DESTROY = (1 << 2),
} TickitEventFlags;

typedef struct {
  int lines, cols;
} TickitResizeEventInfo;

typedef enum {
  TICKIT_KEYEV_KEY = 1,
  TICKIT_KEYEV_TEXT,
} TickitKeyEventType;

typedef struct {
  TickitKeyEventType type;
  int mod;
  const char *str;
} TickitKeyEventInfo;

typedef enum {
  TICKIT_MOUSEEV_PRESS = 1,
  TICKIT_MOUSEEV_DRAG,
  TICKIT_MOUSEEV_RELEASE,
  TICKIT_MOUSEEV_WHEEL,

  TICKIT_MOUSEEV_DRAG_START = 0x101,
  TICKIT_MOUSEEV_DRAG_OUTSIDE,
  TICKIT_MOUSEEV_DRAG_DROP,
  TICKIT_MOUSEEV_DRAG_STOP,
} TickitMouseEventType;

enum {
  TICKIT_MOUSEWHEEL_UP = 1,
  TICKIT_MOUSEWHEEL_DOWN,
};

typedef struct {
  TickitMouseEventType type;
  int button;
  int mod;
  int line, col;
} TickitMouseEventInfo;

typedef struct {
  TickitRect rect;
  TickitRect oldrect;
} TickitGeomchangeEventInfo;

typedef struct {
  TickitRect rect;
  TickitRenderBuffer *rb;
} TickitExposeEventInfo;

typedef enum {
  TICKIT_FOCUSEV_IN = 1,
  TICKIT_FOCUSEV_OUT,
} TickitFocusEventType;

typedef struct {
  TickitFocusEventType type;
  TickitWindow *win;
} TickitFocusEventInfo;

/*
 * Functions
 */

/* TickitPen */

TickitPen *tickit_pen_new(void);
TickitPen *tickit_pen_new_attrs(TickitPenAttr attr, ...);
TickitPen *tickit_pen_clone(const TickitPen *orig);

TickitPen *tickit_pen_ref(TickitPen *pen);
void       tickit_pen_unref(TickitPen *pen);

bool tickit_pen_has_attr(const TickitPen *pen, TickitPenAttr attr);
bool tickit_pen_is_nonempty(const TickitPen *pen);
bool tickit_pen_nondefault_attr(const TickitPen *pen, TickitPenAttr attr);
bool tickit_pen_is_nondefault(const TickitPen *pen);

bool tickit_pen_get_bool_attr(const TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_bool_attr(TickitPen *pen, TickitPenAttr attr, bool val);

int  tickit_pen_get_int_attr(const TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_int_attr(TickitPen *pen, TickitPenAttr attr, int val);

int  tickit_pen_get_colour_attr(const TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_colour_attr(TickitPen *pen, TickitPenAttr attr, int value);

typedef struct {
  uint8_t r, g, b;
} TickitPenRGB8;

bool tickit_pen_has_colour_attr_rgb8(const TickitPen *pen, TickitPenAttr attr);
TickitPenRGB8 tickit_pen_get_colour_attr_rgb8(const TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_colour_attr_rgb8(TickitPen *pen, TickitPenAttr attr, TickitPenRGB8 value);

bool tickit_pen_set_colour_attr_desc(TickitPen *pen, TickitPenAttr attr, const char *value);

void tickit_pen_clear_attr(TickitPen *pen, TickitPenAttr attr);
void tickit_pen_clear(TickitPen *pen);

bool tickit_pen_equiv_attr(const TickitPen *a, const TickitPen *b, TickitPenAttr attr);
bool tickit_pen_equiv(const TickitPen *a, const TickitPen *b);

void tickit_pen_copy_attr(TickitPen *dst, const TickitPen *src, TickitPenAttr attr);
void tickit_pen_copy(TickitPen *dst, const TickitPen *src, bool overwrite);

typedef int TickitPenEventFn(TickitPen *tt, TickitEventFlags flags, void *info, void *user);

typedef enum {
  TICKIT_PEN_ON_DESTROY,
  TICKIT_PEN_ON_CHANGE,
} TickitPenEvent;

int  tickit_pen_bind_event(TickitPen *tt, TickitPenEvent ev, TickitBindFlags flags,
    TickitPenEventFn *fn, void *user);
void tickit_pen_unbind_event_id(TickitPen *tt, int id);

TickitPenAttrType tickit_pen_attrtype(TickitPenAttr attr);
const char *tickit_pen_attrname(TickitPenAttr attr);
TickitPenAttr tickit_pen_lookup_attr(const char *name);

/* TickitRect */

void tickit_rect_init_sized(TickitRect *rect, int top, int left, int lines, int cols);
void tickit_rect_init_bounded(TickitRect *rect, int top, int left, int bottom, int right);

static inline int tickit_rect_bottom(const TickitRect *rect)
{ return rect->top + rect->lines; }

static inline int tickit_rect_right (const TickitRect *rect)
{ return rect->left + rect->cols; }

void tickit_rect_translate(TickitRect *rect, int downward, int rightward);

bool tickit_rect_intersect(TickitRect *dst, const TickitRect *a, const TickitRect *b);

bool tickit_rect_intersects(const TickitRect *a, const TickitRect *b);
bool tickit_rect_contains(const TickitRect *large, const TickitRect *small);

int tickit_rect_add(TickitRect ret[3], const TickitRect *a, const TickitRect *b);
int tickit_rect_subtract(TickitRect ret[4], const TickitRect *orig, const TickitRect *hole);

/* TickitRectSet */

TickitRectSet *tickit_rectset_new(void);
void tickit_rectset_destroy(TickitRectSet *trs);

void tickit_rectset_clear(TickitRectSet *trs);

size_t tickit_rectset_rects(const TickitRectSet *trs);
size_t tickit_rectset_get_rect(const TickitRectSet *trs, size_t i, TickitRect *rects);
size_t tickit_rectset_get_rects(const TickitRectSet *trs, TickitRect rects[], size_t n);

void tickit_rectset_add(TickitRectSet *trs, const TickitRect *rect);
void tickit_rectset_subtract(TickitRectSet *trs, const TickitRect *rect);

void tickit_rectset_translate(TickitRectSet *trs, int downward, int rightward);

bool tickit_rectset_intersects(const TickitRectSet *trs, const TickitRect *rect);
bool tickit_rectset_contains(const TickitRectSet *trs, const TickitRect *rect);

/* TickitString */

TickitString *tickit_string_new(const char *str, size_t len);
TickitString *tickit_string_ref(TickitString *s);
void tickit_string_unref(TickitString *s);
const char *tickit_string_get(const TickitString *s);
size_t tickit_string_len(const TickitString *s);

/* TickitTerm */

TickitTerm *tickit_term_new(void);
TickitTerm *tickit_term_new_for_termtype(const char *termtype);
void tickit_term_destroy(TickitTerm *tt);

typedef void TickitTermOutputFunc(TickitTerm *tt, const char *bytes, size_t len, void *user);

struct TickitTermBuilder {
  const char *termtype;

  enum {
    TICKIT_NO_OPEN,
    TICKIT_OPEN_FDS,    /* use input_fd, output_fd */
    TICKIT_OPEN_STDIO,  /* input=0, output=1 */
    TICKIT_OPEN_STDTTY, /* input = output = first of 0/1/2 for which isatty() is true */
    /* TODO: Consider
     *   TICKIT_OPEN_DEVTTY to open /dev/tty
     */
  } open;
  int input_fd, output_fd; /* only valid if open==TICKIT_OPEN_FDS */

  TickitTermOutputFunc *output_func;
  void                 *output_func_user;

  size_t output_buffersize;

  /* Fields below here are undocumented and for vaguely internal or
   * special-case purposes
   */
  TickitTermDriver *driver;

  const struct TickitTerminfoHook {
    const char *(*getstr)(const char *name, const char *value, void *data);
    void         *data;
  } *ti_hook;
};
TickitTerm *tickit_term_build(const struct TickitTermBuilder *builder);

void tickit_term_teardown(TickitTerm *tt);

TickitTerm *tickit_term_ref(TickitTerm *tt);
void        tickit_term_unref(TickitTerm *tt);

TickitTerm *tickit_term_open_stdio(void);

const char *tickit_term_get_termtype(TickitTerm *tt);

void tickit_term_set_output_fd(TickitTerm *tt, int fd);
int  tickit_term_get_output_fd(const TickitTerm *tt);
void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user);
void tickit_term_set_output_buffer(TickitTerm *tt, size_t len);

void tickit_term_await_started_msec(TickitTerm *tt, long msec);
void tickit_term_await_started_tv(TickitTerm *tt, const struct timeval *timeout);
void tickit_term_flush(TickitTerm *tt);

void tickit_term_pause(TickitTerm *tt);
void tickit_term_resume(TickitTerm *tt);

/* fd is allowed to be unset (-1); works abstractly */
void tickit_term_set_input_fd(TickitTerm *tt, int fd);
int  tickit_term_get_input_fd(const TickitTerm *tt);

TickitMaybeBool tickit_term_get_utf8(const TickitTerm *tt);
void tickit_term_set_utf8(TickitTerm *tt, bool utf8);

void tickit_term_input_push_bytes(TickitTerm *tt, const char *bytes, size_t len);
void tickit_term_input_readable(TickitTerm *tt);
int  tickit_term_input_check_timeout_msec(TickitTerm *tt);
void tickit_term_input_wait_msec(TickitTerm *tt, long msec);
void tickit_term_input_wait_tv(TickitTerm *tt, const struct timeval *timeout);

void tickit_term_get_size(const TickitTerm *tt, int *lines, int *cols);
void tickit_term_set_size(TickitTerm *tt, int lines, int cols);
void tickit_term_refresh_size(TickitTerm *tt);

void tickit_term_observe_sigwinch(TickitTerm *tt, bool observe);

typedef int TickitTermEventFn(TickitTerm *tt, TickitEventFlags flags, void *info, void *user);

typedef enum {
  TICKIT_TERM_ON_DESTROY,
  TICKIT_TERM_ON_RESIZE,
  TICKIT_TERM_ON_KEY,
  TICKIT_TERM_ON_MOUSE,
} TickitTermEvent;

int  tickit_term_bind_event(TickitTerm *tt, TickitTermEvent ev, TickitBindFlags flags,
    TickitTermEventFn *fn, void *user);
void tickit_term_unbind_event_id(TickitTerm *tt, int id);

void tickit_term_print(TickitTerm *tt, const char *str);
void tickit_term_printn(TickitTerm *tt, const char *str, size_t len);
void tickit_term_printf(TickitTerm *tt, const char *fmt, ...);
void tickit_term_vprintf(TickitTerm *tt, const char *fmt, va_list args);
bool tickit_term_goto(TickitTerm *tt, int line, int col);
void tickit_term_move(TickitTerm *tt, int downward, int rightward);
bool tickit_term_scrollrect(TickitTerm *tt, TickitRect rect, int downward, int rightward);

void tickit_term_chpen(TickitTerm *tt, const TickitPen *pen);
void tickit_term_setpen(TickitTerm *tt, const TickitPen *pen);

void tickit_term_clear(TickitTerm *tt);
void tickit_term_erasech(TickitTerm *tt, int count, TickitMaybeBool moveend);

bool tickit_term_getctl_int(TickitTerm *tt, TickitTermCtl ctl, int *value);
bool tickit_term_setctl_int(TickitTerm *tt, TickitTermCtl ctl, int value);
bool tickit_term_setctl_str(TickitTerm *tt, TickitTermCtl ctl, const char *value);

void tickit_term_emit_key(TickitTerm *tt, TickitKeyEventInfo *info);
void tickit_term_emit_mouse(TickitTerm *tt, TickitMouseEventInfo *info);

const char *tickit_term_ctlname(TickitTermCtl ctl);
TickitTermCtl tickit_term_lookup_ctl(const char *name);

TickitType tickit_term_ctltype(TickitTermCtl ctl);

/* String handling utilities */

int tickit_utf8_seqlen(long codepoint);

/* Does NOT NUL-terminate the buffer */
size_t tickit_utf8_put(char *str, size_t len, long codepoint);

size_t tickit_utf8_count(const char *str, TickitStringPos *pos, const TickitStringPos *limit);
size_t tickit_utf8_countmore(const char *str, TickitStringPos *pos, const TickitStringPos *limit);
size_t tickit_utf8_ncount(const char *str, size_t len, TickitStringPos *pos, const TickitStringPos *limit);
size_t tickit_utf8_ncountmore(const char *str, size_t len, TickitStringPos *pos, const TickitStringPos *limit);

// Some convenient mutators for TickitStringPos structs

static inline void tickit_stringpos_zero(TickitStringPos *pos) {
  pos->bytes = pos->codepoints = pos->graphemes = pos->columns = 0;
}

#define INIT_TICKIT_STRINGPOS_LIMIT_NONE { .bytes = -1, .codepoints = -1, .graphemes = -1, .columns = -1 }
static inline void tickit_stringpos_limit_none(TickitStringPos *pos)
{
  pos->bytes = pos->codepoints = pos->graphemes = pos->columns = -1;
}

#define INIT_TICKIT_STRINGPOS_LIMIT_BYTES(v) { .bytes = (v), .codepoints = -1, .graphemes = -1, .columns = -1 }
static inline void tickit_stringpos_limit_bytes(TickitStringPos *pos, size_t bytes) {
  pos->codepoints = pos->graphemes = pos->columns = -1;
  pos->bytes = bytes;
}

#define INIT_TICKIT_STRINGPOS_LIMIT_CODEPOINTS(v) { .bytes = -1, .codepoints = (v), .graphemes = -1, .columns = -1 }
static inline void tickit_stringpos_limit_codepoints(TickitStringPos *pos, int codepoints) {
  pos->bytes = pos->graphemes = pos->columns = -1;
  pos->codepoints = codepoints;
}

#define INIT_TICKIT_STRINGPOS_LIMIT_GRAPHEMES(v) { .bytes = -1, .codepoints = -1, .graphemes = (v), .columns = -1 }
static inline void tickit_stringpos_limit_graphemes(TickitStringPos *pos, int graphemes) {
  pos->bytes = pos->codepoints = pos->columns = -1;
  pos->graphemes = graphemes;
}

#define INIT_TICKIT_STRINGPOS_LIMIT_COLUMNS(v) { .bytes = -1, .codepoints = -1, .graphemes = -1, .columns = (v) }
static inline void tickit_stringpos_limit_columns(TickitStringPos *pos, int columns) {
  pos->bytes = pos->codepoints = pos->graphemes = -1;
  pos->columns = columns;
}

int    tickit_utf8_mbswidth(const char *str);
int    tickit_utf8_byte2col(const char *str, size_t byte);
size_t tickit_utf8_col2byte(const char *str, int col);

/* TickitRenderBuffer */

TickitRenderBuffer *tickit_renderbuffer_new(int lines, int cols);
void tickit_renderbuffer_destroy(TickitRenderBuffer *rb);

TickitRenderBuffer *tickit_renderbuffer_ref(TickitRenderBuffer *rb);
void                tickit_renderbuffer_unref(TickitRenderBuffer *rb);

void tickit_renderbuffer_get_size(const TickitRenderBuffer *rb, int *lines, int *cols);

void tickit_renderbuffer_translate(TickitRenderBuffer *rb, int downward, int rightward);
void tickit_renderbuffer_clip(TickitRenderBuffer *rb, TickitRect *rect);
void tickit_renderbuffer_mask(TickitRenderBuffer *rb, TickitRect *mask);

bool tickit_renderbuffer_has_cursorpos(const TickitRenderBuffer *rb);
void tickit_renderbuffer_get_cursorpos(const TickitRenderBuffer *rb, int *line, int *col);
void tickit_renderbuffer_goto(TickitRenderBuffer *rb, int line, int col);
void tickit_renderbuffer_ungoto(TickitRenderBuffer *rb);

void tickit_renderbuffer_setpen(TickitRenderBuffer *rb, const TickitPen *pen);

void tickit_renderbuffer_reset(TickitRenderBuffer *rb);

void tickit_renderbuffer_save(TickitRenderBuffer *rb);
void tickit_renderbuffer_savepen(TickitRenderBuffer *rb);
void tickit_renderbuffer_restore(TickitRenderBuffer *rb);

void tickit_renderbuffer_skip_at(TickitRenderBuffer *rb, int line, int col, int cols);
void tickit_renderbuffer_skip(TickitRenderBuffer *rb, int cols);
void tickit_renderbuffer_skip_to(TickitRenderBuffer *rb, int col);
void tickit_renderbuffer_skiprect(TickitRenderBuffer *rb, TickitRect *rect);
int tickit_renderbuffer_text_at(TickitRenderBuffer *rb, int line, int col, const char *text);
int tickit_renderbuffer_textn_at(TickitRenderBuffer *rb, int line, int col, const char *text, size_t len);
int tickit_renderbuffer_text(TickitRenderBuffer *rb, const char *text);
int tickit_renderbuffer_textn(TickitRenderBuffer *rb, const char *text, size_t len);
int tickit_renderbuffer_textf_at(TickitRenderBuffer *rb, int line, int col, const char *fmt, ...);
int tickit_renderbuffer_vtextf_at(TickitRenderBuffer *rb, int line, int col, const char *fmt, va_list args);
int tickit_renderbuffer_textf(TickitRenderBuffer *rb, const char *fmt, ...);
int tickit_renderbuffer_vtextf(TickitRenderBuffer *rb, const char *fmt, va_list args);
void tickit_renderbuffer_erase_at(TickitRenderBuffer *rb, int line, int col, int cols);
void tickit_renderbuffer_erase(TickitRenderBuffer *rb, int cols);
void tickit_renderbuffer_erase_to(TickitRenderBuffer *rb, int col);
void tickit_renderbuffer_eraserect(TickitRenderBuffer *rb, TickitRect *rect);
void tickit_renderbuffer_clear(TickitRenderBuffer *rb);
void tickit_renderbuffer_char_at(TickitRenderBuffer *rb, int line, int col, long codepoint);
void tickit_renderbuffer_char(TickitRenderBuffer *rb, long codepoint);

void tickit_renderbuffer_hline_at(TickitRenderBuffer *rb, int line, int startcol, int endcol,
    TickitLineStyle style, TickitLineCaps caps);
void tickit_renderbuffer_vline_at(TickitRenderBuffer *rb, int startline, int endline, int col,
    TickitLineStyle style, TickitLineCaps caps);

void tickit_renderbuffer_copyrect(TickitRenderBuffer *rb, const TickitRect *dest, const TickitRect *src);
void tickit_renderbuffer_moverect(TickitRenderBuffer *rb, const TickitRect *dest, const TickitRect *src);

void tickit_renderbuffer_flush_to_term(TickitRenderBuffer *rb, TickitTerm *tt);

void tickit_renderbuffer_blit(TickitRenderBuffer *dst, TickitRenderBuffer *src);

// This API is still somewhat experimental

typedef struct {
  char north;
  char south;
  char east;
  char west;
} TickitRenderBufferLineMask;

int tickit_renderbuffer_get_cell_active(TickitRenderBuffer *rb, int line, int col);
size_t tickit_renderbuffer_get_cell_text(TickitRenderBuffer *rb, int line, int col, char *buffer, size_t len);
TickitRenderBufferLineMask tickit_renderbuffer_get_cell_linemask(TickitRenderBuffer *rb, int line, int col);

// returns a direct pointer - do not free or modify
TickitPen *tickit_renderbuffer_get_cell_pen(TickitRenderBuffer *rb, int line, int col);

struct TickitRenderBufferSpanInfo {
  bool is_active;
  int n_columns;
  char *text;
  size_t len;
  TickitPen *pen;
};

// returns the text length or -1 on error
size_t tickit_renderbuffer_get_span(TickitRenderBuffer *rb, int line, int startcol, struct TickitRenderBufferSpanInfo *info, char *buffer, size_t len);

/* Window */

TickitWindow *tickit_window_new_root(TickitTerm *term);
TickitWindow *tickit_window_new(TickitWindow *parent, TickitRect rect, TickitWindowFlags flags);

TickitWindow *tickit_window_parent(const TickitWindow *win);
TickitWindow *tickit_window_root(const TickitWindow *win);

size_t tickit_window_children(const TickitWindow *win);
size_t tickit_window_get_children(const TickitWindow *win, TickitWindow *children[], size_t n);

TickitTerm *tickit_window_get_term(const TickitWindow *win);

void tickit_window_close(TickitWindow *win);

void tickit_window_destroy(TickitWindow *win);

TickitWindow *tickit_window_ref(TickitWindow *win);
void          tickit_window_unref(TickitWindow *win);

typedef int TickitWindowEventFn(TickitWindow *win, TickitEventFlags flags, void *info, void *user);

typedef enum {
  TICKIT_WINDOW_ON_DESTROY,
  TICKIT_WINDOW_ON_GEOMCHANGE,
  TICKIT_WINDOW_ON_EXPOSE,
  TICKIT_WINDOW_ON_FOCUS,
  TICKIT_WINDOW_ON_KEY,
  TICKIT_WINDOW_ON_MOUSE,
} TickitWindowEvent;

int  tickit_window_bind_event(TickitWindow *win, TickitWindowEvent ev, TickitBindFlags flags,
    TickitWindowEventFn *fn, void *user);
void tickit_window_unbind_event_id(TickitWindow *win, int id);

void tickit_window_raise(TickitWindow *win);
void tickit_window_raise_to_front(TickitWindow *win);
void tickit_window_lower(TickitWindow *win);
void tickit_window_lower_to_back(TickitWindow *win);

void tickit_window_show(TickitWindow *win);
void tickit_window_hide(TickitWindow *win);
bool tickit_window_is_visible(TickitWindow *win);

TickitRect tickit_window_get_geometry(const TickitWindow *win);
TickitRect tickit_window_get_abs_geometry(const TickitWindow *win);

#define tickit_window_top(win)   (tickit_window_get_geometry(win)).top
#define tickit_window_left(win)  (tickit_window_get_geometry(win)).left
#define tickit_window_lines(win) (tickit_window_get_geometry(win)).lines
#define tickit_window_cols(win)  (tickit_window_get_geometry(win)).cols

int tickit_window_bottom(const TickitWindow *win);
int tickit_window_right(const TickitWindow *win);

void tickit_window_resize(TickitWindow *win, int lines, int cols);
void tickit_window_reposition(TickitWindow *win, int top, int left);
void tickit_window_set_geometry(TickitWindow *win, TickitRect rect);

TickitPen *tickit_window_get_pen(const TickitWindow *win);
void tickit_window_set_pen(TickitWindow *win, TickitPen *pen);
void tickit_window_expose(TickitWindow *win, const TickitRect *exposed);
void tickit_window_flush(TickitWindow *win);

bool tickit_window_scrollrect(TickitWindow *win, const TickitRect *rect, int downward, int rightward, TickitPen *pen);
bool tickit_window_scroll(TickitWindow *win, int downward, int rightward);
bool tickit_window_scroll_with_children(TickitWindow *win, int downward, int rightward);

void tickit_window_set_cursor_position(TickitWindow *win, int line, int col);
void tickit_window_set_cursor_visible(TickitWindow *win, bool visible);
void tickit_window_set_cursor_shape(TickitWindow *win, TickitCursorShape shape);

void tickit_window_take_focus(TickitWindow *win);
bool tickit_window_is_focused(const TickitWindow *win);
void tickit_window_set_focus_child_notify(TickitWindow *win, bool notify);

bool tickit_window_getctl_int(TickitWindow *win, TickitWindowCtl ctl, int *value);
bool tickit_window_setctl_int(TickitWindow *win, TickitWindowCtl ctl, int value);

bool tickit_window_is_steal_input(const TickitWindow *win);
void tickit_window_set_steal_input(TickitWindow *win, bool steal);

const char *tickit_window_ctlname(TickitWindowCtl ctl);
TickitWindowCtl tickit_window_lookup_ctl(const char *name);

TickitType tickit_window_ctltype(TickitWindowCtl ctl);

/* Main object */

typedef int TickitCallbackFn(Tickit *t, TickitEventFlags flags, void *info, void *user);

Tickit *tickit_new_for_term(TickitTerm *tt);
Tickit *tickit_new_stdio(void);
Tickit *tickit_new_stdtty(void);

Tickit *tickit_ref(Tickit *t);
void    tickit_unref(Tickit *t);

struct TickitBuilder {
  TickitTerm *tt;
  struct TickitTermBuilder term_builder; /* used if tt == NULL */

  /* Fields below here are undocumented and for vaguely internal or
   * special-case purposes
   */
  const TickitEventHooks *evhooks;
  void *evinitdata;
};
Tickit *tickit_build(const struct TickitBuilder *builder);

TickitTerm *tickit_get_term(Tickit *t);

TickitWindow *tickit_get_rootwin(Tickit *t);

void tickit_run(Tickit *t);
void tickit_stop(Tickit *t);

void tickit_tick(Tickit *t, TickitRunFlags flags);

bool tickit_getctl_int(Tickit *tt, TickitCtl ctl, int *value);
bool tickit_setctl_int(Tickit *tt, TickitCtl ctl, int value);

const char *tickit_ctlname(TickitCtl ctl);
TickitCtl tickit_lookup_ctl(const char *name);

TickitType tickit_ctltype(TickitCtl ctl);

void *tickit_watch_io(Tickit *t, int fd, TickitIOCondition cond, TickitBindFlags flags, TickitCallbackFn *fn, void *user);

/* Discouraged synonyn for tickit_watch_io cond=TICKIT_IO_IN|TICKIT_IO_HUP */
void *tickit_watch_io_read(Tickit *t, int fd, TickitBindFlags flags, TickitCallbackFn *fn, void *user);

void *tickit_watch_timer_after_msec(Tickit *t, int msec, TickitBindFlags flags, TickitCallbackFn *fn, void *user);
void *tickit_watch_timer_after_tv(Tickit *t, const struct timeval *after, TickitBindFlags flags, TickitCallbackFn *fn, void *user);

void *tickit_watch_timer_at_epoch(Tickit *t, time_t at, TickitBindFlags flags, TickitCallbackFn *fn, void *user);
void *tickit_watch_timer_at_tv(Tickit *t, const struct timeval *at, TickitBindFlags flags, TickitCallbackFn *fn, void *user);

void *tickit_watch_later(Tickit *t, TickitBindFlags flags, TickitCallbackFn *fn, void *user);

void tickit_watch_cancel(Tickit *t, void *watch);

/* Debug support */

void tickit_debug_init(void);

extern bool tickit_debug_enabled;

void tickit_debug_logf(const char *flag, const char *fmt, ...);
void tickit_debug_vlogf(const char *flag, const char *fmt, va_list args);

void tickit_debug_set_func(void (*func)(const char *str, void *data), void *data);
void tickit_debug_set_fh(FILE *fh);
bool tickit_debug_open(const char *path);

#endif

#ifdef __cplusplus
}
#endif
