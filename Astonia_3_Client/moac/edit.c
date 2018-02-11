#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#pragma hdrstop

#include "main.h"
#include "dd.h"
#define ISCLIENT
#include "client.h"
#include "gui.h"
#include "edit.h"
#include "sprite.h"
#include "resource.h"

#ifdef EDITOR

void db_create_pentwall(void);
void db_create_pentlava(void);
void db_create_penthalf(void);

// extern

extern int quit;
extern HINSTANCE instance;
extern HWND mainwnd;

extern HCURSOR c_only,c_take,c_drop,c_attack,c_raise,c_give,c_use,c_usewith,c_swap,c_sell,c_buy,c_look,c_set,c_spell,c_pix,c_say,c_junk,c_get;
HCURSOR ce_pen,ce_rect,ce_darrow,ce_darrow_0101,ce_pip,ce_sizeall,ce_uarrow,ce_uarrowp,ce_uarrowm;
extern HCURSOR cur_cursor;
extern int mousex,mousey,vk_shift,vk_control,vk_alt,vk_rbut,vk_lbut;
extern int mousedx,mousedy;
extern int vk_item,vk_char,vk_spell;
extern int mapsel;
extern int nocut;
extern int allcut;

//extern TIN *tin;
//extern int curtin;

extern unsigned int now;

extern QUICK *quick;
extern int maxquick;
extern void make_quick(int game);

extern void set_cmd_key_states(void);
extern void set_map_values(struct map *cmap, int attick);
extern void set_mapadd(int addx, int addy);
extern display_game_map(struct map *cmap);

// globals

int server_do=0;
int do_beep=0;
int resize_editor=0;
int show_overview=0;
int draw_underwater=1;

struct map *emap;
int emapdx,emapdy;

unsigned int edstarttime;
int editorloading;
MFILE *emf;
MFILE *cmf;
PFILE *pre;

#ifdef STAFFER
char zonepath[1024]={"c:/a3edit/"};  // set via parse cmd - zone path - requires '/' at the end
#else
char zonepath[1024]={"y:/v3/zones/"};  // set via parse cmd - zone path - requires '/' at the end
#endif
int  areaid=1;                                  // set via parse cmd - area id

EFLAG eflags[] = {
        { "S","MF_SIGHTBLOCK",MF_SIGHTBLOCK },  // 0
        { "M","MF_MOVEBLOCK",MF_MOVEBLOCK },    // 1
        { "I","MF_INDOORS",MF_INDOORS },        // 2
        { "R","MF_RESTAREA",MF_RESTAREA },      // 3
        { "D","MF_SOUNDBLOCK",MF_SOUNDBLOCK },  // 4
        { "T","MF_SHOUTBLOCK",MF_SHOUTBLOCK },  // 5
	{ "C","MF_CLAN",MF_CLAN },    		// 6
	{ "A","MF_ARENA",MF_ARENA },    	// 7
        { "P","MF_PEACE",MF_PEACE },       	// 8
	{ "N","MF_NEUTRAL",MF_NEUTRAL },       	// 9
	{ "F","MF_FIRETHRU",MF_FIRETHRU },     	// 10
	{ "H","MF_SLOWDEATH",MF_SLOWDEATH },   	// 11
	{ "L","MF_NOLIGHT",MF_NOLIGHT },   	// 12
	{ "G","MF_NOMAGIC",MF_NOMAGIC },   	// 13
	{ "U","MF_UNDERWATER",MF_UNDERWATER }, 	// 14
	{ "O","MF_NOREGEN",MF_NOREGEN },	// 15
	{ "2","MF_NOMAGIC",MF_RES2 }   		// 16
};

#define MAX_ICON        10

#define EBUT_MAP        0
#define EBUT_CMD        1
#define EBUT_CGS1       2
#define EBUT_CGS2       3
#define EBUT_CFS1       4
#define EBUT_CFS2       5
#define EBUT_CITM       6
#define EBUT_CCHR       7
#define EBUT_CFLG_BEG   8
#define EBUT_CFLG_END   24      // EBUT_CFLG_BEG+MAX_FLAG-1
#define EBUT_ICON_BEG   25
#define EBUT_ICON_END   34      // EBUT_ICON_BEG+MAX_ICON-1
#define EBUT_IMODE      35

#define MAX_EBUT        36

struct but ebut[MAX_EBUT];
extern int butsel;
int barup;              // butsel selection sub value
int emfsel=-1,newemfsel,rcemfsel=-1;
int oldmousex,oldmousey;
int ldragcmd,rdragcmd,ldrag,rdrag;
int ldragexec,rdragexec;
extern int lcmd,rcmd;
extern int capbut;

char cur_dmode[MAX_EMODE] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  1,1,1,1, 1,1}; // display mode analog to emode
char cur_emode[MAX_EMODE] ={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  1,0,1,0, 1,1};
char all_emode[MAX_EMODE] ={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  1,1,1,1, 1,1};
char it16_emode[MAX_EMODE]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  0,0,0,0, 0,0};
char icon_list[MAX_ICON]={ 14,0,8,15,4,16,0,0,0,0 };
int icontool=8;

int spritebrowse=0;             // 0=none 11=CGS1 (analog to ECMD_BROWSE...)
int spriteoffx,spriteoffy;

MFIELD cur_field,rub_field;
ITYPE *cur_itype;
int cur_iaffect=0;

#define ECMD_NONE               0
#define ECMD_GETCURFIELD        1
#define ECMD_SETCURFIELD        2
#define ECMD_CUREMODETOGGLE     3
#define ECMD_CUREMODEON         4
#define ECMD_CUREMODEOFF        5
#define ECMD_CUREFLAGTOGGLE     6
#define ECMD_CUREFLAGON         7
#define ECMD_CUREFLAGOFF        8
#define ECMD_DRAGMAP            9
#define ECMD_SAVEMAP            10
#define ECMD_BROWSE_CGS1        11
#define ECMD_BROWSE_CGS2        12
#define ECMD_BROWSE_CFS1        13
#define ECMD_BROWSE_CFS2        14
#define ECMD_BROWSE_CITM        15
#define ECMD_BROWSE_CCHR        16
#define ECMD_DRAG_BROWSE        17
#define ECMD_SETCURRECT         18
#define ECMD_SETICONTOOL        19
#define ECMD_PASTE              20
#define ECMD_SETINTFIELD        21
#define ECMD_SETINTRECT         22
#define ECMD_GETINTFIELD        23
#define ECMD_BROWSE_IMODE       24
#define ECMD_CURDMODETOGGLE     25
#define ECMD_SETCURIAFFECT      26

// -------- db -----------

void display_trans_sprite(int sprite,int x,int y,int light,int align)
{
        unsigned short c1,c2,c3,shine;
	unsigned char scale,cr,cg,cb,clight,sat;
	DDFX fx;

	bzero(&fx,sizeof(fx));

	sprite=trans_asprite(15000,sprite,tick,&scale,&cr,&cg,&cb,&clight,&sat,&c1,&c2,&c3,&shine);
	fx.sprite=sprite;
	fx.shine=shine;
	fx.c1=c1;
	fx.c2=c2;
	fx.c3=c3;
	fx.cr=cr;
	fx.cr=cr;
	fx.cg=cg;
	fx.cb=cb;
	fx.clight=clight;
	fx.sat=sat;
	fx.scale=scale;
	fx.sink=0;
	fx.align=align;
	fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=light;
        dd_copysprite_fx(&fx,x,y);
}


// -------- end db -------

// button

void set_ebut(int bidx, int dx, int dy, int id, int val, int flags)
{
        PARANOIA( if (bidx<0 || bidx>=MAX_EBUT) paranoia("set_ebut: ill bidx=%d",bidx); )

        bzero(&ebut[bidx],sizeof(BUT));

        ebut[bidx].flags=flags;

        ebut[bidx].id=id;
        ebut[bidx].val=val;
        ebut[bidx].dx=dx;
        ebut[bidx].dy=dy;
}

// file

static void scan_editor_files(void)
{
        char searchpath[1024];
        int i;

        sprintf(searchpath,"%s%d/",zonepath,areaid);
        find_itm_files(searchpath);
        find_chr_files(searchpath);
        find_map_files(searchpath);
        // find_int_files(searchpath);
        find_pre_files(searchpath);

        sprintf(searchpath,"%sgeneric/",zonepath);
        find_itm_files(searchpath);
        find_chr_files(searchpath);
        find_map_files(searchpath);
        // find_int_files(searchpath);
        find_pre_files(searchpath);

        editorloading=1;
}

static void load_editor_files(void)
{
        int i,next=1;

        if (editorloading==0) return;

        for (i=0; i<ifilecount; i++,next++) if (editorloading==next) { load_itm_file(ifilelist[i]); editorloading++; return; }
        for (i=0; i<cfilecount; i++,next++) if (editorloading==next) { load_chr_file(cfilelist[i]); editorloading++; return; }
        for (i=0; i<mfilecount; i++,next++) if (editorloading==next) { load_map_file(mfilelist[i]); editorloading++; return; }
        // for (i=0; i<nfilecount; i++,next++) if (editorloading==next) { load_int_file(nfilelist[i]); editorloading++; return; }
        for (i=0; i<pfilecount; i++,next++) if (editorloading==next) { load_pre_file(pfilelist[i]); editorloading++; return; }

        editorloading=0;
}

// dx

void dx_metal_rect(int x1, int y1, int x2, int y2, int light)
{
        int x,y;

        if (x2<=x1 || y2<=y1) return;

        dd_push_clip();
        dd_more_clip(x1,y1,x2,y2);

        for (y=(y1/200)*200; y<y2; y+=200) {
                for (x=(x1/200)*200; x<x2; x+=200) {
                        dd_copysprite(40,x,y,light,DD_NORMAL);
                }
        }

        dd_pop_clip();
}

// 0=up
// 1=down
void dx_metal_frame(int x1, int y1, int x2, int y2, int style)
{
        if (style==0) {
                dx_metal_rect(x1+1,y1+1,x2-1,y2-1,DDFX_NLIGHT);
                dx_metal_rect(x1,y1,x1+1,y2,DDFX_BRIGHT);
                dx_metal_rect(x1,y1,x2,y1+1,DDFX_BRIGHT);
                dx_metal_rect(x2-1,y1,x2,y2,2);
                dx_metal_rect(x1,y2-1,x2,y2,2);
        }
        else if (style==1) {
                dx_metal_rect(x1+1,y1+1,x2-1,y2-1,DDFX_NLIGHT);
                dx_metal_rect(x2-1,y1,x2,y2,DDFX_BRIGHT);
                dx_metal_rect(x1,y2-1,x2,y2,DDFX_BRIGHT);
                dx_metal_rect(x1,y1,x1+1,y2,2);
                dx_metal_rect(x1,y1,x2,y1+1,2);
        }


}

void dx_ciframe(int x1, int y1, int x2, int y2, int ci, int greenci)
{
        unsigned short int c1,c2;

        if (ci==1) { c2=darkgraycolor; c1=whitecolor; }
        else if (ci==2) { c1=darkgraycolor; c2=whitecolor; }

        if (ci) {
                dx_metal_rect(x1,y1,x2,y1+1,DDFX_NLIGHT);
                dd_rect(x1,y1,x1+1,y2,c1);
                dd_rect(x2-1,y1,x2,y2,c2);
                dd_rect(x1,y2-1,x2,y2,c2);
        }

        if (greenci) {
                x1--;
                y1--;
                x2++;
                y2++;
                if (greenci==1) dd_rect(x1,y1,x2,y1+1,lightgreencolor);
                dd_rect(x1,y1+1,x1+1,y2,lightgreencolor);
                dd_rect(x2-1,y1+1,x2,y2,lightgreencolor);
                dd_rect(x1,y2-1,x2,y2,lightgreencolor);
        }
}

/*
void dx_copysprite_icon(int icon, int scrx, int scry, int ci, int greenci)
{
        DDFX ddfx;

        bzero(&ddfx,sizeof(ddfx));
        ddfx.sprite=41;
        ddfx.align=DD_CENTER;
        ddfx.ml=ddfx.ll=ddfx.rl=ddfx.ul=ddfx.dl=DDFX_NLIGHT;

        ddfx.clipsx=icon*22+1;
        ddfx.clipex=ddfx.clipsx+21-2;
        ddfx.clipsy=-10;
        ddfx.clipey=ddfx.clipsy+21-2;

        dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx+1+0,scry+11+0);
        // if (ci==0) dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx+1+0,scry+11+0);
        // else if (ci==1) dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx+1-1,scry+11-1);
        // else if (ci==2) dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx+1+1,scry+11+1);

        ddfx.clipsx=ci*22;
        ddfx.clipex=ddfx.clipsx+21;
        ddfx.clipsy=-11;
        ddfx.clipey=ddfx.clipsy+21;
        dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx,scry+11+0);

        if (greenci) {
                int x1,y1,x2,y2;

                x1=scrx-1;
                y1=scry-1;
                x2=x1+23;
                y2=y1+23;

                if (greenci==1) dd_rect(x1,y1,x2,y1+1,lightgreencolor);
                dd_rect(x1,y1+1,x1+1,y2,lightgreencolor);
                dd_rect(x2-1,y1+1,x2,y2,lightgreencolor);
                dd_rect(x1,y2-1,x2,y2,lightgreencolor);
        }
}
*/
void dx_copysprite_icon(int icon, int scrx, int scry)
{
        DDFX ddfx;

        bzero(&ddfx,sizeof(ddfx));
        ddfx.sprite=41;
        ddfx.align=DD_CENTER;
        ddfx.ml=ddfx.ll=ddfx.rl=ddfx.ul=ddfx.dl=DDFX_NLIGHT;

        ddfx.clipsx=icon*22+1;
        ddfx.clipex=ddfx.clipsx+21-2;
        ddfx.clipsy=-10;
        ddfx.clipey=ddfx.clipsy+21-2;
	ddfx.scale=100;

        dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx+1+0,scry+11+0);
}


