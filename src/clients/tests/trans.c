/*{{{}}}*/
/*{{{  #includes*/
#include <mgr/mgr.h>
/*}}}  */

/*{{{  bitmap for "frog1", 48 wide, 34 high, 1 bit deep*/
char frog1_data[205] = {
	0016, 0000, 0000, 0000, 0000, 0160, 0021, 0200, 0000, 0000,
	0001, 0210, 0040, 0100, 0000, 0000, 0002, 0004, 0100, 0040,
	0000, 0000, 0004, 0002, 0100, 0020, 0000, 0000, 0010, 0002,
	0200, 0020, 0000, 0000, 0010, 0001, 0200, 0010, 0074, 0170,
	0020, 0001, 0200, 0004, 0102, 0204, 0040, 0001, 0200, 0004,
	0231, 0062, 0040, 0001, 0200, 0002, 0265, 0152, 0100, 0001,
	0200, 0017, 0275, 0172, 0100, 0001, 0200, 0060, 0231, 0063,
	0300, 0001, 0101, 0300, 0102, 0204, 0077, 0002, 0102, 0000,
	0074, 0170, 0000, 0302, 0114, 0040, 0000, 0000, 0020, 0042,
	0120, 0100, 0000, 0000, 0010, 0022, 0120, 0340, 0000, 0000,
	0024, 0022, 0120, 0237, 0340, 0000, 0144, 0022, 0060, 0000,
	0037, 0377, 0200, 0024, 0056, 0000, 0000, 0000, 0000, 0144,
	0041, 0370, 0000, 0000, 0001, 0204, 0020, 0007, 0000, 0000,
	0006, 0010, 0010, 0000, 0300, 0000, 0170, 0020, 0004, 0010,
	0077, 0377, 0200, 0040, 0002, 0010, 0000, 0000, 0060, 0100,
	0003, 0304, 0010, 0020, 0043, 0300, 0014, 0074, 0010, 0020,
	0074, 0060, 0060, 0006, 0010, 0020, 0040, 0014, 0112, 0105,
	0010, 0020, 0042, 0122, 0222, 0111, 0017, 0360, 0162, 0111,
	0155, 0260, 0202, 0100, 0215, 0266, 0000, 0001, 0022, 0110,
	0200, 0000, 0000, 0002, 0114, 0062, 0100, 0000, 0000, 0001,
	0260, 0015, 0200, 0000, 0000, 
	};
/*}}}  */

/*{{{  main*/
int main(int argc, char *argv[])
{
  char frog1_check[205];
  int i;

  m_setup(M_MODEOK);
  m_ttyset();
  m_func(BIT_SRC);
  m_bitldto(48,34,0,0,1,204);
  fwrite(frog1_data,204,1,m_termout);

  m_bitget(1,204,0);
  m_flush();
  fread(frog1_check,204,1,m_termin);
  m_ttyreset();

  for (i=0; i<204; i++) if (frog1_data[i]!=frog1_check[i])
  {
    fprintf(stderr,"images differ at position %d\n",i);
    return 1;
  }

  return 0;
}
/*}}}  */
