/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

#include <stdio.h>
#include "sdl.h"
#include <GL/gl.h>


#define WIDTH 1280 
#define HEIGHT 1024 
#define DEPTH 1  /* TODO ... colours. */


// For now, we use as a global, since we need access to it when creating memory bitmaps...
// (so we can build textures straight away rather than mucking around with surfaces).
SDL_Renderer *sdl_renderer;


DATA *
bit_initscreen(char *name, int *width, int *height, unsigned char *depth, void **device)
{
  printf("bit_initscreen(%s)\n", name);

  struct device_info *device_info = malloc(sizeof(struct device_info));

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
    return NULL;
  }

  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles");

  if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, SDL_WINDOW_SHOWN, &(device_info->window), &sdl_renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
    return NULL;
  }

  sdl_helper_setup(sdl_renderer);

  // We create a texture even for the actual device so we don't have to redraw completely
  // between rerenders (SDL's backbuffer is considered invalid after each present).
  // This is just the way mgr works (for now...).
  if (!(device_info->texture = sdl_create_texture_target(sdl_renderer, WIDTH, HEIGHT))) {
    return NULL;
  }

  *width = WIDTH;
  *height = HEIGHT;
  *depth = DEPTH;
  *device = device_info;
  return NULL;
}

void display_close(BITMAP *bitmap)
{
  printf("display_close(%p)\n", bitmap);

  struct device_info *device_info = (struct device_info *)bitmap->deviceinfo;

  if (device_info == NULL)
    return;

  // TODO - err, what about the other textures? ... how does cleanup work?
  // It's in the unabstracted bit. Just need to rip all this out, I think.
  SDL_DestroyTexture(device_info->texture);
  SDL_DestroyRenderer(sdl_renderer);
  SDL_DestroyWindow(device_info->window);

  free(device_info);
  bitmap->deviceinfo = NULL;

  SDL_Quit();
}
