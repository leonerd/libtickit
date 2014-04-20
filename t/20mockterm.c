#include "tickit.h"
#include "tickit-mockterm.h"
#include "taplib.h"

#include <string.h>

#define streq(a,b) (strcmp(a,b)==0)

// TODO: This lot probably wants moving to a taplib-mockterm
// library

static void is_display_text(TickitMockTerm *mt, char *name, ...)
{
  va_list args;
  va_start(args, name);

  int lines, cols;
  tickit_term_get_size((TickitTerm *)mt, &lines, &cols);

  for(int line = 0; line < lines; line++) {
    const char *expect = va_arg(args, char *);
    size_t len = tickit_mockterm_get_display_text(mt, NULL, 0, line, 0, cols);
    char *got = malloc(len + 1);
    tickit_mockterm_get_display_text(mt, got, len, line, 0, cols);

    if(streq(expect, got)) {
      free(got);
      continue;
    }

    fail(name);
    diag("Got line % 2d |%s|", line, got);
    diag("Expected    |%s|", expect);

    free(got);
    return;
  }

  pass(name);
}

static void fillterm(TickitTerm *tt)
{
  tickit_term_goto(tt, 0, 0);
  tickit_term_print(tt, "0000000000");
  tickit_term_goto(tt, 1, 0);
  tickit_term_print(tt, "1111111111");
  tickit_term_goto(tt, 2, 0);
  tickit_term_print(tt, "2222222222");
}

struct TermLogExpectation {
  enum {
    EXPECT_GOTO       = LOG_GOTO,
    EXPECT_PRINT      = LOG_PRINT,
    EXPECT_ERASECH    = LOG_ERASECH,
    EXPECT_CLEAR      = LOG_CLEAR,
    EXPECT_SCROLLRECT = LOG_SCROLLRECT,
    EXPECT_SETPEN     = LOG_SETPEN,
  } type;
  int val[6];
  char *str;
  // These must match pen attr names
  int fg, bg, b, u, i, rv, strike, af;
};

static char *lognames[] = {
  [EXPECT_GOTO]       = "goto",
  [EXPECT_PRINT]      = "print",
  [EXPECT_ERASECH]    = "erasech",
  [EXPECT_CLEAR]      = "clear",
  [EXPECT_SCROLLRECT] = "scrollrect",
  [EXPECT_SETPEN]     = "setpen",
};

#define GOTO(line,col) \
  &(struct TermLogExpectation){EXPECT_GOTO, .val = {line, col}}
#define PRINT(s) \
  &(struct TermLogExpectation){EXPECT_PRINT, .str = s}
#define ERASECH(count,moveend) \
  &(struct TermLogExpectation){EXPECT_ERASECH, .val = {count, moveend}}
#define CLEAR() \
  &(struct TermLogExpectation){EXPECT_CLEAR}
#define SCROLLRECT(top,left,lines,cols,downward,rightward) \
  &(struct TermLogExpectation){EXPECT_SCROLLRECT, .val = {top, left, lines, cols, downward, rightward}}

// IMPORTANT: set an expectation of this to expect the value 0 explicitly;
//   0 will be treated as unspecified
#define EXPECT_ZERO 0x100000
#define SETPEN(...) \
  &(struct TermLogExpectation){EXPECT_SETPEN, __VA_ARGS__}

static void is_termlog(TickitMockTerm *mt, char *name, ...)
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

    if(exp->type != got->type) {
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
  if(exp = va_arg(args, void *)) {
    fail(name);
    diag("Expected a %s() termlog entry; got none", lognames[exp->type]);
    goto failed;
  }

  pass(name);
failed:
  tickit_mockterm_clearlog(mt);
}

