/*{{{}}}*/
/*{{{  Notes*/
/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
*/

/* drag a box around the screen */
/*}}}  */
/*{{{  #includes*/
#include <mgr/bitblit.h>

#include "defs.h"

#include "get_rect.h"
#include "mouse_get.h"
/*}}}  */

/*{{{  move_box*/
void move_box(screen,mouse,x,y,dx,dy,how)
BITMAP *screen;			/* where to sweep out the box */
int mouse;			/* file to get mouse coords from */
register int *x,*y;		/* starting position */
register int dx,dy;             /* box size */
int how;					/* termination condition */
{
  int x_mouse, y_mouse;
  register int button;

  for (;;) {
    box(screen,*x,*y,dx,dy);
    bit_present(screen);
    button = mouse_get_wait(&x_mouse, &y_mouse);
    box(screen,*x,*y,dx,dy);
    do {
      *x += x_mouse;
      *y -= y_mouse;
      *x = BETWEEN(0,*x,BIT_WIDE(screen)-dx);
      *y = BETWEEN(0,*y,BIT_HIGH(screen)-dy);

      if (how ? button == 0 : button != 0) {
        return;
      }
    } while ((button = mouse_get_poll(&x_mouse, &y_mouse)) != -1);
  }

}
/*}}}  */