// browse
static void editor_display_browse_int(int offx, int offy, ITYPE *itype, int light)
{
        int x,y,sprite;

        if (!itype) { dd_drawtext(offx,offy+12,whitecolor,DD_CENTER|DD_SMALL,"none"); return; }
        switch (itype->generic.type) {
                case ITYPE_GROUND:
                        for (y=0; y<3; y++) {
                                for (x=0; x<3; x++) {
                                        display_trans_sprite(itype->ground.sprite[(x%itype->ground.gridx)+(y%itype->ground.gridy)*itype->ground.gridx],offx+(x-y)*FDX/2,offy+(x+y)*FDY/2-30,light,DD_OFFSET);
                                }
                        }
                        break;
                case ITYPE_LINEWALL:
                        for (x=0,y=0; y<3; y++) display_trans_sprite(itype->linewall.sprite[(y%itype->linewall.len)],offx+(x-y)*FDX/2,offy+(x+y)*FDY/2,light,DD_OFFSET);
                        for (y=0,x=1; x<3; x++) display_trans_sprite(itype->linewall.sprite[(x%itype->linewall.len)],offx+(x-y)*FDX/2,offy+(x+y)*FDY/2,light,DD_OFFSET);
                        break;
                case ITYPE_CHANCE:
                        for (y=0; y<3; y++) {
                                for (x=0; x<3; x++) {
                                        display_trans_sprite(itype->chance.sprite[((x+y*3)%itype->chance.len)],offx+(x-y)*FDX/2,offy+(x+y)*FDY/2-30,light,DD_OFFSET);
                                }
                        }
                        break;
		case ITYPE_PRESET:
                        if (itype->preset.emode[EMODE_SPRITE+0]) display_trans_sprite(itype->preset.field.sprite[0],offx,offy,DDFX_NLIGHT,DD_OFFSET);
                        if (itype->preset.emode[EMODE_SPRITE+1] && itype->preset.field.sprite[1]) display_trans_sprite(itype->preset.field.sprite[1],offx,offy,DDFX_NLIGHT,DD_OFFSET);
                        if (itype->preset.emode[EMODE_SPRITE+2] && itype->preset.field.sprite[2]) display_trans_sprite(itype->preset.field.sprite[2],offx,offy,DDFX_NLIGHT,DD_OFFSET);
                        if (itype->preset.emode[EMODE_SPRITE+3] && itype->preset.field.sprite[3]) display_trans_sprite(itype->preset.field.sprite[3],offx,offy,DDFX_NLIGHT,DD_OFFSET);
                        if (itype->preset.emode[EMODE_ITM] && itype->preset.field.itm) display_trans_sprite(itype->preset.field.itm->sprite,offx,offy,DDFX_NLIGHT,DD_OFFSET);
                        if (itype->preset.emode[EMODE_CHR] && itype->preset.field.chr) {
                                sprite=get_player_sprite(itype->preset.field.chr->sprite,1,0,0,0);
                                dd_copysprite(sprite,offx,offy,DDFX_NLIGHT,DD_OFFSET);
                        }
                        break;

        }

        dd_drawtext_fmt(offx,offy+12,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,"%s %s",get_iname(itype->generic.type),itype->generic.name);
}

static void editor_display_browse(void)
{
        int x,y,scrx,scry,dx,dy,sprite,pnum,xx,yy;
        int selx,sely;
        POINT p;
        int newx,newy;
        char txta[256],txtb[256],*fname;
        int ww,hh;
        // PRESET *preset;

        if (spriteoffx<0) spriteoffx=0;
        if (spriteoffy<0) spriteoffy=0;
        if (spritebrowse!=ECMD_BROWSE_CCHR && spritebrowse!=ECMD_BROWSE_CITM) spriteoffx=0;

        if (spritebrowse==ECMD_BROWSE_IMODE) { ww=120; hh=100; } else { ww=60; hh=100; }

        dx=winxres/ww;
        dy=winyres/hh;
        selx=(mousex+(spriteoffx%ww))/ww;
        sely=(mousey+(spriteoffy%hh))/hh;

        if (spritebrowse==ECMD_BROWSE_CCHR || spritebrowse==ECMD_BROWSE_CITM) { dx++; }

        dx_metal_rect(0,0,winxres,winyres,DDFX_NLIGHT);

        for (y=-1; y<=dy; y++) {
                fname=NULL;
                for (x=-1; x<dx; x++) {

                        scrx=x*ww-(spriteoffx%ww);
                        scry=y*hh-(spriteoffy%hh);

                        if (x==selx && y==sely) {
                                dx_metal_rect(scrx,scry,scrx+ww,scry+hh,10);
                        }

                        if (spritebrowse==ECMD_BROWSE_CCHR) {
                                yy=(y+spriteoffy/hh);
                                xx=(x+spriteoffx/ww);
                                if (yy>=0 && yy<cfilecount && xx>=0 && xx<cfilelist[yy]->ccount) {
                                        if (selx==x && sely==y) sprite=get_player_sprite(cfilelist[yy]->clist[xx]->sprite,(tick/(TPS/2))%8,1,tick%(TPS/2),(TPS/2));
                                        else sprite=get_player_sprite(cfilelist[yy]->clist[xx]->sprite,1,0,0,0);

                                        sprintf(txta,"%s",cfilelist[yy]->clist[xx]->uni);
                                        sprintf(txtb,"%s",txta+strlen(txta)/2);
                                        txta[strlen(txta)/2]=0;
                                        fname=cfilelist[yy]->filename;
                                }
                                else continue;
                        }
                        else if (spritebrowse==ECMD_BROWSE_CITM) {
                                yy=(y+spriteoffy/hh);
                                xx=(x+spriteoffx/ww);
                                if (yy>=0 && yy<ifilecount && xx>=0 && xx<ifilelist[yy]->icount) {

					sprite=ifilelist[yy]->ilist[xx]->sprite;
					//sprite=trans_asprite(0,sprite,0,NULL,NULL,NULL,NULL,NULL);

                                        sprintf(txta,"%s",ifilelist[yy]->ilist[xx]->uni);
                                        sprintf(txtb,"%s",txta+strlen(txta)/2);
                                        txta[strlen(txta)/2]=0;
                                        fname=ifilelist[yy]->filename;
                                }
                                else continue;
                        }
                        else if (spritebrowse==ECMD_BROWSE_IMODE) {
                                pnum=x+(y+spriteoffy/hh)*dx;
                                if (pre && pnum>=0 && pnum<pre->pcount) {
                                        dd_push_clip();
                                        dd_more_clip(scrx,scry,scrx+ww,scry+hh);
                                        editor_display_browse_int(scrx+ww/2,scry+hh-20,pre->plist[pnum],DDFX_NLIGHT);
                                        dd_pop_clip();
                                }
                                /*
                                yy=(y+spriteoffy/hh);
                                xx=(x+spriteoffx/ww);
                                if (yy>=0 && yy<pfilecount && xx>=0 && xx<pfilelist[yy]->pcount) {
                                        dd_push_clip();
                                        dd_more_clip(scrx,scry,scrx+ww,scry+hh);
                                        editor_display_browse_int(scrx+ww/2,scry+hh-20,pfilelist[yy]->plist[xx],DDFX_NLIGHT);
                                        dd_pop_clip();
                                        // fname=pfilelist[yy]->filename;
                                }
                                */
                                continue;
                        }
                        else {
                                sprite=x+(y+spriteoffy/hh)*dx;
                                *txta=0;
                                sprintf(txtb,"%d",sprite);
                        }

                        if (sprite<0) continue;

                        dd_push_clip();
                        dd_more_clip(scrx,scry,scrx+ww,scry+hh);
                        //display_trans_sprite(sprite,scrx+30,scry+80,DDFX_NLIGHT,DD_OFFSET);
			display_trans_sprite(sprite,scrx+30,scry+80,DDFX_NLIGHT,DD_OFFSET);
                        dd_drawtext(scrx+30,scry+90-8,whitecolor,DD_SMALL|DD_FRAME|DD_CENTER,txta);
                        dd_drawtext(scrx+30,scry+90-0,whitecolor,DD_SMALL|DD_FRAME|DD_CENTER,txtb);
                        dd_pop_clip();
                }

                if (fname) dd_drawtext(4,y*hh-(spriteoffy%hh)+4,whitecolor,DD_SMALL|DD_FRAME|DD_LEFT,fname);

        }

        /*tin[TIN_TEXT].sx=6;
        tin[TIN_TEXT].sy=winyres-10;
        tin[TIN_TEXT].color=orangecolor;
        display_tin(TIN_TEXT,DD_FRAME);*/

        if (spritebrowse==ECMD_BROWSE_IMODE && pre) dd_drawtext(4,4,whitecolor,DD_SMALL|DD_FRAME|DD_LEFT,pre->filename);
}

static void editor_start_sprite_browse(int target)
{
        int x,y,sprite;

        spritebrowse=target;

        if (spritebrowse==ECMD_BROWSE_CCHR) {
                for (y=0; y<cfilecount; y++) {
                        for (x=0; x<cfilelist[y]->ccount; x++) {
                                if (cfilelist[y]->clist[x]==cur_field.chr) break;
                        }
                        if (x<cfilelist[y]->ccount) break;
                }
                if (y==cfilecount) {
                        spriteoffx=spriteoffy=0;
                }
                else {
                        spriteoffx=x*60;
                        spriteoffy=y*100;
                }
        }
        else if (spritebrowse==ECMD_BROWSE_CITM) {
                for (y=0; y<ifilecount; y++) {
                        for (x=0; x<ifilelist[y]->icount; x++) {
                                if (ifilelist[y]->ilist[x]==cur_field.itm) break;
                        }
                        if (x<ifilelist[y]->icount) break;
                }
                if (y==ifilecount) {
                        spriteoffx=spriteoffy=0;
                }
                else {
                        spriteoffx=x*60;
                        spriteoffy=y*100;
                }
        }
        else if (spritebrowse==ECMD_BROWSE_IMODE) {
                for (y=0; y<pfilecount; y++) {
                        for (x=0; x<pfilelist[y]->pcount; x++) {
                                if (pfilelist[y]->plist[x]==cur_itype) break;
                        }
                        if (x<pfilelist[y]->pcount) break;
                }
                if (y==pfilecount) {
                        spriteoffx=spriteoffy=0;
                }
                else {
                        pre=pfilelist[y];
                        spriteoffy=(x/(winxres/120))*100;
                }
        }
        else {
                if (spritebrowse==ECMD_BROWSE_CGS1) sprite=cur_field.sprite[0];
                if (spritebrowse==ECMD_BROWSE_CGS2) sprite=cur_field.sprite[1];
                if (spritebrowse==ECMD_BROWSE_CFS1) sprite=cur_field.sprite[2];
                if (spritebrowse==ECMD_BROWSE_CFS2) sprite=cur_field.sprite[3];

                spriteoffx=0;
                spriteoffy=(sprite/(winxres/60))*100;
        }
}

static void editor_end_sprite_browse(int ok)
{
        int selx,sely,sprite,pnum,i;
        // PRESET *preset;

        if (!ok) { spritebrowse=0; return; }

        if (spritebrowse==ECMD_BROWSE_CCHR) {
                selx=(mousex+spriteoffx)/60;
                sely=(mousey+spriteoffy)/100;
                if (sely>=0 && sely<cfilecount && selx>=0 && selx<cfilelist[sely]->ccount) cur_field.chr=cfilelist[sely]->clist[selx];
                else cur_field.chr=NULL;
        }
        else if (spritebrowse==ECMD_BROWSE_CITM) {
                selx=(mousex+spriteoffx)/60;
                sely=(mousey+spriteoffy)/100;
                if (sely>=0 && sely<ifilecount && selx>=0 && selx<ifilelist[sely]->icount) cur_field.itm=ifilelist[sely]->ilist[selx];
                else cur_field.itm=NULL;
        }
        else if (spritebrowse==ECMD_BROWSE_IMODE) {
                selx=mousex/120;
                sely=(mousey+(spriteoffy%100))/100;
                if (selx<0 || selx>=winxres/120) { spritebrowse=0; return; }
                pnum=selx+(sely+spriteoffy/100)*(winxres/120);

                if (pre && pnum>=0 && pnum<pre->pcount) cur_itype=pre->plist[pnum]; else cur_itype=NULL;

                /*
                selx=(mousex+spriteoffx)/120;
                sely=(mousey+spriteoffy)/100;
                if (sely>=0 && sely<=pfilecount && selx>=0 && selx<pfilelist[sely]->pcount) cur_itype=pfilelist[sely]->plist[selx];
                else cur_itype=NULL;
                */

                if (cur_itype && cur_itype->generic.type!=ITYPE_PRESET) {
                        for (i=0; i<MAX_EMODE; i++) cur_emode[i]=cur_itype->preset.emode[i];
                        cur_field.flags=cur_itype->generic.flags;
                        cur_iaffect=cur_itype->generic.affect;
                        icontool=16;
                }
                else if (cur_itype && cur_itype->generic.type==ITYPE_PRESET) {
                        for (i=0; i<MAX_EMODE; i++) cur_emode[i]=cur_itype->preset.emode[i];
                        memcpy(&cur_field,&cur_itype->preset.field,sizeof(MFIELD));
                        icontool=8;
                }
        }
        else {
                selx=mousex/60;
                if (selx<0 || selx>=winxres/60) { spritebrowse=0; return; }
                sely=(mousey+(spriteoffy%100))/100;
                sprite=selx+(sely+spriteoffy/100)*(winxres/60);
                if (sprite<0) { spritebrowse=0; return; }
                if (sprite>0xFFFF) { spritebrowse=0; return; }

                if (spritebrowse==ECMD_BROWSE_CGS1) cur_field.sprite[0]=sprite;
                if (spritebrowse==ECMD_BROWSE_CGS2) cur_field.sprite[1]=sprite;
                if (spritebrowse==ECMD_BROWSE_CFS1) cur_field.sprite[2]=sprite;
                if (spritebrowse==ECMD_BROWSE_CFS2) cur_field.sprite[3]=sprite;
        }

        spritebrowse=0;

}

