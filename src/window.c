#include "tickit-window.h"
#include "hooklists.h"

#include <stdio.h>

/* TODO:
 *   Scrolling
 *   Input events (mouse, keyboard)
 */

#define ROOT_AS_WINDOW(root) (TickitWindow*)root
#define WINDOW_AS_ROOT(win) (TickitRootWindow*)win

typedef enum {
  TICKIT_HIERARCHY_INSERT_FIRST,
  TICKIT_HIERARCHY_INSERT_LAST,
  TICKIT_HIERARCHY_REMOVE,
  TICKIT_HIERARCHY_RAISE,
  TICKIT_HIERARCHY_RAISE_FRONT,
  TICKIT_HIERARCHY_LOWER,
  TICKIT_HIERARCHY_LOWER_BACK
} HierarchyChangeType;

struct TickitWindow {
  TickitWindow *parent;
  TickitWindow *first_child;
  TickitWindow *next;
  TickitWindow *focused_child;
  TickitPen *pen;
  TickitRect rect;
  struct {
    int line;
    int col;
    TickitTermCursorShape shape;
    bool visible;
  } cursor;
  bool is_visible;
  bool is_focused;
  bool steal_input;
  bool focus_child_notify;

  struct TickitEventHook *hooks;
};

DEFINE_HOOKLIST_FUNCS(window,TickitWindow,TickitWindowEventFn)

typedef struct HierarchyChange HierarchyChange;
struct HierarchyChange {
  HierarchyChangeType change;
  TickitWindow *parent;
  TickitWindow *win;
  HierarchyChange *next;
};

typedef struct TickitRootWindow TickitRootWindow;
struct TickitRootWindow {
  TickitWindow win;

  TickitTerm *term;
  TickitRectSet *damage;
  HierarchyChange *hierarchy_changes;
  bool needs_expose;
  bool needs_restore;
  bool needs_later_processing;

  int event_id;
};

static void _request_restore(TickitRootWindow *root);
static void _request_later_processing(TickitRootWindow *root);
static void _request_hierarchy_change(HierarchyChangeType, TickitWindow *);
static void _do_hierarchy_change(HierarchyChangeType change, TickitWindow *parent, TickitWindow *win);
static void _purge_hierarchy_changes(TickitWindow *win);
static int _handle_key(TickitWindow *win, TickitKeyEventInfo *args);
static int _handle_mouse(TickitWindow *win, TickitMouseEventInfo *args);

static int on_term(TickitTerm *term, TickitEventType ev, void *_info, void *data)
{
  TickitRootWindow *root = data;

  if(ev & TICKIT_EV_RESIZE) {
    TickitResizeEventInfo *info = _info;
    tickit_window_resize(ROOT_AS_WINDOW(root), info->lines, info->cols);
  }

  if(ev & TICKIT_EV_KEY)
    _handle_key(ROOT_AS_WINDOW(root), (TickitKeyEventInfo *)_info);

  if(ev & TICKIT_EV_MOUSE)
    _handle_mouse(ROOT_AS_WINDOW(root), (TickitMouseEventInfo *)_info);

  return 1;
}

static void init_window(TickitWindow *win, TickitWindow *parent, int top, int left, int lines, int cols)
{
  win->parent = parent;
  win->first_child = NULL;
  win->next = NULL;
  win->focused_child = NULL;
  win->pen = tickit_pen_new();
  win->rect.top = top;
  win->rect.left = left;
  win->rect.lines = lines;
  win->rect.cols = cols;
  win->cursor.line = 0;
  win->cursor.col = 0;
  win->cursor.shape = TICKIT_TERM_CURSORSHAPE_BLOCK;
  win->cursor.visible = true;
  win->is_visible = true;
  win->is_focused = false;
  win->steal_input = false;
  win->focus_child_notify = false;

  win->hooks = NULL;
}

static TickitWindow* new_window(TickitWindow *parent, int top, int left, int lines, int cols)
{
  TickitWindow *win = malloc(sizeof(TickitWindow));
  if(!win)
    return NULL;

  init_window(win, parent, top, left, lines, cols);

  return win;
}

