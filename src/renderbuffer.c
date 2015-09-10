/* We need strdup */
#define _XOPEN_SOURCE 600

#include "tickit.h"

#include <stdio.h>  // vsnprintf
#include <stdlib.h>
#include <string.h>

#include "linechars.inc"

/* must match .pm file */
enum TickitRenderBufferCellState {
  SKIP  = 0,
  TEXT  = 1,
  ERASE = 2,
  CONT  = 3,
  LINE  = 4,
  CHAR  = 5,
};

enum {
  NORTH_SHIFT = 0,
  EAST_SHIFT  = 2,
  SOUTH_SHIFT = 4,
  WEST_SHIFT  = 6,
};

// Internal cell structure definition
typedef struct {
  enum TickitRenderBufferCellState state;
  int cols; // or "startcol" for state == CONT
  int maskdepth; // -1 if not masked
  TickitPen *pen; // state -> {TEXT, ERASE, LINE, CHAR}
  union {
    struct { int idx; int offs; } text; // state == TEXT
    struct { int mask;          } line; // state == LINE
    struct { int codepoint;     } chr;  // state == CHAR
  } v;
} RBCell;

typedef struct RBStack RBStack;
struct RBStack {
  RBStack *prev;

  int vc_line, vc_col;
  int xlate_line, xlate_col;
  TickitRect clip;
  TickitPen *pen;
  unsigned int pen_only : 1;
};

struct TickitRenderBuffer {
  int lines, cols; // Size
  RBCell **cells;

  unsigned int vc_pos_set : 1;
  int vc_line, vc_col;
  int xlate_line, xlate_col;
  TickitRect clip;
  TickitPen *pen;

  int depth;
  RBStack *stack;

  char **texts;
  size_t n_texts;    // number actually valid
  size_t size_texts; // size of allocated buffer

  char *tmp;
  size_t tmplen;  // actually valid
  size_t tmpsize; // allocated size
};

static void free_stack(RBStack *stack)
{
  while(stack) {
    RBStack *prev = stack->prev;
    if(stack->pen)
      tickit_pen_unref(stack->pen);
    free(stack);

    stack = prev;
  }
}

static void free_texts(TickitRenderBuffer *rb)
{
  for(int i = 0; i < rb->n_texts; i++)
    free(rb->texts[i]);

  // Prevent the buffer growing too big
  if(rb->size_texts > 4 && rb->size_texts > rb->n_texts * 2) {
    rb->size_texts /= 2;
    free(rb->texts);
    rb->texts = malloc(rb->size_texts * sizeof(char *));
  }

  rb->n_texts = 0;
}

static int xlate_and_clip(TickitRenderBuffer *rb, int *line, int *col, int *cols, int *startcol)
{
  *line += rb->xlate_line;
  *col  += rb->xlate_col;

  const TickitRect *clip = &rb->clip;

  if(!clip->lines)
    return 0;

  if(*line < clip->top ||
      *line >= tickit_rect_bottom(clip) ||
      *col  >= tickit_rect_right(clip))
    return 0;

  if(startcol)
    *startcol = 0;

  if(*col < clip->left) {
    *cols      -= clip->left - *col;
    if(startcol)
      *startcol += clip->left - *col;
    *col = clip->left;
  }
  if(*cols <= 0)
    return 0;

  if(*cols > tickit_rect_right(clip) - *col)
    *cols = tickit_rect_right(clip) - *col;

  return 1;
}

static void cont_cell(RBCell *cell, int startcol)
{
  switch(cell->state) {
    case TEXT:
    case ERASE:
    case LINE:
    case CHAR:
      tickit_pen_unref(cell->pen);
      break;
    case SKIP:
    case CONT:
      /* ignore */
      break;
  }

  cell->state     = CONT;
  cell->maskdepth = -1;
  cell->cols      = startcol;
  cell->pen       = NULL;
}