// display

static void editor_display_overview(void)
{
        int i;
        int x,y,mapx,mapy,scrx,scry,aux;
        MFIELD *field;
        unsigned short int *ptr;
        unsigned short int color;

        ptr=dd_lock_ptr();
        if (!ptr) return;

        for (mapy=0; mapy<emf->height; mapy++) {
                for (mapx=0; mapx<emf->width; mapx++) {

                        x=mapx-emf->width/2;//-emf->offsetx/1024;
                        y=mapy-emf->height/2;//-emf->offsety/1024;
                        scrx=winxres/2+x-y;
                        scry=winyres/2+x+y;

                        field=&emf->field[mapx+mapy*emf->width];

                        if (scrx<0 || scry<0 || scrx>=winxres || scry>=winyres) continue;

			aux=(field->sprite[0]/8)%8;

                        // todo - this thing could be nicer
                        if (field->flags&MF_SIGHTBLOCK) color=IRGB(31,31,31-aux);
                        else if (field->flags&MF_MOVEBLOCK) color=IRGB(26-aux,aux,aux);
                        else if (field->sprite[2] || field->sprite[3]) color=IRGB(aux,31-aux,aux);
                        else if (field->sprite[0] || field->sprite[1]) color=IRGB(aux,16-aux,aux);
                        else color=IRGB(0,0,0);

                        color=rgb2scr[color];

                        ptr[scrx+scry*xres]=color;
                        if (++scrx>=winxres) continue;
                        ptr[scrx+scry*xres]=color;
                }
        }

        dd_unlock_ptr();
}

/*static void editor_display_text(void)
{
        int i,x,y,line,nr;
        unsigned short int color;
        int show;

        if (*tin[TIN_TEXT].buf) show=1; else show=0;

        x=6;
        y=ebut[EBUT_MAP].y+ebut[EBUT_MAP].dy-2-(MAXLINESHOW+show)*LINEHEIGHT;

        for (i=0; i<MAXLINESHOW; i++) {

                line=(MAXLINE+topline+i)%MAXLINE;
                if ((int)(now-linetime[line])/50>31 && !show) continue;

                color=rgb2scr[irgb_blend(scr2rgb[whitecolor],scr2rgb[orangecolor],min(31,max(0,(int)(now-linetime[line])/50)))];
                dd_drawtext(x,y+i*LINEHEIGHT,color,DD_LEFT|DD_LARGE|DD_FRAME,linebuf[line]);
        }

        if (show) {
                tin[TIN_TEXT].sx=6;
                tin[TIN_TEXT].sy=ebut[EBUT_MAP].y+ebut[EBUT_MAP].dy-2-(1)*LINEHEIGHT;
                tin[TIN_TEXT].color=orangecolor;
                display_tin(TIN_TEXT,DD_FRAME);
        }
}*/

static void editor_display_current_sprite(int b, MFIELD *field, char *emode)
{
        int sx,sy,ex,ey,cx;
        int sprite;
        char *text,spritetxt[64],*name,*end,tmp;
        int mode,dmode;

        switch (b) {
                case EBUT_CGS1: dmode=cur_dmode[EMODE_SPRITE+0]; mode=emode[EMODE_SPRITE+0]; name="GS1"; sprite=field->sprite[0]; text=spritetxt; if (sprite || b==EBUT_CGS1) sprintf(spritetxt,"%d",sprite); else strcpy(spritetxt,"none"); break;
                case EBUT_CGS2: dmode=cur_dmode[EMODE_SPRITE+1]; mode=emode[EMODE_SPRITE+1]; name="GS2"; sprite=field->sprite[1]; text=spritetxt; if (sprite) sprintf(spritetxt,"%d",sprite); else strcpy(spritetxt,"none"); break;
                case EBUT_CFS1: dmode=cur_dmode[EMODE_SPRITE+2]; mode=emode[EMODE_SPRITE+2]; name="FS1"; sprite=field->sprite[2]; text=spritetxt; if (sprite) sprintf(spritetxt,"%d",sprite); else strcpy(spritetxt,"none"); break;
                case EBUT_CFS2: dmode=cur_dmode[EMODE_SPRITE+3]; mode=emode[EMODE_SPRITE+3]; name="FS2"; sprite=field->sprite[3]; text=spritetxt; if (sprite) sprintf(spritetxt,"%d",sprite); else strcpy(spritetxt,"none"); break;
                case EBUT_CITM: dmode=cur_dmode[EMODE_ITM]; mode=emode[EMODE_ITM]; name="ITM"; if (field->itm) { sprite=field->itm->sprite; text=field->itm->uni; } else { sprite=0; text="no itm"; } break;
                case EBUT_CCHR: dmode=cur_dmode[EMODE_CHR]; mode=emode[EMODE_CHR]; name="CHR"; if (field->chr) { sprite=get_player_sprite(field->chr->sprite,2,0,0,0); text=field->chr->uni; } else { sprite=0; text="no chr"; } break;
                default: paranoia("...display_current: ill b=%d",b); return;
        }

        if (icontool==16) {
                switch (b) {
                        case EBUT_CGS1: if (cur_iaffect==0) { mode=1; sprite=58; } else { sprite=5; } text=name; break;
                        case EBUT_CGS2: if (cur_iaffect==1) { mode=1; sprite=58; } else { sprite=5; } text=name; break;
                        case EBUT_CFS1: if (cur_iaffect==2) { mode=1; sprite=58; } else { sprite=5; } text=name; break;
                        case EBUT_CFS2: if (cur_iaffect==3) { mode=1; sprite=58; } else { sprite=5; } text=name; break;
                        case EBUT_CITM: if (cur_iaffect==4) { mode=1; sprite=58; } else { sprite=5; } text=name; break;
                        case EBUT_CCHR: if (cur_iaffect==5) { mode=1; sprite=58; } else { sprite=5; } text=name; break;
                }
        }

        sx=ebut[b].x;
        sy=ebut[b].y;
        ex=ebut[b].x+ebut[b].dx;
        ey=ebut[b].y+ebut[b].dy;
        cx=sx+(ex-sx+1)/2;

        if (mode) {
                dx_ciframe(sx,sy,ex,ey,2,0);
                // if (icontool!=16)
                dx_metal_rect(sx+1,sy,ex-1,ey-1,10);

                if (sprite || b==EBUT_CGS1) display_trans_sprite(sprite,cx,sy+12,DDFX_NLIGHT,DD_OFFSET);	// bright!!

                if (dd_textlength(DD_SMALL,text)<ebut[b].dx ) {
                        dd_drawtext(cx,ey-8,whitecolor,DD_SMALL|DD_CENTER,text);
                }
                else {
                        end=text+strlen(text)/2; // while (end-1>text && dd_textlength(DD_SMALL,end-1)<ebut[b].dx) end--;
                        tmp=*end; *end=0; dd_drawtext(cx,ey-8-6,whitecolor,DD_SMALL|DD_CENTER,text); *end=tmp;
                        dd_drawtext(cx,ey-8,whitecolor,DD_SMALL|DD_CENTER,end);
                }
        }
        else {
                dd_drawtext(cx,sy+14,whitecolor,DD_SMALL|DD_CENTER,name);
        }

        if (dmode) dx_ciframe(sx,ey+3,ex,ey+5,1,0);
        else dx_ciframe(sx,ey+3,ex,ey+5,2,0);
}

static void editor_display_current_flag(int b, MFIELD *field, char *emode)
{
        int sx,sy,ex,ey,cx,addy;
        int f;
        unsigned short int color,ci;

        f=ebut[b].val;

        sx=ebut[b].x;
        sy=ebut[b].y;
        ex=ebut[b].x+ebut[b].dx;
        ey=ebut[b].y+ebut[b].dy;
        cx=sx+(ex-sx+1)/2;

        if (!emode[EMODE_FLAG+f]) {
                dd_drawtext(cx,sy+14,whitecolor,DD_SMALL|DD_CENTER,eflags[f].letter);
        }
        else if (field->flags&eflags[f].mask) {
                dx_ciframe(sx,sy,ex,ey,2,0);
                dx_metal_rect(sx+1,sy,ex-1,ey-1,10);
                dd_drawtext(cx,sy+4,whitecolor,DD_SMALL|DD_CENTER,eflags[f].letter);
        }
        else {
                dx_ciframe(sx,sy,ex,ey,2,0);
                dx_metal_rect(sx+1,sy,ex-1,ey-1,10);
                dd_drawtext(cx,ey-8,whitecolor,DD_SMALL|DD_CENTER,eflags[f].letter);
        }

        if (cur_dmode[EMODE_FLAG+f]) dx_ciframe(sx,ey+3,ex,ey+5,1,0);
        else dx_ciframe(sx,ey+3,ex,ey+5,2,0);
}

static void editor_display_current(void)
{
        int i;
        char *emode;
        MFIELD *field;

        if (lcmd==ECMD_GETCURFIELD) {
                emode=all_emode;
                field=&emf->field[emfsel];
        }
        else if (lcmd==ECMD_GETINTFIELD) {
                emode=it16_emode;
                field=&emf->field[emfsel];
        }
        else {
                if (icontool==15) {
                        emode=cur_emode;
                        field=&rub_field;
                }
                /*
                else if (icontool==16) {
                        emode=cur_emode;
                        field=&cur_field;
                }
                */
                else {
                        emode=cur_emode;
                        field=&cur_field;
                }
        }
        editor_display_current_sprite(EBUT_CGS1,field,emode);
        editor_display_current_sprite(EBUT_CGS2,field,emode);
        editor_display_current_sprite(EBUT_CFS1,field,emode);
        editor_display_current_sprite(EBUT_CFS2,field,emode);
        editor_display_current_sprite(EBUT_CITM,field,emode);
        editor_display_current_sprite(EBUT_CCHR,field,emode);
        for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) editor_display_current_flag(i,field,emode);
}

static void editor_display_game_strings(void)
{
        int i,mn,scrx,scry,x,y,f;
        char buf[256],*run;

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];
                scrx=mapaddx+quick[i].cx;
                scry=mapaddy+quick[i].cy;

                // flags
                if (emap[mn].ex_map_flags) {
                        for (run=buf,f=0; f<MAX_EFLAG; f++) if (cur_dmode[EMODE_FLAG+f] && (emap[mn].ex_map_flags&eflags[f].mask)) *run++=*eflags[f].letter;
                        *run=0;
                        if (*buf) dd_drawtext(scrx,scry,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,buf);
                }

		// section display!!!!!!
		//dd_drawtext_fmt(scrx,scry,whitecolor,DD_LEFT|DD_SMALL|DD_FRAME,"%d",emap[mn].sec_nr);

                // rest
                if (!emap[mn].rlight) continue;
                if (!emap[mn].csprite) continue;

                x=scrx+emap[mn].xadd;
                y=scry+4+emap[mn].yadd+get_chr_height(emap[mn].csprite)-25;

                dd_drawtext(x,y,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,emap[mn].ex_chrname);
        }
}

static void editor_display_icons(void)
{
        int i,b;

        for (i=0; i<MAX_ICON; i++) {
                if (icon_list[i]<=0) continue;
                b=EBUT_ICON_BEG+i;
                dx_copysprite_icon(icon_list[i],ebut[b].x,ebut[b].y+2);
                if (icontool==icon_list[i]) {
                        dx_ciframe(ebut[b].x,ebut[b].y,ebut[b].x+ebut[b].dx,ebut[b].y+ebut[b].dy,2,0);
                }
        }
}

static void editor_do_getintfield(void);

static void editor_display_i(void)
{
        int b,sx,sy,ex,ey,tmp_iaffect;
        ITYPE *tmp_itype;

        b=EBUT_IMODE;

        sx=ebut[b].x;
        sy=ebut[b].y;
        ex=ebut[b].x+ebut[b].dx;
        ey=ebut[b].y+ebut[b].dy;
        if (icontool==16 || icontool!=16) {
                dx_metal_rect(sx+1,sy,ex-1,ey-1,10);
                dx_ciframe(sx,sy,ex,ey,2,0);
                tmp_itype=cur_itype;
                if (lcmd==ECMD_GETINTFIELD) editor_do_getintfield();
                dd_push_clip();
                dd_more_clip(sx+1,0,ex-1,ey-1);
                editor_display_browse_int(sx+(ex-sx)/2,ey-20,cur_itype,DDFX_NLIGHT);	// bright!!
                dd_pop_clip();
                cur_itype=tmp_itype;
        }
        else {
                dd_drawtext(sx+(ex-sx)/2,sy+14,whitecolor,DD_SMALL|DD_CENTER,"ITYPE");
        }
}