TickitWindow* tickit_window_new_root(TickitTerm *term)
{
  int lines, cols;
  tickit_term_get_size(term, &lines, &cols);

  TickitRootWindow *root = malloc(sizeof(TickitRootWindow));
  if(!root)
    return NULL;

  init_window(ROOT_AS_WINDOW(root), NULL, 0, 0, lines, cols);

  root->term = term;
  root->hierarchy_changes = NULL;
  root->needs_expose = false;
  root->needs_restore = false;
  root->needs_later_processing = false;

  root->damage = tickit_rectset_new();
  if(!root->damage) {
    tickit_window_destroy(ROOT_AS_WINDOW(root));
    return NULL;
  }

  root->event_id = tickit_term_bind_event(term, TICKIT_EV_RESIZE|TICKIT_EV_KEY|TICKIT_EV_MOUSE,
      &on_term, root);

  return ROOT_AS_WINDOW(root);
}

static TickitRootWindow *_get_root(const TickitWindow *win)
{
  const TickitWindow *root = win;
  while(root->parent) {
    root = root->parent;
  }
  return WINDOW_AS_ROOT(root);
}

TickitWindow *tickit_window_new_subwindow(TickitWindow *parent, int top, int left, int lines, int cols)
{
  TickitWindow *win = new_window(parent, top, left, lines, cols);
  _request_hierarchy_change(TICKIT_HIERARCHY_INSERT_LAST, win);
  return win;
}

TickitWindow *tickit_window_new_hidden_subwindow(TickitWindow *parent, int top, int left, int lines, int cols)
{
  TickitWindow *win = new_window(parent, top, left, lines, cols);
  _request_hierarchy_change(TICKIT_HIERARCHY_INSERT_LAST, win);
  win->is_visible = false;
  return win;
}

TickitWindow *tickit_window_new_float(TickitWindow *parent, int top, int left, int lines, int cols)
{
  TickitWindow *win = new_window(parent, top, left, lines, cols);
  _request_hierarchy_change(TICKIT_HIERARCHY_INSERT_FIRST, win);
  return win;
}

TickitWindow *tickit_window_new_popup(TickitWindow *parent, int top, int left, int lines, int cols)
{
  TickitWindow *root = parent;
  while(root->parent) {
    top += root->rect.top;
    left += root->rect.left;
    root = root->parent;
  }
  TickitWindow *win = new_window(root, top, left, lines, cols);
  _request_hierarchy_change(TICKIT_HIERARCHY_INSERT_FIRST, win);
  win->steal_input = true;
  return win;
}

TickitWindow *tickit_window_parent(const TickitWindow *win)
{
  return win->parent;
}

TickitWindow *tickit_window_root(const TickitWindow *win)
{
  return ROOT_AS_WINDOW(_get_root(win));
}

void tickit_window_destroy(TickitWindow *win)
{
  tickit_hooklist_unbind_and_destroy(win->hooks, win);

  if(win->pen)
    tickit_pen_destroy(win->pen);

  for(TickitWindow *child = win->first_child; child; child = child->next)
    tickit_window_destroy(child);

  _purge_hierarchy_changes(win);

  if(win->parent)
    _do_hierarchy_change(TICKIT_HIERARCHY_REMOVE, win->parent, win);

  /* Root cleanup */
  if(!win->parent) {
    TickitRootWindow *root = WINDOW_AS_ROOT(win);
    if(root->damage) {
      tickit_rectset_destroy(root->damage);
    }

    tickit_term_unbind_event_id(root->term, root->event_id);
  }

  free(win);
}

void tickit_window_raise(TickitWindow *win)
{
  _request_hierarchy_change(TICKIT_HIERARCHY_RAISE, win);
}

void tickit_window_raise_to_front(TickitWindow *win)
{
  _request_hierarchy_change(TICKIT_HIERARCHY_RAISE_FRONT, win);
}

void tickit_window_lower(TickitWindow *win)
{
  _request_hierarchy_change(TICKIT_HIERARCHY_LOWER, win);
}

