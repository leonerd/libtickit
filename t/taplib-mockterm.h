#include <tickit-mockterm.h>

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

TickitTerm *make_term(int lines, int cols);

void is_termlog(char *name, ...);