//-----------
struct sector
{
	int fx,fy;
	int tx,ty;
	int section;
};
static struct sector sec[]={
	{  1,  1, 85, 70,  4},
	{123,  2,142, 57,  15},
	{127, 58,142,102,  15},
	{118,102,142,124,  15},
	{118,124,128,183,  16},
	{ 95,125,118,183,  16},
	{ 84,137, 84,183,  16},
	{ 59,148, 84,183,  16},
	{ 39,141, 59,183,  16},
	{ 18,124, 38,183,  16},
	{  2,112, 18,183,  16},
	{ 85,  1,122, 62,  17},
	{122, 58,126, 62,  17},
	{  1, 70, 14, 84,  18},
	{  1, 84,  7,100,  18},
	{ 15, 69, 47, 88,  18},
	{ 48, 74, 64, 74,  18},
	{ 58, 70, 90, 97,  18},
	{ 64, 98, 85,101,  18},
	{ 90, 63,121, 82,  18},
	{ 96, 83,101, 90,  18},
	{  3,101,  7,110,  19},
	{  8, 86, 12, 90,  19},
	{  8, 90, 19,111,  19},
	{ 20, 90, 39,123,  19},
	{ 40, 92, 52,140,  19},
	{ 53, 97, 60,140,  19},
	{ 60, 97, 60,148,  19},
	{ 64,102, 83,147,  19},
	{ 84,124, 94,136,  19},
	{ 84,113,117,113,  19},
	{ 87,101,117,112,  19},
	{ 91, 83, 96,100,  19},
	{ 97, 90,102,100,  19},
	{103, 95,127,100,  19},
	{103, 85,115, 94,  19},
	{116, 83,126, 89,  19},
	{122, 83,126, 63,  19},
	{143,  1,192, 85,  21},
	{143, 85,192,170,  22},
	{143,170,192,254,  23},
	{  1,184,142,254,  24},
	{194,208,254,254,  25},
	{214,197,254,207,  25},
	{194,197,212,207,  26},
	{194,153,254,197,  26},
	{214,116,254,153,  27},
	{0,0,0,0,0}
};


static int get_section(int x,int y,unsigned short *pl)
{
	int n;

	for (n=0; sec[n].fx; n++) {	
		if (x>=sec[n].fx && x<=sec[n].tx &&
		    y>=sec[n].fy && y<=sec[n].ty) break;
	}
	if (pl) *pl=n;

	return sec[n].section;	
}
//--------

static void editor_display(void)
{
        int rx,ry,i,green,deep,b;

        if (spritebrowse) {
                editor_display_browse();
                return;
        }

        if (editorloading) {
                dd_rect(0,0,winxres,winyres,blackcolor);
                dd_drawtext_fmt(winxres/2,winyres/2-10,whitecolor,DD_LARGE|DD_CENTER,"loading (%d%%)",100*editorloading/(1+ifilecount+cfilecount+mfilecount+pfilecount));
        }
        else if (!emf) {
                dd_rect(0,0,winxres,winyres,blackcolor);
                dd_drawtext(winxres/2,winyres/2-10,whitecolor,DD_LARGE|DD_CENTER,"no map selected");
        }
        else {
                set_map_values(emap,tick);
                rx=emf->offsetx%1024;
                ry=emf->offsety%1024;
                set_mapadd(FDX*(-rx/2+ry/2)/1024,FDY*(-rx/2-ry/2)/1024);
                display_game_map(emap);
                editor_display_game_strings();

                if (show_overview) editor_display_overview();

                // filename
                if (emfsel!=-1) {
			int s;
			unsigned short l;
			s=get_section(emfsel%emf->width,emfsel/emf->width,&l);
			dd_drawtext_fmt(4,4,whitecolor,DD_LARGE|DD_FRAME|DD_LEFT,"%s%s (%d,%d - %d (%d))",emf->modified?"* ":"",emf->filename,emfsel%emf->width,emfsel/emf->width,s,l);
                } else dd_drawtext_fmt(4,4,whitecolor,DD_LARGE|DD_FRAME|DD_LEFT,"%s%s",emf->modified?"* ":"",emf->filename);
        }

        // bar
        ry=ebut[EBUT_MAP].y+ebut[EBUT_MAP].dy;
        dx_metal_rect(0,ry,winxres,ry+1,DDFX_BRIGHT);
        dx_metal_rect(0,ry+1,winxres,winyres,DDFX_NLIGHT);

        //editor_display_text();
        editor_display_current();
        editor_display_icons();
        editor_display_i();

        // selected button
        b=butsel;
        if (b==EBUT_MAP) return;
        if (b!=-1) dx_ciframe(ebut[b].x,ebut[b].y,ebut[b].x+ebut[b].dx,ebut[b].y+ebut[b].dy,0,2);
        if (vk_lbut && b>=EBUT_ICON_BEG && b<=EBUT_ICON_END) dx_ciframe(ebut[b].x,ebut[b].y,ebut[b].x+ebut[b].dx,ebut[b].y+ebut[b].dy,2,0);

}
// control

static int mn2emnxy(int mn, int *emapx, int *emapy)
{
        int mapx,mapy;

        if (mn==-1) return -1;
        if (!emf) return -1;

        mapx=originx+mn%emapdx;
        mapy=originy+mn/emapdx;

        if (emapx) *emapx=mapx;
        if (emapy) *emapy=mapy;

        if (mapx<0 || mapy<0 || mapx>=emf->width || mapy>=emf->height) return -1;

        return mapx+mapy*emf->width;
}

static int mn2emn(int mn)
{
        return mn2emnxy(mn,NULL,NULL);
}

static void editor_do_erase_mask(void)
{
        int x,y;

        if (!emf) return;

        for (y=0; y<emf->width; y++) {
                for (x=0; x<emf->width; x++) {
                        if (emf->field[x+y*emf->width].ex_flags&EXF_MASK) {
                                mfile_set_undo(emf,x+y*emf->width);
                                emf->field[x+y*emf->width].ex_flags&=!EXF_MASK;
                                do_beep=1;
                        }
                }
        }
}

static void editor_do_getcurfield(void)
{
        if (emfsel==-1) return;
        memcpy(&cur_field,&emf->field[emfsel],sizeof(MFIELD));
        do_beep=1;
}

static void editor_do_getintfield(void)
{
        MFIELD *field;
        int f,i,s,a,use;
        ITYPE *itype;

        if (emfsel==-1) { cur_itype=NULL; return; }

        field=&emf->field[emfsel];

        for (use=f=0; f<pfilecount; f++) {
                for (i=0; i<pfilelist[f]->pcount; i++) {
                        itype=pfilelist[f]->plist[i];
                        for (a=0; a<4; a++) {

                                if (!field->sprite[a]) continue;

                                switch(itype->generic.type) {
                                        case ITYPE_GROUND:
                                                for(s=0; s<itype->ground.gridx*itype->ground.gridy; s++) if (field->sprite[a]==itype->ground.sprite[s]) { use=1; break; }
                                                break;
                                        case ITYPE_LINEWALL:
                                                for(s=0; s<itype->linewall.len; s++) if (field->sprite[a]==itype->linewall.sprite[s]) { use=1; break; }
                                                break;
                                        case ITYPE_CHANCE:
                                                for(s=0; s<itype->chance.len; s++) if (field->sprite[a]==itype->chance.sprite[s]) { use=1; break; }
                                                break;
                                }

                                if (use) {
                                        cur_itype=itype;
                                        cur_field.flags=field->flags;
                                        if (!server_do) cur_iaffect=a; // :(
                                        do_beep=1;
                                        return;
                                }
                        }
                        /*
                        if (itype->generic.type==ITYPE_GROUND && field->sprite[0]) {
                                for (s=0; s<itype->ground.gridx*itype->ground.gridy; s++) {
                                        if (itype->ground.sprite[s]==field->sprite[0]) {
                                                cur_itype=itype;
                                                cur_field.flags=field->flags;
                                                if (!server_do) cur_iaffect=0; // :(
                                                do_beep=1;
                                                return;
                                        }
                                }
                        }
                        else if (itype->generic.type==ITYPE_LINEWALL && field->sprite[2]) {
                                for (s=0; s<itype->linewall.len; s++) {
                                        if (itype->linewall.sprite[s]==field->sprite[2]) {
                                                cur_itype=itype;
                                                cur_field.flags=field->flags;
                                                if (!server_do) cur_iaffect=2; // :(
                                                do_beep=1;
                                                return;
                                        }
                                }
                        }
                        else if (itype->generic.type==ITYPE_CHANCE) {
                                for (s=0; s<itype->chance.len; s++) {
                                        if (itype->linewall.sprite[s]==field->sprite[2]) {
                                                cur_itype=itype;
                                                cur_field.flags=field->flags;
                                                if (!server_do) cur_iaffect=2; // :(
                                                do_beep=1;
                                                return;
                                        }
                                }
                        }
                        */
                }
        }

        cur_itype=NULL;
}

static void ice_trans(MFIELD *src)
{
	switch(src->sprite[2]) {
		case 21430:
		case 21431:
		case 21432:
		case 21433:	src->sprite[1]=src->sprite[2]+8; break;

		case 21350:
		case 21351:
		case 21352:
		case 21353:	src->sprite[1]=src->sprite[2]+88; break;

		case 21450: case 21452: case 21454: case 21456: case 21458:
		case 21460: case 21462: case 21464: case 21466: case 21468:
		case 21470: case 21472: case 21474: case 21476: case 21478:
		case 21480: case 21482: case 21484: case 21486: case 21488:
		case 21490: case 21492: case 21494: case 21496: case 21498:
		case 21500: case 21502: case 21504: case 21506: case 21508:
		case 21510: case 21512: case 21514: case 21516: case 21518:
		case 21520: case 21522: case 21524: case 21526: case 21528:
		case 21530: case 21532: case 21534: case 21536: case 21538:
		case 21540: case 21542: case 21544: case 21546: case 21548:
			src->sprite[1]=src->sprite[2]+100; break;
	}
}


static void editor_do_setcurfield(void)
{
        MFIELD *src,*dst,resbuf,*res=&resbuf;
        char *emode=cur_emode,i;

        if (emfsel==-1) return;

        if (icontool==15) src=&rub_field;
        else src=&cur_field;

        dst=&emf->field[emfsel];

        // hardcore mask
        res->ex_flags=0;

        if (icontool==4) {
                memcpy(res,dst,sizeof(*res));
                if (vk_alt) res->ex_flags&=~EXF_MASK; else res->ex_flags|=EXF_MASK;
        }
        else {
                if (emode[EMODE_CHR]) res->chr=src->chr; else res->chr=dst->chr;
                if (emode[EMODE_ITM]) res->itm=src->itm; else res->itm=dst->itm;
                for (res->flags=dst->flags,i=0; i<MAX_EFLAG; i++) {
                        if (emode[EMODE_FLAG+i]) {
                                if (src->flags&eflags[i].mask) res->flags|=eflags[i].mask; else res->flags&=~eflags[i].mask;
                        }
                }
                if (emode[EMODE_SPRITE+0]) res->sprite[0]=src->sprite[0]; else res->sprite[0]=dst->sprite[0];
                if (emode[EMODE_SPRITE+1]) res->sprite[1]=src->sprite[1]; else res->sprite[1]=dst->sprite[1];
                if (emode[EMODE_SPRITE+2]) res->sprite[2]=src->sprite[2]; else res->sprite[2]=dst->sprite[2];
                if (emode[EMODE_SPRITE+3]) res->sprite[3]=src->sprite[3]; else res->sprite[3]=dst->sprite[3];

                if (server_do) res->ex_flags|=EXF_BRIGHT;
        }

	ice_trans(src);

        if (memcmp(res,dst,sizeof(MFIELD))) {
                if (!server_do) emf->modified=1;
                do_beep=1;
                mfile_set_undo(emf,emfsel);
                memcpy(dst,res,sizeof(MFIELD));
        }
}

static int _get_linewall_pos(int x, int y, struct itype_linewall *linewall)
{
        int sprite,i;

        if (x<0 || y<0 || x>=emf->width || y>=emf->height) return -1;

        sprite=emf->field[x+y*emf->width].sprite[cur_iaffect];
        for (i=0; i<linewall->len; i++) if (sprite==linewall->sprite[i]) return i;
        return -1;

}

static void editor_do_setintfield(void)
{
        MFIELD srcbuf,*src=&srcbuf,*dst;
        int mx,my,pos,i;
        char *emode=cur_emode;

        if (emfsel==-1) return;
        if (!cur_itype) return;

        dst=&emf->field[emfsel];
        mx=emfsel%emf->width;
        my=emfsel/emf->width;

        bzero(src,sizeof(*src));
        if (cur_iaffect==0 || emode[EMODE_SPRITE+0]) src->sprite[0]=0; else src->sprite[0]=dst->sprite[0];
        if (cur_iaffect==1 || emode[EMODE_SPRITE+1]) src->sprite[1]=0; else src->sprite[1]=dst->sprite[1];
        if (cur_iaffect==2 || emode[EMODE_SPRITE+2]) src->sprite[2]=0; else src->sprite[2]=dst->sprite[2];
        if (cur_iaffect==3 || emode[EMODE_SPRITE+3]) src->sprite[3]=0; else src->sprite[3]=dst->sprite[3];
        if (cur_iaffect==4 || emode[EMODE_ITM]) src->itm=NULL; else src->itm=dst->itm;
        if (cur_iaffect==5 || emode[EMODE_CHR]) src->chr=NULL; else src->chr=dst->chr;

        switch (cur_itype->generic.type) {
                case ITYPE_GROUND:
                        src->sprite[cur_iaffect]=cur_itype->ground.sprite[(mx%cur_itype->ground.gridx)+(my%cur_itype->ground.gridy)*cur_itype->ground.gridx];
                        break;
                case ITYPE_LINEWALL:
                        if ((pos=_get_linewall_pos(mx-1,my,&cur_itype->linewall))!=-1) { src->sprite[cur_iaffect]=cur_itype->linewall.sprite[(pos+1)%cur_itype->linewall.len]; break; }
                        if ((pos=_get_linewall_pos(mx,my-1,&cur_itype->linewall))!=-1) { src->sprite[cur_iaffect]=cur_itype->linewall.sprite[(pos+cur_itype->linewall.len-1)%cur_itype->linewall.len]; break; }
                        if ((pos=_get_linewall_pos(mx,my+1,&cur_itype->linewall))!=-1) { src->sprite[cur_iaffect]=cur_itype->linewall.sprite[(pos+1)%cur_itype->linewall.len]; break; }
                        if ((pos=_get_linewall_pos(mx+1,my,&cur_itype->linewall))!=-1) { src->sprite[cur_iaffect]=cur_itype->linewall.sprite[(pos+cur_itype->linewall.len-1)%cur_itype->linewall.len]; break; }
                        src->sprite[cur_iaffect]=cur_itype->linewall.sprite[0];
                        break;
                case ITYPE_CHANCE:
                        i=rrand(cur_itype->chance.probsum);
                        for (pos=0; pos<cur_itype->chance.len-1; pos++) {
                                i-=cur_itype->chance.prob[pos];
                                if (i<=0) break;
                        }

                        src->sprite[cur_iaffect]=cur_itype->chance.sprite[pos];
                        break;
                default:
                        return;
        }
	
	ice_trans(src);

        // flags
        for (src->flags=dst->flags,i=0; i<MAX_EFLAG; i++) {
                if (cur_emode[EMODE_FLAG+i]) {
                        if (cur_field.flags&eflags[i].mask) src->flags|=eflags[i].mask; else src->flags&=~eflags[i].mask;
                }
        }

        // simply copy src into dst
        if (server_do) src->ex_flags|=EXF_BRIGHT;

        if (memcmp(src,dst,sizeof(MFIELD))) {
                if (!server_do) emf->modified=1;
                do_beep=1;
                mfile_set_undo(emf,emfsel);
                memcpy(dst,src,sizeof(MFIELD));
        }
}