int main(int argc, char *argv[])
{
  TickitMockTerm *mt;
#define TT (TickitTerm *)mt

  mt = tickit_mockterm_new(3, 10);

  is_termlog(mt, "Termlog initially",
      NULL);
  is_display_text(mt, "Display initially",
      "          ",
      "          ",
      "          ");

  tickit_term_goto(TT, 1, 5);
  is_termlog(mt, "Termlog after goto",
      GOTO(1,5),
      NULL);

  tickit_term_print(TT, "foo");

  is_termlog(mt, "Termlog after print",
      PRINT("foo"),
      NULL);
  is_display_text(mt, "Display after print",
      "          ",
      "     foo  ",
      "          ");

  char c = 'l';
  tickit_term_printn(TT, &c, 1);

  is_termlog(mt, "Display after printn 1",
      PRINT("l"),
      NULL);
  is_display_text(mt, "Display after printn 1",
      "          ",
      "     fool ",
      "          ");

  tickit_term_clear(TT);

  is_termlog(mt, "Termlog after clear",
      CLEAR(),
      NULL);
  is_display_text(mt, "Display after clear",
      "          ",
      "          ",
      "          ");

  TickitPen *fg_pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 3, -1);

  tickit_term_setpen(TT, fg_pen);

  is_termlog(mt, "Termlog after setpen",
      SETPEN(.fg=3),
      NULL);

  TickitPen *bg_pen = tickit_pen_new_attrs(TICKIT_PEN_BG, 6, -1);

  tickit_term_chpen(TT, bg_pen);

  is_termlog(mt, "Termlog after chpen",
      SETPEN(.fg=3, .bg=6),
      NULL);

  // Now some test content for scrolling
  fillterm(TT);
  tickit_mockterm_clearlog(mt);

  is_display_text(mt, "Display after scroll fill",
      "0000000000",
      "1111111111",
      "2222222222");

  ok(tickit_term_scrollrect(TT, 0,0,3,10, +1,0), "Scrollrect down OK");
  is_termlog(mt, "Termlog after scroll 1 down",
      SCROLLRECT(0,0,3,10, +1,0),
      NULL);
  is_display_text(mt, "Display after scroll 1 down",
      "1111111111",
      "2222222222",
      "          ");

  ok(tickit_term_scrollrect(TT, 0,0,3,10, -1,0), "Scrollrect up OK");
  is_termlog(mt, "Termlog after scroll 1 up",
      SCROLLRECT(0,0,3,10, -1,0),
      NULL);
  is_display_text(mt, "Display after scroll 1 up",
      "          ",
      "1111111111",
      "2222222222");

  fillterm(TT);
  tickit_mockterm_clearlog(mt);

  tickit_term_scrollrect(TT, 0,0,2,10, +1,0);
  is_termlog(mt, "Termlog after scroll partial 1 down",
      SCROLLRECT(0,0,2,10, +1,0),
      NULL);
  is_display_text(mt, "Display after scroll partial 1 down",
      "1111111111",
      "          ",
      "2222222222");

  tickit_term_scrollrect(TT, 0,0,2,10, -1,0);
  is_termlog(mt, "Termlog after scroll partial 1 up",
      SCROLLRECT(0,0,2,10, -1,0),
      NULL);
  is_display_text(mt, "Display after scroll partial 1 up",
      "          ",
      "1111111111",
      "2222222222");

  for(int line = 0; line < 3; line++)
    tickit_term_goto(TT, line, 0), tickit_term_print(TT, "ABCDEFGHIJ");
  tickit_mockterm_clearlog(mt);

  tickit_term_scrollrect(TT, 0,5,1,5, 0,2);
  is_termlog(mt, "Termlog after scroll right",
      SCROLLRECT(0,5,1,5, 0,+2),
      NULL);
  is_display_text(mt, "Display after scroll right",
      "ABCDEHIJ  ",
      "ABCDEFGHIJ",
      "ABCDEFGHIJ");

  tickit_term_scrollrect(TT, 1,5,1,5, 0,-3);
  is_termlog(mt, "Termlog after scroll left",
      SCROLLRECT(1,5,1,5, 0,-3),
      NULL);
  is_display_text(mt, "Display after scroll left",
      "ABCDEHIJ  ",
      "ABCDE   FG",
      "ABCDEFGHIJ");

  tickit_term_goto(TT, 2, 3);
  tickit_term_erasech(TT, 5, -1);

  is_termlog(mt, "Termlog after erasech",
      GOTO(2,3),
      ERASECH(5, -1),
      NULL);
  is_display_text(mt, "Display after erasech",
      "ABCDEHIJ  ",
      "ABCDE   FG",
      "ABC     IJ");

  return exit_status();
}
