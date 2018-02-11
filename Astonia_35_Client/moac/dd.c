#include <windows.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ddraw.h>
#include <math.h>
#include <stdio.h>
#pragma hdrstop
#include "main.h"
#include "dd.h"
#include "client.h"

DDFONT *fonta_shaded=NULL;
DDFONT *fonta_framed=NULL;

DDFONT *fontb_shaded=NULL;
DDFONT *fontb_framed=NULL;

DDFONT *fontc_shaded=NULL;
DDFONT *fontc_framed=NULL;

void dd_create_font(void);
void dd_init_text(void);
void dd_black(void);

void vid_show(int nr);
void vid_check_for_erasure(int sidx);
static void vid_add_cache_small(struct vid_cache *vn);
static void vid_free_cache(struct vid_cache *vc);
static void vid_reset(void);

// extern ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern HWND mainwnd;

// helpers

#ifndef irgb_blend
unsigned short int irgb_blend(unsigned short int a, unsigned short int b, int alpha)
{
        return IRGB( (IGET_R(a)*alpha+IGET_R(b)*(31-alpha))/31 , (IGET_G(a)*alpha+IGET_G(b)*(31-alpha))/31, (IGET_B(a)*alpha+IGET_B(b)*(31-alpha))/31);
}
#endif

// direct x basics //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int dd_usesysmem=1;     // set from outside
int dd_usealpha=1;      // set from outside from 1 (full alpha) to 31(no alpha)
int dd_windowed=0;      // set from outside
int dd_maxtile=DD_VID32MB;   // (max=0xFFFD) maximum number of tiles this client should use (for multiclient sessions)
//int dd_maxtile=DD_VID4MB;
int dd_gamma=8;
int dd_lighteffect=16;
int newlight=1;

int XRES;               // set to indicate the maximal size of the offscreen surface - respective the display mode to set
int YRES;               // set to indicate the maximal size of the offscreen surface - respective the display mode to set

int dd_usevidmem;       // whats up really

char dderr[256]={""};
const char *DDERR=dderr;

unsigned int BPP;
unsigned int R_MASK;
unsigned int G_MASK;
unsigned int B_MASK;
unsigned int D_MASK;

#define RGBM_R5G6B5     0
#define RGBM_X1R5G5B5   1
#define RGBM_B5G6R5     2

const char *rgbmstr[3]={"RGBM_R5G6B5","RGBM_X1R5G5B5","RGBM_B5G6R5"};

int x_offset,y_offset;
int x_max,y_max;

int vc_cnt=0,sc_cnt=0,ap_cnt=0,np_cnt=0,tp_cnt=0,vm_cnt=0,sm_cnt=0;
int vc_time=0,ap_time=0,tp_time=0,sc_time=0,vm_time=0,bless_time=0,vi_time=0,im_time=0;
//int sb_time=0,ol_time=0;

int rgbm=-1;
int xres;
int yres;

int dd_tick=0;

LPDIRECTDRAW dd=NULL;
LPDIRECTDRAWCLIPPER ddcl=NULL;
LPDIRECTDRAWSURFACE ddps=NULL,ddbs=NULL;

#define DDCS_MAX        32
LPDIRECTDRAWSURFACE ddcs[DDCS_MAX];
int ddcs_usemem[DDCS_MAX];
int ddcs_used=0;
int ddcs_xres[DDCS_MAX];        // cache resolution in pixels
int ddcs_yres[DDCS_MAX];        // cache resolution in pixels
int ddcs_xmax[DDCS_MAX];        // cache resolution in pixels
int ddcs_ymax[DDCS_MAX];        // cache resolution in pixels
int ddcs_tdx[DDCS_MAX];         // cache resolution in tiles
int ddcs_tdy[DDCS_MAX];         // cache resolution in tiles
int ddcs_tiles[DDCS_MAX];       // number of tiles
int ddcs_total=0;               // total number of tiles
int dd_vidmeminuse=0;           // number of bytes videomemory used

int clipsx,clipsy,clipex,clipey;
int clipstore[32][4],clippos=0;

static unsigned short *vidptr=NULL;     // for checking only - remove in final release

void dd_push_clip(void)
{
        if (clippos>=32) return;

        clipstore[clippos][0]=clipsx;
        clipstore[clippos][1]=clipsy;
        clipstore[clippos][2]=clipex;
        clipstore[clippos][3]=clipey;
        clippos++;
}

void dd_pop_clip(void)
{
        if (clippos==0) return;

        clippos--;
        clipsx=clipstore[clippos][0];
        clipsy=clipstore[clippos][1];
        clipex=clipstore[clippos][2];
        clipey=clipstore[clippos][3];
}

void dd_more_clip(int sx, int sy, int ex, int ey)
{
        if (sx>clipsx) clipsx=sx;
        if (sy>clipsy) clipsy=sy;
        if (ex<clipex) clipex=ex;
        if (ey<clipey) clipey=ey;
}

void dd_set_clip(int sx, int sy, int ex, int ey)
{
        clipsx=sx;
        clipsy=sy;
        clipex=ex;
        clipey=ey;
}

extern int gfx_load_image(IMAGE *image, int sprite);
extern int gfx_init(void);
extern int gfx_exit(void);

static const char *dd_errstr(int err)
{
        static char buf[256];

	switch (err) {
                case -1:                                        return "DDERR INTERNAL";
		case DDERR_INVALIDOBJECT:                       return "DDERR_INVALIDOBJECT";
		case DDERR_INVALIDPARAMS:                       return "DDERR_INVALIDPARAMS";
		case DDERR_OUTOFMEMORY:                         return "DDERR_OUTOFMEMORY";
		case DDERR_SURFACEBUSY:                         return "DDERR_SURFACEBUSY";
		case DDERR_SURFACELOST:                         return "DDERR_SURFACELOST";
		case DDERR_WASSTILLDRAWING:                 	return "DDERR_WASSTILLDRAWING";
		case DDERR_INCOMPATIBLEPRIMARY:             	return "DDERR_INCOMPATIBLEPRIMARY";
		case DDERR_INVALIDCAPS:                   	return "DDERR_INVALIDCAPS";
		case DDERR_INVALIDPIXELFORMAT:            	return "DDERR_INVALIDPIXELFORMAT";
		case DDERR_NOALPHAHW:                     	return "DDERR_NOALPHAHW";
		case DDERR_NOCOOPERATIVELEVELSET:         	return "DDERR_NOCOOPERATIVELEVELSET";
		case DDERR_NODIRECTDRAWHW:                	return "DDERR_NODIRECTDRAWHW";
		case DDERR_NOEMULATION:                   	return "DDERR_NOEMULATION";
		case DDERR_NOEXCLUSIVEMODE:               	return "DDERR_NOEXCLUSIVEMODE";
		case DDERR_NOFLIPHW:                      	return "DDERR_NOFLIPHW";
		case DDERR_NOMIPMAPHW:                    	return "DDERR_NOMIPMAPHW";
		case DDERR_NOOVERLAYHW:                   	return "DDERR_NOOVERLAYHW";
		case DDERR_NOZBUFFERHW:                   	return "DDERR_NOZBUFFERHW";
		case DDERR_OUTOFVIDEOMEMORY:              	return "DDERR_OUTOFVIDEOMEMORY";
		case DDERR_PRIMARYSURFACEALREADYEXISTS:   	return "DDERR_PRIMARYSURFACEALREADYEXISTS";
		case DDERR_UNSUPPORTEDMODE:               	return "DDERR_UNSUPPORTEDMODE";
		case DDERR_EXCEPTION:                           return "DDERR_EXCEPTION";
		case DDERR_GENERIC:                             return "DDERR_GENERIC";
		case DDERR_INVALIDRECT:                         return "DDERR_INVALIDRECT";
		case DDERR_NOTFLIPPABLE:                        return "DDERR_NOTFLIPPABLE";
                case DDERR_UNSUPPORTED:                         return "DDERR_UNSUPPORTED";
        }

        sprintf(buf,"DDERR_UNKNOWN(%d,%X,%X)",err,err,DDERR_EXCEPTION);
        return buf;
}

static int dd_error(const char *txt, int err)
{
        sprintf(dderr,"%s : %s",txt,dd_errstr(err));
        if (err==DDERR_SURFACELOST) note("%s",dderr); else fail("%s",dderr);
        return -1;
}

static int restab[][2]= {
	{  1920,   1920},	// optimal size
	{  1920,   1440},	// good sizes...
	{  1920,    960},	// ...
        {  1920,    480},	// ...
	{  1920,    240},	// ...
        {  1440,   1440},	// good sizes...
	{  1440,    960},	// ...
        {  1440,    480},	// ...
	{  1440,    240},	// ...
        {   960,    960},	// ...
        {   960,    480},	// ...
	{   960,    240},	// ...
	{   800,    960},	// ...
        {   800,    480},	// ...
	{   800,    240},	// ...
	{   480,    480},	// ...
	{   480,    240},	// ...
        {   240,    240},	// ...
	{   144,    144},	// ...
	{   800,    144},	// ...
	{   800,     96},	// ...
	{   800,     48},	// ...	
	{    96,     96}	// ... must not contain any surface which will yield less than 3 tiles!
	

        /*{  1600,   1200},	// try big standard sizes... - faulty?
        {  1280,   1200},
	{   800,    600},
	{   640,    480},
	{   320,    200},
	{   160,    160}*/
};

static int max_restab=sizeof(restab)/(2*sizeof(int));

static char *binstr(char *buf, unsigned int val, int num)
{
        int i;
        char *run=buf;

        for (i=num-1; i>=0; i--) if ((1<<i)&val) *run++='1'; else *run++='0';
        *run=0;

        return buf;
}

static char *ddsdstr(char *buf, DDSURFACEDESC *ddsd)
{
        // char rbuf[64];
        // char gbuf[64];
        // char bbuf[64];
        int bpp,pitch;
        char *memstr;

        if (ddsd->ddsCaps.dwCaps&DDSCAPS_LOCALVIDMEM) memstr="localvidmem";
        else if (ddsd->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY) memstr="videomemory";
        else if (ddsd->ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY) memstr="systemmemory";
        else memstr="funnymemory";

        bpp=ddsd->ddpfPixelFormat.u1.dwRGBBitCount;
        if (bpp) pitch=ddsd->u1.lPitch/(bpp/8); else pitch=-1;

        sprintf(buf,"%dx%dx%d %s (pitch=%d) (%08X,%08X,%08X)",
                ddsd->dwWidth,ddsd->dwHeight,bpp,
                memstr,
                pitch,
                ddsd->ddpfPixelFormat.u2.dwRBitMask, // binstr(rbuf,ddsd->ddpfPixelFormat.u2.dwRBitMask,bpp),
                ddsd->ddpfPixelFormat.u3.dwGBitMask, // binstr(gbuf,ddsd->ddpfPixelFormat.u3.dwGBitMask,bpp),
                ddsd->ddpfPixelFormat.u4.dwBBitMask  // binstr(bbuf,ddsd->ddpfPixelFormat.u4.dwBBitMask,bpp)
        );

        return buf;
}

static int dd_vidmembytes(LPDIRECTDRAWSURFACE sur)
{
        DDSURFACEDESC ddsd;
        int err;

        bzero(&ddsd,sizeof(ddsd));
	ddsd.dwSize=sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;
	if ((err=sur->lpVtbl->GetSurfaceDesc(sur,&ddsd))!=DD_OK) return dd_error("GetSurfaceDesc(ddcs[s])",err);

        if (ddsd.ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY) return 0; // if it's not there, it has to be in video memory - works for funnymemory as well

        // note("%dx%d,%d,%d",ddsd.dwHeight,ddsd.dwWidth,ddsd.u1.lPitch/2,ddsd.u1.lPitch);

        return ddsd.dwHeight*ddsd.u1.lPitch;
}

static int ddcs_set_info(int s, int usemem,int xmax,int ymax)
{
        DDSURFACEDESC ddsd;
        char buf[1024];
        int err;

        bzero(&ddsd,sizeof(ddsd));
	ddsd.dwSize=sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;
	if ((err=ddcs[s]->lpVtbl->GetSurfaceDesc(ddcs[s],&ddsd))!=DD_OK) return dd_error("GetSurfaceDesc(ddcs[s])",err);
        note("ddcs[%d] is %s",s,ddsdstr(buf,&ddsd));
	
        ddcs_usemem[s]=usemem;
        ddcs_xres[s]=ddsd.u1.lPitch/2;
        ddcs_yres[s]=ddsd.dwHeight;
	ddcs_xmax[s]=xmax;
        ddcs_ymax[s]=ymax;
        ddcs_tdx[s]=ddcs_xres[s]/TILESIZEDX;
        ddcs_tdy[s]=ddcs_yres[s]/TILESIZEDY;
        ddcs_tiles[s]=ddcs_tdx[s]*ddcs_tdy[s];
        ddcs_total+=ddcs_tiles[s];

        return 0;
}

void dd_get_client_info(struct client_info *ci)
{
	int n;
	DDCAPS caps;
	static MEMORYSTATUS memstat;

	bzero(&caps,sizeof(caps));
	caps.dwSize=sizeof(caps);	
	dd->lpVtbl->GetCaps(dd,&caps,NULL);

	bzero(&memstat,sizeof(memstat));
	memstat.dwLength=sizeof(memstat);
	GlobalMemoryStatus(&memstat);
	
	ci->vidmemtotal=caps.dwVidMemTotal;
	ci->vidmemfree=caps.dwVidMemFree;
	
	ci->systemtotal=memstat.dwTotalPhys;
	ci->systemfree=memstat.dwAvailPhys;

	bzero(ci->surface,sizeof(ci->surface));
	for (n=0; n<min(ddcs_used,CL_MAX_SURFACE); n++) {
		ci->surface[n].xres=ddcs_xres[n];
		ci->surface[n].yres=ddcs_yres[n];
		ci->surface[n].type=ddcs_usemem[n];
	}
}

int dd_set_color_key(void)
{
	DDCOLORKEY key;
	int err;
	int s;

        note("colorkey=0x%X/0x%X - 0x%X - 0x%X",rgbcolorkey,scrcolorkey,scr2rgb[scrcolorkey],R_MASK|B_MASK);

	key.dwColorSpaceLowValue=scrcolorkey;
	key.dwColorSpaceHighValue=scrcolorkey;

        for (s=0; s<ddcs_used; s++) {
                if ((err=ddcs[s]->lpVtbl->SetColorKey(ddcs[s],DDCKEY_SRCBLT,&key))!=DD_OK) return dd_error("SetColorKey()",err);
        }
	
        return 0;
}

int get_free_vidmem(int *free)
{
        int err;
        DDCAPS ddcaps;

        *free=0;

        // determin free video memory
        bzero(&ddcaps,sizeof(ddcaps));
        ddcaps.dwSize=sizeof(ddcaps);
        if ((err=dd->lpVtbl->GetCaps(dd,&ddcaps,NULL))!=DD_OK) return err;
        // note("videomemory total=%.1fM free=%.1fM used=%.1fM",ddcaps.dwVidMemTotal/(1024*1024.0),ddcaps.dwVidMemFree/(1024*1024.0),dd_vidmeminuse/(1024*1024.0));
        note("videomemory total=%db free=%db used=%db",ddcaps.dwVidMemTotal,ddcaps.dwVidMemFree,dd_vidmeminuse);

        *free=ddcaps.dwVidMemFree;

        return DD_OK;
}