static void editor_do_setcurrect(void)
{
        int sx,sy,ex,ey,x,y,tmpemfsel;

        if (emfsel==-1) return;
        if (rcemfsel==-1) { editor_do_setcurfield(); return; }

        sx=rcemfsel%emf->width;
        sy=rcemfsel/emf->width;
        ex=emfsel%emf->width;
        ey=emfsel/emf->width;
        if (sx>ex) { x=sx; sx=ex; ex=x; }
        if (sy>ey) { y=sy; sy=ey; ey=y; }

        tmpemfsel=emfsel;

        for (y=sy; y<=ey; y++) {
                for (x=sx; x<=ex; x++) {
                        emfsel=x+y*emf->width;
                        editor_do_setcurfield();
                }
        }

        emfsel=tmpemfsel;
}

static void editor_do_setintrect(void)
{
        int sx,sy,ex,ey,x,y,tmpemfsel;

        if (emfsel==-1) return;
        if (rcemfsel==-1) { editor_do_setintfield(); return; }

        sx=rcemfsel%emf->width;
        sy=rcemfsel/emf->width;
        ex=emfsel%emf->width;
        ey=emfsel/emf->width;
        if (sx>ex) { x=sx; sx=ex; ex=x; }
        if (sy>ey) { y=sy; sy=ey; ey=y; }

        tmpemfsel=emfsel;

        for (y=sy; y<=ey; y++) {
                for (x=sx; x<=ex; x++) {
                        emfsel=x+y*emf->width;
                        editor_do_setintfield();
                }
        }

        emfsel=tmpemfsel;
}

static void editor_set_curemode(int b, int to) // -1 0 1
{
        int i;
        char *emode=cur_emode;

        if (to==1) {
                if (b==EBUT_CGS1) emode[EMODE_SPRITE+0]=1;
                if (b==EBUT_CGS2) emode[EMODE_SPRITE+1]=1;
                if (b==EBUT_CFS1) emode[EMODE_SPRITE+2]=1;
                if (b==EBUT_CFS2) emode[EMODE_SPRITE+3]=1;
                if (b==EBUT_CITM) emode[EMODE_ITM]=1;
                if (b==EBUT_CCHR) emode[EMODE_CHR]=1;
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (b==i) emode[EMODE_FLAG+ebut[b].val]=1;
        }
        else if (to==0) {
                if (b==EBUT_CGS1) emode[EMODE_SPRITE+0]=0;
                if (b==EBUT_CGS2) emode[EMODE_SPRITE+1]=0;
                if (b==EBUT_CFS1) emode[EMODE_SPRITE+2]=0;
                if (b==EBUT_CFS2) emode[EMODE_SPRITE+3]=0;
                if (b==EBUT_CITM) emode[EMODE_ITM]=0;
                if (b==EBUT_CCHR) emode[EMODE_CHR]=0;
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (b==i) emode[EMODE_FLAG+ebut[b].val]=0;
        }
        else {
                if (b==EBUT_CGS1) emode[EMODE_SPRITE+0]^=1;
                if (b==EBUT_CGS2) emode[EMODE_SPRITE+1]^=1;
                if (b==EBUT_CFS1) emode[EMODE_SPRITE+2]^=1;
                if (b==EBUT_CFS2) emode[EMODE_SPRITE+3]^=1;
                if (b==EBUT_CITM) emode[EMODE_ITM]^=1;
                if (b==EBUT_CCHR) emode[EMODE_CHR]^=1;
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (b==i) emode[EMODE_FLAG+ebut[b].val]^=1;
        }
}

static void editor_set_curdmode(int b, int to) // -1 0 1
{
        int i;
        char *dmode=cur_dmode;

        if (to==1) {
                if (b==EBUT_CGS1) dmode[EMODE_SPRITE+0]=1;
                if (b==EBUT_CGS2) dmode[EMODE_SPRITE+1]=1;
                if (b==EBUT_CFS1) dmode[EMODE_SPRITE+2]=1;
                if (b==EBUT_CFS2) dmode[EMODE_SPRITE+3]=1;
                if (b==EBUT_CITM) dmode[EMODE_ITM]=1;
                if (b==EBUT_CCHR) dmode[EMODE_CHR]=1;
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (b==i) dmode[EMODE_FLAG+ebut[b].val]=1;
        }
        else if (to==0) {
                if (b==EBUT_CGS1) dmode[EMODE_SPRITE+0]=0;
                if (b==EBUT_CGS2) dmode[EMODE_SPRITE+1]=0;
                if (b==EBUT_CFS1) dmode[EMODE_SPRITE+2]=0;
                if (b==EBUT_CFS2) dmode[EMODE_SPRITE+3]=0;
                if (b==EBUT_CITM) dmode[EMODE_ITM]=0;
                if (b==EBUT_CCHR) dmode[EMODE_CHR]=0;
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (b==i) dmode[EMODE_FLAG+ebut[b].val]=0;
        }
        else {
                if (b==EBUT_CGS1) dmode[EMODE_SPRITE+0]^=1;
                if (b==EBUT_CGS2) dmode[EMODE_SPRITE+1]^=1;
                if (b==EBUT_CFS1) dmode[EMODE_SPRITE+2]^=1;
                if (b==EBUT_CFS2) dmode[EMODE_SPRITE+3]^=1;
                if (b==EBUT_CITM) dmode[EMODE_ITM]^=1;
                if (b==EBUT_CCHR) dmode[EMODE_CHR]^=1;
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (b==i) dmode[EMODE_FLAG+ebut[b].val]^=1;
        }
}

static void editor_set_curiaffect(int b)
{
        int i;
        if (b==EBUT_CGS1) cur_iaffect=0;
        if (b==EBUT_CGS2) cur_iaffect=1;
        if (b==EBUT_CFS1) cur_iaffect=2;
        if (b==EBUT_CFS2) cur_iaffect=3;
        if (b==EBUT_CITM) cur_iaffect=4;
        if (b==EBUT_CCHR) cur_iaffect=5;
}

static void editor_set_cureflag(int b, int to)  // -1 0 1
{
        unsigned int mask;

        mask=eflags[ebut[b].val].mask;

        if (to==1) {
                cur_field.flags|=mask;
        }
        else if (to==0) {
                cur_field.flags&=~mask;
        }
        else {
                if (cur_field.flags&mask) cur_field.flags&=~mask; else cur_field.flags|=mask;
        }
}

static void editor_set_icontool(int i)
{
        if (i<0 || i>=MAX_ICON) return;

        if (icontool==icon_list[i]) return;

        icontool=icon_list[i];
        rcemfsel=-1;
}

static void editor_do_dragmap(void)
{
        int newy,newx;
        POINT p;

        if (!emf) return;

        // x
        emf->offsetx+=1024*(oldmousex-mousex)/FDX;
        emf->offsety+=-1024*(oldmousex-mousex)/FDX;

        // y
        emf->offsetx+=1024*(oldmousey-mousey)/FDY;
        emf->offsety+=1024*(oldmousey-mousey)/FDY;

        // mouse hop
        if (mousey-(oldmousey-mousey)<=0) newy=(winyres+mousey-(oldmousey-mousey))%winyres;
        else if (mousey-(oldmousey-mousey)>=winyres-1) newy=(winyres+mousey-(oldmousey-mousey))%winyres;
        else newy=mousey;

        if (mousex-(oldmousex-mousex)<=0) newx=(winxres+mousex-(oldmousex-mousex))%winxres;
        else if (mousex-(oldmousex-mousex)>=winxres-1) newx=(winxres+mousex-(oldmousex-mousex))%winxres;
        else newx=mousex;

        if (newy!=mousey || newx!=mousex) {
                p.x=newx;
                p.y=newy;
                ClientToScreen(mainwnd,&p);
                mousex=newx;
                mousey=newy;
                SetCursorPos(p.x,p.y);
        }

}

static void editor_do_savemap(void)
{
        if (!emf) return;
        save_map_file(emf);
        addline("saved \"%s\"",emf->filename);
}

static void editor_do_copy(void)
{
        int x,y,sx,sy,ex,ey,dx,dy,t;
        MFIELD *dst,*src;

        if (!emf) return;

        for ( sx=emf->width,sy=emf->height,ex=ey=y=0; y<emf->height; y++) {
                for (x=0; x<emf->width; x++) {
                        if (emf->field[x+y*emf->width].ex_flags&EXF_MASK) {
                                if (x<sx) sx=x;
                                if (x>ex) ex=x;
                                if (y<sy) sy=y;
                                if (y>ey) ey=y;
                        }
                }
        }

        dx=ex-sx+1;
        dy=ey-sy+1;
        if (dx<=0 || dy<=0) return;

        if (cmf) {
                // delete old
                xfree(cmf->mundo);
                xfree(cmf->field);
                xfree(cmf);
        }

        cmf=xcalloc(sizeof(*cmf),MEM_EDIT);
        cmf->width=dx;
        cmf->height=dy;
        cmf->field=xcalloc(cmf->width*cmf->height*sizeof(MFIELD),MEM_EDIT);

        for (t=y=0; y<cmf->height; y++) {
                for (x=0; x<cmf->width; x++) {
                        dst=&cmf->field[x+y*cmf->width];
                        src=&emf->field[(sx+x)+(sy+y)*emf->width];
                        if (src->ex_flags&EXF_MASK) { memcpy(dst,src,sizeof(*dst)); t++; }
                }
        }

        addline("copied %d tiles (into %dx%d)",t,cmf->width,cmf->height);
}

static void editor_do_paste(void)
{
        int x,y,sx,sy;
        MFIELD *src,*dst;

        if (!emfsel) return;
        if (!cmf) return;

        sx=emfsel%emf->width-cmf->width/2;;
        sy=emfsel/emf->width-cmf->height/2;

        for (y=0; y<cmf->height; y++) {
                if (sy+y<0 || sy+y>=emf->height) continue;
                for (x=0; x<cmf->width; x++) {
                        if (sx+x<0 || sx+x>=emf->width) continue;

                        src=&cmf->field[x+y*cmf->width];
                        dst=&emf->field[(sx+x)+(sy+y)*emf->width];

                        if (!(src->ex_flags&EXF_MASK)) continue;

                        mfile_set_undo(emf,(sx+x)+(sy+y)*emf->width);
                        memcpy(dst,src,sizeof(*dst));
                        if (server_do) dst->ex_flags|=EXF_BRIGHT;
                        do_beep=1;
                }
        }
}

static void editor_next_emf(int dir)
{
        int i;

        if (!mfilecount) return;
        if (!emf) { emf=mfilelist[0]; return; }

        for (i=0; i<mfilecount; i++) if (emf==mfilelist[i]) break;
        if (i==mfilecount) { emf=mfilelist[0]; return; }

        if (dir>0) emf=mfilelist[(i+1)%mfilecount];
        if (dir<0) emf=mfilelist[(i+mfilecount-1)%mfilecount];
}

static void editor_next_pre(int dir)
{
        int i;

        if (!pfilecount) return;
        if (!pre) { pre=pfilelist[0]; return; }

        for (i=0; i<pfilecount; i++) if (pre==pfilelist[i]) break;
        if (i==pfilecount) { pre=pfilelist[0]; return; }

        if (dir>0) pre=pfilelist[(i+1)%pfilecount];
        if (dir<0) pre=pfilelist[(i+pfilecount-1)%pfilecount];
}


// control 2

static void editor_set_cmd_cursor(int cmd)
{
        HCURSOR cursor;

        switch (cmd) {
                case ECMD_GETCURFIELD:          cursor=ce_pip; break;
                case ECMD_SETCURFIELD:          cursor=c_only; break;
                case ECMD_SETCURRECT:           cursor=ce_rect; break;
                case ECMD_DRAGMAP:              cursor=ce_sizeall; break;
                case ECMD_DRAG_BROWSE:          cursor=ce_sizeall; break;
                case ECMD_PASTE:                cursor=ce_uarrow; break;
                case ECMD_SETINTFIELD:          cursor=c_only; break;
                case ECMD_SETINTRECT:           cursor=ce_rect; break;
                case ECMD_GETINTFIELD:          cursor=ce_pip; break;
                default:                        cursor=ce_darrow; break;
        }

        if (cur_cursor!=cursor) {
                SetCursor(cursor);
                cur_cursor=cursor;
        }
}

