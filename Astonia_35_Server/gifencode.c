/* @(#)root/win32gdk:$Name:  $:$Id: gifencode.c,v 1.2 2007/08/13 18:52:01 devel Exp $ */
/* Author: E.Chernyaev   19/01/94*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gifencode.h"

#ifdef __STDC__
#define ARGS(alist) alist
#else
#define ARGS(alist) ()
#endif

#define BITS     12                     /* largest code size */
#define THELIMIT 4096                   /* NEVER generate this */
#define HSIZE    5003                   /* hash table size */
#define SHIFT    4                      /* shift for hashing */

#define put_byte(A) (*put_b)((byte)(A)); Nbyte++

typedef unsigned char byte;

static long     HashTab [HSIZE];        /* hash table */
static int      CodeTab [HSIZE];        /* code table */

static int      BitsPixel,              /* number of bits per pixel */
                IniCodeSize,            /* initial number of bits per code */
                CurCodeSize,            /* current number of bits per code */
                CurMaxCode,             /* maximum code, given CurCodeSize */
                ClearCode,              /* reset code */
                EOFCode,                /* end of file code */
                FreeCode;               /* first unused entry */

static long      Nbyte;
static void     (*put_b) ARGS((byte));

static void     output ARGS((int));
static void     char_init();
static void     char_out ARGS((int));
static void     char_flush();
static void     put_short ARGS((int));

/***********************************************************************
 *                                                                     *
 * Name: GIFencode                                   Date:    02.10.92 *
 * Author: E.Chernyaev (IHEP/Protvino)               Revised:          *
 *                                                                     *
 * Function: GIF compression of the image                              *
 *                                                                     *
 * Input: Width      - image width  (must be >= 8)                     *
 *        Height     - image height (must be >= 8)                     *
 *        Ncol       - number of colors                                *
 *        R[]        - red components                                  *
 *        G[]        - green components                                *
 *        B[]        - blue components                                 *
 *        ScLine[]   - array for scan line (byte per pixel)            *
 *        get_scline - user routine to read scan line:                 *
 *                       get_scline(y, Width, ScLine)                  *
 *        pb         - user routine for "put_byte": pb(b)              *
 *                                                                     *
 * Return: size of GIF                                                 *
 *                                                                     *
 ***********************************************************************/
long GIFencode(Width, Height, Ncol, R, G, B, ScLine, get_scline, pb)
          int  Width, Height, Ncol;
          byte R[], G[], B[], ScLine[];
          void (*get_scline) ARGS((int, int, byte *)), (*pb) ARGS((byte));
{
  long          CodeK;
  int           ncol, i, x, y, disp, Code, K;

  /*   C H E C K   P A R A M E T E R S   */

  Code = 0;
  if (Width <= 0 || Width > 4096 || Height <= 0 || Height > 4096) {
    fprintf(stderr,
            "\nGIFencode: incorrect image size: %d x %d\n", Width, Height);
    return 0;
  }

  if (Ncol <= 0 || Ncol > 256) {
    fprintf(stderr,"\nGIFencode: wrong number of colors: %d\n", Ncol);
    return 0;
  }

  /*   I N I T I A L I S A T I O N   */

  put_b  = pb;
  Nbyte  = 0;
  char_init();                          /* initialise "char_..." routines */

  /*   F I N D   #   O F   B I T S   P E R    P I X E L   */

  BitsPixel = 1;
  if (Ncol > 2)   BitsPixel = 2;
  if (Ncol > 4)   BitsPixel = 3;
  if (Ncol > 8)   BitsPixel = 4;
  if (Ncol > 16)  BitsPixel = 5;
  if (Ncol > 32)  BitsPixel = 6;
  if (Ncol > 64)  BitsPixel = 7;
  if (Ncol > 128) BitsPixel = 8;

  ncol  = 1 << BitsPixel;
  IniCodeSize = BitsPixel;
  if (BitsPixel <= 1) IniCodeSize = 2;

  /*   W R I T E   H E A D E R  */

  put_byte('G');                        /* magic number: GIF87a */
  put_byte('I');
  put_byte('F');
  put_byte('8');
  put_byte('7');
  put_byte('a');

  put_short(Width);                     /* screen size */
  put_short(Height);

  K  = 0x80;                            /* yes, there is a color map */
  K |= (8-1)<<4;                        /* OR in the color resolution */
  K |= (BitsPixel - 1);                 /* OR in the # of bits per pixel */
  put_byte(K);

  put_byte(0);                          /* background color */
  put_byte(0);                          /* future expansion byte */

  for (i=0; i<Ncol; i++) {              /* global colormap */
    put_byte(R[i]);
    put_byte(G[i]);
    put_byte(B[i]);
  }
  for (; i<ncol; i++) {
    put_byte(0);
    put_byte(0);
    put_byte(0);
  }

  put_byte(',');                        /* image separator */
  put_short(0);                         /* left offset of image */
  put_short(0);                         /* top offset of image */
  put_short(Width);                     /* image size */
  put_short(Height);
  put_byte(0);                          /* no local colors, no interlace */
  put_byte(IniCodeSize);                /* initial code size */

  /*   L W Z   C O M P R E S S I O N   */

  CurCodeSize = ++IniCodeSize;
  CurMaxCode  = (1 << (IniCodeSize)) - 1;
  ClearCode   = (1 << (IniCodeSize - 1));
  EOFCode     = ClearCode + 1;
  FreeCode    = ClearCode + 2;
  output(ClearCode);
  for (y=0; y<Height; y++) {
    (*get_scline)(y, Width, ScLine);
    x     = 0;
    if (y == 0)
      Code  = ScLine[x++];
    while(x < Width) {
      K     = ScLine[x++];              /* next symbol */
      CodeK = ((long) K << BITS) + Code;  /* set full code */
      i     = (K << SHIFT) ^ Code;      /* xor hashing */

      if (HashTab[i] == CodeK) {        /* full code found */
        Code = CodeTab[i];
        continue;
      }
      else if (HashTab[i] < 0 )         /* empty slot */
        goto NOMATCH;

      disp  = HSIZE - i;                /* secondary hash */
      if (i == 0) disp = 1;

PROBE:
      if ((i -= disp) < 0)
        i  += HSIZE;

      if (HashTab[i] == CodeK) {        /* full code found */
        Code = CodeTab[i];
        continue;
      }

      if (HashTab[i] > 0)               /* try again */
        goto PROBE;

NOMATCH:
      output(Code);                     /* full code not found */
      Code = K;

      if (FreeCode < THELIMIT) {
        CodeTab[i] = FreeCode++;        /* code -> hashtable */
        HashTab[i] = CodeK;
      }
      else
        output(ClearCode);
    }
  }
   /*   O U T P U T   T H E   R E S T  */

  output(Code);
  output(EOFCode);
  put_byte(0);                          /* zero-length packet (EOF) */
  put_byte(';');                        /* GIF file terminator */

  return (Nbyte);
}