int dd_init(int width, int height)
{
	DDSURFACEDESC ddsd;
        int err,s,r,flags,freevidmem;
        char buf[1024];

        // return dd_init_old(width,height);

        note("NEW starting %s using %s (%dx%d)",dd_windowed?"WINDOWED":"FULLSCREEN",dd_usesysmem?"system memory":"video memory",width,height);

        // create dd
        if ((err=DirectDrawCreate(NULL,&dd,NULL))!=DD_OK) return dd_error("DirectDrawCreate()",err);

        // determin current display mode (needed for windowed mode, or if we should auto-detect the offscreen size)
        if (!width || dd_windowed) {
                bzero(&ddsd,sizeof(ddsd));
                ddsd.dwSize=sizeof(ddsd);
                ddsd.dwFlags=DDSD_ALL;
                if ((err=dd->lpVtbl->GetDisplayMode(dd,&ddsd))!=DD_OK) return dd_error("GetDisplayMode()",err);
                note("mode %s",ddsdstr(buf,&ddsd));
                R_MASK=ddsd.ddpfPixelFormat.u2.dwRBitMask;
                G_MASK=ddsd.ddpfPixelFormat.u3.dwGBitMask;
                B_MASK=ddsd.ddpfPixelFormat.u4.dwBBitMask;
                XRES=ddsd.dwWidth;
                YRES=ddsd.dwHeight;
                BPP=ddsd.ddpfPixelFormat.u1.dwRGBBitCount;
        }

        // you can force any screen (and offscreen) size
        if (width) {
                XRES=width;
                YRES=height;
        }

        if (dd_windowed) {

                // quick 32bit hack
                if (BPP!=16 && !dd_usesysmem) {
                        note("forceing back surface into systemmemory! switch to a 16bit display mode to avoid this.");
                        dd_usesysmem=1;
                }

                // set cooperative level
                if ((err=dd->lpVtbl->SetCooperativeLevel(dd,mainwnd,DDSCL_NORMAL))!=DD_OK) return dd_error("SetCooperativeLevel()",err);

                // get the primary surface
                bzero(&ddsd,sizeof(ddsd));
                ddsd.dwSize=sizeof(ddsd);
                ddsd.dwFlags=DDSD_CAPS;
                ddsd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
                if ((err=dd->lpVtbl->CreateSurface(dd,&ddsd,&ddps,NULL))!=DD_OK) return dd_error("CreateSurface(ddps)",err);

                // create a back surface (offscreen, always using a 16 bit mode, prefering the one of the current screen mode if this is also 16 bit)
                bzero(&ddsd,sizeof(ddsd));
                ddsd.dwSize=sizeof(ddsd);
                ddsd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
                if (dd_usesysmem) ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
                else ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
                ddsd.dwWidth=XRES;
                ddsd.dwHeight=YRES;
                ddsd.ddpfPixelFormat.dwSize=sizeof(ddsd.ddpfPixelFormat);
                ddsd.ddpfPixelFormat.dwFlags=DDPF_RGB;
                ddsd.ddpfPixelFormat.u1.dwRGBBitCount=16;
                if (BPP==16) {
                        // current
                        ddsd.ddpfPixelFormat.u2.dwRBitMask=R_MASK;
                        ddsd.ddpfPixelFormat.u3.dwGBitMask=G_MASK;
                        ddsd.ddpfPixelFormat.u4.dwBBitMask=B_MASK;
                        ddsd.ddpfPixelFormat.u5.dwRGBAlphaBitMask=0;
                }
                else {
                        // RGBM_R5G6B5
                        ddsd.ddpfPixelFormat.u2.dwRBitMask=0xF800;
                        ddsd.ddpfPixelFormat.u3.dwGBitMask=0x07E0;
                        ddsd.ddpfPixelFormat.u4.dwBBitMask=0x001F;
                        ddsd.ddpfPixelFormat.u5.dwRGBAlphaBitMask=0;
                }
                if ((err=dd->lpVtbl->CreateSurface(dd,&ddsd,&ddbs,NULL))!=DD_OK) return dd_error("CreateSurface(ddbs)",err);    // create Backsurface

                // do some neccassary clipper stuff
                if ((err=dd->lpVtbl->CreateClipper(dd,0,&ddcl,NULL))!=DD_OK) return dd_error("CreateClipper(ddbs)",err);        // CreateClipper
                if ((err=ddcl->lpVtbl->SetHWnd(ddcl,0,mainwnd))!=DD_OK) return dd_error("SetHWnd(ddcl)",err);                   // Attach Clipper to Window
                if ((err=ddps->lpVtbl->SetClipper(ddps,ddcl))!=DD_OK) return dd_error("SetClipper(ddps)",err);                  // Attach Clipper to ddps
        }
        else {
                // set cooperative level
	        if ((err=dd->lpVtbl->SetCooperativeLevel(dd,mainwnd,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))!=DD_OK) return dd_error("SetCooperativeLevel()",err);

                // set the display mode (16bit)
                if ((err=dd->lpVtbl->SetDisplayMode(dd,XRES,YRES,16))!=DD_OK) return dd_error("SetDisplayMode()",err);

                // create the primary surface with one back surface
        	bzero(&ddsd,sizeof(ddsd));
        	ddsd.dwSize=sizeof(ddsd);
        	ddsd.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT;
                if (dd_usesysmem) ddsd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_SYSTEMMEMORY;
        	else ddsd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
                ddsd.dwBackBufferCount=1;
        	if ((err=dd->lpVtbl->CreateSurface(dd,&ddsd,&ddps,NULL))!=DD_OK) return dd_error("CreateSurface(ddps)",err);

                // get the back surface
                bzero(&ddsd,sizeof(ddsd));
        	ddsd.dwSize=sizeof(ddsd);
        	ddsd.dwFlags=DDSD_CAPS;
        	ddsd.ddsCaps.dwCaps=DDSCAPS_BACKBUFFER;
                if ((err=ddps->lpVtbl->GetAttachedSurface(ddps,&ddsd.ddsCaps,&ddbs))!=DD_OK) return dd_error("GetAttachedSurface(ddbs)",err);
        }

        // now check the display mode
        bzero(&ddsd,sizeof(ddsd));
        ddsd.dwSize=sizeof(ddsd);
        ddsd.dwFlags=DDSD_ALL;
        if ((err=dd->lpVtbl->GetDisplayMode(dd,&ddsd))!=DD_OK) return dd_error("GetDisplayMode()",err);
        if (ddsd.ddpfPixelFormat.u1.dwRGBBitCount!=16) note("16 bit display modes might be faster"); // return dd_error("need a 16bit screen mode",-1);
        note("ddps %s",ddsdstr(buf,&ddsd));

        // get informations about the back surface
        bzero(&ddsd,sizeof(ddsd));
	ddsd.dwSize=sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;
	if ((err=ddbs->lpVtbl->GetSurfaceDesc(ddbs,&ddsd))!=DD_OK) return dd_error("GetSurfaceDesc(ddbs)",err);
	note("ddbs is %s",ddsdstr(buf,&ddsd));
        dd_usevidmem=ddsd.ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY;
	xres=ddsd.u1.lPitch/2;
	yres=ddsd.dwHeight;
        BPP=ddsd.ddpfPixelFormat.u1.dwRGBBitCount;

        if (!(ddsd.ddpfPixelFormat.dwFlags&DDPF_RGB)) return dd_error("CANNOT HANDLE PIXEL FORMAT",-1);
        R_MASK=ddsd.ddpfPixelFormat.u2.dwRBitMask;
        G_MASK=ddsd.ddpfPixelFormat.u3.dwGBitMask;
        B_MASK=ddsd.ddpfPixelFormat.u4.dwBBitMask;

        if (R_MASK==0xF800 && G_MASK==0x07E0 && B_MASK==0x001F) { rgbm=RGBM_R5G6B5; D_MASK=0xE79C; }
        else if (R_MASK==0x7C00 && G_MASK==0x03E0 && B_MASK==0x001F) { rgbm=RGBM_X1R5G5B5; D_MASK=0x739C; }
        else if (R_MASK==0x001F && G_MASK==0x07E0 && B_MASK==0xF800) { rgbm=RGBM_B5G6R5; D_MASK=0xE79C; }
        else return dd_error("CANNOT HANDLE RGB MASK",-1);

        note("rgbm=%s BPP=%d %s",rgbmstr[rgbm],BPP,dd_usevidmem?"videomemory":"non-videomemory");

        // determin how much videomemory is in use so far
        dd_vidmeminuse+=dd_vidmembytes(ddps);
        dd_vidmeminuse+=dd_vidmembytes(ddbs);

        // create some local video memory cache surfaces (using the format of the back surface)
        while (dd_usevidmem && ddcs_used<DDCS_MAX) {

                // get_free_vidmem(&freevidmem);

                for (r=0; r<max_restab; r++) {

                        if (ddcs_total+(restab[r][0]/TILESIZEDX)*(restab[r][1]/TILESIZEDY)>dd_maxtile || ddcs_total>=dd_maxtile) continue;
                        // if (restab[r][0]*restab[r][1]*2>freevidmem) { note("skip"); continue; }
			
			// dont allocate the real small surfaces if we a lot tiles already
                        //if (ddcs_total/10>(restab[r][0]/TILESIZEDX)*(restab[r][1]/TILESIZEDY)) continue;
			

                        bzero(&ddsd,sizeof(ddsd));
                        ddsd.dwSize=sizeof(ddsd);
                        ddsd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
                        ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY|DDSCAPS_LOCALVIDMEM;
                        ddsd.dwWidth=restab[r][0];
                        ddsd.dwHeight=restab[r][1];
                        ddsd.ddpfPixelFormat.dwSize=sizeof(ddsd.ddpfPixelFormat);
                        ddsd.ddpfPixelFormat.dwFlags=DDPF_RGB;
                        ddsd.ddpfPixelFormat.u1.dwRGBBitCount=BPP;
                        ddsd.ddpfPixelFormat.u2.dwRBitMask=R_MASK;
                        ddsd.ddpfPixelFormat.u3.dwGBitMask=G_MASK;
                        ddsd.ddpfPixelFormat.u4.dwBBitMask=B_MASK;
                        ddsd.ddpfPixelFormat.u5.dwRGBAlphaBitMask=0;
                        if ((err=dd->lpVtbl->CreateSurface(dd,&ddsd,&ddcs[ddcs_used],NULL))!=DD_OK) continue;
                        if (ddcs_set_info(ddcs_used,DD_LOCMEM,restab[r][0],restab[r][1])==-1) return -1;
                        dd_vidmeminuse+=dd_vidmembytes(ddcs[ddcs_used]);
                        ddcs_used++;

                        break;
                }
                if (r==max_restab) break;
        }

        // create some video memory cache surfaces (using the format of the back surface)
        if (ddcs_total<150) while (dd_usevidmem && ddcs_used<DDCS_MAX) {

                // get_free_vidmem(&freevidmem);

                for (r=0; r<max_restab; r++) {

                        if (ddcs_total+(restab[r][0]/TILESIZEDX)*(restab[r][1]/TILESIZEDY)>dd_maxtile || ddcs_total>=dd_maxtile) continue;
                        // if (restab[r][0]*restab[r][1]*2>freevidmem) { note("skip"); continue; }

                        bzero(&ddsd,sizeof(ddsd));
                        ddsd.dwSize=sizeof(ddsd);
                        ddsd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
                        ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
                        ddsd.dwWidth=restab[r][0];
                        ddsd.dwHeight=restab[r][1];
                        ddsd.ddpfPixelFormat.dwSize=sizeof(ddsd.ddpfPixelFormat);
                        ddsd.ddpfPixelFormat.dwFlags=DDPF_RGB;
                        ddsd.ddpfPixelFormat.u1.dwRGBBitCount=BPP;
                        ddsd.ddpfPixelFormat.u2.dwRBitMask=R_MASK;
                        ddsd.ddpfPixelFormat.u3.dwGBitMask=G_MASK;
                        ddsd.ddpfPixelFormat.u4.dwBBitMask=B_MASK;
                        ddsd.ddpfPixelFormat.u5.dwRGBAlphaBitMask=0;
                        if ((err=dd->lpVtbl->CreateSurface(dd,&ddsd,&ddcs[ddcs_used],NULL))!=DD_OK) continue;
                        if (ddcs_set_info(ddcs_used,DD_VIDMEM,restab[r][0],restab[r][1])==-1) return -1;
                        dd_vidmeminuse+=dd_vidmembytes(ddcs[ddcs_used]);
                        ddcs_used++;
                        break;
                }
                if (r==max_restab) break;
        }

        note("----");
        get_free_vidmem(&freevidmem);

        // display information about the cache surfaces
        for (s=0; s<ddcs_used; s++) {
                note("ddcs[%d] memtype=%d %d*%d=%d tiles",s,ddcs_usemem[s],ddcs_xres[s]/TILESIZEDX,ddcs_yres[s]/TILESIZEDY,ddcs_tiles[s]);
        }
        note("total_tiles=%d (maxtiles=%d)",ddcs_total,dd_maxtile);

        //if (ddcs_total<2) return dd_error("NOT ENOUGH TILES",-1);

        // initialize cache (will initialize color tables, too)
        if (dd_init_cache()==-1) return -1;

        // set the color key of all cache surfaces
        dd_set_color_key();

        // set the clipping to the maximum possible
        clippos=0;
        clipsx=0;
        clipsy=0;
        clipex=XRES;
        clipey=YRES;

        // initialize the gfx loading stuff - TODO: call this in dd_init_cache();
        gfx_init();

	dd_create_font();
	dd_init_text();

	dd_black();

	return 0;
}

int dd_exit(void)
{
        int s,left;

	// removed - slow!
        //gfx_exit();
        //dd_exit_cache();

        for (s=0; s<ddcs_used; s++) {
                left=ddcs[s]->lpVtbl->Release(ddcs[s]);
                ddcs[s]=NULL;
                // note("released ddcs[%d]. %d references left",s,left);
        }
        ddcs_used=0;
        ddcs_total=0;
	
        if (ddps) {
                left=ddps->lpVtbl->Release(ddps);
                ddps=NULL;
                // note("released ddps. %d references left",left);
        }

        if (ddcl) {
                left=ddcl->lpVtbl->Release(ddcl);
                ddps=NULL;
                // note("released ddcl. %d references left",left);
        }
	
        if (dd) {
		dd->lpVtbl->RestoreDisplayMode(dd);
		left=dd->lpVtbl->Release(dd);
		dd=NULL;
                // note("released dd. %d references left",left);
	}

        dd_vidmeminuse=0;

        if (left==12345678) return left; // grrr

        return 0;
}

void *dd_lock_surface(LPDIRECTDRAWSURFACE surface)
{
	DDSURFACEDESC ddsd;
	int err;

        bzero(&ddsd,sizeof(ddsd));
	ddsd.dwSize=sizeof(ddsd);					
	if ((err=surface->lpVtbl->Lock(surface,NULL,&ddsd,DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT,NULL))!=DD_OK) { dd_error("Lock()",err); return NULL; }
	
        return ddsd.lpSurface;
}

int dd_unlock_surface(LPDIRECTDRAWSURFACE surface)
{
	int err;

	if ((err=surface->lpVtbl->Unlock(surface,NULL))!=DD_OK) return dd_error("Unlock()",err);

        return 0;
}

HDC dd_get_dc(LPDIRECTDRAWSURFACE surface)
{
	int err;
        HDC dc;

	if ((err=surface->lpVtbl->GetDC(surface,&dc))!=DD_OK) { dd_error("dd_get_dc()",err); return NULL; }

        return dc;
}

int dd_release_dc(LPDIRECTDRAWSURFACE surface, HDC dc)
{
	int err;

        if ((err=surface->lpVtbl->ReleaseDC(surface,dc))!=DD_OK) return dd_error("dd_release_dc()",err);

        return 0;
}

void *dd_lock_ptr(void)
{
        return dd_lock_surface(ddbs);
}

int dd_unlock_ptr(void)
{
        return dd_unlock_surface(ddbs);
}

void dd_flip(void)
{
	int err;


        if (dd_windowed) {
                HDC srcdc;
                HDC tgtdc;

		srcdc=dd_get_dc(ddbs);
                tgtdc=GetDC(mainwnd);

                if (srcdc && tgtdc) BitBlt(tgtdc,0,0,XRES,YRES,srcdc,0,0,SRCCOPY);

		dd_release_dc(ddbs,srcdc);
		ReleaseDC(mainwnd,tgtdc);
        }
        else
        /*if (dd_windowed) {
                POINT p;
                RECT wrc,brc;

                p.x=0;
                p.y=0;
                ClientToScreen(mainwnd,&p);
                wrc.left=p.x;
                wrc.top=p.y;
                wrc.right=wrc.left+XRES;
                wrc.bottom=wrc.top+YRES;

                brc.left=0;
                brc.top=0;
                brc.right=XRES;
                brc.bottom=YRES;

	        if ((err=ddps->lpVtbl->Blt(ddps,&wrc,ddbs,&brc,DDBLT_WAIT,NULL))!=DD_OK) dd_error("flipping Blt(ddps,ddbs)",err);
        }
        else */{
                if ((err=ddps->lpVtbl->Flip(ddps,NULL,DDFLIP_WAIT))!=DD_OK) dd_error("Flip(ddps)",err);
        }
}

int dd_islost(void)
{
	int s;

	if (ddps->lpVtbl->IsLost(ddps)!=DD_OK) return 1;// ddps->lpVtbl->Restore(ddps); else ok++;
	if (ddbs->lpVtbl->IsLost(ddbs)!=DD_OK) return 1;// ddbs->lpVtbl->Restore(ddbs); else ok++;
        for (s=0; s<ddcs_used; s++) if (ddcs[s]->lpVtbl->IsLost(ddcs[s])!=DD_OK) return 1;

        return 0;
}

int dd_restore(void)
{
	int s;
        static int cacheflag=0;

	if (ddps->lpVtbl->IsLost(ddps)!=DD_OK) if (ddps->lpVtbl->Restore(ddps)!=DD_OK) return -1;
	if (ddbs->lpVtbl->IsLost(ddbs)!=DD_OK) if (ddbs->lpVtbl->Restore(ddbs)!=DD_OK) return -1;

        for (s=0; s<ddcs_used; s++) {
                if (ddcs[s]->lpVtbl->IsLost(ddcs[s])!=DD_OK) {
                        cacheflag=1;
                        if (ddcs[s]->lpVtbl->Restore(ddcs[s])!=DD_OK) return -1;
                }
        }

        if (cacheflag) { dd_reset_cache(0,0,1); cacheflag=0; }

	dd_black();

        return 0;
}

// cache ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int vget(int tx, int ty, int tdx, int tdy)
{
        int val;

        val=tx + ty*tdx;
        PARANOIA( if (val<0 || val>tdx*tdy) paranoia("vget: val(%d) to large (tx=%d ty=%d tdx=%d tdy=%d",tx,ty,tdx,tdy); )

        return val;
}


#define VIDX_NONE   0xFFFF              // still not set (invalid position)
#define VIDX_KILL   0xFFFE              // was empty so there was no need to store it
#define SIDX_NONE       -1              // still not set (invalid position) set in videocache[v].sidx
#define SPRITE_NONE     -1
#define IIDX_NONE       -1

struct imagecache
{
        int prev;                       // last used list
        int next;


        int hnext,hprev;		// hash link

        int sprite;                     // if sprite==SPRITE_NONE this entry is free

        IMAGE image;
};

typedef struct imagecache IMAGECACHE;

struct apix
{
        unsigned char x;
        unsigned char y;
        unsigned char a;
        unsigned short int rgb;
};

typedef struct apix APIX;

struct systemcache
{
        int prev;                       // last used list
        int next;

        // fx
        int sprite;                     // if sprite==SPRITE_NONE this entry is free

	signed char sink;
	unsigned char scale;
	char cr,cg,cb,light,sat;
        unsigned short c1,c2,c3,shine;

	unsigned char freeze;
	unsigned char grid;

	// light
	char ml,ll,rl,ul,dl;

	// hash links
	int hnext,hprev;

        // primary
        unsigned short xres;            // x resolution in pixels
        unsigned short yres;            // y resolution in pixels
        unsigned char tdx;              // x-resolution in tiles
        unsigned char tdy;              // y-resolution in tiles
        short xoff;                     // offset to blit position
        short yoff;                     // offset to blit position
	short int size;

        unsigned short *rgb;            // colors in internal screen format
        short acnt;                     // number of alpha pixel
        APIX *apix;                     // alpha pixel

        // vc access
	unsigned int used;
	unsigned int lost;
	unsigned int tick;

	struct vid_cache **vc;
};

typedef struct systemcache SYSTEMCACHE;

int max_imagecache=0;
int max_systemcache=0;

IMAGECACHE *imagecache;
SYSTEMCACHE *systemcache;

int sidx_best,sidx_last;
int iidx_best,iidx_last;

unsigned short rgbcolorkey=IRGB(31,0,31);
unsigned short rgbcolorkey2=IRGB(16,0,16);
unsigned short scrcolorkey;
unsigned short *rgb2scr=NULL;
unsigned short *scr2rgb=NULL;
#define MAX_RGBFX_LIGHT                 16
unsigned short **rgbfx_light;           // 16 light values (0-14=light 15=bright)
unsigned short **rgbfx_freeze;          // DDFX_MAX_FREEZE light values (0 is empty, no freeze - take care empty=NULL)

#define IMA_HASH_SIZE	1024
#define SYS_HASH_SIZE	1024
static int *ima_hash;   // [IMA_HASH_SIZE];
static int *sys_hash;   // [SYS_HASH_SIZE],sys_hash_init=0;


int sc_miss=0,sc_hit=0,sc_maxstep=0;
int vc_miss=0,vc_hit=0,vc_unique=0,vc_unique24=0,sc_blits=0;

int dd_init_cache(void)
{
        int scr,rgb,r,g,b,s,i,v,x,y;
        int tr,tg,tb;
        //unsigned char gammatab[32];
        //unsigned char darkitab[16][32];

        // perfect testing values for luzia max_imagecacge=100; max_systemcache=200; max_videocache=512;

        // imagecache
        max_imagecache=1024;
        imagecache=xcalloc(max_imagecache*sizeof(IMAGECACHE),MEM_GLOB);
        for (i=0; i<max_imagecache; i++) {
                imagecache[i].sprite=SPRITE_NONE;
                imagecache[i].prev=i-1;
                imagecache[i].next=i+1;
                imagecache[i].hnext=-1;
                imagecache[i].hprev=-1;
        }
        imagecache[0].prev=IIDX_NONE;
        imagecache[max_imagecache-1].next=IIDX_NONE;
        iidx_best=0;
        iidx_last=max_imagecache-1;

        ima_hash=xmalloc(IMA_HASH_SIZE*sizeof(int),MEM_GLOB);
        for (i=0; i<IMA_HASH_SIZE; i++) ima_hash[i]=-1;

        // systemcache
        max_systemcache=1024*10; //*20;
        systemcache=xcalloc(max_systemcache*sizeof(SYSTEMCACHE),MEM_GLOB);
        for (i=0; i<max_systemcache; i++) {
                systemcache[i].sprite=SPRITE_NONE;
                systemcache[i].prev=i-1;
                systemcache[i].next=i+1;
                systemcache[i].hnext=-1;
                systemcache[i].hprev=-1;
        }
        systemcache[0].prev=SIDX_NONE;
        systemcache[max_systemcache-1].next=SIDX_NONE;
        sidx_best=0;
        sidx_last=max_systemcache-1;

        sys_hash=xmalloc(SYS_HASH_SIZE*sizeof(int),MEM_GLOB);
        for (i=0; i<SYS_HASH_SIZE; i++) sys_hash[i]=-1;


        // tables
        rgb2scr=xmalloc(65536*sizeof(unsigned short),MEM_GLOB);
        scr2rgb=xmalloc(65536*sizeof(unsigned short),MEM_GLOB);

        for (scr=0; scr<65536; scr++) {

                if (rgbm==RGBM_R5G6B5) {
                        // rrrrrggggggbbbbb
                        r=(scr>>11)&0x1F;
                        g=(scr>>6)&0x1F;
                        b=(scr>>0)&0x1F;
                }
                else if (rgbm==RGBM_X1R5G5B5) {
                        // xrrrrrgggggbbbbb
                        r=(scr>>10)&0x1F;
                        g=(scr>>5)&0x1F;
                        b=(scr>>0)&0x1F;
                }
                else if (rgbm==RGBM_B5G6R5) {
                        // bbbbbggggggrrrrr
                        r=(scr>>0)&0x1F;
                        g=(scr>>6)&0x1F;
                        b=(scr>>11)&0x1F;
                }
                else return -1;

                scr2rgb[scr]=IRGB(r,g,b);
                rgb2scr[IRGB(r,g,b)]=scr;
        }

        scrcolorkey=R_MASK|B_MASK;
        scr2rgb[scrcolorkey]=IRGB(31,31,0);
        rgb2scr[rgbcolorkey]=scrcolorkey;

        /*for (v=0; v<=31; v++) {
                gammatab[v]=(pow((double)v/31.0,gamma)*31.0+0.5);
        }

        for (i=1; i<=15; i++) {
                // double darken=1+(15-i)*0.06 + (i==1?0.5:0);
                double darken=1+(15-i)*(15-i)*0.003;
                for (v=0; v<=31; v++) {
                        darkitab[i][v]=(pow((double)v/32.0,darken)*32.0+0.5);
                }
        }*/

        // light
        rgbfx_light=xcalloc(MAX_RGBFX_LIGHT*sizeof(unsigned short int *),MEM_GLOB);
        for (i=0; i<16; i++) rgbfx_light[i]=xcalloc(65536*sizeof(unsigned short),MEM_GLOB);
        for (rgb=0; rgb<32768; rgb++) rgbfx_light[0][rgb]=IRGB( min(31,2*IGET_R(rgb)+4), min(31,2*IGET_G(rgb)+4), min(31,2*IGET_B(rgb)+4) );
        for (i=1; i<=15; i++) {
		for (rgb=0; rgb<32768; rgb++) {
	
			r=IGET_R(rgb);
			g=IGET_G(rgb);
			b=IGET_B(rgb);
	
#if 0                   // 1=ankmode 0=dbmode
			{
				int gray,scal;
                                const int grayshift=8; // 8;
                                const int darkshift=6; // 4;// 8 for db (guess); but 4 is already to bright for me:(
                                // const int scalshift=64; // 0;
				
				gray=(6969*r+23434*g+2365*b)/32768;
                                // scal=max(max(r,g),b);

                                r=(i+darkshift)*( (14-(i-1))*gray + ((i-1)+grayshift)*r )/((14+grayshift)*(15+darkshift));
                                g=(i+darkshift)*( (14-(i-1))*gray + ((i-1)+grayshift)*g )/((14+grayshift)*(15+darkshift));
                                b=(i+darkshift)*( (14-(i-1))*gray + ((i-1)+grayshift)*b )/((14+grayshift)*(15+darkshift));
                                //r=( ( ((31-scal)*/*((14-(i-1))*gray + ((i-1)+grayshift)*r)/(14+grayshift))*/r) ) + ((i+darkshift)*( (14-(i-1))*gray + ((i-1)+grayshift)*r )/((14+grayshift)*(15+darkshift)))*(scal+scalshift) )/(31+scalshift);
                                //g=( ( ((31-scal)*/*((14-(i-1))*gray + ((i-1)+grayshift)*g)/(14+grayshift))*/g) ) + ((i+darkshift)*( (14-(i-1))*gray + ((i-1)+grayshift)*g )/((14+grayshift)*(15+darkshift)))*(scal+scalshift) )/(31+scalshift);
                                //b=( ( ((31-scal)*/*((14-(i-1))*gray + ((i-1)+grayshift)*b)/(14+grayshift))*/b) ) + ((i+darkshift)*( (14-(i-1))*gray + ((i-1)+grayshift)*b )/((14+grayshift)*(15+darkshift)))*(scal+scalshift) )/(31+scalshift);
                        }
#else
                        {
                                static int lightmulti[16]={ 0, 3, 8,16,18,20,22,24,25,26,27,28,29,30,31,32};
				//static int lightmulti[16]={ 0, 1, 2, , 1,10,22,24,25,26,27,28,29,30,31,32};
                                int lm=lightmulti[i],le=0; //dd_lighteffect;
				int sum,gray;
				
				//if (newlight) le=i;

                                r=min(31,(lm+le)*(r*dd_gamma/8)/(32+le));
				g=min(31,(lm+le)*(g*dd_gamma/8)/(32+le));
				b=min(31,(lm+le)*(b*dd_gamma/8)/(32+le));
                        }
#endif


                        if (r<0 || r>31 || g<0 || g>31 || b<0 || b>31) paranoia("some ill rgbfx_light here r=%d g=%d b=%d i=%d",r,g,b,i);
	
			rgbfx_light[i][rgb]=IRGB(r,g,b);
                }
        }

        // freeze
        rgbfx_freeze=xcalloc(DDFX_MAX_FREEZE*sizeof(unsigned short int *),MEM_GLOB);
        for (i=1; i<DDFX_MAX_FREEZE; i++) rgbfx_freeze[i]=xcalloc(65536*sizeof(unsigned short),MEM_GLOB);
        for (i=1; i<DDFX_MAX_FREEZE; i++) {
                for (rgb=0; rgb<32768; rgb++) {

			r=IGET_R(rgb);
			g=IGET_G(rgb);
			b=IGET_B(rgb);

                        r=min(31,r+31*  i/(3*DDFX_MAX_FREEZE-1));
                        g=min(31,g+31*  i/(3*DDFX_MAX_FREEZE-1));
                        b=min(31,b+31*3*i/(3*DDFX_MAX_FREEZE-1));

                        if (r<0 || r>31 || g<0 || g>31 || b<0 || b>31) paranoia("some ill rgbfx_light here r=%d g=%d b=%d i=%d",r,g,b,i);
	
			rgbfx_freeze[i][rgb]=IRGB(r,g,b);
                }
        }


        return 0;
}

