/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

#include <stdio.h>
#include <stdlib.h>

#include <mgr/bitblit.h>

#include "sdl.h"

void bit_destroy(BITMAP *bitmap)
{
  if (bitmap == NULL || IS_STATIC(bitmap)) {
    return;
  }

  if (IS_PRIMARY(bitmap)) {
    SDL_DestroyTexture(bitmap->data);

    if (IS_SCREEN(bitmap) && bitmap->primary == bitmap) {
      SDL_DestroyRenderer(sdl_renderer);
      SDL_DestroyWindow(sdl_window);
      SDL_Quit();
    }
  }

  free(bitmap);

#ifdef MOVIE
  log_destroy(bitmap);
#endif

  return;
}
