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

    TEXTURE *bdr = W(border);
    int w = bdr->rect.w;
    int h = bdr->rect.h;

    if (both <= 0) {
        return;
    }

    SDL_Rect outer_rect = {.x = 0, .y = 0, .w = w, .h = h};
    SDL_Rect inner_rect = {.x = out, .y = out, .w = w - out * 2, .h = h - out * 2};
    texture_rect(bdr, outer_rect, W(fg_color), out);
    texture_rect(bdr, inner_rect, W(bg_color), inr);
}
