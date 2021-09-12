/*                        Copyright (c) 1988 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/*{{{  #includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mgr/bitblit.h>

#include "sdl.h"
/*}}}  */

SDL_Texture *get_texture(BITMAP *bitmap) {
  if (!IS_STATIC(bitmap)) {
    return (SDL_Texture *)bitmap->data;
  }

  SDL_Texture *texture = sdl_create_texture_from_static_bitmap(sdl_renderer, bitmap->data, bitmap->wide, bitmap->high, bitmap->depth);
  if (texture == NULL) {
    return NULL;
  }

  bitmap->data = texture;
  bitmap->type = _MEMORY;
  return texture;
}

/*{{{  bit_blit -- map bit_blits into mem_rops, caching 8 bit images as needed*/
void bit_blit(
    BITMAP *dst_map,      /* destination bitmap */
    int x_dst, int y_dst, /* destination coords */
    int wide, int high,   /* bitmap size */
    int op,               /* bitmap function */
    BITMAP *src_map,      /* source bitmap */
    int x_src, int y_src  /* source coords */
    )
{
  // printf("bit_blit(%p, %d, %d, %d, %d, %d, %p, %d, %d)\n", dst_map, x_dst, y_dst, wide, high, op, src_map, x_src, y_src);
  sdl_use_func(op);
  SDL_Rect dst_rect = {.x = dst_map->x0 + x_dst, .y = dst_map->y0 + y_dst, .w = wide, .h = high};
  SDL_Texture *dst_texture = (SDL_Texture *)dst_map->data;

  if (src_map == NULL) {
    SDL_SetRenderTarget(sdl_renderer, dst_texture);
    SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, 0x00);
    SDL_RenderFillRect(sdl_renderer, &dst_rect);
  } else {
    SDL_Texture *src_texture = get_texture(src_map);
    SDL_Rect src_rect = {.x = src_map->x0 + x_src, .y = src_map->y0 + y_src, .w = wide, .h = high};
    if (src_texture == dst_texture && 0) {
      SDL_Texture *new_src_texture = sdl_create_texture_target(sdl_renderer, wide, high);
      sdl_use_func(SRC);
      SDL_SetRenderTarget(sdl_renderer, new_src_texture);
      SDL_Rect new_src_rect = {.x = 0, .y = 0, .w = wide, .h = high};
      SDL_RenderCopy(sdl_renderer, src_texture, &src_rect, &new_src_rect);

      src_rect = new_src_rect;
      src_texture = new_src_texture;
      sdl_use_func(op);

      SDL_SetRenderTarget(sdl_renderer, dst_texture);
      SDL_RenderCopy(sdl_renderer, src_texture, &src_rect, &dst_rect);
      SDL_DestroyTexture(new_src_texture);
    } else {
      SDL_SetRenderTarget(sdl_renderer, dst_texture);
      SDL_RenderCopy(sdl_renderer, src_texture, &src_rect, &dst_rect);
    }

  }
}
/*}}}  */

BITMAP *bit_expand(
    BITMAP *map,   /* bitmap to expand */
    int fg, int bg /* foreground and background colors */
    )
{
  printf("bit_expand(%p, %d, %d)\n", map, fg, bg);
  return NULL;
}
/*}}}  */
/*{{{  bit_shrink -- shrink an 8-bit bitmap into a 1 bit bitmap*/
/* shrink an 8-bit bitmap into a 1 bit bitmap */
/* only works for primary bitmaps for now */
/* assumes 32 bit data, 8 bits per pixel */

BITMAP *bit_shrink(
    BITMAP *src_map, /* bitmap to shrink  - must be a primary bitmap */
    int bg_color     /* color to use as background - all else is on! */
    )
{
  printf("bit_shrink(%p, %d)\n", src_map, bg_color);
  return NULL;
}
/*}}}  */

void bit_bytescroll(BITMAP *map, int x, int y, int wide, int high, int delta) {
}