static RBCell *make_span(TickitRenderBuffer *rb, int line, int col, int cols)
{
  int end = col + cols;
  RBCell **cells = rb->cells;

  // If the following cell is a CONT, it needs to become a new start
  if(end < rb->cols && cells[line][end].state == CONT) {
    int spanstart = cells[line][end].cols;
    RBCell *spancell = &cells[line][spanstart];
    int spanend = spanstart + spancell->cols;
    int afterlen = spanend - end;
    RBCell *endcell = &cells[line][end];

    switch(spancell->state) {
      case SKIP:
        endcell->state = SKIP;
        endcell->cols  = afterlen;
        break;
      case TEXT:
        endcell->state       = TEXT;
        endcell->cols        = afterlen;
        endcell->pen         = tickit_pen_ref(spancell->pen);
        endcell->v.text.idx  = spancell->v.text.idx;
        endcell->v.text.offs = spancell->v.text.offs + end - spanstart;
        break;
      case ERASE:
        endcell->state = ERASE;
        endcell->cols  = afterlen;
        endcell->pen   = tickit_pen_ref(spancell->pen);
        break;
      case LINE:
      case CHAR:
      case CONT:
        abort();
    }

    // We know these are already CONT cells
    for(int c = end + 1; c < spanend; c++)
      cells[line][c].cols = end;
  }

  // If the initial cell is a CONT, shorten its start
  if(cells[line][col].state == CONT) {
    int beforestart = cells[line][col].cols;
    RBCell *spancell = &cells[line][beforestart];
    int beforelen = col - beforestart;

    switch(spancell->state) {
      case SKIP:
      case TEXT:
      case ERASE:
        spancell->cols = beforelen;
        break;
      case LINE:
      case CHAR:
      case CONT:
        abort();
    }
  }

  // cont_cell() also frees any pens in the range
  for(int c = col; c < end; c++)
    cont_cell(&cells[line][c], col);

  cells[line][col].cols = cols;

  return &cells[line][col];
}

static void tmp_cat_utf8(TickitRenderBuffer *rb, long codepoint)
{
  int seqlen = tickit_string_seqlen(codepoint);
  if(rb->tmpsize < rb->tmplen + seqlen) {
    rb->tmpsize *= 2;
    rb->tmp = realloc(rb->tmp, rb->tmpsize);
  }

  tickit_string_putchar(rb->tmp + rb->tmplen, rb->tmpsize - rb->tmplen, codepoint);
  rb->tmplen += seqlen;

  /* rb->tmp remains NOT nul-terminated */
}

static void tmp_alloc(TickitRenderBuffer *rb, size_t len)
{
  if(rb->tmpsize < len) {
    free(rb->tmp);

    while(rb->tmpsize < len)
      rb->tmpsize *= 2;
    rb->tmp = malloc(rb->tmpsize);
  }
}

static int put_text(TickitRenderBuffer *rb, int line, int col, const char *text, size_t len)
{
  TickitStringPos endpos;
  len = tickit_string_ncount(text, len, &endpos, NULL);
  if(1 + len == 0)
    return -1;

  int cols = endpos.columns;
  int ret = cols;

  int startcol;
  if(!xlate_and_clip(rb, &line, &col, &cols, &startcol))
    return ret;

  if(rb->n_texts == rb->size_texts) {
    rb->size_texts *= 2;
    rb->texts = realloc(rb->texts, rb->size_texts * sizeof(char *));
  }

  rb->texts[rb->n_texts] = malloc(len + 1);
  memcpy(rb->texts[rb->n_texts], text, len);
  rb->texts[rb->n_texts][len] = '\0';

  RBCell *linecells = rb->cells[line];

  while(cols) {
    while(cols && linecells[col].maskdepth > -1) {
      col++;
      cols--;
      startcol++;
    }
    if(!cols)
      break;

    int spanlen = 0;
    while(cols && linecells[col + spanlen].maskdepth == -1) {
      spanlen++;
      cols--;
    }
    if(!spanlen)
      break;

    RBCell *cell = make_span(rb, line, col, spanlen);
    cell->state       = TEXT;
    cell->pen         = tickit_pen_ref(rb->pen);
    cell->v.text.idx  = rb->n_texts;
    cell->v.text.offs = startcol;

    col      += spanlen;
    startcol += spanlen;
  }

  rb->n_texts++;

  return ret;
}