static void editor_exec_cmd(int cmd)
{
        //static int oldemfsel[2]={-1,-1};

        // protect from doing twice (might be not perfect)
        /*switch(cmd) {
                case ECMD_GETCURFIELD:  if (emfsel!=oldemfsel[0] || newemfsel) { oldemfsel[0]=emfsel; break; } else return;
                case ECMD_GETINTFIELD:  if (emfsel!=oldemfsel[0] || newemfsel) { oldemfsel[0]=emfsel; break; } else return;
                case ECMD_SETCURFIELD:  if (emfsel!=oldemfsel[1] || newemfsel) { oldemfsel[1]=emfsel; break; } else return;
                case ECMD_SETINTFIELD:  if (emfsel!=oldemfsel[1] || newemfsel) { oldemfsel[1]=emfsel; break; } else return;
        }*/

        switch(cmd) {
                case ECMD_GETCURFIELD:          do_beep=0; editor_do_getcurfield(); if (do_beep) MessageBeep(-1); break;
                case ECMD_SETCURFIELD:          do_beep=0; mfile_inc_undo(emf); editor_do_setcurfield(); if (do_beep) MessageBeep(-1); break;
                case ECMD_SETCURRECT:           if (rcemfsel==-1) { rcemfsel=emfsel; } else { do_beep=0; mfile_inc_undo(emf); editor_do_setcurrect(); if (do_beep) MessageBeep(-1); rcemfsel=-1; } break;
                case ECMD_SETINTFIELD:          do_beep=0; mfile_inc_undo(emf); editor_do_setintfield(); if (do_beep) MessageBeep(-1); break;
                case ECMD_GETINTFIELD:          do_beep=0; editor_do_getintfield(); if (do_beep) MessageBeep(-1); break;
                case ECMD_SETINTRECT:           if (rcemfsel==-1) { rcemfsel=emfsel; } else { do_beep=0; mfile_inc_undo(emf); editor_do_setintrect(); if (do_beep) MessageBeep(-1); rcemfsel=-1; } break;
                case ECMD_PASTE:                do_beep=0; mfile_inc_undo(emf); editor_do_erase_mask(); editor_do_paste(); if (do_beep) MessageBeep(-1); break;
                case ECMD_CUREMODETOGGLE:       editor_set_curemode(butsel,-1); break;
                case ECMD_CUREMODEON:           editor_set_curemode(butsel, 1); break;
                case ECMD_CUREMODEOFF:          editor_set_curemode(butsel, 0); break;
                case ECMD_CUREFLAGTOGGLE:       editor_set_cureflag(butsel,-1); break;
                case ECMD_CUREFLAGON:           editor_set_cureflag(butsel, 1); break;
                case ECMD_CUREFLAGOFF:          editor_set_cureflag(butsel, 0); break;
                case ECMD_DRAGMAP:              editor_do_dragmap(); break;
                case ECMD_SAVEMAP:              editor_do_savemap(); break;

                case ECMD_BROWSE_CGS1:
                case ECMD_BROWSE_CGS2:
                case ECMD_BROWSE_CFS1:
                case ECMD_BROWSE_CFS2:
                case ECMD_BROWSE_CITM:
                case ECMD_BROWSE_CCHR:
                case ECMD_BROWSE_IMODE:         editor_start_sprite_browse(cmd); break;

                case ECMD_SETICONTOOL:          editor_set_icontool(butsel-EBUT_ICON_BEG); break;
                case ECMD_CURDMODETOGGLE:       editor_set_curdmode(butsel,-1); break;
                case ECMD_SETCURIAFFECT:        editor_set_curiaffect(butsel);
        }
}

