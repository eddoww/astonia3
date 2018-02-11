#include <windows.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#pragma hdrstop;
#include "../lib/zlib.h"
#include "../lib/png.h"
#include "main.h"
#include "dd.h"
#include "neuquant.h"
#include "sprite.h"


int read_JPEG_file(IMAGE *image, char *filename, int usealpha);

// helpers - might move to main

static void *xcompress(void *src, unsigned int srcsize, unsigned int *dstsize, int MEM_ID)
{
        unsigned char *dst;
        unsigned int dstlen;

        dstlen=12+srcsize*2;
        dst=xmalloc(dstlen,MEM_TEMP1);

        compress2(dst,(uLongf *)&dstlen,src,srcsize,9);

        dst=xrealloc(dst,dstlen,MEM_ID);

        *dstsize=dstlen;

        // note("compressed %d to %d bytes (%.2f%%)",srcsize,dstlen,100.0*(float)dstlen/(float)srcsize);

        return dst;
}

static void *xuncompress(void *src, unsigned int srcsize, unsigned int dstsize, int MEM_ID)
{
        unsigned char *dst;
        unsigned int dstlen;

        dstlen=dstsize;
        dst=xmalloc(dstlen,MEM_ID);
	if (!dst) {
		note("xuncompress trying to get %d bytes failed.",dstlen);
		return NULL;
	}

        uncompress(dst,(uLongf *)&dstlen,src,srcsize);

        if (dstlen!=dstsize) {
                xfree(dst);
                return NULL;
        }

        // note("uncompressed %d to %d bytes (%.2f%%)",srcsize,dstsize,100.0*(float)srcsize/(float)dstsize);

        return dst;
}

static char *binstr(char *dst, int dstlen, unsigned char *src, int srclen)
{
        char *run=dst;
        char hex[16]="0123456789ABCDEF";

        while(dstlen>3 && srclen>0) {

                *run++=hex[((*src)&0xF0)>>4];
                *run++=hex[((*src)&0x0F)];
                src++;

                srclen--;
                dstlen-=2;
        }

        if (dstlen) *run=0;

        return dst;
}

// palette

/*
static double pal_dist(unsigned short int rgb_a, unsigned short int rgb_b)
{
        double dr,dg,db;

        // HAS TO return 0 on exact match !!!  (only - i dunno)

        dr=(double)IGET_R(rgb_a)-(double)IGET_R(rgb_b);
        dg=(double)IGET_G(rgb_a)-(double)IGET_G(rgb_b);
        db=(double)IGET_B(rgb_a)-(double)IGET_B(rgb_b);

        // return (dr*dr+dg*dg+db*db)/(31*31*3.0);
        return (fabs(dr)+fabs(dg)+fabs(db))/(31*3.0);
}
*/

static unsigned int pal_dist(unsigned short int rgb_a, unsigned short int rgb_b)
{
        int dr,dg,db;

        // HAS TO return 0 on exact match !!!  (only - i dunno)

        dr=IGET_R(rgb_a)-IGET_R(rgb_b);
        dg=IGET_G(rgb_a)-IGET_G(rgb_b);
        db=IGET_B(rgb_a)-IGET_B(rgb_b);

        return dr*dr+dg*dg+db*db;
}

static int get_pal_entry(unsigned short int col, int pal_cnt, unsigned short int *pal)
{
        int i;

        for (i=0; i<pal_cnt; i++) if (col==pal[i]) return i;
        return -1;
}

#pragma argsused
static int get_pal_match(unsigned short int rgb, int pal_cnt, unsigned short int *pal)
{
	return inxsearch(IGET_R(rgb),IGET_G(rgb),IGET_B(rgb));
}

static int get_pal_match_old(unsigned short int rgb, int pal_cnt, unsigned short int *pal)
{
        int i,n;
        unsigned int dist,ndist;

        for (n=-1,ndist=0xFFFFFFFF,i=0; i<pal_cnt; i++) {
                dist=pal_dist(rgb,pal[i]);
                if (dist==0) return i;

                if (dist<ndist) {
                        ndist=dist;
                        n=i;
                }
        }

        return n;
}

// loading images

static int gfx_load_image_png(IMAGE *image, char *filename, int usealpha)
{
        int x,y,xres,yres,tmp,r,g,b,a,sx,sy,ex,ey;//,framecnt=0,framemax=0,ffx,ffy;
        int format;
        unsigned char **row;
        unsigned short *bmp;
        FILE *fp;
        png_structp png_ptr;
        png_infop info_ptr;
        png_infop end_info;
        int hasalpha;

        fp=fopen(filename,"rb");
        if (!fp) return -1;

        png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
        if (!png_ptr) { fclose(fp); printf("create read\n"); return -1; }

        info_ptr=png_create_info_struct(png_ptr);
        if (!info_ptr) { fclose(fp); png_destroy_read_struct(&png_ptr,(png_infopp)NULL,(png_infopp)NULL); printf("create info1\n"); return -1; }

        end_info=png_create_info_struct(png_ptr);
        if (!end_info) { fclose(fp); png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL); printf("create info2\n"); return -1; }

        png_init_io(png_ptr,fp);
        png_set_strip_16(png_ptr);
        png_read_png(png_ptr,info_ptr,PNG_TRANSFORM_PACKING,NULL);

        row=png_get_rows(png_ptr,info_ptr);
        if (!row) { fclose(fp); png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL); printf("read row\n"); return -1; }

        xres=png_get_image_width(png_ptr,info_ptr);
        yres=png_get_image_height(png_ptr,info_ptr);

        tmp=png_get_rowbytes(png_ptr,info_ptr);

        if (tmp==xres*3) format=3;
        else if (tmp==xres*4) format=4;
        else { fclose(fp); png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL); printf("rowbytes!=xres*4 (%d)",tmp); return -1; }

        if (png_get_bit_depth(png_ptr,info_ptr)!=8) { fclose(fp); png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL); printf("bit depth!=8\n"); return -1; }
        if (png_get_channels(png_ptr, info_ptr)!=format) { fclose(fp); png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL); printf("channels!=format\n"); return -1; }

        // prescan
        sx=xres;
        sy=yres;
        ex=0;
        ey=0;

        for (y=0; y<yres; y++) {
                for (x=0; x<xres; x++) {
                        if (format==4 && ( row[y][x*4+3]==0 || (row[y][x*4+0]==255 && row[y][x*4+1]==0 && row[y][x*4+2]==255) ) ) continue;
                        if (format==3 && (                     (row[y][x*3+0]==255 && row[y][x*3+1]==0 && row[y][x*3+2]==255) ) ) continue;
                        if (x<sx) sx=x;
                        if (x>ex) ex=x;
                        if (y<sy) sy=y;
                        if (y>ey) ey=y;
                }
        }

        if (ex<sx) ex=sx-1;
        if (ey<sy) ey=sy-1;

        // write

        image->xres=ex-sx+1;
        image->yres=ey-sy+1;
        image->xoff=-(xres/2)+sx;
        image->yoff=-(yres/2)+sy;

        image->rgb=xrealloc(image->rgb,image->xres*image->yres*sizeof(unsigned short int),MEM_IC);

        if (format==4) {

                image->a=xrealloc(image->a,image->xres*image->yres*sizeof(unsigned char),MEM_IC);

                hasalpha=0;

                for (y=0; y<image->yres; y++) {
                        for (x=0; x<image->xres; x++) {

                                a=row[(sy+y)][(sx+x)*4+3];

                                if (a) {
                                        r=min(255,row[(sy+y)][(sx+x)*4+0]*255/a);
                                        g=min(255,row[(sy+y)][(sx+x)*4+1]*255/a);
                                        b=min(255,row[(sy+y)][(sx+x)*4+2]*255/a);
                                }
                                else {
                                        r=255;
                                        g=0;
                                        b=255;
                                }

                                a>>=3;  // !!!

                                if (a<=usealpha/2) a=0;
				else if (a>=31-usealpha/2) a=31;

                                if (a==0 || (r==255 && g==0 && b==255)) {
                                        a=0;
                                        image->rgb[x+y*image->xres]=rgbcolorkey;
                                }
                                else image->rgb[x+y*image->xres]=IRGB(r>>3,g>>3,b>>3);

                                image->a[x+y*image->xres]=a;

                                if (a && a!=31) hasalpha=1;
                        }
                }
        }
        else {
                hasalpha=0;

                for (y=0; y<image->yres; y++) {
                        for (x=0; x<image->xres; x++) {

                                r=row[(sy+y)][(sx+x)*3+0];
                                g=row[(sy+y)][(sx+x)*3+1];
                                b=row[(sy+y)][(sx+x)*3+2];

                                if (r==255 && g==0 && b==255) image->rgb[x+y*image->xres]=rgbcolorkey;
                                else image->rgb[x+y*image->xres]=IRGB(r>>3,g>>3,b>>3);
                        }
                }
        }

        if (hasalpha==0) {
                xfree(image->a);
                image->a=NULL;
        }

        png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL);
        fclose(fp);

        return 0;
}