void dd_exit_cache(void)
{
        int i,s;

        // tables
        xfree(rgb2scr);
        rgb2scr=NULL;
        xfree(scr2rgb);
        scr2rgb=NULL;

        // freeze
        if (rgbfx_freeze) {
                for (i=1; i<DDFX_MAX_FREEZE; i++) xfree(rgbfx_freeze[i]);
                xfree(rgbfx_freeze);
                rgbfx_freeze=NULL;
        }

        // light
        if (rgbfx_light) {
                for (i=0; i<16; i++) xfree(rgbfx_light[i]);
                xfree(rgbfx_light);
                rgbfx_light=NULL;
        }

        // systemcache
        for (i=0; i<max_systemcache; i++) {
                xfree(systemcache[i].rgb);
                xfree(systemcache[i].apix);
		xfree(systemcache[i].vc);
        }
        xfree(systemcache);
        systemcache=NULL;
        max_systemcache=0;

        xfree(sys_hash);
        sys_hash=NULL;

        // imagecache
        for (i=0; i<max_imagecache; i++) {
                xfree(imagecache[i].image.rgb);
                xfree(imagecache[i].image.a);
        }
        xfree(imagecache);
        imagecache=NULL;
        max_imagecache=0;

        xfree(ima_hash);
        ima_hash=NULL;

        return;
}

int dd_reset_cache(int reset_image, int reset_system, int reset_video)
{
        int i,iidx,sidx,vidx;
        int vmax,v;

        note("reset%s%s%s",reset_image?" image_cache":"",reset_system?" system_cache":"",reset_video?" video_cache":"");

        // imagecache
        if (reset_image) {
                for (iidx=0; iidx<max_imagecache; iidx++) {
                        if (imagecache[iidx].sprite==SPRITE_NONE) continue;
                        imagecache[iidx].sprite=SPRITE_NONE;
                        imagecache[iidx].hnext=-1;
                        imagecache[iidx].hprev=-1;
                }

                for (i=0; i<IMA_HASH_SIZE; i++) ima_hash[i]=-1;
        }

        // systemcache
        if (reset_system) {
                for (sidx=0; sidx<max_systemcache; sidx++) {

                        if (systemcache[sidx].sprite==SPRITE_NONE) continue;

                        vmax=systemcache[sidx].tdx*systemcache[sidx].tdy;
			for (v=0; v<vmax; v++) {
				if (systemcache[sidx].vc[v]==((void*)(-1))) systemcache[sidx].vc[v]=NULL;
				if (systemcache[sidx].vc[v]==NULL) continue;
				
				vid_free_cache(systemcache[sidx].vc[v]);
				systemcache[sidx].vc[v]=NULL;
			}

                        systemcache[sidx].sprite=SPRITE_NONE;
                }

                for (i=0; i<SYS_HASH_SIZE; i++) sys_hash[i]=-1;
        }

        // videocache
        if (reset_video) {
		vid_reset();
        }

        return 0;
}

static void ic_best(int iidx)
{
        PARANOIA( if (iidx==IIDX_NONE) paranoia("ic_best(): iidx=IIDX_NONE"); )
        PARANOIA( if (iidx>=max_imagecache) paranoia("ic_best(): iidx>max_imagecache (%d>=%d)",iidx,max_imagecache); )

        if (imagecache[iidx].prev==IIDX_NONE) {

                PARANOIA( if (iidx!=iidx_best) paranoia("ic_best(): iidx should be best"); )

                return;
        }
        else if (imagecache[iidx].next==IIDX_NONE) {

                PARANOIA( if (iidx!=iidx_last) paranoia("ic_best(): iidx should be last"); )

                iidx_last=imagecache[iidx].prev;
                imagecache[iidx_last].next=IIDX_NONE;
                imagecache[iidx_best].prev=iidx;
                imagecache[iidx].prev=IIDX_NONE;
                imagecache[iidx].next=iidx_best;
                iidx_best=iidx;

                return;
        }
        else {
                imagecache[imagecache[iidx].prev].next=imagecache[iidx].next;
                imagecache[imagecache[iidx].next].prev=imagecache[iidx].prev;
                imagecache[iidx].prev=IIDX_NONE;
                imagecache[iidx].next=iidx_best;
                imagecache[iidx_best].prev=iidx;
                iidx_best=iidx;
                return;
        }
}

static void ic_last(int iidx)
{
        PARANOIA( if (iidx==IIDX_NONE) paranoia("ic_last(): iidx=IIDX_NONE"); )
        PARANOIA( if (iidx>=max_imagecache) paranoia("ic_last(): iidx>max_imagecache (%d>=%d)",iidx,max_imagecache); )

        if (imagecache[iidx].next==IIDX_NONE) {

                PARANOIA( if (iidx!=iidx_last) paranoia("ic_last(): iidx should be last"); )

                return;
        }
        else if (imagecache[iidx].prev==IIDX_NONE) {

                PARANOIA( if (iidx!=iidx_best) paranoia("ic_last(): iidx should be best"); )

                iidx_best=imagecache[iidx].next;
                imagecache[iidx_best].prev=IIDX_NONE;
                imagecache[iidx_last].next=iidx;
                imagecache[iidx].prev=iidx_last;
                imagecache[iidx].next=IIDX_NONE;
                iidx_last=iidx;
        }
        else {
                imagecache[imagecache[iidx].prev].next=imagecache[iidx].next;
                imagecache[imagecache[iidx].next].prev=imagecache[iidx].prev;
                imagecache[iidx].prev=iidx_last;
                imagecache[iidx].next=IIDX_NONE;
                imagecache[iidx_last].next=iidx;
                iidx_last=iidx;
        }
}

static void sc_best(int sidx)
{
        PARANOIA( if (sidx==SIDX_NONE) paranoia("sc_best(): sidx=SIDX_NONE"); )
        PARANOIA( if (sidx>=max_systemcache) paranoia("sc_best(): sidx>max_systemcache (%d>=%d)",sidx,max_systemcache); )

        if (systemcache[sidx].prev==SIDX_NONE) {

                PARANOIA( if (sidx!=sidx_best) paranoia("sc_best(): sidx should be best"); )

                return;
        }
        else if (systemcache[sidx].next==SIDX_NONE) {

                PARANOIA( if (sidx!=sidx_last) paranoia("sc_best(): sidx should be last"); )

                sidx_last=systemcache[sidx].prev;
                systemcache[sidx_last].next=SIDX_NONE;
                systemcache[sidx_best].prev=sidx;
                systemcache[sidx].prev=SIDX_NONE;
                systemcache[sidx].next=sidx_best;
                sidx_best=sidx;

                return;
        }
        else {
                systemcache[systemcache[sidx].prev].next=systemcache[sidx].next;
                systemcache[systemcache[sidx].next].prev=systemcache[sidx].prev;
                systemcache[sidx].prev=SIDX_NONE;
                systemcache[sidx].next=sidx_best;
                systemcache[sidx_best].prev=sidx;
                sidx_best=sidx;
                return;
        }
}

static void sc_last(int sidx)
{
        PARANOIA( if (sidx==SIDX_NONE) paranoia("sc_last(): sidx=SIDX_NONE"); )
        PARANOIA( if (sidx>=max_systemcache) paranoia("sc_last(): sidx>max_systemcache (%d>=%d)",sidx,max_systemcache); )

        if (systemcache[sidx].next==SIDX_NONE) {

                PARANOIA( if (sidx!=sidx_last) paranoia("sc_last(): sidx should be last"); )

                return;
        }
        else if (systemcache[sidx].prev==SIDX_NONE) {

                PARANOIA( if (sidx!=sidx_best) paranoia("sc_last(): sidx should be best"); )

                sidx_best=systemcache[sidx].next;
                systemcache[sidx_best].prev=SIDX_NONE;
                systemcache[sidx_last].next=sidx;
                systemcache[sidx].prev=sidx_last;
                systemcache[sidx].next=SIDX_NONE;
                sidx_last=sidx;
        }
        else {
                systemcache[systemcache[sidx].prev].next=systemcache[sidx].next;
                systemcache[systemcache[sidx].next].prev=systemcache[sidx].prev;
                systemcache[sidx].prev=sidx_last;
                systemcache[sidx].next=SIDX_NONE;
                systemcache[sidx_last].next=sidx;
                sidx_last=sidx;
        }
}

static void sc_blit_apix(int sidx, int scrx, int scry, int grid, int freeze)
{
        int i,start;
        unsigned short int src,dst,r,g,b,a;
        unsigned short int *ptr;
        APIX *apix;
        int acnt,xoff,yoff,x,y,m;

        apix=systemcache[sidx].apix;
        acnt=systemcache[sidx].acnt;
        xoff=systemcache[sidx].xoff;
        yoff=systemcache[sidx].yoff;

	start=GetTickCount();

        if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return;

        for (i=0; i<acnt; i++,apix++) {
                if (grid==DDFX_LEFTGRID) { if ((xoff+apix->x+yoff+apix->y)&1) continue; }
                else if (grid==DDFX_RIGHTGRID) {  if ((xoff+apix->x+yoff+apix->y+1)&1) continue; }

                x=scrx+apix->x;
                y=scry+apix->y;
                if (x<clipsx || y<clipsy || x>=clipex || y>=clipey) continue;

                a=apix->a;
                src=apix->rgb;
                if (freeze) src=rgbfx_freeze[freeze][src];

		m=(x+x_offset)+(y+y_offset)*xres;

                dst=scr2rgb[ptr[m]];

                r=(a*IGET_R(src)+(31-a)*IGET_R(dst))/31;
                g=(a*IGET_G(src)+(31-a)*IGET_G(dst))/31;
                b=(a*IGET_B(src)+(31-a)*IGET_B(dst))/31;

		if (x+y*xres>=xres*yres || x+y*xres<0) {
			note("PANIC #2");
			continue;
		}

                ptr[m]=rgb2scr[IRGB(r,g,b)];
        }
	
	dd_unlock_surface(ddbs);

	ap_time+=GetTickCount()-start;
	ap_cnt+=acnt;
}

static int ic_load(int sprite)
{
        int iidx,start;
        int amax,nidx,pidx,v;
        IMAGE *image;

	start=GetTickCount();

	if (sprite>MAXSPRITE || sprite<0) {
		note("illegal sprite %d wanted in ic_load",sprite);
		return IIDX_NONE;
	}

	for (iidx=ima_hash[sprite%IMA_HASH_SIZE]; iidx!=-1; iidx=imagecache[iidx].hnext) {
		if (imagecache[iidx].sprite!=sprite) continue;

                ic_best(iidx);

                // remove from old pos
		nidx=imagecache[iidx].hnext;
		pidx=imagecache[iidx].hprev;

		if (pidx==-1) ima_hash[sprite%IMA_HASH_SIZE]=nidx;
		else imagecache[pidx].hnext=imagecache[iidx].hnext;

		if (nidx!=-1) imagecache[nidx].hprev=imagecache[iidx].hprev;

		// add to top pos
		nidx=ima_hash[sprite%IMA_HASH_SIZE];

		if (nidx!=-1) imagecache[nidx].hprev=iidx;
	
		imagecache[iidx].hprev=-1;
		imagecache[iidx].hnext=nidx;
	
		ima_hash[sprite%IMA_HASH_SIZE]=iidx;

                return iidx;
	}	

        // find free "iidx=rrand(max_imagecache);"
        iidx=iidx_last;

        // delete
        if (imagecache[iidx].sprite!=SPRITE_NONE) {
		nidx=imagecache[iidx].hnext;
		pidx=imagecache[iidx].hprev;

		if (pidx==-1) ima_hash[imagecache[iidx].sprite%IMA_HASH_SIZE]=nidx;
		else imagecache[pidx].hnext=imagecache[iidx].hnext;

		if (nidx!=-1) imagecache[nidx].hprev=imagecache[iidx].hprev;

                imagecache[iidx].sprite=SPRITE_NONE;
        }

        // build
        if (gfx_load_image(&imagecache[iidx].image,sprite)) { ic_last(iidx); fail("ic_load: load_image(%u,%u) failed",iidx,sprite); return IIDX_NONE; }

        // init
        imagecache[iidx].sprite=sprite;
        ic_best(iidx);

	nidx=ima_hash[sprite%IMA_HASH_SIZE];

	if (nidx!=-1) imagecache[nidx].hprev=iidx;

	imagecache[iidx].hprev=-1;
	imagecache[iidx].hnext=nidx;

	ima_hash[sprite%IMA_HASH_SIZE]=iidx;

	im_time+=GetTickCount()-start;

        return iidx;
}

struct prefetch
{
        int attick;
	int sprite;
	signed char sink;
	unsigned char scale,cr,cg,cb,light,sat;
        unsigned short c1,c2,c3,shine;
	char ml,ll,rl,ul,dl;
	unsigned char freeze;
	unsigned char grid;
};

#define MAXPRE (16384)
static struct prefetch pre[MAXPRE];
static int pre_in=0,pre_out=0;

static int sc_load(int sprite,int sink,int freeze,int grid,int scale,int cr,int cg,int cb,int light,int sat,int c1,int c2,int c3,int shine,int ml,int ll,int rl,int ul,int dl,int checkonly,int isprefetch);

void pre_add(int attick,int sprite,signed char sink,unsigned char freeze,unsigned char grid,unsigned char scale,char cr,char cg,char cb,char light,char sat,int c1,int c2,int c3,int shine,char ml,char ll,char rl,char ul,char dl)
{
	if ((pre_in+1)%MAXPRE==pre_out) return;	// buffer is full

	if (sprite>MAXSPRITE || sprite<0) {
		note("illegal sprite %d wanted in pre_add",sprite);
		return;
	}
        if (sc_load(sprite,sink,freeze,grid,scale,cr,cg,cb,light,sat,c1,c2,c3,shine,ml,ll,rl,ul,dl,1,1)) return;	// already in systemcache

        pre[pre_in].attick=attick;
	pre[pre_in].sprite=sprite;
	pre[pre_in].sink=sink;
	pre[pre_in].freeze=freeze;
	pre[pre_in].grid=grid;
	pre[pre_in].scale=scale;
	pre[pre_in].cr=cr;
	pre[pre_in].cg=cg;
	pre[pre_in].cb=cb;
	pre[pre_in].light=light;
	pre[pre_in].sat=sat;
	pre[pre_in].c1=c1;
	pre[pre_in].c2=c2;
	pre[pre_in].c3=c3;
	pre[pre_in].shine=shine;
	pre[pre_in].ml=ml;
	pre[pre_in].ll=ll;
	pre[pre_in].rl=rl;
	pre[pre_in].dl=dl;
	pre[pre_in].ul=ul;

	//note("add_pre: %d %d %d %d %d %d",pre[pre_in].sprite,pre[pre_in].ml,pre[pre_in].ll,pre[pre_in].rl,pre[pre_in].ul,pre[pre_in].dl);

	pre_in=(pre_in+1)%MAXPRE;
}

int pre_do(int curtick)
{
        while (pre[pre_out].attick<curtick && pre_in!=pre_out) pre_out=(pre_out+1)%MAXPRE;

	if (pre_in==pre_out) return 0;	// prefetch buffer is empty
	
	// load into systemcache
        sc_load(pre[pre_out].sprite,
		pre[pre_out].sink,
		pre[pre_out].freeze,
		pre[pre_out].grid,
		pre[pre_out].scale,
		pre[pre_out].cr,
		pre[pre_out].cg,
		pre[pre_out].cb,
		pre[pre_out].light,
		pre[pre_out].sat,
		pre[pre_out].c1,
		pre[pre_out].c2,
		pre[pre_out].c3,
		pre[pre_out].shine,
		pre[pre_out].ml,
		pre[pre_out].ll,
		pre[pre_out].rl,
		pre[pre_out].ul,
		pre[pre_out].dl,0,1);
	//note("pre_do: %d %d %d %d %d %d",pre[pre_out].sprite,pre[pre_out].ml,pre[pre_out].ll,pre[pre_out].rl,pre[pre_out].ul,pre[pre_out].dl);

	pre_out=(pre_out+1)%MAXPRE;

	if (pre_in>=pre_out) return pre_in-pre_out;
	else return MAXPRE+pre_in-pre_out;
}

static unsigned short shine_pix(unsigned short irgb,unsigned short shine)
{
	double r,g,b;

	if (irgb==rgbcolorkey) return irgb;

	r=IGET_R(irgb)/15.5;
	g=IGET_G(irgb)/15.5;
	b=IGET_B(irgb)/15.5;

	r=((r*r*r*r)*shine+r*(100-shine))/200;
	g=((g*g*g*g)*shine+g*(100-shine))/200;
	b=((b*b*b*b)*shine+b*(100-shine))/200;

	if (r>1) r=1;
	if (g>1) g=1;
	if (b>1) b=1;

	irgb=IRGB((int)(r*31),(int)(g*31),(int)(b*31));

	if (irgb==rgbcolorkey) return irgb-1;
	else return irgb;
}

#define REDCOL		(0.40)
#define GREENCOL	(0.70)
#define BLUECOL		(0.70)

