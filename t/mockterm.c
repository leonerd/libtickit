#include "taplib-mockterm.h"
#include "taplib.h"

#include "tickit-mockterm.h"

#include <string.h>

#define streq(a,b) (!strcmp(a,b))

static char *lognames[] = {
  [EXPECT_GOTO]       = "goto",
  [EXPECT_PRINT]      = "print",
  [EXPECT_ERASECH]    = "erasech",
  [EXPECT_CLEAR]      = "clear",
  [EXPECT_SCROLLRECT] = "scrollrect",
  [EXPECT_SETPEN]     = "setpen",
};

static TickitMockTerm *mt;

TickitTerm *make_term(int lines, int cols)
{
  if(!mt)
    mt = tickit_mockterm_new(lines, cols);

  return (TickitTerm *)mt;
}

void drain_termlog(void)
{
  tickit_mockterm_clearlog(mt);
}

void is_termlog(char *name, ...)
{
  va_list args;
  va_start(args, name);

  int loglen = tickit_mockterm_loglen(mt);
  int logi;
  for(logi = 0; logi < loglen; logi++) {
    struct TermLogExpectation *exp = va_arg(args, void *);
    if(!exp)
      break;
    TickitMockTermLogEntry *got = tickit_mockterm_peeklog(mt, logi);

    if((int)exp->type != (int)got->type) {
      fail(name);
      diag("Expected %s(), got %s()", lognames[exp->type], lognames[got->type]);
      goto failed;
    }

    switch(exp->type) {
    case EXPECT_GOTO:
    case EXPECT_ERASECH:
      if(exp->val[0] == got->val1 && exp->val[1] == got->val2)
        continue;
      fail(name);
      diag("Expected %s(%d,%d), got %s(%d,%d)",
          lognames[exp->type], exp->val[0], exp->val[1],
          lognames[got->type], got->val1, got->val2);
      goto failed;

    case EXPECT_PRINT:
      if(streq(exp->str, got->str))
        continue;
      fail(name);
      diag("Expected print(\"%s\"), got print(\"%s\")",
          exp->str, got->str);
      goto failed;

    case EXPECT_CLEAR:
      continue;

    case EXPECT_SCROLLRECT:
      if(exp->val[0] == got->rect.top   && exp->val[1] == got->rect.left &&
         exp->val[2] == got->rect.lines && exp->val[3] == got->rect.cols &&
         exp->val[4] == got->val1       && exp->val[5] == got->val2)
        continue;
      fail(name);
      diag("Expected scrollrect(%d,%d,%d,%d, %+d,%+d), got scrollrect(%d,%d,%d,%d, %+d,%+d)",
          exp->val[0], exp->val[1], exp->val[2], exp->val[3], exp->val[4], exp->val[5],
          got->rect.top, got->rect.left, got->rect.lines, got->rect.cols, got->val1, got->val2);
      goto failed;

    case EXPECT_SETPEN:
      {
        int gotval;
        int expval;
        // Colours are tricky; -1 is the unspecified default
        if((expval = exp->fg == 0 ? -1 : exp->fg == EXPECT_ZERO ? 0 : exp->fg)
            != (gotval = tickit_pen_get_colour_attr(got->pen, TICKIT_PEN_FG))) {
          fail(name);
          diag("Expected setpen(fg=%d,...), got setpen(fg=%d,...)", expval, gotval);
          goto failed;
        }
        if((expval = exp->bg == 0 ? -1 : exp->bg == EXPECT_ZERO ? 0 : exp->bg)
          != (gotval = tickit_pen_get_colour_attr(got->pen, TICKIT_PEN_BG))) {
          fail(name);
          diag("Expected setpen(bg=%d,...), got setpen(bg=%d,...)", expval, gotval);
          goto failed;
        }
        // TODO: add remaining attributes
      }
      break;
    }
  }

  if(logi < loglen) {
    TickitMockTermLogEntry *got = tickit_mockterm_peeklog(mt, logi);

    fail(name);
    diag("Expected no more termlog entries; got %s()", lognames[got->type]);
    goto failed;
  }

  struct TermLogExpectation *exp;
  if((exp = va_arg(args, void *))) {
    fail(name);
    diag("Expected a %s() termlog entry; got none", lognames[exp->type]);
    goto failed;
  }

  pass(name);
failed:
  tickit_mockterm_clearlog(mt);
}

void press_key(int type, char *str, int mod)
{
  TickitKeyEventInfo info = {
    .type = type,
    .mod = mod,
    .str = str,
  };

  tickit_term_emit_key((TickitTerm *)mt, &info);
}

void press_mouse(int type, int button, int line, int col, int mod)
{
  TickitMouseEventInfo info = {
    .type = type,
    .button = button,
    .line = line,
    .col = col,
    .mod = mod,
  };

  tickit_term_emit_mouse((TickitTerm *)mt, &info);
}
