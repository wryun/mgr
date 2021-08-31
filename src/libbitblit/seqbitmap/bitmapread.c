/*{{{}}}*/
/*{{{  Notes*/
/*                        Copyright (c) 1988 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
*/
/*}}}  */
/*{{{  #includes*/
#include "screen.h"
#include <mgr/share.h>
#include <stdlib.h>
#include <stdio.h>
/*}}}  */

/*{{{  variables*/
extern int bm_compressed;
/*}}}  */
/*{{{  b_compget*/
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
  static int repn=0, cr=0;
  kb=bcount*bct1;

  while (kb-- >0)
  {
    if (repn==0)
    {
      repn = getc(fp);
      if (repn==EOF)
      {
        repn=0;
        return(0);
      }
      repn&=0377;
      if (repn<128)
      cr=getc(fp);
    }
    if (repn>=128)
    {
      *datap++ = c =getc(fp);
      if (--repn == 127) repn=0;
      if (c==EOF)
      {repn=0; return(0);}
    }
    else
    {
      *datap++= cr;
      repn--;
    }
  }
  return(1);
}
/*}}}  */

/*{{{  bit_alloc -- allocate space for, and create a memory bitmap*/
BITMAP *bit_fake_alloc(int wide, int high, DATA *data, unsigned char depth)
{
  register BITMAP *result;
  register int size;

  if ((result=(BITMAP*)malloc(sizeof(BITMAP)))==(BITMAP*)0) return (result);

  result->x0=0;
  result->y0=0;
  result->high=high;
  result->wide=wide;
  result->depth=depth;
  result->cache=NULL;
  result->color=0;

  size=bit_size(wide,high,depth);

  if (data != (DATA *) 0)
  {
    result->data = data;
    /* convert from external to internal format (if required) */
  }
  else
  {
    if ((result->data = (DATA *) malloc(size)) == (DATA *) 0)
    {
      free(result);
      return ((BITMAP *) 0);
    }
  }

  result->primary = result;
  result->type = _MEMORY;
  result->id = 0;	/* assign elsewhere? */
  result->deviceinfo = NULL;
  return (result);
}
/*}}}  */

/*{{{  bitmapread*/
BITMAP *bitmapread(FILE *fp)
{
  BITMAP *bp = 0;
  char *datap;
  int h,w;
  unsigned char d;
  int sizefile1; /* the size of 1 line of the bitmap as stored in a file, in bytes */
  int sizemem1; /* the size of 1 line of the bitmap as stored in memory, in bytes */
  int size1diff=0; /* if the file padding is greater than the memory padding, the difference in bytes */

  if (bitmaphead(fp,&w,&h,&d,&sizefile1))
  {
    sizemem1=bit_linesize(w,d);
    if (sizefile1 > sizemem1 )
    {
      size1diff = sizefile1 - sizemem1;
      sizefile1 = sizemem1;
    }
    if ((bp=bit_fake_alloc(w,h,(DATA*)0,d))==(BITMAP*)0) return (BITMAP*)0;
#ifdef MOVIE
    SET_DIRTY(bp);
#endif
    datap=(char*)BIT_DATA(bp);

    /*{{{  Notes*/
    /*
    The bytes of the bitmap data in the file may have different
    alignments than the bitmap data in memory.  We read one line at a
    time in such a way as to get the memory alignment needed.
    */
    /*}}}  */
    if (bm_compressed==1)
    {
      while (h-->0)
      {
        if (b_compget(datap, sizefile1, 1, fp)!=1)
        {
          bit_destroy(bp);
          return (BITMAP*)0;
        }
        if (size1diff) b_compget(datap,size1diff,1,fp);
        datap+=sizemem1;
      }
    }
    else while (h-->0)
    {
      if (fread(datap,sizefile1,1,fp)!=1)
      {
        bit_destroy(bp);
        return (BITMAP*)0;
      }
      if (size1diff) fseek(fp,size1diff,1);
      datap+=sizemem1;
    }
  }
  return bp;
}
/*}}}  */


