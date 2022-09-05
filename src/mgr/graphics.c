/* Light wrappers around SDL2 to make our normal usage a bit simpler
 * and closer to how mgr used to work.
 *
 * Primarily relies on wrapping SDL_Texture in something which:
 *   - lets us know the height/width without asking
 *   - allows us to create regions that act as new pseudo-textures
 *     (for the purposes of these functions)
 *   - makes it easier to deal with the 1-bit bitmaps that mgr
 *     uses for icons, fonts and cursors
 *
 * Also hides our dodgy globals (sdl_window/sdl_renderer) from the rest of the world.
 *
 * We don't properly maintain the abstraction: we let SDL data structures
 * (e.g. SDL_Texture, SDL_Rect, SDL_Point) leak into the main codebase.
 * Since we're so heavily tied to SDL now (cf event loop), I'm ok with this.
 */

#include <assert.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "defs.h"
#include "graphics.h"


static SDL_PixelFormatEnum static_bitmap_pixel_format = SDL_PIXELFORMAT_INDEX1MSB;
/* This is what most OpenGL renderers like. I think. We try to fix it up in the init, though,
 * based on SDL_GetWindowPixelFormat.
 */
static SDL_PixelFormatEnum preferred_pixel_format = SDL_PIXELFORMAT_ABGR8888;
/* bitmap_palette_colors is quite important to how we're handling things.
 *
 * For fonts, we find it pretty useful to be able to change the background/foreground
 * independently, and the easiest way to do that is to make the background transparent.
 * Then it's possible to just plonk the font on top of a plain rectangle background
 * (and easily recolour the black foreground by applying a colour transform).
 *
 * If we want to support more depths other than 1-bit, this approach will
 * need to be reconsidered...
 */
static const SDL_Color bitmap_palette_colors[] = {
  {0x00, 0x00, 0x00, SDL_ALPHA_TRANSPARENT},
  {0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE},
};


/* For now, we use as a global, since we need access to it when creating memory bitmaps...
 * (so we can build textures straight away rather than mucking around with surfaces).
 */
static SDL_Renderer *sdl_renderer;
static SDL_Window *sdl_window;
/* We create a texture even for the actual device so we don't have to redraw completely
 * between rerenders (SDL's backbuffer is considered invalid after each present).
 * This is just the way mgr works (for now...). The remaining blocker for removing this
 * is the menu system, which draws on top of the windows and blocks everything up.
 */
static TEXTURE *screen_texture;


TEXTURE *screen_init(int width, int height)
{
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_OPENGL, &sdl_window, &sdl_renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
        return NULL;
    }

    SDL_PixelFormatEnum pf = SDL_GetWindowPixelFormat(sdl_window);

    if (pf == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find window pixel format: %s", SDL_GetError());
    } else {
        preferred_pixel_format = pf;
    }

    screen_texture = texture_create_empty(width, height);
    return screen_texture;
}

void screen_present()
{
    SDL_SetRenderTarget(sdl_renderer, NULL);
    SDL_RenderCopy(sdl_renderer, screen_texture->sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);
}

void screen_flush()
{
    SDL_RenderFlush(sdl_renderer);
}

void cursor_warp(SDL_Point point)
{
    SDL_WarpMouseInWindow(sdl_window, point.x, point.y);
}

SDL_Cursor *cursor_create(void *pixels, int width, int height, int depth)
{
    /* Currently, we only support mono bitmaps */
    assert(depth == SDL_BITSPERPIXEL(static_bitmap_pixel_format));

    const size_t sdl_cursor_height = (height < 16) ? height : 16;
    const size_t sdl_cursor_size = sdl_cursor_height * 16 / 8;
    int row_byte_width = width * depth / 8;
    int cursor_row_byte_width = 16 * depth / 8;
    Uint8 data[sdl_cursor_size];
    Uint8 mask[sdl_cursor_size];
    Uint8 *source_white = (Uint8 *)pixels;
    Uint8 *source_black = source_white;
  
    if (height >= 32) {
        source_black = &(((Uint8 *)pixels)[16 * row_byte_width]);
    }

    for (size_t i = 0, x = 0, y = 0; i < sdl_cursor_size; ++i, ++x) {
        if (x >= cursor_row_byte_width) {
            x = 0;
            y += row_byte_width;
        }
        data[i] = source_white[y + x];
        mask[i] = source_white[y + x] | source_black[y + x];
    }
    SDL_Cursor *cursor = SDL_CreateCursor(data, mask, 16, sdl_cursor_height, 0, 0);

    if (cursor == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create cursor: %s", SDL_GetError());
        return NULL;
    }

    return cursor;
}