static void put_char(TickitRenderBuffer *rb, int line, int col, long codepoint)
{
  int cols = 1;

  if(!xlate_and_clip(rb, &line, &col, &cols, NULL))
    return;

  if(rb->cells[line][col].maskdepth > -1)
    return;

  RBCell *cell = make_span(rb, line, col, cols);
  cell->state           = CHAR;
  cell->pen             = tickit_pen_ref(rb->pen);
  cell->v.chr.codepoint = codepoint;
}

static void erase(TickitRenderBuffer *rb, int line, int col, int cols)
{
  if(!xlate_and_clip(rb, &line, &col, &cols, NULL))
    return;

  RBCell *linecells = rb->cells[line];

  while(cols) {
    while(cols && linecells[col].maskdepth > -1) {
      col++;
      cols--;
    }
    if(!cols)
      break;

    int spanlen = 0;
    while(cols && linecells[col + spanlen].maskdepth == -1) {
      spanlen++;
      cols--;
    }
    if(!spanlen)
      break;

    RBCell *cell = make_span(rb, line, col, spanlen);
    cell->state = ERASE;
    cell->pen   = tickit_pen_ref(rb->pen);

    col += spanlen;
  }
}

TickitRenderBuffer *tickit_renderbuffer_new(int lines, int cols)
{
  TickitRenderBuffer *rb = malloc(sizeof(TickitRenderBuffer));

  rb->lines = lines;
  rb->cols  = cols;

  rb->cells = malloc(rb->lines * sizeof(RBCell *));
  for(int line = 0; line < rb->lines; line++) {
    rb->cells[line] = malloc(rb->cols * sizeof(RBCell));

    rb->cells[line][0].state     = SKIP;
    rb->cells[line][0].maskdepth = -1;
    rb->cells[line][0].cols      = rb->cols;
    rb->cells[line][0].pen       = NULL;

    for(int col = 1; col < rb->cols; col++) {
      rb->cells[line][col].state     = CONT;
      rb->cells[line][col].maskdepth = -1;
      rb->cells[line][col].cols      = 0;
    }
  }

  rb->vc_pos_set = 0;

  rb->xlate_line = 0;
  rb->xlate_col  = 0;

  tickit_rect_init_sized(&rb->clip, 0, 0, rb->lines, rb->cols);

  rb->pen = tickit_pen_new();

  rb->stack = NULL;
  rb->depth = 0;

  rb->n_texts = 0;
  rb->size_texts = 4;
  rb->texts = malloc(rb->size_texts * sizeof(char *));

  rb->tmpsize = 256; // hopefully enough but will grow if required
  rb->tmp = malloc(rb->tmpsize);
  rb->tmplen = 0;

  return rb;
}

void tickit_renderbuffer_destroy(TickitRenderBuffer *rb)
{
  for(int line = 0; line < rb->lines; line++) {
    for(int col = 0; col < rb->cols; col++) {
      RBCell *cell = &rb->cells[line][col];
      switch(cell->state) {
        case TEXT:
        case ERASE:
        case LINE:
        case CHAR:
          tickit_pen_unref(cell->pen);
          break;
        case SKIP:
        case CONT:
          break;
      }
    }
    free(rb->cells[line]);
  }

  free(rb->cells);
  rb->cells = NULL;

  tickit_pen_unref(rb->pen);

  if(rb->stack)
    free_stack(rb->stack);

  free_texts(rb);
  free(rb->texts);

  free(rb->tmp);

  free(rb);
}

void tickit_renderbuffer_get_size(const TickitRenderBuffer *rb, int *lines, int *cols)
{
  if(lines)
    *lines = rb->lines;

  if(cols)
    *cols = rb->cols;
}

void tickit_renderbuffer_translate(TickitRenderBuffer *rb, int downward, int rightward)
{
  rb->xlate_line += downward;
  rb->xlate_col  += rightward;
}

void tickit_renderbuffer_clip(TickitRenderBuffer *rb, TickitRect *rect)
{
  TickitRect other;

  other = *rect;
  other.top  += rb->xlate_line;
  other.left += rb->xlate_col;

  if(!tickit_rect_intersect(&rb->clip, &rb->clip, &other))
    rb->clip.lines = 0;
}

