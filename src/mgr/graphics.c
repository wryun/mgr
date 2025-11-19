/* Light wrappers around SDL2 to make our normal usage a bit simpler
 * and closer to how mgr thinks about the world.
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
 * (i.e. SDL_Rect, SDL_Point, SDL_Color) leak into the main codebase
 * to avoid unnecessary copying or type shenanigans. Since we're so heavily
 * tied to SDL now (cf event loop), I'm ok with this.
 */

#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <SDL2/SDL.h>

#include "graphics.h"
#include "bitmap.h"
#include "defs.h"


/* Wrapper around an SDL texture that allows us to:
 *  - have easy access to its dimensions
 *  - potentially have a 'child' texture (see texture_create_child)
 *    which points to a sub-rectangle (set orig = 0)
 *  - note that we assume if you texture_destroy when orig = 1,
 *    you've cleaned up the children (we don't ref count).
 *
 * It's private to this file because users really shouldn't care
 * about orig or the 'actual' x/y of rect. To get the h/w of a texture,
 * use texture_get_rect().
 */
typedef struct texture {
    SDL_Texture *sdl_texture;
    SDL_Rect rect;
    int orig;
} TEXTURE;


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
 * is the menu system, which draws on top of the windows and has its own event loop.
 *
 * We'd probably see a significant performance bump without this.
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

static SDL_Surface *surface_load_from_icon(const char *iconpath)
{
    char path[PATH_MAX];
    sprintf(path, "%s/%s", icon_dir, iconpath);
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    SDL_Surface *s = bitmapread(f);
    fclose(f);
    return s;
}

void cursor_warp(SDL_Point point)
{
    SDL_WarpMouseInWindow(sdl_window, point.x, point.y);
}