static void editor_set_cmd_states(void)
{
        int mapx,mapy,i,up;

        if (resize_editor) {
                int b,sx,sy,ex,flag;

                note("resize");
                resize_editor=0;

                make_quick(0);
                //MAXLINESHOW=min(30,(winyres-32)/LINEHEIGHT);

                // map
                b=EBUT_MAP; ebut[b].dx=winxres; ebut[b].dy=winyres-40;

                // bar
                sy=winyres-40;
                ex=winxres-4;
                sx=4;

                // left side
                // b=EBUT_CMD;  ebut[b].x=sx; sx+=ebut[b].dx+4; ebut[b].y=sy;
                for (flag=i=0; i<MAX_ICON; i++) {

                        b=EBUT_ICON_BEG+i;

                        if (!icon_list[i]) {
                                ebut[b].flags|=BUTF_NOHIT;
                                if (!flag) sx+=ebut[b].dx;
                                flag=1;
                        }
                        else {
                                ebut[b].flags|=BUTF_RECT;
                                ebut[b].x=sx;
                                ebut[b].y=sy;
                                flag=0;
                                sx+=ebut[b].dx;
                        }
                }

                // right side
                b=EBUT_CFS1; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;
                b=EBUT_CGS1; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;
                b=EBUT_CFS2; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;
                b=EBUT_CGS2; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;
                b=EBUT_CITM; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;
                b=EBUT_CCHR; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;

                for (i=0; i<MAX_EFLAG; i++) {
                        b=EBUT_CFLG_BEG+i; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+1; ebut[b].y=sy;
                }
                ex-=4;

                b=EBUT_IMODE; ebut[b].x=ex-ebut[b].dx; ex-=ebut[b].dx+4; ebut[b].y=sy;

                note("sx=%d ex=%d",sx,ex);
        }

        // if (capbut==-1 || !(mousex>=0 && mousey>=0 && mousex<winxres && mousey<winyres)) return; // captured or out of the window

        set_cmd_key_states();

        if (spritebrowse) return;

        if (rdragexec==ECMD_DRAGMAP) {
                editor_set_cmd_cursor(rdragexec);
                return;
        }

        // get button
        butsel=-1;

        for (i=0; i<MAX_EBUT; i++) {
                if (ebut[i].flags&BUTF_NOHIT) continue;
                if (mousey<ebut[i].y || mousey>=ebut[i].y+ebut[i].dy) continue;
                if (mousex<ebut[i].x || mousex>=ebut[i].x+ebut[i].dx) continue;

                butsel=i;
                break;
        }

        if (butsel==EBUT_MAP || capbut==EBUT_MAP) {
                stom(mousex,mousey,&mapx,&mapy);
                emfsel=mn2emn(mapx+mapy*emapdx);
        }
        else emfsel=-1;

        // get barup (bad style)
        if (butsel!=-1) {
                if (mousey<=ebut[butsel].y+ebut[butsel].dy/2) barup=0; else barup=1;
        }
        else {
                for (i=0; i<MAX_EBUT; i++) {
                        if (ebut[i].flags&BUTF_NOHIT) continue;
                        if (mousey<ebut[i].y+ebut[i].dy || mousey>=winyres) continue;
                        if (mousex<ebut[i].x || mousex>=ebut[i].x+ebut[i].dx) continue;
                        if (i==EBUT_MAP) continue;

                        butsel=i;
                        barup=3;
                        break;
                }
        }


        if (capbut!=EBUT_MAP) {

                // set lcmd
                lcmd=ldragcmd=ECMD_NONE;

                // if (emfsel!=-1 && !vk_control) ldragcmd=lcmd=ECMD_SETCURFIELD;
                // if (emfsel!=-1 &&  vk_control) lcmd=ECMD_SETCURRECT;

                if (icontool== 8 && emfsel!=-1 && !vk_control) lcmd=ldragcmd=ECMD_SETCURFIELD;
                if (icontool== 8 && emfsel!=-1 &&  vk_control) lcmd=ECMD_SETCURRECT;
                if (icontool== 8 && emfsel!=-1 &&  vk_shift)   ldragcmd=lcmd=ECMD_GETCURFIELD; // overrides - order is important
                if (icontool==15 && emfsel!=-1 && !vk_control) lcmd=ldragcmd=ECMD_SETCURFIELD;
                if (icontool==15 && emfsel!=-1 &&  vk_control) lcmd=ECMD_SETCURRECT;
                if (icontool== 4 && emfsel!=-1 && !vk_control) lcmd=ldragcmd=ECMD_SETCURFIELD;
                if (icontool== 4 && emfsel!=-1 &&  vk_control) lcmd=ECMD_SETCURRECT;
                if (icontool==16 && emfsel!=-1 && !vk_control) lcmd=ldragcmd=ECMD_SETINTFIELD;
                if (icontool==16 && emfsel!=-1 &&  vk_control) lcmd=ECMD_SETINTRECT;
                if (icontool==16 && emfsel!=-1 &&  vk_shift)   ldragcmd=lcmd=ECMD_GETINTFIELD;  // overrides - order is important

                if (icontool==0 && emfsel!=-1 && cmf) lcmd=ECMD_PASTE;

                if (butsel>=EBUT_ICON_BEG && butsel<=EBUT_ICON_END) {
                        i=butsel-EBUT_ICON_BEG;
                        if (icon_list[i]==14) lcmd=ECMD_SAVEMAP;
                        if (icon_list[i]==8) lcmd=ECMD_SETICONTOOL;
                        if (icon_list[i]==15) lcmd=ECMD_SETICONTOOL;
                        if (icon_list[i]==4) lcmd=ECMD_SETICONTOOL;
                        if (icon_list[i]==16) lcmd=ECMD_SETICONTOOL;
                }

                if (barup==3) {
                        if (butsel==EBUT_CGS1) lcmd=ECMD_CURDMODETOGGLE;
                        if (butsel==EBUT_CGS2) lcmd=ECMD_CURDMODETOGGLE;
                        if (butsel==EBUT_CFS1) lcmd=ECMD_CURDMODETOGGLE;
                        if (butsel==EBUT_CFS2) lcmd=ECMD_CURDMODETOGGLE;
                        if (butsel==EBUT_CITM) lcmd=ECMD_CURDMODETOGGLE;
                        if (butsel==EBUT_CCHR) lcmd=ECMD_CURDMODETOGGLE;
                        for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) if (butsel==i) lcmd=ECMD_CURDMODETOGGLE;
                }
                else {
                        if (icontool!=16) {
                                if (butsel==EBUT_CGS1 && cur_emode[EMODE_SPRITE+0]) lcmd=ECMD_BROWSE_CGS1;
                                if (butsel==EBUT_CGS2 && cur_emode[EMODE_SPRITE+1]) lcmd=ECMD_BROWSE_CGS2;
                                if (butsel==EBUT_CFS1 && cur_emode[EMODE_SPRITE+2]) lcmd=ECMD_BROWSE_CFS1;
                                if (butsel==EBUT_CFS2 && cur_emode[EMODE_SPRITE+3]) lcmd=ECMD_BROWSE_CFS2;
                                if (butsel==EBUT_CITM && cur_emode[EMODE_ITM]) lcmd=ECMD_BROWSE_CITM;
                                if (butsel==EBUT_CCHR && cur_emode[EMODE_CHR]) lcmd=ECMD_BROWSE_CCHR;
                        }
                        else {
                                if (butsel==EBUT_CGS1) lcmd=ECMD_SETCURIAFFECT;
                                if (butsel==EBUT_CGS2) lcmd=ECMD_SETCURIAFFECT;
                                if (butsel==EBUT_CFS1) lcmd=ECMD_SETCURIAFFECT;
                                if (butsel==EBUT_CFS2) lcmd=ECMD_SETCURIAFFECT;
                                if (butsel==EBUT_CITM) lcmd=ECMD_SETCURIAFFECT;
                                if (butsel==EBUT_CCHR) lcmd=ECMD_SETCURIAFFECT;
                        }
                        for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) {
                                if (butsel==i/* && cur_emode[EMODE_FLAG+i-EBUT_CFLG_BEG]*/) {
                                        if (barup==0) ldragcmd=lcmd=ECMD_CUREFLAGON; else ldragcmd=lcmd=ECMD_CUREFLAGOFF;
                                }
                        }
                        if (butsel==EBUT_IMODE/* && cur_emode[EMODE_CHR]*/ /*&& icontool==16*/) lcmd=ECMD_BROWSE_IMODE;
                }

                // set rcmd
                rcmd=rdragcmd=ECMD_NONE;
                if (butsel==EBUT_MAP) rcmd=rdragcmd=ECMD_DRAGMAP;

                if (butsel==EBUT_CGS1) { if (cur_emode[EMODE_SPRITE+0]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                if (butsel==EBUT_CGS2) { if (cur_emode[EMODE_SPRITE+1]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                if (butsel==EBUT_CFS1) { if (cur_emode[EMODE_SPRITE+2]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                if (butsel==EBUT_CFS2) { if (cur_emode[EMODE_SPRITE+3]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                if (butsel==EBUT_CITM) { if (cur_emode[EMODE_ITM]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                if (butsel==EBUT_CCHR) { if (cur_emode[EMODE_CHR]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                for (i=EBUT_CFLG_BEG; i<=EBUT_CFLG_END; i++) {
                                if (butsel==i) { if (cur_emode[EMODE_FLAG+ebut[i].val]) rcmd=ECMD_CUREMODEOFF; else rcmd=ECMD_CUREMODEON; }
                }

        }

        // cursor
        if (rdragexec) editor_set_cmd_cursor(rdragexec);
        else if (ldragexec) editor_set_cmd_cursor(ldragexec);
        else if(vk_rbut) editor_set_cmd_cursor(rcmd);
        else editor_set_cmd_cursor(lcmd);
}

// editor callbacks

int editor_sizeproc(void)
{
        note("size_editor");
        resize_editor=1;
        return 0;
}

int editor_mouseproc(int msg)
{
        static int lcap,rcap,lcapbut=-1,rcapbut=-1;
        static int oldemfsel=-1;
        POINT p;
        int newx,newy;

        if (spritebrowse) {

                if (msg==WM_RBUTTONDOWN) SetCapture(mainwnd);
                if (msg==WM_RBUTTONUP) ReleaseCapture();
                if (msg==WM_LBUTTONUP) editor_end_sprite_browse(1);
                if (msg==WM_MOUSEMOVE && vk_rbut) {
                        if (oldmousex!=mousex || oldmousey!=mousey) {
                                spriteoffx-=mousex-oldmousex;
                                spriteoffy-=mousey-oldmousey;
                        }

                        // hop
                        if (mousey-(oldmousey-mousey)<=0) newy=(winyres+mousey-(oldmousey-mousey))%winyres;
                        else if (mousey-(oldmousey-mousey)>=winyres-1) newy=(winyres+mousey-(oldmousey-mousey))%winyres;
                        else newy=mousey;

                        if (mousex-(oldmousex-mousex)<=0) newx=(winxres+mousex-(oldmousex-mousex))%winxres;
                        else if (mousex-(oldmousex-mousex)>=winxres-1) newx=(winxres+mousex-(oldmousex-mousex))%winxres;
                        else newx=mousex;

                        if (newy!=mousey || newx!=mousex) {
                                p.x=newx;
                                p.y=newy;
                                ClientToScreen(mainwnd,&p);
                                mousex=newx;
                                mousey=newy;
                                SetCursorPos(p.x,p.y);
                        }

                }

                oldmousex=mousex;
                oldmousey=mousey;

                return 0;
        }

        if (emfsel!=oldemfsel) { newemfsel=1; oldemfsel=emfsel; } else newemfsel=0;

        if (msg==WM_LBUTTONDOWN) {
                if (lcmd && lcmd==ldragcmd) editor_exec_cmd(lcmd);
                if (lcmd==ECMD_SETCURRECT && !ldragcmd) editor_exec_cmd(lcmd);
                if (lcmd==ECMD_SETINTRECT && !ldragcmd) editor_exec_cmd(lcmd);
                lcap=1;
                lcapbut=capbut=butsel;
                if (!rcap) SetCapture(mainwnd);
        }
        if (msg==WM_RBUTTONDOWN) {
                if (rcmd && rcmd==rdragcmd) editor_exec_cmd(rcmd);
                rcap=1;
                rcapbut=capbut=butsel;
                if (!lcap) SetCapture(mainwnd);
        }
        if (msg==WM_LBUTTONUP) {
                if (lcmd && !ldragexec && lcapbut==butsel) editor_exec_cmd(lcmd);
                lcap=0;
                lcapbut=capbut=-1;
                ldragexec=0;
                if (!rcap) ReleaseCapture(); else capbut=rcapbut;
        }
        if (msg==WM_RBUTTONUP) {
                if (rcmd && !rdragexec && rcapbut==butsel) editor_exec_cmd(rcmd);
                rcap=0;
                rcapbut=capbut=-1;
                rdragexec=0;
                if (!lcap) ReleaseCapture(); else capbut=lcapbut;
        }
        if (msg==WM_MOUSEMOVE && !spritebrowse) {
                if (oldmousex!=mousex || oldmousey!=mousey) {
                        /*
                        if (ldragexec && vk_lbut) { ldragexec=ldragcmd; editor_exec_cmd(ldragcmd); }
                        else if (ldragcmd && vk_lbut && lcapbut==butsel) { ldragexec=ldragcmd; editor_exec_cmd(ldragcmd); }
                        if (rdragexec && vk_rbut) { rdragexec=rdragcmd; editor_exec_cmd(rdragcmd); }
                        else if (rdragcmd && vk_rbut && rcapbut==butsel) { rdragexec=rdragcmd; editor_exec_cmd(rdragcmd); }
                        */
                        if (ldragcmd && vk_lbut) {
                                if (butsel!=EBUT_MAP || lcapbut==butsel) { ldragexec=ldragcmd; editor_exec_cmd(ldragcmd); }
                        }
                        if (rdragcmd && vk_rbut) {
                                if (butsel!=EBUT_MAP || rcapbut==butsel) { rdragexec=rdragcmd; editor_exec_cmd(rdragcmd); }
                        }

                }
        }

        oldmousex=mousex;
        oldmousey=mousey;

        return 0;
}

void editor_keyproc(int wparam)
{
        int i;

        switch(wparam) {

		case VK_ESCAPE:         if (spritebrowse) editor_end_sprite_browse(0); return;
		case VK_F1:            	show_overview^=1; addline("show_overview=%d",show_overview); return;
		case VK_F2:            	return;
		case VK_F3:            	return;
                case VK_F4:            	return;
                case VK_F5:            	dd_reset_cache(1,1,1); return;
		case VK_F6:             nocut^=1; return;
		case VK_F7:             allcut^=1; return;
		
		//case VK_F8:		db_remove_overdraw(); return;
		//case VK_F9:		db_create_pentlava(); return;
		//case VK_F10:		db_create_penthalf(); return;
                case VK_F9:             draw_underwater^=1; return;
                case VK_F11:            list_mem(); return;
                case VK_F12:            if (spritebrowse) editor_end_sprite_browse(0); else quit=1; return;

                case '1':               if (vk_control) editor_set_icontool(2); return;
                case '2':               if (vk_control) editor_set_icontool(3); return;
                case '3':               if (vk_control) editor_set_icontool(4); return;
                case '4':               if (vk_control) editor_set_icontool(5); return;


                case 'Z':               if (vk_control) { mfile_use_undo(emf); MessageBeep(-1); } return;
                case 'A':               if (vk_control) { for (i=0; i<MAX_EMODE; i++) cur_emode[i]=1; } return;
                case 'N':               if (vk_control) { for (i=0; i<MAX_EMODE; i++) cur_emode[i]=0; } return;

                case 'D':               if (icontool==4 && vk_control) {
                                                do_beep=0;
                                                mfile_inc_undo(emf);
                                                editor_do_erase_mask();
                                                if (do_beep) MessageBeep(-1);
                                        }
                                        return;

                case VK_INSERT:         if (icontool==4 && vk_control) { editor_do_copy(); return; }
                                        if (vk_shift) { if (cmf) icontool=0; return; }
                                        return;

                case VK_RETURN:         cmd_proc(CMD_RETURN); return;
                case VK_DELETE:         cmd_proc(CMD_DELETE); return;
                case VK_BACK:           cmd_proc(CMD_BACK); return;

                case VK_HOME:           cmd_proc(CMD_HOME); return;
                case VK_END:            cmd_proc(CMD_END); return;

                case VK_LEFT:           if (!vk_control && !vk_shift && !vk_alt) { cmd_proc(CMD_LEFT); return; }
                                        if (vk_shift && emf) { emf->offsetx--; emf->offsety++; return;}
                                        return;
                case VK_RIGHT:          if (!vk_control && !vk_shift && !vk_alt) { cmd_proc(CMD_RIGHT); return; }
                                        if (vk_shift && emf) { emf->offsetx++; emf->offsety--; return; }
                                        return;
                case VK_UP:             if (!vk_control && !vk_shift && !vk_alt) { cmd_proc(CMD_UP); return; }
                                        if (vk_shift && emf) { emf->offsetx--; emf->offsety--; return; }
                                        return;
                case VK_DOWN:           if (!vk_control && !vk_shift && !vk_alt) { cmd_proc(CMD_DOWN); return; }
                                        if (vk_shift && emf) { emf->offsetx++; emf->offsety++; return; }
                                        return;

                case VK_TAB:            if (spritebrowse==ECMD_BROWSE_IMODE) {
                                                if (vk_control && vk_shift) editor_next_pre(-1);
                                                else if (vk_control && !vk_shift) editor_next_pre(+1);
                                        }
                                        else if (spritebrowse==0) {
                                                if (vk_control && vk_shift) editor_next_emf(-1);
                                                else if (vk_control && !vk_shift) editor_next_emf(+1);
                                        }
                                        return;
        }
}

int docharview=0,dochar=30;

#pragma argsused
void editor_tinproc(int t, char *buf)
{
        int sprite,num;

        if (!strncmp(buf,"gs1",3)) { num=atoi(buf+3); if (num<0xFFFF) cur_field.sprite[0]=num; }
        else if (!strncmp(buf,"gs2",3)) { num=atoi(buf+3); if (num<0xFFFF) cur_field.sprite[1]=num; }
        else if (!strncmp(buf,"fs1",3)) { num=atoi(buf+3); if (num<0xFFFF) cur_field.sprite[2]=num; }
        else if (!strncmp(buf,"fs2",3)) { num=atoi(buf+3); if (num<0xFFFF) cur_field.sprite[3]=num; }
	else if (!strncmp(buf,"v",1)) { num=atoi(buf+1); docharview=num; dochar=num+10; }
        else if (!strncmp(buf,"a",1)) { num=atoi(buf+1); dochar=num; }
        else {
                num=atoi(buf);
                if (num<0xFFFF) {
                        if (spritebrowse) {
                                spriteoffx=0;
                                spriteoffy=(num/(winxres/60))*100;
                        }
                        else if (butsel==EBUT_CGS1) cur_field.sprite[0]=num;
                        else if (butsel==EBUT_CGS2) cur_field.sprite[1]=num;
                        else if (butsel==EBUT_CFS1) cur_field.sprite[2]=num;
                        else if (butsel==EBUT_CFS2) cur_field.sprite[3]=num;
                }
        }
}

//--------------------------

#define MAXDIST		25
#define SIZE		(MAXDIST*2+1)

unsigned char lostab[SIZE][SIZE];
int lxoff=0,lyoff=0,losmaxdist=0;

static void add_los(int x,int y,int v)
{
	int tx,ty;
	tx=x+lxoff;
	ty=y+lyoff;
	if (tx<0 || tx>=SIZE || ty<0 || ty>=SIZE) return;
	
        if (!lostab[tx][ty]) lostab[tx][ty]=v;
}

static int check_map(int x,int y)
{
        int m;

        if (x<1 || x>=255 || y<1 || y>=255) return 0;

        m=x+y*256;

        if (emf->field[m].flags&(MF_SIGHTBLOCK)) return 0;

        return 1;
}

static int is_close_los_down(int xc,int yc,int v)
{
	int x,y;

        x=xc+lxoff;
	y=yc+lyoff;

        if (lostab[x][y+1]==v && check_map(xc,yc+1)) return 1;
        if (lostab[x+1][y+1]==v && check_map(xc+1,yc+1)) return 1;
        if (lostab[x-1][y+1]==v && check_map(xc-1,yc+1)) return 1;

        return 0;
}

static int is_close_los_up(int xc,int yc,int v)
{
	int x,y;

        x=xc+lxoff;
	y=yc+lyoff;

        if (lostab[x][y-1]==v && check_map(xc,yc-1)) return 1;
        if (lostab[x+1][y-1]==v && check_map(xc+1,yc-1)) return 1;
        if (lostab[x-1][y-1]==v && check_map(xc-1,yc-1)) return 1;

        return 0;
}

static int is_close_los_left(int xc,int yc,int v)
{
	int x,y;

        x=xc+lxoff;
	y=yc+lyoff;

        if (lostab[x+1][y]==v && check_map(xc+1,yc)) return 1;
        if (lostab[x+1][y+1]==v && check_map(xc+1,yc+1)) return 1;
        if (lostab[x+1][y-1]==v && check_map(xc+1,yc-1)) return 1;

        return 0;
}

static int is_close_los_right(int xc,int yc,int v)
{
	int x,y;

        x=xc+lxoff;
	y=yc+lyoff;

        if (lostab[x-1][y]==v && check_map(xc-1,yc)) return 1;
        if (lostab[x-1][y+1]==v && check_map(xc-1,yc+1)) return 1;
        if (lostab[x-1][y-1]==v && check_map(xc-1,yc-1)) return 1;

        return 0;
}

static int check_los(int x,int y)
{
        x=x+lxoff;
        y=y+lyoff;
	if (x<0 || x>=SIZE || y<0 || y>=SIZE) return 0;

        return lostab[x][y];
}

static void build_los(int xc,int yc,int maxdist)
{
        int x,y,dist,found;

        bzero(lostab,sizeof(lostab));

	lxoff=MAXDIST-xc;
	lyoff=MAXDIST-yc;
	losmaxdist=maxdist;

        add_los(xc,yc,1);

	add_los(xc+1,yc,2);
	add_los(xc-1,yc,2);
	add_los(xc,yc+1,2);
	add_los(xc,yc-1,2);

	add_los(xc+1,yc+1,2);
	add_los(xc+1,yc-1,2);
	add_los(xc-1,yc+1,2);
	add_los(xc-1,yc-1,2);

        for (dist=2; dist<maxdist; dist++) {
		found=0;
                for (x=xc-dist; x<=xc+dist; x++) {
                        if (is_close_los_down(x,yc-dist,dist)) { add_los(x,yc-dist,dist+1); found=1; }
                        if (is_close_los_up(x,yc+dist,dist)) { add_los(x,yc+dist,dist+1); found=1; }
                }
                for (y=yc-dist; y<=yc+dist; y++) {
                        if (is_close_los_left(xc-dist,y,dist)) { add_los(xc-dist,y,dist+1); found=1; }
                        if (is_close_los_right(xc+dist,y,dist)) { add_los(xc+dist,y,dist+1); found=1; }
                }
		if (!found) break;		
        }	
}

int map_dist(int fx,int fy,int tx,int ty)
{
	int dx,dy;

	dx=abs(fx-tx);
	dy=abs(fy-ty);
	
	if (dx>dy) return (dx<<1)+dy;
	else return (dy<<1)+dx;
}

static void charview(int mn)
{
	int x,y,xs,ys,xe,ye,xt,yt,xc,yc;

	if (!docharview) return;

        xc=x=mn%256; yc=y=mn/256;
	if (x<0 || x>=256 || y<0 || y>=256) return;

        build_los(x,y,DIST);

	xs=max(1,x-DIST);
	ys=max(1,y-DIST);
	xe=min(256-1,x+DIST+1);
	ye=min(256-1,y+DIST+1);

	//note("%d,%d to %d,%d (%d,%d)",xs,ys,xe,ye,x,y);

        for (y=ys; y<ye; y++) {
		for (x=xs; x<xe; x++) {
			if (map_dist(xc,yc,x,y)>=dochar) continue;
                        if (x+lxoff<0 || x+lxoff>=SIZE || y+lyoff<0 || y+lyoff>SIZE) continue;
			
                        if (lostab[x+lxoff][y+lyoff]) {
				xt=x-originx;
				yt=y-originy;
				if (xt<0 || xt>=emapdx || yt<0 || yt>=emapdy) continue;
				
				if (map_dist(xc,yc,x,y)>=docharview) emap[xt+yt*emapdx].gsprite2=12;
				else emap[xt+yt*emapdx].gsprite2=10;
			}
		}
	}	
}

//--------------------------

// server
static void charview(int mn);
static void editor_server(void)
{
        int i,x,y,mn,mapx,mapy,t;
        int emn;
        static int oldtick=0;
        MFIELD *field;
        int did;
        int brightmn;

        if (!emf) return;

        //mapsel=(emfsel%emf->width)-originx+((emfsel/emf->width)-originy)*emapdx;

        mfile_inc_undo(emf);
        did=emf->uid;
        server_do=1;

        if (lcmd==ECMD_SETCURFIELD && !vk_rbut) {
                editor_do_setcurfield();
        }
        if (lcmd==ECMD_SETCURRECT) {
                editor_do_setcurrect();
        }
        if (lcmd==ECMD_SETINTFIELD && !vk_rbut) {
                editor_do_setintfield();
        }
        if (lcmd==ECMD_SETINTRECT) {
                editor_do_setintrect();
        }
        if (lcmd==ECMD_GETCURFIELD || (vk_rbut && rcmd==ECMD_GETCURFIELD)) {
                mfile_set_undo(emf,emfsel);
                emf->field[emfsel].ex_flags|=EXF_BRIGHT;
        }
        if (lcmd==ECMD_GETINTFIELD || (vk_rbut && rcmd==ECMD_GETINTFIELD)) {
                mfile_set_undo(emf,emfsel);
                emf->field[emfsel].ex_flags|=EXF_BRIGHT;
        }
        if (lcmd==ECMD_PASTE) {
                editor_do_paste();
        }

        server_do=0;

        originx=emf->offsetx/1024;
        originy=emf->offsety/1024;

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];
                mapx=quick[i].mapx;
                mapy=quick[i].mapy;

                emn=(mapx+originx)+(mapy+originy)*emf->width;

                // border of emap
                if (mapx+originx<0 || mapy+originy<0 || mapx+originx>=emf->width || mapy+originy>=emf->height) {
                        bzero(&emap[mn],sizeof(emap[mn]));
                        emap[mn].flags|=CMF_VISIBLE;
                        emap[mn].gsprite=1;
                        emap[mn].ex_flags=0;
                        emap[mn].ex_map_flags=0;
			emap[mn].sec_nr=emap[mn].sec_line=0;
                        emap[mn].ex_chrname=NULL;
                        emap[mn].ex_itmname=NULL;
                        continue;
                }

                // src field
                field=&emf->field[emn];

                emap[mn].ex_flags=field->ex_flags;
                emap[mn].ex_map_flags=field->flags;
		emap[mn].sec_nr=get_section(mapx+originx,mapy+originy,&emap[mn].sec_line);

                if (icontool==4 && (field->ex_flags&EXF_MASK)) {
                        emap[mn].ex_flags|=EXF_BRIGHT;
                }

                emap[mn].flags=0;
                emap[mn].flags|=CMF_VISIBLE;
                if (field->flags&MF_SIGHTBLOCK) emap[mn].flags|=15; else emap[mn].flags|=0;
		if (field->flags&MF_UNDERWATER && draw_underwater) emap[mn].flags|=CMF_UNDERWATER;

                if (cur_dmode[EMODE_SPRITE+0]) emap[mn].gsprite=field->sprite[0]; else emap[mn].gsprite=0;
                if (cur_dmode[EMODE_SPRITE+2]) emap[mn].fsprite=field->sprite[2]; else emap[mn].fsprite=0;
                if (cur_dmode[EMODE_SPRITE+1]) emap[mn].gsprite2=field->sprite[1]; else emap[mn].gsprite2=0;
                if (cur_dmode[EMODE_SPRITE+3]) emap[mn].fsprite2=field->sprite[3]; else emap[mn].fsprite2=0;

                if (cur_dmode[EMODE_ITM] && field->itm) {
                        emap[mn].isprite=field->itm->sprite;
                        emap[mn].ex_itmname=field->itm->uni;
                }
                else emap[mn].isprite=0;

                if (cur_dmode[EMODE_CHR] && field->chr) {
                        emap[mn].csprite=field->chr->sprite;
                        emap[mn].ex_chrname=field->chr->uni;
                        /*if (emap[mn].dir==0) emap[mn].dir=1;
                        for (t=oldtick; t<tick; t++) {
                                if (++emap[mn].step>=emap[mn].duration) {
                                        emap[mn].dir=1+(8+emap[mn].dir-1+rrand(3)-1)%8;
                                        emap[mn].duration=8+rrand(24);
                                        emap[mn].action=rrand(25);
                                        emap[mn].step=0;
                                }
                        }*/
			emap[mn].dir=1;
			emap[mn].step=emap[mn].duration=1;
			emap[mn].action=0;			
                }
                else emap[mn].csprite=0;
        }

	for (i=0; i<maxquick; i++) {

                //mn=quick[i].mn[4];
                mapx=quick[i].mapx;
                mapy=quick[i].mapy;

                emn=(mapx+originx)+(mapy+originy)*emf->width;

                // border of emap
                if (mapx+originx<0 || mapy+originy<0 || mapx+originx>=emf->width || mapy+originy>=emf->height) continue;

                // src field
                //field=&emf->field[emn];

                if (emn==emfsel) {
                        charview(emn);
                }
        }

        oldtick=tick;

        if (emf->uend && emf->mundo[emf->uend-1].uid==did) mfile_use_undo(emf);
}

// main loop

int init_editor(void)
{
        int i;

        note("init_editor");

        // cursors
        ce_pen=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_PEN));
        ce_rect=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_RECT));
        ce_darrow=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_DARROW));
        ce_darrow_0101=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_DARROW_0101));
        ce_pip=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_PIP));
        ce_sizeall=LoadCursor(NULL,IDC_SIZEALL);
        ce_uarrow=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_UARROW));
        ce_uarrowp=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_UARROWP));
        ce_uarrowm=LoadCursor(instance,MAKEINTRESOURCE(IDCU_E_UARROWM));

        // init
        scan_editor_files();

        allcut=1;

        // clipboard map
        /*
        cmf=xcalloc(sizeof(*cmf),MEM_EDIT);
        cmf->width=256;
        cmf->height=256;
        cmf->field=xcalloc(cmf->width*cmf->height*sizeof(MFIELD),MEM_EDIT);
        */

        // buttons
        set_ebut(EBUT_MAP,    0,0,0,0,BUTF_RECT);
        set_ebut(EBUT_CMD, 100,33,0,0,BUTF_RECT);
        set_ebut(EBUT_CGS1, 50,33,0,0,BUTF_RECT);
        set_ebut(EBUT_CGS2, 50,33,0,0,BUTF_RECT);
        set_ebut(EBUT_CFS1, 50,33,0,0,BUTF_RECT);
        set_ebut(EBUT_CFS2, 50,33,0,0,BUTF_RECT);
        set_ebut(EBUT_CITM, 50,33,0,0,BUTF_RECT);
        set_ebut(EBUT_CCHR, 50,33,0,0,BUTF_RECT);

        for (i=0; i<MAX_EFLAG; i++) set_ebut(EBUT_CFLG_BEG+i,10,33,0,MAX_EFLAG-i-1,BUTF_RECT);
        for (i=0; i<MAX_ICON; i++) set_ebut(EBUT_ICON_BEG+i,22,33,0,0,BUTF_RECT);

        set_ebut(EBUT_IMODE, 130,33,0,0,BUTF_RECT);

        return 0;
}