void tickit_renderbuffer_mask(TickitRenderBuffer *rb, TickitRect *mask)
{
  TickitRect hole;

  hole = *mask;
  hole.top  += rb->xlate_line;
  hole.left += rb->xlate_col;

  if(hole.top < 0) {
    hole.lines += hole.top;
    hole.top = 0;
  }
  if(hole.left < 0) {
    hole.cols += hole.left;
    hole.left = 0;
  }

  for(int line = hole.top; line < hole.top + hole.lines && line < rb->lines; line++) {
    for(int col = hole.left; col < hole.left + hole.cols && col < rb->cols; col++) {
      RBCell *cell = &rb->cells[line][col];
      if(cell->maskdepth == -1)
        cell->maskdepth = rb->depth;
    }
  }
}

bool tickit_renderbuffer_has_cursorpos(const TickitRenderBuffer *rb)
{
  return rb->vc_pos_set;
}

void tickit_renderbuffer_get_cursorpos(const TickitRenderBuffer *rb, int *line, int *col)
{
  if(rb->vc_pos_set && line)
    *line = rb->vc_line;
  if(rb->vc_pos_set && col)
    *col = rb->vc_col;
}

void tickit_renderbuffer_goto(TickitRenderBuffer *rb, int line, int col)
{
  rb->vc_pos_set = 1;
  rb->vc_line = line;
  rb->vc_col  = col;
}

void tickit_renderbuffer_ungoto(TickitRenderBuffer *rb)
{
  rb->vc_pos_set = 0;
}

void tickit_renderbuffer_setpen(TickitRenderBuffer *rb, const TickitPen *pen)
{
  TickitPen *prevpen = rb->stack ? rb->stack->pen : NULL;

  /* never mutate the pen inplace; make a new one */
  TickitPen *newpen = tickit_pen_new();

  if(pen)
    tickit_pen_copy(newpen, pen, 1);
  if(prevpen)
    tickit_pen_copy(newpen, prevpen, 0);

  tickit_pen_unref(rb->pen);
  rb->pen = newpen;
}

void tickit_renderbuffer_reset(TickitRenderBuffer *rb)
{
  for(int line = 0; line < rb->lines; line++) {
    // cont_cell also frees pen
    for(int col = 0; col < rb->cols; col++)
      cont_cell(&rb->cells[line][col], 0);

    rb->cells[line][0].state     = SKIP;
    rb->cells[line][0].maskdepth = -1;
    rb->cells[line][0].cols      = rb->cols;
  }

  rb->vc_pos_set = 0;

  rb->xlate_line = 0;
  rb->xlate_col  = 0;

  tickit_rect_init_sized(&rb->clip, 0, 0, rb->lines, rb->cols);

  tickit_pen_unref(rb->pen);
  rb->pen = tickit_pen_new();

  if(rb->stack) {
    free_stack(rb->stack);
    rb->stack = NULL;
    rb->depth = 0;
  }

  free_texts(rb);
}

void tickit_renderbuffer_clear(TickitRenderBuffer *rb)
{
  for(int line = 0; line < rb->lines; line++)
    erase(rb, line, 0, rb->cols);
}

void tickit_renderbuffer_save(TickitRenderBuffer *rb)
{
  RBStack *stack = malloc(sizeof(struct RBStack));

  stack->vc_line    = rb->vc_line;
  stack->vc_col     = rb->vc_col;
  stack->xlate_line = rb->xlate_line;
  stack->xlate_col  = rb->xlate_col;
  stack->clip       = rb->clip;
  stack->pen        = tickit_pen_ref(rb->pen);
  stack->pen_only   = 0;

  stack->prev = rb->stack;
  rb->stack = stack;
  rb->depth++;
}

void tickit_renderbuffer_savepen(TickitRenderBuffer *rb)
{
  RBStack *stack = malloc(sizeof(struct RBStack));

  stack->pen      = tickit_pen_ref(rb->pen);
  stack->pen_only = 1;

  stack->prev = rb->stack;
  rb->stack = stack;
  rb->depth++;
}