void tickit_window_lower_to_back(TickitWindow *win)
{
  _request_hierarchy_change(TICKIT_HIERARCHY_LOWER_BACK, win);
}

void tickit_window_show(TickitWindow *win)
{
  win->is_visible = true;
  if(win->parent) {
    if(!win->parent->focused_child &&
       (win->focused_child || win->is_focused)) {
      win->parent->focused_child = win;
    }
  }
  tickit_window_expose(win, NULL);
}

void tickit_window_hide(TickitWindow *win)
{
  win->is_visible = false;

  if(win->parent) {
    TickitWindow *parent = win->parent;
    if(parent->focused_child && (parent->focused_child == win)) {
      parent->focused_child = NULL;
    }
    tickit_window_expose(parent, &win->rect);
  }
}

bool tickit_window_is_visible(TickitWindow *win)
{
  return win->is_visible;
}

int tickit_window_top(const TickitWindow *win)
{
  return win->rect.top;
}

int tickit_window_abs_top(const TickitWindow *win)
{
  int top = win->rect.top;
  TickitWindow* parent = win->parent;
  while(parent) {
    top += parent->rect.top;
    parent = parent->parent;
  }
  return top;
}

int tickit_window_left(const TickitWindow *win)
{
  return win->rect.left;
}

int tickit_window_abs_left(const TickitWindow *win)
{
  int left = win->rect.left;
  TickitWindow* parent = win->parent;
  while(parent) {
    left += parent->rect.left;
    parent = parent->parent;
  }
  return left;
}

int tickit_window_lines(const TickitWindow *win)
{
  return win->rect.lines;
}

int tickit_window_cols(const TickitWindow *win)
{
  return win->rect.cols;
}

int tickit_window_bottom(const TickitWindow *win)
{
  return win->rect.top + win->rect.lines;
}

int tickit_window_right(const TickitWindow *win)
{
  return win->rect.left + win->rect.cols;
}

void tickit_window_resize(TickitWindow *win, int lines, int cols)
{
  tickit_window_set_geometry(win, win->rect.top, win->rect.left, lines, cols);
}

void tickit_window_reposition(TickitWindow *win, int top, int left)
{
  tickit_window_set_geometry(win, top, left, win->rect.lines, win->rect.cols);

  if(win->is_focused)
    _request_restore(_get_root(win));
}

void tickit_window_set_geometry(TickitWindow *win, int top, int left, int lines, int cols)
{
  if((win->rect.top != top) ||
     (win->rect.left != left) ||
     (win->rect.lines != lines) ||
     (win->rect.cols != cols))
  {
    win->rect.top = top;
    win->rect.left = left;
    win->rect.lines = lines;
    win->rect.cols = cols;

    TickitGeomchangeEventInfo info = {
      .rect = { .top = top, .left = left, .lines = lines, .cols = cols },
    };
    run_events(win, TICKIT_EV_GEOMCHANGE, &info);
  }
}

TickitPen *tickit_window_get_pen(TickitWindow *win)
{
  return win->pen;
}

void tickit_window_set_pen(TickitWindow *win, TickitPen *pen)
{
  /* TODO: Refcounting the pen would be nice. Until then, we assume we don't own it. */
  win->pen = pen;
}

void tickit_window_expose(TickitWindow *win, const TickitRect *exposed)
{
  TickitRect selfrect = { .top = 0, .left = 0, .lines = win->rect.lines, .cols = win->rect.cols };
  TickitRect damaged;

  if(exposed) {
    if(!tickit_rect_intersect(&damaged, &selfrect, exposed))
      return;
  }
  else
    damaged = selfrect;

  if(!win->is_visible)
    return;

  if(win->parent) {
    tickit_rect_translate(&damaged, win->rect.top, win->rect.left);
    tickit_window_expose(win->parent, &damaged);
    return;
  }

  /* If we're here, then we're a root win. */
  TickitRootWindow *root = WINDOW_AS_ROOT(win);
  if(tickit_rectset_contains(root->damage, &damaged))
    return;

  tickit_rectset_add(root->damage, &damaged);

  root->needs_expose = true;
  _request_later_processing(root);
}