int exit_editor(void)
{
        int i,ii;

        emf=NULL;
        pre=NULL;

        if (cmf) {
                xfree(cmf->field);
                xfree(cmf);
                cmf=NULL;
        }

        for (i=0; i<mfilecount; i++) {
                xfree(mfilelist[i]->field);
                xfree(mfilelist[i]->mundo);
                xfree(mfilelist[i]);
        }
        xfree(mfilelist);
        mfilelist=NULL;
        mfilecount=0;

        for (i=0; i<cfilecount; i++) {
                for (ii=0; ii<cfilelist[i]->ccount; ii++) {
                        xfree(cfilelist[i]->clist[ii]);
                }
                xfree(cfilelist[i]->clist);
                xfree(cfilelist[i]);
        }
        xfree(cfilelist);
        cfilelist=NULL;
        cfilecount=0;

        for (i=0; i<ifilecount; i++) {
                for (ii=0; ii<ifilelist[i]->icount; ii++) {
                        xfree(ifilelist[i]->ilist[ii]);
                }
                xfree(ifilelist[i]->ilist);
                xfree(ifilelist[i]);
        }
        xfree(ifilelist);
        ifilelist=NULL;
        ifilecount=0;

        xfree(emap);
        emap=NULL;
        emapdx=0;
        emapdy=0;

        // free_int_files();
        free_pre_files();

        return 0;
}

int editor_main_loop(void)
{
        MSG msg;

        edstarttime=GetTickCount();

        dd_rect(0,0,XRES,YRES,blackcolor); dd_flip();
        dd_rect(0,0,XRES,YRES,blackcolor); dd_flip();

        while (editorloading) {
                while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                }
                editor_set_cmd_states();
                editor_display();
                load_editor_files();
                load_editor_files();
                load_editor_files();
                dd_flip();
        }
        if (mfilecount) emf=mfilelist[0];
        if (pfilecount) pre=pfilelist[0];

        while (!quit) {

                if (editorloading) {
                        load_editor_files();
                        load_editor_files();
                        load_editor_files();
                }
                else {
                        POINT p;
                        GetCursorPos(&p);
                        if (WindowFromPoint(p)!=mainwnd) Sleep(200);
                }

                now=GetTickCount();
                tick=TPS*(now-edstarttime)/1000;

                if (dd_islost()) {
                        while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                        }
                        Sleep(50);
                        dd_restore();
                }
                else {
                        editor_set_cmd_states();
                        if (!spritebrowse) editor_server();
                        editor_display();
                        do {
                                while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
                                        TranslateMessage(&msg);
                                        DispatchMessage(&msg);
                                }
                                Sleep(0);
                        }
                        while(TPS*(GetTickCount()-edstarttime)/1000<=(unsigned int)tick);

                        dd_flip();
                }
        }

        return 0;
}

int db_pents_wall(int x,int y)
{
	int m,base=14303,sprite,flags=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK|MF_INDOORS;

	m=x+y*256;

	if (emf->field[m].flags&MF_MOVEBLOCK) return 1;

	mfile_set_undo(emf,m);

	sprite=base+x%3+(y%3)*3;

	emf->field[m].sprite[2]=sprite;
	emf->field[m].flags=flags;

        return 0;
}

int db_pents_halfwall(int x,int y)
{
	int m,base=14312,sprite,flags=MF_MOVEBLOCK|MF_INDOORS;

        m=x+y*256;

	if (emf->field[m].flags&MF_MOVEBLOCK) return 1;
	
	mfile_set_undo(emf,m);

	sprite=base+x%3+(y%3)*3;

	emf->field[m].sprite[2]=sprite;
	emf->field[m].flags=flags;

        return 0;
}

int db_pents_lava(int x,int y)
{
	int m,base=12163,sprite,flags=MF_MOVEBLOCK|MF_FIRETHRU|MF_INDOORS;

	m=x+y*256;

	if (emf->field[m].flags&MF_MOVEBLOCK) return 1;
	
	mfile_set_undo(emf,m);

	sprite=base+x%2+(y%2)*2;

	emf->field[m].sprite[0]=sprite;
	emf->field[m].flags=flags;

        return 0;
}

void db_create_pentwall(void)
{
	float x,y,angle,change;
	int tx,ty,n,fx,fy,stop=0;

        fx=x=emfsel%256; fy=y=emfsel/256;
	//fx=x=rrand(255); fy=y=rrand(255);
	angle=rrand(360); change=rrand(4000)/500.0-2;
	//addline("x=%d, y=%d",fx,fy);
		
	for (n=0; !stop && n<100; n++) {
		tx=x; ty=y;
		if (tx<2 || tx>254 || ty<2 || ty>254) break;

		while (fx<tx) { fx++; stop=db_pents_wall(fx,fy); }
		while (!stop && fx>tx) { fx--; stop=db_pents_wall(fx,fy); }
		while (!stop && fy<ty) { fy++; stop=db_pents_wall(fx,fy); }
		while (!stop && fy>ty) { fy--; stop=db_pents_wall(fx,fy); }		

		x+=sin(2*M_PI*angle/360);
		y+=cos(2*M_PI*angle/360);

		angle+=change;
		if (angle<0) angle+=360;
		if (angle>=360) angle-=360;
	}	

	mfile_inc_undo(emf);
}

void db_create_pentlava(void)
{
        int x,y,n;

	x=emfsel%256; y=emfsel/256;

	for (n=0; n<50; n++) {
		if (x<2 || x>254 || y<2 || y>254) break;

		db_pents_lava(x,y);

		if (rand()%4==0) x++;
		else if (rand()%4==0) x--;
		else if (rand()%4==0) y++;
		else if (rand()%4==0) y--;
	}
	mfile_inc_undo(emf);
}

void db_create_penthalf(void)
{
	int sx,sy,dx,dy,x,y,m;

	sx=emfsel%256; sy=emfsel/256;

	for (dx=-6; dx<=6; dx++) {
		x=sx+dx;
		if (x<2 || x>254) continue;
		
		for (dy=-6; dy<=6; dy++) {
			y=sy+dy;
			if (y<2 || y>254) continue;

			if (rand()%6) continue;

			m=x+y*256;

			if (emf->field[m+1].sprite[2] ||
			    emf->field[m-1].sprite[2] ||
			    emf->field[m+256].sprite[2] ||
			    emf->field[m-256].sprite[2])
				db_pents_halfwall(x,y);			
		}
	}
	mfile_inc_undo(emf);
}
#endif



