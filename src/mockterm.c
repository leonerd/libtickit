#define _XOPEN_SOURCE 700

#include "tickit.h"

#include "tickit-mockterm.h"
#include "tickit-termdrv.h"

#include <stdlib.h>
#include <string.h>

#define BOUND(var,min,max) \
  if(var < (min)) var = (min); \
  if(var > (max)) var = (max)

typedef struct
{
  char      *str;
  TickitPen *pen;
} MockTermCell;

typedef struct
{
  TickitTermDriver super;

  int lines;
  int cols;
  MockTermCell ***cells;

  TickitMockTermLogEntry *log;
  size_t            logsize;
  size_t            logi;

  TickitPen *pen;
  int line;
  int col;
  int cursorvis;
  int cursorshape;
} MockTermDriver;

static void mtd_free_cell(MockTermDriver *mtd, int line, int col)
{
  MockTermCell *cell = mtd->cells[line][col];

  if(cell->str)
    free(cell->str);
  if(cell->pen)
    tickit_pen_unref(cell->pen);

  free(cell);
}

static void mtd_free_line(MockTermDriver *mtd, int line)
{
  for(int col = 0; col < mtd->cols; col++)
    mtd_free_cell(mtd, line, col);

  free(mtd->cells[line]);
}

static void mtd_clear_cells(MockTermDriver *mtd, int line, int startcol, int stopcol)
{
  /* This code is also used to initialise brand new cells in the structure, so
   * it should be careful to vivify them correctly
   */

  MockTermCell **linecells = mtd->cells[line];
  if(!linecells) {
    linecells = malloc(mtd->cols * sizeof(MockTermCell *));
    mtd->cells[line] = linecells;

    for(int col = 0; col < mtd->cols; col++)
      linecells[col] = NULL;
  }

  for(int col = startcol; col < stopcol; col++) {
    MockTermCell *cell = linecells[col];

    if(!cell) {
      cell = malloc(sizeof(MockTermCell));
      linecells[col] = cell;

      cell->str = NULL;
      cell->pen = NULL;
    }

    if(cell->str)
      free(cell->str);
    if(cell->pen)
      tickit_pen_unref(cell->pen);

    cell->str = strdup(" ");
    cell->pen = tickit_pen_clone(mtd->pen);
  }
}

static TickitMockTermLogEntry *mtd_nextlog(MockTermDriver *mtd)
{
  if(mtd->logi == mtd->logsize) {
    mtd->logsize *= 2;
    mtd->log = realloc(mtd->log, mtd->logsize * sizeof(TickitMockTermLogEntry));
  }

  TickitMockTermLogEntry *entry = mtd->log + mtd->logi++;

  entry->str = NULL;
  entry->pen = NULL;
  return entry;
}

static void mtd_free_logentry(TickitMockTermLogEntry *entry)
{
  if(entry->str)
    free((void *)entry->str);
  entry->str = NULL;

  if(entry->pen)
    tickit_pen_unref(entry->pen);
  entry->pen = NULL;
}

static void mtd_destroy(TickitTermDriver *ttd)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  for(int i = 0; i < mtd->logi; i++)
    mtd_free_logentry(mtd->log + i);
  free(mtd->log);

  for(int line = 0; line < mtd->lines; line++)
    mtd_free_line(mtd, line);
  free(mtd->cells);

  tickit_pen_unref(mtd->pen);

  free(mtd);
}