static int colorize_pix(unsigned short irgb,unsigned short c1v,unsigned short c2v,unsigned short c3v)
{
	double rf,gf,bf,m,str,rm,gm,bm,rv,gv,bv;
	double c1=0,c2=0,c3=0;
	double shine=0;
        int r,g,b;

	if (irgb==rgbcolorkey) return irgb;

	rf=IGET_R(irgb)/32.0;
	gf=IGET_G(irgb)/32.0;
	bf=IGET_B(irgb)/32.0;

        m=max(max(rf,gf),bf)+0.000001;
        rm=rf/m; gm=gf/m; bm=bf/m;

	// channel 1: green max
        if (c1v && gm>0.99 && rm<GREENCOL && bm<GREENCOL) {
                c1=gf-max(bf,rf);
		if (c1v&0x8000) shine+=gm-max(rm,bm);
		
                gf-=c1;
	}
	
	m=max(max(rf,gf),bf)+0.000001;
        rm=rf/m; gm=gf/m; bm=bf/m;

	// channel 2: blue max
	if (c2v && bm>0.99 && rm<BLUECOL && gm<BLUECOL) {
		c2=bf-max(rf,gf);
		if (c2v&0x8000) shine+=bm-max(rm,gm);
		
		bf-=c2;
	}

	m=max(max(rf,gf),bf)+0.000001;
        rm=rf/m; gm=gf/m; bm=bf/m;

	// channel 3: red max
	if (c3v && rm>0.99 && gm<REDCOL && bm<REDCOL) {
		c3=rf-max(gf,bf);
		if (c3v&0x8000) shine+=rm-max(gm,bm);
	
		rf-=c3;
	}

        // sanity
	rf=max(0,rf);
	gf=max(0,gf);
	bf=max(0,bf);

        // collect
	r=min(31,
	      2*c1*IGET_R(c1v)+
	      2*c2*IGET_R(c2v)+
	      2*c3*IGET_R(c3v)+
	      rf*31);
	g=min(31,
	      2*c1*IGET_G(c1v)+
	      2*c2*IGET_G(c2v)+
	      2*c3*IGET_G(c3v)+
	      gf*31);
	b=min(31,
	      2*c1*IGET_B(c1v)+
	      2*c2*IGET_B(c2v)+
	      2*c3*IGET_B(c3v)+
	      bf*31);

        irgb=IRGB(r,g,b);

	if (shine>0.1) irgb=shine_pix(irgb,(int)(shine*50));	

	if (irgb==rgbcolorkey) return irgb-1;
	else return irgb;
}

static IMAGE *ic_merge(IMAGE *a,IMAGE *b)
{
	IMAGE *c;
	int x,y;
	int x1,x2,y1,y2;
	int a1,rgb1;
	int a2,rgb2;
	
        c=xmalloc(sizeof(IMAGE),MEM_IC);
	c->xres=max(a->xres,b->xres);
	c->yres=max(a->yres,b->yres);
	c->xoff=min(a->xoff,b->xoff);
	c->yoff=min(a->yoff,b->yoff);
	
	x1=c->xoff-a->xoff;
	x2=c->xoff-b->xoff;
	y1=c->yoff-a->yoff;
	y2=c->yoff-b->yoff;

	//note("cxres=%d, cyres=%d, axres=%d, ayres=%d, bxres=%d, byres=%d, x1=%d, x2=%d, y1=%d, y2=%d",c->xres,c->yres,a->xres,a->yres,b->xres,b->yres,x1,x2,y1,y2);

	c->a=xmalloc(c->xres*c->yres*sizeof(unsigned char),MEM_IC);
	c->rgb=xmalloc(c->xres*c->yres*sizeof(unsigned short),MEM_IC);

	for (y=0; y<c->yres; y++) {
		for (x=0; x<c->xres; x++) {
			if (x+x1<0 || x+x1>=a->xres || y+y1<0 || y+y1>=a->yres) {
				a1=rgb1=0;
			} else {
				a1=a->a[(x+x1)+(y+y1)*a->xres];
				rgb1=a->rgb[(x+x1)+(y+y1)*a->xres];
			}

			if (x+x2<0 || x+x2>=b->xres || y+y2<0 || y+y2>=b->yres) {
				a2=rgb2=0;
			} else {
				a2=b->a[(x+x2)+(y+y2)*b->xres];
				rgb2=b->rgb[(x+x2)+(y+y2)*b->xres];
			}

			c->a[x+y*c->xres]=max(a1,a2);

			if (IGET_R(rgb2)>1 ||
			    IGET_G(rgb2)>1 ||
			    IGET_B(rgb2)>1) c->rgb[x+y*c->xres]=rgb2;
			else c->rgb[x+y*c->xres]=rgb1;
		}
	}
	
	return c;
}

static unsigned short colorbalance(unsigned short irgb,char cr,char cg,char cb,char light,char sat)
{
	int r,g,b,grey;

	if (irgb==rgbcolorkey) return irgb;

	r=IGET_R(irgb)*8;
	g=IGET_G(irgb)*8;
	b=IGET_B(irgb)*8;

	// lightness
	if (light) {
		r+=light; g+=light; b+=light;
	}

	// saturation
	if (sat) {
		grey=(r+g+b)/3;
		r=((r*(20-sat))+(grey*sat))/20;
		g=((g*(20-sat))+(grey*sat))/20;
		b=((b*(20-sat))+(grey*sat))/20;
	}

	// color balancing
	cr*=0.75; cg*=0.75; cg*=0.75;

        r+=cr; g-=cr/2; b-=cr/2;
	r-=cg/2; g+=cg; b-=cg/2;
	r-=cb/2; g-=cb/2; b+=cb;

        if (r<0) r=0;
	if (g<0) g=0;
	if (b<0) b=0;

	if (r>255) { g+=(r-255)/2; b+=(r-255)/2; r=255; }
	if (g>255) { r+=(g-255)/2; b+=(g-255)/2; g=255; }
	if (b>255) { r+=(b-255)/2; g+=(b-255)/2; b=255; }

	if (r>255) r=255;
	if (g>255) g=255;
	if (b>255) b=255;

        irgb=IRGB(r/8,g/8,b/8);
	
	if (irgb==rgbcolorkey) return irgb-1;
	else return irgb;
}

static void sc_make_slow(SYSTEMCACHE *sc, IMAGE *image,signed char sink,unsigned char freeze,unsigned char grid,unsigned char scale,char cr,char cg,char cb,char light,char sat,unsigned short c1v,unsigned short c2v,unsigned short c3v,unsigned short shine,char ml,char ll,char rl,char ul,char dl)
{
	int x,y,amax,xm;
	double ix,iy,low_x,low_y,high_x,high_y,dbr,dbg,dbb,dba;
        unsigned short int irgb,*sptr;
        unsigned char a;
        int a_r,a_g,a_b,b_r,b_g,b_b,r,g,b,da,db;

	if (image->xres==0 || image->yres==0) scale=100;	// !!! needs better handling !!!

        if (scale!=100) {
		sc->xres=ceil((double)(image->xres-1)*scale/100.0);
                sc->yres=ceil((double)(image->yres-1)*scale/100.0);
		sc->size=sc->xres*sc->yres;
		
		sc->xoff=floor(image->xoff*scale/100.0+0.5);
                sc->yoff=floor(image->yoff*scale/100.0+0.5);
	} else {
		sc->xres=(int)image->xres; //*scale/100;
		sc->yres=(int)image->yres; //*scale/100;
		sc->size=sc->xres*sc->yres;
		sc->xoff=image->xoff;
		sc->yoff=image->yoff;
	}

        if (sink) sink=min(sink,max(0,sc->yres-4));

        sc->rgb=xrealloc(sc->rgb,sc->xres*sc->yres*sizeof(unsigned short int),MEM_SC);

        amax=sc->acnt;
        sc->acnt=0;

        for (y=0; y<sc->yres; y++) {
                for (x=0; x<sc->xres; x++) {

                        if (scale!=100) {
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
					if (c1v || c2v || c3v) irgb=colorize_pix(irgb,c1v,c2v,c3v);
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
					if (c1v || c2v || c3v) irgb=colorize_pix(irgb,c1v,c2v,c3v);
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
					if (c1v || c2v || c3v) irgb=colorize_pix(irgb,c1v,c2v,c3v);
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
					if (c1v || c2v || c3v) irgb=colorize_pix(irgb,c1v,c2v,c3v);
                                        dbr+=IGET_R(irgb)*high_x*high_y;
					dbg+=IGET_G(irgb)*high_x*high_y;
					dbb+=IGET_B(irgb)*high_x*high_y;
					
					if (!image->a) dba+=31*high_x*high_y;
					else dba+=image->a[ceil(ix)+ceil(iy)*image->xres]*high_x*high_y;
				}


				irgb=IRGB(((int)dbr),((int)dbg),((int)dbb));
				a=((int)dba);

				if (a>31) { note("oops: %d %d %d %d (%.2f %.2f %.2f %.2f) (%d,%d of %d,%d) (%.2f, %.2f of %d,%d)",
							a,(int)dbr,(int)dbg,(int)dbb,low_x,low_y,high_x,high_y,
							x,y,sc->xres,sc->yres,
							ix,iy,image->xres,image->yres); }
			} else {
				irgb=image->rgb[x+y*image->xres];
				if (c1v || c2v || c3v) irgb=colorize_pix(irgb,c1v,c2v,c3v);

				if (irgb==rgbcolorkey) a=0;
				else if (!image->a) a=31;
				else a=image->a[x+y*image->xres];
			}

			if ((cr || cg || cb || light || sat) && irgb!=rgbcolorkey) irgb=colorbalance(irgb,cr,cg,cb,light,sat);
                        if (shine) irgb=shine_pix(irgb,shine);

                        sptr=&sc->rgb[x+y*sc->xres];

			if (ll!=ml || rl!=ml || ul!=ml || dl!=ml) {
				int r1,r2,r3,r4,r5;
				int g1,g2,g3,g4,g5;
				int b1,b2,b3,b4,b5;
				int v1,v2,v3,v4,v5;
				int div;

                                if (y<10+(20-abs(20-x))/2) {
					if (x/2<20-y) {
						v2=-(x/2-(20-y))+1;
						r2=IGET_R(rgbfx_light[ll][irgb]);
						g2=IGET_G(rgbfx_light[ll][irgb]);
						b2=IGET_B(rgbfx_light[ll][irgb]);
					} else v2=0;
					if (x/2>20-y) {
						v3=(x/2-(20-y))+1;
						r3=IGET_R(rgbfx_light[rl][irgb]);
						g3=IGET_G(rgbfx_light[rl][irgb]);
						b3=IGET_B(rgbfx_light[rl][irgb]);
					} else v3=0;
					if (x/2>y) {
						v4=(x/2-y)+1;
						r4=IGET_R(rgbfx_light[ul][irgb]);
						g4=IGET_G(rgbfx_light[ul][irgb]);
						b4=IGET_B(rgbfx_light[ul][irgb]);
					} else v4=0;
					if (x/2<y) {
						v5=-(x/2-y)+1;
						r5=IGET_R(rgbfx_light[dl][irgb]);
						g5=IGET_G(rgbfx_light[dl][irgb]);
						b5=IGET_B(rgbfx_light[dl][irgb]);
					} else v5=0;
				} else {
					if (x<10) {
						v2=(10-x)*2-2;
						r2=IGET_R(rgbfx_light[ll][irgb]);
						g2=IGET_G(rgbfx_light[ll][irgb]);
						b2=IGET_B(rgbfx_light[ll][irgb]);
					} else v2=0;
					if (x>10 && x<20) {
						v3=(x-10)*2-2;
						r3=IGET_R(rgbfx_light[rl][irgb]);
						g3=IGET_G(rgbfx_light[rl][irgb]);
						b3=IGET_B(rgbfx_light[rl][irgb]);
					} else v3=0;
					if (x>20 && x<30) {
						v5=(10-(x-20))*2-2;
						r5=IGET_R(rgbfx_light[dl][irgb]);
						g5=IGET_G(rgbfx_light[dl][irgb]);
						b5=IGET_B(rgbfx_light[dl][irgb]);
					} else v5=0;
					if (x>30 && x<40) {
						v4=(x-30)*2-2;
						r4=IGET_R(rgbfx_light[ul][irgb]);
						g4=IGET_G(rgbfx_light[ul][irgb]);
						b4=IGET_B(rgbfx_light[ul][irgb]);
					} else v4=0;					
				}

				//addline("v1=%d, v2=%d, v3=%d, v4=%d, v5=%d",v1,v2,v3,v4,v5);
				
				v1=20-(v2+v3+v4+v5)/2;
                                r1=IGET_R(rgbfx_light[ml][irgb]);
				g1=IGET_G(rgbfx_light[ml][irgb]);
				b1=IGET_B(rgbfx_light[ml][irgb]);
				
				div=v1+v2+v3+v4+v5;

                                r=(r1*v1+r2*v2+r3*v3+r4*v4+r5*v5)/div;
				g=(g1*v1+g2*v2+g3*v3+g4*v4+g5*v5)/div;
				b=(b1*v1+b2*v2+b3*v3+b4*v4+b5*v5)/div;

				irgb=IRGB(r,g,b);
				
			} else  irgb=rgbfx_light[ml][irgb];

			if (sink) {
                                if (sc->yres-sink<y) a=0;
			}

                        if (freeze) irgb=rgbfx_freeze[freeze][irgb];
			if (grid==DDFX_LEFTGRID) { if ((sc->xoff+x+sc->yoff+y)&1) a=0; }
                        if (grid==DDFX_RIGHTGRID) {  if ((sc->xoff+x+sc->yoff+y+1)&1) a=0; }

                        if (a==31) {
                                *sptr=rgb2scr[irgb];
                        } else if (a==0) {
                                *sptr=scrcolorkey;	//rgbcolorkey;				
                        } else {
                                if (sc->acnt==amax) {
                                        amax+=64;
                                        sc->apix=xrealloc(sc->apix,amax*sizeof(APIX),MEM_SC);
                                }

                                sc->apix[sc->acnt].x=x;
                                sc->apix[sc->acnt].y=y;
                                sc->apix[sc->acnt].a=a;
                                sc->apix[sc->acnt].rgb=irgb;

                                sc->acnt++;

                                *sptr=scrcolorkey;	//rgbcolorkey;				
                        }
                }
        }

        sc->apix=xrealloc(sc->apix,sc->acnt*sizeof(APIX),MEM_SC);	
}

#pragma argsused
static void sc_make_fast(SYSTEMCACHE *sc, IMAGE *image,signed char sink,unsigned char freeze,unsigned char grid,unsigned char scale,char cr,char cg,char cb,char light,char sat,unsigned short c1v,unsigned short c2v,unsigned short c3v,unsigned short shine,char ml,char ll,char rl,char ul,char dl)
{
	int x,y,amax,xm,pos;
        unsigned short int irgb;
        unsigned char a;
	int need_colorize=0;
	int need_colorbalance=0;

        sc->xres=(int)image->xres;
	sc->yres=(int)image->yres;
	sc->size=sc->xres*sc->yres;
	sc->xoff=image->xoff;
	sc->yoff=image->yoff;

        sc->rgb=xrealloc(sc->rgb,sc->xres*sc->yres*sizeof(unsigned short int),MEM_SC);

        amax=sc->acnt;
        sc->acnt=0;

        if (c1v || c2v || c3v) need_colorize=1;
	if (cr || cg || cb || light || sat) need_colorbalance=1;

        for (y=pos=0; y<sc->yres; y++) {
                for (x=0; x<sc->xres; x++,pos++) {

			irgb=image->rgb[pos];

			if (irgb==rgbcolorkey) {
                                sc->rgb[pos]=scrcolorkey;
                                continue;
			} else {
				if (!image->a) a=31;
				else a=image->a[pos];	

				if (need_colorize) irgb=colorize_pix(irgb,c1v,c2v,c3v);
                                if (need_colorbalance) irgb=colorbalance(irgb,cr,cg,cb,light,sat);
				if (shine) irgb=shine_pix(irgb,shine);

				irgb=rgbfx_light[ml][irgb];
				if (freeze) irgb=rgbfx_freeze[freeze][irgb];
			}

                        if (a==31) {
                                sc->rgb[pos]=rgb2scr[irgb];
                        } else if (a==0) {
                                sc->rgb[pos]=scrcolorkey;	//rgbcolorkey;				
                        } else {
                                if (sc->acnt==amax) {
                                        amax+=64;
                                        sc->apix=xrealloc(sc->apix,amax*sizeof(APIX),MEM_SC);
                                }

                                sc->apix[sc->acnt].x=x;
                                sc->apix[sc->acnt].y=y;
                                sc->apix[sc->acnt].a=a;
                                sc->apix[sc->acnt].rgb=irgb;

                                sc->acnt++;

                                sc->rgb[pos]=scrcolorkey;	//rgbcolorkey;				
                        }			
                }
        }

        sc->apix=xrealloc(sc->apix,sc->acnt*sizeof(APIX),MEM_SC);	
}

static void sc_make(SYSTEMCACHE *sc, IMAGE *image,signed char sink,unsigned char freeze,unsigned char grid,unsigned char scale,char cr,char cg,char cb,char light,char sat,unsigned short c1v,unsigned short c2v,unsigned short c3v,unsigned short shine,char ml,char ll,char rl,char ul,char dl)
{
	int start;
	
	start=GetTickCount();
	
	if (scale!=100 || ll!=ml || rl!=ml || ul!=ml || dl!=ml || grid || sink) {
		sc_make_slow(sc,image,sink,freeze,grid,scale,cr,cg,cb,light,sat,c1v,c2v,c3v,shine,ml,ll,rl,ul,dl);
		//printf("slow: %d %d %d %d\n",scale!=100,ll!=ml || rl!=ml || ul!=ml || dl!=ml,grid,sink);
	} else sc_make_fast(sc,image,sink,freeze,grid,scale,cr,cg,cb,light,sat,c1v,c2v,c3v,shine,ml,ll,rl,ul,dl);

	sc_time+=GetTickCount()-start;
	sm_cnt++;
}

int sys_hash_key2(int sprite,int ml,int cr,int cg,int cb,int light)
{
	return ((sprite)+(ml<<4)+(cr<<8)+(cg<<12)+(cb<<16)+(light<<0))&(SYS_HASH_SIZE-1);
}

int sys_hash_key(int sidx)
{
	return sys_hash_key2(systemcache[sidx].sprite,
		systemcache[sidx].ml,
		systemcache[sidx].cr,
		systemcache[sidx].cg,
		systemcache[sidx].cb,
		systemcache[sidx].light);
}


