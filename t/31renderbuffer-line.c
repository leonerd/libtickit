#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *rb;

  rb = tickit_renderbuffer_new(30, 30);

  // Simple lines, end caps
  {
    TickitPen *fg_pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 1, -1);

    tickit_renderbuffer_setpen(rb, fg_pen);
    tickit_renderbuffer_hline_at(rb, 10, 10, 14, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_hline_at(rb, 11, 10, 14, TICKIT_LINE_SINGLE, TICKIT_LINECAP_START);
    tickit_renderbuffer_hline_at(rb, 12, 10, 14, TICKIT_LINE_SINGLE, TICKIT_LINECAP_END);
    tickit_renderbuffer_hline_at(rb, 13, 10, 14, TICKIT_LINE_SINGLE, TICKIT_LINECAP_BOTH);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders hline_at to terminal",
        GOTO(10,10), SETPEN(.fg=1), PRINT("╶───╴"),
        GOTO(11,10), SETPEN(.fg=1), PRINT("────╴"),
        GOTO(12,10), SETPEN(.fg=1), PRINT("╶────"),
        GOTO(13,10), SETPEN(.fg=1), PRINT("─────"),
        NULL);

    tickit_renderbuffer_setpen(rb, fg_pen);
    tickit_renderbuffer_vline_at(rb, 10, 13, 10, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(rb, 10, 13, 11, TICKIT_LINE_SINGLE, TICKIT_LINECAP_START);
    tickit_renderbuffer_vline_at(rb, 10, 13, 12, TICKIT_LINE_SINGLE, TICKIT_LINECAP_END);
    tickit_renderbuffer_vline_at(rb, 10, 13, 13, TICKIT_LINE_SINGLE, TICKIT_LINECAP_BOTH);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders vline_at to terminal",
        GOTO(10,10), SETPEN(.fg=1), PRINT("╷│╷│"),
        GOTO(11,10), SETPEN(.fg=1), PRINT("││││"),
        GOTO(12,10), SETPEN(.fg=1), PRINT("││││"),
        GOTO(13,10), SETPEN(.fg=1), PRINT("╵╵││"),
        NULL);

    tickit_pen_unref(fg_pen);
  }

  // Line merging
  {
    tickit_renderbuffer_hline_at(rb, 10, 10, 14, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_hline_at(rb, 11, 10, 14, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_hline_at(rb, 12, 10, 14, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(rb, 10, 12, 10, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(rb, 10, 12, 12, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(rb, 10, 12, 14, TICKIT_LINE_SINGLE, 0);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders line merging",
        GOTO(10,10), SETPEN(), PRINT("┌─┬─┐"),
        GOTO(11,10), SETPEN(), PRINT("├─┼─┤"),
        GOTO(12,10), SETPEN(), PRINT("└─┴─┘"),
        NULL);
  }

  tickit_renderbuffer_destroy(rb);
  tickit_term_destroy(tt);

  return exit_status();
}
