#include "screen.h"

int bit_size(int wide, int high, unsigned char depth)
{
  return BIT_Size(wide,high,depth);
}

/* stub palette handling routines */

/* returns the color index in the color lookup table of the foreground */
unsigned int fg_color_idx( void){ return 1;}

void
setpalette(BITMAP *bp,
	   unsigned int index,
           unsigned int red,
           unsigned int green,
           unsigned int blue,
           unsigned int maxi)
{ }


void
getpalette(BITMAP *bp,
	   unsigned int index,
           unsigned int *red,
           unsigned int *green,
           unsigned int *blue,
           unsigned int *maxi)
{
    *red = *green = *blue = 0;
    *maxi = 1;
}

