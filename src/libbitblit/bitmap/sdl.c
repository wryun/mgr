/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

#include <stdio.h>
#include <signal.h>
#include <assert.h>

#include <GL/gl.h>

#include <mgr/bitblit.h>

#include "sdl.h"


static SDL_PixelFormatEnum static_bitmap_pixel_format = SDL_PIXELFORMAT_INDEX1MSB;
static SDL_PixelFormatEnum preferred_pixel_format = SDL_PIXELFORMAT_ABGR8888;
static const SDL_Color bitmap_palette_colors[] = {
  {0x00, 0x00, 0x00, 0x00},
  {0xFF, 0xFF, 0xFF, 0xFF},
};

static void (APIENTRY *glLogicOp_f)(int);
static void (APIENTRY *glEnable_f)(int);


int sdl_helper_setup(SDL_Renderer *renderer) {
  glLogicOp_f = SDL_GL_GetProcAddress("glLogicOp");
  glEnable_f = SDL_GL_GetProcAddress("glEnable");

  glEnable_f(GL_COLOR_LOGIC_OP);

  SDL_RendererInfo rendererInfo;
  // Below code fails anyway.
  return 1;

  int err;
  if (!(err = SDL_GetRendererInfo(renderer, &rendererInfo))) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't interrogate renderer: %s", SDL_GetError());
    return 1;
  }
  if (rendererInfo.num_texture_formats > 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer had no texture pixel formats? (what?)");
    return 1;
  }

  // Let's just grab the first one for now...
  preferred_pixel_format = rendererInfo.texture_formats[0];
  SDL_Log("Chosen texture format: %s", SDL_GetPixelFormatName(preferred_pixel_format));

  return 0;
}


SDL_Texture *sdl_create_texture_target(SDL_Renderer *renderer, int x, int y) {
  SDL_Texture *texture = SDL_CreateTexture(renderer, preferred_pixel_format, SDL_TEXTUREACCESS_TARGET, x, y);
  if (texture == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create dynamic texture: %s", SDL_GetError());
    return NULL;
  }

  return texture;
}


SDL_Texture *sdl_create_texture_from_static_bitmap(SDL_Renderer *renderer, void *pixels, int wide, int high, int depth) {
  assert(depth == SDL_BITSPERPIXEL(static_bitmap_pixel_format));

  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, wide, high, depth, wide * depth / 8, static_bitmap_pixel_format);
  if (surface == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create surface for static texture: %s", SDL_GetError());
    return NULL;
  }

  if (SDL_SetPaletteColors(surface->format->palette, bitmap_palette_colors, 0, 2) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set surface palette colors for static texture: %s", SDL_GetError());
    return NULL;
  }

  glLogicOp_f(GL_COPY);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create static texture %d: %s", renderer, SDL_GetError());
    return NULL;
  }

  SDL_FreeSurface(surface);

  return texture;
}


// Surely these should be the same-ish numbers? I don't understand raster ops.
// Why are SRC/DST 0xC/0xA?
static inline int mgr_to_gl_opcode(int op) {
  switch(op) {
  case 0:
    return GL_CLEAR;
  case BIT_NOT(0):
    return GL_SET;
  case SRC:
    return GL_COPY;
  case BIT_NOT(SRC):
    return GL_COPY_INVERTED;
  case DST:
    return GL_NOOP;
  case BIT_NOT(DST):
    return GL_INVERT;
  case SRC & DST:
    return GL_AND;
  case BIT_NOT(SRC & DST):
    return GL_NAND;
  case SRC | DST:
    return GL_OR;
  case BIT_NOT(SRC | DST):
    return GL_NOR;
  case SRC ^ DST:
    return GL_XOR;
  case BIT_NOT(SRC ^ DST):
    return GL_EQUIV;
  case SRC & BIT_NOT(DST):
    return GL_AND_REVERSE;
  case BIT_NOT(SRC) & DST:
    return GL_AND_INVERTED;
  case SRC | BIT_NOT(DST):
    return GL_OR_REVERSE;
  case BIT_NOT(SRC) | DST:
    return GL_OR_INVERTED;
  default:
    assert(0);
    return GL_COPY;
  }
}


void sdl_use_func(int func) {
  glLogicOp_f(mgr_to_gl_opcode(OPCODE(func)));

  /* TODO handle fg/bg colour... but can't do it with shader because GL 1.1 for logic op? Bah.
   * I guess we could set the _foreground_ colour with glColor4f, at least?
   * Black and white here we come, for now...
   * Probably should just dump the raster ops and use textures for everything, once I've
   * got something minimally working.
   *
   * This all seems really broken. If I enable logic ops, I get reversed colors?
   * and fonts are ... not there.
   */
}
