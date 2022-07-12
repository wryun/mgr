/* }}} */
/* Notes */
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
/* #includes */
#include <mgr/bitblit.h>
#include <stdio.h>

#include "defs.h"

/* scroll -- scroll a bitmap */
void scroll(win, map, start, end, delta, op)
register WINDOW *win;   /* window to scroll */
register BITMAP *map;   /* bitmap in window to scroll */
int start, end, delta, op; /* starting line, ending line, # of lines */
{
    register int ems = end - start;

    if (delta > 0) {
        if (end - start > delta) {
            bit_blit(map, 0, start, BIT_WIDE(map), ems - delta, BIT_SRC, map, 0, start + delta);
        }

        bit_blit_color(map, 0, end - delta, BIT_WIDE(map), delta, &C_WHITE, NULL, 0, 0, 0);
    } else if (delta < 0) {
        if (ems + delta > 0) {
            bit_blit(map, 0, start - delta, BIT_WIDE(map), ems + delta,
                     BIT_SRC, map, 0, start);
        }

        bit_blit_color(map, 0, start, BIT_WIDE(map), -delta, &C_WHITE, NULL, NULL_DATA, 0, 0);
    }
}
