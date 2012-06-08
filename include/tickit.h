#ifndef __TICKIT_H__
#define __TICKIT_H__

#include <stdlib.h>

/*
 * Tickit events
 */

/* bitmasks */
typedef enum {
  TICKIT_EV_RESIZE = 0x01, // lines, cols
  TICKIT_EV_KEY    = 0x02, // type(TickitKeyEventType), str
  TICKIT_EV_MOUSE  = 0x04, // type(TickitMouseEventType), button, line, col

  TICKIT_EV_UNBIND = 0x80000000, // event handler is being unbound
} TickitEventType;

typedef enum {
  TICKIT_KEYEV_KEY = 1,
  TICKIT_KEYEV_TEXT,
} TickitKeyEventType;

typedef enum {
  TICKIT_MOUSEEV_PRESS = 1,
  TICKIT_MOUSEEV_DRAG,
  TICKIT_MOUSEEV_RELEASE,
  TICKIT_MOUSEEV_WHEEL,
} TickitMouseEventType;

enum {
  TICKIT_MOUSEWHEEL_UP = 1,
  TICKIT_MOUSEWHEEL_DOWN,
};

typedef struct {
  int         lines, cols; // RESIZE
  int         type;        // KEY, MOUSE
  const char *str;         // KEY
  int         button;      // MOUSE
  int         line, col;   // MOUSE
} TickitEvent;

/*
 * TickitPen
 */

typedef struct TickitPen TickitPen;

typedef enum {
  TICKIT_PEN_FG,         /* colour */
  TICKIT_PEN_BG,         /* colour */
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
  TICKIT_PENTYPE_INT,
  TICKIT_PENTYPE_COLOUR,
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

int  tickit_pen_get_colour_attr(TickitPen *pen, TickitPenAttr attr);
void tickit_pen_set_colour_attr(TickitPen *pen, TickitPenAttr attr, int value);
int  tickit_pen_set_colour_attr_desc(TickitPen *pen, TickitPenAttr attr, const char *value);

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
  int    codepoints;
  int    graphemes;
  int    columns;
} TickitStringPos;

size_t tickit_string_count(const char *str, TickitStringPos *pos, const TickitStringPos *limit);
size_t tickit_string_countmore(const char *str, TickitStringPos *pos, const TickitStringPos *limit);

// Some convenient mutators for TickitStringPos structs

static inline void tickit_stringpos_zero(TickitStringPos *pos) {
  pos->bytes = pos->codepoints = pos->graphemes = pos->columns = 0;
}

static inline void tickit_stringpos_limit_bytes(TickitStringPos *pos, size_t bytes) {
  pos->codepoints = pos->graphemes = pos->columns = -1;
  pos->bytes = bytes;
}

static inline void tickit_stringpos_limit_codepoints(TickitStringPos *pos, int codepoints) {
  pos->bytes = pos->graphemes = pos->columns = -1;
  pos->codepoints = codepoints;
}

static inline void tickit_stringpos_limit_graphemes(TickitStringPos *pos, int graphemes) {
  pos->bytes = pos->codepoints = pos->columns = -1;
  pos->graphemes = graphemes;
}

static inline void tickit_stringpos_limit_columns(TickitStringPos *pos, int columns) {
  pos->bytes = pos->codepoints = pos->graphemes = -1;
  pos->columns = columns;
}

#endif