static void _do_expose(TickitWindow *win, const TickitRect *rect, TickitRenderBuffer *rb)
{
  if(win->pen)
    tickit_renderbuffer_setpen(rb, win->pen);

  for(TickitWindow* child = win->first_child; child; child = child->next) {
    if(!child->is_visible)
      continue;

    TickitRect exposed;
    if(tickit_rect_intersect(&exposed, rect, &child->rect)) {
      tickit_renderbuffer_save(rb);

      tickit_renderbuffer_clip(rb, &exposed);
      tickit_renderbuffer_translate(rb, child->rect.top, child->rect.left);
      tickit_rect_translate(&exposed, -child->rect.top, -child->rect.left);
      _do_expose(child, &exposed, rb);

      tickit_renderbuffer_restore(rb);
    }

    tickit_renderbuffer_mask(rb, &child->rect);
  }

  TickitExposeEventInfo info = {
    .rect = *rect,
    .rb = rb,
  };
  run_events(win, TICKIT_EV_EXPOSE, &info);
}

static void _request_restore(TickitRootWindow *root)
{
  root->needs_restore = true;
  _request_later_processing(root);
}

static void _request_later_processing(TickitRootWindow *root)
{
  root->needs_later_processing = true;
}

static void _do_restore(TickitRootWindow *root)
{
  TickitWindow *win = ROOT_AS_WINDOW(root);

  while(win) {
    if(!win->is_visible)
      break;

    if(!win->focused_child)
      break;

    win = win->focused_child;
  }

  if(win && win->is_focused && win->cursor.visible) {
    /* TODO finish the visibility check here. */
    tickit_term_setctl_int(root->term, TICKIT_TERMCTL_CURSORVIS, 1);
    int cursor_line = win->cursor.line + tickit_window_abs_top(win);
    int cursor_col = win->cursor.col + tickit_window_abs_left(win);
    tickit_term_goto(root->term, cursor_line, cursor_col);
    tickit_term_setctl_int(root->term, TICKIT_TERMCTL_CURSORSHAPE, win->cursor.shape);
  }
  else
    tickit_term_setctl_int(root->term, TICKIT_TERMCTL_CURSORVIS, 0);

  tickit_term_flush(root->term);
}

void tickit_window_tick(TickitWindow *win)
{
  if(win->parent)
    // Can't tick non-root.
    return;

  TickitRootWindow *root = WINDOW_AS_ROOT(win);
  if(!root->needs_later_processing)
    return;

  root->needs_later_processing = false;

  if(root->hierarchy_changes) {
    HierarchyChange *req = root->hierarchy_changes;
    while(req) {
      _do_hierarchy_change(req->change, req->parent, req->win);
      HierarchyChange *next = req->next;
      free(req);
      req = next;
    }
    root->hierarchy_changes = NULL;
  }

  if(root->needs_expose) {
    root->needs_expose = false;

    TickitWindow *root_window = ROOT_AS_WINDOW(root);
    TickitRenderBuffer *rb = tickit_renderbuffer_new(root_window->rect.lines, root_window->rect.cols);

    int damage_count = tickit_rectset_rects(root->damage);
    TickitRect *rects = malloc(damage_count * sizeof(TickitRect));
    tickit_rectset_get_rects(root->damage, rects, damage_count);

    tickit_rectset_clear(root->damage);

    for(int i = 0; i < damage_count; i++) {
      TickitRect *rect = &rects[i];
      tickit_renderbuffer_save(rb);
      tickit_renderbuffer_clip(rb, rect);
      _do_expose(root_window, rect, rb);
      tickit_renderbuffer_restore(rb);
    }

    free(rects);

    tickit_renderbuffer_flush_to_term(rb, root->term);
    tickit_renderbuffer_destroy(rb);

    root->needs_restore = true;
  }

  if(root->needs_restore) {
    root->needs_restore = false;
    _do_restore(root);
  }
}

static TickitWindow **_find_child(TickitWindow *parent, TickitWindow *win)
{
  TickitWindow **winp = &parent->first_child;
  while(*winp && *winp != win)
    winp = &(*winp)->next;

  return winp;
}

