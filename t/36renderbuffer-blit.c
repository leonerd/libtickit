#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *screen, *window;

  screen = tickit_renderbuffer_new(25, 80);
  window = tickit_renderbuffer_new(10, 20);

  // Basic blit
  {
    tickit_renderbuffer_text_at(window, 0, 0, "Hello");
    tickit_renderbuffer_char_at(window, 1, 1, 'A');
    tickit_renderbuffer_skip_at(window, 2, 2, 2);

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer basic blitting",
        GOTO(0,0), SETPEN(), PRINT("Hello"),
        GOTO(1,1), SETPEN(), PRINT("A"),
        NULL);

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer blitting doesn't wipe src rb",
        GOTO(0,0), SETPEN(), PRINT("Hello"),
        GOTO(1,1), SETPEN(), PRINT("A"),
        NULL);
  }

  // Blitting an erase wipes underlying content
  {
    tickit_renderbuffer_reset(window);
    tickit_renderbuffer_text_at(screen, 0, 0, "Hello");
    tickit_renderbuffer_erase_at(window, 0, 0, 4);

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer blit can erase underlying content",
        GOTO(0,0), SETPEN(), ERASECH(4,1),
        SETPEN(), PRINT("o"),
        NULL);
  }

  // Blitting text that spans a mask
  {
    tickit_renderbuffer_reset(window);
    TickitRect mask = { .top = 0, .left = 2, .lines = 1, .cols = 1 };
    tickit_renderbuffer_mask(window, &mask);
    tickit_renderbuffer_text_at(window, 0, 0, "Hello");

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer blit can have text spanning a mask",
        GOTO(0,0), SETPEN(), PRINT("He"),
        GOTO(0,3), SETPEN(), PRINT("lo"),
        NULL);
  }

  // Blitting merges line segments
  {
    tickit_renderbuffer_reset(window);
    tickit_renderbuffer_hline_at(window, 1, 0, 20, TICKIT_LINE_SINGLE, 0);

    tickit_renderbuffer_vline_at(screen, 0, 2, 5, TICKIT_LINE_SINGLE, 0);

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer blit merges line segments",
        GOTO(0,5), SETPEN(),      PRINT("╷"),
        GOTO(1,0), SETPEN(), PRINT("╶────┼──────────────"),
        GOTO(2,5), SETPEN(),      PRINT("╵"),
        NULL);
  }

  // Blitting obeys translation
  {
    tickit_renderbuffer_reset(window);

    tickit_renderbuffer_translate(window, 3, 3);
    tickit_renderbuffer_translate(screen, 2, 4);

    tickit_renderbuffer_text_at(window, 0, 1, "Hello");
    tickit_renderbuffer_char_at(window, 2, 4, 'B');

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer blit obeys translation",
        GOTO(5,8), SETPEN(), PRINT("Hello"),
        GOTO(7,11), SETPEN(), PRINT("B"),
        NULL);
  }

  // Blitting obeys clipping
  {
    tickit_renderbuffer_reset(window);

    TickitRect clip = { .top=2, .left=2, .lines=6, .cols=16 };
    tickit_renderbuffer_clip(screen, &clip);

    tickit_renderbuffer_text_at(window, 1, 0, "Hello");
    tickit_renderbuffer_text_at(window, 2, 0, "World");
    tickit_renderbuffer_char_at(window, 2, 19, 'C');

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer blit obeys clipping",
        GOTO(2,2), SETPEN(), PRINT("rld"),
        NULL);
  }

  // Blitting overrides destination's pen
  {
    tickit_renderbuffer_reset(window);

    TickitPen *screen_pen = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 4,
        0);
    tickit_renderbuffer_setpen(screen, screen_pen);
    tickit_pen_unref(screen_pen);

    TickitPen *window_pen = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 5,
        0);
    tickit_renderbuffer_setpen(window, window_pen);
    tickit_pen_unref(window_pen);

    TickitPen *bg_pen = tickit_pen_new_attrs(
        TICKIT_PEN_BG, 6,
        0);

    tickit_renderbuffer_text_at(window, 0, 0, "Hello");
    {
      tickit_renderbuffer_savepen(window);
      tickit_renderbuffer_setpen(window, bg_pen);
      tickit_renderbuffer_char_at(window, 1, 1, 'A');
      tickit_renderbuffer_restore(window);
    }

    tickit_renderbuffer_setpen(window, NULL);

    {
      tickit_renderbuffer_savepen(window);
      tickit_renderbuffer_setpen(window, bg_pen);
      tickit_renderbuffer_text_at(window, 2, 2, "World");
      tickit_renderbuffer_restore(window);
    }

    tickit_renderbuffer_text_at(window, 3, 3, "Again");
    tickit_renderbuffer_text_at(screen, 4, 4, "Preserved Pen");

    tickit_renderbuffer_blit(screen, window);

    tickit_renderbuffer_flush_to_term(screen, tt);
    is_termlog("RenderBuffer basic blitting",
        GOTO(0,0), SETPEN(.fg=5), PRINT("Hello"),
        GOTO(1,1), SETPEN(.fg=5,.bg=6), PRINT("A"),
        GOTO(2,2), SETPEN(.fg=4,.bg=6), PRINT("World"),
        GOTO(3,3), SETPEN(.fg=4), PRINT("Again"),
        GOTO(4,4), SETPEN(.fg=4), PRINT("Preserved Pen"),
        NULL);

    tickit_pen_unref(bg_pen);
  }

  tickit_renderbuffer_unref(window);
  tickit_renderbuffer_unref(screen);
  tickit_term_unref(tt);

  return exit_status();
}