static int sc_load(int sprite,int sink,int freeze,int grid,int scale,int cr,int cg,int cb,int light,int sat,int c1,int c2,int c3,int shine,int ml,int ll,int rl,int ul,int dl,int checkonly,int isprefetch)
{
        int sidx,iidx,v,vmax,step,pidx,nidx;

	if (sprite>MAXSPRITE || sprite<0) {
		note("illegal sprite %d wanted in sc_load",sprite);
		return SIDX_NONE;
	}

        for (sidx=sys_hash[sys_hash_key2(sprite,ml,cr,cg,cb,light)],step=0; sidx!=-1; sidx=systemcache[sidx].hnext,step++) {
		if (systemcache[sidx].sprite!=sprite) continue;
		if (systemcache[sidx].sink!=sink) continue;
		if (systemcache[sidx].freeze!=freeze) continue;
		if (systemcache[sidx].grid!=grid) continue;
		if (systemcache[sidx].scale!=scale) continue;
		if (systemcache[sidx].cr!=cr) continue;
		if (systemcache[sidx].cg!=cg) continue;
		if (systemcache[sidx].cb!=cb) continue;
		if (systemcache[sidx].light!=light) continue;
		if (systemcache[sidx].sat!=sat) continue;
                if (systemcache[sidx].c1!=c1) continue;
		if (systemcache[sidx].c2!=c2) continue;
		if (systemcache[sidx].c3!=c3) continue;
		if (systemcache[sidx].shine!=shine) continue;
                if (systemcache[sidx].ml!=ml) continue;
                if (systemcache[sidx].ll!=ll) continue;
		if (systemcache[sidx].rl!=rl) continue;
		if (systemcache[sidx].ul!=ul) continue;
		if (systemcache[sidx].dl!=dl) continue;

		if (checkonly) return 1;

                sc_hit++;
                sc_best(sidx);

		sc_maxstep+=step;

		// remove from old pos
		nidx=systemcache[sidx].hnext;
		pidx=systemcache[sidx].hprev;

		if (pidx==-1) sys_hash[sys_hash_key(sidx)]=nidx;
		else systemcache[pidx].hnext=systemcache[sidx].hnext;

		if (nidx!=-1) systemcache[nidx].hprev=systemcache[sidx].hprev;

		// add to top pos
		nidx=sys_hash[sys_hash_key(sidx)];

		if (nidx!=-1) systemcache[nidx].hprev=sidx;
	
		systemcache[sidx].hprev=-1;
		systemcache[sidx].hnext=nidx;
	
		sys_hash[sys_hash_key(sidx)]=sidx;

		return sidx;
	}

	if (checkonly) return 0;

	if (!isprefetch) {
		sc_miss++;
	}

        // find free "sidx=rrand(max_systemcache);"
        sidx=sidx_last;

        // delete
        if (systemcache[sidx].sprite!=SPRITE_NONE) {
		
		//printf("PAAAAANIC!\n");

		nidx=systemcache[sidx].hnext;
		pidx=systemcache[sidx].hprev;

		if (pidx==-1) sys_hash[sys_hash_key(sidx)]=nidx;
		else systemcache[pidx].hnext=systemcache[sidx].hnext;

		if (nidx!=-1) systemcache[nidx].hprev=systemcache[sidx].hprev;

                vmax=systemcache[sidx].tdx*systemcache[sidx].tdy;
		for (v=0; v<vmax; v++) {
                        if (systemcache[sidx].vc[v]==((void*)(-1))) systemcache[sidx].vc[v]=NULL;
			if (systemcache[sidx].vc[v]==NULL) continue;
			
			vid_free_cache(systemcache[sidx].vc[v]);
			systemcache[sidx].vc[v]=NULL;
                }
		//vid_check_for_erasure(sidx); - enable for debugging

                systemcache[sidx].sprite=SPRITE_NONE;
        }

        // build
        iidx=ic_load(sprite);
        if (iidx==IIDX_NONE) { sc_last(sidx); return SIDX_NONE; }

        systemcache[sidx].sprite=sprite;	// dup
	sc_make(&systemcache[sidx],&imagecache[iidx].image,sink,freeze,grid,scale,cr,cg,cb,light,sat,c1,c2,c3,shine,ml,ll,rl,ul,dl);
	//if (systemcache[sidx].xres>TILESIZEDX || systemcache[sidx].yres>TILESIZEDY) addline("res=%d,%d",systemcache[sidx].xres,systemcache[sidx].yres);

        // init
        systemcache[sidx].sprite=sprite;
	systemcache[sidx].sink=sink;
	systemcache[sidx].freeze=freeze;
	systemcache[sidx].grid=grid;
	systemcache[sidx].scale=scale;
	systemcache[sidx].cr=cr;
	systemcache[sidx].cg=cg;
	systemcache[sidx].cb=cb;
	systemcache[sidx].light=light;
	systemcache[sidx].sat=sat;
	systemcache[sidx].c1=c1;
	systemcache[sidx].c2=c2;
	systemcache[sidx].c3=c3;
	systemcache[sidx].shine=shine;
        systemcache[sidx].ml=ml;
	systemcache[sidx].ll=ll;
	systemcache[sidx].rl=rl;
	systemcache[sidx].ul=ul;
	systemcache[sidx].dl=dl;

	nidx=sys_hash[sys_hash_key(sidx)];

	if (nidx!=-1) systemcache[nidx].hprev=sidx;
	
	systemcache[sidx].hprev=-1;
	systemcache[sidx].hnext=nidx;
	
	sys_hash[sys_hash_key(sidx)]=sidx;

        systemcache[sidx].tdx=(systemcache[sidx].xres+TILESIZEDX-1)/TILESIZEDX;
        systemcache[sidx].tdy=(systemcache[sidx].yres+TILESIZEDY-1)/TILESIZEDY;

        vmax=systemcache[sidx].tdx*systemcache[sidx].tdy;

	systemcache[sidx].vc=xrealloc(systemcache[sidx].vc,vmax*sizeof(struct vid_cache **),MEM_VPC);	
	for (v=0; v<vmax; v++) systemcache[sidx].vc[v]=NULL;
	
	systemcache[sidx].lost=1;

        sc_best(sidx);

        return sidx;
}

static int sc_blit2(DDFX *ddfx, int sidx, int scrx, int scry);
int dd_copysprite_fx(DDFX *ddfx, int scrx, int scry)
{
        int sidx;

        PARANOIA( if (!ddfx) paranoia("dd_copysprite_fx: ddfx=NULL"); )
        PARANOIA( if (ddfx->light<0 || ddfx->light>16) paranoia("dd_copysprite_fx: ddfx->light=%d",ddfx->light); )
        PARANOIA( if (ddfx->freeze<0 || ddfx->freeze>=DDFX_MAX_FREEZE) paranoia("dd_copysprite_fx: ddfx->freeze=%d",ddfx->freeze); )

        sidx=sc_load(ddfx->sprite,
		ddfx->sink,
		ddfx->freeze,
		ddfx->grid,
		ddfx->scale,
		ddfx->cr,
		ddfx->cg,
		ddfx->cb,
		ddfx->clight,
		ddfx->sat,
		ddfx->c1,
		ddfx->c2,
		ddfx->c3,
		ddfx->shine,
		ddfx->ml,
		ddfx->ll,
		ddfx->rl,
		ddfx->ul,
		ddfx->dl,0,0);	

	if (sidx==SIDX_NONE) return 0;

        // note("sprite=%d xoff=%d yoff=%d",imagecache[iidx].sprite,imagecache[iidx].image.xoff,imagecache[iidx].image.yoff);

        // shift position according to align
        if (ddfx->align==DD_OFFSET) {
                scrx+=systemcache[sidx].xoff;
                scry+=systemcache[sidx].yoff;
        }
        else if (ddfx->align==DD_CENTER) {
                scrx-=systemcache[sidx].xres/2;
                scry-=systemcache[sidx].yres/2;
        }	

        // add the additional cliprect
        if (ddfx->clipsx!=ddfx->clipex || ddfx->clipsy!=ddfx->clipey) {
                dd_push_clip();
                if (ddfx->clipsx!=ddfx->clipex) dd_more_clip(scrx-systemcache[sidx].xoff+ddfx->clipsx,clipsy,scrx-systemcache[sidx].xoff+ddfx->clipex,clipey);
                if (ddfx->clipsy!=ddfx->clipey) dd_more_clip(clipsx,scry-systemcache[sidx].yoff+ddfx->clipsy,clipex,scry-systemcache[sidx].yoff+ddfx->clipey);
        }

        // blit it
	sc_blit2(ddfx,sidx,scrx,scry);

        // remove additional cliprect
        if (ddfx->clipsx!=ddfx->clipex || ddfx->clipsy!=ddfx->clipey) dd_pop_clip();	

        return 1;
}

void dd_copysprite_callfx(int sprite, int scrx, int scry, int light, int ml, int grid, int align)
{
        DDFX ddfx;

        bzero(&ddfx,sizeof(DDFX));

        ddfx.sprite=sprite;
        if (light<1000) ddfx.light=DDFX_NLIGHT;
        ddfx.grid=grid;
        ddfx.align=align;

	ddfx.ml=ddfx.ll=ddfx.rl=ddfx.ul=ddfx.dl=ml;
	ddfx.sink=0;
	ddfx.scale=100;
	ddfx.cr=ddfx.cg=ddfx.cb=ddfx.clight=ddfx.sat=0;
	ddfx.c1=ddfx.c2=ddfx.c3=ddfx.shine=0;

        dd_copysprite_fx(&ddfx,scrx,scry);
}

void dd_copysprite(int sprite, int scrx, int scry, int light, int align)
{
        DDFX ddfx;

        bzero(&ddfx,sizeof(DDFX));

        ddfx.sprite=sprite;
        ddfx.light=DDFX_NLIGHT;
        ddfx.align=align;

	ddfx.ml=ddfx.ll=ddfx.rl=ddfx.ul=ddfx.dl=light;
	ddfx.sink=0;
	ddfx.scale=100;
	ddfx.cr=ddfx.cg=ddfx.cb=ddfx.clight=ddfx.sat=0;
	ddfx.c1=ddfx.c2=ddfx.c3=ddfx.shine=0;

        dd_copysprite_fx(&ddfx,scrx,scry);
}

void dd_copysprite_callfx_old(int sprite, int scrx, int scry, int fx, int align)
{
        DDFX ddfx;
        int light;

        bzero(&ddfx,sizeof(DDFX));

        ddfx.sprite=sprite;

        light=fx%16;
        if (light==FX_BRIGHT) ddfx.light=0;
        else ddfx.light=15-light;

        ddfx.ml=ddfx.ll=ddfx.rl=ddfx.ul=ddfx.dl=ddfx.light;
	ddfx.sink=0;
	ddfx.scale=100;
	ddfx.cr=ddfx.cg=ddfx.cb=ddfx.clight=ddfx.sat=0;
	ddfx.c1=ddfx.c2=ddfx.c3=ddfx.shine=0;

        switch(fx/STARTSLICE) {
                case 3: ddfx.grid=DDFX_LEFTGRID; break;
                case 2: ddfx.grid=DDFX_RIGHTGRID; break;
                case 1: addline("aha"); break;
                default: ddfx.grid=0; break;
        }
        // ddfx.grid=0;
        ddfx.align=align;
        dd_copysprite_fx(&ddfx,scrx,scry);
}

void dd_rect(int sx, int sy, int ex, int ey, unsigned short int color)
{
        int x,y,err;
	unsigned short *ptr;
	RECT rc;
	DDBLTFX bltfx;

        if (sx<clipsx) sx=clipsx;
        if (sy<clipsy) sy=clipsy;
        if (ex>clipex) ex=clipex;
        if (ey>clipey) ey=clipey;

	if (sx>ex || sy>ey) return;

	if ((ex-sx)*(ey-sy)>100) {	// large rect? use hardware then
		bzero(&bltfx,sizeof(bltfx));
		bltfx.dwSize=sizeof(bltfx);
		bltfx.u5.dwFillColor=color;
		
		rc.left=sx+x_offset;
		rc.top=sy+y_offset;
		rc.right=ex+x_offset;
		rc.bottom=ey+y_offset;
	
		if ((err=ddbs->lpVtbl->Blt(ddbs,&rc,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&bltfx))!=DD_OK) {
			char buf[80];
			sprintf(buf,"dd_rect(): %d,%d -> %d,%d (%d,%d)",sx,sy,ex,ey,xres,yres);
			dd_error(buf,err);
		}
	} else {
		if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return;
	
		ptr+=(sx+x_offset)+(sy+y_offset)*xres;
	
		for (y=sy; y<ey; y++,ptr+=xres-(ex-sx)) {
			for (x=sx; x<ex; x++,ptr++) {
				if (ptr-vidptr>=xres*yres || ptr<vidptr) {
					note("PANIC #3");
					ptr=vidptr;
				}
				*ptr=color;
			}
		}
	
		dd_unlock_surface(ddbs);
	}
}

void dd_black(void)
{
	int err;
        RECT rc;
	DDBLTFX bltfx;
	char buf[256];


	bzero(&bltfx,sizeof(bltfx));
	bltfx.dwSize=sizeof(bltfx);
	bltfx.u5.dwFillColor=0;
	
	rc.left=0;
	rc.top=0;
	rc.right=XRES;	//xres;
	rc.bottom=YRES;	//yres;

	if ((err=ddbs->lpVtbl->Blt(ddbs,&rc,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&bltfx))!=DD_OK) {
		//sprintf(buf,"dd_black(ddbs,%d,%d):",xres,yres);
                dd_error(buf,err);
	}
	if ((err=ddbs->lpVtbl->Blt(ddps,&rc,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&bltfx))!=DD_OK) {
		//sprintf(buf,"dd_black(ddps,%d,%d):",xres,yres);
                dd_error(buf,err);
	}
}

void dd_shaded_rect(int sx, int sy, int ex, int ey)
{
        int x,y,r,g,b;
	unsigned short *ptr,col;

        if (sx<clipsx) sx=clipsx;
        if (sy<clipsy) sy=clipsy;
        if (ex>clipex) ex=clipex;
        if (ey>clipey) ey=clipey;

	if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return;

        ptr+=(sx+x_offset)+(sy+y_offset)*xres;

        for (y=sy; y<ey; y++,ptr+=xres-(ex-sx)) {
                for (x=sx; x<ex; x++,ptr++) {
			if (ptr-vidptr>=xres*yres || ptr<vidptr) {
				note("PANIC #3");
				ptr=vidptr;
			}
			col=*ptr;
			col=scr2rgb[col];
			
			r=IGET_R(col);
			g=IGET_G(col);
			b=IGET_B(col);
			r=min(31,r+16);
			g=min(31,g+16);

			col=IRGB(r,g,b);
			col=rgb2scr[col];

                        *ptr=col;
                }
        }

        dd_unlock_surface(ddbs);
}

void dd_line(int fx,int fy,int tx,int ty,unsigned short col)
{
	unsigned short *ptr,val;
	int dx,dy,x,y,rx,ry;

	if (fx<clipsx) fx=clipsx;
	if (fy<clipsy) fy=clipsy;
	if (fx>=clipex) fx=clipex-1;
	if (fy>=clipey) fy=clipey-1;

	if (tx<clipsx) tx=clipsx;
	if (ty<clipsy) ty=clipsy;
	if (tx>=clipex) tx=clipex-1;
	if (ty>=clipey) ty=clipey-1;

        fx+=x_offset; tx+=x_offset;
	fy+=y_offset; ty+=y_offset;

	dx=(tx-fx);
        dy=(ty-fy);

        if (dx==0 && dy==0) return;

	if (abs(dx)>abs(dy)) { dy=dy*1024/abs(dx); dx=dx*1024/abs(dx); }
        else { dx=dx*1024/abs(dy); dy=dy*1024/abs(dy); }

	x=fx*1024+512;
	y=fy*1024+512;

	if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return;

	rx=x/1024; ry=y/1024;
        while (1) {
		if (rx+ry*xres>=xres*yres || rx+ry*xres<0) {
			note("PANIC #4B");
			break;
		}
                ptr[rx+ry*xres]=col;
		
		x+=dx;
                y+=dy;
		rx=x/1024; ry=y/1024;
		if (rx==tx && ry==ty) break;
        }
	
        dd_unlock_surface(ddbs);
}

void dd_line2(int fx,int fy,int tx,int ty,unsigned short col,unsigned short *ptr)
{
	unsigned short val;
	int dx,dy,x,y,rx,ry;

	if (fx<clipsx) fx=clipsx;
	if (fy<clipsy) fy=clipsy;
	if (fx>=clipex) fx=clipex-1;
	if (fy>=clipey) fy=clipey-1;

	if (tx<clipsx) tx=clipsx;
	if (ty<clipsy) ty=clipsy;
	if (tx>=clipex) tx=clipex-1;
	if (ty>=clipey) ty=clipey-1;

	fx+=x_offset; tx+=x_offset;
	fy+=y_offset; ty+=y_offset;

	dx=(tx-fx);
        dy=(ty-fy);

        if (dx==0 && dy==0) return;

	if (abs(dx)>abs(dy)) { dy=dy*1024/abs(dx); dx=dx*1024/abs(dx); }
        else { dx=dx*1024/abs(dy); dy=dy*1024/abs(dy); }

	x=fx*1024+512;
	y=fy*1024+512;

	rx=x/1024; ry=y/1024;
        while (1) {
		if (rx+ry*xres>=xres*yres || rx+ry*xres<0) {
			note("PANIC #4B");
			break;
		}
                ptr[rx+ry*xres]=col;

                x+=dx;
                y+=dy;
		rx=x/1024; ry=y/1024;
		if (rx==tx && ry==ty) break;
        }
}

void dd_display_strike(int fx,int fy,int tx,int ty)
{
        int mx,my;
	int dx,dy,d,l;
	unsigned short col;

        dx=abs(tx-fx);
	dy=abs(ty-fy);

        mx=(fx+tx)/2+15-rrand(30);
	my=(fy+ty)/2+15-rrand(30);

	if (dx>=dy) {
		for (d=-4; d<5; d++) {
			l=(4-abs(d))*4;
			col=rgb2scr[IRGB(l,l,31)];
                        dd_line(fx,fy,mx,my+d,col);
                        dd_line(mx,my+d,tx,ty,col);
		}
	} else {
		for (d=-4; d<5; d++) {
			l=(4-abs(d))*4;
			col=rgb2scr[IRGB(l,l,31)];
                        dd_line(fx,fy,mx+d,my,col);
                        dd_line(mx+d,my,tx,ty,col);
		}
	}
}



void dd_draw_curve(int cx,int cy,int nr,int size,unsigned short col)
{
	unsigned short *ptr;
	int n,x,y;

	col=rgb2scr[col];

        if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return;

	for (n=nr*90; n<nr*90+90; n+=4) {
		x=sin(n/360.0*M_PI*2)*size+cx;
		y=cos(n/360.0*M_PI*2)*size*2/3+cy;

		if (x<clipsx) continue;
		if (y<clipsy) continue;
		if (x>=clipex) continue;
		if (y+10>=clipey) continue;

		x+=x_offset; y+=y_offset;

		ptr[x+y*xres]=col;
		
		ptr[x+y*xres]=col;
		ptr[x+y*xres+xres*5]=col;
		//ptr[x+y*xres-xres*5]=col;
		ptr[x+y*xres+xres*10]=col;
		//ptr[x+y*xres-xres*10]=col;
	}

	dd_unlock_surface(ddbs);
}

void dd_display_pulseback(int fx,int fy,int tx,int ty)
{
        int mx,my;
	int dx,dy,d,l;
	unsigned short col;

        dx=abs(tx-fx);
	dy=abs(ty-fy);

        mx=(fx+tx)/2+15-rrand(30);
	my=(fy+ty)/2+15-rrand(30);

	if (dx>=dy) {
		for (d=-4; d<5; d++) {
			l=(4-abs(d))*4;
			col=rgb2scr[IRGB(l,31,l)];
                        dd_line(fx,fy,mx,my+d,col);
                        dd_line(mx,my+d,tx,ty,col);
		}
	} else {
		for (d=-4; d<5; d++) {
			l=(4-abs(d))*4;
			col=rgb2scr[IRGB(l,31,l)];
                        dd_line(fx,fy,mx+d,my,col);
                        dd_line(mx+d,my,tx,ty,col);
		}
	}
}

// text

int dd_textlength(int flags, const char *text)
{
        DDFONT *font;
        int x;
        const char *c;

        if (flags&DD_SMALL) font=fontb;
	else if (flags&DD_BIG) font=fontc;
	else font=fonta;

        for (x=0,c=text; *c && *c!=DDT; c++) x+=font[*c].dim;

        return x;
}

int dd_textlen(int flags, const char *text, int n)
{
        DDFONT *font;
        int x;
        const char *c;

        if (n<0) return dd_textlength(flags,text);

        if (flags&DD_SMALL) font=fontb;
	else if (flags&DD_BIG) font=fontc;
	else font=fonta;

        for (x=0,c=text; *c && *c!=DDT && n; c++,n--) x+=font[*c].dim;

        return x;
}

int dd_drawtext(int sx, int sy, unsigned short int color, int flags, const char *text)
{
        unsigned short *ptr,*dst;
        unsigned char *rawrun;
        int x,y,start;
        const char *c;
        DDFONT *font;

	start=GetTickCount();

	if (flags&DD__SHADEFONT) {
		if (flags&DD_SMALL) font=fontb_shaded;
		else if (flags&DD_BIG) font=fontc_shaded;
		else font=fonta_shaded;		
	} else if (flags&DD__FRAMEFONT) {
		if (flags&DD_SMALL) font=fontb_framed;
		else if (flags&DD_BIG) font=fontc_framed;
		else font=fonta_framed;
	} else {
		if (flags&DD_SMALL) font=fontb;
		else if (flags&DD_BIG) font=fontc;
		else font=fonta;
	}

	if (!font) return 42;

        if (flags&DD_CENTER) {
                for (x=0,c=text; *c; c++) x+=font[*c].dim;
                sx-=x/2;
        } else if (flags&DD_RIGHT) {
                for (x=0,c=text; *c; c++) x+=font[*c].dim;
                sx-=x;
        }

        if (flags&DD_SHADE) {
                dd_drawtext(sx-1,sy-1,rgb2scr[IRGB(0,0,0)],DT_LEFT|(flags&(DD_SMALL|DD_BIG))|DD__SHADEFONT,text);
        } else if (flags&DD_FRAME) {
                if (flags&64) dd_drawtext(sx-1,sy-1,rgb2scr[IRGB(24,0,0)],DT_LEFT|(flags&(DD_SMALL|DD_BIG))|DD__FRAMEFONT,text);
		else dd_drawtext(sx-1,sy-1,rgb2scr[IRGB(0,0,0)],DT_LEFT|(flags&(DD_SMALL|DD_BIG))|DD__FRAMEFONT,text);
        }

        if (sy>=clipey) return sx;

	if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return sx;

        while (*text && *text!=DDT && sx+font[*text].dim<clipsx) sx+=font[*text++].dim;

        while (*text && *text!=DDT) {

                if (*text<0) { note("PANIC: char over limit"); text++; continue; }

                rawrun=font[*text].raw;

                x=sx;
                y=sy;

                dst=ptr+(x+x_offset)+(y+y_offset)*xres;

                while (*rawrun!=255) {

                        if (*rawrun==254) {
                                y++;
                                x=sx;
                                rawrun++;
                                if (y>=clipey) break;
                                dst=ptr+(x+x_offset)+(y+y_offset)*xres;
                                continue;
                        }

                        dst+=*rawrun;
                        x+=*rawrun;

                        if (x>=clipex) {
                                while (*rawrun!=255 && *rawrun!=254) rawrun++;
                                continue;
                        }

                        rawrun++;
                        if (x>=clipsx && y>=clipsy) {
				if (dst-vidptr>xres*yres || dst<vidptr) {
					note("PANIC #5");
					dst=vidptr;
				}
				*dst=color;
				tp_cnt++;
			}
                }

                if (x>=clipex) break;

                sx+=font[*text++].dim;
        }

        dd_unlock_surface(ddbs);

	tp_time+=GetTickCount()-start;

        return sx;
}