#define DATATYPE_GFX_NONE               0       // just to have something to start with
#define DATATYPE_GFX_1                  1       // full (5bit) r,g,b,a channels (8bit) [a,[r,g,b]][a,[r,g,b]][a,[r,g,b]] - rgb only if a
#define DATATYPE_GFX_2                  2
#define DATATYPE_GFX_3                  3
#define DATATYPE_GFX_KILL               4       // same like NONE, but the other side

struct gfxdata
{
        int dat_id;                     // the id of the data (sprite)
        int dat_type;                   // the type of the data (when decompressed ;-)

        unsigned int compsize;          // size of the compressed data
        unsigned int realsize;          // size of the decompressed data

        unsigned char *data;            // the compressed/decompressed data (depending where you are in the program)

        unsigned short int xres;        // x resolution in pixels
        unsigned short int yres;        // y resolution in pixels
        short int xoff;                 // x offset to blit position in pixels (created from the center of an image)
        short int yoff;                 // y offset to blit position in pixels (created from the center of an image)

};

struct pakdat
{
        unsigned int offset;
        unsigned int totsize;
        int dat_id;
        int dat_type;
};

struct pak_cache
{
        int fd;                 // if !=-1 this one is open
        unsigned int use_time;

        unsigned int savetime;  // if !=-1 this one is loaded
        int dat_cnt;
        struct pakdat *dat;
        int pal_cnt;
        unsigned short int *pal;
};

struct pak_cache *pak_cache;
int max_pakcache;
int max_open_pakcache; // has to be >= 1
int max_read_pakcache; // has to be >= max_open_pakcache
int num_open_pakcache;
int num_read_pakcache;

#ifdef DEVELOPER
static int pack_noise=1;
#endif

static char *pakfilename_str(char *dst, int sprite)
{
#ifdef DEVELOPER
        sprintf(dst,GFXPATH"%08d.pak",(sprite/1000)*1000);
#else
	sprintf(dst,"%08d.pak",(sprite/1000)*1000);
#endif
        return dst;
}

// gfx convert (reallocate image->rgb and image->a - convert src into this pointers) - sprite is just for output

static int load_gfx_1(IMAGE *image, unsigned int srcsize, unsigned char *src, int usealpha)
{
        int x,y,a,r,g,b;
        unsigned char *end=src+srcsize;
        unsigned short int rgb;
        int hasalpha=0;

        image->rgb=xrealloc(image->rgb,image->xres*image->yres*2,MEM_IC);
        image->a=xrealloc(image->a,image->xres*image->yres,MEM_IC);

        for (y=0; y<image->yres; y++) {
                for (x=0; x<image->xres; x++) {

                        if (src==end) return fail("srcsize seems to be not long enough [type 1]!");

                        if ((a=*src++)!=0) {
                                r=*src++;
                                g=*src++;
                                b=*src++;
                                rgb=IRGB(r,g,b);
                        }

                        if (a<=usealpha/2) a=0;
			else if (a>=31-usealpha/2) a=31;

			if (!a) rgb=rgbcolorkey;

                        image->rgb[x+y*image->xres]=rgb;
                        image->a[x+y*image->xres]=a;

                        if (a && a!=31) hasalpha=1;
                }
        }

        if (hasalpha==0) {
                xfree(image->a);
                image->a=NULL;
        }

        return 0;
}

static int load_gfx_2(IMAGE *image, unsigned int srcsize, unsigned char *src, int pal_cnt, unsigned short int *pal, int usealpha)
{
        int x,y,a,r,g,b,p;
        unsigned char *end=src+srcsize;
        unsigned short int rgb;
        int hasalpha=0;

        image->rgb=xrealloc(image->rgb,image->xres*image->yres*2,MEM_IC);
        image->a=xrealloc(image->a,image->xres*image->yres,MEM_IC);

        for (y=0; y<image->yres; y++) {
                for (x=0; x<image->xres; x++) {

                        if (src==end) return fail("srcsize seems to be not long enough [type 2]!");

                        if ((a=*src++)!=0) {

                                p=*src;
                                if (p>pal_cnt) return fail("index(%d) out of palette(%d) [type 2]!",p,pal_cnt);

                                rgb=pal[p];
                                src++;

                        }

                        if (a<=usealpha/2) a=0;
			else if (a>=31-usealpha/2) a=31;
                        if (!a) rgb=rgbcolorkey;

                        image->rgb[x+y*image->xres]=rgb;
                        image->a[x+y*image->xres]=a;

                        if (a && a!=31) hasalpha=1;
                }
        }

        if (hasalpha==0) {
                xfree(image->a);
                image->a=NULL;
        }

        return 0;
}

static int load_gfx_3(IMAGE *image, unsigned int srcsize, unsigned char *src, int pal_cnt, unsigned short int *pal, int usealpha)
{
        int x,y,a,r,g,b,amask,p;
        unsigned char *end=src+srcsize;
        unsigned short int rgb;
        int hasalpha=0;

        image->rgb=xrealloc(image->rgb,image->xres*image->yres*2,MEM_IC);
        image->a=xrealloc(image->a,image->xres*image->yres,MEM_IC);

        /*
        if (pal_cnt<=8)         amask=128|64|32|16|8;   // alpha 5:3 palette bits
        else if (pal_cnt<=16)   amask=128|64|32|16;     // alpha 4:4 palette bits
        else if (pal_cnt<=32)   amask=128|64|32;        // alpha 3:5 palette bits
        else if (pal_cnt<=64)   amask=128|64;           // alpha 2:6 palette bits
        else if (pal_cnt<=128)  amask=128;              // alpha 1:7 palette bits
        */

        if (pal_cnt<=8)         amask=128|64;           // alpha 5:3 palette bits
        else if (pal_cnt<=16)   amask=128|64;           // alpha 4:4 palette bits
        else if (pal_cnt<=32)   amask=128|64;           // alpha 3:5 palette bits
        else if (pal_cnt<=64)   amask=128|64;           // alpha 2:6 palette bits
        else if (pal_cnt<=128)  amask=128;              // alpha 1:7 palette bits
	else amask=0;

	if (pal_cnt<=128) {
		for (y=0; y<image->yres; y++) {
			for (x=0; x<image->xres; x++) {
	
				if (src==end) return fail("srcsize seems to be not long enough [type 3]!");
	
				if (*src) {
                                        a=*src&amask;
					if (a==amask) a=31; else a>>=3;
		
					p=(*src&(~amask));
						
					src++;

					if (p>pal_cnt) return fail("index(%d) out of palette(%d) [type 3]!",p,pal_cnt);
	
					rgb=pal[p];
				}
				else {
					src++;
					a=0;
				}
	
				if (a<=usealpha/2) a=0;
				else if (a>=31-usealpha/2) a=31;
	
				if (!a) rgb=rgbcolorkey;
	
				image->rgb[x+y*image->xres]=rgb;
				image->a[x+y*image->xres]=a;
	
				if (a && a!=31) hasalpha=1;
			}
		}
	} else {
		for (y=0; y<image->yres; y++) {
			for (x=0; x<image->xres; x++) {
	
				if (src==end) return fail("srcsize seems to be not long enough [type 3]!");
	
                                a=*src++;
				
				image->a[x+y*image->xres]=a;				
			}
		}
		for (y=0; y<image->yres; y++) {
			for (x=0; x<image->xres; x++) {
	
                                a=image->a[x+y*image->xres];
	
				if (a) {
					if (src==end) return fail("srcsize seems to be not long enough [type 3]! (%d,%d / %d,%d)",x,y,image->xres,image->yres);
					
					p=*src++;
                                        if (p>pal_cnt) return fail("index(%d) out of palette(%d) [type 3]!",p,pal_cnt);
	
					rgb=pal[p];
				}
	
				if (a<=usealpha/2) a=0;
				else if (a>=31-usealpha/2) a=31;
	
				if (!a) rgb=rgbcolorkey;
	
				image->rgb[x+y*image->xres]=rgb;
				image->a[x+y*image->xres]=a;
	
				if (a && a!=31) hasalpha=1;
			}
		}
        }
	if (src!=end) {
		note("type 3 is confused");
	}


        /*for (y=0; y<image->yres; y++) {
                for (x=0; x<image->xres; x++) {

                        if (src==end) return fail("srcsize seems to be not long enough [type 3]!");

                        if (*src) {
				if (pal_cnt<=128) {
					a=*src&amask;
					if (a==amask) a=31; else a>>=3;
	
					p=(*src&(~amask));
					
					src++;
				} else {
					a=*src++; a>>=3;
					p=*src++;
				}

                                if (p>pal_cnt) return fail("index(%d) out of palette(%d) [type 3]!",p,pal_cnt);

                                rgb=pal[p];
                        }
                        else {
                                src++;
                                a=0;
                        }

                        if (a<=usealpha/2) a=0;
			else if (a>=31-usealpha/2) a=31;

			if (!a) rgb=rgbcolorkey;

                        image->rgb[x+y*image->xres]=rgb;
                        image->a[x+y*image->xres]=a;

                        if (a && a!=31) hasalpha=1;
                }
        }*/

        if (hasalpha==0) {
                xfree(image->a);
                image->a=NULL;
        }

        return 0;
}