static void _do_hierarchy_insert_first(TickitWindow *parent, TickitWindow *win)
{
  win->next = parent->first_child;
  parent->first_child = win;
}

static void _do_hierarchy_insert_last(TickitWindow *parent, TickitWindow *win)
{
  TickitWindow **lastp = &parent->first_child;
  while(*lastp)
    lastp = &(*lastp)->next;

  *lastp = win;
  win->next = NULL;
}

static void _do_hierarchy_remove(TickitWindow *parent, TickitWindow *win)
{
  TickitWindow **winp = _find_child(parent, win);
  if(!winp)
    return;

  *winp = (*winp)->next;
  win->next = NULL;
}

static void _do_hierarchy_raise(TickitWindow *parent, TickitWindow *win)
{
  TickitWindow **prevp = &parent->first_child;
  if(*prevp == win) // already first
    return;

  while(*prevp && (*prevp)->next != win)
    prevp = &(*prevp)->next;

  if(!prevp) // not found
    return;

  TickitWindow *after  = win->next;

  win->next = *prevp;
  (*prevp)->next = after;
  *prevp = win;
}

static void _do_hierarchy_lower(TickitWindow *parent, TickitWindow *win)
{
  TickitWindow **winp = _find_child(parent, win);
  if(!winp) // not found
    return;

  TickitWindow *after = win->next;
  if(!after) // already last
    return;

  win->next = after->next;
  *winp = after;
  after->next = win;
}

static void _do_hierarchy_change(HierarchyChangeType change, TickitWindow *parent, TickitWindow *win)
{
  switch(change) {
    case TICKIT_HIERARCHY_INSERT_FIRST:
      _do_hierarchy_insert_first(parent, win);
      break;
    case TICKIT_HIERARCHY_INSERT_LAST:
      _do_hierarchy_insert_last(parent, win);
      break;
    case TICKIT_HIERARCHY_REMOVE: {
        _do_hierarchy_remove(parent, win);
        if(parent->focused_child && parent->focused_child == win) {
          parent->focused_child = NULL;
        }
        break;
      }
    case TICKIT_HIERARCHY_RAISE:
      _do_hierarchy_raise(parent, win);
      break;
    case TICKIT_HIERARCHY_RAISE_FRONT:
      _do_hierarchy_remove(parent, win);
      _do_hierarchy_insert_first(parent, win);
      break;
    case TICKIT_HIERARCHY_LOWER:
      _do_hierarchy_lower(parent, win);
      break;
    case TICKIT_HIERARCHY_LOWER_BACK:
      _do_hierarchy_remove(parent, win);
      _do_hierarchy_insert_last(parent, win);
      break;
  }

  tickit_window_expose(parent, &win->rect);
}

static void _request_hierarchy_change(HierarchyChangeType change, TickitWindow *win)
{
  if(!win->parent)
    /* Can't do anything to the root win */
    return;

  HierarchyChange *req = malloc(sizeof(HierarchyChange));
  req->change = change;
  req->parent = win->parent;
  req->win = win;
  req->next = NULL;
  TickitRootWindow *root = _get_root(win);
  if(!root->hierarchy_changes) {
    root->hierarchy_changes = req;
    _request_later_processing(root);
  }
  else {
    HierarchyChange *chain = root->hierarchy_changes;
    while(chain->next) {
      chain = chain->next;
    }
    chain->next = req;
  }
}

static void _purge_hierarchy_changes(TickitWindow *win)
{
  TickitRootWindow *root = _get_root(win);
  HierarchyChange **changep = &root->hierarchy_changes;
  while(*changep) {
    HierarchyChange *req = *changep;
    if(req->parent == win || req->win == win) {
      *changep = req->next;
      free(req);
    }
    else
      changep = &req->next;
  }
}

bool tickit_window_scrollrect(TickitWindow *win, const TickitRect *rect, int downward, int rightward)
{
  return false;
}