static bool mtd_print(TickitTermDriver *ttd, const char *str, size_t len)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
  entry->type = LOG_PRINT;
  entry->str  = strndup(str, len);
  entry->val1 = len;

  TickitStringPos pos;
  tickit_stringpos_zero(&pos);
  pos.columns = mtd->col;

  MockTermCell **linecells = mtd->cells[mtd->line];

  TickitStringPos limit;
  tickit_stringpos_limit_columns(&limit, pos.columns);
  limit.bytes = len;

  while(pos.bytes < len) {
    TickitStringPos start = pos;

    limit.columns++;
    tickit_utf8_ncountmore(str, len, &pos, &limit);

    if(pos.columns == start.columns)
      continue;

    // Wrap but don't scroll - for now. This shouldn't cause scrolling anyway
    if(start.columns >= mtd->cols) {
      start.columns = 0;
      if(mtd->line < mtd->lines-1) {
        mtd->line++;
        linecells = mtd->cells[mtd->line];
      }
    }

    MockTermCell *cell = linecells[start.columns];

    if(cell->str)
      free(cell->str);
    if(cell->pen)
      tickit_pen_unref(cell->pen);

    cell->str = strndup(str + start.bytes, pos.bytes - start.bytes);
    cell->pen = tickit_pen_clone(mtd->pen);

    // Empty out the other cells for doublewidth
    for(start.columns++; start.columns < pos.columns; start.columns++) {
      cell = linecells[start.columns];

      if(cell->str)
        free(cell->str);
      if(cell->pen)
        tickit_pen_unref(cell->pen);

      cell->str = NULL; /* empty */
      cell->pen = tickit_pen_clone(mtd->pen);
    }
  }

  mtd->col = pos.columns;

  return true;
}

static bool mtd_goto_abs(TickitTermDriver *ttd, int line, int col)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  BOUND(line, 0, mtd->lines-1);
  BOUND(col,  0, mtd->cols-1);

  TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
  entry->type = LOG_GOTO;
  entry->val1 = line;
  entry->val2 = col;

  mtd->line = line;
  mtd->col  = col;

  return true;
}

static bool mtd_move_rel(TickitTermDriver *ttd, int downward, int rightward)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  mtd_goto_abs(ttd, mtd->line + downward, mtd->col + rightward);

  return true;
}

static bool mtd_scrollrect(TickitTermDriver *ttd, const TickitRect *rect, int downward, int rightward)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  if(!downward && !rightward)
    return true;

  int top    = rect->top;
  int left   = rect->left;
  int bottom = tickit_rect_bottom(rect);
  int right  = tickit_rect_right(rect);

  BOUND(top,    0,   mtd->lines-1);
  BOUND(bottom, top, mtd->lines);
  BOUND(left,  0,    mtd->cols-1);
  BOUND(right, left, mtd->cols);

  if((abs(downward) >= (bottom - top)) || (abs(rightward) >= (right - left)))
    return false;

  if(left == 0 && right == mtd->cols && rightward == 0) {
    MockTermCell ***cells = mtd->cells;

    TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
    entry->type = LOG_SCROLLRECT;
    entry->val1 = downward;
    entry->val2 = rightward;
    entry->rect = *rect;

    if(downward > 0) {
      int line;
      for(line = top; line < top + downward; line++)
        mtd_free_line(mtd, line);

      for(line = top; line < bottom - downward; line++)
        cells[line] = cells[line + downward];

      for(/* line */; line < bottom; line++) {
        cells[line] = NULL;
        mtd_clear_cells(mtd, line, 0, mtd->cols);
      }
    }
    else {
      int upward = -downward;

      int line;
      for(line = bottom-1; line >= bottom - upward; line--)
        mtd_free_line(mtd, line);

      for(line = bottom-1; line >= top + upward; line--)
        cells[line] = cells[line - upward];

      for(/* line */;    line >= top; line--) {
        cells[line] = NULL;
        mtd_clear_cells(mtd, line, 0, mtd->cols);
      }
    }

    return true;
  }

  if(right == mtd->cols && downward == 0) {
    TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
    entry->type = LOG_SCROLLRECT;
    entry->val1 = downward;
    entry->val2 = rightward;
    entry->rect = *rect;

    for(int line = top; line < bottom; line++) {
      MockTermCell **linecells = mtd->cells[line];

      if(rightward > 0) {
        int col;
        for(col = left; col < left + rightward; col++)
          mtd_free_cell(mtd, line, col);

        for(col = left; col < right - rightward; col++)
          linecells[col] = linecells[col + rightward];

        for(/* col */; col < right; col++)
          linecells[col] = NULL;
        mtd_clear_cells(mtd, line, right - rightward, right);
      }
      else {
        int leftward = -rightward;

        int col;
        for(col = right-1; col >= right - leftward; col--)
          mtd_free_cell(mtd, line, col);

        for(col = right-1; col >= left + leftward; col--)
          linecells[col] = linecells[col - leftward];

        for(/* col */;    col >= left; col--)
          linecells[col] = NULL;
        mtd_clear_cells(mtd, line, left, left + leftward);
      }
    }

    return true;
  }

  return false;
}

