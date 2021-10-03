/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/*{{{}}}*/
/*{{{  #includes*/
#include <mgr/bitblit.h>
#include <mgr/share.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdl.h"
#include <GL/gl.h>
/*}}}  */


#define WIDTH 1366
#define HEIGHT 768
#define DEPTH 1  /* TODO ... colours. */


/* For now, we use as a global, since we need access to it when creating memory bitmaps...
 * (so we can build textures straight away rather than mucking around with surfaces).
 */
SDL_Renderer *sdl_renderer;
SDL_Window *sdl_window;


/*{{{  bit_open.c*/
BITMAP *bit_open(char *name)
{
  BITMAP *result = (BITMAP*)0;

  if ((result=malloc(sizeof(BITMAP)))==(BITMAP*)0) return (BITMAP*)0;
  result->primary = result;

  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
    return NULL;
  }

  if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, SDL_WINDOW_FULLSCREEN_DESKTOP, &sdl_window, &sdl_renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
    return NULL;
  }

  sdl_helper_setup(sdl_renderer, sdl_window);

  result->wide = WIDTH;
  result->high = HEIGHT;
  result->depth = DEPTH;

  /* We create a texture even for the actual device so we don't have to redraw completely
   * between rerenders (SDL's backbuffer is considered invalid after each present).
   * This is just the way mgr works (for now...).
   */
  result->data = sdl_create_texture_target(sdl_renderer, WIDTH, HEIGHT);

  if (result->data == NULL) {
    return NULL;
  }

  result->x0 = 0;
  result->y0 = 0;
  result->type = _SCREEN;
  result->cache = 0;
  result->color = 0;
  result->id = 0;	/* set elsewhere? */
# ifdef MOVIE
  log_open(result);
# endif
  return result;
}
/*}}}  */

void bit_warpmouse(int x, int y) {
  SDL_WarpMouseInWindow(sdl_window, x, y);
}
