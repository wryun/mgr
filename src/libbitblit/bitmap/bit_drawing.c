/*                        Copyright (c) 1988 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

#include <stdio.h>

#include <mgr/bitblit.h>

#include "sdl.h"

void bit_line(BITMAP *dest, int x0, int y0, int x1, int y1, int func)
{
  printf("bit_line(%p, %d, %d, %d, %d, %d)\n", dest, x0, y0, x1, y1, func);

  sdl_use_func(func);

  SDL_SetRenderTarget(sdl_renderer, (SDL_Texture *)dest->data);
  SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, 0x00);
  SDL_RenderDrawLine(sdl_renderer, x0, y0, x1, y1);
}

int bit_point(BITMAP *dest, int x, int y, int func)
{
  sdl_use_func(func);

  SDL_SetRenderTarget(sdl_renderer, (SDL_Texture *)dest->data);
  SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, 0x00);
  SDL_RenderDrawPoint(sdl_renderer, x, y);

  /* TODO - read pix at location??? */
  return 0;
}

int bit_on(BITMAP *bp, int x, int y) {
  /* RenderReadPixels - probably a bad idea - slow/might not work with textures */
  return 0;
}