static bool mtd_erasech(TickitTermDriver *ttd, int count, TickitMaybeBool moveend)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
  entry->type = LOG_ERASECH;
  entry->val1 = count;
  entry->val2 = moveend;

  int right = mtd->col + count;
  BOUND(right, 0, mtd->cols);

  mtd_clear_cells(mtd, mtd->line, mtd->col, right);

  if(moveend != TICKIT_NO)
    mtd->col = right;

  return true;
}

static bool mtd_clear(TickitTermDriver *ttd)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
  entry->type = LOG_CLEAR;

  for(int line = 0; line < mtd->lines; line++)
    mtd_clear_cells(mtd, line, 0, mtd->cols);

  return true;
}

static bool mtd_chpen(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  TickitMockTermLogEntry *entry = mtd_nextlog(mtd);
  entry->type = LOG_SETPEN;
  entry->pen  = tickit_pen_clone(final);

  tickit_pen_clear(mtd->pen);
  tickit_pen_copy(mtd->pen, final, 1);

  return true;
}

static bool mtd_getctl_int(TickitTermDriver *ttd, TickitTermCtl ctl, int *value)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  switch(ctl) {
    case TICKIT_TERMCTL_CURSORVIS:
      *value = mtd->cursorvis;
      return true;
    case TICKIT_TERMCTL_CURSORSHAPE:
      *value = mtd->cursorshape;
      return true;
    case TICKIT_TERMCTL_COLORS:
      *value = 256;
      return true;

    default:
      return false;
  }
}

static bool mtd_setctl_int(TickitTermDriver *ttd, TickitTermCtl ctl, int value)
{
  MockTermDriver *mtd = (MockTermDriver *)ttd;

  switch(ctl) {
    case TICKIT_TERMCTL_CURSORVIS:
      mtd->cursorvis = !!value; break;
    case TICKIT_TERMCTL_CURSORSHAPE:
      mtd->cursorshape = value; break;
    case TICKIT_TERMCTL_ALTSCREEN:
    case TICKIT_TERMCTL_MOUSE:
      break;
    default:
      return false;
  }

  return true;
}

static TickitTermDriverVTable mtd_vtable = {
  .destroy    = mtd_destroy,
  .print      = mtd_print,
  .goto_abs   = mtd_goto_abs,
  .move_rel   = mtd_move_rel,
  .scrollrect = mtd_scrollrect,
  .erasech    = mtd_erasech,
  .clear      = mtd_clear,
  .chpen      = mtd_chpen,
  .getctl_int = mtd_getctl_int,
  .setctl_int = mtd_setctl_int,
};

TickitMockTerm *tickit_mockterm_new(int lines, int cols)
{
  MockTermDriver *mtd = malloc(sizeof(MockTermDriver));
  mtd->super.vtable = &mtd_vtable;

  mtd->logsize = 16; // should be sufficient; or it will grow
  mtd->log = malloc(mtd->logsize * sizeof(TickitMockTermLogEntry));
  mtd->logi = 0;

  mtd->pen       = tickit_pen_new();

  mtd->lines       = lines;
  mtd->cols        = cols;
  mtd->line        = -1;
  mtd->col         = -1;
  mtd->cursorvis   = 0;
  mtd->cursorshape = 0;

  mtd->cells = malloc(lines * sizeof(MockTermCell **));
  for(int line = 0; line < lines; line++) {
    mtd->cells[line] = NULL;
    mtd_clear_cells(mtd, line, 0, cols);
  }

  TickitMockTerm *mt = (TickitMockTerm *)tickit_term_new_for_driver(&mtd->super);
  if(!mt) {
    mtd_destroy((TickitTermDriver *)mtd);
    return NULL;
  }

  tickit_term_set_size((TickitTerm *)mt, lines, cols);

  return mt;
}