/* Allows creation of sub-regions of textures without needing to copy
 * to a new texture.
 */
TEXTURE *texture_create_child(TEXTURE *src_texture, SDL_Rect rect)
{
    assert(rect.x + rect.w <= src_texture->rect.w);
    assert(rect.y + rect.h <= src_texture->rect.h);

    TEXTURE *new_texture = malloc(sizeof(TEXTURE));
    if (new_texture == NULL) {
        return NULL;
    }

    new_texture->sdl_texture = src_texture->sdl_texture;
    new_texture->rect.x = src_texture->rect.x + rect.x;
    new_texture->rect.y = src_texture->rect.y + rect.y;
    new_texture->rect.h = rect.h;
    new_texture->rect.w = rect.w;
    new_texture->orig = 0;

    return new_texture;
}

void texture_destroy(TEXTURE *texture)
{
    if (texture->orig) {
        SDL_DestroyTexture(texture->sdl_texture);
    }
    free(texture);
}

static SDL_Texture *create_empty_target_texture(int width, int height) {
    SDL_Texture *sdl_texture = SDL_CreateTexture(sdl_renderer, preferred_pixel_format, SDL_TEXTUREACCESS_TARGET, width, height);
    if (sdl_texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create dynamic texture: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode on texture: %s", SDL_GetError());
        return NULL;
    }

    return sdl_texture;
}

TEXTURE *texture_create_empty(int width, int height)
{
    SDL_Texture *sdl_texture = create_empty_target_texture(width, height);
    if (sdl_texture == NULL) {
        return NULL;
    }

    TEXTURE *new_texture = malloc(sizeof(TEXTURE));
    if (new_texture == NULL) {
        return NULL;
    }

    new_texture->sdl_texture = sdl_texture;
    new_texture->orig = 1;
    new_texture->rect.x = 0;
    new_texture->rect.y = 0;
    new_texture->rect.w = width;
    new_texture->rect.h = height;

    return new_texture;
}

TEXTURE *texture_create_from_pixels(void *pixels, int width, int height, int depth)
{
    /* Currently, we only support mono bitmaps */
    assert(depth == SDL_BITSPERPIXEL(static_bitmap_pixel_format));

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, width, height, depth, ((width + 8) * depth - 1) / 8, static_bitmap_pixel_format);
    if (surface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create surface for static texture: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_SetPaletteColors(surface->format->palette, bitmap_palette_colors, 0, 2) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set surface palette colors for static texture: %s", SDL_GetError());
        return NULL;
    }

    SDL_Texture *sdl_texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
    SDL_FreeSurface(surface);

    if (sdl_texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create static texture: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode on texture: %s", SDL_GetError());
        return NULL;
    }

    TEXTURE *new_texture = malloc(sizeof(TEXTURE));
    if (new_texture == NULL) {
        return NULL;
    }

    new_texture->sdl_texture = sdl_texture;
    new_texture->orig = 1;
    new_texture->rect.x = 0;
    new_texture->rect.y = 0;
    new_texture->rect.w = width;
    new_texture->rect.h = height;
}

void texture_fill_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color)
{
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    rect.x += texture->rect.x;
    rect.y += texture->rect.y;
    SDL_RenderFillRect(sdl_renderer, &rect);
}

void texture_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color, int line_width)
{
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    rect.x += texture->rect.x;
    rect.y += texture->rect.y;
    if (line_width == 1) {
        SDL_RenderDrawRect(sdl_renderer, &rect);
    } else {
        SDL_Rect rects[] = {
            {.x = rect.x, .y = rect.y, .w = rect.w, .h = line_width},
            {.x = rect.x + rect.w - line_width, .y = rect.y + line_width, .w = line_width, .h = rect.h - 2 * line_width},
            {.x = rect.x, .y = rect.y + rect.h - line_width, .w = rect.w, .h = line_width},
            {.x = rect.x, .y = rect.y + line_width, .w = line_width, .h = rect.h - 2 * line_width},
        };
        SDL_RenderFillRects(sdl_renderer, rects, 4);
    }
}

void texture_point(TEXTURE *texture, SDL_Point point, SDL_Color color)
{
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawPoint(sdl_renderer, texture->rect.x + point.x, texture->rect.y + point.y);
}

