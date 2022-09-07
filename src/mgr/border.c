/*
 * draw the border around this window.             broman@nosc.mil, 1996/03
 */

#include "defs.h"

#include "graphics.h"


void border( WINDOW *win, int be_fat)
{
    int both = win->borderwid;
    int out = (be_fat == BORDER_FAT)? both - 1: win->outborderwid;
    int inr = both - out;

    if (both <= 0) {
        return;
    }

    SDL_Rect outer_rect = texture_get_rect(W(border));
    SDL_Rect inner_rect = {.x = out, .y = out, .w = outer_rect.w - out * 2, .h = outer_rect.h - out * 2};
    texture_rect(W(border), outer_rect, W(fg_color), out);
    texture_rect(W(border), inner_rect, W(bg_color), inr);
}