void tickit_mockterm_destroy(TickitMockTerm *mt)
{
  tickit_term_destroy((TickitTerm *)mt);
}

size_t tickit_mockterm_get_display_text(TickitMockTerm *mt, char *buffer, size_t len, int line, int col, int width)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  MockTermCell **linecells = mtd->cells[line];

  size_t ret = 0;
  for(/* col */; width; col++, width--) {
    MockTermCell *cell = linecells[col];
    size_t celllen = cell->str ? strlen(cell->str) : 0;

    if(buffer && celllen && len >= celllen) {
      strcpy(buffer, cell->str);
      buffer += celllen;
      len    -= celllen;
      if(len <= 0)
        buffer = NULL;
    }

    ret += celllen;
  }

  return ret;
}

TickitPen *tickit_mockterm_get_display_pen(TickitMockTerm *mt, int line, int col)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  return mtd->cells[line][col]->pen;
}

void tickit_mockterm_resize(TickitMockTerm *mt, int newlines, int newcols)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  MockTermCell ***newcells = malloc(newlines * sizeof(MockTermCell **));

  int oldlines = mtd->lines;
  int oldcols =  mtd->cols;

  int line;
  for(line = newlines; line < oldlines; line++)
    mtd_free_line(mtd, line);

  for(line = 0; line < newlines && line < oldlines; line++) {
    MockTermCell **newlinecells;

    if(newcols == oldcols)
      newlinecells = mtd->cells[line];
    else {
      newlinecells = malloc(newcols * sizeof(MockTermCell *));

      int col;
      for(col = newcols; col < oldcols; col++)
        mtd_free_cell(mtd, line, col);

      for(col = 0; col < newcols && col < oldcols; col++)
        newlinecells[col] = mtd->cells[line][col];
      for(/* col */; col < newcols; col++)
        newlinecells[col] = NULL;

      free(mtd->cells[line]);
    }

    newcells[line] = newlinecells;
  }
  for(/* line */; line < newlines; line++)
    newcells[line] = NULL;

  free(mtd->cells);
  mtd->cells = newcells;

  mtd->lines = newlines;
  mtd->cols  = newcols;

  if(newcols > oldcols)
    for(line = 0; line < newlines && line < oldlines; line++)
      mtd_clear_cells(mtd, line, oldcols, newcols);

  for(line = oldlines; line < newlines; line++)
    mtd_clear_cells(mtd, line, 0, newcols);

  tickit_term_set_size((TickitTerm *)mt, newlines, newcols);

  BOUND(mtd->line, 0, mtd->lines-1);
  BOUND(mtd->col,  0, mtd->cols-1);
}

int tickit_mockterm_loglen(TickitMockTerm *mt)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  return mtd->logi;
}

TickitMockTermLogEntry *tickit_mockterm_peeklog(TickitMockTerm *mt, int i)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  if(i >= 0 && i < mtd->logi)
    return mtd->log + i;

  return NULL;
}

void tickit_mockterm_clearlog(TickitMockTerm *mt)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  for(int i = 0; i < mtd->logi; i++)
    mtd_free_logentry(mtd->log + i);

  mtd->logi = 0;
}

void tickit_mockterm_get_position(TickitMockTerm *mt, int *line, int *col)
{
  MockTermDriver *mtd = (MockTermDriver *)tickit_term_get_driver((TickitTerm *)mt);

  if(line)
    *line = mtd->line;
  if(col)
    *col = mtd->col;
}
