#ifndef __TICKIT_H__
#define __TICKIT_H__

#include <stdlib.h>

/*
 * Tickit events
 */

/* bitmasks */
typedef enum {
  TICKIT_EV_RESIZE = 0x01, // lines, cols
  TICKIT_EV_KEY    = 0x02, // keytype, str
} TickitEventType;

typedef enum {
  TICKIT_KEYEV_KEY,
  TICKIT_KEYEV_TEXT,
} TickitKeyEventType;

typedef struct {
  int                 lines, cols; // RESIZE
  TickitKeyEventType  keytype;     // KEY
  const char         *str;         // KEY
} TickitEvent;

/*
 * TickitPen
 */

typedef struct TickitPen TickitPen;

typedef enum {
  TICKIT_PEN_FG,         /* number: TODO - colour? */
  TICKIT_PEN_BG,         /* number: TODO - colour? */
  TICKIT_PEN_BOLD,       /* bool */
  TICKIT_PEN_UNDER,      /* bool: TODO - number? */
  TICKIT_PEN_ITALIC,     /* bool */
  TICKIT_PEN_REVERSE,    /* bool */
  TICKIT_PEN_STRIKE,     /* bool */
  TICKIT_PEN_ALTFONT,    /* number */

  TICKIT_N_PEN_ATTRS
} TickitPenAttr;

typedef enum {
  TICKIT_PENTYPE_BOOL,
  TICKIT_PENTYPE_INT
} TickitPenAttrType;

TickitPen *tickit_pen_new(void);
void       tickit_pen_destroy(TickitPen *pen);

int tickit_pen_has_attr(TickitPen *pen, TickitPenAttr attr);
int tickit_pen_is_nonempty(TickitPen *pen);
int tickit_pen_is_nondefault(TickitPen *pen);

int  tickit_pen_get_bool_attr(TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_bool_attr(TickitPen *pen, TickitPenAttr attr, int val);

int  tickit_pen_get_int_attr(TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_int_attr(TickitPen *pen, TickitPenAttr attr, int val);

void tickit_pen_clear_attr(TickitPen *pen, TickitPenAttr attr);

int tickit_pen_equiv_attr(TickitPen *a, TickitPen *b, TickitPenAttr attr);

void tickit_pen_copy_attr(TickitPen *dst, TickitPen *src, TickitPenAttr attr);

TickitPenAttrType tickit_pen_attrtype(TickitPenAttr attr);

/*
 * TickitTerm
 */

typedef struct TickitTerm TickitTerm;
typedef void TickitTermOutputFunc(TickitTerm *tt, const char *bytes, size_t len, void *user);

TickitTerm *tickit_term_new(void);
TickitTerm *tickit_term_new_for_termtype(const char *termtype);
void tickit_term_free(TickitTerm *tt);
void tickit_term_destroy(TickitTerm *tt);

void tickit_term_set_output_fd(TickitTerm *tt, int fd);
int  tickit_term_get_output_fd(TickitTerm *tt);
void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user);

/* fd is allowed to be unset (-1); works abstractly */
void tickit_term_set_input_fd(TickitTerm *tt, int fd);
int  tickit_term_get_input_fd(TickitTerm *tt);

void tickit_term_input_push_bytes(TickitTerm *tt, const char *bytes, size_t len);
void tickit_term_input_readable(TickitTerm *tt);
int  tickit_term_input_check_timeout(TickitTerm *tt);
void tickit_term_input_wait(TickitTerm *tt);

void tickit_term_get_size(TickitTerm *tt, int *lines, int *cols);
void tickit_term_set_size(TickitTerm *tt, int lines, int cols);
void tickit_term_refresh_size(TickitTerm *tt);

typedef void TickitTermEventFn(TickitTerm *tt, TickitEventType ev, TickitEvent *args, void *data);

int  tickit_term_bind_event(TickitTerm *tt, TickitEventType ev, TickitTermEventFn *fn, void *data);
void tickit_term_unbind_event_id(TickitTerm *tt, int id);

void tickit_term_print(TickitTerm *tt, const char *str);
void tickit_term_goto(TickitTerm *tt, int line, int col);
void tickit_term_move(TickitTerm *tt, int downward, int rightward);
int  tickit_term_scrollrect(TickitTerm *tt, int top, int left, int lines, int cols, int downward, int rightward);

void tickit_term_chpen(TickitTerm *tt, TickitPen *pen);
void tickit_term_setpen(TickitTerm *tt, TickitPen *pen);

void tickit_term_clear(TickitTerm *tt);
void tickit_term_erasech(TickitTerm *tt, int count, int moveend);

void tickit_term_set_mode_altscreen(TickitTerm *tt, int on);
void tickit_term_set_mode_cursorvis(TickitTerm *tt, int on);
void tickit_term_set_mode_mouse(TickitTerm *tt, int on);

/*
 * String handling utilities
 */

typedef struct {
  size_t bytes;
  int    chars;
  int    graphemes;
  int    columns;
} TickitStringPos;

size_t tickit_string_count(const char *str, TickitStringPos *pos, const TickitStringPos *limit);
size_t tickit_string_countmore(const char *str, TickitStringPos *pos, const TickitStringPos *limit);

#endif