void tickit_renderbuffer_restore(TickitRenderBuffer *rb)
{
  RBStack *stack;

  if(!rb->stack)
    return;

  stack = rb->stack;
  rb->stack = stack->prev;

  if(!stack->pen_only) {
    rb->vc_line    = stack->vc_line;
    rb->vc_col     = stack->vc_col;
    rb->xlate_line = stack->xlate_line;
    rb->xlate_col  = stack->xlate_col;
    rb->clip       = stack->clip;
  }

  tickit_pen_unref(rb->pen);
  rb->pen = stack->pen;
  // We've now definitely taken ownership of the old stack frame's pen, so
  //   it doesn't need destroying now

  rb->depth--;

  // TODO: this could be done more efficiently by remembering the edges of masking
  for(int line = 0; line < rb->lines; line++)
    for(int col = 0; col < rb->cols; col++)
      if(rb->cells[line][col].maskdepth > rb->depth)
        rb->cells[line][col].maskdepth = -1;

  free(stack);
}

void tickit_renderbuffer_skip_at(TickitRenderBuffer *rb, int line, int col, int cols)
{
  if(!xlate_and_clip(rb, &line, &col, &cols, NULL))
    return;

  RBCell *linecells = rb->cells[line];

  while(cols) {
    while(cols && linecells[col].maskdepth > -1) {
      col++;
      cols--;
    }
    if(!cols)
      break;

    int spanlen = 0;
    while(cols && linecells[col + spanlen].maskdepth == -1) {
      spanlen++;
      cols--;
    }
    if(!spanlen)
      break;

    RBCell *cell = make_span(rb, line, col, spanlen);
    cell->state = SKIP;

    col += spanlen;
  }
}

void tickit_renderbuffer_skip(TickitRenderBuffer *rb, int cols)
{
  if(!rb->vc_pos_set)
    return;

  tickit_renderbuffer_skip_at(rb, rb->vc_line, rb->vc_col, cols);
  rb->vc_col += cols;
}

void tickit_renderbuffer_skip_to(TickitRenderBuffer *rb, int col)
{
  if(!rb->vc_pos_set)
    return;

  if(rb->vc_col < col)
    tickit_renderbuffer_skip_at(rb, rb->vc_line, rb->vc_col, col - rb->vc_col);

  rb->vc_col = col;
}

int tickit_renderbuffer_text_at(TickitRenderBuffer *rb, int line, int col, const char *text)
{
  return put_text(rb, line, col, text, -1);
}

int tickit_renderbuffer_textn_at(TickitRenderBuffer *rb, int line, int col, const char *text, size_t len)
{
  return put_text(rb, line, col, text, len);
}

int tickit_renderbuffer_text(TickitRenderBuffer *rb, const char *text)
{
  if(!rb->vc_pos_set)
    return -1;

  int cols = tickit_renderbuffer_text_at(rb, rb->vc_line, rb->vc_col, text);
  rb->vc_col += cols;

  return cols;
}

int tickit_renderbuffer_textn(TickitRenderBuffer *rb, const char *text, size_t len)
{
  if(!rb->vc_pos_set)
    return -1;

  int cols = put_text(rb, rb->vc_line, rb->vc_col, text, len);
  rb->vc_col += cols;

  return cols;
}

int tickit_renderbuffer_textf_at(TickitRenderBuffer *rb, int line, int col, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int ret = tickit_renderbuffer_vtextf_at(rb, line, col, fmt, args);

  va_end(args);

  return ret;
}

int tickit_renderbuffer_vtextf_at(TickitRenderBuffer *rb, int line, int col, const char *fmt, va_list args)
{
  /* It's likely the string will fit in, say, 64 bytes */
  char buffer[64];
  size_t len;
  {
    va_list args_for_size;
    va_copy(args_for_size, args);

    len = vsnprintf(buffer, sizeof buffer, fmt, args_for_size);

    va_end(args_for_size);
  }

  if(len < sizeof buffer)
    return put_text(rb, line, col, buffer, len);

  tmp_alloc(rb, len + 1);
  vsnprintf(rb->tmp, rb->tmpsize, fmt, args);
  return put_text(rb, line, col, rb->tmp, len);
}

