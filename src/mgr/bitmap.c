/*                        Copyright (c) 1988 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* These functions are copied from the old libbitblit, and help us deal
 * with the standard mgr icon formats.
 *
 * Probably everything could be converted to PNGs or similar and
 * this code could be removed...
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include <mgr/mgr.h>

#define LOGBITS 3
#define BITS (~(~(unsigned)0<<LOGBITS))
#define bit_linesize(wide,depth) ((((depth)*(wide)+BITS)&~BITS)>>3)

/*	Read the header of a bitmap (aka icon) file using the given FILE
        pointer, fp.
        Return 0 if the file isn't in bitmap format or is unreadable.
        Otherwise, return "true" (non-zero) and populate the integers
        pointed at by:
                wp	width of the bitmap in bits
                hp	height of bitmap in bits
                dp	depth of bitmap in bits
                size1p	number of bytes in a single line (including padding)
                bm_compressedp whether its compressed (RLE - see squeeze/unsqueeze)
 */

static int bitmaphead(FILE *fp, int *wp, int *hp, unsigned char *dp, int *size1p, int *bm_compressedp)
{
    struct b_header head;

    if (fread((char *)&head, sizeof(struct old_b_header), 1, fp ) != 1) {
        return 0;
    }

    if (BS_ISHDR( &head )) {    /* compressed bitmaps */
        *bm_compressedp = 1;
        /* fprintf(stderr,"Got compressed header\n"); */
        head.magic[1] = 'z';
    } else {
        *bm_compressedp = 0;
    }

    if (B_ISHDR8(&head)) {
        /* modern, self-describing
           bitmap, 8-bit alignment */
        if (fread(&head.depth, sizeof head - sizeof(struct old_b_header), 1, fp) != 1) {
            return 0;
        }

        B_GETHDR8(&head, *wp, *hp, *dp);
        *size1p = B_SIZE8(*wp, 1, *dp);
    } else if (B_ISHDR32( &head )) {    /* 1 bit deep, 32 bits align */
        B_GETOLDHDR(&head, *wp, *hp);
        *size1p = B_SIZE32(*wp, 1, 1);
        *dp = 1;
    } else if (B_ISHDR16(&head)) {      /* 1 bit deep, 16 bits align */
        B_GETOLDHDR(&head, *wp, *hp );
        *size1p = B_SIZE16(*wp, 1, 1);
        *dp = 1;
    } else if (B8_ISHDR(&head)) {       /* 8 bits deep, 16 bits align */
        B_GETOLDHDR(&head, *wp, *hp);
        *size1p = B8_SIZE(*wp, 1);
        *dp = 8;
    } else {
        return 0;
    }

    return 1;
}


static int b_compget(char *datap, int bcount, int bct1, FILE *fp)
{/* reads bcount*bct1 compressed bytes, into datap, from file fp */
/* byte runlength  encoding scheme  is easy to
   compact/uncompact.  Something like:
   <control_byte>[data]<control_byte>[data]...
   where:
   0<control_byte<128: repeat the next byte <control_byte> times
   128<=contropl_byte<256: the next (control_byte-127) bytes are sent
   "as is".  */
/* magic number is 'y' 'x' instead of 'y' 'z' */

    register int kb, c;
    static int repn = 0, cr = 0;
    kb = bcount * bct1;

    while (kb-- > 0) {
        if (repn == 0) {
            repn = getc(fp);

            if (repn == EOF) {
                repn = 0;

                return(0);
            }

            repn &= 0377;

            if (repn < 128) {
                cr = getc(fp);
            }
        }

        if (repn >= 128) {
            *datap++ = c = getc(fp);

            if (--repn == 127) {
                repn = 0;
            }

            if (c == EOF) {
                repn = 0; return(0);
            }
        } else {
            *datap++ = cr;
            repn--;
        }
    }

    return(1);
}

SDL_Surface *bitmapread(FILE *fp)
{
    SDL_Surface *surface;
    char *datap;
    int h, w;
    unsigned char d;
    int sizefile1; /* the size of 1 line of the bitmap as stored in a file, in bytes */
    int sizemem1; /* the size of 1 line of the bitmap as stored in memory, in bytes */
    int size1diff = 0; /* if the file padding is greater than the memory padding, the difference in bytes */
    int bm_compressed = 0;

    if (!bitmaphead(fp, &w, &h, &d, &sizefile1, &bm_compressed)) {
        return NULL;
    }

    sizemem1 = bit_linesize(w, d);

    if (sizefile1 > sizemem1) {
        size1diff = sizefile1 - sizemem1;
        sizefile1 = sizemem1;
    }

    /* TODO */
    if (d != 1) {
        return NULL;
    }
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, d, SDL_PIXELFORMAT_INDEX1MSB);
    if (!surface) {
        return NULL;
    }

    const SDL_Color bitmap_palette_colors[] = {
        {0x00, 0x00, 0x00, SDL_ALPHA_TRANSPARENT},
        {0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE},
    };

    if (SDL_SetPaletteColors(surface->format->palette, bitmap_palette_colors, 0, 2) < 0) {
        return NULL;
    }

    datap = (char *)surface->pixels;

    /*
       The bytes of the bitmap data in the file may have different
       alignments than the bitmap data in memory.  We read one line at a
       time in such a way as to get the memory alignment needed.
     */
    if (bm_compressed == 1) {
        while (h-- > 0) {
            if (b_compget(datap, sizefile1, 1, fp) != 1) {
                SDL_FreeSurface(surface);
                return NULL;
            }

            if (size1diff) {
                b_compget(datap, size1diff, 1, fp);
            }

            datap += sizemem1;
        }
    } else {
        while (h-- > 0) {
            if (fread(datap, sizefile1, 1, fp) != 1) {
                SDL_FreeSurface(surface);
                return NULL;
            }

            if (size1diff) {
                fseek(fp, size1diff, 1);
            }

            datap += sizemem1;
        }
    }

    return surface;
}


/*
        Write a bitmap.
        Given an open FILE pointer to an file and a pointer to a bitmap,
        write the header and the bitmap.  Return 0 on failure, positive on
        success.
 */
int bitmapwrite(FILE *fp, SDL_Surface *surface)
{
    char *datap;
    int w, h;
    unsigned char d;
    struct b_header head;
    int sizefile1; /* the size of 1 line of the bitmap as stored in a file, in bytes */
    int sizemem1; /* the size of 1 line of the bitmap as stored in memory, in bytes */

    w = surface->w;
    h = surface->h;
    d = surface->pitch;
    B_PUTHDR8(&head, w, h, d);
    sizefile1 = B_SIZE8(w, 1, d);

    if (fwrite((char *)&head, sizeof head, 1, fp) != 1) {
        return 0;
    }

    sizemem1 = bit_linesize(w, d);
    datap = (char *)surface->pixels;

    while (h-- > 0) {
        if (fwrite(datap, sizefile1, 1, fp) != 1) {
            return 0;
        }

        datap += sizemem1;
    }

    return 1;
}
