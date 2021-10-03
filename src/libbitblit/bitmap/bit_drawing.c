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
  SDL_SetRenderTarget(sdl_renderer, (SDL_Texture *)dest->data);
  SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
  SDL_RenderDrawLine(sdl_renderer, dest->x0 + x0, dest->y0 + y0, dest->x0 + x1, dest->y0 + y1);
}

int bit_point(BITMAP *dest, int x, int y, int func)
{
  SDL_SetRenderTarget(sdl_renderer, (SDL_Texture *)dest->data);
  SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
  SDL_RenderDrawPoint(sdl_renderer, dest->x0 + x, dest->y0 + y);

  /* TODO - read pix at location??? */
  return 0;
}

void bit_rect(BITMAP *dest, int x, int y, int wide, int high, COLOR *color) {
  SDL_Rect dst_rect = {.x = dest->x0 + x, .y = dest->y0 + y, .w = wide, .h = high};
  SDL_Texture *dst_texture = (SDL_Texture *)dest->data;

  SDL_SetRenderTarget(sdl_renderer, dst_texture);
  SDL_SetRenderDrawColor(sdl_renderer, color->r, color->g, color->b, color->a);
  SDL_RenderFillRect(sdl_renderer, &dst_rect);
}

int bit_on(BITMAP *bp, int x, int y) {
  /* RenderReadPixels - probably a bad idea - slow/might not work with textures */
  return 0;
}