int dd_drawtext_break(int x,int y,int breakx,unsigned short color,int flags,const char *ptr)
{
	char buf[256];
	int xp,n,size;
	
	xp=x;

	while (*ptr) {
		while (*ptr==' ') ptr++;
		
		for (n=0; n<256 && *ptr && *ptr!=' '; buf[n++]=*ptr++) ;
		buf[n]=0;
	
		size=dd_textlength(flags,buf);
		if (xp+size>breakx) {
			xp=x; y+=10;
		}
		dd_drawtext(xp,y,color,flags,buf);
		xp+=size+4;
	}
	return y+10;
}


static void dd_pixel_fast(int x,int y,unsigned short col,unsigned short *ptr)
{
	if (x<0 || y<0 || x>=800 || y>=600) return;	// !!!! xres yres ???
	if ((x+x_offset)>=xres || (y+y_offset)>=yres) {
		note("PANIC 5b - %d,%d %d,%d %X",x,y,x_offset,y_offset,col);
		return;
	}
	
        ptr[(x+x_offset)+(y+y_offset)*xres]=col;
	np_cnt++;
}

void dd_pixel(int x,int y,unsigned short col)
{
	unsigned short *ptr;

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;

        dd_pixel_fast(x,y,col,ptr);
	
        dd_unlock_surface(ddbs);
}

int dd_drawtext_fmt(int sx, int sy, unsigned short int color, int flags, const char *format, ...)
{
        char buf[1024];
        va_list va;

        va_start(va,format);
        vsprintf(buf,format,va);
        va_end(va);

        return dd_drawtext(sx,sy,color,flags,buf);
}


static int bless_init=0;
static int bless_sin[36];
static int bless_cos[36];
static int bless_hight[200];

void dd_draw_bless_pix(int x,int y,int nr,int color,int front,unsigned short *ptr)
{
	int sy;
	
        //sy=sin((nr%36)/36.0*M_PI*2)*8;
	sy=bless_sin[nr%36];
	if (front && sy<0) return;
	if (!front && sy>=0) return;

	//x+=cos((nr%36)/36.0*M_PI*2)*16;
	x+=bless_cos[nr%36];
	//y=y+sy-20+sin((nr%200)/200.0*M_PI*2)*20;
	y=y+sy+bless_hight[nr%200];

	if (x<clipsx || x>=clipex || y<clipsy || y>=clipey) return;

	dd_pixel_fast(x,y,color,ptr);
}

void dd_draw_heal_pix(int x,int y,int nr,int color,int front,unsigned short *ptr)
{
	int sy,val;
	double off;
	
	val=(nr/36)%5;
	off=0.6+val*0.10;
	//note("val=%d",val);
        sy=bless_sin[nr%36]*off;
	if (front && sy<0) return;
	if (!front && sy>=0) return;

	x+=bless_cos[nr%36]*off;
	//y+=sy-40;
	
	y=y+sy+bless_hight[nr%200]/10-45;

	if (x<clipsx || x>=clipex || y<clipsy || y>=clipey) return;

	dd_pixel_fast(x,y,color,ptr);
}


void dd_draw_rain_pix(int x,int y,int nr,int color,int front,unsigned short *ptr)
{
	int sy;
	
	x+=((nr/30)%30)+15;
	sy=((nr/330)%20)+10;
	if (front && sy<0) return;
	if (!front && sy>=0) return;
	
	y=y+sy-((nr*2)%60)-60;

	if (x<clipsx || x>=clipex || y<clipsy || y>=clipey) return;
	
	dd_pixel_fast(x,y,color,ptr);
}

void dd_draw_bless(int x,int y,int ticker,int strength,int front)
{
	int step,nr;
	double light;
	unsigned short *ptr;
	int start;
	//static int bless_time=0,bless_cnt=0;

        start=GetTickCount();

        if (!bless_init) {
		for (nr=0; nr<36; nr++) {
			bless_sin[nr]=sin((nr%36)/36.0*M_PI*2)*8;
			bless_cos[nr]=cos((nr%36)/36.0*M_PI*2)*16;
		}
		for (nr=0; nr<200; nr++) {
			bless_hight[nr]=-20+sin((nr%200)/200.0*M_PI*2)*20;
		}
		bless_init=1;
	}

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;

	if (ticker>62) light=1.0;
	else light=(ticker)/62.0;

	for (step=0; step<strength*10; step+=17) {
		dd_draw_bless_pix(x,y,ticker+step+0,rgb2scr[IRGB(((int)(24*light)),((int)(24*light)),((int)(31*light)))],front,ptr);
                dd_draw_bless_pix(x,y,ticker+step+1,rgb2scr[IRGB(((int)(20*light)),((int)(20*light)),((int)(28*light)))],front,ptr);
		dd_draw_bless_pix(x,y,ticker+step+2,rgb2scr[IRGB(((int)(16*light)),((int)(16*light)),((int)(24*light)))],front,ptr);
		dd_draw_bless_pix(x,y,ticker+step+3,rgb2scr[IRGB(((int)(12*light)),((int)(12*light)),((int)(20*light)))],front,ptr);
		dd_draw_bless_pix(x,y,ticker+step+4,rgb2scr[IRGB(((int)( 8*light)),((int)( 8*light)),((int)(16*light)))],front,ptr);
	}
	dd_unlock_surface(ddbs);

	bless_time+=GetTickCount()-start;
	/*bless_cnt++;

	if ((bless_cnt&1023)==1023) {
		addline("bless: %d / %d (%f ms/bless)",bless_time,bless_cnt,(double)bless_time/bless_cnt);
	}*/
}

void dd_draw_heal(int x,int y,int start,int front)
{
	int step,nr,ticker;
	double light;
	unsigned short *ptr;
	int tmp;
	extern int tick;
	//static int bless_time=0,bless_cnt=0;

        tmp=GetTickCount();

        if (!bless_init) {
		for (nr=0; nr<36; nr++) {
			bless_sin[nr]=sin((nr%36)/36.0*M_PI*2)*8;
			bless_cos[nr]=cos((nr%36)/36.0*M_PI*2)*16;
		}
		for (nr=0; nr<200; nr++) {
			bless_hight[nr]=-20+sin((nr%200)/200.0*M_PI*2)*20;
		}
		bless_init=1;
	}

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;

	ticker=start+tick;
	if (ticker>62) light=1.0;
	else light=(ticker)/62.0;

	for (step=0; step<12*17; step+=17) {
		dd_draw_heal_pix(x,y,ticker+step+0,rgb2scr[IRGB(((int)(24*light)),((int)(31*light)),((int)(24*light)))],front,ptr);
                dd_draw_heal_pix(x,y,ticker+step+1,rgb2scr[IRGB(((int)(20*light)),((int)(28*light)),((int)(20*light)))],front,ptr);
		dd_draw_heal_pix(x,y,ticker+step+2,rgb2scr[IRGB(((int)(16*light)),((int)(24*light)),((int)(16*light)))],front,ptr);
		dd_draw_heal_pix(x,y,ticker+step+3,rgb2scr[IRGB(((int)(12*light)),((int)(20*light)),((int)(12*light)))],front,ptr);
		dd_draw_heal_pix(x,y,ticker+step+4,rgb2scr[IRGB(((int)( 8*light)),((int)(16*light)),((int)( 8*light)))],front,ptr);
	}
	dd_unlock_surface(ddbs);

	bless_time+=GetTickCount()-tmp;
	/*bless_cnt++;

	if ((bless_cnt&1023)==1023) {
		addline("bless: %d / %d (%f ms/bless)",bless_time,bless_cnt,(double)bless_time/bless_cnt);
	}*/
}

void dd_draw_potion(int x,int y,int ticker,int strength,int front)
{
	int step,nr;
	double light;
	unsigned short *ptr;

        if (!bless_init) {
		for (nr=0; nr<36; nr++) {
			bless_sin[nr]=sin((nr%36)/36.0*M_PI*2)*8;
			bless_cos[nr]=cos((nr%36)/36.0*M_PI*2)*16;
		}
		for (nr=0; nr<200; nr++) {
			bless_hight[nr]=-20+sin((nr%200)/200.0*M_PI*2)*20;
		}
		bless_init=1;
	}

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;

	if (ticker>62) light=1.0;
	else light=(ticker)/62.0;

	for (step=0; step<strength*10; step+=17) {						
		dd_draw_bless_pix(x,y,ticker+step+0,rgb2scr[IRGB(((int)(31*light)),((int)(24*light)),((int)(24*light)))],front,ptr);
                dd_draw_bless_pix(x,y,ticker+step+1,rgb2scr[IRGB(((int)(28*light)),((int)(20*light)),((int)(20*light)))],front,ptr);
		dd_draw_bless_pix(x,y,ticker+step+2,rgb2scr[IRGB(((int)(24*light)),((int)(16*light)),((int)(16*light)))],front,ptr);
		dd_draw_bless_pix(x,y,ticker+step+3,rgb2scr[IRGB(((int)(20*light)),((int)(12*light)),((int)(12*light)))],front,ptr);
		dd_draw_bless_pix(x,y,ticker+step+4,rgb2scr[IRGB(((int)(16*light)),((int)( 8*light)),((int)( 8*light)))],front,ptr);
	}
	
	dd_unlock_surface(ddbs);
}

void dd_draw_rain(int x,int y,int ticker,int strength,int front)
{
	int step;
	double light;
	unsigned short *ptr;

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;
		
	for (step=-(strength*100); step<0; step+=237) {
		dd_draw_rain_pix(x,y,-ticker+step+0,rgb2scr[IRGB(31,24,16)],front,ptr);
		dd_draw_rain_pix(x,y,-ticker+step+1,rgb2scr[IRGB(24,16,8)],front,ptr);
		dd_draw_rain_pix(x,y,-ticker+step+2,rgb2scr[IRGB(16,8,0)],front,ptr);
	}

	dd_unlock_surface(ddbs);
}

void dd_create_letter(unsigned char *rawrun,int sx,int sy,int val,char letter[16][16])
{
	int x=sx,y=sy;

	while (*rawrun!=255) {
		if (*rawrun==254) {
			y++;
			x=sx;
			rawrun++;
			continue;
		}

                x+=*rawrun++;

                letter[y][x]=val;
	}
}

char *dd_create_rawrun(char letter[16][16])
{
	char *ptr,*fon,*last;
	int x,y,step;

	last=fon=ptr=xmalloc(256,MEM_TEMP);

	for (y=3; y<16; y++) {
		step=0;
		for (x=3; x<16; x++) {
			if (letter[y][x]==2) {
				*ptr++=step; last=ptr;
				step=1;
			} else step++;
		}
		*ptr++=254;
	}
	ptr=last;
	*ptr++=255;

        fon=xrealloc(fon,ptr-fon,MEM_GLOB);
	return fon;
}

void create_shade_font(DDFONT *src,DDFONT *dst)
{
	char letter[16][16];
	int x,y,c;

	for (c=0; c<128; c++) {
		bzero(letter,sizeof(letter));
                dd_create_letter(src[c].raw,4,5,2,letter);
		dd_create_letter(src[c].raw,5,4,2,letter);
		dd_create_letter(src[c].raw,4,4,1,letter);	
                dst[c].raw=dd_create_rawrun(letter);
                dst[c].dim=src[c].dim;
	}
}

void create_frame_font(DDFONT *src,DDFONT *dst)
{
	char letter[16][16];
	int x,y,c;

	for (c=0; c<128; c++) {
		bzero(letter,sizeof(letter));
                dd_create_letter(src[c].raw,5,4,2,letter);
		dd_create_letter(src[c].raw,3,4,2,letter);
                dd_create_letter(src[c].raw,4,5,2,letter);
		dd_create_letter(src[c].raw,4,3,2,letter);
                dd_create_letter(src[c].raw,5,5,2,letter);
		dd_create_letter(src[c].raw,5,3,2,letter);
                dd_create_letter(src[c].raw,3,5,2,letter);
		dd_create_letter(src[c].raw,3,3,2,letter);
                dd_create_letter(src[c].raw,4,4,1,letter);	
                dst[c].raw=dd_create_rawrun(letter);
                dst[c].dim=src[c].dim;
	}
}

void dd_create_font(void)
{
	if (fonta_shaded) return;

	fonta_shaded=xmalloc(sizeof(DDFONT)*128,MEM_GLOB); create_shade_font(fonta,fonta_shaded);
	fontb_shaded=xmalloc(sizeof(DDFONT)*128,MEM_GLOB); create_shade_font(fontb,fontb_shaded);
	fontc_shaded=xmalloc(sizeof(DDFONT)*128,MEM_GLOB); create_shade_font(fontc,fontc_shaded);

	fonta_framed=xmalloc(sizeof(DDFONT)*128,MEM_GLOB); create_frame_font(fonta,fonta_framed);
	fontb_framed=xmalloc(sizeof(DDFONT)*128,MEM_GLOB); create_frame_font(fontb,fontb_framed);
	fontc_framed=xmalloc(sizeof(DDFONT)*128,MEM_GLOB); create_frame_font(fontc,fontc_framed);
	
}

DDFONT *textfont=fonta;
int textdisplay_dy=10;

int dd_drawtext_char(int sx, int sy, int c, unsigned short int color)
{
        unsigned short *ptr,*dst;
        unsigned char *rawrun;
        int x,y,start;

	if (c>127 || c<0) return 0;

        if ((vidptr=ptr=dd_lock_surface(ddbs))==NULL) return 0;

        rawrun=textfont[c].raw;

	x=sx;
        y=sy;

	dst=ptr+(x+x_offset)+(y+y_offset)*xres;

        while (*rawrun!=255) {
		if (*rawrun==254) {
			y++;
			x=sx;
			rawrun++;
                        dst=ptr+(x+x_offset)+(y+y_offset)*xres;
			continue;
		}

		dst+=*rawrun;
		//x+=*rawrun;
                rawrun++;

                if (dst-vidptr>xres*yres || dst<vidptr) {
			note("PANIC #5");
			dst=vidptr;
		}
		*dst=color;
		tp_cnt++;		
	}

        dd_unlock_surface(ddbs);

        return textfont[c].dim;
}

int dd_text_len(const char *text)
{
        int x;
        const char *c;

        for (x=0,c=text; *c; c++) x+=textfont[*c].dim;

        return x;
}

int dd_char_len(char c)
{
        return textfont[c].dim;
}


#define MAXTEXTLINES		256
#define MAXTEXTLETTERS		256

#define TEXTDISPLAY_X		230
#define TEXTDISPLAY_Y		438

#define TEXTDISPLAY_DY		(textdisplay_dy)

#define TEXTDISPLAY_SX		396
#define TEXTDISPLAY_SY		150

#define TEXTDISPLAYLINES	(TEXTDISPLAY_SY/TEXTDISPLAY_DY)

int textnextline=0,textdisplayline=0;

struct letter
{
	char c;
	unsigned char color;
	unsigned char link;
};

struct letter *text=NULL;

unsigned short palette[256];

void dd_init_text(void)
{
        text=xcalloc(sizeof(struct letter)*MAXTEXTLINES*MAXTEXTLETTERS,MEM_GLOB);
	palette[ 0]=rgb2scr[IRGB(31,31,31)];	// normal white text (talk, game messages)
	palette[ 1]=rgb2scr[IRGB(16,16,16)];	// dark gray text (now entering ...)
	palette[ 2]=rgb2scr[IRGB(16,31,16)];	// light green (normal chat)
	palette[ 3]=rgb2scr[IRGB(31,16,16)];	// light red (announce)
	palette[ 4]=rgb2scr[IRGB(16,16,31)];	// light blue (text links)
	palette[ 5]=rgb2scr[IRGB(24,24,31)];	// orange (item desc headings)
	palette[ 6]=rgb2scr[IRGB(31,31,16)];	// yellow (tells)
	palette[ 7]=rgb2scr[IRGB(16,24,31)];	// violet (staff chat)
	palette[ 8]=rgb2scr[IRGB(24,24,31)];	// light violet (god chat)

	palette[ 9]=rgb2scr[IRGB(24,24,16)];	// chat - auction
	palette[10]=rgb2scr[IRGB(24,16,24)];	// chat - grats
	palette[11]=rgb2scr[IRGB(16,24,24)];	// chat	- mirror
	palette[12]=rgb2scr[IRGB(31,24,16)];	// chat - info
	palette[13]=rgb2scr[IRGB(31,16,24)];	// chat - area
	palette[14]=rgb2scr[IRGB(16,31,24)];	// chat - v2, games
	palette[15]=rgb2scr[IRGB(24,31,16)];	// chat - public clan
	palette[16]=rgb2scr[IRGB(24,16,31)];	// chat	- internal clan

	palette[17]=rgb2scr[IRGB(31,31,31)];	// fake white text (hidden links)
}

void dd_set_textfont(int nr)
{
	extern int namesize;
	int n;
	
	switch(nr) {
		case 0:	textfont=fonta; textdisplay_dy=10; break;
		case 1:	textfont=fontc; textdisplay_dy=12; break;
	}
	bzero(text,MAXTEXTLINES*MAXTEXTLETTERS*sizeof(struct letter));
	textnextline=textdisplayline=0;
}

void dd_display_text(void)
{
	int n,m,rn,x,y,pos;

        for (n=textdisplayline,y=TEXTDISPLAY_Y; y<=TEXTDISPLAY_Y+TEXTDISPLAY_SY-TEXTDISPLAY_DY; n++,y+=TEXTDISPLAY_DY) {
		rn=n%MAXTEXTLINES;

		x=TEXTDISPLAY_X;
		pos=rn*MAXTEXTLETTERS;

		for (m=0; m<MAXTEXTLETTERS; m++,pos++) {
			if (text[pos].c==0) break;

			if (text[pos].c>0 && text[pos].c<32) {
				int i;

				x=((int)text[pos].c)*12+TEXTDISPLAY_X;

				// better display for numbers
				for (i=pos+1; isdigit(text[i].c) || text[i].c=='-'; i++) {
					x-=textfont[text[i].c].dim;
				}
				continue;
			}
                        x+=dd_drawtext_char(x,y,text[pos].c,palette[text[pos].color]);			
		}				
	}	
}

void dd_add_text(char *ptr)
{
	int n,m,pos,x=0,color=0,tmp,link=0;
	char buf[256];

        pos=textnextline*MAXTEXTLETTERS;
	bzero(text+pos,sizeof(struct letter)*MAXTEXTLETTERS);

        while (*ptr) {

		
		while (*ptr==' ') ptr++;
		while (*ptr=='°') {
			ptr++;
			switch(*ptr) {
				case 'c':	tmp=atoi(ptr+1);
						if (tmp==18) link=0;
                                                else if (tmp!=17) { color=tmp; link=0; }
						if (tmp==4 || tmp==17) link=1;
						ptr++;
						while (isdigit(*ptr)) ptr++;
						break;
				default:	ptr++; break;
			}
		}
		while (*ptr==' ') ptr++;
		
		n=0;
                while (*ptr && *ptr!=' ' && *ptr!='°' && n<49) buf[n++]=*ptr++;
		buf[n]=0;
		
		if (x+(tmp=dd_text_len(buf))>=TEXTDISPLAY_SX) {
			if (textdisplayline==(textnextline+(MAXTEXTLINES-TEXTDISPLAYLINES))%MAXTEXTLINES) textdisplayline=(textdisplayline+1)%MAXTEXTLINES;
			textnextline=(textnextline+1)%MAXTEXTLINES;
                        pos=textnextline*MAXTEXTLETTERS;
			bzero(text+pos,sizeof(struct letter)*MAXTEXTLETTERS);
			x=tmp;

#define INDENT_TEXT
#ifdef INDENT_TEXT
			for (m=0; m<2; m++) {
				text[pos].c=32; x+=textfont[32].dim;
				text[pos].color=color;
				text[pos].link=link;
				pos++;
			}
#endif

		} else x+=tmp;
		
		//printf("adding %s at pos %d (nextline=%d)\n",buf,pos,textnextline);

		for (m=0; m<n; m++,pos++) {
			text[pos].c=buf[m];
			text[pos].color=color;
			text[pos].link=link;
		}
		text[pos].c=32; x+=textfont[32].dim;
		text[pos].color=color;
		text[pos].link=link;
		
		pos++;
	}
        pos=(pos+MAXTEXTLETTERS*MAXTEXTLINES-1)%(MAXTEXTLETTERS*MAXTEXTLINES);
	if (text[pos].c==32) {
		text[pos].c=0;
		text[pos].color=0;
		text[pos].link=0;
		//printf("erase space at %d\n",pos);
	}

	if (textdisplayline==(textnextline+(MAXTEXTLINES-TEXTDISPLAYLINES))%MAXTEXTLINES) textdisplayline=(textdisplayline+1)%MAXTEXTLINES;
		
        textnextline=(textnextline+1)%MAXTEXTLINES;
	if (textnextline==textdisplayline) textdisplayline=(textdisplayline+1)%MAXTEXTLINES;
        //textdisplayline=(textnextline+(MAXTEXTLINES-TEXTDISPLAYLINES))%MAXTEXTLINES;
}