#ifdef DEVELOPER

#define MKUSE(nr)               (1<<(nr-1))

struct pakopts
{
        int use;                // oring the DATATYPE_GFX_X X useing MKUSE(X) (f.e. MKUSE(1)|MKUSE(3)|MKUSE(4) ) - you have to set at least one
        int num_pal;            // number of palette entries that should be used (0 to 256)
        int num_alp;            // number of alpha values that should be used (0 to 32) - 0 ???
	int scale;
	int shine;
};

static void *make_gfx_1(struct pakopts *pakopts, IMAGE *image, unsigned int *size)
{
        int x,y,used,a;
        unsigned char *dst;

        if (!(pakopts->use&(1<<(1-1)))) return NULL;

        dst=xmalloc(image->xres*image->yres*4,MEM_TEMP2);

        for (used=y=0; y<image->yres; y++) {
                for (x=0; x<image->xres; x++) {

                        if (image->a) a=image->a[x+y*image->xres]; else a=31;
                        if (image->rgb[x+y*image->xres]==rgbcolorkey) a=0;

                        if (a) {
                                dst[used++]=a;
                                dst[used++]=IGET_R(image->rgb[x+y*image->xres]);
                                dst[used++]=IGET_G(image->rgb[x+y*image->xres]);
                                dst[used++]=IGET_B(image->rgb[x+y*image->xres]);
                        }
                        else {
                                dst[used++]=0;
                        }
                }
        }

        dst=xrealloc(dst,used,MEM_TEMP2);
        *size=used;

        return dst;
}

static void *make_gfx_2(struct pakopts *pakopts, IMAGE *image, unsigned int *size, int pal_cnt, unsigned short int *pal)
{
        int x,y,used,a;
        unsigned char *dst;

        if (!(pakopts->use&(1<<(2-1)))) return NULL;

        dst=xmalloc(image->xres*image->yres*4,MEM_TEMP3);

        for (used=y=0; y<image->yres; y++) {
                for (x=0; x<image->xres; x++) {

                        if (image->a) a=image->a[x+y*image->xres]; else a=31;
                        if (image->rgb[x+y*image->xres]==rgbcolorkey) a=0;

                        if (a) {
                                dst[used++]=a;
                                dst[used++]=get_pal_match(image->rgb[x+y*image->xres],pal_cnt,pal);
                        }
                        else {
                                dst[used++]=0;
                        }
                }
        }

        note("[2] used %d",used);

        dst=xrealloc(dst,used,MEM_TEMP3);
        *size=used;

        return dst;
}

static void *make_gfx_3(struct pakopts *pakopts, IMAGE *image, unsigned int *size, int pal_cnt, unsigned short int *pal)
{
        int x,y,used,a,amask;
        unsigned char *dst;

        if (!(pakopts->use&(1<<(3-1)))) return NULL;
        if (pal_cnt<=0 || pal_cnt>256) return NULL;

        /*
        if (pal_cnt<=8)         amask=128|64|32|16|8;   // alpha 5:3 palette bits
        else if (pal_cnt<=16)   amask=128|64|32|16;     // alpha 4:4 palette bits
        else if (pal_cnt<=32)   amask=128|64|32;        // alpha 3:5 palette bits
        else if (pal_cnt<=64)   amask=128|64;           // alpha 2:6 palette bits
        else if (pal_cnt<=128)  amask=128;              // alpha 1:7 palette bits
        */

        if (pal_cnt<=8)         amask=128|64;           // alpha 5:3 palette bits
        else if (pal_cnt<=16)   amask=128|64;           // alpha 4:4 palette bits
        else if (pal_cnt<=32)   amask=128|64;           // alpha 3:5 palette bits
        else if (pal_cnt<=64)   amask=128|64;           // alpha 2:6 palette bits
        else if (pal_cnt<=128)  amask=128;              // alpha 1:7 palette bits
	else amask=0xf8;

        dst=xmalloc(image->xres*image->yres*4,MEM_TEMP4);

	if (pal_cnt<=128) {
		for (used=y=0; y<image->yres; y++) {
			for (x=0; x<image->xres; x++) {
	
				if (image->a) a=image->a[x+y*image->xres]; else a=31;
				if (image->rgb[x+y*image->xres]==rgbcolorkey) a=0;
	
				a=(a<<3)&amask;
	
				if (a) {
                                        dst[used++]=a|get_pal_match(image->rgb[x+y*image->xres],pal_cnt,pal);
				}
				else {
					dst[used++]=0;
				}
			}
		}
	} else {
		for (used=y=0; y<image->yres; y++) {
			for (x=0; x<image->xres; x++) {
	
				if (image->a) a=image->a[x+y*image->xres]; else a=31;
                                if (image->rgb[x+y*image->xres]==rgbcolorkey) a=0;

                                dst[used++]=a;
			}
		}

		for (y=0; y<image->yres; y++) {
			for (x=0; x<image->xres; x++) {
	
				if (image->a) a=image->a[x+y*image->xres]; else a=31;
				if (image->rgb[x+y*image->xres]==rgbcolorkey) a=0;
	
				if (a) {
					dst[used++]=get_pal_match(image->rgb[x+y*image->xres],pal_cnt,pal);
				}
			}
		}
	}

        dst=xrealloc(dst,used,MEM_TEMP4);
        *size=used;

        return dst;
}

void gfx_remove_dup(IMAGE *i2,int nr1);

