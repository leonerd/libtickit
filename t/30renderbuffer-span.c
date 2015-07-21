#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *rb;
  int lines, cols;
  char buffer[256];

  rb = tickit_renderbuffer_new(10, 20);

  ok(!!rb, "tickit_renderbuffer_new");

  tickit_renderbuffer_get_size(rb, &lines, &cols);
  is_int(lines, 10, "get_size lines");
  is_int(cols,  20, "get_size cols");

  tickit_renderbuffer_flush_to_term(rb, tt);
  is_termlog("Empty RenderBuffer renders nothing to term",
      NULL);

  ok(!tickit_renderbuffer_get_cell_active(rb, 0, 0), "get_cell_active SKIP");

  // Absolute spans
  {
    TickitPen *fg_pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 1, -1);
    tickit_renderbuffer_setpen(rb, fg_pen);
    cols = tickit_renderbuffer_text_at(rb, 0, 1, "text span");
    is_int(cols, 9, "cols from text_at");
    tickit_renderbuffer_erase_at(rb, 1, 1, 5);

    is_int(tickit_renderbuffer_get_cell_text(rb, 0, 1, buffer, sizeof buffer), 1, "get_cell_text TEXT at 0,1");
    is_str(buffer, "t", "buffer text at TEXT 0,1");
    is_int(tickit_pen_get_colour_attr(tickit_renderbuffer_get_cell_pen(rb, 0, 1), TICKIT_PEN_FG), 1,
        "get_cell_pen FG at 0,1");

    is_int(tickit_renderbuffer_get_cell_text(rb, 0, 2, buffer, sizeof buffer), 1, "get_cell_text TEXT at 0,2");
    is_str(buffer, "e", "buffer text at TEXT 0,2");

    is_int(tickit_renderbuffer_get_cell_text(rb, 1, 1, buffer, sizeof buffer), 0, "get_cell_text ERASE at 1,1");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders text to terminal",
        GOTO(0,1), SETPEN(.fg=1), PRINT("text span"),
        GOTO(1,1), SETPEN(.fg=1), ERASECH(5,-1),
        NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer now empty after render to terminal",
        NULL);

    tickit_pen_destroy(fg_pen);
  }

  // UTF-8 handling
  {
    cols = tickit_renderbuffer_text_at(rb, 6, 0, "somé text ĉi tie");
    is_int(cols, 16, "cols from text_at UTF-8");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders UTF-8 text",
        GOTO(6,0), SETPEN(), PRINT("somé text ĉi tie"),
        NULL);
  }

  // Error conditions
  {
    is_int(tickit_renderbuffer_text_at(rb, 0, 0, "foo\nbar"), -1,
        "text_at() returns -1");
  }

  // Span splitting
  {
    TickitPen *b_pen = tickit_pen_new_attrs(TICKIT_PEN_BOLD, 1, -1);

    // aaaAAaaa
    // BBBBBBBB
    // cccCCCCC
    // DDDDDddd

    tickit_renderbuffer_text_at(rb, 0, 0, "aaaaaaaa");
    tickit_renderbuffer_text_at(rb, 1, 2, "bbbb");
    tickit_renderbuffer_text_at(rb, 2, 0, "cccccc");
    tickit_renderbuffer_text_at(rb, 3, 2, "dddddd");

    {
      tickit_renderbuffer_savepen(rb);
      tickit_renderbuffer_setpen(rb, b_pen);
      tickit_renderbuffer_text_at(rb, 0, 3, "AA");
      tickit_renderbuffer_text_at(rb, 1, 0, "BBBBBBBB");
      tickit_renderbuffer_text_at(rb, 2, 3, "CCCCC");
      tickit_renderbuffer_text_at(rb, 3, 0, "DDDDD");
      tickit_renderbuffer_restore(rb);
    }

    // empty text should do nothing
    tickit_renderbuffer_text_at(rb, 4, 4, "");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer spans can be split",
        GOTO(0,0), SETPEN(), PRINT("aaa"), SETPEN(.b=1), PRINT("AA"), SETPEN(), PRINT("aaa"),
        GOTO(1,0), SETPEN(.b=1), PRINT("BBBBBBBB"),
        GOTO(2,0), SETPEN(), PRINT("ccc"), SETPEN(.b=1), PRINT("CCCCC"),
        GOTO(3,0), SETPEN(.b=1), PRINT("DDDDD"), SETPEN(), PRINT("ddd"),
        NULL);

    tickit_pen_destroy(b_pen);
  }

  {
    tickit_renderbuffer_text_at(rb, 0, 0, "abcdefghijkl");
    tickit_renderbuffer_text_at(rb, 0, 2, "-");
    tickit_renderbuffer_text_at(rb, 0, 4, "-");
    tickit_renderbuffer_text_at(rb, 0, 6, "-");
    tickit_renderbuffer_text_at(rb, 0, 8, "-");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders overwritten text split chunks",
        GOTO(0,0),
        SETPEN(), PRINT("ab"),
        SETPEN(), PRINT("-"), // c
        SETPEN(), PRINT("d"),
        SETPEN(), PRINT("-"), // e
        SETPEN(), PRINT("f"),
        SETPEN(), PRINT("-"), // g
        SETPEN(), PRINT("h"),
        SETPEN(), PRINT("-"), // i
        SETPEN(), PRINT("jkl"),
        NULL);
  }

  // VC spans
  {
    TickitPen *fg_pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 3, -1);
    tickit_renderbuffer_setpen(rb, fg_pen);
    tickit_renderbuffer_goto(rb, 0, 2);
    cols = tickit_renderbuffer_text(rb, "text span");
    is_int(cols, 9, "cols from text");

    tickit_renderbuffer_goto(rb, 1, 2);
    tickit_renderbuffer_erase(rb, 5);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders text at VC",
        GOTO(0,2), SETPEN(.fg=3), PRINT("text span"),
        GOTO(1,2), SETPEN(.fg=3), ERASECH(5,-1),
        NULL);

    tickit_pen_destroy(fg_pen);
  }

  // Translation
  {
    tickit_renderbuffer_translate(rb, 3, 5);

    cols = tickit_renderbuffer_text_at(rb, 0, 0, "at 0,0");
    is_int(cols, 6, "cols from text_at translated");

    is_int(tickit_renderbuffer_get_cell_text(rb, 0, 0, buffer, sizeof buffer), 1,
        "get_cell_text TEXT at 0,0 translated");
    is_str(buffer, "a", "buffer text at TEXT 0,0 translatd");

    tickit_renderbuffer_goto(rb, 1, 0);

    int line, col;
    tickit_renderbuffer_get_cursorpos(rb, &line, &col);
    is_int(line, 1, "RenderBuffer line position after translate");
    is_int(col,  0, "RenderBuffer column position after translate");

    tickit_renderbuffer_text(rb, "at 1,0");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders text with translation",
        GOTO(3,5), SETPEN(), PRINT("at 0,0"),
        GOTO(4,5), SETPEN(), PRINT("at 1,0"),
        NULL);
  }

  // Truncates correctly
  {
    cols = tickit_renderbuffer_textn_at(rb, 4, 0, "ABCDEFGHI", 3);
    is_int(cols, 3, "cols from textn_at truncated correctly");
    cols = tickit_renderbuffer_textn_at(rb, 5, 1, "ABCDEF", 6);
    is_int(cols, 6, "cols from textn_at allows the full string");
    cols = tickit_renderbuffer_textn_at(rb, 6, 2, "LMNOP", -1);
    is_int(cols, 5, "cols from textn_at handles -1");

    tickit_renderbuffer_goto(rb, 7, 3);
    cols = tickit_renderbuffer_textn(rb, "QRSTUV", 4);
    is_int(cols, 4, "cols from textn truncated correctly");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer textn rendering",
        GOTO(4,0), SETPEN(), PRINT("ABC"),
        GOTO(5,1), SETPEN(), PRINT("ABCDEF"),
        GOTO(6,2), SETPEN(), PRINT("LMNOP"),
        GOTO(7,3), SETPEN(), PRINT("QRST"),
        NULL);
  }

  // Eraserect
  {
    tickit_renderbuffer_eraserect(rb, &(TickitRect){.top = 2, .left = 3, .lines = 5, .cols = 8});

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders eraserect",
        GOTO(2,3), SETPEN(), ERASECH(8,-1),
        GOTO(3,3), SETPEN(), ERASECH(8,-1),
        GOTO(4,3), SETPEN(), ERASECH(8,-1),
        GOTO(5,3), SETPEN(), ERASECH(8,-1),
        GOTO(6,3), SETPEN(), ERASECH(8,-1),
        NULL);
  }

  // Clear
  {
    TickitPen *bg_pen = tickit_pen_new_attrs(TICKIT_PEN_BG, 3, -1);

    tickit_renderbuffer_setpen(rb, bg_pen);
    tickit_renderbuffer_clear(rb);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders clear",
        GOTO(0,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(1,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(2,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(3,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(4,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(5,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(6,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(7,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(8,0), SETPEN(.bg=3), ERASECH(20,-1),
        GOTO(9,0), SETPEN(.bg=3), ERASECH(20,-1),
        NULL);

    tickit_pen_destroy(bg_pen);
  }

  tickit_renderbuffer_destroy(rb);

  return exit_status();
}