void texture_line(TEXTURE *texture, SDL_Point start, SDL_Point end, SDL_Color color)
{
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(sdl_renderer, texture->rect.x + start.x, texture->rect.y + start.y, texture->rect.x + end.x, texture->rect.y + end.y);
}

void texture_copy(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color)
{
    assert(src_texture != dst_texture);

    SDL_Rect dst_rect = {
        .x = dst_texture->rect.x + point.x, .y = dst_texture->rect.y + point.y,
        .w = src_texture->rect.w, .h = src_texture->rect.h,
    };

    SDL_SetTextureColorMod(src_texture->sdl_texture, fg_color.r, fg_color.g, fg_color.b);
    SDL_SetTextureAlphaMod(src_texture->sdl_texture, fg_color.a);
    SDL_SetRenderTarget(sdl_renderer, dst_texture->sdl_texture);
    SDL_RenderCopy(sdl_renderer, src_texture->sdl_texture, &(src_texture->rect), &dst_rect);
}

void texture_scroll(TEXTURE *texture, SDL_Color bg_color, int start, int delta) {
    /* Copy textures around for vertical scrolling effects.
     *
     * Now we're using textures, really scrolling should be implementing by replacing
     * the texture rather than doing this dance. However, because our 'interesting'
     * TEXTURE struct, a single sdl_texture could be referred to multiple times,
     * and we have no current way of tracking down the other references.
     *
     * Note that it appears to work if we copy the source texture _over_
     * the destination texture as long as dst_rect.y > src_rect.y, but this
     * optimisation is not endorsed by the documentation, so we abandon
     * it for now...
     */

    assert(delta != 0);

    int abs_delta = abs(delta);

    /* src_rect -> temp_rect -> texture_target_rect; bg_color -> fill_rect */
    SDL_Rect src_rect = texture->rect;        /* Bit of original texture to keep */
    src_rect.y += start;
    src_rect.h -= start;

    /* If we're scrolling more than the height, we can just blank it out. */
    if (abs_delta >= src_rect.h) {
        texture_fill_rect(texture, src_rect, bg_color);
        return;
    }

    SDL_Rect texture_target_rect = src_rect;  /* Where to copy new texture */
    SDL_Rect fill_target_rect = src_rect;     /* Where to blank out scrolled lines */

    src_rect.h -= abs_delta;  /* No point copying the part of original rect lost to scroll */
    texture_target_rect.h -= abs_delta;  /* Similarly, when we copy it back we don't need this */
    fill_target_rect.h = abs_delta;  /* Because we _fill_ this height using DrawRect */

    if (delta > 0) { /* If we're scrolling down... */
        src_rect.y += abs_delta;  /* Need to pull the lines from further down */
        fill_target_rect.y += src_rect.h;  /* And the background fill is going to be at the end */
    } else if (delta < 0) {  /* If we're scrolling up... */
        texture_target_rect.y += src_rect.h;  /* Need to copy the lines further down */
    }

    /* Copy scrollable region to temporary texture */
    SDL_Rect temp_rect = {.x = 0, .y = 0, .w = src_rect.w, .h = src_rect.h};
    SDL_Texture *temp_sdl_texture = create_empty_target_texture(temp_rect.w, temp_rect.h);
    SDL_SetRenderTarget(sdl_renderer, temp_sdl_texture);
    SDL_SetTextureAlphaMod(texture->sdl_texture, SDL_ALPHA_OPAQUE);
    SDL_SetTextureColorMod(texture->sdl_texture, 0xFF, 0xFF, 0xFF);
    SDL_RenderCopy(sdl_renderer, texture->sdl_texture, &src_rect, &temp_rect);

    /* Copy back onto original texture */
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetTextureAlphaMod(temp_sdl_texture, SDL_ALPHA_OPAQUE);
    SDL_SetTextureColorMod(temp_sdl_texture, 0xFF, 0xFF, 0xFF);
    SDL_RenderCopy(sdl_renderer, temp_sdl_texture, &temp_rect, &texture_target_rect);

    /* Fill in 'new' area with the background colour */
    SDL_SetRenderDrawColor(sdl_renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderFillRect(sdl_renderer, &fill_target_rect);

    SDL_DestroyTexture(temp_sdl_texture);
}

void texture_copy_withbg(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color, SDL_Color bg_color)
{
    SDL_Rect dst_rect = {
        .x = point.x, .y = point.y,
        .w = src_texture->rect.w, .h = src_texture->rect.h,
    };
    texture_fill_rect(dst_texture, dst_rect, bg_color);
    texture_copy(dst_texture, point, src_texture, fg_color);
}