int tickit_renderbuffer_textf(TickitRenderBuffer *rb, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int ret = tickit_renderbuffer_vtextf(rb, fmt, args);

  va_end(args);

  return ret;
}

int tickit_renderbuffer_vtextf(TickitRenderBuffer *rb, const char *fmt, va_list args)
{
  if(!rb->vc_pos_set)
    return -1;

  int cols = tickit_renderbuffer_vtextf_at(rb, rb->vc_line, rb->vc_col, fmt, args);
  rb->vc_col += cols;

  return cols;
}

void tickit_renderbuffer_erase_at(TickitRenderBuffer *rb, int line, int col, int cols)
{
  erase(rb, line, col, cols);
}

void tickit_renderbuffer_erase(TickitRenderBuffer *rb, int cols)
{
  if(!rb->vc_pos_set)
    return;

  erase(rb, rb->vc_line, rb->vc_col, cols);
  rb->vc_col += cols;
}

void tickit_renderbuffer_erase_to(TickitRenderBuffer *rb, int col)
{
  if(!rb->vc_pos_set)
    return;

  if(rb->vc_col < col)
    erase(rb, rb->vc_line, rb->vc_col, col - rb->vc_col);

  rb->vc_col = col;
}

void tickit_renderbuffer_eraserect(TickitRenderBuffer *rb, TickitRect *rect)
{
  for(int line = rect->top; line < tickit_rect_bottom(rect); line++)
    erase(rb, line, rect->left, rect->cols);
}

void tickit_renderbuffer_char_at(TickitRenderBuffer *rb, int line, int col, long codepoint)
{
  put_char(rb, line, col, codepoint);
}

void tickit_renderbuffer_char(TickitRenderBuffer *rb, long codepoint)
{
  if(!rb->vc_pos_set)
    return;

  put_char(rb, rb->vc_line, rb->vc_col, codepoint);
  // TODO: might not be 1; would have to look it up
  rb->vc_col += 1;
}

static void linecell(TickitRenderBuffer *rb, int line, int col, int bits)
{
  int cols = 1;

  if(!xlate_and_clip(rb, &line, &col, &cols, NULL))
    return;

  if(rb->cells[line][col].maskdepth > -1)
    return;

  RBCell *cell = &rb->cells[line][col];
  if(cell->state != LINE) {
    make_span(rb, line, col, cols);
    cell->state       = LINE;
    cell->cols        = 1;
    cell->pen         = tickit_pen_ref(rb->pen);
    cell->v.line.mask = 0;
  }
  else if(!tickit_pen_equiv(cell->pen, rb->pen)) {
    tickit_pen_unref(cell->pen);
    cell->pen   = tickit_pen_ref(rb->pen);
  }

  cell->v.line.mask |= bits;
}

void tickit_renderbuffer_hline_at(TickitRenderBuffer *rb, int line, int startcol, int endcol,
    TickitLineStyle style, TickitLineCaps caps)
{
  int east = style << EAST_SHIFT;
  int west = style << WEST_SHIFT;

  linecell(rb, line, startcol, east | (caps & TICKIT_LINECAP_START ? west : 0));
  for(int col = startcol + 1; col <= endcol - 1; col++)
    linecell(rb, line, col, east | west);
  linecell(rb, line, endcol, (caps & TICKIT_LINECAP_END ? east : 0) | west);
}

void tickit_renderbuffer_vline_at(TickitRenderBuffer *rb, int startline, int endline, int col,
    TickitLineStyle style, TickitLineCaps caps)
{
  int north = style << NORTH_SHIFT;
  int south = style << SOUTH_SHIFT;

  linecell(rb, startline, col, south | (caps & TICKIT_LINECAP_START ? north : 0));
  for(int line = startline + 1; line <= endline - 1; line++)
    linecell(rb, line, col, south | north);
  linecell(rb, endline, col, (caps & TICKIT_LINECAP_END ? south : 0) | north);
}