SDL_Cursor *cursor_create_from_icon(char *iconpath)
{
    SDL_Surface *surface = surface_load_from_icon(iconpath);
    if (surface == NULL) {
        return NULL;
    }

    /* Could just user CreateColorCursor... */
    if (surface->format->format != static_bitmap_pixel_format) {
        SDL_FreeSurface(surface);
        return NULL;
    }

    int depth = 1;
    const size_t sdl_cursor_height = (surface->h < 16) ? surface->h : 16;
    const size_t sdl_cursor_size = sdl_cursor_height * 16 / 8;
    int row_byte_width = surface->w * depth / 8;
    int cursor_row_byte_width = 16 * depth / 8;
    Uint8 data[sdl_cursor_size];
    Uint8 mask[sdl_cursor_size];
    Uint8 *source_white = (Uint8 *)surface->pixels;
    Uint8 *source_black = source_white;

    if (surface->h>= 32) {
        source_black = &(((Uint8 *)surface->pixels)[16 * row_byte_width]);
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

static SDL_Texture *create_empty_target_sdl_texture(int width, int height) {
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

static TEXTURE *texture_create(SDL_Texture *sdl_texture, int width, int height)
{
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

TEXTURE *texture_clone(TEXTURE *src_texture) {
    TEXTURE *dst_texture = texture_create_empty(texture->rect.w, texture->rect.h);
    if (!dst_texture) {
        return NULL;
    }
    SDL_Point p = {.x = 0, .y = 0};
    texture_copy(dst_texture, p, src_texture, C_WHITE);
    return dst_texture;
}

TEXTURE *texture_create_empty(int width, int height)
{
    SDL_Texture *sdl_texture = create_empty_target_sdl_texture(width, height);
    if (sdl_texture == NULL) {
        return NULL;
    }

    return texture_create(sdl_texture, width, height);
}

static TEXTURE *texture_create_from_surface(SDL_Surface *surface)
{
    SDL_Texture *sdl_texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);

    if (sdl_texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create static texture: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode on texture: %s", SDL_GetError());
        return NULL;
    }

    return texture_create(sdl_texture, surface->w, surface->h);
}

TEXTURE *texture_create_from_icon(const char *iconpath)
{
    SDL_Surface *surface = surface_load_from_icon(iconpath);
    if (surface == NULL) {
        return NULL;
    }

    TEXTURE *texture = texture_create_from_surface(surface);
    SDL_FreeSurface(surface);
    return texture;
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

    TEXTURE *texture = texture_create_from_surface(surface);
    SDL_FreeSurface(surface);
    return texture;
}


/* Expose width/height of our texture, but don't leak the 'internal' x/y.
 * That way madness lies...
 */
SDL_Rect texture_get_rect(TEXTURE *texture) {
    SDL_Rect rect = {
        .x = 0, .y = 0,
        .w = texture->rect.w, .h = texture->rect.h,
    };
    return rect;
}

void texture_fill_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color)
{
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    rect.x += texture->rect.x;
    rect.y += texture->rect.y;
    SDL_RenderFillRect(sdl_renderer, &rect);
}

void texture_clear(TEXTURE *texture, SDL_Color color) {
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(sdl_renderer, &texture->rect);
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

/* Move part of a texture, leaving 'empty' space set to bg_color.
 *
 * This is useful for add/del line (i.e. vert scroll) and add/del char (i.e. horiz scroll).
 *
 * 'src' is the whole region in which we're scrolling, and defines the clip extent.
 *
 * For the normal scroll (i.e. just new chars at end) where we move the whole texture,
 * it would definitely be nicer to just create a new texture and call it a day. However,
 * because of our 'creative' approach to TEXTURE, we can't chase down all the references.
 */
void texture_scroll(TEXTURE *texture, SDL_Rect region, int delta_x, int delta_y, SDL_Color bg_color)
{
    /* Makes no sense to attempt a scroll which does nothing. */
    assert(!(delta_x == 0 && delta_y == 0));
    region.x += texture->rect.x;
    region.y += texture->rect.y;

#if 0
    /* Validate that our source rect is inside texture */
    SDL_Rect clipped_src;
    SDL_bool valid_intersection = SDL_IntersectRect(&texture->rect, &region, &clipped_region);
    assert(valid_intersection);
    assert(SDL_RectEquals(&region, &clipped_region);
#endif

    /* Figure out where on the texture we're copying to. */
    SDL_Rect texture_target = region;
    texture_target.x += delta_x;
    texture_target.y += delta_y;
    SDL_bool intersection = SDL_IntersectRect(&texture_target, &region, &texture_target);
    if (!intersection) {
        /* Our 'target' location is entirely outside our scroll location.
         * So all we need to do is fill in the 'new' area with the background colour.
         */
        SDL_SetRenderDrawColor(sdl_renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        SDL_RenderFillRect(sdl_renderer, &region);
        return;
    }

    /* And do the opposite to decide where to copy from... */
    SDL_Rect texture_src = region;
    texture_src.x -= delta_x;
    texture_src.y -= delta_y;
    /* Since this is just the inverse of the above, intersection is guaranteed (?) */
    assert(SDL_IntersectRect(&texture_src, &region, &texture_src));

    /* Copy texture_src (bit of region to preserve) to temporary texture */
    SDL_Rect temp_rect = {.x = 0, .y = 0, .w = texture_src.w, .h = texture_src.h};
    SDL_Texture *temp_sdl_texture = create_empty_target_sdl_texture(temp_rect.w, temp_rect.h);
    SDL_SetRenderTarget(sdl_renderer, temp_sdl_texture);
    SDL_SetTextureAlphaMod(texture->sdl_texture, SDL_ALPHA_OPAQUE);
    SDL_SetTextureColorMod(texture->sdl_texture, 0xFF, 0xFF, 0xFF);
    SDL_RenderCopy(sdl_renderer, texture->sdl_texture, &texture_src, &temp_rect);

    /* Fill the whole thing with the background colour; easier than calculating rectangles... */
    SDL_SetRenderDrawColor(sdl_renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderFillRect(sdl_renderer, &region);

    /* Copy back onto original texture */
    SDL_SetRenderTarget(sdl_renderer, texture->sdl_texture);
    SDL_RenderCopy(sdl_renderer, temp_sdl_texture, &temp_rect, &texture_target);

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


#define LOAD_ICON(var) {var = texture_create_from_icon("server/" #var); if (!var) return 0;}
#define LOAD_CURSOR(var) {var = cursor_create_from_icon("server/" #var); if (!var) return 0;}
TEXTURE *def_pattern;
SDL_Cursor *mouse_arrow;
SDL_Cursor *mouse_box;
SDL_Cursor *mouse_bull;
SDL_Cursor *mouse_bull2;
SDL_Cursor *mouse_cross;
SDL_Cursor *mouse_cup;
SDL_Cursor *mouse_cut;
int load_server_icons() {
    LOAD_ICON(def_pattern);
    LOAD_CURSOR(mouse_arrow);
    LOAD_CURSOR(mouse_box);
    LOAD_CURSOR(mouse_bull);
    LOAD_CURSOR(mouse_bull2);
    LOAD_CURSOR(mouse_cross);
    LOAD_CURSOR(mouse_cup);
    LOAD_CURSOR(mouse_cut);
    return 1;
}