static unsigned long cur_accum;
static int           cur_bits;
static int           a_count;
static char          accum[256];
static unsigned long masks[] = { 0x0000,
                                 0x0001, 0x0003, 0x0007, 0x000F,
                                 0x001F, 0x003F, 0x007F, 0x00FF,
                                 0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

/***************************************************************
 *                                                             *
 * Name: output                                 Date: 02.10.92 *
 *                                                             *
 * Function: output GIF code                                   *
 *                                                             *
 * Input: code - GIF code                                      *
 *                                                             *
 ***************************************************************/
static void output(code)
               int code;
{
  /*   O U T P U T   C O D E   */

   cur_accum &= masks[cur_bits];
   if (cur_bits > 0)
     cur_accum |= ((long)code << cur_bits);
   else
     cur_accum = code;
   cur_bits += CurCodeSize;
   while( cur_bits >= 8 ) {
     char_out( (unsigned int) (cur_accum & 0xFF) );
     cur_accum >>= 8;
     cur_bits -= 8;
   }

  /*   R E S E T   */

  if (code == ClearCode ) {
    memset((char *) HashTab, -1, sizeof(HashTab));
    FreeCode = ClearCode + 2;
    CurCodeSize = IniCodeSize;
    CurMaxCode  = (1 << (IniCodeSize)) - 1;
  }

  /*   I N C R E A S E   C O D E   S I Z E   */

  if (FreeCode > CurMaxCode ) {
      CurCodeSize++;
      if ( CurCodeSize == BITS )
        CurMaxCode = THELIMIT;
      else
        CurMaxCode = (1 << (CurCodeSize)) - 1;
   }

  /*   E N D   O F   F I L E :  write the rest of the buffer  */

  if( code == EOFCode ) {
    while( cur_bits > 0 ) {
      char_out( (unsigned int)(cur_accum & 0xff) );
      cur_accum >>= 8;
      cur_bits -= 8;
    }
    char_flush();
  }
}

static void char_init()
{
   a_count = 0;
   cur_accum = 0;
   cur_bits  = 0;
}

static void char_out(c)
                 int c;
{
   accum[a_count++] = c;
   if (a_count >= 254)
      char_flush();
}

static void char_flush()
{
  int i;

  if (a_count == 0) return;
  put_byte(a_count);
  for (i=0; i<a_count; i++) {
    put_byte(accum[i]);
  }
  a_count = 0;
}

static void put_short(word)
                  int word;
{
  put_byte(word & 0xFF);
  put_byte((word>>8) & 0xFF);
}

static unsigned char *gifimage;
void get_line(int y,int width,unsigned char *dst)
{
	memcpy(dst,gifimage+y*width,width);
}
void make_gif(int width,int height,int ncol,unsigned char *red,unsigned char *green,unsigned char *blue,unsigned char *image)
{
	unsigned char scline[width];
	
	gifimage=image;
	
	printf("Content-Type: image/gif\n\n");
	GIFencode(width,height,ncol,red,green,blue,scline,get_line,putchar);
}

struct image *make_image(int width,int height)
{
	struct image *i;

	i=malloc(sizeof(struct image));
	i->width=width;
	i->height=height;
	
	i->sx=20;
	i->sy=20;

	i->ex=i->width;
	i->ey=i->height-10;

	i->dx=i->ex-i->sx;
	i->dy=i->ey-i->sy;
	
	i->ptr=calloc(i->width,i->height);

	return i;
}

void make_raster(struct image *i,int xstep,int ystep,int color,float high,float low,float high2,float low2,char *title)
{
	int x,y,p,skip=0;
	char buf[80],*format;
	double tmp,range;

	range=high-low;

        if (fabs(high)>1000) format="%4.0f";
	else if (fabs(high)>100) format="%5.1f";
	else if (fabs(high)>10) format="%5.2f";
	else format="%5.3f";
	

	for (y=i->ey; y>=i->sy; y-=ystep) {
                for (x=i->sx-1; x<i->ex; x++) {
			i->ptr[x+y*i->width]=color;
		}
		
		p=i->dy-(y-i->sy);
		tmp=low+range*p/i->dy;
		
		sprintf(buf,format,tmp);
		drawtext(i,1,y-3,color,0,buf);
	}

	range=high2-low2;

	if (fabs(high2)>100) format="%4.0f";
	else if (fabs(high2)>10) format="%4.1f";
        else format="%4.2f";
	
	for (x=i->sx; x<i->ex; x+=xstep) {
		for (y=i->sy; y<i->ey+2; y++) {
			i->ptr[x+y*i->width]=color;
		}

		p=i->dx-(x-i->sx);
		tmp=low2+range*p/i->dx;

		if (skip) {
			skip--;
			continue;
		}

		sprintf(buf,format,tmp);
		drawtext(i,x-10,i->ey+4,color,0,buf);
		skip=3;
	}

	drawtext(i,i->width/2,4,1,2+8,title);
}

void set_pix(struct image *i,int x,int y,int color)
{
	if (x<0 || y<0 || x>=i->width || y>=i->height) return;
	
	i->ptr[x+y*i->width]=color;
}


struct ddfont
{
        unsigned char dim;
        unsigned char *raw;
};

unsigned char fontb_000[] = /*   */ {255};
unsigned char fontb_001[] = /*   */ {255};
unsigned char fontb_002[] = /*   */ {255};
unsigned char fontb_003[] = /*   */ {255};
unsigned char fontb_004[] = /*   */ {255};
unsigned char fontb_005[] = /*   */ {255};
unsigned char fontb_006[] = /*   */ {255};
unsigned char fontb_007[] = /*   */ {255};
unsigned char fontb_008[] = /*   */ {255};
unsigned char fontb_009[] = /*   */ {255};
unsigned char fontb_010[] = /*   */ {255};
unsigned char fontb_011[] = /*   */ {255};
unsigned char fontb_012[] = /*   */ {255};
unsigned char fontb_013[] = /*   */ {255};
unsigned char fontb_014[] = /*   */ {255};
unsigned char fontb_015[] = /*   */ {255};
unsigned char fontb_016[] = /*   */ {255};
unsigned char fontb_017[] = /*   */ {255};
unsigned char fontb_018[] = /*   */ {255};
unsigned char fontb_019[] = /*   */ {255};
unsigned char fontb_020[] = /*   */ {255};
unsigned char fontb_021[] = /*   */ {255};
unsigned char fontb_022[] = /*   */ {255};
unsigned char fontb_023[] = /*   */ {255};
unsigned char fontb_024[] = /*   */ {255};
unsigned char fontb_025[] = /*   */ {255};
unsigned char fontb_026[] = /*   */ {255};
unsigned char fontb_027[] = /*   */ {255};
unsigned char fontb_028[] = /*   */ {255};
unsigned char fontb_029[] = /*   */ {255};
unsigned char fontb_030[] = /*   */ {255};
unsigned char fontb_031[] = /*   */ {255};
unsigned char fontb_032[] = /*   */ {255};
unsigned char fontb_033[] = /* ! */ {255};
unsigned char fontb_034[] = /* " */ {255};
unsigned char fontb_035[] = /* # */ {255};
unsigned char fontb_036[] = /* $ */ {255};
unsigned char fontb_037[] = /* % */ {254,0,3,254,2,254,1,254,0,3,255};
unsigned char fontb_038[] = /* & */ {255};
unsigned char fontb_039[] = /* ' */ {0,254,0,255};
unsigned char fontb_040[] = /* ( */ {255};
unsigned char fontb_041[] = /* ) */ {255};
unsigned char fontb_042[] = /* * */ {254,0,2,254,1,254,0,2,255};
unsigned char fontb_043[] = /* + */ {254,1,254,0,1,1,254,1,255};
unsigned char fontb_044[] = /* , */ {255};
unsigned char fontb_045[] = /* - */ {254,254,0,1,1,255};
unsigned char fontb_046[] = /* . */ {254,254,254,254,0,255};
unsigned char fontb_047[] = /* / */ {2,254,1,254,1,254,1,254,0,255};
unsigned char fontb_048[] = /* 0 */ {0,1,1,254,0,2,254,0,2,254,0,2,254,0,1,1,255};
unsigned char fontb_049[] = /* 1 */ {1,254,1,254,1,254,1,254,1,255};
unsigned char fontb_050[] = /* 2 */ {0,1,1,254,2,254,0,1,1,254,0,254,0,1,1,255};
unsigned char fontb_051[] = /* 3 */ {0,1,1,254,2,254,0,1,1,254,2,254,0,1,1,255};
unsigned char fontb_052[] = /* 4 */ {0,2,254,0,2,254,0,1,1,254,2,254,2,255};
unsigned char fontb_053[] = /* 5 */ {0,1,1,254,0,254,0,1,1,254,2,254,0,1,1,255};
unsigned char fontb_054[] = /* 6 */ {0,1,1,254,0,254,0,1,1,254,0,2,254,0,1,1,255};
unsigned char fontb_055[] = /* 7 */ {0,1,1,254,2,254,2,254,2,254,2,255};
unsigned char fontb_056[] = /* 8 */ {0,1,1,254,0,2,254,0,1,1,254,0,2,254,0,1,1,255};
unsigned char fontb_057[] = /* 9 */ {0,1,1,254,0,2,254,0,1,1,254,2,254,0,1,1,255};
unsigned char fontb_058[] = /* : */ {255};
unsigned char fontb_059[] = /* ; */ {255};
unsigned char fontb_060[] = /* < */ {255};
unsigned char fontb_061[] = /* = */ {254,0,1,1,254,254,0,1,1,255};
unsigned char fontb_062[] = /* > */ {255};
unsigned char fontb_063[] = /* ? */ {255};
unsigned char fontb_064[] = /* @ */ {255};
unsigned char fontb_065[] = /* A */ {1,254,0,2,254,0,1,1,254,0,2,254,0,2,255};
unsigned char fontb_066[] = /* B */ {0,1,254,0,2,254,0,1,254,0,2,254,0,1,255};
unsigned char fontb_067[] = /* C */ {1,1,254,0,254,0,254,0,254,1,1,255};
unsigned char fontb_068[] = /* D */ {0,1,254,0,2,254,0,2,254,0,2,254,0,1,255};
unsigned char fontb_069[] = /* E */ {0,1,1,254,0,254,0,1,254,0,254,0,1,1,255};
unsigned char fontb_070[] = /* F */ {0,1,1,254,0,254,0,1,254,0,254,0,255};
unsigned char fontb_071[] = /* G */ {1,1,254,0,254,0,2,254,0,2,254,1,1,255};
unsigned char fontb_072[] = /* H */ {0,2,254,0,2,254,0,1,1,254,0,2,254,0,2,255};
unsigned char fontb_073[] = /* I */ {0,254,0,254,0,254,0,254,0,255};
unsigned char fontb_074[] = /* J */ {2,254,2,254,2,254,0,2,254,0,1,1,255};
unsigned char fontb_075[] = /* K */ {0,2,254,0,2,254,0,1,254,0,2,254,0,2,255};
unsigned char fontb_076[] = /* L */ {0,254,0,254,0,254,0,254,0,1,1,255};
unsigned char fontb_077[] = /* M */ {0,4,254,0,1,2,1,254,0,2,2,254,0,4,254,0,4,255};
unsigned char fontb_078[] = /* N */ {0,3,254,0,1,2,254,0,2,1,254,0,2,1,254,0,3,255};
unsigned char fontb_079[] = /* O */ {1,254,0,2,254,0,2,254,0,2,254,1,255};
unsigned char fontb_080[] = /* P */ {0,1,254,0,2,254,0,1,254,0,254,0,255};
unsigned char fontb_081[] = /* Q */ {1,254,0,2,254,0,2,254,0,1,1,254,1,1,255};
unsigned char fontb_082[] = /* R */ {0,1,254,0,2,254,0,1,254,0,2,254,0,2,255};
unsigned char fontb_083[] = /* S */ {1,1,254,0,254,0,1,1,254,2,254,0,1,255};
unsigned char fontb_084[] = /* T */ {0,1,1,254,1,254,1,254,1,254,1,255};
unsigned char fontb_085[] = /* U */ {0,2,254,0,2,254,0,2,254,0,2,254,0,1,1,255};
unsigned char fontb_086[] = /* V */ {0,2,254,0,2,254,0,2,254,0,2,254,1,255};
unsigned char fontb_087[] = /* W */ {0,4,254,0,4,254,0,2,2,254,0,1,2,1,254,0,4,255};
unsigned char fontb_088[] = /* X */ {0,2,254,0,2,254,1,254,0,2,254,0,2,255};
unsigned char fontb_089[] = /* Y */ {0,2,254,0,2,254,0,2,254,1,254,1,255};
unsigned char fontb_090[] = /* Z */ {0,1,1,254,2,254,1,254,0,254,0,1,1,255};
unsigned char fontb_091[] = /* [ */ {255};
unsigned char fontb_092[] = /* \ */ {255};
unsigned char fontb_093[] = /* ] */ {255};
unsigned char fontb_094[] = /* ^ */ {255};
unsigned char fontb_095[] = /* _ */ {254,254,254,254,0,1,1,255};
unsigned char fontb_096[] = /* ` */ {255};
unsigned char fontb_097[] = /* a */ {254,1,1,254,0,2,254,0,2,254,1,1,255};
unsigned char fontb_098[] = /* b */ {0,254,0,1,254,0,2,254,0,2,254,0,1,255};
unsigned char fontb_099[] = /* c */ {254,1,1,254,0,254,0,254,1,1,255};
unsigned char fontb_100[] = /* d */ {2,254,1,1,254,0,2,254,0,2,254,1,1,255};
unsigned char fontb_101[] = /* e */ {254,1,254,0,2,254,0,1,254,1,1,255};
unsigned char fontb_102[] = /* f */ {0,1,254,0,254,0,1,254,0,254,0,255};
unsigned char fontb_103[] = /* g */ {254,1,254,0,2,254,1,1,254,0,1,1,255};
unsigned char fontb_104[] = /* h */ {0,254,0,1,254,0,2,254,0,2,254,0,2,255};
unsigned char fontb_105[] = /* i */ {0,254,254,0,254,0,254,0,255};
unsigned char fontb_106[] = /* j */ {1,254,254,1,254,1,254,0,255};
unsigned char fontb_107[] = /* k */ {0,254,0,2,254,0,2,254,0,1,254,0,2,255};
unsigned char fontb_108[] = /* l */ {0,254,0,254,0,254,0,254,0,255};
unsigned char fontb_109[] = /* m */ {254,0,1,2,254,0,2,2,254,0,2,2,254,0,2,2,255};
unsigned char fontb_110[] = /* n */ {254,0,1,254,0,2,254,0,2,254,0,2,255};
unsigned char fontb_111[] = /* o */ {254,1,254,0,2,254,0,2,254,1,255};
unsigned char fontb_112[] = /* p */ {254,1,254,0,2,254,0,1,254,0,255};
unsigned char fontb_113[] = /* q */ {254,1,254,0,2,254,1,1,254,2,255};
unsigned char fontb_114[] = /* r */ {254,0,1,254,0,254,0,254,0,255};
unsigned char fontb_115[] = /* s */ {254,1,254,0,254,1,254,0,255};
unsigned char fontb_116[] = /* t */ {0,254,0,1,254,0,254,0,254,0,255};
unsigned char fontb_117[] = /* u */ {254,0,2,254,0,2,254,0,2,254,1,1,255};
unsigned char fontb_118[] = /* v */ {254,0,2,254,0,2,254,1,254,1,255};
unsigned char fontb_119[] = /* w */ {254,0,4,254,0,2,2,254,0,1,2,1,254,0,4,255};
unsigned char fontb_120[] = /* x */ {254,0,2,254,0,1,254,1,1,254,0,2,255};
unsigned char fontb_121[] = /* y */ {254,0,2,254,0,2,254,1,1,254,0,1,1,255};
unsigned char fontb_122[] = /* z */ {254,0,1,1,254,2,254,1,254,0,1,1,255};
unsigned char fontb_123[] = /* { */ {255};
unsigned char fontb_124[] = /* | */ {255};
unsigned char fontb_125[] = /* } */ {255};
unsigned char fontb_126[] = /* ~ */ {255};
unsigned char fontb_127[] = /*   */ {255};
struct ddfont fontb[] = {
	{0,fontb_000},
	{0,fontb_001},
	{0,fontb_002},
	{0,fontb_003},
	{0,fontb_004},
	{0,fontb_005},
	{0,fontb_006},
	{0,fontb_007},
	{0,fontb_008},
	{0,fontb_009},
	{0,fontb_010},
	{0,fontb_011},
	{0,fontb_012},
	{0,fontb_013},
	{0,fontb_014},
	{0,fontb_015},
	{0,fontb_016},
	{0,fontb_017},
	{0,fontb_018},
	{0,fontb_019},
	{0,fontb_020},
	{0,fontb_021},
	{0,fontb_022},
	{0,fontb_023},
	{0,fontb_024},
	{0,fontb_025},
	{0,fontb_026},
	{0,fontb_027},
	{0,fontb_028},
	{0,fontb_029},
	{0,fontb_030},
	{0,fontb_031},
	{4,fontb_032},
	{0,fontb_033},
	{0,fontb_034},
	{0,fontb_035},
	{0,fontb_036},
	{5,fontb_037},
	{0,fontb_038},
	{2,fontb_039},
	{0,fontb_040},
	{0,fontb_041},
	{4,fontb_042},
	{4,fontb_043},
	{0,fontb_044},
	{4,fontb_045},
	{2,fontb_046},
	{4,fontb_047},
	{4,fontb_048},
	{4,fontb_049},
	{4,fontb_050},
	{4,fontb_051},
	{4,fontb_052},
	{4,fontb_053},
	{4,fontb_054},
	{4,fontb_055},
	{4,fontb_056},
	{4,fontb_057},
	{0,fontb_058},
	{0,fontb_059},
	{0,fontb_060},
	{4,fontb_061},
	{0,fontb_062},
	{0,fontb_063},
	{0,fontb_064},
	{4,fontb_065},
	{4,fontb_066},
	{4,fontb_067},
	{4,fontb_068},
	{4,fontb_069},
	{4,fontb_070},
	{4,fontb_071},
	{4,fontb_072},
	{2,fontb_073},
	{4,fontb_074},
	{4,fontb_075},
	{4,fontb_076},
	{6,fontb_077},
	{5,fontb_078},
	{4,fontb_079},
	{4,fontb_080},
	{4,fontb_081},
	{4,fontb_082},
	{4,fontb_083},
	{4,fontb_084},
	{4,fontb_085},
	{4,fontb_086},
	{6,fontb_087},
	{4,fontb_088},
	{4,fontb_089},
	{4,fontb_090},
	{0,fontb_091},
	{0,fontb_092},
	{0,fontb_093},
	{0,fontb_094},
	{4,fontb_095},
	{0,fontb_096},
	{4,fontb_097},
	{4,fontb_098},
	{4,fontb_099},
	{4,fontb_100},
	{4,fontb_101},
	{3,fontb_102},
	{4,fontb_103},
	{4,fontb_104},
	{2,fontb_105},
	{3,fontb_106},
	{4,fontb_107},
	{2,fontb_108},
	{6,fontb_109},
	{4,fontb_110},
	{4,fontb_111},
	{4,fontb_112},
	{4,fontb_113},
	{3,fontb_114},
	{3,fontb_115},
	{3,fontb_116},
	{4,fontb_117},
	{4,fontb_118},
	{6,fontb_119},
	{4,fontb_120},
	{4,fontb_121},
	{4,fontb_122},
	{0,fontb_123},
	{0,fontb_124},
	{0,fontb_125},
	{0,fontb_126},
	{0,fontb_127},
};

unsigned char fonta_000[] = /*   */ {255};
unsigned char fonta_001[] = /*   */ {255};
unsigned char fonta_002[] = /*   */ {255};
unsigned char fonta_003[] = /*   */ {255};
unsigned char fonta_004[] = /*   */ {255};
unsigned char fonta_005[] = /*   */ {255};
unsigned char fonta_006[] = /*   */ {255};
unsigned char fonta_007[] = /*   */ {255};
unsigned char fonta_008[] = /*   */ {255};
unsigned char fonta_009[] = /*   */ {255};
unsigned char fonta_010[] = /*   */ {255};
unsigned char fonta_011[] = /*   */ {255};
unsigned char fonta_012[] = /*   */ {255};
unsigned char fonta_013[] = /*   */ {255};
unsigned char fonta_014[] = /*   */ {255};
unsigned char fonta_015[] = /*   */ {255};
unsigned char fonta_016[] = /*   */ {255};
unsigned char fonta_017[] = /*   */ {255};
unsigned char fonta_018[] = /*   */ {255};
unsigned char fonta_019[] = /*   */ {255};
unsigned char fonta_020[] = /*   */ {255};
unsigned char fonta_021[] = /*   */ {255};
unsigned char fonta_022[] = /*   */ {255};
unsigned char fonta_023[] = /*   */ {255};
unsigned char fonta_024[] = /*   */ {255};
unsigned char fonta_025[] = /*   */ {255};
unsigned char fonta_026[] = /*   */ {255};
unsigned char fonta_027[] = /*   */ {255};
unsigned char fonta_028[] = /*   */ {255};
unsigned char fonta_029[] = /*   */ {255};
unsigned char fonta_030[] = /*   */ {255};
unsigned char fonta_031[] = /*   */ {255};
unsigned char fonta_032[] = /*   */ {255};
unsigned char fonta_033[] = /* ! */ {0,254,0,254,0,254,0,254,0,254,0,254,254,0,255};
unsigned char fonta_034[] = /* " */ {0,2,254,0,2,255};
unsigned char fonta_035[] = /* # */ {254,1,2,254,0,1,1,1,1,254,1,2,254,1,2,254,0,1,1,1,1,254,1,2,255};
unsigned char fonta_036[] = /* $ */ {1,1,1,254,0,2,2,254,0,2,254,1,1,1,254,2,2,254,2,2,254,0,2,2,254,1,1,1,255};
unsigned char fonta_037[] = /* % */ {254,0,1,3,254,0,3,254,3,254,2,254,1,254,1,3,254,0,3,1,255};
unsigned char fonta_038[] = /* & */ {1,1,254,0,3,254,0,2,254,1,254,1,254,0,2,2,254,0,3,254,1,1,2,255};
unsigned char fonta_039[] = /* ' */ {0,254,0,255};
unsigned char fonta_040[] = /* ( */ {1,254,0,254,0,254,0,254,0,254,0,254,0,254,1,255};
unsigned char fonta_041[] = /* ) */ {0,254,1,254,1,254,1,254,1,254,1,254,1,254,0,255};
unsigned char fonta_042[] = /* * */ {254,254,254,0,2,254,1,254,0,2,255};
unsigned char fonta_043[] = /* + */ {254,254,254,1,254,0,1,1,254,1,255};
unsigned char fonta_044[] = /* , */ {254,254,254,254,254,254,254,1,254,0,255};
unsigned char fonta_045[] = /* - */ {254,254,254,254,0,1,1,255};
unsigned char fonta_046[] = /* . */ {254,254,254,254,254,254,254,0,255};
unsigned char fonta_047[] = /* / */ {2,254,2,254,1,254,1,254,1,254,1,254,0,254,0,255};
unsigned char fonta_048[] = /* 0 */ {1,1,254,0,2,1,254,0,3,254,0,3,254,0,3,254,0,3,254,0,1,2,254,1,1,255};
unsigned char fonta_049[] = /* 1 */ {2,254,1,1,254,0,2,254,2,254,2,254,2,254,2,254,1,1,1,255};
unsigned char fonta_050[] = /* 2 */ {1,1,254,0,3,254,3,254,3,254,2,254,1,254,0,254,0,1,1,1,255};
unsigned char fonta_051[] = /* 3 */ {1,1,254,0,3,254,3,254,1,1,1,254,3,254,3,254,0,3,254,1,1,255};
unsigned char fonta_052[] = /* 4 */ {2,1,254,1,2,254,1,2,254,0,3,254,0,1,1,1,254,3,254,3,254,3,255};
unsigned char fonta_053[] = /* 5 */ {0,1,1,1,254,0,254,0,254,1,254,2,254,3,254,0,3,254,1,1,255};
unsigned char fonta_054[] = /* 6 */ {1,1,254,0,254,0,254,0,1,1,254,0,3,254,0,3,254,0,3,254,1,1,255};
unsigned char fonta_055[] = /* 7 */ {0,1,1,1,254,3,254,3,254,2,254,2,254,1,254,1,254,0,255};
unsigned char fonta_056[] = /* 8 */ {1,1,254,0,3,254,0,3,254,1,1,254,0,3,254,0,3,254,0,3,254,1,1,255};
unsigned char fonta_057[] = /* 9 */ {1,1,254,0,3,254,0,3,254,1,1,1,254,3,254,3,254,0,3,254,1,1,255};
unsigned char fonta_058[] = /* : */ {254,254,0,254,254,254,254,0,255};
unsigned char fonta_059[] = /* ; */ {254,254,0,254,254,254,254,0,254,0,255};
unsigned char fonta_060[] = /* < */ {254,254,2,254,1,254,0,254,1,254,2,255};
unsigned char fonta_061[] = /* = */ {254,254,254,0,1,1,254,254,0,1,1,255};
unsigned char fonta_062[] = /* > */ {254,254,0,254,1,254,2,254,1,254,0,255};
unsigned char fonta_063[] = /* ? */ {1,1,1,254,0,4,254,4,254,3,254,2,254,2,254,254,2,255};
unsigned char fonta_064[] = /* @ */ {2,1,1,254,1,4,254,0,3,1,2,254,0,2,2,2,254,0,2,2,2,254,0,3,2,254,1,254,2,1,1,255};
unsigned char fonta_065[] = /* A */ {2,254,2,254,1,2,254,1,2,254,1,1,1,254,0,4,254,0,4,254,0,4,255};
unsigned char fonta_066[] = /* B */ {0,1,1,254,0,3,254,0,3,254,0,1,1,254,0,3,254,0,3,254,0,3,254,0,1,1,255};
unsigned char fonta_067[] = /* C */ {1,1,254,0,3,254,0,254,0,254,0,254,0,254,0,3,254,1,1,255};
unsigned char fonta_068[] = /* D */ {0,1,1,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,0,1,1,255};
unsigned char fonta_069[] = /* E */ {0,1,1,1,254,0,254,0,254,0,1,1,254,0,254,0,254,0,254,0,1,1,1,255};
unsigned char fonta_070[] = /* F */ {0,1,1,1,254,0,254,0,254,0,1,1,254,0,254,0,254,0,254,0,255};
unsigned char fonta_071[] = /* G */ {1,1,254,0,3,254,0,254,0,254,0,2,1,254,0,3,254,0,3,254,1,1,255};
unsigned char fonta_072[] = /* H */ {0,3,254,0,3,254,0,3,254,0,3,254,0,1,1,1,254,0,3,254,0,3,254,0,3,255};
unsigned char fonta_073[] = /* I */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fonta_074[] = /* J */ {0,1,1,1,254,3,254,3,254,3,254,3,254,3,254,0,3,254,1,1,255};
unsigned char fonta_075[] = /* K */ {0,4,254,0,3,254,0,3,254,0,2,254,0,1,254,0,2,254,0,3,254,0,4,255};
unsigned char fonta_076[] = /* L */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,1,1,1,255};
unsigned char fonta_077[] = /* M */ {0,4,254,0,1,2,1,254,0,1,2,1,254,0,2,2,254,0,2,2,254,0,2,2,254,0,4,254,0,4,255};
unsigned char fonta_078[] = /* N */ {0,3,254,0,3,254,0,1,2,254,0,1,2,254,0,2,1,254,0,2,1,254,0,2,1,254,0,3,255};
unsigned char fonta_079[] = /* O */ {1,1,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,1,1,255};
unsigned char fonta_080[] = /* P */ {0,1,1,254,0,3,254,0,3,254,0,3,254,0,1,1,254,0,254,0,254,0,255};
unsigned char fonta_081[] = /* Q */ {1,1,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,0,2,1,254,1,1,1,255};
unsigned char fonta_082[] = /* R */ {0,1,1,254,0,3,254,0,3,254,0,3,254,0,1,1,254,0,1,254,0,2,254,0,2,1,255};
unsigned char fonta_083[] = /* S */ {1,1,254,0,3,254,0,254,0,254,1,1,254,3,254,0,3,254,1,1,255};
unsigned char fonta_084[] = /* T */ {0,1,1,1,1,254,2,254,2,254,2,254,2,254,2,254,2,254,2,255};
unsigned char fonta_085[] = /* U */ {0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,0,3,254,1,1,255};
unsigned char fonta_086[] = /* V */ {0,4,254,0,4,254,0,4,254,0,4,254,1,2,254,1,2,254,1,2,254,2,255};
unsigned char fonta_087[] = /* W */ {0,4,254,0,4,254,0,4,254,0,4,254,0,2,2,254,0,2,2,254,0,1,2,1,254,1,2,255};
unsigned char fonta_088[] = /* X */ {0,4,254,0,4,254,1,2,254,1,2,254,2,254,1,2,254,0,4,254,0,4,255};
unsigned char fonta_089[] = /* Y */ {0,4,254,0,4,254,0,4,254,1,2,254,1,2,254,2,254,2,254,1,1,1,255};
unsigned char fonta_090[] = /* Z */ {1,1,1,1,254,4,254,3,254,3,254,2,254,2,254,1,254,1,1,1,1,255};
unsigned char fonta_091[] = /* [ */ {0,1,254,0,254,0,254,0,254,0,254,0,254,0,254,0,1,255};
unsigned char fonta_092[] = /* \ */ {255};
unsigned char fonta_093[] = /* ] */ {0,1,254,1,254,1,254,1,254,1,254,1,254,1,254,0,1,255};
unsigned char fonta_094[] = /* ^ */ {255};
unsigned char fonta_095[] = /* _ */ {254,254,254,254,254,254,254,0,1,1,1,1,255};
unsigned char fonta_096[] = /* ` */ {255};
unsigned char fonta_097[] = /* a */ {254,254,254,1,1,254,3,254,1,1,1,254,0,3,254,1,1,1,255};
unsigned char fonta_098[] = /* b */ {0,254,0,254,0,254,0,1,1,254,0,3,254,0,3,254,0,3,254,0,1,1,255};
unsigned char fonta_099[] = /* c */ {254,254,254,1,1,254,0,3,254,0,254,0,3,254,1,1,255};
unsigned char fonta_100[] = /* d */ {3,254,3,254,3,254,1,1,1,254,0,3,254,0,3,254,0,3,254,1,1,1,255};
unsigned char fonta_101[] = /* e */ {254,254,254,1,1,254,0,3,254,0,1,1,1,254,0,254,1,1,255};
unsigned char fonta_102[] = /* f */ {2,254,1,254,1,254,0,1,1,254,1,254,1,254,1,254,1,255};
unsigned char fonta_103[] = /* g */ {254,254,254,1,1,254,0,3,254,0,3,254,1,1,1,254,3,254,1,1,255};
unsigned char fonta_104[] = /* h */ {0,254,0,254,0,254,0,1,1,254,0,3,254,0,3,254,0,3,254,0,3,255};
unsigned char fonta_105[] = /* i */ {254,0,254,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fonta_106[] = /* j */ {254,1,254,254,0,1,254,1,254,1,254,1,254,1,254,0,255};
unsigned char fonta_107[] = /* k */ {0,254,0,254,0,254,0,3,254,0,2,254,0,1,254,0,2,254,0,3,255};
unsigned char fonta_108[] = /* l */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,1,255};
unsigned char fonta_109[] = /* m */ {254,254,254,0,1,2,254,0,2,2,254,0,2,2,254,0,2,2,254,0,2,2,255};
unsigned char fonta_110[] = /* n */ {254,254,254,0,1,1,254,0,3,254,0,3,254,0,3,254,0,3,255};
unsigned char fonta_111[] = /* o */ {254,254,254,1,1,254,0,3,254,0,3,254,0,3,254,1,1,255};
unsigned char fonta_112[] = /* p */ {254,254,254,0,1,1,254,0,3,254,0,3,254,0,1,1,254,0,254,0,255};
unsigned char fonta_113[] = /* q */ {254,254,254,1,1,1,254,0,3,254,0,3,254,1,1,1,254,3,254,3,255};
unsigned char fonta_114[] = /* r */ {254,254,254,0,2,254,0,1,254,0,254,0,254,0,255};
unsigned char fonta_115[] = /* s */ {254,254,254,1,1,254,0,254,0,1,1,254,2,254,0,1,255};
unsigned char fonta_116[] = /* t */ {1,254,1,254,1,254,0,1,1,254,1,254,1,254,1,254,1,1,255};
unsigned char fonta_117[] = /* u */ {254,254,254,0,3,254,0,3,254,0,3,254,0,3,254,1,1,1,255};
unsigned char fonta_118[] = /* v */ {254,254,254,0,4,254,0,4,254,1,2,254,1,2,254,2,255};
unsigned char fonta_119[] = /* w */ {254,254,254,0,4,254,0,4,254,0,2,2,254,0,2,2,254,1,2,255};
unsigned char fonta_120[] = /* x */ {254,254,254,0,3,254,0,1,2,254,1,1,254,0,2,1,254,0,3,255};
unsigned char fonta_121[] = /* y */ {254,254,254,0,3,254,0,3,254,0,3,254,1,1,1,254,3,254,0,1,1,255};
unsigned char fonta_122[] = /* z */ {254,254,254,0,1,1,1,254,3,254,1,1,254,0,254,0,1,1,1,255};
unsigned char fonta_123[] = /* { */ {255};
unsigned char fonta_124[] = /* | */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fonta_125[] = /* } */ {255};
unsigned char fonta_126[] = /* ~ */ {2,1,254,2,254,1,254,0,1,254,1,254,1,254,2,254,2,1,255};
unsigned char fonta_127[] = /*   */ {255};
struct ddfont fonta[] = {
	{0,fonta_000},
	{0,fonta_001},
	{0,fonta_002},
	{0,fonta_003},
	{0,fonta_004},
	{0,fonta_005},
	{0,fonta_006},
	{0,fonta_007},
	{0,fonta_008},
	{0,fonta_009},
	{0,fonta_010},
	{0,fonta_011},
	{0,fonta_012},
	{0,fonta_013},
	{0,fonta_014},
	{0,fonta_015},
	{0,fonta_016},
	{0,fonta_017},
	{0,fonta_018},
	{0,fonta_019},
	{0,fonta_020},
	{0,fonta_021},
	{0,fonta_022},
	{0,fonta_023},
	{0,fonta_024},
	{0,fonta_025},
	{0,fonta_026},
	{0,fonta_027},
	{0,fonta_028},
	{0,fonta_029},
	{0,fonta_030},
	{0,fonta_031},
	{4,fonta_032},
	{2,fonta_033},
	{4,fonta_034},
	{6,fonta_035},
	{6,fonta_036},
	{6,fonta_037},
	{6,fonta_038},
	{2,fonta_039},
	{3,fonta_040},
	{3,fonta_041},
	{4,fonta_042},
	{4,fonta_043},
	{3,fonta_044},
	{4,fonta_045},
	{3,fonta_046},
	{4,fonta_047},
	{5,fonta_048},
	{5,fonta_049},
	{5,fonta_050},
	{5,fonta_051},
	{5,fonta_052},
	{5,fonta_053},
	{5,fonta_054},
	{5,fonta_055},
	{5,fonta_056},
	{5,fonta_057},
	{3,fonta_058},
	{3,fonta_059},
	{4,fonta_060},
	{4,fonta_061},
	{4,fonta_062},
	{6,fonta_063},
	{8,fonta_064},
	{6,fonta_065},
	{5,fonta_066},
	{5,fonta_067},
	{5,fonta_068},
	{5,fonta_069},
	{5,fonta_070},
	{5,fonta_071},
	{5,fonta_072},
	{2,fonta_073},
	{5,fonta_074},
	{6,fonta_075},
	{5,fonta_076},
	{6,fonta_077},
	{5,fonta_078},
	{5,fonta_079},
	{5,fonta_080},
	{5,fonta_081},
	{5,fonta_082},
	{5,fonta_083},
	{6,fonta_084},
	{5,fonta_085},
	{6,fonta_086},
	{6,fonta_087},
	{6,fonta_088},
	{6,fonta_089},
	{6,fonta_090},
	{3,fonta_091},
	{0,fonta_092},
	{3,fonta_093},
	{0,fonta_094},
	{6,fonta_095},
	{0,fonta_096},
	{5,fonta_097},
	{5,fonta_098},
	{5,fonta_099},
	{5,fonta_100},
	{5,fonta_101},
	{4,fonta_102},
	{5,fonta_103},
	{5,fonta_104},
	{2,fonta_105},
	{3,fonta_106},
	{5,fonta_107},
	{3,fonta_108},
	{6,fonta_109},
	{5,fonta_110},
	{5,fonta_111},
	{5,fonta_112},
	{5,fonta_113},
	{4,fonta_114},
	{4,fonta_115},
	{4,fonta_116},
	{5,fonta_117},
	{6,fonta_118},
	{6,fonta_119},
	{5,fonta_120},
	{5,fonta_121},
	{5,fonta_122},
	{0,fonta_123},
	{2,fonta_124},
	{0,fonta_125},
	{5,fonta_126},
	{0,fonta_127},
};

