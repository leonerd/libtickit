#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TICKIT_WINDOW_H__
#define __TICKIT_WINDOW_H__

#include "tickit.h"

typedef struct TickitWindow TickitWindow;

/* Root window */

TickitWindow *tickit_window_new_root(TickitTerm *term);

/* Windows */

TickitWindow *tickit_window_new_subwindow(TickitWindow *parent, int top, int left, int lines, int cols);
TickitWindow *tickit_window_new_hidden_subwindow(TickitWindow *parent, int top, int left, int lines, int cols);
TickitWindow *tickit_window_new_float(TickitWindow *parent, int top, int left, int lines, int cols);
TickitWindow *tickit_window_new_popup(TickitWindow *parent, int top, int left, int lines, int cols);

TickitWindow *tickit_window_parent(const TickitWindow *win);
TickitWindow *tickit_window_root(const TickitWindow *win);

void tickit_window_destroy(TickitWindow *win);

// internal API for event management
void tickit_window_tick(TickitWindow *win);

/* Event hooks */

typedef void TickitWindowEventFn(TickitWindow *win, TickitEventType ev, TickitEventInfo *args, void *data);

int  tickit_window_bind_event(TickitWindow *win, TickitEventType ev, TickitWindowEventFn *fn, void *data);
void tickit_window_unbind_event_id(TickitWindow *win, int id);

/* Layering */

void tickit_window_raise(TickitWindow *win);
void tickit_window_raise_to_front(TickitWindow *win);
void tickit_window_lower(TickitWindow *win);
void tickit_window_lower_to_back(TickitWindow *win);

/* Visibility */

void tickit_window_show(TickitWindow *win);
void tickit_window_hide(TickitWindow *win);
bool tickit_window_is_visible(TickitWindow *win);

/* Geometry management */

int tickit_window_top(const TickitWindow *win);
int tickit_window_abs_top(const TickitWindow *win);
int tickit_window_left(const TickitWindow *win);
int tickit_window_abs_left(const TickitWindow *win);
int tickit_window_lines(const TickitWindow *win);
int tickit_window_cols(const TickitWindow *win);
int tickit_window_bottom(const TickitWindow *win);
int tickit_window_right(const TickitWindow *win);

void tickit_window_resize(TickitWindow *win, int lines, int cols);
void tickit_window_reposition(TickitWindow *win, int top, int left);
void tickit_window_set_geometry(TickitWindow *win, int top, int left, int lines, int cols);

/* Drawing */

void tickit_window_set_pen(TickitWindow *win, TickitPen *pen);
void tickit_window_expose(TickitWindow *win, const TickitRect *exposed);

/* Cursor */

void tickit_window_cursor_at(TickitWindow *win, int line, int col);
void tickit_window_cursor_visible(TickitWindow *win, bool visible);
void tickit_window_cursor_shape(TickitWindow *win, TickitTermCursorShape shape);

/* Focus */

void tickit_window_take_focus(TickitWindow *win);
bool tickit_window_is_focused(TickitWindow *win);
void tickit_window_set_focus_child_notify(TickitWindow *win, bool notify);

typedef enum {
  TICKIT_FOCUSEV_IN = 1,
  TICKIT_FOCUSEV_OUT,
} TickitFocusEventType;

#endif

#ifdef __cplusplus
}
#endif