int dd_text_init_done(void)
{
	return text!=NULL;
}

int dd_scantext(int x,int y,char *hit)
{
	int n,m,pos,dx,panic=0,tmp=0;

	if (x<TEXTDISPLAY_X || y<TEXTDISPLAY_Y) return 0;
	if (x>TEXTDISPLAY_X+TEXTDISPLAY_SX) return 0;
	if (y>TEXTDISPLAY_Y+TEXTDISPLAY_SY) return 0;	

	n=(y-TEXTDISPLAY_Y)/TEXTDISPLAY_DY;
	n=(n+textdisplayline)%MAXTEXTLINES;
	
        for (pos=n*MAXTEXTLETTERS,dx=m=0; m<MAXTEXTLETTERS && text[pos].c; m++,pos++) {
		if (text[pos].c>0 && text[pos].c<32) { dx=((int)text[pos].c)*12+TEXTDISPLAY_X; continue; }
		dx+=textfont[text[pos].c].dim;
		if (dx+TEXTDISPLAY_X>x) {
			if (text[pos].link) {	// link palette color
				while ((text[pos].link || text[pos].c==0) && panic++<5000) {
					pos--;
					if (pos<0) pos=MAXTEXTLETTERS*MAXTEXTLINES-1;
				}
				
				pos++;
				if (pos==MAXTEXTLETTERS*MAXTEXTLINES) pos=0;
                                while ((text[pos].link || text[pos].c==0) && panic++<5000 && tmp<80) {
					if (tmp>0 && text[pos].c==' ' && hit[tmp-1]==' ') ;
                                        else if (text[pos].c) hit[tmp++]=text[pos].c;
					pos++;
				}
				if (tmp>0 && hit[tmp-1]==' ') hit[tmp-1]=0;
                                else hit[tmp]=0;
				return 1;
			}
			return 0;
		}
	}
	return 0;
}

void dd_text_lineup(void)
{
	int tmp;

        tmp=(textdisplayline+MAXTEXTLINES-1)%MAXTEXTLINES;
	if (tmp!=textnextline) textdisplayline=tmp;
}

void dd_text_pageup(void)
{
	int n,tmp;

	for (n=0; n<TEXTDISPLAYLINES; n++) {
		tmp=(textdisplayline+MAXTEXTLINES-1)%MAXTEXTLINES;
		if (tmp==textnextline) break;
		textdisplayline=tmp;
	}
}

void dd_text_linedown(void)
{
	int tmp;

        tmp=(textdisplayline+1)%MAXTEXTLINES;
	if (tmp!=(textnextline+MAXTEXTLINES-TEXTDISPLAYLINES+1)%MAXTEXTLINES) textdisplayline=tmp;
}

void dd_text_pagedown(void)
{
	int n,tmp;

	for (n=0; n<TEXTDISPLAYLINES; n++) {
		tmp=(textdisplayline+1)%MAXTEXTLINES;
		if (tmp==(textnextline+MAXTEXTLINES-TEXTDISPLAYLINES+1)%MAXTEXTLINES) break;
		textdisplayline=tmp;
	}
}

//#define DEBUGGING

#ifdef DEBUGGING
void vid_mark(struct vid_cache *vc,int type);
#endif

#define VID_MIN_KEEP_BYTES	300
#define VID_MIN_KEEP_DX		10
#define VID_MIN_KEEP_DY		10

struct vid_cache
{
	unsigned char cs;
	unsigned char dx,dy;

	unsigned short px,py;
        unsigned short size;

	unsigned short vidx;		// index into vidx array of sprite which is using this entry
	unsigned int sidx;		// index nr. of sprite which is using this entry

	struct vid_cache *next;         // large cache: next entry in large list
					// small cache: next entry in free list
	
	struct vid_cache *prev;		// large cache: previous entry in large list
					// small cache: previous entry in free list

	struct vid_cache *child;	// large cache: first child
					// small cache: next child

	struct vid_cache *parent;	// large cache: unused
					// small cache: parent

	unsigned int used,tick,junk;
	unsigned int value;
};

struct vid_cache *vcs_first=NULL;
struct vid_cache *vcs_last=NULL;

struct vid_cache *vcl_first=NULL;
struct vid_cache *vcl_last=NULL;

struct vid_cache *vc_unused=NULL;

static struct vid_cache *insert_new_hint=NULL;
static struct vid_cache *insert_used_hint=NULL;

// Initialise video cache
void vid_init(void)
{
	int n,x,y;
	struct vid_cache *vc;
	struct vid_cache *vc_mempool;
	int mempoolsize=0;

	//printf("vid init start\n");
	for (n=0; n<ddcs_used; n++) {
                for (y=0; y+TILESIZEDY-1<ddcs_ymax[n]; y+=TILESIZEDY) {
			for (x=0; x+TILESIZEDX-1<ddcs_xmax[n]; x+=TILESIZEDX) {
				// create new entry
				if (!mempoolsize) {
					mempoolsize=256;
					vc_mempool=xmalloc(sizeof(struct vid_cache)*mempoolsize,MEM_VLC);
				}
				vc=vc_mempool++; mempoolsize--;
				//vc=xmalloc(sizeof(struct vid_cache),MEM_VLC);

				// add to large entry list
				vc->next=vcl_first;
				vc->prev=NULL;
				if (vcl_first) vcl_first->prev=vc;
				vcl_first=vc;
				if (!vcl_last) vcl_last=vc;
				
				// add values
				vc->child=NULL;
				vc->parent=NULL;
				vc->cs=n;
				vc->dx=TILESIZEDX;
				vc->dy=TILESIZEDY;
				vc->px=x;
				vc->py=y;
				vc->sidx=SIDX_NONE;
				vc->size=TILESIZEDX*TILESIZEDY;
				vc->vidx=VIDX_NONE;

				vc->used=vc->tick=vc->value=vc->junk=0;
			}
		}
	}

	//printf("vid init done\n");
}

// calculate the value of this cache entry (ie is it worth keeping?)
static unsigned int vid_value(struct vid_cache *vc)
{
	return min(1024,vc->used)+(vc->tick<<8);
}

// add a cache entry to the unused list
static void vid_free_free_small(struct vid_cache *vc)
{
	vc->next=vc_unused;
	vc_unused=vc;
}

// get a unused cache entry from out internal list (faster than malloc'ing and free'ing them over and over)
static struct vid_cache *vid_get_free_small(void)
{
	struct vid_cache *vn;

	if (vc_unused) {
		vn=vc_unused;
		vc_unused=vn->next;
	} else {
		int n;

		vn=xmalloc(sizeof(struct vid_cache)*256,MEM_VSC);
		for (n=1; n<256; n++) {
			vid_free_free_small(vn+n);
		}
		
		//vn=xmalloc(sizeof(struct vid_cache),MEM_VSC);
	}

        return vn;
}


// reset video cache (eg after the surface was lost)
static void vid_reset(void)
{
	struct vid_cache *vr,*vl,*tmp;
	int sidx,vidx;
	
	//printf("vid reset start.\n");

        for (vl=vcl_first; vl; vl=vl->next) {
                for (vr=vl->child; vr; vr=tmp) {
			tmp=vr->child;

			if (vr->sidx!=SIDX_NONE) {
				systemcache[vr->sidx].vc[vr->vidx]=NULL;
				systemcache[vr->sidx].lost++;
                                vid_free_free_small(vr);
			}
		}
		vl->used=vl->tick=vl->value=0;
		vl->child=vl->parent=NULL;
	}

	for (vr=vcs_first; vr; vr=tmp) {
		tmp=vr->next;
                vid_free_free_small(vr);
	}
	vcs_first=NULL;
	vcs_last=NULL;
	insert_new_hint=NULL;
	insert_used_hint=NULL;
	
	// purely error checking:
	for (sidx=0; sidx<max_systemcache; sidx++) {
		for (vidx=0; vidx<systemcache[sidx].tdx*systemcache[sidx].tdy; vidx++) {
			if (systemcache[sidx].vc[vidx]) {
				printf("systemcache %d,%d still linked to %p\n",sidx,vidx,systemcache[sidx].vc[vidx]);
			}
			
		}
	}
        //printf("vid reset done.\n");
}

// create the bitmap for a video cache entry from a systemcache entry
static int vid_create(struct vid_cache *vc,int sidx, int sx, int sy, int dx, int dy)
{
        SYSTEMCACHE *sc;
        int x,y,start,dststep,srcstep;
        unsigned short int *ptr,*dst,*src,rgb;
	unsigned int pix=0;

        start=GetTickCount();

        // easy use of sc and vc ...
        sc=&systemcache[sidx];

        // copy the colors to the tile
        ptr=dd_lock_surface(ddcs[vc->cs]);
        if (!ptr) return fail("dd_lock_surface(ddbs); failed in sys_copy()");

        dststep=ddcs_xres[vc->cs]-dx;
	dst=&ptr[vc->px+vc->py*ddcs_xres[vc->cs]];

        srcstep=sc->xres-dx;
	src=&sc->rgb[sx+sy*sc->xres];

        for (y=0; y<dy; y++) {
                for (x=0; x<dx; x++) {
                        rgb=*src++;
                        if (rgb!=scrcolorkey) pix++;
                        *dst++=rgb;
                }
		dst+=dststep;
		src+=srcstep;
        }

        dd_unlock_surface(ddcs[vc->cs]);

	vm_time+=GetTickCount()-start;
	vm_cnt++;

        return pix==0;
}

// blit a video cache entry to the back surface
static void vid_blit(struct vid_cache *vc, int sx, int sy, int dx, int dy)
{
        int err,addx=0,addy=0,start;
        RECT rc;

        if (sx<clipsx) { addx=clipsx-sx; dx-=addx; sx=clipsx; }
        if (sy<clipsy) { addy=clipsy-sy; dy-=addy; sy=clipsy; }
        if (sx+dx>=clipex) dx=clipex-sx;
        if (sy+dy>=clipey) dy=clipey-sy;

        if (dy<=0 || dx<=0) return;

	if (dx>vc->dx || dy>vc->dy) {
		printf("want: %dx%d, have: %dx%d\n",dx,dy,vc->dx,vc->dy);
		return;
	}

	start=GetTickCount();

        rc.left=vc->px+addx;
        rc.top=vc->py+addy;
        rc.right=rc.left+dx;
        rc.bottom=rc.top+dy;

	if ((err=ddbs->lpVtbl->BltFast(ddbs,sx+x_offset,sy+y_offset,ddcs[vc->cs],&rc,DDBLTFAST_SRCCOLORKEY|DDBLTFAST_WAIT))!=DD_OK) {
		char buf[256];
		sprintf(buf,"vc_blit(): %d,%d -> %d,%d to %d,%d from %d (%d,%d %dx%d) daddy: %d,%d size %dx%d, addx=%d, addy=%d, want size %dx%d",
			rc.left,rc.top,rc.right,rc.bottom,sx+x_offset,sy+y_offset,vc->cs,vc->px,vc->py,vc->dx,vc->dy,
			vc->parent ? vc->parent->px : 42,
			vc->parent ? vc->parent->py : 42,
			vc->parent ? vc->parent->dx : 42,
			vc->parent ? vc->parent->dy : 42,
			addx,addy,dx,dy);
		dd_error(buf,err);
	}

	vc_time+=GetTickCount()-start;
	vc_cnt++;
}

// remove large cache entry from large chain
void vid_remove_vcl(struct vid_cache *vcl)
{	
	// remove from chain
	if (vcl->next) vcl->next->prev=vcl->prev;
	else vcl_last=vcl->prev;
	if (vcl->prev) vcl->prev->next=vcl->next;
	else vcl_first=vcl->next;
}

// insert large cache entry to correct position in chain
void vid_insert_vcl(struct vid_cache *vcl,struct vid_cache *hint1,struct vid_cache *hint2)
{
	struct vid_cache *vc,*hint;
	int start;
	//int step=0,nostep=0,prevstep=0,nextstep=0;
	//static int calls=0,steps=0,nosteps=0,prevsteps=0,nextsteps=0;

	//printf("vid insert start\n");
	start=GetTickCount();

	if (!hint2) hint=hint1;
	else if (!hint1) hint=hint2;
        else if (abs(hint1->value-vcl->value)<abs(hint2->value-vcl->value)) hint=hint1;
	else hint=hint2;

	//printf("insert start\n");
	if (hint==vcl) { printf("hu?\n"); hint=NULL; }

        /* //check if hint is part of chain (slow!)
	for (vc=vcl_first; vc; vc=vc->next) {
			if (vc==hint) break;
			//step++; nostep++;
	}
	if (hint!=vc) { printf("hint not found!\n"); hint=NULL; }*/
	
	// find correct place in list
	if (!hint) {	// no hint? search whole chain
		for (vc=vcl_first; vc; vc=vc->next) {
			if (vc->value>=vcl->value) break;
			//step++; nostep++;
		}
	} else {
		if (hint->value>vcl->value) {	// we need to go backwards
			for (vc=hint; vc->prev; vc=vc->prev) {
				if (vc->prev->value<=vcl->value) break;
				//step++; prevstep++;
			}
		} else { // go forward
			for (vc=hint; vc; vc=vc->next) {
			       if (vc->value>=vcl->value) break;
			       //step++; nextstep++;
			}
		}
	}

	/*calls++;
	steps+=step;
	nosteps+=nostep;
	prevsteps+=prevstep;
	nextsteps+=nextstep;

	if (calls%1000==0 || step>20) {
		if (hint) printf("%d step calls: %d, step: %d steps per call: %d (%d,%d,%d) hint=%d, want=%d\n",step,calls,steps,steps/calls,nosteps,prevsteps,nextsteps,hint->value,vcl->value);
	}*/
	
	// add to new place in chain
	if (vc) {	// not end of chain
		if (vc->prev) vc->prev->next=vcl;
		else vcl_first=vcl;
		vcl->prev=vc->prev;
		vc->prev=vcl;
		vcl->next=vc;
	} else {	// end of chain
		vcl->prev=vcl_last;
		vcl->next=NULL;
		vcl_last->next=vcl;
		vcl_last=vcl;
	}

	vi_time+=GetTickCount()-start;

	//printf("vid insert stop\n");
}

// add small cache entry to list containing free (but used) entries
static void vid_add_cache_small(struct vid_cache *vn)
{
	vn->next=vcs_first;
	vn->prev=NULL;
	if (vcs_first) vcs_first->prev=vn;
	vcs_first=vn;
	if (!vcs_last) vcs_last=vn;	

#ifdef DEBUGGING
	vid_mark(vn,2);
#endif
}

// mark small cache entry as free (but used) and add it to the free chain
static void vid_free_cache(struct vid_cache *vc)
{
	vc->sidx=SIDX_NONE;
	vc->vidx=VIDX_NONE;

        vid_add_cache_small(vc);
}

#ifdef DEBUGGING
void vid_mark(struct vid_cache *vc,int type)
{
	int err;
        RECT rc;
	DDBLTFX bltfx;
	
	bzero(&bltfx,sizeof(bltfx));
	bltfx.dwSize=sizeof(bltfx);
	if (type==0) bltfx.u5.dwFillColor=rgb2scr[IRGB(0,0,16)];
	else if (type==1) bltfx.u5.dwFillColor=rgb2scr[IRGB(16,0,0)];
	else bltfx.u5.dwFillColor=rgb2scr[IRGB(0,16,0)];

	rc.left=vc->px;
	rc.top=vc->py;
	rc.right=vc->px+vc->dx;
	rc.bottom=vc->py+vc->dy;

	if ((err=ddbs->lpVtbl->Blt(ddcs[vc->cs],&rc,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&bltfx))!=DD_OK) {
                dd_error("vid erase1",err);
	}
	if (type==0) bltfx.u5.dwFillColor=rgb2scr[IRGB(0,0,31)];
	else if (type==1) bltfx.u5.dwFillColor=rgb2scr[IRGB(31,0,0)];
	else bltfx.u5.dwFillColor=rgb2scr[IRGB(0,31,0)];

	rc.left=vc->px+1;
	rc.top=vc->py+1;
	rc.right=vc->px+vc->dx-1;
	rc.bottom=vc->py+vc->dy-1;

	if (rc.left>=rc.right || rc.top>=rc.bottom) return;

	if ((err=ddbs->lpVtbl->Blt(ddcs[vc->cs],&rc,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&bltfx))!=DD_OK) {
		char buf[80];
		sprintf(buf,"vid erase2: %d,%d %d,%d (%dx%d at %d,%d)",rc.left,rc.top,rc.right,rc.bottom,vc->dx,vc->dy,vc->px,vc->py);
                dd_error(buf,err);
	}
}
#endif

// remove small cache entry from free (but used) list and add it to unused list
static void vid_remove_free_small(struct vid_cache *vc)
{
	if (vc->next) vc->next->prev=vc->prev;
	else vcs_last=vc->prev;
	if (vc->prev) vc->prev->next=vc->next;
	else vcs_first=vc->next;
	vid_free_free_small(vc);
}

// check the children of a large entry and unify adjacent ones
static void vid_unify(struct vid_cache *vcl)
{
	struct vid_cache *vc1,*vc2,*prev;

	for (vc1=vcl->child; vc1; vc1=vc1->child) {
                if (vc1->sidx!=SIDX_NONE) continue;
		
		for (vc2=vcl->child,prev=vcl; vc2; prev=vc2,vc2=vc2->child) {
                        if (vc1==vc2) continue;
			if (vc2->sidx!=SIDX_NONE) continue;

			/*if (vc1->dx!=vc2->dx && vc1->dy!=vc2->dy) continue;
			if (vc1->px!=vc2->px && vc1->py!=vc2->py) continue;

			printf("trying unify: %d,%d %dx%d with %d,%d %dx%d\n",vc1->px,vc1->py,vc1->dx,vc1->dy,vc2->px,vc2->py,vc2->dx,vc2->dy);*/
			
			if (vc1->px+vc1->dx==vc2->px && vc1->py==vc2->py && vc1->dy==vc2->dy) {
				vc1->dx+=vc2->dx;
				
				prev->child=vc2->child;
				vid_remove_free_small(vc2);

#ifdef DEBUGGING
				vid_mark(vc1,2);
#endif
				return;
			}
			if (vc1->py+vc1->dy==vc2->py && vc1->px==vc2->px && vc1->dx==vc2->dx) {
				vc1->dy+=vc2->dy;
				
				prev->child=vc2->child;
				vid_remove_free_small(vc2);

#ifdef DEBUGGING
				vid_mark(vc1,2);
#endif
				return;
			}

		}
	}
}

// create a small cache entry which contains the systemcache bitmat addressed by sidx sx,sy dx,dy
// updates small and large chains
static struct vid_cache *vid_make_tile(int vidx,int sidx, int sx, int sy, int dx, int dy)
{
	struct vid_cache *vcs,*bestvcs=NULL,*tmp;
	struct vid_cache *vcl;
	struct vid_cache *vc;
	struct vid_cache *vn;
        int waste,bestwaste=99999999,size,empty,cnt;

	//printf("make tile start\n");

	// search free list to find fitting tile
	size=dx+dy;
        for (vcs=vcs_first,cnt=0; vcs && cnt<25; vcs=vcs->next,cnt++) {	
		if (vcs->dx<dx || vcs->dy<dy) continue;

		waste=vcs->size-size;
		if (waste<bestwaste) {
			bestwaste=waste;
			bestvcs=vcs;
		}
	}

        // put entries we looked at at end of chain
	if (vcs && vcs->prev && vcs->next) {
		vcs_first->prev=vcs_last;
		vcs_last->next=vcs_first;
		vcs_last=vcs->prev;
		vcs_last->next=NULL;
		vcs->prev=NULL;
		vcs_first=vcs;
	}