void tickit_renderbuffer_flush_to_term(TickitRenderBuffer *rb, TickitTerm *tt)
{
  for(int line = 0; line < rb->lines; line++) {
    int phycol = -1; /* column where the terminal cursor physically is */

    for(int col = 0; col < rb->cols; /**/) {
      RBCell *cell = &rb->cells[line][col];

      if(cell->state == SKIP) {
        col += cell->cols;
        continue;
      }

      if(phycol < col)
        tickit_term_goto(tt, line, col);
      phycol = col;

      switch(cell->state) {
        case TEXT:
          {
            TickitStringPos start, end, limit;
            char *text = rb->texts[cell->v.text.idx];

            tickit_stringpos_limit_columns(&limit, cell->v.text.offs);
            tickit_string_count(text, &start, &limit);

            limit.columns += cell->cols;
            end = start;
            tickit_string_countmore(text, &end, &limit);

            tickit_term_setpen(tt, cell->pen);
            tickit_term_printn(tt, text + start.bytes, end.bytes - start.bytes);

            phycol += cell->cols;
          }
          break;
        case ERASE:
          {
            /* No need to set moveend=true to erasech unless we actually
             * have more content */
            int moveend = col + cell->cols < rb->cols &&
                          rb->cells[line][col + cell->cols].state != SKIP;

            tickit_term_setpen(tt, cell->pen);
            tickit_term_erasech(tt, cell->cols, moveend ? TICKIT_YES : TICKIT_MAYBE);

            if(moveend)
              phycol += cell->cols;
            else
              phycol = -1;
          }
          break;
        case LINE:
          {
            TickitPen *pen = cell->pen;

            do {
              tmp_cat_utf8(rb, linemask_to_char[cell->v.line.mask]);

              col++;
              phycol += cell->cols;
            } while(col < rb->cols &&
                    (cell = &rb->cells[line][col]) &&
                    cell->state == LINE &&
                    tickit_pen_equiv(cell->pen, pen));

            tickit_term_setpen(tt, pen);
            tickit_term_printn(tt, rb->tmp, rb->tmplen);
            rb->tmplen = 0;
          }
          continue; /* col already updated */
        case CHAR:
          {
            tmp_cat_utf8(rb, cell->v.chr.codepoint);

            tickit_term_setpen(tt, cell->pen);
            tickit_term_printn(tt, rb->tmp, rb->tmplen);
            rb->tmplen = 0;

            phycol += cell->cols;
          }
          break;
        case SKIP:
        case CONT:
          /* unreachable */
          abort();
      }

      col += cell->cols;
    }
  }

  tickit_renderbuffer_reset(rb);
}

void tickit_renderbuffer_blit(TickitRenderBuffer *dst, TickitRenderBuffer *src)
{
  for(int line = 0; line < src->lines; line++) {
    for(int col = 0; col < src->cols; /**/) {
      RBCell *cell = &src->cells[line][col];

      if(cell->state != SKIP) {
        tickit_renderbuffer_savepen(dst);
        tickit_renderbuffer_setpen(dst, cell->pen);
      }

      switch(cell->state) {
        case SKIP:
          break;
        case TEXT:
          {
            TickitStringPos start, end, limit;
            char *text = src->texts[cell->v.text.idx];

            tickit_stringpos_limit_columns(&limit, cell->v.text.offs);
            tickit_string_count(text, &start, &limit);

            limit.columns += cell->cols;
            end = start;
            tickit_string_countmore(text, &end, &limit);

            put_text(dst, line, col, text + start.bytes, end.bytes - start.bytes);
          }
          break;
        case ERASE:
          erase(dst, line, col, cell->cols);
          break;
        case LINE:
          linecell(dst, line, col, cell->v.line.mask);
          break;
        case CHAR:
          put_char(dst, line, col, cell->v.chr.codepoint);
          break;
        case CONT:
          /* unreachable */
          abort();
      }

      if(cell->state != SKIP)
        tickit_renderbuffer_restore(dst);

      col += cell->cols;
    }
  }
}

