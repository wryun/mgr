/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/*****************************************************************************
 *	scroll a bitmap
 */
#include <stdio.h>

#include "defs.h"

#include "graphics.h"

/* scroll -- scroll a bitmap */
void scroll(map, start, end, delta)
register TEXTURE *texture;   /* texture to scroll */
int start, end, delta; /* starting line, ending line, # of lines */
{
    register int ems = end - start;

    /* Now we're using textures, we really should just be copying straight to a new texture.
     * The problem with this is that with our 'interesting' TEXTURE struct, a single
     * sdl_texture could be referred to multiple times, and we have no current
     * way of tracking down the other references.
     */

    if (delta > 0) {
        if (end - start > delta) {
            //SDL_Rect keep_rect = {.x = 0, .y = start + delta, .w = texture->rect.w, .h = ems};
            //texture_copy(texture, 
            bit_blit(texture, 0, start, BIT_WIDE(texture), ems - delta, BIT_SRC, texture, 0, start + delta);
        }

        bit_blit_color(texture, 0, end - delta, BIT_WIDE(texture), delta, &W(bg_color), NULL, 0, 0, 0);
    } else if (delta < 0) {
        if (ems + delta > 0) {
            bit_blit(texture, 0, start - delta, BIT_WIDE(texture), ems + delta,
                     BIT_SRC, texture, 0, start);
        }

        bit_blit_color(texture, 0, start, BIT_WIDE(texture), -delta, &W(bg_color), NULL, NULL_DATA, 0, 0);
    }
}