static int gfx_make_gfxdata(struct pakopts *pakopts, int sprite, struct gfxdata *gfxdata, IMAGE *image, int pal_cnt, unsigned short int *pal)
{
        unsigned char *comp,*data,*ncomp=NULL;
        unsigned int compsize,realsize,ncompsize,nrealsize;
        unsigned int n,i;
        char buf[32];

        if (!gfxdata) return -1;

        // determin best type (we might need this more intelligent to increase speed and quality)

        for (n=-1,i=DATATYPE_GFX_NONE+1; i<DATATYPE_GFX_KILL; i++) {

                switch (i) {
                        case DATATYPE_GFX_1:    data=make_gfx_1(pakopts,image,&realsize); break;
			case DATATYPE_GFX_2:    data=make_gfx_2(pakopts,image,&realsize,pal_cnt,pal); break;
			case DATATYPE_GFX_3:    data=make_gfx_3(pakopts,image,&realsize,pal_cnt,pal); break;

                        default:                return fail("oops in gfx_max_gfxdata() i=%d",i);
                }

                if (!data) continue;
                if (!realsize) continue; // paranoia ?

                comp=xcompress(data,realsize,&compsize,MEM_TEMP5);

                xfree(data);

                if (pack_noise>2) note("sprite[%08d] [type %d] (%.2f%%, %d>%d bytes) [%s]",sprite,i,100.0*compsize/realsize,realsize,compsize,binstr(buf,sizeof(buf),comp,compsize));

                if (n==-1 || compsize<ncompsize) {
                        n=i;
                        xfree(ncomp);
                        ncomp=comp;
                        ncompsize=compsize;
                        nrealsize=realsize;
                }
                else {
                        xfree(comp);
                }

        }

        if (n==-1) return fail("oops, found no type that can pack sprite[%d]",sprite);

        if (pack_noise>1) note("sprite[%08d] [TYPE %d] (%3.2f%%, %d>%d bytes)",sprite,n,100.0*ncompsize/nrealsize,nrealsize,ncompsize);
        if (pack_noise>2) note("-------------------------------------------------------");

        gfxdata->dat_type=n;

        gfxdata->dat_id=sprite;

        gfxdata->xres=image->xres;
        gfxdata->yres=image->yres;
        gfxdata->xoff=image->xoff;
        gfxdata->yoff=image->yoff;

        gfxdata->realsize=nrealsize;
        gfxdata->compsize=ncompsize;
        gfxdata->data=ncomp;

        return 0;
}

static int gfx_save_pak(char *filename, int dat_cnt, struct gfxdata *gfxdata, int pal_cnt, unsigned short int *pal)
{
        unsigned int savetime,totsize;
        int fd,i;
        unsigned int start,offset;

        fd=open(filename,O_CREAT|O_TRUNC|O_BINARY|O_WRONLY,S_IREAD|S_IWRITE);
        if (fd==-1) return fail("failed to create '%s' (%s)",filename,strerror(errno));

        // savetime
        savetime=time(NULL);
        write(fd,&savetime,4);

        // dat_cnt
        write(fd,&dat_cnt,4);

        // calc start of dat offsets
        start=4+4 + dat_cnt*(4+4+4+4);

        // write the gfxdata's index
        for (totsize=0,offset=start,i=0; i<dat_cnt; i++,offset+=totsize) {

                totsize=4+4+gfxdata[i].compsize+8;
                // note("idx: sprite[%08d] offset=%d totsize=%d",gfxdata[i].dat_id,offset,totsize);

                // offset of that entry
                write(fd,&offset,4);

                // total size of that entry
                write(fd,&totsize,4);

                // data id and type of this entry
                write(fd,&gfxdata[i].dat_id,4);
                write(fd,&gfxdata[i].dat_type,4);
        }

        // write the gfxdata's
        for (i=0; i<dat_cnt; i++) {

                char buf[32];

                // note("dat: sprite[%08d] offset=%d realsize=%d compsize=%d [%s]",gfxdata[i].dat_id,tell(fd),gfxdata[i].realsize,gfxdata[i].compsize,binstr(buf,sizeof(buf),gfxdata[i].data,gfxdata[i].compsize));

                // compression info and the compressed data
                write(fd,&gfxdata[i].compsize,4);
                write(fd,&gfxdata[i].realsize,4);
                write(fd,gfxdata[i].data,gfxdata[i].compsize);

                // more information about the data
                write(fd,&gfxdata[i].xres,2);
                write(fd,&gfxdata[i].yres,2);
                write(fd,&gfxdata[i].xoff,2);
                write(fd,&gfxdata[i].yoff,2);
        }

        // pal_cnt and pal
        write(fd,&pal_cnt,4);
        write(fd,pal,2*pal_cnt);

        // note("save_pal_cnt=%d",pal_cnt);
        // for (i=0; i<pal_cnt; i++) note("save[%d]: %d %d %d",i,IGET_R(pal[i]),IGET_G(pal[i]),IGET_B(pal[i]));

        // done
        close(fd);

        return 0;
}

#endif

int gfx_load_pak(int sprite)
{
        int pidx,i,kill;
        char filename[1024];
        int fd;

        PARANOIA( if (sprite/1000>=max_pakcache) paranoia("gfx_load_pak: increase max_pak to %d/1000",sprite); )

        // find ;-)
        pidx=sprite/1000;

        // look if it's loaded
        if (pak_cache[pidx].fd!=-1) {
                pak_cache[pidx].use_time=GetTickCount();
                return pidx;
        }

        // we might need to close one
        if (num_open_pakcache==max_open_pakcache) {

                for (kill=-1,i=0; i<max_pakcache; i++) {
                        if (pak_cache[i].fd==-1) continue;
                        if (kill==-1 || pak_cache[i].use_time<pak_cache[kill].use_time) kill=i;
                }
                PARANOIA( if (kill==-1) paranoia("gfx_load_pak: should have found a pak_cache to kill(close)"); )

                // note("close pak_cache[%d]",kill);

                close(pak_cache[kill].fd);
                pak_cache[kill].fd=-1;
                num_open_pakcache--;
        }

        // we might need to kill one
        if (num_read_pakcache==max_read_pakcache) {
                for (kill=-1,i=0; i<max_pakcache; i++) {
                        if (pak_cache[i].fd!=-1) continue; // don't kill open
                        if (pak_cache[i].savetime==-1) continue;
                        if (kill==-1 || pak_cache[i].use_time<pak_cache[kill].use_time) kill=i;
                }

                PARANOIA( if (kill==-1) paranoia("gfx_load_pak: should have found a pak_cache to kill(free)"); )

                // note("kill pak_cache[%d]",kill);

                pak_cache[kill].savetime=-1;

                xfree(pak_cache[kill].dat);
                pak_cache[kill].dat=NULL;
                pak_cache[kill].dat_cnt=0;

                xfree(pak_cache[kill].pal);
                pak_cache[kill].pal=NULL;
                pak_cache[kill].pal_cnt=0;

                num_read_pakcache--;
        }

        // open
        if (pak_cache[pidx].fd==-1) {
                pakfilename_str(filename,sprite);
                pak_cache[pidx].fd=open(filename,O_RDONLY|O_BINARY);
                if (pak_cache[pidx].fd==-1) return fail("failed to open %s",filename);
                num_open_pakcache++;
        }

        fd=pak_cache[pidx].fd;

        // check if it's already loaded
        if (pak_cache[pidx].savetime!=-1) {
                pak_cache[pidx].use_time=GetTickCount();
                return pidx;
        }

        num_read_pakcache++;

        // load time
        read(fd,&pak_cache[pidx].savetime,4);
        PARANOIA( if (pak_cache[pidx].savetime==-1) paranoia("gfx_load_pak: pak[%d/1000] has ill savetime"); )

        // load dat_cnt
        read(fd,&pak_cache[pidx].dat_cnt,4);

        // load dat
        pak_cache[pidx].dat=xcalloc(pak_cache[pidx].dat_cnt*sizeof(* pak_cache[pidx].dat),MEM_PC);
        read(fd,pak_cache[pidx].dat,pak_cache[pidx].dat_cnt*sizeof(* pak_cache[pidx].dat));
	

        // load pal
        lseek(fd,pak_cache[pidx].dat[pak_cache[pidx].dat_cnt-1].offset+pak_cache[pidx].dat[pak_cache[pidx].dat_cnt-1].totsize,SEEK_SET);

        read(fd,&pak_cache[pidx].pal_cnt,4);
        pak_cache[pidx].pal_cnt&=~(0xFF000000);

        pak_cache[pidx].pal=xcalloc(pak_cache[pidx].pal_cnt*2,MEM_PC);
        read(fd,pak_cache[pidx].pal,pak_cache[pidx].pal_cnt*2);

	//note("loaded pack[%d] pal_cnt=%d dat_cnt=%d sizeofs(%d)",pidx,pak_cache[pidx].pal_cnt,pak_cache[pidx].dat_cnt,sizeof(struct pakdat));

        // for (i=0; i<pak_cache[pidx].pal_cnt; i++) note("load [%d]: %d,%d,%d",i,IGET_R(pak_cache[pidx].pal[i]),IGET_G(pak_cache[pidx].pal[i]),IGET_B(pak_cache[pidx].pal[i]));
        // for (i=0; i<pak_cache[pidx].dat_cnt; i++) note("sprite=%d type=%d offset=%d totsize=%d",pak_cache[pidx].dat[i].dat_id,pak_cache[pidx].dat[i].dat_type,pak_cache[pidx].dat[i].offset,pak_cache[pidx].dat[i].totsize);
        // note("loaded pak %d/1000 (dat_cnt=%d pal_cnt=%d)",sprite,pak_cache[pidx].dat_cnt,pak_cache[pidx].pal_cnt);

        // ok
        pak_cache[pidx].use_time=GetTickCount();
        return pidx;
}

