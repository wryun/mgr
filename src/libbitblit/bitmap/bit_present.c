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

void bit_present(BITMAP *dest)
{
  if (!IS_SCREEN(dest)) {
    printf("calling present without screen?");
    return;
  }

  sdl_use_func(SRC);

  SDL_SetRenderTarget(sdl_renderer, NULL);
  SDL_RenderCopy(sdl_renderer, (SDL_Texture *)dest->data, NULL, NULL);
  SDL_RenderPresent(sdl_renderer);

  // SDL recommended... is this really necessary for OpenGL stuff?
  SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 0);
  SDL_RenderClear(sdl_renderer);
}