        if (bestvcs) {		// we found one in the small list. remove it from list and use it
                // remove from free list
                if (bestvcs->prev) bestvcs->prev->next=bestvcs->next;
		else vcs_first=bestvcs->next;
		
		if (bestvcs->next) bestvcs->next->prev=bestvcs->prev;
		else vcs_last=bestvcs->prev;

                vc=bestvcs;
	} else {		// none found in small list. get one entry from big list, move it to the bottom and
				// create a small entry for the new tile from the big one
                // remove from first place in list and update hint if needed
		vcl=vcl_first;
		if (insert_new_hint==vcl) insert_new_hint=vcl->next;
		if (insert_used_hint==vcl) insert_used_hint=vcl->next;
                vcl_first=vcl->next;
		vcl_first->prev=NULL;

		//printf("looping on\n");
                // delete sprites and free children
		for (vc=vcl->child; vc; vc=tmp) {
			tmp=vc->child;

			if (vc->sidx!=SIDX_NONE) {				
				systemcache[vc->sidx].vc[vc->vidx]=NULL;
				systemcache[vc->sidx].lost++;
				vid_free_free_small(vc);
			} else {
                                vid_remove_free_small(vc);
			}
		}		

		// reset stats
		vcl->used=1;	//systemcache[sidx].used;
		vcl->tick=dd_tick;
		vcl->junk=dd_tick+rrand(24*90);
		vcl->value=vid_value(vcl);

		vid_insert_vcl(vcl,insert_new_hint,NULL); insert_new_hint=vcl;

		// create small cache entry and initialise it from large entry
		vc=vid_get_free_small();

		vcl->child=vc;
		vc->cs=vcl->cs;
		vc->dx=vcl->dx;
		vc->dy=vcl->dy;
		vc->child=NULL;
		vc->px=vcl->px;
		vc->py=vcl->py;
		vc->size=vc->dx*vc->dy;
		vc->parent=vcl;
		vc->sidx=SIDX_NONE;
		vc->vidx=VIDX_NONE;
	}

#ifdef DEBUGGING
	vid_mark(vc,0);
#endif


        empty=vid_create(vc,sidx,sx,sy,dx,dy);
	
	if (empty) {	// add to small cache if empty
                vid_add_cache_small(vc);

		return NULL;
	}

	// remember the sprite stored
	vc->vidx=vidx;
	vc->sidx=sidx;
	vc->next=NULL;
	vc->prev=NULL;

#if 1
	if (vc->dx-dx>vc->dy-dy) {
		if (vc->dx-dx>VID_MIN_KEEP_DX && vc->dy>VID_MIN_KEEP_DY && (vc->dx-dx)*(vc->dy)>VID_MIN_KEEP_BYTES) {	// worth keeping?
			// create new small cache entry
			vn=vid_get_free_small();

			// add it to the list of tiles belonging to large entry
			vn->child=vc->child;
			vc->child=vn;
	
			// note surface
			vn->cs=vc->cs;
	
			// compute position
			vn->px=vc->px+dx;
			vn->py=vc->py;
	
			// compute size
			vn->dx=vc->dx-dx;
			vn->dy=vc->dy;
			vn->size=vn->dx*vn->dy;
	
			// not used, so erase sprite index
			vn->sidx=SIDX_NONE;
			vn->vidx=VIDX_NONE;

			// remember parent
			vn->parent=vc->parent;

			// and add to free list
			vid_add_cache_small(vn);
		}
	
		if (vc->dy-dy>VID_MIN_KEEP_DY && dx>VID_MIN_KEEP_DX && (vc->dy-dy)*(dx)>VID_MIN_KEEP_BYTES) {	// worth keeping?
			// create new small cache entry
			vn=vid_get_free_small();
	
			// add it to the list of tiles belonging to large entry
			vn->child=vc->child;
			vc->child=vn;
	
			// note surface
			vn->cs=vc->cs;
	
			// compute position
			vn->px=vc->px;
			vn->py=vc->py+dy;
	
			// compute size
			vn->dx=dx;
			vn->dy=vc->dy-dy;
			vn->size=vn->dx*vn->dy;
	
			// not used, so erase sprite index
			vn->sidx=SIDX_NONE;
			vn->vidx=VIDX_NONE;

			// remember parent
			vn->parent=vc->parent;

			// and add to free list
			vid_add_cache_small(vn);
		}
	} else {
		if (vc->dx-dx>VID_MIN_KEEP_DX && dy>VID_MIN_KEEP_DY && (vc->dx-dx)*(dy)>VID_MIN_KEEP_BYTES) {	// worth keeping?
			// create new small cache entry
			vn=vid_get_free_small();
	
			// add it to the list of tiles belonging to large entry
			vn->child=vc->child;
			vc->child=vn;
	
			// note surface
			vn->cs=vc->cs;
	
			// compute position
			vn->px=vc->px+dx;
			vn->py=vc->py;
	
			// compute size
			vn->dx=vc->dx-dx;
			vn->dy=dy;
			vn->size=vn->dx*vn->dy;
	
			// not used, so erase sprite index
			vn->sidx=SIDX_NONE;
			vn->vidx=VIDX_NONE;

			// remember parent
			vn->parent=vc->parent;

			// and add to free list
			vid_add_cache_small(vn);
		}
	
		if (vc->dy-dy>VID_MIN_KEEP_DY && vc->dx>VID_MIN_KEEP_DX && (vc->dy-dy)*(vc->dx)>VID_MIN_KEEP_BYTES) {	// worth keeping?
			// create new small cache entry
			vn=vid_get_free_small();
	
			// add it to the list of tiles belonging to large entry
			vn->child=vc->child;
			vc->child=vn;
	
			// note surface
			vn->cs=vc->cs;
	
			// compute position
			vn->px=vc->px;
			vn->py=vc->py+dy;
	
			// compute size
			vn->dx=vc->dx;
			vn->dy=vc->dy-dy;
			vn->size=vn->dx*vn->dy;
	
			// not used, so erase sprite index
			vn->sidx=SIDX_NONE;
			vn->vidx=VIDX_NONE;

			// remember parent
			vn->parent=vc->parent;

			// and add to free list
			vid_add_cache_small(vn);
		}
	}
#endif
	vc->dx=dx;
	vc->dy=dy;
	vc->size=vc->dx*vc->dy;

	vid_unify(vc->parent);

	//printf("creating kids done (%p %d,%d %dx%d)\n",vc,vc->px,vc->py,vc->dx,vc->dy);

	//printf("make tile done2\n");

	return vc;
}

// blit a systemcache entry to backsurface without touching the video cache
void sc_blit3(SYSTEMCACHE *sc,int sx,int sy)
{
	int x,y,addx=0,addy=0,sstep,dstep,dx,dy;
	unsigned short *ptr,col,*dst,*src;

	dx=sc->xres; dy=sc->yres;

        if (sx<clipsx) { addx=clipsx-sx; dx-=addx; sx=clipsx; }
        if (sy<clipsy) { addy=clipsy-sy; dy-=addy; sy=clipsy; }
        if (sx+dx>=clipex) dx=clipex-sx;
        if (sy+dy>=clipey) dy=clipey-sy;

        if (dy<=0 || dx<=0) return;

        if ((ptr=dd_lock_surface(ddbs))==NULL) return;

        //spos=addx+addy*sc->xres;
	src=sc->rgb+addx+addy*sc->xres;
	sstep=sc->xres-dx;
	//dpos=sx+sy*xres;
	dst=ptr+(sx+x_offset)+(sy+y_offset)*xres;
	dstep=xres-dx;

	for (y=0; y<dy; y++) {
                for (x=0; x<dx; x++) {
			col=*src++;
			
			if (col==scrcolorkey) { dst++; continue; }

			*dst++=col;
		}
		src+=sstep;
		dst+=dstep;
	}

        dd_unlock_surface(ddbs);
}

// blit a systemcache entry to backsurface without touching the video cache
void sc_blit4(SYSTEMCACHE *sc,int scrx,int scry,int sx,int sy,int dx,int dy)
{
	int x,y,addx=0,addy=0,sstep,dstep;
	unsigned short *ptr,col,*dst,*src;

        if (sx+scrx<clipsx) { addx=clipsx-(sx+scrx); dx-=addx; sx+=addx; }
        if (sy+scry<clipsy) { addy=clipsy-(sy+scry); dy-=addy; sy+=addy; }
        if (sx+scrx+dx>=clipex) dx=clipex-(sx+scrx);
        if (sy+scry+dy>=clipey) dy=clipey-(sy+scry);

        if (dy<=0 || dx<=0) return;

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;

	//spos=addx+addy*sc->xres;
	src=sc->rgb+addx+sx+(addy+sy)*sc->xres;
	sstep=sc->xres-dx;
	//dpos=sx+sy*xres;
	dst=ptr+(sx+x_offset+scrx)+(sy+y_offset+scry)*xres;
	dstep=xres-dx;

	for (y=0; y<dy; y++) {
                for (x=0; x<dx; x++) {

			col=*src++;
			
			if (col==scrcolorkey) { dst++; continue; }

			*dst++=col;
		}
		src+=sstep;
		dst+=dstep;
	}

        dd_unlock_surface(ddbs);
}

// blit a systemcache entry to the screen
static int sc_blit2(DDFX *ddfx, int sidx, int scrx, int scry)
{
        SYSTEMCACHE *sc;
        int v,tx,ty,use,size;
        struct vid_cache *vcl,*vc;

        PARANOIA( if (sidx==SIDX_NONE) paranoia("sc_blit: sidx==SIDX_NONE"); )
        PARANOIA( if (sidx>=max_systemcache) paranoia("sc_blit: sidx>=max_systemcache (%d>=%d)",sidx,max_systemcache); )

        // easy use of sc
        sc=&systemcache[sidx];

	if (!vcl_first || (sc->lost && min(1024,sc->used)+(sc->tick<<8)-256<vcl_first->value)) {	// not in cache and not important enough to add to cache
		sc_blit3(sc,scrx,scry);
		sc_blits++;
	} else {
		// loop over all neccassary tiles
		for (ty=0; ty<sc->tdy; ty++) {
			for (tx=0; tx<sc->tdx; tx++) {
	
				v=vget(tx,ty,sc->tdx,sc->tdy);
	
				if (sc->vc[v]==((void*)(-1))) continue;	// empty tile
	
				if ((vc=sc->vc[v])!=NULL) {
					vid_blit(vc,scrx+tx*TILESIZEDX,scry+ty*TILESIZEDY,min(TILESIZEDX,sc->xres-tx*TILESIZEDX),min(TILESIZEDY,sc->yres-ty*TILESIZEDY));
	
					vc_hit++;

                                        // increase usage count of tile:
					// get large cache tile containing this tile
					vcl=vc->parent;
					
					if ((unsigned)dd_tick>vcl->junk) {
						vcl->used=vcl_first->used;
						vcl->tick=vcl_first->tick;
						vcl->value=vcl_first->value;
					} else {
						if (vc->size>(TILESIZEDX*TILESIZEDY/2)) use=8;
						else if (vc->size>(TILESIZEDX*TILESIZEDY/4)) use=4;
						else if (vc->size>(TILESIZEDX*TILESIZEDY/8)) use=2;
						else use=1;

						vcl->used+=use;
						vcl->tick=dd_tick;
						vcl->value=vid_value(vcl);
					}
	
					// re-sort
					vc=vcl->next;			// remember insert hint
					if (!vc) vc=vcl_last->prev;	// we are at end of list, return the one in front of us
					if (insert_used_hint==vcl) insert_used_hint=vcl->next;

                                        vid_remove_vcl(vcl);
                                        vid_insert_vcl(vcl,vc,insert_used_hint); insert_used_hint=vcl;
					
                                        continue;
				}
	
				vc_miss++;
	
				// build
				vc=vid_make_tile(v,sidx,tx*TILESIZEDX,ty*TILESIZEDY,min(TILESIZEDX,sc->xres-tx*TILESIZEDX),min(TILESIZEDY,sc->yres-ty*TILESIZEDY));
	
				// note and ignore empty ones
				if (!vc) { sc->vc[v]=((void*)(-1)); continue; }
	
				// remember
				sc->vc[v]=vc;
	
				vc_unique++;
	
				// and, finally, blit it onto the screen
				vid_blit(sc->vc[v],scrx+tx*TILESIZEDX,scry+ty*TILESIZEDY,min(TILESIZEDX,sc->xres-tx*TILESIZEDX),min(TILESIZEDY,sc->yres-ty*TILESIZEDY));
			}
		}		
		sc->lost=0;
		sc->used=0;
		sc->tick=0;
	}
        // draw the alpha pixels
        if (sc->acnt) sc_blit_apix(sidx,scrx,scry,ddfx->grid,ddfx->freeze);

	sc->tick=dd_tick;
        sc->used++;

	sc_cnt++;

        return 0;
};

#ifdef DEVELOPER
static int children,large,small;
static int used,freed,wasted,w40;

void vid_show_child(struct vid_cache *vc,struct vid_cache *parent,int nr)
{
	int tused=0,tfreed=0,w40ed=0;

	for (; vc; vc=vc->child) {
		//if (vc->sidx!=SIDX_NONE) printf(" used child at %d, pos %d,%d, size %dx%d (%d) used by sprite %d at pos %d\n",vc->cs,vc->px,vc->py,vc->dx,vc->dy,vc->size,vc->sidx,vc->vidx);
		//else printf(" unused child at %d, pos %d,%d, size %dx%d (%d) used by sprite %d at pos %d\n",vc->cs,vc->px,vc->py,vc->dx,vc->dy,vc->size,vc->sidx,vc->vidx);
		if (vc->parent!=parent) {
			printf(" child has wrong parent! (%d)\n",nr);
			exit(1);
		}
		if (vc->sidx!=SIDX_NONE) { children++; tused+=vc->size; }
		else tfreed+=vc->size;

		w40ed+=48*48-vc->dx*vc->dy;
	}
        //printf("Used: %dB, Free: %dB, Wasted: %dB / %dB\n",tused,tfreed,parent->size-tused-tfreed,w40ed);

	used+=tused; freed+=tfreed; wasted+=parent->size-tused-tfreed; w40+=w40ed;
}

void vid_show(int nr)
{
	struct vid_cache *vc,*prev=NULL;
	unsigned int lastvalue=0;

	children=large=small=0;
	used=freed=wasted=w40=0;

	//printf("large entries:\n");
	for (vc=vcl_first; vc; vc=vc->next) {
		//printf("surface %d, pos %d,%d, size %dx%d (%p,%p), child: %p, used=%d, age=%d, value=%d\n",vc->cs,vc->px,vc->py,vc->dx,vc->dy,vc->next,vc->prev,vc->child,vc->used,dd_tick-vc->tick,vc->value);
		if (vc->value!=vid_value(vc)) {
			printf("large: value is incorrect (%d)\n",nr);
			exit(1);
		}
		if (vc->value<lastvalue) {
			printf("large: chain is not ordered! %d %d (%d)\n",lastvalue,vc->value,nr);
			exit(1);
		}
		if (vc->prev!=prev) {
			printf("large: mismatch: prev!=prev (%d)\n",nr);
			exit(1);
		}
		if (vc->child) vid_show_child(vc->child,vc,nr);

		prev=vc;
		lastvalue=vc->value;

		large++;
	}
	if (prev!=vcl_last) {
		printf("large last not correctly set. (%d)\n",nr);
		exit(1);
	}

	//printf("small entries:\n");
	prev=NULL;
	for (vc=vcs_first; vc; vc=vc->next) {
		//printf("surface %d, pos %d,%d, size %dx%d (%p,%p), dad is at surface %d, pos %d,%d, size %dx%d\n",vc->cs,vc->px,vc->py,vc->dx,vc->dy,vc->next,vc->prev,vc->parent->cs,vc->parent->px,vc->parent->py,vc->parent->dx,vc->parent->dy);

		if (vc->prev!=prev) {
			printf("small: mismatch: prev!=prev (%d)\n",nr);
			exit(1);
		}

		if (vc->sidx!=SIDX_NONE) {
			printf("small: free sprite %p with sidx set?? (%d)\n",vc,nr);
			exit(1);
		}

		prev=vc;

		small++;
	}
	if (prev!=vcs_last) {
		printf("small last not correctly set (%d).\n",nr);
		exit(1);
	}

	note("dd_tick=%d (%dK %dK %dK)",dd_tick,dd_tick>>2,dd_tick<<2,dd_tick<<6);

	note("used=%d, large=%d, free=%d\n",children,large,small);
	note("Used %dK, Free %dK, Waste %dK, Waste2 %dK\n",used>>9,freed>>9,wasted>>9,w40>>9);
}

void vid_test(void)
{
	int n;

	for (n=0; n<200000; n++) {
		if (n%1000==0) printf("%d...\n",n);
		
		vid_make_tile(n,3,42,42,rrand(70)+10,rrand(70)+10);
	}

	vid_show(0);
}

void vid_describe(struct vid_cache *vc)
{
	struct vid_cache *vr,*vl;

	for (vr=vc_unused; vr; vr=vr->next) {
		if (vr==vc) {
			printf("vc is in unused list\n");
		}
	}

	for (vr=vcs_first; vr; vr=vr->next) {
		if (vr==vc) {
			printf("vc is in free list\n");
		}
	}

	for (vl=vcl_first; vl; vl=vl->next) {
		if (vl==vc) {
			printf("vc is in large cache\n");
			break;
		}
		for (vr=vl->child; vr; vr=vr->child) {
			if (vc==vr) {
				printf("vc has parent:\n");
				if (vr->parent!=vl) {
					printf(" but link is destroyed.\n");
				}
				vid_describe(vl);
			}
		}
	}
}

void vid_check_for_erasure(int sidx)
{
	struct vid_cache *vc,*vcc;
	
	printf("erase-check\n");
	for (vc=vcl_first; vc; vc=vc->next) {
		for (vcc=vc->child; vcc; vcc=vcc->child) {
			if (vcc->sidx==(unsigned)sidx) {
				printf("child not erased:\n");
				vid_describe(vcc);
				printf("---\n");
			}
			if (vcc->parent!=vc) {
				printf("parent wrong!\n");
			}
		}
	}
}

void vid_display(void)
{
	int m,n,x,y,diff,used;
	unsigned short *ptr;
	struct vid_cache *vc,*vcc;
	static int sur_y[DDCS_MAX];

	if ((ptr=dd_lock_surface(ddbs))==NULL) return;

	for (n=0,y=100; n<ddcs_used; n++) {
		sur_y[n]=y;
		y+=ddcs_tdy[n];
	}
	
	for (vc=vcl_first; vc; vc=vc->next) {
		for (vcc=vc->child,used=0; vcc; vcc=vcc->child) {
			if (vcc->sidx) {
                                used+=vcc->size;
			}
		}
		x=vc->px/TILESIZEDX+750;
		y=sur_y[vc->cs]+vc->py/TILESIZEDY;
		
		diff=100*used/(TILESIZEDX*TILESIZEDY);
		
		if (diff==0) dd_pixel_fast(x,y,rgb2scr[IRGB(16,0,0)],ptr);
		else if (diff<50) dd_pixel_fast(x,y,rgb2scr[IRGB(31,0,0)],ptr);
		else if (diff<75) dd_pixel_fast(x,y,rgb2scr[IRGB(24,24,0)],ptr);
		else if (diff<88) dd_pixel_fast(x,y,rgb2scr[IRGB(0,0,31)],ptr);
		else if (diff<101) dd_pixel_fast(x,y,rgb2scr[IRGB(0,31,0)],ptr);
		else dd_pixel_fast(x,y,rgb2scr[IRGB(31,31,31)],ptr);
		
	}

	for (vc=vcl_first; vc; vc=vc->next) {
                x=vc->px/TILESIZEDX+700;
		y=sur_y[vc->cs]+vc->py/TILESIZEDY;
		
		diff=sqrt((dd_tick-vc->tick));
		
                if (diff<30) dd_pixel_fast(x,y,rgb2scr[IRGB(31-diff,0,0)],ptr);
		else dd_pixel_fast(x,y,rgb2scr[IRGB(0,0,0)],ptr);		
		
	}

	for (vc=vcl_first; vc; vc=vc->next) {
                x=vc->px/TILESIZEDX+650;
		y=sur_y[vc->cs]+vc->py/TILESIZEDY;
		
		diff=sqrt(sqrt(vc->used*50));
		
                if (diff<30) dd_pixel_fast(x,y,rgb2scr[IRGB(0,diff,0)],ptr);
		else dd_pixel_fast(x,y,rgb2scr[IRGB(0,31,0)],ptr);		
		
	}

	dd_unlock_surface(ddbs);
}

int vid_display2(int nr)
{
	int err;
        RECT rc,rc2;

	if (nr>ddcs_used) return 0;

        rc.left=0;
        rc.top=0;
        rc.right= ddcs_xmax[nr-1];
        rc.bottom=ddcs_ymax[nr-1];

	rc2.left=x_offset;
	rc2.top=y_offset;
	rc2.right=rc2.left+min(800,ddcs_xmax[nr-1]);
	rc2.bottom=rc2.top+min(600,ddcs_ymax[nr-1]);

        if ((err=ddbs->lpVtbl->Blt(ddbs,&rc2,ddcs[nr-1],&rc,DDBLT_WAIT,0))!=DD_OK) dd_error("sys_blit()",err);

	return nr;
}
#endif