int gfx_load_image_pak(IMAGE *image, int sprite)
{
        int pidx,i;
        unsigned int totsize,compsize,realsize;
        unsigned char *dat,*buf;
        int ret;
        char buff[32];

        pidx=gfx_load_pak(sprite);
        if (pidx==-1) return fail("pak for sprite %d not found",sprite);

        // find dat
        for (i=0; i<pak_cache[pidx].dat_cnt; i++) if (pak_cache[pidx].dat[i].dat_id==sprite) break;
        if (i==pak_cache[pidx].dat_cnt) {
	return fail("sprite %d isn't in pack %d",sprite,pidx);
	}

        // load from file
        lseek(pak_cache[pidx].fd,pak_cache[pidx].dat[i].offset,SEEK_SET);

        totsize=pak_cache[pidx].dat[i].totsize;
        dat=xmalloc(totsize,MEM_TEMP6);
        read(pak_cache[pidx].fd,dat,totsize);

        // look at gfx_save_pak to understand me ;-)
        compsize=*(unsigned int *)(dat);
        realsize=*(unsigned int *)(dat+4);

        buf=xuncompress(dat+8,compsize,realsize,MEM_TEMP7);
        if (!buf) { xfree(dat); return fail("uncompressing sprite %d failed",sprite); }

        image->xres=*(unsigned short int *)(dat+8+compsize+0);
        image->yres=*(unsigned short int *)(dat+8+compsize+2);
        image->xoff=*(short int *)(dat+8+compsize+4);
        image->yoff=*(short int *)(dat+8+compsize+6);

        // convert GFX_DATAFORMAT into image
        switch (pak_cache[pidx].dat[i].dat_type) {
                case DATATYPE_GFX_1:    ret=load_gfx_1(image,realsize,buf,dd_usealpha); break;
                case DATATYPE_GFX_2:    ret=load_gfx_2(image,realsize,buf,pak_cache[pidx].pal_cnt,pak_cache[pidx].pal,dd_usealpha); break;
                case DATATYPE_GFX_3:    ret=load_gfx_3(image,realsize,buf,pak_cache[pidx].pal_cnt,pak_cache[pidx].pal,dd_usealpha); break;
                default:                return fail("oops in gfx_load_image_pak(%d,%d)",sprite,pidx);
        }

        xfree(buf);
        xfree(dat);

        if (ret) return fail("failed to convert %d",sprite);

        return 0;
};

int gfx_force_png=0;

int _gfx_load_image(IMAGE *image, int sprite)
{
        char filename[1024];
        int fd;

	if (sprite>MAXSPRITE || sprite<0) {
		note("illegal sprite %d wanted",sprite);
                return -1;
	}

	if (gfx_force_png) {
                sprintf(filename,"%s%08d/%08d.png",GFXPATH,(sprite/1000)*1000,sprite);
                if (gfx_load_image_png(image,filename,dd_usealpha)==0) return 0;
                note("%s not found",filename);
        }

        // load from pack
        if (gfx_load_image_pak(image,sprite)==0) return 0;
        //note("update your .pak files (missing sprite %d)!",sprite);

#ifdef DEVELOPER
        // check if we can load it in a XXXXXXXX path
        sprintf(filename,"%s%08d/%08d.png",GFXPATH,(sprite/1000)*1000,sprite);
        if (gfx_load_image_png(image,filename,dd_usealpha)==0) return 0;
#endif

#ifdef STAFFER
        // check if we can load it in a XXXXXXXX path
        sprintf(filename,"gfx/%08d.png",sprite);
	note("trying %s",filename);
        if (gfx_load_image_png(image,filename,dd_usealpha)==0) return 0;
#endif

	note("missing sprite %d!",sprite);

        // then load the missing sprite image from pak
        if (gfx_load_image_pak(image,2)==0) return 0;
        note("update your .pak files (missing sprite image is missing)!!!");

#ifdef DEVELOPER
        // then load the missing sprite image from png
        sprintf(filename,"%s00000000/00000002.png",GFXPATH);
        if (gfx_load_image_png(image,filename,dd_usealpha)==0) return 0;
#endif

        paranoia("can't even find the missing image as png file - i'll quit");

        return -1;
}

int gfx_force_dh=0;
// this contains the duplicate hack --- DH!!!
int gfx_load_image(IMAGE *image, int sprite)
{
	return _gfx_load_image(image,sprite);
}

int gfx_init(void)
{
        int i;

        // init pak cache
        max_pakcache=MAXSPRITE/1000;	//200;
        max_open_pakcache=10;
        num_open_pakcache=0;
        max_read_pakcache=20;//max_open_pakcache*2;
        num_read_pakcache=0;
        pak_cache=xcalloc(max_pakcache*sizeof(* pak_cache),MEM_GLOB);

        for (i=0; i<max_pakcache; i++) {
                pak_cache[i].fd=-1;
                pak_cache[i].savetime=-1;
        }

        return 0;
}