unsigned char fontc_000[] = /*   */ {255};
unsigned char fontc_001[] = /*   */ {255};
unsigned char fontc_002[] = /*   */ {255};
unsigned char fontc_003[] = /*   */ {255};
unsigned char fontc_004[] = /*   */ {255};
unsigned char fontc_005[] = /*   */ {255};
unsigned char fontc_006[] = /*   */ {255};
unsigned char fontc_007[] = /*   */ {255};
unsigned char fontc_008[] = /*   */ {255};
unsigned char fontc_009[] = /*   */ {255};
unsigned char fontc_010[] = /*   */ {255};
unsigned char fontc_011[] = /*   */ {255};
unsigned char fontc_012[] = /*   */ {255};
unsigned char fontc_013[] = /*   */ {255};
unsigned char fontc_014[] = /*   */ {255};
unsigned char fontc_015[] = /*   */ {255};
unsigned char fontc_016[] = /*   */ {255};
unsigned char fontc_017[] = /*   */ {255};
unsigned char fontc_018[] = /*   */ {255};
unsigned char fontc_019[] = /*   */ {255};
unsigned char fontc_020[] = /*   */ {255};
unsigned char fontc_021[] = /*   */ {255};
unsigned char fontc_022[] = /*   */ {255};
unsigned char fontc_023[] = /*   */ {255};
unsigned char fontc_024[] = /*   */ {255};
unsigned char fontc_025[] = /*   */ {255};
unsigned char fontc_026[] = /*   */ {255};
unsigned char fontc_027[] = /*   */ {255};
unsigned char fontc_028[] = /*   */ {255};
unsigned char fontc_029[] = /*   */ {255};
unsigned char fontc_030[] = /*   */ {255};
unsigned char fontc_031[] = /*   */ {255};
unsigned char fontc_032[] = /*   */ {255};
unsigned char fontc_033[] = /* ! */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,254,0,255};
unsigned char fontc_034[] = /* " */ {0,254,0,254,0,255};
unsigned char fontc_035[] = /* # */ {1,3,254,1,3,254,0,1,1,1,1,1,254,1,3,254,1,3,254,1,3,254,0,1,1,1,1,1,254,1,3,254,1,3,255};
unsigned char fontc_036[] = /* $ */ {2,254,1,1,1,254,0,2,2,254,0,2,254,1,1,254,2,1,254,2,2,254,0,2,2,254,1,1,1,254,2,255};
unsigned char fontc_037[] = /* % */ {1,1,254,0,3,3,254,1,1,3,254,4,254,3,254,2,254,1,3,1,254,0,3,3,254,4,1,255};
unsigned char fontc_038[] = /* & */ {1,254,0,2,254,0,2,254,1,254,1,254,0,2,2,254,0,3,254,0,3,254,1,1,2,255};
unsigned char fontc_039[] = /* ' */ {0,2,254,0,2,254,0,2,255};
unsigned char fontc_040[] = /* ( */ {1,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,1,255};
unsigned char fontc_041[] = /* ) */ {0,254,1,254,1,254,1,254,1,254,1,254,1,254,1,254,1,254,1,254,0,255};
unsigned char fontc_042[] = /* * */ {254,254,254,254,0,2,254,1,254,0,2,255};
unsigned char fontc_043[] = /* + */ {254,254,254,254,1,254,0,1,1,254,1,255};
unsigned char fontc_044[] = /* , */ {254,254,254,254,254,254,254,254,1,254,0,255};
unsigned char fontc_045[] = /* - */ {254,254,254,254,254,0,1,1,255};
unsigned char fontc_046[] = /* . */ {254,254,254,254,254,254,254,254,0,255};
unsigned char fontc_047[] = /* / */ {3,254,3,254,3,254,2,254,2,254,1,254,1,254,0,254,0,255};
unsigned char fontc_048[] = /* 0 */ {1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,0,4,254,0,4,254,0,4,254,1,1,1,255};
unsigned char fontc_049[] = /* 1 */ {2,254,1,1,254,2,254,2,254,2,254,2,254,2,254,2,254,2,255};
unsigned char fontc_050[] = /* 2 */ {1,1,1,254,0,4,254,4,254,4,254,3,254,2,254,1,254,0,254,0,1,1,1,1,255};
unsigned char fontc_051[] = /* 3 */ {1,1,1,254,0,4,254,4,254,4,254,2,1,254,4,254,4,254,0,4,254,1,1,1,255};
unsigned char fontc_052[] = /* 4 */ {3,254,2,1,254,2,1,254,1,2,254,1,2,254,0,3,254,0,1,1,1,1,254,3,254,3,255};
unsigned char fontc_053[] = /* 5 */ {0,1,1,1,1,254,0,254,0,254,0,1,1,1,254,0,4,254,4,254,4,254,0,4,254,1,1,1,255};
unsigned char fontc_054[] = /* 6 */ {1,1,1,254,0,4,254,0,254,0,254,0,1,1,1,254,0,4,254,0,4,254,0,4,254,1,1,1,255};
unsigned char fontc_055[] = /* 7 */ {0,1,1,1,1,254,4,254,3,254,3,254,2,254,2,254,1,254,1,254,1,255};
unsigned char fontc_056[] = /* 8 */ {1,1,1,254,0,4,254,0,4,254,0,4,254,1,1,1,254,0,4,254,0,4,254,0,4,254,1,1,1,255};
unsigned char fontc_057[] = /* 9 */ {1,1,1,254,0,4,254,0,4,254,0,4,254,1,1,1,1,254,4,254,4,254,0,4,254,1,1,1,255};
unsigned char fontc_058[] = /* : */ {254,254,254,0,254,254,254,254,254,0,255};
unsigned char fontc_059[] = /* ; */ {254,254,254,1,254,254,254,254,254,1,254,0,255};
unsigned char fontc_060[] = /* < */ {254,254,3,254,2,254,1,254,0,254,1,254,2,254,3,255};
unsigned char fontc_061[] = /* = */ {254,254,254,254,0,1,1,254,254,0,1,1,255};
unsigned char fontc_062[] = /* > */ {254,254,0,254,1,254,2,254,3,254,2,254,1,254,0,255};
unsigned char fontc_063[] = /* ? */ {1,1,1,254,0,4,254,4,254,4,254,3,254,2,254,2,254,254,2,255};
unsigned char fontc_064[] = /* @ */ {3,1,1,1,254,1,1,5,1,254,1,7,254,0,4,1,1,3,254,0,3,3,3,254,0,3,3,3,254,0,4,1,2,1,1,254,1,254,1,1,254,3,1,1,1,1,255};
unsigned char fontc_065[] = /* A */ {3,254,3,254,2,2,254,2,2,254,1,4,254,1,4,254,1,1,1,1,1,254,0,6,254,0,6,255};
unsigned char fontc_066[] = /* B */ {0,1,1,1,254,0,4,254,0,4,254,0,4,254,0,1,1,1,254,0,4,254,0,4,254,0,4,254,0,1,1,1,255};
unsigned char fontc_067[] = /* C */ {1,1,1,1,254,0,5,254,0,254,0,254,0,254,0,254,0,254,0,5,254,1,1,1,1,255};
unsigned char fontc_068[] = /* D */ {0,1,1,1,254,0,4,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,4,254,0,1,1,1,255};
unsigned char fontc_069[] = /* E */ {0,1,1,1,1,254,0,254,0,254,0,254,0,1,1,1,254,0,254,0,254,0,254,0,1,1,1,1,255};
unsigned char fontc_070[] = /* F */ {0,1,1,1,1,254,0,254,0,254,0,254,0,1,1,1,254,0,254,0,254,0,254,0,255};
unsigned char fontc_071[] = /* G */ {1,1,1,1,254,0,5,254,0,254,0,254,0,3,1,1,254,0,5,254,0,5,254,0,4,1,254,1,1,1,2,255};
unsigned char fontc_072[] = /* H */ {0,5,254,0,5,254,0,5,254,0,5,254,0,1,1,1,1,1,254,0,5,254,0,5,254,0,5,254,0,5,255};
unsigned char fontc_073[] = /* I */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_074[] = /* J */ {3,254,3,254,3,254,3,254,3,254,3,254,0,3,254,0,3,254,1,1,255};
unsigned char fontc_075[] = /* K */ {0,4,254,0,3,254,0,2,254,0,1,254,0,1,254,0,2,254,0,3,254,0,4,254,0,5,255};
unsigned char fontc_076[] = /* L */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,1,1,1,1,255};
unsigned char fontc_077[] = /* M */ {0,6,254,0,6,254,0,1,4,1,254,0,1,4,1,254,0,2,2,2,254,0,2,2,2,254,0,3,3,254,0,3,3,254,0,6,255};
unsigned char fontc_078[] = /* N */ {0,5,254,0,1,4,254,0,1,4,254,0,2,3,254,0,2,3,254,0,3,2,254,0,4,1,254,0,4,1,254,0,5,255};
unsigned char fontc_079[] = /* O */ {1,1,1,1,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,1,1,1,1,255};
unsigned char fontc_080[] = /* P */ {0,1,1,1,1,254,0,5,254,0,5,254,0,5,254,0,1,1,1,1,254,0,254,0,254,0,254,0,255};
unsigned char fontc_081[] = /* Q */ {1,1,1,1,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,3,2,254,0,4,1,254,1,1,1,1,254,5,255};
unsigned char fontc_082[] = /* R */ {0,1,1,1,1,254,0,5,254,0,5,254,0,5,254,0,1,1,1,1,254,0,5,254,0,5,254,0,5,254,0,5,255};
unsigned char fontc_083[] = /* S */ {1,1,1,254,0,4,254,0,254,0,254,1,1,1,254,4,254,4,254,0,4,254,1,1,1,255};
unsigned char fontc_084[] = /* T */ {0,1,1,1,1,254,2,254,2,254,2,254,2,254,2,254,2,254,2,254,2,255};
unsigned char fontc_085[] = /* U */ {0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,0,5,254,1,1,1,1,255};
unsigned char fontc_086[] = /* V */ {0,6,254,0,6,254,1,4,254,1,4,254,1,4,254,2,2,254,2,2,254,3,254,3,255};
unsigned char fontc_087[] = /* W */ {0,10,254,0,10,254,1,4,4,254,1,4,4,254,1,4,4,254,2,2,2,2,254,2,2,2,2,254,3,4,254,3,4,255};
unsigned char fontc_088[] = /* X */ {0,6,254,0,6,254,1,4,254,2,2,254,3,254,2,2,254,1,4,254,0,6,254,0,6,255};
unsigned char fontc_089[] = /* Y */ {0,6,254,0,6,254,1,4,254,2,2,254,3,254,3,254,3,254,3,254,3,255};
unsigned char fontc_090[] = /* Z */ {0,1,1,1,1,1,1,254,6,254,5,254,4,254,3,254,2,254,1,254,0,254,0,1,1,1,1,1,1,255};
unsigned char fontc_091[] = /* [ */ {0,1,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,1,255};
unsigned char fontc_092[] = /* \ */ {255};
unsigned char fontc_093[] = /* ] */ {0,1,254,1,254,1,254,1,254,1,254,1,254,1,254,1,254,1,254,1,254,0,1,255};
unsigned char fontc_094[] = /* ^ */ {255};
unsigned char fontc_095[] = /* _ */ {254,254,254,254,254,254,254,254,0,1,1,1,1,255};
unsigned char fontc_096[] = /* ` */ {255};
unsigned char fontc_097[] = /* a */ {254,254,254,1,1,1,254,4,254,1,1,1,1,254,0,4,254,0,4,254,1,1,1,1,255};
unsigned char fontc_098[] = /* b */ {0,254,0,254,0,254,0,1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,0,1,1,1,255};
unsigned char fontc_099[] = /* c */ {254,254,254,1,1,1,254,0,4,254,0,254,0,254,0,4,254,1,1,1,255};
unsigned char fontc_100[] = /* d */ {4,254,4,254,4,254,1,1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,1,1,1,1,255};
unsigned char fontc_101[] = /* e */ {254,254,254,1,1,1,254,0,4,254,0,1,1,1,1,254,0,254,0,4,254,1,1,1,255};
unsigned char fontc_102[] = /* f */ {1,254,0,254,0,254,0,1,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_103[] = /* g */ {254,254,254,1,1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,1,1,1,1,254,4,254,0,1,1,1,255};
unsigned char fontc_104[] = /* h */ {0,254,0,254,0,254,0,2,1,254,0,1,3,254,0,4,254,0,4,254,0,4,254,0,4,255};
unsigned char fontc_105[] = /* i */ {0,254,254,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_106[] = /* j */ {0,254,254,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_107[] = /* k */ {0,254,0,254,0,254,0,3,254,0,2,254,0,1,254,0,2,254,0,3,254,0,4,255};
unsigned char fontc_108[] = /* l */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_109[] = /* m */ {254,254,254,0,1,1,2,1,254,0,3,3,254,0,3,3,254,0,3,3,254,0,3,3,254,0,3,3,255};
unsigned char fontc_110[] = /* n */ {254,254,254,0,2,1,254,0,1,3,254,0,4,254,0,4,254,0,4,254,0,4,255};
unsigned char fontc_111[] = /* o */ {254,254,254,1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,1,1,1,255};
unsigned char fontc_112[] = /* p */ {254,254,254,0,1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,0,1,1,1,254,0,254,0,255};
unsigned char fontc_113[] = /* q */ {254,254,254,1,1,1,1,254,0,4,254,0,4,254,0,4,254,0,4,254,1,1,1,1,254,4,254,4,255};
unsigned char fontc_114[] = /* r */ {254,254,254,0,1,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_115[] = /* s */ {254,254,254,1,1,254,0,3,254,1,254,2,254,0,3,254,1,1,255};
unsigned char fontc_116[] = /* t */ {254,0,254,0,254,0,1,254,0,254,0,254,0,254,0,254,1,255};
unsigned char fontc_117[] = /* u */ {254,254,254,0,4,254,0,4,254,0,4,254,0,4,254,0,3,1,254,1,1,2,255};
unsigned char fontc_118[] = /* v */ {254,254,254,0,4,254,0,4,254,1,2,254,1,2,254,2,254,2,255};
unsigned char fontc_119[] = /* w */ {254,254,254,0,3,3,254,0,3,3,254,0,2,2,2,254,0,2,2,2,254,1,4,254,1,4,255};
unsigned char fontc_120[] = /* x */ {254,254,254,0,3,254,0,3,254,1,1,254,1,1,254,0,3,254,0,3,255};
unsigned char fontc_121[] = /* y */ {254,254,254,0,3,254,0,3,254,0,3,254,0,3,254,1,1,254,1,254,1,254,1,255};
unsigned char fontc_122[] = /* z */ {254,254,254,0,1,1,1,254,3,254,2,254,1,254,0,254,0,1,1,1,255};
unsigned char fontc_123[] = /* { */ {255};
unsigned char fontc_124[] = /* | */ {0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,254,0,255};
unsigned char fontc_125[] = /* } */ {255};
unsigned char fontc_126[] = /* ~ */ {254,254,254,254,1,1,3,254,0,3,1,255};
unsigned char fontc_127[] = /*   */ {255};
struct ddfont fontc[] = {
	{0,fontc_000},
	{0,fontc_001},
	{0,fontc_002},
	{0,fontc_003},
	{0,fontc_004},
	{0,fontc_005},
	{0,fontc_006},
	{0,fontc_007},
	{0,fontc_008},
	{0,fontc_009},
	{0,fontc_010},
	{0,fontc_011},
	{0,fontc_012},
	{0,fontc_013},
	{0,fontc_014},
	{0,fontc_015},
	{0,fontc_016},
	{0,fontc_017},
	{0,fontc_018},
	{0,fontc_019},
	{0,fontc_020},
	{0,fontc_021},
	{0,fontc_022},
	{0,fontc_023},
	{0,fontc_024},
	{0,fontc_025},
	{0,fontc_026},
	{0,fontc_027},
	{0,fontc_028},
	{0,fontc_029},
	{0,fontc_030},
	{0,fontc_031},
	{5,fontc_032},
	{2,fontc_033},
	{2,fontc_034},
	{7,fontc_035},
	{6,fontc_036},
	{8,fontc_037},
	{6,fontc_038},
	{4,fontc_039},
	{3,fontc_040},
	{3,fontc_041},
	{4,fontc_042},
	{4,fontc_043},
	{3,fontc_044},
	{4,fontc_045},
	{2,fontc_046},
	{5,fontc_047},
	{6,fontc_048},
	{6,fontc_049},
	{6,fontc_050},
	{6,fontc_051},
	{6,fontc_052},
	{6,fontc_053},
	{6,fontc_054},
	{6,fontc_055},
	{6,fontc_056},
	{6,fontc_057},
	{2,fontc_058},
	{3,fontc_059},
	{5,fontc_060},
	{4,fontc_061},
	{5,fontc_062},
	{6,fontc_063},
	{11,fontc_064},
	{8,fontc_065},
	{6,fontc_066},
	{7,fontc_067},
	{7,fontc_068},
	{6,fontc_069},
	{6,fontc_070},
	{7,fontc_071},
	{7,fontc_072},
	{2,fontc_073},
	{5,fontc_074},
	{7,fontc_075},
	{6,fontc_076},
	{8,fontc_077},
	{7,fontc_078},
	{7,fontc_079},
	{7,fontc_080},
	{7,fontc_081},
	{7,fontc_082},
	{6,fontc_083},
	{6,fontc_084},
	{7,fontc_085},
	{8,fontc_086},
	{12,fontc_087},
	{8,fontc_088},
	{8,fontc_089},
	{8,fontc_090},
	{3,fontc_091},
	{0,fontc_092},
	{3,fontc_093},
	{0,fontc_094},
	{6,fontc_095},
	{0,fontc_096},
	{6,fontc_097},
	{6,fontc_098},
	{6,fontc_099},
	{6,fontc_100},
	{6,fontc_101},
	{3,fontc_102},
	{6,fontc_103},
	{6,fontc_104},
	{2,fontc_105},
	{2,fontc_106},
	{6,fontc_107},
	{2,fontc_108},
	{8,fontc_109},
	{6,fontc_110},
	{6,fontc_111},
	{6,fontc_112},
	{6,fontc_113},
	{3,fontc_114},
	{5,fontc_115},
	{3,fontc_116},
	{6,fontc_117},
	{6,fontc_118},
	{8,fontc_119},
	{5,fontc_120},
	{5,fontc_121},
	{5,fontc_122},
	{0,fontc_123},
	{2,fontc_124},
	{0,fontc_125},
	{7,fontc_126},
	{0,fontc_127},
};

int drawtext(struct image *i,int sx, int sy, int color, int fontsize, const char *text)
{
        unsigned char *ptr,*dst;
        unsigned char *rawrun;
        int x,y;
	const char *c;
        struct ddfont *font;
	

	switch(fontsize&7) {
		case 0:		font=fontb; break;
		case 1:		font=fonta; break;
		default:	font=fontc; break;
		
	}

	if (fontsize&8) {
                for (x=0,c=text; *c; c++) x+=font[(int)*c].dim;
                sx-=x/2;
	}

        if (sy>=i->height) return sx;

	ptr=i->ptr;

        while (*text) {
                if (*text<0) { text++; continue; }

                rawrun=font[(int)*text].raw;

                x=sx;
                y=sy;

                dst=ptr+x+y*i->width;

                while (*rawrun!=255) {
                        if (*rawrun==254) {
                                y++;
                                x=sx;
                                rawrun++;
                                if (y>=i->height) break;
                                dst=ptr+x+y*i->width;
                                continue;
                        }

                        dst+=*rawrun;
                        x+=*rawrun;

                        if (x>=i->width) {
                                while (*rawrun!=255 && *rawrun!=254) rawrun++;
                                continue;
                        }

                        rawrun++;
                        if (x>=0 && y>=0) *dst=color;			
                }

                if (x>=i->width) break;

                sx+=font[(int)*text++].dim;
        }

        return sx;
}

