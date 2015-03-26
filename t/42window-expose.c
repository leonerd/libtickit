#include "tickit.h"
#include "tickit-window.h"
#include "taplib.h"
#include "taplib-mockterm.h"

#include <stdio.h>  // sprintf

void is_rect(TickitRect a, TickitRect b, char *name)
{
  if(a.top != b.top) {
    fail(name);
    diag("Got a.top = %d, expected b.top = %d", a.top, b.top);
    return;
  }
  if(a.left != b.left) {
    fail(name);
    diag("Got a.left = %d, expected b.left = %d", a.left, b.left);
    return;
  }
  if(a.lines != b.lines) {
    fail(name);
    diag("Got a.lines = %d, expected b.lines = %d", a.lines, b.lines);
    return;
  }
  if(a.cols != b.cols) {
    fail(name);
    diag("Got a.cols = %d, expected b.cols = %d", a.cols, b.cols);
    return;
  }

  pass(name);
}

int on_expose_incr(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  (*(int *)data)++;
  return 1;
}

static int next_rect = 0;
static TickitRect exposed_rects[16];

int on_expose_pushrect(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  if(next_rect >= sizeof(exposed_rects)/sizeof(exposed_rects[0]))
    return 0;

  exposed_rects[next_rect++] = info->rect;
  return 1;
}

int on_expose_render_text(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  TickitRenderBuffer *rb = info->rb;

  switch(*(int *)data) {
    case 1:
      tickit_renderbuffer_text_at(rb, 1, 1, "The text", NULL);
      tickit_renderbuffer_erase_at(rb, 2, 2, 4, NULL);
      break;

    case 2:
      for(int line = info->rect.top; line < info->rect.top + info->rect.lines; line++) {
        char buffer[16];
        sprintf(buffer, "Line %d", line);
        tickit_renderbuffer_text_at(rb, line, 0, buffer, NULL);
      }
      break;

    case 3: // parent
      tickit_renderbuffer_text_at(rb, 0,  0, "Parent", NULL);
      tickit_renderbuffer_text_at(rb, 0, 14, "Parent", NULL);
      break;

    case 4: // child
      tickit_renderbuffer_text_at(rb, 0, 0, "Child", NULL);
      break;
  }

  return 1;
}

int on_expose_fillX(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  for(int line = info->rect.top; line < info->rect.top + info->rect.lines; line++) {
    char buffer[80];
    for(int i = 0; i < info->rect.cols; i++)
      buffer[i] = 'X';
    buffer[info->rect.cols] = 0;
    tickit_renderbuffer_text_at(info->rb, line, info->rect.left, buffer, NULL);
  }

  return 1;
}