void gfx_exit(void)
{
        int i;

        // free pak cache
        for (i=0; i<max_pakcache; i++) {
                if (pak_cache[i].fd!=-1) {
                        close(pak_cache[i].fd);
                }

                if (pak_cache[i].savetime!=-1) {
                        xfree(pak_cache[i].pal);
                        xfree(pak_cache[i].dat);
                }
        }

        xfree(pak_cache);
        pak_cache=NULL;
        max_pakcache=0;
        max_open_pakcache=0;
        num_open_pakcache=0;
        max_read_pakcache=0;
        num_read_pakcache=0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEVELOPER

int xint_qcmp(const void *va, const void *vb)
{
        return *(int *)va - *(int *)vb; // sort increasing
}

struct xpal {
        unsigned int r, g, b;
        unsigned int pixel_count;
        unsigned int err;
        int cc;
};

static unsigned int xpal_err(int c1, int c2, struct xpal *rgb_table)
{
        unsigned int dist1, dist2, P1, P2, P3, R1, G1, B1, R2, G2, B2, R3, G3, B3;
        int r_dist, g_dist, b_dist;

        R1 = rgb_table[c1].r;
        G1 = rgb_table[c1].g;
        B1 = rgb_table[c1].b;
        P1 = rgb_table[c1].pixel_count;

        R2 = rgb_table[c2].r;
        G2 = rgb_table[c2].g;
        B2 = rgb_table[c2].b;
        P2 = rgb_table[c2].pixel_count;

        P3 = P1 + P2;
        R3 = (R1 + R2 + (P3 >> 1)) / P3;
        G3 = (G1 + G2 + (P3 >> 1)) / P3;
        B3 = (B1 + B2 + (P3 >> 1)) / P3;

        R1 = (R1 + (P1 >> 1)) / P1;
        G1 = (G1 + (P1 >> 1)) / P1;
        B1 = (B1 + (P1 >> 1)) / P1;

        R2 = (R2 + (P2 >> 1)) / P2;
        G2 = (G2 + (P2 >> 1)) / P2;
        B2 = (B2 + (P2 >> 1)) / P2;

        r_dist = R3 - R1;
        g_dist = G3 - G1;
        b_dist = B3 - B1;
        dist1 = r_dist*r_dist + g_dist*g_dist + b_dist*b_dist;
        dist1 = (unsigned int)(sqrt(dist1) * P1);

        r_dist = R2 - R3;
        g_dist = G2 - G3;
        b_dist = B2 - B3;
        dist2 = r_dist*r_dist + g_dist*g_dist + b_dist*b_dist;
        dist2 = (unsigned int)(sqrt(dist2) * P2);

        return (dist1 + dist2);
}

#define MAXSAMPLE	(1024*1024*3)
static int gfx_build_pal(int image_cnt,IMAGE *image,int num_pal,unsigned short int *pal,int *sprite)
{
	unsigned char *bigrgb;
	int n,i,x,y;
	extern int netsize;
	extern void learn(void);
	extern void unbiasnet(void);
	extern void inxbuild(void);

	note("transform images for paletizing");
	
	netsize=num_pal;
	
	bigrgb=xmalloc(MAXSAMPLE,MEM_TEMP8);
	for (n=x=y=i=0; n<MAXSAMPLE; n+=3) {
		
		again:
                if (x>=image[i].xres) {
			x=0;
			y++;
		}
                if (y>=image[i].yres) {
			x=y=0;
			i++;
			if (i<image_cnt) {
				//note("sprite=%d (%d) (%dx%d %p %p)",sprite[i],i,image[i].xres,image[i].yres,image[i].rgb,image[i].a);
				if (image[i].xres<15 || image[i].yres<40) {
					note("small sprite: %d (%dx%d)",sprite[i],image[i].xres,image[i].yres);
				}
			}
		}
		if (i>=image_cnt) break;

		if (image[i].rgb[x+y*image[i].xres]==rgbcolorkey) { x++; goto again; }

		bigrgb[n+0]=IGET_R(image[i].rgb[x+y*image[i].xres]);
		bigrgb[n+1]=IGET_G(image[i].rgb[x+y*image[i].xres]);
		bigrgb[n+2]=IGET_B(image[i].rgb[x+y*image[i].xres]);

		x++;
		
	}
	note("initnet: n=%.2fK",n/1024.0);
	initnet(bigrgb,n,1);
	note("learn");
	learn();
	note("unbiasnet");
	unbiasnet();
	note("writecolourmap");
	writecolourmap(pal);

        note("inxbuild");
	inxbuild();
	note("done (phew)");
	xfree(bigrgb);

	return num_pal;
}

static int gfx_build_pal_old(int image_cnt, IMAGE *image, int num_pal, unsigned short int *pal)
{
        int x,y,index;
        struct xpal *rgb_table;
        unsigned short int irgb;
        int tot_colors;
        int i,j,c1,c2,c3;
        unsigned int err, cur_err, sum;
        int r,g,b;

	note("build pal init");
        // init
        rgb_table=xcalloc(32768*sizeof(struct xpal),MEM_TEMP9);

	note("build pal build");
        // build
        for (i=0; i<image_cnt; i++) {
		//note("i=%d/%d (%dx%d)",i,image_cnt,image[i].xres,image[i].yres);
                for (y=0; y<image[i].yres; y++) {
                        for (x=0; x<image[i].xres; x++) {
                                irgb=image[i].rgb[x+y*image->xres];
                                if (irgb==rgbcolorkey) continue;

                                index = (((IGET_R(irgb)<<3)&248)<<7) + (((IGET_G(irgb)<<3)&248)<<2) + ((IGET_B(irgb)<<3)>>3);
				rgb_table[index].r += IGET_R(irgb)<<3; // *rweight
                                rgb_table[index].g += IGET_G(irgb)<<3; // *gweight
                                rgb_table[index].b += IGET_B(irgb)<<3; // *bweight
                                rgb_table[index].pixel_count++;
                        }
                }		
        }

	note("build pal bring");
        // bring together
        tot_colors = 0;
        for (i=0; i<32768; i++) {
                if (rgb_table[i].pixel_count) {
                        rgb_table[tot_colors++]=rgb_table[i];
                }
        }

	note("build pal reduce");
        // reduce
        for (i=0; i<(tot_colors-1); i++) {

                err = ~0L;
                for (j=i+1; j<tot_colors; j++) {
                        cur_err = xpal_err(i,j,rgb_table);
                        if (cur_err<err) {
                                err=cur_err;
                                c2=j;
                        }
                }
                rgb_table[i].err=err;
                rgb_table[i].cc=c2;
        }
        rgb_table[i].err=~0L;
        rgb_table[i].cc=tot_colors;

        while (tot_colors>num_pal) {
                err=~0L;
                for (i=0; i < tot_colors; i++) {
                        if (rgb_table[i].err<err) {
                                err=rgb_table[i].err;
                                c1=i;
                        }
                }
                c2=rgb_table[c1].cc;
                rgb_table[c2].r += rgb_table[c1].r;
                rgb_table[c2].g += rgb_table[c1].g;
                rgb_table[c2].b += rgb_table[c1].b;
                rgb_table[c2].pixel_count += rgb_table[c1].pixel_count;
                tot_colors--;

                rgb_table[c1]=rgb_table[tot_colors];
                rgb_table[tot_colors-1].err=~0L;
                rgb_table[tot_colors-1].cc=tot_colors;

                for (i=0; i<c1; i++) {
                        if (rgb_table[i].cc == tot_colors) rgb_table[i].cc=c1;
                }

                for (i=c1+1; i<tot_colors; i++) {
                        if (rgb_table[i].cc == tot_colors) {
                                err=~0L;
                                for (j=i+1; j<tot_colors; j++) {
                                        cur_err=xpal_err(i,j,rgb_table);
                                        if (cur_err < err) {
                                                err=cur_err;
                                                c3=j;
                                        }
                                }
                                rgb_table[i].err=err;
                                rgb_table[i].cc=c3;
                        }
                }

                err=~0L;
                for (i=c1+1; i<tot_colors; i++) {
                        cur_err=xpal_err(i,c1,rgb_table);
                        if (cur_err<err) {
                                err=cur_err;
                                c3=i;
                        }
                }
                rgb_table[c1].err=err;
                rgb_table[c1].cc=c3;

                for (i=0; i<c1; i++) {
                        if (rgb_table[i].cc==c1) {
                                err=~0L;
                                for (j=i+1; j<tot_colors; j++) {
                                        cur_err=xpal_err(i,j,rgb_table);
                                        if (cur_err < err) {
                                                err=cur_err;
                                                c3=j;
                                        }
                                }
                                rgb_table[i].err=err;
                                rgb_table[i].cc=c3;
                        }
                }

                for (i=0;i<c1; i++) {
                        cur_err=xpal_err(i,c1,rgb_table);
                        if (cur_err < rgb_table[i].err) {
                                rgb_table[i].err=cur_err;
                                rgb_table[i].cc=c1;
                        }
                }

                if (c2!=tot_colors) {
                        err=~0L;
                        for (i=c2+1; i<tot_colors; i++) {
                                cur_err=xpal_err(c2,i,rgb_table);
                                if (cur_err < err) {
                                        err=cur_err;
                                        c3=i;
                                }
                        }
                        rgb_table[c2].err=err;
                        rgb_table[c2].cc=c3;
                        for (i=0; i<c2; i++) {
                                if (rgb_table[i].cc == c2) {
                                        err=~0L;
                                        for (j=i+1; j<tot_colors; j++) {
                                                cur_err=xpal_err(i,j,rgb_table);
                                                if (cur_err<err) {
                                                        err=cur_err;
                                                        c3=j;
                                                }
                                        }
                                        rgb_table[i].err=err;
                                        rgb_table[i].cc=c3;
                                }
                        }
                        for (i=0; i < c2; i++) {
                                cur_err=xpal_err(i,c2,rgb_table);
                                if (cur_err < rgb_table[i].err) {
                                        rgb_table[i].err=cur_err;
                                        rgb_table[i].cc=c2;
                                }
                        }
                }
        }


	note("build pal fill");
        // fill
        for (i=0; i < tot_colors; i++) {
                sum=rgb_table[i].pixel_count;

                r=(rgb_table[i].r+(sum>>1))/ sum;    // rgb_table[i].r/rweight
                g=(rgb_table[i].g+(sum>>1))/ sum;    // rgb_table[i].g/gweight
                b=(rgb_table[i].b+(sum>>1))/ sum;    // rgb_table[i].b/bweight

                pal[i]=IRGB(r>>3,g>>3,b>>3);
        }

        xfree(rgb_table);

        return tot_colors;
}

static int set_pakopts(struct pakopts *pakopts, int use, int num_pal,int scale,int shine)
{
        pakopts->use=use;
        pakopts->num_pal=num_pal;
	pakopts->scale=scale;
	pakopts->shine=shine;
        return 0;
}

static int get_pakopts(int paknr, struct pakopts *pakopts)
{
        switch (paknr) {
                case 0:         return set_pakopts(pakopts,MKUSE(1),0,100,0);       // gui elements
                case 1:         return set_pakopts(pakopts,MKUSE(1),0,100,0);       // spells
		case 10:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 11:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 12:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 13:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 14:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 15:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 16:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
		case 17:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // general items
                case 20:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // landscape (bad mixture so far, even items)
		case 21:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // landscape - ruins
		case 22:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // landscape - pents
		case 23:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // landscape - mountains
                case 30:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // landscape (ground tiles so far)
		case 40:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // landscape (walls so far)

		case 50:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // db - misc
		case 51:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // db - misc
		case 52:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // db - misc
		case 53:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // db - misc
		case 56:        return set_pakopts(pakopts,MKUSE(1),0,100,0);       // db - misc

                case 101:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // female warrior - still have to lookup the names of the anims for the player chars :(
                case 102:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // male warrior		
                case 103:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // female mage
                case 104:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // male mage		
                case 105:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // female seyan
                case 106:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // male seyan

                case 107:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // orc
                case 108:       return set_pakopts(pakopts,MKUSE(3), 8,100,0);       // skeleton
                case 109:       return set_pakopts(pakopts,MKUSE(3), 8,100,0);       // hades (skeleton)
                case 110:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // hagen (nordish)
                case 111:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // harpy
                case 112:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       // bear (big brown one)
                case 113:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // merlin (old red long beard mage)
                case 114:       return set_pakopts(pakopts,MKUSE(3),64, 85,0);       // nomad
                case 115:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // hector (...)
                case 116:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // creatur (like a crabbe, or so)
                case 117:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // samson (was an atzec mage somewhen, now i don't know)
                case 118:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // diana (long legs, small gold bikini and slip)
                case 119:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       // wolf
		case 120:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       // babybear (small brown one)

		case 124:       return set_pakopts(pakopts,MKUSE(3), 8,100,0);       // babybear (small brown one)
		case 125:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // simple woman
		case 126:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // lady


		case 128:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // dwarf
		case 129:       return set_pakopts(pakopts,MKUSE(3),16,75,0);       // earth demon
		case 130:       return set_pakopts(pakopts,MKUSE(3),32,75,0);       // ratling
		case 131:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //
		case 132:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //
		case 133:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //
		case 134:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // monk
		case 135:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //
		case 136:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //
		case 137:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       // werewolf
		case 138:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //
		case 139:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       //

		case 140:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 141:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
                case 142:       return set_pakopts(pakopts,MKUSE(3),16,100,0);       // testing, testing, 1, 2, 3
		case 143:       return set_pakopts(pakopts,MKUSE(3),32,100,0);       // testing, testing, 1, 2, 3
		case 144:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 145:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 146:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 147:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 148:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 149:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3

		case 150:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 151:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 152:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 153:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 154:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 155:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 156:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 157:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 158:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 159:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3

                case 160:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 161:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 162:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 163:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 164:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 165:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 166:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 167:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 168:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 169:       return set_pakopts(pakopts,MKUSE(3),64, 88,0);       // testing, testing, 1, 2, 3
		case 170:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 171:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 172:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 173:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 174:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 175:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 176:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 177:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 178:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3
		case 179:       return set_pakopts(pakopts,MKUSE(3),64,100,0);       // testing, testing, 1, 2, 3

		case 180:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 181:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 182:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 183:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 184:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 185:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 186:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 187:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 188:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 189:       return set_pakopts(pakopts,MKUSE(3),64,100,5);       // testing, testing, 1, 2, 3
		case 190:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 191:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 192:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 193:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 194:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 195:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 196:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 197:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 198:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 199:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 200:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 201:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 202:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 203:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 204:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 205:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 206:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 207:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 208:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 209:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 210:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 211:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 212:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 213:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 214:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 215:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 216:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 217:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 218:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
		case 219:       return set_pakopts(pakopts,MKUSE(3),64,100,10);       // testing, testing, 1, 2, 3
        }

        return -1;
}

static void scale_image(IMAGE *image,int scale)
{
	int x,y,xres,yres,xoff,yoff;
	double ix,iy,low_x,low_y,high_x,high_y,dbr,dbg,dbb,dba;
        unsigned short *rgb,irgb;
	unsigned char *a;

	xres=ceil((double)(image->xres-1)*scale/100.0);
	yres=ceil((double)(image->yres-1)*scale/100.0);
		
	xoff=floor(image->xoff*scale/100.0+0.5);
	yoff=floor(image->yoff*scale/100.0+0.5);

	rgb=xcalloc(xres*yres*sizeof(short int),MEM_TEMP10);
	a=xcalloc(xres*yres*sizeof(char),MEM_TEMP10);

	for (y=0; y<yres; y++) {
		for (x=0; x<xres; x++) {
			ix=x*100.0/scale;
			iy=y*100.0/scale;
			
			high_x=ix-floor(ix);
			high_y=iy-floor(iy);
			low_x=1-high_x;
			low_y=1-high_y;

			irgb=image->rgb[floor(ix)+floor(iy)*image->xres];

			if (irgb==rgbcolorkey) {
				dbr=0;
				dbg=0;
				dbb=0;
				dba=0;
			} else {
                                dbr=IGET_R(irgb)*low_x*low_y;
				dbg=IGET_G(irgb)*low_x*low_y;
				dbb=IGET_B(irgb)*low_x*low_y;
				if (!image->a) dba=31*low_x*low_y;
				else dba=image->a[floor(ix)+floor(iy)*image->xres]*low_x*low_y;
			}

			irgb=image->rgb[ceil(ix)+floor(iy)*image->xres];

			if (irgb==rgbcolorkey) {
				dbr+=0;
				dbg+=0;
				dbb+=0;
				dba+=0;
			} else {
                                dbr+=IGET_R(irgb)*high_x*low_y;
				dbg+=IGET_G(irgb)*high_x*low_y;
				dbb+=IGET_B(irgb)*high_x*low_y;
				if (!image->a) dba+=31*high_x*low_y;
				else dba+=image->a[ceil(ix)+floor(iy)*image->xres]*high_x*low_y;
			}

			irgb=image->rgb[floor(ix)+ceil(iy)*image->xres];

			if (irgb==rgbcolorkey) {
				dbr+=0;
				dbg+=0;
				dbb+=0;
				dba+=0;
			} else {
                                dbr+=IGET_R(irgb)*low_x*high_y;
				dbg+=IGET_G(irgb)*low_x*high_y;
				dbb+=IGET_B(irgb)*low_x*high_y;
				if (!image->a) dba+=31*low_x*high_y;
				else dba+=image->a[floor(ix)+ceil(iy)*image->xres]*low_x*high_y;
			}

			irgb=image->rgb[ceil(ix)+ceil(iy)*image->xres];

			if (irgb==rgbcolorkey) {
				dbr+=0;
				dbg+=0;
				dbb+=0;
				dba+=0;
			} else {
                                dbr+=IGET_R(irgb)*high_x*high_y;
				dbg+=IGET_G(irgb)*high_x*high_y;
				dbb+=IGET_B(irgb)*high_x*high_y;
				
				if (!image->a) dba+=31*high_x*high_y;
				else dba+=image->a[ceil(ix)+ceil(iy)*image->xres]*high_x*high_y;
			}


			rgb[x+y*xres]=IRGB(((int)dbr),((int)dbg),((int)dbb));
			a[x+y*xres]=((int)dba);
		}
	}

	image->xres=xres;
	image->yres=yres;
	image->xoff=xoff;
	image->yoff=yoff;

	xfree(image->rgb);
	xfree(image->a);

	image->rgb=rgb;
	image->a=a;
}

static void shine_image(IMAGE *image,int shine)
{
	int x,y,r,g,b;
	unsigned short irgb;

	for (y=0; y<image->yres; y++) {
		for (x=0; x<image->xres; x++) {
                        irgb=image->rgb[x+y*image->xres];

			if (irgb==rgbcolorkey) continue;
			
			r=IGET_R(irgb);
			g=IGET_G(irgb);
			b=IGET_B(irgb);

			if (abs(r-g)<2 && abs(r-b)<2 && abs(b-g)<2) {
				r=min(31,(r/15.5)*(r/15.5)*100/shine);
				g=min(31,(g/15.5)*(g/15.5)*100/shine);
				b=min(31,(b/15.5)*(b/15.5)*100/shine);
			}
			image->rgb[x+y*image->xres]=IRGB(r,g,b);
		}
	}
}

static void remove_alpha(IMAGE *image,int limit)
{
	int n;

        if (!image->a) return;

	for (n=0; n<image->xres*image->yres; n++) {
		if (image->a[n]>=limit) image->a[n]=31;
		else image->a[n]=0;		
	}
}

static int gfx_make_pak(int paknr)
{
        HANDLE fh;
        WIN32_FIND_DATA find;
        IMAGE *image;
        struct gfxdata *gfxdata;
        int *sprite=NULL;
        int i,image_used=0,image_max=0;
        char filepath[1024],filename[1024];
        unsigned short int pal[256];
        int pal_cnt;
        char *check;
        int t1,t2;
        struct pakopts pakopts;
	int last=0;

        // set path
        sprintf(filepath,GFXPATH"%08d/",paknr*1000);

        // find files
        sprintf(filename,"%s*.png",filepath);

	note("look for %s",filename);

        if ((fh=FindFirstFile(filename,&find))!=INVALID_HANDLE_VALUE) {
                do {
                        check=find.cFileName;

                        // check ;-) hard style
                        if (!isdigit(*check++) || !isdigit(*check++) || !isdigit(*check++) || !isdigit(*check++)) continue;
                        if (!isdigit(*check++) || !isdigit(*check++) || !isdigit(*check++) || !isdigit(*check++)) continue;
                        if (*check++!='.' || *check++!='p' || *check++!='n' || *check++!='g' || *check++!=0) continue;

                        // re-c-alloc
                        if (image_used==image_max) {
                                image_max+=64;
                                sprite=xrecalloc(sprite,image_max*sizeof(* sprite),MEM_TEMP);
                        }

                        sprite[image_used]=atoi(find.cFileName);
                        image_used++;

                        // if (image_used==3) break;

                }
                while (FindNextFile(fh,&find));

                FindClose(fh);
        }

        // nothing to do
        if (image_used==0) {
                if (pack_noise>0) note("skip '%s' (no images)",filepath);
                return 0;
        }

        // get packopts
        if (get_pakopts(paknr,&pakopts)==-1) return fail("have no pakopts for pak (%d)",paknr);

        // go ahead
        note("pak[%d] found %d image%s in '%s'",paknr,image_used,(image_used==1?"":"s"),filepath);

        image=xcalloc(image_max*sizeof(* image),MEM_TEMP);
        gfxdata=xcalloc(image_max*sizeof(* gfxdata),MEM_TEMP);

        // sort them by sprite
        qsort(sprite,image_used,sizeof(int),xint_qcmp);

	for (i=0; i<image_used; i++) {
		if (sprite[i]!=last+1) {
			note("Gap %d %d",last,sprite[i]);			
		}
		last=sprite[i];
	}

	note("load images");
        // load images
        for (i=0; i<image_used; i++) {
                sprintf(filename,"%s%08d.png",filepath,sprite[i]);
                if (gfx_load_image_png(&image[i],filename,1)==-1) return fail("failed to load image '%s'",filename);
		if (no_alpha_sprite(sprite[i])) remove_alpha(image+i,no_alpha_sprite(sprite[i]));
                if (pakopts.scale!=100) scale_image(image+i,pakopts.scale);
		if (pakopts.shine) shine_image(image+i,pakopts.shine);
        }	

	note("build palette");
        // build palettes
        if (pakopts.num_pal>0) {
                t1=GetTickCount();
                pal_cnt=gfx_build_pal(image_used,image,pakopts.num_pal,pal,sprite);
                if (pal_cnt==-1) return fail("gfx_build_pal(paknr=%d) failed",paknr);
                t2=GetTickCount();
                note("pak[%d] using %d colors (%.2fs)",paknr,pal_cnt,(t2-t1)/1000.0);
        }
	else pal_cnt=0;

	note("pack images");
        // pack images
        for (i=0; i<image_used; i++) {
		gfx_make_gfxdata(&pakopts,sprite[i],&gfxdata[i],&image[i],pal_cnt,pal);
        }

	note("save images");
        // save all into the .pak file
        pakfilename_str(filename,paknr*1000);
        if (gfx_save_pak(filename,image_used,gfxdata,pal_cnt,pal)!=0) return fail("failed to save '%s'",filename);
        note("pak[%d] saved to '%s'",paknr,filename);

        // free memory
        for (i=0; i<image_used; i++) {
                xfree(image[i].a);
                xfree(image[i].rgb);
                xfree(gfxdata[i].data);
        }
        xfree(sprite);
        xfree(image);
        xfree(gfxdata);

	list_mem();

        return 0;
}

int gfx_main(char cmd, int nr)
{
        gfx_init();

        note("- gfx_main(%c,%d) -----------------------------------",cmd,nr);

        if (cmd=='t') {         // test
                IMAGE image;

                gfx_make_pak(nr/1000);
                bzero(&image,sizeof(image));
                gfx_load_image_pak(&image,nr);
        } else if (cmd=='p') {    // pack <nr>
                if (nr==-1) {
                        for (nr=160; nr<200; nr++) {
				int i;

				gfx_make_pak(nr);

				// clear pak cache - it has changed
				for (i=0; i<max_pakcache; i++) {
					if (pak_cache[i].fd==-1) continue;
					
					close(pak_cache[i].fd);
					pak_cache[i].fd=-1;
					num_open_pakcache--;

					if (pak_cache[i].savetime!=-1) {
						pak_cache[i].savetime=-1;

						xfree(pak_cache[i].dat);
						pak_cache[i].dat=NULL;
						pak_cache[i].dat_cnt=0;
				
						xfree(pak_cache[i].pal);
						pak_cache[i].pal=NULL;
						pak_cache[i].pal_cnt=0;
				
						num_read_pakcache--;
					}
				}				
				note("pak open=%d, read=%d",num_open_pakcache,num_read_pakcache);
			}
                }
                else {
			note("nr=%d",nr);
                        if (gfx_make_pak(nr)) return fail("failed!!!");
                }
        } else {                  // oops
                warn("unknown command");
        }

        gfx_exit();

        list_mem();

        return 0;
}
#endif

