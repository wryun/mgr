/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* sweep out a rectangle */

/*{{{}}}*/
/*{{{  #includes*/
#include <mgr/bitblit.h>

#include "defs.h"

#include "mouse_get.h"
/*}}}  */

/*{{{  #defines*/
#define Box(screen,x,y,dx,dy) \
	type ? \
           bit_line(screen,x,y,x+dx,y+dy,BIT_NOT(BIT_DST)) : \
           box(screen,x,y,dx,dy) 
/*}}}  */

/*{{{  box -- draw a box*/
void box(screen,x1,y1,dx,dy)
BITMAP *screen;
int x1,y1,dx,dy;
   {
   if (dx<0)
      x1 += dx,dx = -dx;
   if (dy<0)
      y1 += dy,dy = -dy;
   if (dx<3)
      dx=3;
   if (dy<3)
      dy=3;

   bit_blit(screen,x1+1,y1,dx-1,1 ,BIT_NOT(BIT_DST),NULL_DATA,0,0);
   bit_blit(screen,x1+1,y1+dy,dx-1,1 ,BIT_NOT(BIT_DST),NULL_DATA,0,0);
   bit_blit(screen,x1,y1,1, dy,BIT_NOT(BIT_DST),NULL_DATA,0,0);
   bit_blit(screen,x1+dx,y1,1, dy,BIT_NOT(BIT_DST),NULL_DATA,0,0);
   }
/*}}}  */
/*{{{  get_rect*/
void get_rect(screen,mouse,x,y,dx,dy,type)
BITMAP *screen;		/* where to sweep out the box */
int mouse;			/* file to get mouse coords from */
int x,y;			/* starting position */
register int *dx,*dy;		/* box width,height */
int type;			/* rectangle or line */
   {
   int x_mouse, y_mouse;
   register int button;
   register int newx;
   register int newy;

   newx = *dx;
   newy = *dy;

   for (;;) {
      Box(screen,x,y,*dx,*dy);
      bit_present(screen);
      button = mouse_get_wait(&x_mouse, &y_mouse);
      Box(screen,x,y,*dx,*dy);
      do {
         newx += x_mouse<<1;
         newy -= y_mouse<<1;
         *dx = BETWEEN(-x,newx,(int) BIT_WIDE(screen)-x);
         *dy = BETWEEN(-y,newy,(int) BIT_HIGH(screen)-y);

         if (button == 0) {
            return;
         }
      } while ((button = mouse_get_poll(&x_mouse, &y_mouse)) != -1);
   }
   }
/*}}}  */
