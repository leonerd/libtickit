#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define streq(a,b) (!strcmp(a,b))

struct {
  char *name;
  int  val;

  TickitPen *pen_fg, *pen_fg_hi, *pen_bg, *pen_bg_hi;
} colours[] = {
  { "red   ", 1 },
  { "blue  ", 4 },
  { "green ", 2 },
  { "yellow", 3 },
};

struct {
  char *name;
  TickitPenAttr attr;

  TickitPen *pen;
} attrs[] = {
  { "bold",           TICKIT_PEN_BOLD },
  { "underline",      TICKIT_PEN_UNDER },
  { "italic",         TICKIT_PEN_ITALIC },
  { "strikethrough",  TICKIT_PEN_STRIKE },
  { "reverse video",  TICKIT_PEN_REVERSE },
  { "blink",          TICKIT_PEN_BLINK },
  { "alternate font", TICKIT_PEN_ALTFONT },
};

static int on_expose(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  /* ANSI colours foreground */
  tickit_renderbuffer_goto(rb, 0, 0);

  for(int i = 0; i < 4; i++) {
    tickit_renderbuffer_savepen(rb);

    if(!colours[i].pen_fg)
      colours[i].pen_fg = tickit_pen_new_attrs(
          TICKIT_PEN_FG, colours[i].val,
          0);

    tickit_renderbuffer_setpen(rb, colours[i].pen_fg);
    tickit_renderbuffer_textf(rb, "fg %s", colours[i].name);

    tickit_renderbuffer_restore(rb);

    tickit_renderbuffer_text(rb, "     ");
  }

  /* ANSI high-brightness colours foreground */
  tickit_renderbuffer_goto(rb, 2, 0);

  for(int i = 0; i < 4; i++) {
    tickit_renderbuffer_savepen(rb);

    if(!colours[i].pen_fg_hi)
      colours[i].pen_fg_hi = tickit_pen_new_attrs(
          TICKIT_PEN_FG, colours[i].val+8,
          0);

    tickit_renderbuffer_setpen(rb, colours[i].pen_fg_hi);
    tickit_renderbuffer_textf(rb, "fg hi-%s", colours[i].name);

    tickit_renderbuffer_restore(rb);

    tickit_renderbuffer_text(rb, "  ");
  }

  /* ANSI colours background */
  tickit_renderbuffer_goto(rb, 4, 0);

  for(int i = 0; i < 4; i++) {
    tickit_renderbuffer_savepen(rb);

    if(!colours[i].pen_bg)
      colours[i].pen_bg = tickit_pen_new_attrs(
          TICKIT_PEN_BG, colours[i].val,
          TICKIT_PEN_FG, 0,
          0);

    tickit_renderbuffer_setpen(rb, colours[i].pen_bg);
    tickit_renderbuffer_textf(rb, "bg %s", colours[i].name);

    tickit_renderbuffer_restore(rb);

    tickit_renderbuffer_text(rb, "     ");
  }

  tickit_renderbuffer_goto(rb, 6, 0);

  /* ANSI high-brightness colours background */
  for(int i = 0; i < 4; i++) {
    tickit_renderbuffer_savepen(rb);

    if(!colours[i].pen_bg_hi)
      colours[i].pen_bg_hi = tickit_pen_new_attrs(
          TICKIT_PEN_BG, colours[i].val+8,
          TICKIT_PEN_FG, 0,
          0);

    tickit_renderbuffer_setpen(rb, colours[i].pen_bg_hi);
    tickit_renderbuffer_textf(rb, "bg hi-%s", colours[i].name);

    tickit_renderbuffer_restore(rb);

    tickit_renderbuffer_text(rb, "  ");
  }

  /* Some interesting rendering attributes */
  for(int i = 0; i < 7; i++) {
    tickit_renderbuffer_savepen(rb);

    tickit_renderbuffer_goto(rb, 8 + 2*i, 0);

    if(!attrs[i].pen)
      attrs[i].pen = tickit_pen_new_attrs(attrs[i].attr, 1, 0);

    tickit_renderbuffer_setpen(rb, attrs[i].pen);
    tickit_renderbuffer_text(rb, attrs[i].name);

    if(attrs[i].attr == TICKIT_PEN_UNDER) {
      /* show off other styles */
      TickitPenUnderline under;

      for(under = TICKIT_PEN_UNDER_DOUBLE; under < TICKIT_N_PEN_UNDERS; under++) {
        TickitPen *pen = tickit_pen_new_attrs(TICKIT_PEN_UNDER, under, 0);
        tickit_renderbuffer_skip(rb, 4);

        tickit_renderbuffer_setpen(rb, pen);
        tickit_renderbuffer_text(rb, 
            (char *[]){NULL, NULL, "double-underline", "undercurl"}[under]);
      }
    }

    tickit_renderbuffer_restore(rb);
  }

  // might not be room below but we can put this off to the size
  {
    TickitPen *pen = tickit_pen_new();

    tickit_renderbuffer_goto(rb, 8 + 4, 20);

    tickit_pen_set_int_attr(pen, TICKIT_PEN_SIZEPOS, TICKIT_PEN_SIZEPOS_SUPERSCRIPT);
    tickit_renderbuffer_text(rb, "super");

    {
      tickit_renderbuffer_savepen(rb);
      tickit_renderbuffer_setpen(rb, pen);
      tickit_renderbuffer_text(rb, "script");
      tickit_renderbuffer_restore(rb);
    }

    tickit_renderbuffer_goto(rb, 8 + 6, 20);

    tickit_pen_set_int_attr(pen, TICKIT_PEN_SIZEPOS, TICKIT_PEN_SIZEPOS_SUBSCRIPT);
    tickit_renderbuffer_text(rb, "sub");

    {
      tickit_renderbuffer_savepen(rb);
      tickit_renderbuffer_setpen(rb, pen);
      tickit_renderbuffer_text(rb, "script");
      tickit_renderbuffer_restore(rb);
    }
  }

  return 1;
}

static int checksuspend(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitKeyEventInfo *info = _info;
  if(info->mod != TICKIT_MOD_CTRL ||
      !streq(info->str, "C-z"))
    return 0;

  TickitTerm *term = tickit_window_get_term(win);

  tickit_term_pause(term);
  raise(SIGSTOP);
  tickit_term_resume(term);
  tickit_window_expose(win, NULL);

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_stdtty();
  if(!t) {
    fprintf(stderr, "Cannot create Tickit - %s\n", strerror(errno));
    return 1;
  }

  TickitWindow *root = tickit_get_rootwin(t);
  if(!root) {
    fprintf(stderr, "Cannot create root window - %s\n", strerror(errno));
    return 1;
  }

  tickit_window_bind_event(root, TICKIT_WINDOW_ON_EXPOSE, 0, &on_expose, NULL);

  tickit_window_bind_event(root, TICKIT_WINDOW_ON_KEY, 0, &checksuspend, NULL);

  tickit_run(t);

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}