static RBCell *get_span(TickitRenderBuffer *rb, int line, int col, int *offset)
{
  int cols = 1;
  if(!xlate_and_clip(rb, &line, &col, &cols, NULL))
    return NULL;

  *offset = 0;
  RBCell *cell = &rb->cells[line][col];
  if(cell->state == CONT) {
    *offset = col - cell->cols; // startcol
    cell = &rb->cells[line][cell->cols];
  }

  return cell;
}

static size_t get_span_text(TickitRenderBuffer *rb, RBCell *span, int offset, int one_grapheme, char *buffer, size_t len)
{
  size_t bytes;

  switch(span->state) {
    case CONT: // should be unreachable
      return -1;

    case SKIP:
    case ERASE:
      bytes = 0;
      break;

    case TEXT:
      {
        char *text = rb->texts[span->v.text.idx];
        TickitStringPos start, end, limit;

        tickit_stringpos_limit_columns(&limit, span->v.text.offs + offset);
        tickit_string_count(text, &start, &limit);

        if(one_grapheme)
          tickit_stringpos_limit_graphemes(&limit, start.graphemes + 1);
        else
          tickit_stringpos_limit_columns(&limit, span->cols);
        end = start;
        tickit_string_countmore(text, &end, &limit);

        bytes = end.bytes - start.bytes;

        if(buffer) {
          if(len < bytes)
            return -1;
          strncpy(buffer, text + start.bytes, bytes);
        }
        break;
      }
    case LINE:
      bytes = tickit_string_putchar(buffer, len, linemask_to_char[span->v.line.mask]);
      break;

    case CHAR:
      bytes = tickit_string_putchar(buffer, len, span->v.chr.codepoint);
      break;
  }

  if(buffer && len > bytes)
    buffer[bytes] = 0;

  return bytes;
}

int tickit_renderbuffer_get_cell_active(TickitRenderBuffer *rb, int line, int col)
{
  int offset;
  RBCell *span = get_span(rb, line, col, &offset);
  if(!span)
    return -1;

  return span->state != SKIP;
}

size_t tickit_renderbuffer_get_cell_text(TickitRenderBuffer *rb, int line, int col, char *buffer, size_t len)
{
  int offset;
  RBCell *span = get_span(rb, line, col, &offset);
  if(!span || span->state == CONT)
    return -1;

  return get_span_text(rb, span, offset, 1, buffer, len);
}

TickitRenderBufferLineMask tickit_renderbuffer_get_cell_linemask(TickitRenderBuffer *rb, int line, int col)
{
  int offset;
  RBCell *span = get_span(rb, line, col, &offset);
  if(!span || span->state != LINE)
    return (TickitRenderBufferLineMask){ 0 };

  return (TickitRenderBufferLineMask){
    .north = (span->v.line.mask >> NORTH_SHIFT) & 0x03,
    .south = (span->v.line.mask >> SOUTH_SHIFT) & 0x03,
    .east  = (span->v.line.mask >> EAST_SHIFT ) & 0x03,
    .west  = (span->v.line.mask >> WEST_SHIFT ) & 0x03,
  };
}

TickitPen *tickit_renderbuffer_get_cell_pen(TickitRenderBuffer *rb, int line, int col)
{
  int offset;
  RBCell *span = get_span(rb, line, col, &offset);
  if(!span || span->state == SKIP)
    return NULL;

  return span->pen;
}

size_t tickit_renderbuffer_get_span(TickitRenderBuffer *rb, int line, int startcol, struct TickitRenderBufferSpanInfo *info, char *text, size_t len)
{
  int offset;
  RBCell *span = get_span(rb, line, startcol, &offset);
  if(!span || span->state == CONT)
    return -1;

  if(info)
    info->n_columns = span->cols - offset;

  if(span->state == SKIP) {
    if(info)
      info->is_active = 0;
    return 0;
  }

  if(info)
    info->is_active = 1;

  if(info && info->pen) {
    tickit_pen_clear(info->pen);
    tickit_pen_copy(info->pen, span->pen, 1);
  }

  size_t retlen = get_span_text(rb, span, offset, 0, text, len);
  if(info) {
    info->len = retlen;
    info->text = text;
  }
  return len;
}
