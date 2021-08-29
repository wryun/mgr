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
#include "sdl.h"
#include <GL/gl.h>
/*}}}  */

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

  if (src_map == NULL) {
    // TODO - Use RenderDrawRect
    return;
  }

  struct device_info *src_info = (struct device_info *)(src_map->deviceinfo);
  struct device_info *dst_info = (struct device_info *)(dst_map->deviceinfo);

  if (src_info->texture == NULL) {  // looks like it's a static that we haven't seen yet.
    src_info->texture = sdl_create_texture_from_static_bitmap(sdl_renderer, src_map);
  }

  sdl_use_func(op);
  SDL_SetRenderTarget(sdl_renderer, dst_info->texture);

  SDL_Rect dst_rect = {.x = x_dst, .y = y_dst, .w = wide, .h = high};
  SDL_Rect src_rect = {.x = x_src, .y = y_src, .w = wide, .h = high};
  SDL_RenderCopy(sdl_renderer, src_info->texture, &src_rect, &dst_rect);

  // TODO - at an appropriate time...
  SDL_SetRenderTarget(sdl_renderer, NULL);
  SDL_RenderCopy(sdl_renderer, dst_info->texture, NULL, NULL);
  SDL_RenderPresent(sdl_renderer);
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

void foo () {
}
