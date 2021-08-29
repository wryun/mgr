/*                        Copyright (c) 1988 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/*  Draw a line stub */

#include <stdio.h>

#include "sdl.h"

void bit_line(BITMAP *dest, int x0, int y0, int x1, int y1, int func)
{
  printf("bit_line(%p, %d, %d, %d, %d, %d)\n", dest, x0, y0, x1, y1, func);

  struct device_info *deviceinfo = (struct device_info *)(dest->deviceinfo);

  sdl_use_func(func);

  SDL_SetRenderTarget(sdl_renderer, deviceinfo->texture);
  SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, 0x00);
  SDL_RenderDrawLine(sdl_renderer, x0, y0, x1, y1);
  // TODO remove
  SDL_SetRenderTarget(sdl_renderer, NULL);
  SDL_RenderCopy(sdl_renderer, deviceinfo->texture, NULL, NULL);
  SDL_RenderPresent(sdl_renderer);
}
