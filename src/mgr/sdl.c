/* Light wrappers around SDL2 to make our normal usage a bit simpler
 * and closer to how mgr used to work.
 *
 * Primarily relies on wrapping SDL_Texture in something which:
 *   - lets us know the height/width without asking
 *   - allows us to create regions that act as new pseudo-textures
 *     (for the purposes of these functions)
 *
 * Also hides our dodgy globals (sdl_window/sdl_renderer) from the rest of the world.
 *
 * We don't properly maintain the abstraction: we let SDL data structures
 * (e.g. SDL_Texture, SDL_Rect, SDL_Point) leak into the main codebase.
 * Since we're so heavily tied to SDL now (cf event loop), I'm ok with this.
 */

#include <stdlib.h>

#include <sdl.h>


static SDL_PixelFormatEnum static_bitmap_pixel_format = SDL_PIXELFORMAT_INDEX1MSB;
static SDL_PixelFormatEnum preferred_pixel_format = SDL_PIXELFORMAT_ABGR8888;
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

    SDL_PixelFormatEnum pf = SDL_GetWindowPixelFormat(window);

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
    SDL_RenderCopy(sdl_renderer, screen_texture, NULL, NULL);
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

SDL_Cursor *cursor_create(void *pixels, int wide, int high, int depth)
{
    const size_t sdl_cursor_height = (high < 16) ? high : 16;
    const size_t sdl_cursor_size = sdl_cursor_height * 16 / 8;
    int row_byte_width = wide * depth / 8;
    int cursor_row_byte_width = 16 * depth / 8;
    Uint8 data[sdl_cursor_size];
    Uint8 mask[sdl_cursor_size];
    Uint8 *source_white = (Uint8 *)pixels;
    Uint8 *source_black = source_white;
  
    if (high >= 32) {
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
    assert(rect.x + rect.w <= texture.rect.w);
    assert(rect.y + rect.h <= texture.rect.h);

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

TEXTURE *texture_create_empty(width, height)
{
    SDL_Texture *sdl_texture = SDL_CreateTexture(renderer, preferred_pixel_format, SDL_TEXTUREACCESS_TARGET, width, height);
    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create dynamic texture: %s", SDL_GetError());
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
    new_texture->orig = 0;
    new_texture->rect.x = 0;
    new_texture->rect.y = 0;
    new_texture->rect.w = width;
    new_texture->rect.h = height;

    return new_texture;
}

TEXTURE *texture_create_from_pixels(void *pixels, int wide, int high, int depth)
{
    /* Currently, we only support mono bitmaps */
    assert(depth == SDL_BITSPERPIXEL(static_bitmap_pixel_format));

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, wide, high, depth, ((wide + 8) * depth - 1) / 8, static_bitmap_pixel_format);
    if (surface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create surface for static texture: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_SetPaletteColors(surface->format->palette, bitmap_palette_colors, 0, 2) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set surface palette colors for static texture: %s", SDL_GetError());
        return NULL;
    }

    SDL_Texture *sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
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
    new_texture->orig = 0;
    new_texture->rect.x = 0;
    new_texture->rect.y = 0;
    new_texture->rect.w = width;
    new_texture->rect.h = height;
}

void texture_fill_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color)
{
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color->r, color->g, color->b, color->a);
    rect.x += texture->rect.x;
    rect.y += texture->rect.y;
    SDL_RenderFillRect(sdl_renderer, &rect);
}

void texture_point(TEXTURE *texture, SDL_Point point, SDL_Color color) {
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color->r, color->g, color->b, color->a);
    SDL_RenderDrawPoint(sdl_renderer, texture->rect.x + point.x, texture->rect.y + point.y);
}

void texture_line(TEXTURE *texture, SDL_Point start, SDL_Point end, SDL_Color color) {
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color->r, color->g, color->b, color->a);
    SDL_RenderDrawLine(sdl_renderer, texture->rect.x + start.x, texture->rect.y + start.y, texture->rect.x + end.x, texture->rect.y + end.y);
}

void texture_copy(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color)
{
    SDL_Rect dst_rect = {
        .x = dst_texture->rect.x + point.x, .y = dst_texture->rect.y + point.y,
        .w = src_texture->rect.w, .h = src_texture->rect.h,
    };
    SDL_Rect src_rect = src_sdl_texture->rect;
    SDL_Texture *src_sdl_texture = src_texture->sdl_texture;
    SDL_Texture *new_src_sdl_texture = NULL;
    /* TODO: This nonsense is probably only needed for scrolling, which should be implemented differently
     * anyway I suspect... (i.e. just create a _new_ texture and replace the existing one).
     * Also has dodgy 'works but not in API' check for y coordinate. Replace with an assert?
     */
    if (src_sdl_texture == dst_texture->sdl_texture && dst_rect.y > src_rect.y) {
      new_src_sdl_texture = sdl_create_texture_target(sdl_renderer, src_rect.w, src_rect.h);
      SDL_SetTextureAlphaMod(src_sdl_texture, SDL_ALPHA_OPAQUE);
      SDL_SetTextureColorMod(src_sdl_texture, 0xFF, 0xFF, 0xFF);
      SDL_SetRenderTarget(sdl_renderer, new_src_sdl_texture);
      SDL_Rect new_src_rect = {.x = 0, .y = 0, .w = src_rect.w, .h = src_rect.h};
      SDL_RenderCopy(sdl_renderer, src_sdl_texture, &src_rect, &new_src_rect);

      src_sdl_texture = new_src_texture;
      src_rect = *new_src_rect;
    }

    SDL_SetTextureColorMod(src_texture, color->r, color->g, color->b);
    SDL_SetTextureAlphaMod(src_texture, color->a);
    SDL_SetRenderTarget(sdl_renderer, dst_texture);
    SDL_RenderCopy(sdl_renderer, src_texture, &src_rect, &dst_rect);

    if (new_src_texture != NULL) {
      SDL_DestroyTexture(new_src_texture);
    }
}

void texture_copy_withbg(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color, SDL_Color bg_color)
{
    SDL_Rect dst_rect = {
        .x = dst_texture->rect.x + point.x, .y = dst_texture->rect.y + point.y,
        .w = src_texture->rect.w, .h = src_texture->rect.h,
    };
    texture_fill_rect(dst_texture, dst_rect, bg_color);
    texture_copy(dst_texture, point, src_texture, fg_color);
}