int on_expose_textat(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  tickit_renderbuffer_text_at(info->rb, 0, 0, data, NULL);
  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *win = tickit_window_new_subwindow(root, 3, 10, 4, 20);
  tickit_window_tick(root);

  int root_exposed = 0;
  tickit_window_bind_event(root, TICKIT_EV_EXPOSE, &on_expose_incr, &root_exposed);

  // Basics
  {
    int win_exposed = 0;
    int bind_id = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_incr, &win_exposed);
    int bind_id2 = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_pushrect, NULL);

    tickit_window_expose(root, NULL);

    is_int(win_exposed, 0, "EV_EXPOSE not yet invoked");

    tickit_window_tick(root);

    is_int(root_exposed, 1, "root expose count after tick");
    is_int(win_exposed,  1, "win expose count after tick");

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects[0], (TickitRect){ .top = 0, .left = 0, .lines = 4, .cols = 20 }, "exposed_rects[0]");

    next_rect = 0;

    tickit_window_expose(win, NULL);

    tickit_window_tick(root);

    is_int(root_exposed, 2, "root expose count after expose on win");
    is_int(win_exposed,  2, "win expose count after expose on win");

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects[0], (TickitRect){ .top = 0, .left = 0, .lines = 4, .cols = 20 }, "exposed_rects[0]");

    next_rect = 0;

    tickit_window_expose(root, NULL);
    tickit_window_expose(win, NULL);

    tickit_window_tick(root);

    is_int(root_exposed, 3, "root expose count after expose on root-then-win");
    is_int(win_exposed,  3, "win expose count after expose on root-then-win");

    tickit_window_expose(win, NULL);
    tickit_window_expose(root, NULL);

    tickit_window_tick(root);

    is_int(root_exposed, 4, "root expose count after expose on win-then-root");
    is_int(win_exposed,  4, "win expose count after expose on win-then-root");

    tickit_window_hide(win);

    tickit_window_tick(root);

    is_int(root_exposed, 5, "root expose count after hide");
    is_int(win_exposed,  4, "win expose count after hide");

    tickit_window_show(win);

    tickit_window_tick(root);

    is_int(root_exposed, 6, "root expose count after show");
    is_int(win_exposed,  5, "win expose count after show");

    next_rect = 0;

    tickit_window_expose(win, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 20 });
    tickit_window_expose(win, &(TickitRect){ .top = 2, .left = 0, .lines = 1, .cols = 20 });

    tickit_window_tick(root);

    is_int(win_exposed, 7, "win expose count after expose two regions");

    is_int(next_rect, 2, "exposed 2 regions");
    is_rect(exposed_rects[0], (TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 20 }, "exposed_rects[0]");
    is_rect(exposed_rects[1], (TickitRect){ .top = 2, .left = 0, .lines = 1, .cols = 20 }, "exposed_rects[1]");

    next_rect = 0;

    tickit_window_expose(root, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 20 });
    tickit_window_expose(win, &(TickitRect){ .top = 0, .left = 5, .lines = 1, .cols = 10 });

    tickit_window_tick(root);

    is_int(win_exposed, 8, "win expose count after expose separate root+win");

    is_int(next_rect, 1, "exposed 1 region");
    is_rect(exposed_rects[0], (TickitRect){ .top = 0, .left = 5, .lines = 1, .cols = 10 }, "exposed_rects[0]");

    next_rect = 0;

    tickit_window_expose(win, &(TickitRect){ .top = -2, .left = -2, .lines = 50, .cols = 200 });

    tickit_window_tick(root);

    is_int(next_rect, 1, "exposed 1 region");
    is_rect(exposed_rects[0], (TickitRect){ .top = 0, .left = 0, .lines = 4, .cols = 20 }, "exposed_rects[0]");

    tickit_window_unbind_event_id(win, bind_id);
    tickit_window_unbind_event_id(win, bind_id2);
  }

  // Rendering inside EV_EXPOSE
  {
    int idx = 1;
    int bind_id = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_render_text, &idx);

    tickit_window_expose(win, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog after Window expose with output",
        GOTO(4,11), SETPEN(), PRINT("The text"),
        GOTO(5,12), SETPEN(), ERASECH(4,-1),
        NULL);

    idx = 2;

    tickit_window_expose(win, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 20 });
    tickit_window_expose(win, &(TickitRect){ .top = 2, .left = 0, .lines = 1, .cols = 20 });
    tickit_window_tick(root);

    is_termlog("Termlog after Window expose twice",
        GOTO(3,10), SETPEN(), PRINT("Line 0"),
        GOTO(5,10), SETPEN(), PRINT("Line 2"),
        NULL);

    tickit_window_unbind_event_id(win, bind_id);
  }

  // Rendering parent and child simultaneously
  {
    int idx_in_win = 3;
    int bind_id_in_win = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_render_text, &idx_in_win);

    tickit_window_expose(win, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 20 });

    TickitWindow *sub = tickit_window_new_subwindow(win, 0, 7, 1, 7);

    int idx_in_sub = 4;
    tickit_window_bind_event(sub, TICKIT_EV_EXPOSE, &on_expose_render_text, &idx_in_sub);

    tickit_window_tick(root);

    is_termlog("Display after simultaneous expose in parent + child",
        GOTO(3,10), SETPEN(), PRINT("Parent"),
        GOTO(3,17), SETPEN(), PRINT("Child"),
        GOTO(3,24), SETPEN(), PRINT("Parent"),
        NULL);

    tickit_window_destroy(sub);

    tickit_window_tick(root);

    tickit_window_unbind_event_id(win, bind_id_in_win);
  }

  // Expose count
  {
    int exposed = 0;
    int bind_id = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_incr, &exposed);

    for(int i = 0; i < 100; i++) {
      tickit_window_expose(win, NULL);
      tickit_window_tick(root);
    }

    is_int(exposed, 100, "exposed 100 times");

    tickit_window_unbind_event_id(win, bind_id);
  }

  // Child masks a hole in parent
  {
    int bind_id = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_fillX, NULL);

    TickitWindow *sub = tickit_window_new_subwindow(win, 0, 5, 1, 10);
    // no expose event

    tickit_window_expose(win, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 80 });
    tickit_window_tick(root);

    is_termlog("Termlog after expose parent with visible child",
        GOTO(3,10), SETPEN(), PRINT("XXXXX"),
        GOTO(3,25), SETPEN(), PRINT("XXXXX"),
        NULL);

    tickit_window_destroy(sub);
    tickit_window_unbind_event_id(win, bind_id);
  }

  tickit_window_destroy(win);

  // Window ordering
  {
    TickitWindow *winA = tickit_window_new_subwindow(root, 0, 0, 4, 80);
    TickitWindow *winB = tickit_window_new_subwindow(root, 0, 0, 4, 80);
    TickitWindow *winC = tickit_window_new_subwindow(root, 0, 0, 4, 80);
    tickit_window_tick(root);

    int bind_idA = tickit_window_bind_event(winA, TICKIT_EV_EXPOSE, &on_expose_textat, "Window A");
    int bind_idB = tickit_window_bind_event(winB, TICKIT_EV_EXPOSE, &on_expose_textat, "Window B");
    int bind_idC = tickit_window_bind_event(winC, TICKIT_EV_EXPOSE, &on_expose_textat, "Window C");

    tickit_window_expose(root, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog for overlapping initially",
        GOTO(0,0), SETPEN(), PRINT("Window A"),
        NULL);

    tickit_window_raise(winB);
    tickit_window_tick(root);

    is_termlog("Termlog for overlapping after winB raise",
        GOTO(0,0), SETPEN(), PRINT("Window B"),
        NULL);

    tickit_window_lower(winB);
    tickit_window_tick(root);

    is_termlog("Termlog for overlapping after winB lower",
        GOTO(0,0), SETPEN(), PRINT("Window A"),
        NULL);

    tickit_window_raise_to_front(winC);
    tickit_window_tick(root);

    is_termlog("Termlog for overlapping after winC raise_to_front",
        GOTO(0,0), SETPEN(), PRINT("Window C"),
        NULL);

    tickit_window_destroy(winA);
    tickit_window_destroy(winB);
    tickit_window_destroy(winC);
  }

  tickit_window_destroy(root);
  tickit_term_destroy(tt);

  return exit_status();
}
