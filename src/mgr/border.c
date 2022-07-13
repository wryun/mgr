/*
 * draw the border around this window.             broman@nosc.mil, 1996/03
 */

#include <mgr/bitblit.h>
#include "defs.h"


#define ONE_BOX( bm, x, y, w, h, b, col)                                   \
/* top, right, bottom, left */                                            \
    bit_blit_color( bm, (x), (y), (w), (b), (col), NULL, 0, 0, 0); \
    bit_blit_color( bm, (x) + (w) - (b), (y) + (b), (b), (h) - (b) - (b), (col), NULL, 0, 0, 0); \
    bit_blit_color( bm, (x), (y) + (h) - (b), (w), (b), (col), NULL, 0, 0, 0); \
    bit_blit_color( bm, (x), (y) + (b), (b), (h) - (b) - (b), (col), NULL, 0, 0, 0);


void border( WINDOW *win, int be_fat)
{
    int both = win->borderwid;
    int out = (be_fat == BORDER_FAT)? both - 1: win->outborderwid;
    int inr = both - out;

    BITMAP *bdr = W(border);
    int w = BIT_WIDE(bdr);
    int h = BIT_HIGH(bdr);

    if (both <= 0) {
        return;
    }

    ONE_BOX( bdr, 0, 0, w, h, out, &C_BLACK);
    ONE_BOX( bdr, out, out, w - out - out, h - out - out, inr, &C_WHITE);
}