bool tickit_window_scroll(TickitWindow *win, int downward, int rightward)
{
  return tickit_window_scrollrect(win,
    &((TickitRect){ .top = 0, .left = 0, .lines = win->rect.lines, .cols = win->rect.cols }),
    downward, rightward);
}

void tickit_window_cursor_at(TickitWindow *win, int line, int col)
{
  win->cursor.line = line;
  win->cursor.col = col;

  if(win->is_focused)
    _request_restore(_get_root(win));
}

void tickit_window_cursor_visible(TickitWindow *win, bool visible)
{
  win->cursor.visible = visible;

  if(win->is_focused)
    _request_restore(_get_root(win));
}

void tickit_window_cursor_shape(TickitWindow *win, TickitTermCursorShape shape)
{
  win->cursor.shape = shape;

  if(win->is_focused)
    _request_restore(_get_root(win));
}

static void _focus_gained(TickitWindow *win, TickitWindow *child);
static void _focus_lost(TickitWindow *win);

void tickit_window_take_focus(TickitWindow *win)
{
  _focus_gained(win, NULL);
}

static void _focus_gained(TickitWindow *win, TickitWindow *child)
{
  if(win->focused_child && child && win->focused_child != child)
    _focus_lost(win->focused_child);

  if(win->parent) {
    if(win->is_visible)
      _focus_gained(win->parent, win);
  }
  else
    _request_restore(_get_root(win));

  if(!child) {
    win->is_focused = true;

    TickitFocusEventInfo info = { .type = TICKIT_FOCUSEV_IN, .win = win };
    run_events(win, TICKIT_EV_FOCUS, &info);
  }
  else if(win->focus_child_notify) {
    TickitFocusEventInfo info = { .type = TICKIT_FOCUSEV_IN, .win = child };
    run_events(win, TICKIT_EV_FOCUS, &info);
  }

  win->focused_child = child;
}

static void _focus_lost(TickitWindow *win)
{
  if(win->focused_child) {
    _focus_lost(win->focused_child);

    if(win->focus_child_notify) {
      TickitFocusEventInfo info = { .type = TICKIT_FOCUSEV_OUT, .win = win->focused_child };
      run_events(win, TICKIT_EV_FOCUS, &info);
    }
  }

  if(win->is_focused) {
    win->is_focused = false;
    TickitFocusEventInfo info = { .type = TICKIT_FOCUSEV_OUT, .win = win };
    run_events(win, TICKIT_EV_FOCUS, &info);
  }
}

bool tickit_window_is_focused(TickitWindow *win)
{
  return win->is_focused;
}

void tickit_window_set_focus_child_notify(TickitWindow *win, bool notify)
{
  win->focus_child_notify = notify;
}

static int _handle_key(TickitWindow *win, TickitKeyEventInfo *info)
{
  if(!win->is_visible)
    return 0;

  if(win->first_child && win->first_child->steal_input)
    if(_handle_key(win->first_child, info))
      return 1;

  if(win->focused_child)
    if(_handle_key(win->focused_child, info))
      return 1;

  if(run_events_whilefalse(win, TICKIT_EV_KEY, info))
    return 1;

  // Last-ditch attempt to spread it around other children
  for(TickitWindow *child = win->first_child; child; child = child->next) {
    if(child == win->focused_child)
      continue;

    if(_handle_key(child, info))
      return 1;
  }

  return 0;
}

static int _handle_mouse(TickitWindow *win, TickitMouseEventInfo *info)
{
  if(!win->is_visible)
    return 0;

  for(TickitWindow *child = win->first_child; child; child = child->next) {
    int child_line = info->line - child->rect.top;
    int child_col  = info->col  - child->rect.left;

    if(!child->steal_input) {
      if(child_line < 0 || child_line >= child->rect.lines)
        continue;
      if(child_col < 0 || child_col >= child->rect.cols)
        continue;
    }

    TickitMouseEventInfo childinfo = *info;
    childinfo.line = child_line;
    childinfo.col  = child_col;

    if(_handle_mouse(child, &childinfo))
      return 1;
  }

  if(run_events_whilefalse(win, TICKIT_EV_MOUSE, info))
    return 1;

  return 0;
}
