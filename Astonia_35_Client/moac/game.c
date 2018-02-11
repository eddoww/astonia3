#include <windows.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define ISCLIENT
#include "main.h"
#include "dd.h"
#include "client.h"
#include "sprite.h"
#include "gui.h"
#include "spell.h"
#include "edit.h"
#include "sound.h"

// extern

extern int mapsel;
extern int itmsel;
extern int chrsel;
extern unsigned int now;

extern int lightquality;
extern int nocut;
extern int allcut;

extern int mapoffx,mapoffy;
extern int reduce_light;

int fsprite_cnt=0,f2sprite_cnt=0,gsprite_cnt=0,g2sprite_cnt=0,isprite_cnt=0,csprite_cnt=0;
int qs_time=0,dg_time=0,ds_time=0;

int get_sink(int mn,struct map *cmap);

// dl_copysprite

#define DL_STEP 128

#define DLC_STRIKE      1
#define DLC_NUMBER	2
#define DLC_DUMMY	3       // used to get space in the list to reduce compares ;-)
#define DLC_PIXEL	4
#define DLC_BLESS	5
#define DLC_POTION	6
#define DLC_RAIN	7
#define DLC_PULSE	8
#define DLC_PULSEBACK	9
#define DLC_HEAL	10

struct dl
{
        int layer;
        int x,y,h;      // scrx=x scry=y-h sorted bye x,y ;) normally used for height, but also misused to place doors right
        // int movy;

        DDFX ddfx;

        // functions to call
        char call;
        int call_x1,call_y1,call_x2,call_y2,call_x3;
};

typedef struct dl DL;

DL *dllist=NULL;
DL **dlsort=NULL;
int dlused=0,dlmax=0;
int stat_dlsortcalls,stat_dlused;

DL *dl_next(void)
{
        int d,diff;
        DL *rem;

        if (dlused==dlmax) {
                rem=dllist;
                dllist=xrealloc(dllist,(dlmax+DL_STEP)*sizeof(DL),MEM_DL);
                dlsort=xrealloc(dlsort,(dlmax+DL_STEP)*sizeof(DL *),MEM_DL);
                diff=(unsigned char *)dllist-(unsigned char *)rem;
                for (d=0; d<dlmax; d++) dlsort[d]=(DL *)(((unsigned char *)(dlsort[d]))+diff);
                for (d=dlmax; d<dlmax+DL_STEP; d++) dlsort[d]=&dllist[d];
                dlmax+=DL_STEP;
        }
        else if (dlused>dlmax) {
                fail("dlused normally shouldn't exceed dlmax - the error is somewhere else ;-)");
                return dlsort[dlused-1];
        }

        dlused++;
        bzero(dlsort[dlused-1],sizeof(DL));

        if (dlused%16==0) {
                dlsort[dlused-1]->call=DLC_DUMMY;
                return dl_next();
        }

	dlsort[dlused-1]->ddfx.sink=0;
	dlsort[dlused-1]->ddfx.scale=100;
        dlsort[dlused-1]->ddfx.cr=dlsort[dlused-1]->ddfx.cg=dlsort[dlused-1]->ddfx.cb=dlsort[dlused-1]->ddfx.clight=dlsort[dlused-1]->ddfx.sat=0;
	dlsort[dlused-1]->ddfx.c1=0;
	dlsort[dlused-1]->ddfx.c2=0;
	dlsort[dlused-1]->ddfx.c3=0;
	dlsort[dlused-1]->ddfx.shine=0;
        return dlsort[dlused-1];
}

DL *dl_next_set(int layer, int sprite, int scrx, int scry, int light)
{
        DL *dl;
        DDFX *ddfx;

	if (sprite>MAXSPRITE || sprite<0) {
		note("trying to add illegal sprite %d in dl_next_set",sprite);
		return NULL;
	}

        ddfx=&(dl=dl_next())->ddfx;

        dl->x=scrx;
        dl->y=scry;
        dl->layer=layer;

        ddfx->sprite=sprite;
	ddfx->ml=ddfx->ll=ddfx->rl=ddfx->ul=ddfx->dl=light;
	ddfx->sink=0;
	ddfx->scale=100;
        ddfx->cr=ddfx->cg=ddfx->cb=ddfx->clight=ddfx->sat=0;
	ddfx->c1=0;
	ddfx->c2=0;
	ddfx->c3=0;
	ddfx->shine=0;

        return dl;
}

DL *dl_call_strike(int layer, int x1, int y1, int h1, int x2, int y2, int h2)
{
        DL *dl;

        dl=dl_next();

        dl->call=DLC_STRIKE;
        dl->layer=layer;
        dl->call_x1=x1;
        dl->call_y1=y1-h1;
        dl->call_x2=x2;
        dl->call_y2=y2-h2;

        if (y1>y2) {
                dl->x=x1;
                dl->y=y1;
        }
        else {
                dl->x=x2;
                dl->y=y2;
        }

        return dl;
}

DL *dl_call_pulseback(int layer, int x1, int y1, int h1, int x2, int y2, int h2)
{
        DL *dl;

        dl=dl_next();

        dl->call=DLC_PULSEBACK;
        dl->layer=layer;
        dl->call_x1=x1;
        dl->call_y1=y1-h1;
        dl->call_x2=x2;
        dl->call_y2=y2-h2;

        if (y1>y2) {
                dl->x=x1;
                dl->y=y1;
        }
        else {
                dl->x=x2;
                dl->y=y2;
        }

        return dl;
}

DL *dl_call_bless(int layer,int x,int y,int ticker,int strength,int front)
{
	DL *dl;

	dl=dl_next();

        dl->call=DLC_BLESS;
	dl->layer=layer;
	
	dl->call_x1=x;
	dl->call_y1=y;
	dl->call_x2=ticker;
	dl->call_y2=strength;
	dl->call_x3=front;
	
	dl->x=x;
	if (front) dl->y=y+8;
	else dl->y=y-8;

	return dl;
}

DL *dl_call_heal(int layer,int x,int y,int start,int front)
{
	DL *dl;

	dl=dl_next();

        dl->call=DLC_HEAL;
	dl->layer=layer;
	
	dl->call_x1=x;
	dl->call_y1=y;
	dl->call_x2=start;
	dl->call_x3=front;

	dl->x=x;
	if (front) dl->y=y+8;
	else dl->y=y-8;

	return dl;
}

DL *dl_call_pulse(int layer,int x,int y,int nr,int size,int color)
{
	DL *dl;

        dl=dl_next();

        dl->call=DLC_PULSE;
	dl->layer=layer;
	
	dl->call_x1=x;
	dl->call_y1=y-20;
	dl->call_x2=nr;
	dl->call_y2=size;
	dl->call_x3=color;
	
	dl->x=x;
	switch(nr) {
		case 0:		dl->x=x+20; dl->y=y+10; break;
		case 1:		dl->x=x+20; dl->y=y-10; break;
		case 2:		dl->x=x-20; dl->y=y-10; break;
		case 3:		dl->x=x-20; dl->y=y+10; break;


	}	

	return dl;
}

DL *dl_call_potion(int layer,int x,int y,int ticker,int strength,int front)
{
	DL *dl;

	dl=dl_next();

        dl->call=DLC_POTION;
	dl->layer=layer;
	
	dl->call_x1=x;
	dl->call_y1=y;
	dl->call_x2=ticker;
	dl->call_y2=strength;
	dl->call_x3=front;
	
	dl->x=x;
	if (front) dl->y=y+8;
	else dl->y=y-8;

	return dl;
}

DL *dl_call_rain(int layer,int x,int y,int nr,int color)
{
	DL *dl;
	int sy;

        x+=((nr/30)%30)+15;
	sy=y+((nr/330)%20)+10;
	y=sy-((nr*2)%60)-60;

        dl=dl_next();

        dl->call=DLC_PIXEL;
	dl->layer=layer;
	
	dl->call_x1=x;
	dl->call_y1=y;
        dl->call_x2=color;
	
	dl->x=x;
	dl->y=sy;

	return dl;
}

DL *dl_call_rain2(int layer,int x,int y,int ticker,int strength,int front)
{
	DL *dl;

        dl=dl_next();

        dl->call=DLC_RAIN;
	dl->layer=layer;
	
	dl->call_x1=x;
	dl->call_y1=y;
        dl->call_x2=ticker;
	dl->call_y2=strength;
	dl->call_x3=front;
	
	dl->x=x;
	if (front) dl->y=y+10;
	else dl->y=y-10;

	return dl;
}

DL *dl_call_number(int layer,int x,int y,int nr)
{
	DL *dl;

	dl=dl_next();

	dl->call=DLC_NUMBER;
	dl->layer=layer;
	dl->call_x1=x;
	dl->call_y1=y;
	dl->call_x2=nr;

	return dl;
}

DL *dl_autogrid(void)
{
        DL *dllast,*dl;

        PARANOIA( if (dlused<2) paranoia("dl_grid_last(): we have no last"); )

        dl=dl_next();

	dllast=dlsort[dlused-1];
        if (dllast->call==DLC_DUMMY) dllast=dlsort[dlused-2];

        memcpy(dl,dllast,sizeof(DL));

        if ((mapaddx+mapaddy+dllast->x+dllast->y)&1) {
                dl->ddfx.grid=DDFX_RIGHTGRID;
                dllast->ddfx.grid=DDFX_LEFTGRID;
        }
        else {
                dl->ddfx.grid=DDFX_LEFTGRID;
                dllast->ddfx.grid=DDFX_RIGHTGRID;
        }

        dl->layer+=GMEGRD_LAYADD;

        return dl;
}

int dl_qcmp(const void *ca,const void *cb)
{
        DL *a,*b;
        int diff;

        stat_dlsortcalls++;

        a=*(DL **)ca;
        b=*(DL **)cb;

	if (a->call==DLC_DUMMY && b->call==DLC_DUMMY) return  0;
        if (a->call==DLC_DUMMY) return -1;
        if (b->call==DLC_DUMMY) return  1;

        diff=a->layer-b->layer;
        if (diff) return diff;

        diff=a->y-b->y;
        if (diff) return diff;

        diff=a->x-b->x;
        if (diff) return diff;

        return a->ddfx.sprite-b->ddfx.sprite;
}

void draw_pixel(int x,int y,int color)
{
        dd_pixel(x,y,color);
}

void dl_play(void)
{
        int d,wasgrid,start;

	start=GetTickCount();
        stat_dlsortcalls=0;
        stat_dlused=dlused;
        qsort(dlsort,dlused,sizeof(DL *),dl_qcmp);
	qs_time+=GetTickCount()-start;

        for (d=0; d<dlused && !quit; d++) {
#ifdef DOSOUND
		sound_mood();
#endif

                if (dlsort[d]->call==0) {
                        dd_copysprite_fx(&dlsort[d]->ddfx,dlsort[d]->x,dlsort[d]->y-dlsort[d]->h);			
                } else {			
                        switch (dlsort[d]->call) {
                                case DLC_STRIKE:
                                        dd_display_strike(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_y2);
                                        break;
				case DLC_NUMBER:
					dd_drawtext_fmt(dlsort[d]->call_x1,dlsort[d]->call_y1,0xffff,DD_CENTER|DD_SMALL|DD_FRAME,"%d",dlsort[d]->call_x2);
					break;
				case DLC_DUMMY:
                                        break;
				case DLC_PIXEL:
					draw_pixel(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2);
					break;
				case DLC_BLESS:
					dd_draw_bless(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_y2,dlsort[d]->call_x3);
					break;
				case DLC_HEAL:
					dd_draw_heal(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_x3);
					break;
				case DLC_POTION:
					dd_draw_potion(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_y2,dlsort[d]->call_x3);
					break;
				case DLC_RAIN:
					dd_draw_rain(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_y2,dlsort[d]->call_x3);
					break;
				case DLC_PULSE:
					dd_draw_curve(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_y2,dlsort[d]->call_x3);
					break;
				case DLC_PULSEBACK:
                                        dd_display_pulseback(dlsort[d]->call_x1,dlsort[d]->call_y1,dlsort[d]->call_x2,dlsort[d]->call_y2);
                                        break;
                        }
                }
        }

        dlused=0;
}

void pre_add(int attick,int sprite,signed char sink,unsigned char freeze,unsigned char grid,unsigned char scale,char cr,char cg,char cb,char light,char sat,int c1,int c2,int c3,int shine,char ml,char ll,char rl,char ul,char dl);

void dl_prefetch(int attick)
{
        int d,wasgrid;

        // qsort(dlsort,dlused,sizeof(DL *),dl_qcmp);

        for (d=0; d<dlused && !quit; d++) {
                if (dlsort[d]->call==0) {
                        pre_add(attick,
                                dlsort[d]->ddfx.sprite,
				dlsort[d]->ddfx.sink,
				dlsort[d]->ddfx.freeze,
				dlsort[d]->ddfx.grid,
				dlsort[d]->ddfx.scale,
				dlsort[d]->ddfx.cr,
				dlsort[d]->ddfx.cg,
				dlsort[d]->ddfx.cb,
				dlsort[d]->ddfx.clight,
				dlsort[d]->ddfx.sat,
				dlsort[d]->ddfx.c1,
				dlsort[d]->ddfx.c2,
				dlsort[d]->ddfx.c3,
				dlsort[d]->ddfx.shine,
				dlsort[d]->ddfx.ml,
				dlsort[d]->ddfx.ll,
				dlsort[d]->ddfx.rl,
				dlsort[d]->ddfx.ul,
				dlsort[d]->ddfx.dl);
                }
        }

        dlused=0;
}

// analyse
QUICK *quick;
int maxquick;

#define MMF_SIGHTBLOCK  (1<<1)  // indicates sight block (set_map_lights)
#define MMF_DOOR        (1<<2)  // a door - helpful when cutting sprites - (set_map_sprites)
#define MMF_CUT         (1<<3)  // indicates cut (set_map_cut)
#define MMF_NOCUT       (1<<4)  // helps not looking twice (set_map_cut)
#define MMF_STRAIGHT_T  (1<<5)  // (set_map_straight)
#define MMF_STRAIGHT_B  (1<<6)  // (set_map_straight)
#define MMF_STRAIGHT_L  (1<<7)  // (set_map_straight)
#define MMF_STRAIGHT_R  (1<<8)  // (set_map_straight)

static void safepix(unsigned short int *ptr, int x, int y, unsigned short int irgb)
{
        if (x<0 || y<0 || x>=XRES || y>=YRES) return;
        ptr[x+y*xres]=rgb2scr[irgb];
}

void set_map_lights(struct map *cmap)
{
        int i,mn;

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];

                if (!(cmap[mn].flags&CMF_VISIBLE)) {
                        cmap[mn].rlight=0;
                        continue;
                }

                cmap[mn].value=0;
		if ((cmap[mn].flags&CMF_LIGHT)==15) cmap[mn].rlight=15;
                else if (reduce_light) {
			cmap[mn].rlight=(cmap[mn].flags&CMF_LIGHT)/4*4;
			//cmap[mn].rlight=(cmap[mn].flags&CMF_LIGHT)/2*2;
		}
		else cmap[mn].rlight=(cmap[mn].flags&CMF_LIGHT);

		if (cmap[mn].rlight!=15) {
			//cmap[mn].rlight+=cmap[mn].dim;	what?
			cmap[mn].rlight=max(0,cmap[mn].rlight);
			cmap[mn].rlight=min(14,cmap[mn].rlight);
		}
                cmap[mn].mmf=0;

                if (cmap[mn].rlight==15) {
                        if (cmap[quick[i].mn[1]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[1]].flags&CMF_LIGHT);
                        if (cmap[quick[i].mn[3]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[3]].flags&CMF_LIGHT);
                        if (cmap[quick[i].mn[5]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[5]].flags&CMF_LIGHT);
                        if (cmap[quick[i].mn[7]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[7]].flags&CMF_LIGHT);

                        if (cmap[mn].rlight==15) {
				if (cmap[quick[i].mn[0]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[0]].flags&CMF_LIGHT);
				if (cmap[quick[i].mn[2]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[2]].flags&CMF_LIGHT);
				if (cmap[quick[i].mn[6]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[6]].flags&CMF_LIGHT);
				if (cmap[quick[i].mn[8]].flags&CMF_VISIBLE) cmap[mn].rlight=min((unsigned)cmap[mn].rlight,cmap[quick[i].mn[8]].flags&CMF_LIGHT);

				if (cmap[mn].rlight==15) {
					cmap[mn].rlight=0;
					continue;
				}
                        }

                        cmap[mn].mmf|=MMF_SIGHTBLOCK;
                }

                cmap[mn].rlight=15-cmap[mn].rlight;
        }
}

void sprites_colorbalance(struct map *cmap,int mn,int r,int g,int b)
{
	cmap[mn].rf.cr=min(120,cmap[mn].rf.cr+r);
	cmap[mn].rf.cg=min(120,cmap[mn].rf.cg+g);
	cmap[mn].rf.cb=min(120,cmap[mn].rf.cb+b);

	cmap[mn].rf2.cr=min(120,cmap[mn].rf2.cr+r);
	cmap[mn].rf2.cg=min(120,cmap[mn].rf2.cg+g);
	cmap[mn].rf2.cb=min(120,cmap[mn].rf2.cb+b);

	cmap[mn].rg.cr=min(120,cmap[mn].rg.cr+r);
	cmap[mn].rg.cg=min(120,cmap[mn].rg.cg+g);
	cmap[mn].rg.cb=min(120,cmap[mn].rg.cb+b);

	cmap[mn].rg2.cr=min(120,cmap[mn].rg2.cr+r);
	cmap[mn].rg2.cg=min(120,cmap[mn].rg2.cg+g);
	cmap[mn].rg2.cb=min(120,cmap[mn].rg2.cb+b);

	cmap[mn].ri.cr=min(120,cmap[mn].ri.cr+r);
	cmap[mn].ri.cg=min(120,cmap[mn].ri.cg+g);
	cmap[mn].ri.cb=min(120,cmap[mn].ri.cb+b);

	cmap[mn].rc.cr=min(120,cmap[mn].rc.cr+r);
	cmap[mn].rc.cg=min(120,cmap[mn].rc.cg+g);
	cmap[mn].rc.cb=min(120,cmap[mn].rc.cb+b);
}

void xrect_colorbalance(struct map *cmap,int mn,int r,int g,int b,int dist)
{
	static unsigned char seen[MAPDX*MAPDY];

        if (dist==6) bzero(seen,sizeof(seen));
	
	if (seen[mn]>=dist) return;
        if (seen[mn]) sprites_colorbalance(cmap,mn,-r*seen[mn],-g*seen[mn],-b*seen[mn]);
	sprites_colorbalance(cmap,mn,r*dist,g*dist,b*dist);
	seen[mn]=dist;
	
	if ((cmap[mn].flags&CMF_LIGHT)==15) return;
	if (dist<1) return;

        xrect_colorbalance(cmap,mn+1,r,g,b,dist-1);
	xrect_colorbalance(cmap,mn-1,r,g,b,dist-1);
	xrect_colorbalance(cmap,mn+MAPDX,r,g,b,dist-1);
	xrect_colorbalance(cmap,mn-MAPDX,r,g,b,dist-1);
}

#define RANDOM(a)	(rand()%(a))
#define MAXBUB		100
struct bubble
{
	int type;
	int origx,origy;
	int cx,cy;
	int state;
};

struct bubble bubble[MAXBUB];


void add_bubble(int x,int y,int h)
{
        int n;
	int offx,offy;

	mtos(originx,originy,&offx,&offy);
	offx-=mapaddx*2; offy-=mapaddx*2;

	for (n=0; n<MAXBUB; n++) {
		if (!bubble[n].state) {
			bubble[n].state=1;
			bubble[n].origx=x+offx;
			bubble[n].origy=y+offy;
			bubble[n].cx=x+offx;
			bubble[n].cy=y-h+offy;
			bubble[n].type=RANDOM(3);
			//addline("added bubble at %d,%d",offx,offy);
			return;
		}
	}
}

void show_bubbles(void)
{
	int n,spr,offx,offy;
	DL *dl;
	//static int oo=0;

	mtos(originx,originy,&offx,&offy);
	offx-=mapaddx*2; offy-=mapaddy*2;
	//if (oo!=mapaddx) addline("shown bubble at %d,%d %d,%d",offx,offy,oo=mapaddx,mapaddy);

	for (n=0; n<MAXBUB; n++) {
		if (!bubble[n].state) continue;

		spr=(bubble[n].state-1)%6;
		if (spr>3) spr=3-(spr-3);
		spr+=bubble[n].type*3;

		dl=dl_next_set(GME_LAY,1140+spr,bubble[n].cx-offx,bubble[n].origy-offy,DDFX_NLIGHT);
		dl->h=bubble[n].origy-bubble[n].cy;
		bubble[n].state++;
		bubble[n].cx+=2-RANDOM(5);
		bubble[n].cy-=1+RANDOM(3);
		if (bubble[n].cy<1) bubble[n].state=0;
		if (bubble[n].state>50) bubble[n].state=0;		
	}

}


void set_map_sprites(struct map *cmap, int attick)
{
        int i,mn;

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];

                if (!cmap[mn].rlight) continue;

                if (cmap[mn].gsprite) cmap[mn].rg.sprite=trans_asprite(mn,cmap[mn].gsprite,attick,&cmap[mn].rg.scale,&cmap[mn].rg.cr,&cmap[mn].rg.cg,&cmap[mn].rg.cb,&cmap[mn].rg.light,&cmap[mn].rg.sat,&cmap[mn].rg.c1,&cmap[mn].rg.c2,&cmap[mn].rg.c3,&cmap[mn].rg.shine); else cmap[mn].rg.sprite=0;
                if (cmap[mn].fsprite) cmap[mn].rf.sprite=trans_asprite(mn,cmap[mn].fsprite,attick,&cmap[mn].rf.scale,&cmap[mn].rf.cr,&cmap[mn].rf.cg,&cmap[mn].rf.cb,&cmap[mn].rf.light,&cmap[mn].rf.sat,&cmap[mn].rf.c1,&cmap[mn].rf.c2,&cmap[mn].rf.c3,&cmap[mn].rf.shine); else cmap[mn].rf.sprite=0;
		if (cmap[mn].gsprite2) cmap[mn].rg2.sprite=trans_asprite(mn,cmap[mn].gsprite2,attick,&cmap[mn].rg2.scale,&cmap[mn].rg2.cr,&cmap[mn].rg2.cg,&cmap[mn].rg2.cb,&cmap[mn].rg2.light,&cmap[mn].rg2.sat,&cmap[mn].rg2.c1,&cmap[mn].rg2.c2,&cmap[mn].rg2.c3,&cmap[mn].rg2.shine); else cmap[mn].rg2.sprite=0;
                if (cmap[mn].fsprite2) cmap[mn].rf2.sprite=trans_asprite(mn,cmap[mn].fsprite2,attick,&cmap[mn].rf2.scale,&cmap[mn].rf2.cr,&cmap[mn].rf2.cg,&cmap[mn].rf2.cb,&cmap[mn].rf2.light,&cmap[mn].rf2.sat,&cmap[mn].rf2.c1,&cmap[mn].rf2.c2,&cmap[mn].rf2.c3,&cmap[mn].rf2.shine); else cmap[mn].rf2.sprite=0;

                if (cmap[mn].isprite) {
                        cmap[mn].ri.sprite=trans_asprite(mn,cmap[mn].isprite,attick,&cmap[mn].ri.scale,&cmap[mn].ri.cr,&cmap[mn].ri.cg,&cmap[mn].ri.cb,&cmap[mn].ri.light,&cmap[mn].ri.sat,&cmap[mn].ri.c1,&cmap[mn].ri.c2,&cmap[mn].ri.c3,&cmap[mn].ri.shine);
			if (cmap[mn].ic1 || cmap[mn].ic2 || cmap[mn].ic3) {
				cmap[mn].ri.c1=cmap[mn].ic1;
				cmap[mn].ri.c2=cmap[mn].ic2;
				cmap[mn].ri.c3=cmap[mn].ic3;
			}

			if (is_door_sprite(cmap[mn].ri.sprite)) cmap[mn].mmf|=MMF_DOOR;
		} else cmap[mn].ri.sprite=0;
                if (cmap[mn].csprite) trans_csprite(mn,cmap,attick);
        }

#ifdef COLORPLAY
	for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];

                if (!cmap[mn].rlight) continue;

		if (cmap[mn].isprite && cmap[mn].ri.sprite==50493) {
			xrect_colorbalance(cmap,mn,0,0,3,6);
		}
		if (cmap[mn].isprite && cmap[mn].ri.sprite==50492) {
			xrect_colorbalance(cmap,mn,0,3,0,6);
		}
		if (cmap[mn].isprite && cmap[mn].ri.sprite==50491) {
			xrect_colorbalance(cmap,mn,3,0,0,6);
		}
        }
#endif
}

static void set_map_cut_fill(int i, int panic,struct map *cmap)
{
        int mn;

        if (panic>2*MAPDX*+2*MAPDY+1) { if (!panic_reached++) note("panic(%d) reached in _mmf_sameindoor_rec()",panic); quit=1; return; }

        mn=quick[i].mn[4];
        if (!mn) return;

        if (cmap[mn].mmf&MMF_CUT) return;

        if (!cmap[mn].rlight) return;
        if (!(cmap[mn].mmf&(MMF_SIGHTBLOCK|MMF_DOOR)) && is_cut_sprite(cmap[mn].rf.sprite)==(int)cmap[mn].rf.sprite &&
	    is_cut_sprite(cmap[mn].rf2.sprite)==(int)cmap[mn].rf2.sprite) return;

        if (panic==0 && !cmap[quick[i].mn[0]].rlight) return;

        cmap[mn].mmf|=MMF_CUT;

        set_map_cut_fill(quick[i].qi[7],panic+1,cmap);
        set_map_cut_fill(quick[i].qi[5],panic+1,cmap);
}

static int cut_negative(struct map *cmap, int qi)       // __AA__
{
        if ((cmap[quick[qi].mn[3]].mmf&MMF_SIGHTBLOCK) && !(cmap[quick[qi].mn[3]].mmf&MMF_CUT)) return 0;
        if ((cmap[quick[qi].mn[1]].mmf&MMF_SIGHTBLOCK) && !(cmap[quick[qi].mn[1]].mmf&MMF_CUT)) return 0;
        if ((cmap[quick[qi].mn[0]].mmf&MMF_SIGHTBLOCK) && !(cmap[quick[qi].mn[0]].mmf&MMF_CUT)) return 0;
        return 1;
}

static void set_map_cut_old_old(struct map *cmap)
{
        int i;

        if (nocut) return;

        // set flags
        for (i=0; i<maxquick; i++) set_map_cut_fill(i,0,cmap);

        // change sprites
        for (i=0; i<maxquick; i++) {
                if (cmap[quick[i].mn[4]].mmf&MMF_CUT) {
                        int tmp;
                        /* __AA__
                        cmap[quick[i].mn[4]].rfsprite=is_cut_sprite(cmap[quick[i].mn[4]].rfsprite);
                        cmap[quick[i].mn[4]].rfsprite2=is_cut_sprite(cmap[quick[i].mn[4]].rfsprite2);   // __AA__ ??? richtig ?
                        cmap[quick[i].mn[4]].risprite=is_cut_sprite(cmap[quick[i].mn[4]].risprite);
                        */

                        // __AA__ new version
                        tmp=is_cut_sprite(cmap[quick[i].mn[4]].rf.sprite);
                        if (tmp>=0) cmap[quick[i].mn[4]].rf.sprite=tmp;
                        else if (cut_negative(cmap,i)) cmap[quick[i].mn[4]].rf.sprite=-tmp;

                        tmp=is_cut_sprite(cmap[quick[i].mn[4]].rf2.sprite);
                        if (tmp>=0) cmap[quick[i].mn[4]].rf2.sprite=tmp;
                        else if (cut_negative(cmap,i)) cmap[quick[i].mn[4]].rf2.sprite=-tmp;

                        tmp=is_cut_sprite(cmap[quick[i].mn[4]].ri.sprite);
                        if (tmp>=0) cmap[quick[i].mn[4]].ri.sprite=tmp;
                        else if (cut_negative(cmap,i)) cmap[quick[i].mn[4]].ri.sprite=-tmp;
                }
        }

}

static void set_map_cut_old(struct map *cmap)
{
        int i,mn,mn2,i2;
	unsigned int tmp;

        for (i=0; i<maxquick; i++) {
                tmp=abs(is_cut_sprite(cmap[quick[i].mn[4]].rf.sprite));
		if (tmp!=cmap[quick[i].mn[4]].rf.sprite) cmap[quick[i].mn[4]].rf.sprite=tmp;
		
		tmp=abs(is_cut_sprite(cmap[quick[i].mn[4]].rf2.sprite));
		if (tmp!=cmap[quick[i].mn[4]].rf2.sprite) cmap[quick[i].mn[4]].rf2.sprite=tmp;
		
		tmp=abs(is_cut_sprite(cmap[quick[i].mn[4]].ri.sprite));
		if (tmp!=cmap[quick[i].mn[4]].ri.sprite) cmap[quick[i].mn[4]].ri.sprite=tmp;
        }	
}

static void set_map_cut(struct map *cmap)
{
        int i,mn,mn2,i2;
	unsigned int tmp;

        if (nocut) return;
	if (allcut) { set_map_cut_old(cmap); return; }

        // set flags
        //for (i=0; i<maxquick; i++) set_map_cut_fill(i,0,cmap);
	
	// change sprites
        for (i=0; i<maxquick; i++) {
                mn=quick[i].mn[0];
		i2=quick[i].qi[0];
		if (mn) mn2=quick[i2].mn[0]; else mn2=0;

		if ((!mn || !cmap[mn].rlight ||
		     ((unsigned)abs(is_cut_sprite(cmap[mn].rf.sprite))!=cmap[mn].rf.sprite && is_cut_sprite(cmap[mn].rf.sprite)>0) ||
		     ((unsigned)abs(is_cut_sprite(cmap[mn].rf2.sprite))!=cmap[mn].rf2.sprite && is_cut_sprite(cmap[mn].rf2.sprite)>0) ||
		     ((unsigned)abs(is_cut_sprite(cmap[mn].ri.sprite))!=cmap[mn].ri.sprite  && is_cut_sprite(cmap[mn].ri.sprite)>0)) &&
		    (!mn2 || !cmap[mn2].rlight ||
		     ((unsigned)abs(is_cut_sprite(cmap[mn2].rf.sprite))!=cmap[mn2].rf.sprite && is_cut_sprite(cmap[mn2].rf.sprite)>0) ||
		     ((unsigned)abs(is_cut_sprite(cmap[mn2].rf2.sprite))!=cmap[mn2].rf.sprite && is_cut_sprite(cmap[mn2].rf2.sprite)>0) ||
		     ((unsigned)abs(is_cut_sprite(cmap[mn2].ri.sprite))!=cmap[mn2].ri.sprite) && is_cut_sprite(cmap[mn2].ri.sprite)>0)) continue;

                cmap[quick[i].mn[4]].mmf|=MMF_CUT;
        }	
	for (i=0; i<maxquick; i++) {
		if (!allcut && !(cmap[quick[i].mn[4]].mmf&MMF_CUT)) continue;

		if (is_cut_sprite(cmap[quick[i].mn[4]].rf.sprite)<0 &&
		    ((!(cmap[quick[i].mn[1]].mmf&MMF_CUT) && is_cut_sprite(cmap[quick[i].mn[1]].rf.sprite)) ||
		     (!(cmap[quick[i].mn[3]].mmf&MMF_CUT) && is_cut_sprite(cmap[quick[i].mn[3]].rf.sprite)))) continue;
		
                tmp=abs(is_cut_sprite(cmap[quick[i].mn[4]].rf.sprite));
		if (tmp!=cmap[quick[i].mn[4]].rf.sprite) cmap[quick[i].mn[4]].rf.sprite=tmp;
		
		tmp=abs(is_cut_sprite(cmap[quick[i].mn[4]].rf2.sprite));
		if (tmp!=cmap[quick[i].mn[4]].rf2.sprite) cmap[quick[i].mn[4]].rf2.sprite=tmp;
		
		tmp=abs(is_cut_sprite(cmap[quick[i].mn[4]].ri.sprite));
		if (tmp!=cmap[quick[i].mn[4]].ri.sprite) cmap[quick[i].mn[4]].ri.sprite=tmp;
        }	
}

void set_map_straight(struct map *cmap)
{
        int i,mn,mna,vl,vr,vt,vb,wl,wr,wt,wb;

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];

                if (!cmap[mn].rlight) continue;

                if ((mna=quick[i].mn[3])!=0) { vl=cmap[mna].rlight; wl=cmap[mna].mmf&MMF_SIGHTBLOCK; } else vl=wl=0;
                if ((mna=quick[i].mn[5])!=0) { vr=cmap[mna].rlight; wr=cmap[mna].mmf&MMF_SIGHTBLOCK; } else vr=wr=0;
                if ((mna=quick[i].mn[1])!=0) { vt=cmap[mna].rlight; wt=cmap[mna].mmf&MMF_SIGHTBLOCK; } else vt=wt=0;
                if ((mna=quick[i].mn[7])!=0) { vb=cmap[mna].rlight; wb=cmap[mna].mmf&MMF_SIGHTBLOCK; } else vb=wb=0;

                if (!(cmap[mn].mmf&MMF_SIGHTBLOCK)) {
                        if ((!vl || wl) && (!vb || wb) &&   vt        &&   vr        && (!wl || !wb) ) cmap[mn].mmf|=MMF_STRAIGHT_L;
                        if (  vl        &&   vb        && (!vt || wt) && (!vr || wr) && (!wt || !wr) ) cmap[mn].mmf|=MMF_STRAIGHT_R;
                        if ((!vl || wl) &&   vb        && (!vt || wt) &&   vr        && (!wl || !wt) ) cmap[mn].mmf|=MMF_STRAIGHT_T;
                        if (  vl        && (!vb || wb) &&   vt        && (!vr || wr) && (!wb || !wr) ) cmap[mn].mmf|=MMF_STRAIGHT_B;
                }
                else {
                        if (!vt && !vr && !(wl && wb)) cmap[mn].mmf|=MMF_STRAIGHT_R;
                        if (!vb && !vl && !(wr && wt)) cmap[mn].mmf|=MMF_STRAIGHT_L;
                }

        }

}

void set_map_values(struct map *cmap, int attick)
{
        set_map_lights(cmap);
        set_map_sprites(cmap,attick);
        set_map_cut(cmap);
        set_map_straight(cmap);
}

static int trans_x_off(int x,int y)
{
	int xoff,yoff;

	xoff=x%1024-512;
	yoff=y%1024-512;

	xoff=xoff*20/512;
	yoff=-yoff*20/512;

	return (xoff/2)+(yoff/2);
}

static int trans_y_off(int x,int y)
{
	int xoff,yoff;

	xoff=x%1024-512;
	yoff=y%1024-512;

	xoff=xoff*20/512;
	yoff=-yoff*20/512;

        return (xoff/4)-(yoff/4);
}

static int trans_x(int frx,int fry,int tox,int toy,int step,int start)
{
	int x,y,dx,dy;

	dx=(tox-frx);
	dy=(toy-fry);

	if (abs(dx)>abs(dy)) { dy=dy*step/abs(dx); dx=dx*step/abs(dx); }
	else { dx=dx*step/abs(dy); dy=dy*step/abs(dy); }

	x=frx*1024+512;
	y=fry*1024+512;

	x+=dx*(tick-start);
	y+=dy*(tick-start);

	x-=(originx-DIST)*1024;
	y-=(originy-DIST)*1024;

        return (x-y)*20/1024+mapoffx+mapaddx;
}

static int trans_y(int frx,int fry,int tox,int toy,int step,int start)
{
	int x,y,dx,dy;

	dx=(tox-frx);
	dy=(toy-fry);

	if (abs(dx)>abs(dy)) { dy=dy*step/abs(dx); dx=dx*step/abs(dx); }
	else { dx=dx*step/abs(dy); dy=dy*step/abs(dy); }

	x=frx*1024+512;
	y=fry*1024+512;

	x+=dx*(tick-start);
	y+=dy*(tick-start);

	x-=(originx-DIST)*1024;
	y-=(originy-DIST)*1024;

	return (x+y)*10/1024+mapoffy+mapaddy /*MR*/ - FDY/2;
}

static void display_game_spells(void)
{
        int i,mn,scrx,scry,x,y,dx,sprite,start;
        int nr,fn,e;
        int mapx,mapy,mna,x1,y1,x2,y2,h1,h2,size,n;
        DL *dl;
        int light;
        float alpha;

	start=GetTickCount();

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];
                scrx=mapaddx+quick[i].cx;
                scry=mapaddy+quick[i].cy;
		light=map[mn].rlight;

                if (!light) continue;

		map[mn].sink=0;

		if (map[mn].gsprite>=59405 && map[mn].gsprite<=59413) map[mn].sink=8;
		if (map[mn].gsprite>=59414 && map[mn].gsprite<=59422) map[mn].sink=16;
		if (map[mn].gsprite>=59423 && map[mn].gsprite<=59431) map[mn].sink=24;
		if (map[mn].gsprite>=20815 && map[mn].gsprite<=20823) map[mn].sink=36;

                for (e=0; e<68; e++) {
			
			if (e<4) {
				if ((fn=map[mn].ef[e])!=0) nr=find_ceffect(fn);
				else continue;
			} else if (map[mn].cn) {
				for (nr=e-4; nr<MAXEF; nr++) {
					if (ueffect[nr] && is_char_ceffect(ceffect[nr].generic.type) && (unsigned)ceffect[nr].flash.cn==map[mn].cn) break;
				}
				if (nr==MAXEF) break;
				else e=nr+4;
			} else break;;

			if (nr!=-1) {
				//addline("%d %d %d %d %d",fn,e,nr,ceffect[nr].generic.type,map[mn].cn);
                                //if (e>3) addline("%d: effect %d at %d",tick,ceffect[nr].generic.type,nr);
                                switch(ceffect[nr].generic.type) {

					case 1:	// shield
						if (tick-ceffect[nr].shield.start<3) {
							dl=dl_next_set(GME_LAY,1002+tick-ceffect[nr].shield.start,scrx+map[mn].xadd,scry+map[mn].yadd+1,DDFX_NLIGHT);
							if (!dl) { note("error in shield #1"); break; }
						}
                                                break;

                                        case 5:	// flash
                                                x=scrx+map[mn].xadd + cos(2*M_PI*(now%1000)/1000.0)*16;
                                                y=scry+map[mn].yadd + sin(2*M_PI*(now%1000)/1000.0)*8;
                                                dl=dl_next_set(GME_LAY,1006,x,y,DDFX_NLIGHT); // shade
						if (!dl) { note("error in flash #1"); break; }
                                                dl=dl_next_set(GME_LAY,1005,x,y,DDFX_NLIGHT); // small lightningball
						if (!dl) { note("error in flash #2"); break; }
                                                dl->h=50;
                                                break;

                                        case 3:	// strike
                                                // set source coords - mna is source
                                                mapx=ceffect[nr].strike.x-originx+DIST;
                                                mapy=ceffect[nr].strike.y-originy+DIST;
                                                mna=mapmn(mapx,mapy);
                                                mtos(mapx,mapy,&x1,&y1);

                                                if (map[mna].cn==0) { // no char, so source should be a lightning ball
                                                        h1=20;
                                                }
                                                else {  // so i guess we spell from a char (use the flying ball as source)
                                                        x1=x1+map[mna].xadd + cos(2*M_PI*(now%1000)/1000.0)*16;
                                                        y1=y1+map[mna].yadd + sin(2*M_PI*(now%1000)/1000.0)*8;
                                                        h1=50;
                                                }

                                                // set target coords - mn is target
                                                x2=scrx+map[mn].xadd;
                                                y2=scry+map[mn].yadd;
                                                h2=25;

						// sanity check
						if (abs(x1-x2)+abs(y1-y2)>200) break;

                                                // mn is target
                                                dl_call_strike(GME_LAY,x1,y1,h1,x2,y2,h2);
						//addline("strike %d,%d to %d,%d",x1,y1,x2,y2);
                                                break;

					case 7:	// explosion
						if (tick-ceffect[nr].explode.start<8) {
							x=scrx;
							y=scry;
	
							if (ceffect[nr].explode.base>=50450 && ceffect[nr].explode.base<=50454) {
								dx=15;
								sprite=50450;
							} else {
								dx=15;
								sprite=ceffect[nr].explode.base;
							}
	
							dl=dl_next_set(GME_LAY2,min(sprite+tick-ceffect[nr].explode.start,sprite+7),x,y-dx,DDFX_NLIGHT);
	
							if (!dl) { note("error in explosion #1"); break; }
							dl->h=dx;
							if (ceffect[nr].explode.base<50450 || ceffect[nr].explode.base>50454) {
								if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }							
								break;
							}
							if (ceffect[nr].explode.base==50451) dl->ddfx.c1=IRGB(16,12,0);
	
							dl=dl_next_set(GME_LAY2,min(sprite+8+tick-ceffect[nr].explode.start,sprite+15),x,y+dx,DDFX_NLIGHT);
							if (!dl) { note("error in explosion #2"); break; }
							dl->h=dx;
							if (ceffect[nr].explode.base==50451) dl->ddfx.c1=IRGB(16,12,0);
						}

                                                break;
					
					case 8:	// warcry
                                                alpha=-2*M_PI*(now%1000)/1000.0;

                                                for (x1=0; x1<4; x1++) {
                                                        x=scrx+map[mn].xadd+cos(alpha+x1*M_PI/2)*15;
                                                        y=scry+map[mn].yadd+sin(alpha+x1*M_PI/2)*15/2;
                                                        dl=dl_next_set(GME_LAY,1020+(tick/4+x1)%4,x,y,DDFX_NLIGHT);
							if (!dl) { note("error in warcry #1"); break; }
                                                        dl->h=40;
                                                }


                                                break;
                                        case 9:	// bless
                                                dl_call_bless(GME_LAY,scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].bless.stop-tick,ceffect[nr].bless.strength,1);
						dl_call_bless(GME_LAY,scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].bless.stop-tick,ceffect[nr].bless.strength,0);
                                                break;

					case 10:// heal
                                                //dl=dl_next_set(GME_LAY,50114,scrx+map[mn].xadd,scry+map[mn].yadd+1,DDFX_NLIGHT);
						dl_call_heal(GME_LAY,scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].heal.start,1);
						dl_call_heal(GME_LAY,scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].heal.start,0);
						if (!dl) { note("error in heal #1"); break; }
                                                break;
					
                                        case 12:// burn //
                                                x=scrx+map[mn].xadd;
                                                y=scry+map[mn].yadd-3;
                                                dl=dl_next_set(GME_LAY,1024+((tick)%10),x,y,DDFX_NLIGHT); // burn behind
						if (!dl) { note("error in bun #1"); break; }
						if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
						
                                                x=scrx+map[mn].xadd;
                                                y=scry+map[mn].yadd+3;
                                                dl=dl_next_set(GME_LAY,1024+((5+tick)%10),x,y,DDFX_NLIGHT); // small lightningball
						if (!dl) { note("error in burn #2"); break; }
						if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
						
                                                break;
					case 13:// mist
                                                if (tick-ceffect[nr].mist.start<24) {
							x=scrx;
							y=scry;
							dl=dl_next_set(GME_LAY+1,1034+(tick-ceffect[nr].mist.start),x,y,DDFX_NLIGHT);
							if (!dl) { note("error in mist #1"); break; }
						}
                                                break;

					case 14:	// potion
						dl_call_potion(GME_LAY,scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].potion.stop-tick,ceffect[nr].potion.strength,1);
						dl_call_potion(GME_LAY,scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].potion.stop-tick,ceffect[nr].potion.strength,0);						
                                                break;

					case 15:	// earth-rain
                                                dl_call_rain2(GME_LAY,scrx,scry,tick,ceffect[nr].earthrain.strength,1);
						dl_call_rain2(GME_LAY,scrx,scry,tick,ceffect[nr].earthrain.strength,0);
						break;
					case 16:	// earth-mud
                                                mapx=mn%MAPDX+originx-MAPDX/2;
						mapy=mn/MAPDX+originy-MAPDY/2;
						dl=dl_next_set(GME_LAY-1,50254+(mapx%3)+((mapy/3)%3),scrx,scry,light);
						if (!dl) { note("error in mud #1"); break; }
						map[mn].sink=12;
						break;
					case 21:	// pulse
						size=((tick-ceffect[nr].pulse.start)%6)*4+10;
						for (n=0; n<4; n++) {
							dl_call_pulse(GME_LAY,scrx,scry-3,n,size+1,IRGB(0,12,0));
							dl_call_pulse(GME_LAY,scrx,scry-2,n,size-2,IRGB(0,16,0));
							dl_call_pulse(GME_LAY,scrx,scry-1,n,size-1,IRGB(0,20,0));
							dl_call_pulse(GME_LAY,scrx,scry,n,size,IRGB(16,31,16));
						}
						break;
					case 22:	// pulseback
                                                // set source coords - mna is source
                                                mapx=ceffect[nr].pulseback.x-originx+DIST;
                                                mapy=ceffect[nr].pulseback.y-originy+DIST;
                                                mna=mapmn(mapx,mapy);
                                                mtos(mapx,mapy,&x1,&y1);

                                                if (map[mna].cn==0) { // no char, so source should be a lightning ball
                                                        h1=20;
                                                }
                                                else {  // so i guess we spell from a char (use the flying ball as source)
                                                        h1=50;
                                                }

                                                // set target coords - mn is target
                                                x2=scrx+map[mn].xadd;
                                                y2=scry+map[mn].yadd;
                                                h2=25;

						// sanity check
						if (abs(x1-x2)+abs(y1-y2)>200) break;

                                                // mn is target
                                                dl_call_pulseback(GME_LAY,x1,y1,h1,x2,y2,h2);
						//addline("strike %d,%d to %d,%d",x1,y1,x2,y2);
                                                break;
					case 23:	// fire ringlet
						if (tick-ceffect[nr].firering.start<7) {
							dl=dl_next_set(GME_LAY,51601+(tick-ceffect[nr].firering.start)*2,scrx,scry+20,DDFX_NLIGHT);
							dl->h=40;
							if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
							dl=dl_next_set(GME_LAY,51600+(tick-ceffect[nr].firering.start)*2,scrx,scry,DDFX_NLIGHT);
							dl->h=20;
							if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
						}
						break;
					case 24:	// forever blowing bubbles...
						if (ceffect[nr].bubble.yoff) add_bubble(scrx+map[mn].xadd,scry+map[mn].yadd,ceffect[nr].bubble.yoff);
						else add_bubble(scrx,scry,ceffect[nr].bubble.yoff);
						break;
                                }
                        }
                }
        }	

	ds_time=GetTickCount()-start;
}

static void display_game_spells2(void)
{
        int x,y,nr,mapx,mapy,mn;
        DL *dl;

        for (nr=0; nr<MAXEF; nr++) {
		if (!ueffect[nr]) continue;
		
		switch(ceffect[nr].generic.type) {
			case 2:	// ball
				x=trans_x(ceffect[nr].ball.frx,ceffect[nr].ball.fry,ceffect[nr].ball.tox,ceffect[nr].ball.toy,128,ceffect[nr].ball.start);
				y=trans_y(ceffect[nr].ball.frx,ceffect[nr].ball.fry,ceffect[nr].ball.tox,ceffect[nr].ball.toy,128,ceffect[nr].ball.start);
				
				stom(x,y,&mapx,&mapy);
				mn=mapmn(mapx,mapy);
				if (!map[mn].rlight) break;
				
                                dl=dl_next_set(GME_LAY,1008,x,y,DDFX_NLIGHT);      // shade
				if (!dl) { note("error in ball #1"); break; }
				dl=dl_next_set(GME_LAY,1000,x,y,DDFX_NLIGHT);   // lightningball
				if (!dl) { note("error in ball #2"); break; }
				dl->h=20;
				break;
			case 4:	// fireball
				x=trans_x(ceffect[nr].fireball.frx,ceffect[nr].fireball.fry,ceffect[nr].fireball.tox,ceffect[nr].fireball.toy,1024,ceffect[nr].fireball.start);
				y=trans_y(ceffect[nr].fireball.frx,ceffect[nr].fireball.fry,ceffect[nr].fireball.tox,ceffect[nr].fireball.toy,1024,ceffect[nr].fireball.start);
				
				stom(x,y,&mapx,&mapy);
				mn=mapmn(mapx,mapy);
				if (!map[mn].rlight) break;

                                dl=dl_next_set(GME_LAY,1007,x,y,DDFX_NLIGHT);      // shade
				if (!dl) { note("error in fireball #1"); break; }
				if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
                                dl=dl_next_set(GME_LAY,1001,x,y,DDFX_NLIGHT);   // fireball
				if (!dl) { note("error in fireball #2"); break; }
				if (map[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
				dl->h=20;
				break;
			case 17:	// edemonball
				x=trans_x(ceffect[nr].edemonball.frx,ceffect[nr].edemonball.fry,ceffect[nr].edemonball.tox,ceffect[nr].edemonball.toy,256,ceffect[nr].edemonball.start);
				y=trans_y(ceffect[nr].edemonball.frx,ceffect[nr].edemonball.fry,ceffect[nr].edemonball.tox,ceffect[nr].edemonball.toy,256,ceffect[nr].edemonball.start);
				
				stom(x,y,&mapx,&mapy);
				mn=mapmn(mapx,mapy);
				if (!map[mn].rlight) break;

                                dl=dl_next_set(GME_LAY,50281,x,y,DDFX_NLIGHT);      // shade
				if (!dl) { note("error in edemonball #1"); break; }
                                dl=dl_next_set(GME_LAY,50264,x,y,DDFX_NLIGHT);   // edemonball
				if (!dl) { note("error in edemonball #2"); break; }
				dl->h=10;
				
				if (ceffect[nr].edemonball.base==1) dl->ddfx.c1=IRGB(16,12,0);
                                //else if (ceffect[nr].edemonball.base==2) dl->ddfx.tint=EDEMONBALL_TINT3;
				//else if (ceffect[nr].edemonball.base==3) dl->ddfx.tint=EDEMONBALL_TINT4;
				
				break;
		}
	}
}

static char *roman(int nr)
{
	int h,t,o;
	static char buf[80];
	char *ptr=buf;
	
	if (nr>399) return "???";
	
        h=nr/100;
	nr-=h*100;
	t=nr/10;
	nr-=t*10;
	o=nr;

	while (h) { *ptr++='C'; h--; }

	if (t==9) { *ptr++='X'; *ptr++='C'; t=0; }
	if (t>4) { *ptr++='L'; t-=5; }
	if (t==4) { *ptr++='X'; *ptr++='L'; t=0; }
	while (t) { *ptr++='X'; t--; }
	
	if (o==9) { *ptr++='I'; *ptr++='X'; o=0; }
	if (o>4) { *ptr++='V'; o-=5; }
	if (o==4) { *ptr++='I'; *ptr++='V'; o=0; }
	while (o) { *ptr++='I'; o--; }

	*ptr=0;

	return buf;
}

int namesize=DD_SMALL;

static void display_game_names(void)
{
        int i,mn,scrx,scry,x,y,col;
	char *sign;
	unsigned short clancolor[33];
	
	clancolor[1]=rgb2scr[IRGB(31, 0, 0)];
	clancolor[2]=rgb2scr[IRGB( 0,31, 0)];
	clancolor[3]=rgb2scr[IRGB( 0, 0,31)];
	clancolor[4]=rgb2scr[IRGB(31,31, 0)];
	clancolor[5]=rgb2scr[IRGB(31, 0,31)];
	clancolor[6]=rgb2scr[IRGB( 0,31,31)];
	clancolor[7]=rgb2scr[IRGB(31,16,16)];
	clancolor[8]=rgb2scr[IRGB(16,16,31)];

	clancolor[ 9]=rgb2scr[IRGB(24, 8, 8)];
	clancolor[10]=rgb2scr[IRGB( 8,24, 8)];
	clancolor[11]=rgb2scr[IRGB( 8, 8,24)];
	clancolor[12]=rgb2scr[IRGB(24,24, 8)];
	clancolor[13]=rgb2scr[IRGB(24, 8,24)];
	clancolor[14]=rgb2scr[IRGB( 8,24,24)];
	clancolor[15]=rgb2scr[IRGB(24,24,24)];
	clancolor[16]=rgb2scr[IRGB(16,16,16)];

	clancolor[17]=rgb2scr[IRGB(31,24,24)];
	clancolor[18]=rgb2scr[IRGB(24,31,24)];
	clancolor[19]=rgb2scr[IRGB(24,24,31)];
	clancolor[20]=rgb2scr[IRGB(31,31,24)];
	clancolor[21]=rgb2scr[IRGB(31,24,31)];
	clancolor[22]=rgb2scr[IRGB(24,31,31)];
	clancolor[23]=rgb2scr[IRGB(31, 8, 8)];
	clancolor[24]=rgb2scr[IRGB( 8, 8,31)];

	clancolor[25]=rgb2scr[IRGB(16, 8, 8)];
	clancolor[26]=rgb2scr[IRGB( 8,16, 8)];
	clancolor[27]=rgb2scr[IRGB( 8, 8,16)];
	clancolor[28]=rgb2scr[IRGB(16,16, 8)];
	clancolor[29]=rgb2scr[IRGB(16, 8,16)];
	clancolor[30]=rgb2scr[IRGB( 8,16,16)];
	clancolor[31]=rgb2scr[IRGB( 8,31, 8)];
	clancolor[32]=rgb2scr[IRGB(31, 8,31)];

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];
                scrx=mapaddx+quick[i].cx;
                scry=mapaddy+quick[i].cy;

                if (!map[mn].rlight) continue;
                if (!map[mn].csprite) continue;
		if (map[mn].gsprite==51066) continue;
		if (map[mn].gsprite==51067) continue;

                x=scrx+map[mn].xadd;
                y=scry+4+map[mn].yadd+get_chr_height(map[mn].csprite)-25+get_sink(mn,map);

                //dd_drawtext(x,y,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,player[map[mn].cn].name);
		col=whitecolor;

		if (player[map[mn].cn].clan) {
			if (player[map[mn].cn].clan<33) col=clancolor[player[map[mn].cn].clan];
			else col=clancolor[player[map[mn].cn].clan-32];
		}

		sign="";
		if (player[map[mn].cn].pk_status==5) sign=" **";
		else if (player[map[mn].cn].pk_status==4) sign=" *";
		else if (player[map[mn].cn].pk_status==3) sign=" ++";
		else if (player[map[mn].cn].pk_status==2) sign=" +";
		else if (player[map[mn].cn].pk_status==1) sign=" -";

		if (namesize!=DD_SMALL) y-=3;
                if (player[map[mn].cn].clan>32) dd_drawtext_fmt(x,y,col,DD_CENTER|namesize|DD_REDFRAME,"%s%s",player[map[mn].cn].name,sign);
		else dd_drawtext_fmt(x,y,col,DD_CENTER|namesize|DD_FRAME,"%s%s",player[map[mn].cn].name,sign);
		

		if (namesize!=DD_SMALL) y+=3;
		y+=12;
                dd_drawtext(x,y,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,roman(player[map[mn].cn].level));


                x-=12;
                y-=6;
                if (map[mn].health>1) {
                        dd_rect(x,y,x+25,y+1,blackcolor);
                        dd_rect(x,y,x+map[mn].health/4,y+1,healthcolor);
                        y++;
                }
                if (map[mn].shield>1) {
                        dd_rect(x,y,x+25,y+1,blackcolor);
                        dd_rect(x,y,x+map[mn].shield/4,y+1,shieldcolor);
                        y++;
                }
                if (map[mn].mana>1) {
                        dd_rect(x,y,x+25,y+1,blackcolor);
                        dd_rect(x,y,x+map[mn].mana/4,y+1,manacolor);
                }		
        }
}

static void display_game_act(void)
{
        int mn,scrx,scry,mapx,mapy;
        DL *dl;
        DDFX *ddfx;
        char *actstr;
        int acttyp;

        // act
        actstr=NULL;

	switch(act) {
		case PAC_MOVE:          acttyp=0; actstr=""; break;
		
                case PAC_FIREBALL:      acttyp=1; actstr="fireball"; break;
		case PAC_BALL:          acttyp=1; actstr="ball"; break;
		case PAC_LOOK_MAP:      acttyp=1; actstr="look"; break;
		case PAC_DROP:          acttyp=1; actstr="drop"; break;
		
                case PAC_TAKE:          acttyp=2; actstr="take"; break;
                case PAC_USE:           acttyp=2; actstr="use"; break;
		
                case PAC_KILL:          acttyp=3; actstr="attack"; break;
		case PAC_HEAL:          acttyp=3; actstr="heal"; break;
		case PAC_BLESS:         acttyp=3; actstr="bless"; break;
		case PAC_FREEZE:        acttyp=3; actstr="freeze"; break;
                case PAC_GIVE:          acttyp=3; actstr="give"; break;

		case PAC_IDLE:          acttyp=-1; break;
                case PAC_MAGICSHIELD:   acttyp=-1; break;
		case PAC_FLASH:         acttyp=-1; break;
		case PAC_WARCRY:        acttyp=-1; break;
		case PAC_BERSERK:       acttyp=-1; break;
                default:                acttyp=-1; break;
	}

        if (acttyp!=-1 && actstr) {
                mn=mapmn(actx-originx+MAPDX/2,acty-originy+MAPDY/2);
                mapx=mn%MAPDX;
                mapy=mn/MAPDX;
                mtos(mapx,mapy,&scrx,&scry);
                if (acttyp==0) {
                        dl_next_set(GNDSEL_LAY,5,scrx,scry,DDFX_NLIGHT);
                        dl_autogrid();
                }
                else dd_drawtext(scrx,scry,textcolor,DD_CENTER|DD_SMALL|DD_FRAME,actstr);
        }
}

int get_sink(int mn,struct map *cmap)
{
	int x,y,mn2=-1,xp,yp,tot;

	x=cmap[mn].xadd;
	y=cmap[mn].yadd;
	
	xp=mn%MAPDX;
	yp=mn/MAPDX;

	if (x==0 && y==0) return cmap[mn].sink;

	if (x>0 && y==0 && xp<MAPDX-1) { tot=40;  mn2=mn-MAPDX+1; }
	if (x<0 && y==0 && xp>0) { tot=40; mn2=mn+MAPDX-1; }
	if (x==0 && y>0 && yp<MAPDY-1) { tot=20; mn2=mn+MAPDX+1; }
	if (x==0 && y<0 && yp>0) { tot=20; mn2=mn-MAPDX-1; }

	if (x>0 && y>0 && xp<MAPDX-1 && yp<MAPDY-1) { tot=30; mn2=mn+1; }
	if (x>0 && y<0 && xp<MAPDY-1 && yp>0) { tot=30; mn2=mn-MAPDX; }
	if (x<0 && y>0 && xp>0 && yp<MAPDY-1) { tot=30; mn2=mn+MAPDX; }	
	if (x<0 && y<0 && xp>0 && yp>0) { tot=30;  mn2=mn-1; }
	
        if (mn2==-1) return cmap[mn].sink;

        x=abs(x);
	y=abs(y);

        return (cmap[mn].sink*(tot-x-y)+cmap[mn2].sink*(x+y))/tot;
}

void display_game_map(struct map *cmap)
{
        int i,e,fn,nr,mapx,mapy,mn,scrx,scry,light,x,y,x1,y1,x2,y2,mna,sprite,n,sink,xoff,yoff,start;
        DL *dl;
        DDFX *ddfx;
        int heightadd;

	start=GetTickCount();

        for (i=0; i<maxquick; i++) {

                mn=quick[i].mn[4];
                scrx=mapaddx+quick[i].cx;
                scry=mapaddy+quick[i].cy;
                light=cmap[mn].rlight;

                // field is invisible - draw a black square and ignore everything else
                if (!light) { dl_next_set(GNDSTR_LAY,0,scrx,scry,DDFX_NLIGHT); continue; }
#ifdef EDITOR
                if (cmap[mn].ex_flags&EXF_BRIGHT) light=DDFX_BRIGHT;
#endif

                // blit the grounds and straighten it, if neccassary ...
		if (cmap[mn].rg.sprite) {
                        dl=dl_next_set(get_lay_sprite(cmap[mn].gsprite,GND_LAY),cmap[mn].rg.sprite,scrx,scry-10,light);
                        if (!dl) { note("error in game #1"); continue; }
			if (lightquality&2) {
				if ((mna=quick[i].mn[3])!=0 && (cmap[mna].rlight)) dl->ddfx.ll=cmap[mna].rlight; else dl->ddfx.ll=light;
				if ((mna=quick[i].mn[5])!=0 && (cmap[mna].rlight)) dl->ddfx.rl=cmap[mna].rlight; else dl->ddfx.rl=light;
				if ((mna=quick[i].mn[1])!=0 && (cmap[mna].rlight)) dl->ddfx.ul=cmap[mna].rlight; else dl->ddfx.ul=light;
				if ((mna=quick[i].mn[7])!=0 && (cmap[mna].rlight)) dl->ddfx.dl=cmap[mna].rlight; else dl->ddfx.dl=light;			
			}
			dl->ddfx.scale=cmap[mn].rg.scale;
			dl->ddfx.cr=cmap[mn].rg.cr;
			dl->ddfx.cg=cmap[mn].rg.cg;
			dl->ddfx.cb=cmap[mn].rg.cb;
			dl->ddfx.clight=cmap[mn].rg.light;
			dl->ddfx.sat=cmap[mn].rg.sat;
			dl->ddfx.c1=cmap[mn].rg.c1;
			dl->ddfx.c2=cmap[mn].rg.c2;
			dl->ddfx.c3=cmap[mn].rg.c3;
			dl->ddfx.shine=cmap[mn].rg.shine;
			dl->h=-10;

			if (cmap[mn].flags&CMF_INFRA) { dl->ddfx.cr=min(120,dl->ddfx.cr+80); dl->ddfx.sat=min(20,dl->ddfx.sat+15); }
			if (cmap[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }

			gsprite_cnt++;
		}

                // ... 2nd (gsprite2)
                if (cmap[mn].rg2.sprite) {
        		dl=dl_next_set(get_lay_sprite(cmap[mn].gsprite2,GND2_LAY),cmap[mn].rg2.sprite,scrx,scry,light);
			if (!dl) { note("error in game #2"); continue; }
        		if (lightquality&2) {
        			if ((mna=quick[i].mn[3])!=0 && (cmap[mna].rlight)) dl->ddfx.ll=cmap[mna].rlight; else dl->ddfx.ll=light;
        			if ((mna=quick[i].mn[5])!=0 && (cmap[mna].rlight)) dl->ddfx.rl=cmap[mna].rlight; else dl->ddfx.rl=light;
        			if ((mna=quick[i].mn[1])!=0 && (cmap[mna].rlight)) dl->ddfx.ul=cmap[mna].rlight; else dl->ddfx.ul=light;
        			if ((mna=quick[i].mn[7])!=0 && (cmap[mna].rlight)) dl->ddfx.dl=cmap[mna].rlight; else dl->ddfx.dl=light;			
        		}
			dl->ddfx.scale=cmap[mn].rg2.scale;
			dl->ddfx.cr=cmap[mn].rg2.cr;
			dl->ddfx.cg=cmap[mn].rg2.cg;
			dl->ddfx.cb=cmap[mn].rg2.cb;
			dl->ddfx.clight=cmap[mn].rg2.light;
			dl->ddfx.sat=cmap[mn].rg2.sat;
			dl->ddfx.c1=cmap[mn].rg2.c1;
			dl->ddfx.c2=cmap[mn].rg2.c2;
			dl->ddfx.c3=cmap[mn].rg2.c3;
			dl->ddfx.shine=cmap[mn].rg2.shine;

			if (cmap[mn].flags&CMF_INFRA) { dl->ddfx.cr=min(120,dl->ddfx.cr+80); dl->ddfx.sat=min(20,dl->ddfx.sat+15); }
			if (cmap[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }

			g2sprite_cnt++;
                }

		if (cmap[mn].mmf&MMF_STRAIGHT_T) dl_next_set(GNDSTR_LAY,50,scrx,scry,DDFX_NLIGHT);
                if (cmap[mn].mmf&MMF_STRAIGHT_B) dl_next_set(GNDSTR_LAY,51,scrx,scry,DDFX_NLIGHT);
                if (cmap[mn].mmf&MMF_STRAIGHT_L) dl_next_set(GNDSTR_LAY,52,scrx,scry,DDFX_NLIGHT);
                if (cmap[mn].mmf&MMF_STRAIGHT_R) dl_next_set(GNDSTR_LAY,53,scrx,scry,DDFX_NLIGHT);

                // blit fsprites
                if (cmap[mn].rf.sprite) {
                        if (lightquality&1) {
                                dl=dl_next_set(get_lay_sprite(cmap[mn].fsprite,GME_LAY),cmap[mn].rf.sprite,scrx,scry-9,light);
				if (!dl) { note("error in game #3"); continue; }
				dl->h=-9;
                                if ((mna=quick[i].mn[3])!=0 && (cmap[mna].rlight)) dl->ddfx.ll=cmap[mna].rlight; else dl->ddfx.ll=light;
				if ((mna=quick[i].mn[5])!=0 && (cmap[mna].rlight)) dl->ddfx.rl=cmap[mna].rlight; else dl->ddfx.rl=light;
				if ((mna=quick[i].mn[1])!=0 && (cmap[mna].rlight)) dl->ddfx.ul=cmap[mna].rlight; else dl->ddfx.ul=light;
				if ((mna=quick[i].mn[7])!=0 && (cmap[mna].rlight)) dl->ddfx.dl=cmap[mna].rlight; else dl->ddfx.dl=light;
                        } else {
                                dl=dl_next_set(GME_LAY,cmap[mn].rf.sprite,scrx,scry-9,light);
				if (!dl) { note("error in game #4"); continue; }
				dl->h=-9;
                        }

                        // fsprite can increase the height of items and fsprite2
                        heightadd=is_yadd_sprite(cmap[mn].rf.sprite);

			dl->ddfx.scale=cmap[mn].rf.scale;
			dl->ddfx.cr=cmap[mn].rf.cr;
			dl->ddfx.cg=cmap[mn].rf.cg;
			dl->ddfx.cb=cmap[mn].rf.cb;
			dl->ddfx.clight=cmap[mn].rf.light;
			dl->ddfx.sat=cmap[mn].rf.sat;
			dl->ddfx.c1=cmap[mn].rf.c1;
			dl->ddfx.c2=cmap[mn].rf.c2;
			dl->ddfx.c3=cmap[mn].rf.c3;
			dl->ddfx.shine=cmap[mn].rf.shine;

			if (cmap[mn].flags&CMF_INFRA) { dl->ddfx.cr=min(120,dl->ddfx.cr+80); dl->ddfx.sat=min(20,dl->ddfx.sat+15); }
			if (cmap[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }

			if (get_offset_sprite(cmap[mn].fsprite,&xoff,&yoff)) {
				dl->x+=xoff;
				dl->y+=yoff;
			}

			fsprite_cnt++;
                } else heightadd=0;

		// ... 2nd (fsprite2)
                if (cmap[mn].rf2.sprite) {
                        if (lightquality&1) {
                                dl=dl_next_set(get_lay_sprite(cmap[mn].fsprite2,GME_LAY),cmap[mn].rf2.sprite,scrx,scry+1,light);
				if (!dl) { note("error in game #5"); continue; }
				dl->h=1;
                                if ((mna=quick[i].mn[3])!=0 && (cmap[mna].rlight)) dl->ddfx.ll=cmap[mna].rlight; else dl->ddfx.ll=light;
				if ((mna=quick[i].mn[5])!=0 && (cmap[mna].rlight)) dl->ddfx.rl=cmap[mna].rlight; else dl->ddfx.rl=light;
				if ((mna=quick[i].mn[1])!=0 && (cmap[mna].rlight)) dl->ddfx.ul=cmap[mna].rlight; else dl->ddfx.ul=light;
				if ((mna=quick[i].mn[7])!=0 && (cmap[mna].rlight)) dl->ddfx.dl=cmap[mna].rlight; else dl->ddfx.dl=light;
                        } else {
                                dl=dl_next_set(GME_LAY,cmap[mn].rf2.sprite,scrx,scry+1,light);
				if (!dl) { note("error in game #6"); continue; }
				dl->h=1;
                        }
                        dl->y+=1;
                        dl->h+=1;
                        dl->h+=heightadd;
			dl->ddfx.scale=cmap[mn].rf2.scale;
			dl->ddfx.cr=cmap[mn].rf2.cr;
			dl->ddfx.cg=cmap[mn].rf2.cg;
			dl->ddfx.cb=cmap[mn].rf2.cb;
			dl->ddfx.clight=cmap[mn].rf2.light;
			dl->ddfx.sat=cmap[mn].rf2.sat;
			dl->ddfx.c1=cmap[mn].rf2.c1;
			dl->ddfx.c2=cmap[mn].rf2.c2;
			dl->ddfx.c3=cmap[mn].rf2.c3;
			dl->ddfx.shine=cmap[mn].rf2.shine;

			if (cmap[mn].flags&CMF_INFRA) { dl->ddfx.cr=min(120,dl->ddfx.cr+80); dl->ddfx.sat=min(20,dl->ddfx.sat+15); }
			if (cmap[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }

			if (get_offset_sprite(cmap[mn].fsprite2,&xoff,&yoff)) {
				dl->x+=xoff;
				dl->y+=yoff;
			}

			f2sprite_cnt++;
                }

                // blit items
                if (cmap[mn].isprite) {
			dl=dl_next_set(get_lay_sprite(cmap[mn].isprite,GME_LAY),cmap[mn].ri.sprite,scrx,scry-8,itmsel==mn?DDFX_BRIGHT:light);
			if (!dl) { note("error in game #8 (%d,%d)",cmap[mn].ri.sprite,cmap[mn].isprite); continue; }
			if (lightquality&1) {
				if ((mna=quick[i].mn[3])!=0 && (cmap[mna].rlight)) dl->ddfx.ll=cmap[mna].rlight; else dl->ddfx.ll=light;
				if ((mna=quick[i].mn[5])!=0 && (cmap[mna].rlight)) dl->ddfx.rl=cmap[mna].rlight; else dl->ddfx.rl=light;
				if ((mna=quick[i].mn[1])!=0 && (cmap[mna].rlight)) dl->ddfx.ul=cmap[mna].rlight; else dl->ddfx.ul=light;
				if ((mna=quick[i].mn[7])!=0 && (cmap[mna].rlight)) dl->ddfx.dl=cmap[mna].rlight; else dl->ddfx.dl=light;
                        }
                        dl->h+=heightadd-8;
			dl->ddfx.scale=cmap[mn].ri.scale;
			dl->ddfx.cr=cmap[mn].ri.cr;
			dl->ddfx.cg=cmap[mn].ri.cg;
			dl->ddfx.cb=cmap[mn].ri.cb;
			dl->ddfx.clight=cmap[mn].ri.light;
			dl->ddfx.sat=cmap[mn].ri.sat;
			dl->ddfx.c1=cmap[mn].ri.c1;
			dl->ddfx.c2=cmap[mn].ri.c2;
			dl->ddfx.c3=cmap[mn].ri.c3;
			dl->ddfx.shine=cmap[mn].ri.shine;

			if (cmap[mn].flags&CMF_INFRA) { dl->ddfx.cr=min(120,dl->ddfx.cr+80); dl->ddfx.sat=min(20,dl->ddfx.sat+15); }
			if (cmap[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }

			if (cmap[mn].flags&CMF_TAKE) {
				dl->ddfx.sink=min(12,cmap[mn].sink);
				dl->y+=min(6,cmap[mn].sink/2);
				dl->h+=-min(6,cmap[mn].sink/2);
			} else if (cmap[mn].flags&CMF_USE) {
				dl->ddfx.sink=min(20,cmap[mn].sink);
				dl->y+=min(10,cmap[mn].sink/2);
				dl->h+=-min(10,cmap[mn].sink/2);
			}

			if (get_offset_sprite(cmap[mn].isprite,&xoff,&yoff)) {
				dl->x+=xoff;
				dl->y+=yoff;
			}

			isprite_cnt++;
                }

                // blit chars
                if (cmap[mn].csprite) {
                        dl=dl_next_set(GME_LAY,cmap[mn].rc.sprite,scrx+cmap[mn].xadd,scry+cmap[mn].yadd,chrsel==mn?DDFX_BRIGHT:light);
			if (!dl) { note("error in game #9"); continue; }
			sink=get_sink(mn,cmap);
                        dl->ddfx.sink=sink;
			dl->y+=sink/2;
			dl->h=-sink/2;
			dl->ddfx.scale=cmap[mn].rc.scale;
                        //addline("sprite=%d, scale=%d",cmap[mn].rc.sprite,cmap[mn].rc.scale);

			// keep male mage sprite from jumping one pixel during holding torch idle animatiom
			switch(dl->ddfx.sprite) {
				case 163818:
				case 163819:

				case 164818:
				case 164819:
				
				case 168818:
				case 168819:

				case 169818:
				case 169819:
				case 169844:
				case 169845:
				case 169846:
				case 169847:	dl->x-=1; break;
			}

                        dl->ddfx.cr=cmap[mn].rc.cr;
			dl->ddfx.cg=cmap[mn].rc.cg;
			dl->ddfx.cb=cmap[mn].rc.cb;
			dl->ddfx.clight=cmap[mn].rc.light;
			dl->ddfx.sat=cmap[mn].rc.sat;
			dl->ddfx.c1=cmap[mn].rc.c1;
			dl->ddfx.c2=cmap[mn].rc.c2;
			dl->ddfx.c3=cmap[mn].rc.c3;
			dl->ddfx.shine=cmap[mn].rc.shine;

                        // check for spells on char
                        for (nr=0; nr<MAXEF; nr++) {
                                if (!ueffect[nr]) continue;
                                if ((unsigned int)ceffect[nr].freeze.cn==map[mn].cn && ceffect[nr].generic.type==11) { // freeze
					int diff;

                                        if ((diff=tick-ceffect[nr].freeze.start)<DDFX_MAX_FREEZE*4) {	// starting
						dl->ddfx.freeze=diff/4;						
					} else if (ceffect[nr].freeze.stop<tick) {			// already finished
						continue;
					} else if ((diff=ceffect[nr].freeze.stop-tick)<DDFX_MAX_FREEZE*4) {	// ending
						dl->ddfx.freeze=diff/4;						
					} else dl->ddfx.freeze=DDFX_MAX_FREEZE-1;		// running
                                }
				if ((unsigned int)ceffect[nr].curse.cn==map[mn].cn && ceffect[nr].generic.type==18) { // curse
					
                                        dl->ddfx.sat=min(20,dl->ddfx.sat+(ceffect[nr].curse.strength/4)+5);
					dl->ddfx.clight=min(120,dl->ddfx.clight+ceffect[nr].curse.strength*2+40);
					dl->ddfx.cb=min(80,dl->ddfx.cb+ceffect[nr].curse.strength/2+10);
                                }
				if ((unsigned int)ceffect[nr].cap.cn==map[mn].cn && ceffect[nr].generic.type==19) { // palace cap
					
					dl->ddfx.sat=min(20,dl->ddfx.sat+20);
					dl->ddfx.clight=min(120,dl->ddfx.clight+80);
                                        dl->ddfx.cb=min(80,dl->ddfx.cb+80);
                                }
				if ((unsigned int)ceffect[nr].lag.cn==map[mn].cn && ceffect[nr].generic.type==20) { // lag
					
					dl->ddfx.sat=min(20,dl->ddfx.sat+20);
					dl->ddfx.clight=max(-120,dl->ddfx.clight-80);
                                }
                        }
			if (cmap[mn].gsprite==51066) {
				dl->ddfx.sat=20;
				dl->ddfx.cr=80;
				dl->ddfx.clight=-80;
				dl->ddfx.shine=50;
				dl->ddfx.ml=dl->ddfx.ll=dl->ddfx.rl=dl->ddfx.ul=dl->ddfx.dl=chrsel==mn?DDFX_BRIGHT:DDFX_NLIGHT;
				//dl->ddfx.grid=DDFX_RIGHTGRID;
			} else if (cmap[mn].gsprite==51067) {
				dl->ddfx.sat=20;
				dl->ddfx.cb=80;
				dl->ddfx.clight=-80;
				dl->ddfx.shine=50;
				dl->ddfx.ml=dl->ddfx.ll=dl->ddfx.rl=dl->ddfx.ul=dl->ddfx.dl=chrsel==mn?DDFX_BRIGHT:DDFX_NLIGHT;
				//dl->ddfx.grid=DDFX_RIGHTGRID;
			} else {
				if (cmap[mn].flags&CMF_INFRA) { dl->ddfx.cr=min(120,dl->ddfx.cr+80); dl->ddfx.sat=min(20,dl->ddfx.sat+15); }
				if (cmap[mn].flags&CMF_UNDERWATER) { dl->ddfx.cb=min(120,dl->ddfx.cb+80); dl->ddfx.sat=min(20,dl->ddfx.sat+10); }
			}

                        if (nocut) dl_autogrid();

                        csprite_cnt++;			
                }		
        }
	show_bubbles();
	dg_time+=GetTickCount()-start;

#ifdef EDITOR
	if (cmap==map || editor) {	// double ouch!!
#else
	if (cmap==map) {	        // ouch!!
#endif
		// selection on ground
		if (mapsel!=-1) {
			mn=mapsel;
			mapx=mn%MAPDX;
			mapy=mn/MAPDX;
#ifdef EDITOR
                        if (editor) {
                                extern int emapdx,emapdy;
                                mapx=mn%emapdx;
                                mapy=mn/emapdy;
                        }
#endif
			mtos(mapx,mapy,&scrx,&scry);
                        if (cmap[mn].rlight==0 || (cmap[mn].mmf&MMF_SIGHTBLOCK)) sprite=SPR_FFIELD; else sprite=SPR_FIELD;
			dl=dl_next_set(GNDSEL_LAY,sprite,scrx,scry,DDFX_NLIGHT);
			if (!dl) note("error in game #10");
                        dl_autogrid();			
		}
                // act (field) quick and dirty
                if (act==PAC_MOVE) display_game_act();
		
                dl_play();		

		// act (text)  quick and dirty
		if (act!=PAC_MOVE) display_game_act();		
	}			
}

int do_display_help(int nr)
{				
	int y=45,oldy;

	switch(nr) {
		case 1:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Help Index"); y+=15;

                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Fast Help"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Walk: LEFT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Look on Ground:  RIGHT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Take/Drop/Use: SHIFT LEFT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Look at Item: SHIFT RIGHT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Attack/Give: CTRL LEFT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Look at Character: CTRL RIGHT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Use Item in Inventory: LEFT-CLICK or F1...F4");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Fast/Normal/Stealth: F5/F6/F7");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Scroll Chat Window: PAGE-UP/PAGE-DOWN");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Repeat last Tell: TAB");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Close Help: F11");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Show Walls: F8");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Quit Game: F12 - preferably on blue square");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Assign Wheel Button: Use Wheel"); y+=10;

			oldy=y;
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* A");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* A");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* B");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* C");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* C");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* C");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* D");
                        y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* E");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* E");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* F");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* G");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* I");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* K");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* L");

			y=oldy;
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* M");
                        y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* N");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* O");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* P");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* Q");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* Q");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* R");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* R");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* S");
                        y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* S");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* S");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* T");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* T");
			y=dd_drawtext_break(110,y,202,lightbluecolor,0,"* W");
			break;

		case 2:

                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Accounts"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Your account belongs to you only.  If you log in to other players' accounts, or if you let other players log in to your account, all accounts involved will be banned.  Any account that appears to be shared or traded will be banned permanently.  We reserve the right to cancel any account at any time. Be smart - protect your account!  If you have a question concerning your account or trouble with your account payments, please write cash@astonia.com."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"There are two types of accounts - free and premium.  Free accounts can be played by anyone.  Premium (paid) accounts offer several advantages, such as larger inventory, ability to use 2- and 3-stat items and preferred access if the server is busy."); y+=10;
                        break;
		case 3:

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Alias commands"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Text phrases that you use repeatedly can be stored as Alias commands and retrieved with a few characters. You can store up to 32 alias commands. To store an alias command, you first have to pick a phrase to store, then give that phrase a name. The alias command for storing text is: /alias <alias phrase name> <phrase>"); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"For example, to get the phrase, \"Let's go penting today!\", whenever you type p1, you'd type: /alias p1 Let's go penting today!"); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To delete an alias, first type: /alias to bring up your list of aliases.  Choose which alias you want to delete, then type:  /alias <name of alias>."); y+=10;
			break;

		case 4:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Banking"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Money and items can be stored in a depot (item storage locker) at the Imperial Bank.  All characters on your account share the same bank depot and you can access your depot at any bank.  SHIFT + LEFT CLICK on the wood cabinet in any bank to access your depot.  Only gold coins can be deposited with the bank or in your depot - silver coins cannot be deposited.  To quickly sort items in your depot, type:  /sortdepot.  Talk to the Banker to get more information about banking."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Base/Mod Values"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"All your attributes and skills show two values:  the first one is the base value (it shows how much you have raised it); the second is the modified (mod) value.  The mod value consists of the base value, plus bonuses from your base attributes and special items.  No skill or spell values can be raised through items by more than 50% of its base value."); y+=10;
			break;
		case 5:

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Chat and Channels"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Everything you hear and say is displayed in the chat window at the bottom of your screen.  To talk to a character standing next to you, just type what you'd like to say and hit ENTER.  To say something in the general chat channel (the \"Gossip\" channel) which will be heard by all other players, type:  /c2 <your message> and hit ENTER.  Use the PAGE UP and PAGE DOWN keys on your keyboard to scroll the chat window up and down.  To see a list of channels in the game, type:  /channels.  To join a channel, type:  /join <channel number>.  To leave a channel, type:  /leave <channel number>.  Spamming, offensive language, and disruptive chatter is not allowed.  To send a message to a particular player, type:  /tell <player name> and then your message.  Nobody else will hear what you said."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Clans"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"There is detailed information about clans in the Game Manual at urd.astonia.com. To see a list of clans in the game, type: /clan."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"A player can join a clan at any time, provided that the player has not joined a clan within the last 3 days and/or the player has not left or been fired from a clan within the last 12 hours."); y+=10;
			break;

		case 6:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Colors"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"All characters enter the game with a set of default colors for their clothing and hair, but you can change the color of your character's shirt, pants/skirt, and hair/cap if you choose."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Matte Colors.  Use the Color Toolbox in the game to choose matte colors.  Type:  /col1  to bring up the Color Toolbox.  From left to right, the three circles at the bottom represent shirt color, pants/skirt color, and hair/cap color.  Click on a circle, then move the color bars to find a color that you like.  The model in the box displays your color choices.  Click the Exit button to exit the Color Toolbox."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Glossy Colors.  To make glossy colors, you use a command typed into the chat area instead of using the Color Toolbox.  Like mixing paints, the number values you choose (between 1-31) for the red (R), green (G), and blue (B) amounts determine how much of each is mixed in.  Adding an extra 31 to the red (R) value makes the color combination you have chosen a glossy color."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"When typing the command, you first start by hitting the spacebar on your keyboard once, then  typing one of these commands:  "); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"/col1 <R><G><B> shirt color");
			y=dd_drawtext_break(10,y,202,graycolor,0,"/col2 <R><G><B> pants/skirt/cape color");
			y=dd_drawtext_break(10,y,202,graycolor,0,"/col3 <R><G><B> hair/cap color");
			//y=dd_drawtext_break(10,y,202,graycolor,0,"Example:  to make your pants a glossy green color, you would hit the spacebar once, then type:  /col2 32 31 1.  To make your hair/cap a glossy yellow, hit the spacebar once then type:  /col3 62 31 1.  Black is 1 1 1 and white is 31 31 31.  There is no glossy black color.  If you are having trouble with the command, make sure you have hit the spacebar on your keyboard once before typing in the command.  The command will not work without a space in front of it.  Try experimenting with different color combinations to see what colors you can develop!"); y+=10;
			break;
		case 7:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Commands"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Type: /help to see a list of all available commands.  You can type:  /status  to see a list of optional toggle commands that may aid your character's performance."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Complains/Harassment"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"If you feel another player's behavior is disrupting the game, you can type:  /complain <player name><reason>  This command sends a screenshot of the disruptive chat to Game Admin.  The <reason> portion of the command is for you to enter your own comments regarding the situation. Please be aware that only the last 80 lines of text are sent and that each server-change (teleport) erases this buffer.  You can also type:  /ignore <name>  to ignore the things a player is saying."); y+=10;
			break;
		case 8:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Dying"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"When you die, you will suddenly find yourself on a blue square - the last blue square you stepped on.  If you did not have a 'save', then your corpse will be at the place where you died and all the items from your Inventory will be with your corpse. You have 30 minutes to make it back to your corpse to get these items. After 30 minutes, your corpse disappears, along with your inventory items.  You are the only one allowed access to your corpse to retrieve items."); y+=10;
                        break;

		case 9:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Enhanced Items"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"An Enhanced Item has 4 qualities:");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Strength - The health of the item. This will drop over time due to combat and can be brought back up by using silver units and gold units (not coins).");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Complexity - The health (Strength) of an item at 100% effectivity. A lower complexity means less silver units or gold units have to be used to repair it.");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Quality - The overall quality of an item. A high Quality means a lower cost of repairing the item.");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Effectivity - The health of the item in percentages. As this drops the bonus from the item will drop. A +10 item with 50% effectivity will give only a +5 bonus. With 120% effectivity the same item will give a +12 bonus. Use silver units to get up to 105%, gold units up to 120%.");
			break;
		case 10:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Enhancing Equipment"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Orbs can be used to further enhance equipment that already have bonuses. For example, a Magic Shield +1 ring has a bonus; a plain ring does not and cannot be enhanced. To enhance an item with a bonus, you will need stones and an orb. Stones are found on corpses of monsters in the Pentagram Quest(tm) - the monster corpses won't disappear if there is an item, including stones, left behind on their corpse, so be sure to check all non-disappearing corpses! Stones (earth, fire, ice, and hell), increase the strength and complexity of an orb."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"In your inventory, SHIFT + LEFT CLICK the piece of equipment you wish to enhance and move it over the top of the orb, then release the SHIFT key and LEFT CLICK the piece on the orb. If there is enough Strength on the orb, the piece will be enhanced by 1. If there is not enough Strength on the orb, you will get a text message telling you how much is needed to enhance the item. RIGHT click on orbs and stones to read about them."); y+=10;
			break;
		case 11:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Fighting"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To attack somebody, place your cursor over the character you'd like to attack, and then hit CTRL + LEFT CLICK."); y+=10;
			break;
		case 12:
                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Game Moderators"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"The Game Moderators are members of Admin who help keep the in-game playing field running smoothly.  Admin can be recognized by their name being in capital letters (i.e. 'COLOMAN' is a member of Admin, 'Coloman' is not).  Admin help keep the peace, answer questions, and can punish unruly players (if needed)."); y+=10;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Gold"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Gold and silver are the monetary units used in the game.  To give money to another person, place your cursor over your gold (bottom of your screen), LEFT CLICK, and slowly drag your mouse upwards until you have the amount on your cursor that you want.  Then, let your cursor rest on the person you wish to give the money to, and hit CTRL + LEFT CLICK."); y+=10;
			break;
		case 13:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Items"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To use or pick up items around you, put your cursor over the item and hit SHIFT + LEFT CLICK.  To use an item in your Inventory, LEFT CLICK on it.  To give an item to another character, take the item by using SHIFT + LEFT CLICK, then pull it over the other character and hit CTRL + LEFT CLICK.  To loot the corpses of slain enemies, place your cursor over the body and hit SHIFT + LEFT CLICK."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Some items can only be worn by mages, warriors, or seyans.  For example, a mage cannot use a sword and a warrior cannot use a staff.  If you cannot equip an item it may be because your class of character cannot wear that particular item, or because of level/skill restrictions on that item.  RIGHT click on the item to read more about it."); y+=10;
			break;
		case 14:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Karma"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Karma is a measurement of how well a player has obeyed game rules. When you receive a Punishment, you lose karma. All players enter the game with 0 karma. If you receive a Level 1 punishment, for example, your karma will drop to -1. Please review the Laws, Rules, and Regulations section in the Game Manual to familiarize yourself with the punishment system."); y+=10;
			break;
		case 15:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Leaving the game"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To leave the game, step on one of the blue tile rest areas and hit F12. You can also hit F12 when not on a blue tile, but your character will stay in that same spot for five minutes and risks being attacked."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Light"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Torches provide the main source of light in the game.  To use a torch, equip it, then LEFT CLICK on it to light it.  It is a good idea to carry extras with you at all times."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Looking at characters/items"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To look at a character, place your cursor over him and hit CTRL + RIGHT CLICK.  To look at an item around you, place your cursor over the item and hit  SHIFT + RIGHT CLICK.  To look at an item in your Equipment/Inventory areas or in a shop window, (place your cursor over the item and) RIGHT CLICK."); y+=10;
			break;
		case 16:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Mirrors"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Each area can have up to 26 mirrors (servers), to allow more players to be online at once.  Your mirror number determines which mirror you use.  You can see which mirror you are currently on by \"looking\" at yourself (place your cursor over yourself and hit CTRL + RIGHT CLICK).  If you would like to meet a player on a different mirror, go to a teleport station, click on the corresponding mirror number (M1 to M26) and teleport to the area that the other player is in.  You have to teleport, even if the other player is in the same area."); y+=10;
			break;
		case 17:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Navigational Directions"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Compass directions (North, East, South, West) are the same in the game as in real life.  North, for example, would be 'up' (the top of your screen).  East would be to the direct right of your screen."); y+=10;
			
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Negative Experience"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"When you die, you lose experience points. Too many deaths can result in Negative Experience. Once this Negative Experience is made up, then experience points obtained will once again count towards leveling."); y+=10;
			break;
		case 18:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Orbs"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"An Orb has the same 3 qualities as a Stone:");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Strength - Used to determine if a piece of equipment can be enhanced or not. The amount it takes to enhance the item is added to the equipment and subtracted from the Orb.");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Complexity - A certain amount will be added to the equipment when an Orb is used. The formula is: (str / quality) * 100 = complexity added");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Quality - A percentage showing strength compared to complexity. The higher the percentage the better.");
			break;
		case 19:
                        y=dd_drawtext_break(10,y,202,whitecolor,0,"The Pentagram Quest(tm)"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"The Pentagram Quest(tm) is a game that all players (from about level 10 and up) can play together.  It's an ongoing game that takes place in \"the pents\" - a large, cavernous area partitioned off according to player levels.  The walls and floors of this area are covered with \"stars\" (pentagrams) and the object of the game is to touch as many pentagrams as possible as you fight off the evil demons that inhabit the area.  Once a randomly chosen number of pentagrams have been touched, the pents are \"solved\", and you receive experience points for the pentagrams you touched. The entrances to the Pentagram Quest  are southeast of blue squares in Aston - be sure to SHIFT + RIGHT CLICK on the doors to determine which level area is right for you!"); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Punishments"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Punishments range from a temporary ban to complete deletion of a player's account.  Please review the Terms section in the Game Manual to familiarize yourself with the punishment system."); y+=10;
			break;
                case 20:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Quests"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Your first quest will be to find Lydia, the daughter of Gwendylon the mage.  She will ask you to find a potion which was stolen the night before.  Lydia lives in the grey building across from the fortress (the place where you first arrived in the game)."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"NPCs (Non-Player Characters) give the quests in the game.  Even if you talked to an NPC before, talk to him again; he may tell you something new or give you another quest.  Say \"hi\" or <name> \"repeat\" to get an NPC to talk to you.   Be sure to step all the way into a room or area as you quest; monsters, chests, and doors may be hidden in the shadows.  Carry a torch to light your way, and always check the bodies of slain enemies (SHIFT + LEFT CLICK)."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Repeating a quest is an excellent way to gain extra experience points.  Hit the F9 button to display a list of completed quests.  Click on 're-open' to do a quest over.  Each quest that can be re-opened shows the percentage of experience points you can get for that quest by doing the quest again.  Some quests cannot be repeated and there will be no Re-open option for those."); y+=10;
			break;
		case 21:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Questions"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"If you have a question while in the game, you can always ask a Staffer. Staffers and other admin can be recognized by their name being in capital letters (i.e. \"COLOMAN\" is a member of Admin, \"Coloman\" is not).  For any other game related questions, please write to game@astonia.com."); y+=10;
			break;
		case 22:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Reading books, signs, etc."); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To read books, use SHIFT + LEFT CLICK.  To read signs, use SHIFT + RIGHT CLICK."); y+=10;
			break;
			
		case 23:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Repairing Equipment"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Equipment will degrade over time, but only if actively used. Damage and wear affects both the Strength and Effectivity of equipment (RIGHT click on the piece to read about it).  All new items have a 110% Effectivity level. An item repaired with silver can be repaired up to 105% Effectivity, while an item repaired with gold can be repaired up to the maximum level of 120% Effectivity. The level of Effectivity of a piece also affects its bonus stats. For example, a +10 parry ring with 100% Effectivity will have a bonus of +10; the same ring at 50% Effectivity will have a bonus of +5."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Silver will repair a piece up to 105%; gold can be used to repair it to a maximum Effectivity level of 120%."); y+=10;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Strength determines how much silver or gold will be needed to repair the piece. For example, a +1 fire ring has a maximum Strength of 300 at 120% Effectivity. At 105% Effectivity, this ring has a Strength of 262. If a piece is at 60% Effectivity, the Strength is 150. Repairing with silver first, up to 105% Effectivity, you will need (262 - 150) = 112 silver. Once it's up to 105% Effectivity, then use gold to repair it up to 120%. For this you will need (300 - 262) = 38 gold."); y+=10;
			break;
		case 24:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Saves"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"With each new level you obtain as you play, you also receive a Save. A Save is a gift from Ishtar: if you die, your items stay with you instead of having them left on your corpse, and you will not get negative experience. The maximum number of Saves that a player can have at any time is three."); y+=10;
			
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Scamming"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Most cases of scamming happen when players share passwords.  NEVER give your password to another player for any reason!  Make your passwords hard to guess by using a combination of numbers and letters.  Change your password often; go to www.astonia.com, then click on Account Management to change your password.  Always use an NPC Trader when trading with another player.  The NPC Trader can be found in most towns in or near the banks - he is a non-playing character that will handle the trade for both parties.  If a player does not want to use an NPC Trader for trading with you, then do not trade with him - he could potentially steal your items.  Do not put your items on the ground when trading with another player or you risk losing them.  Be wary of loaning your equipment to others - unfortunately, many never see their items again."); y+=10;
                        break;
		
		case 25:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Shops"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To open a shop window with a merchant, type:  trade <merchant name>  When trading with a merchant, the items for sale are shown at the bottom-left of your screen (the view of your skills/stats is temporarily replaced by the shop window).  To read about the items, RIGHT CLICK on them.  To buy something, LEFT CLICK on it.  To sell an item from your inventory, LEFT CLICK on it."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Skills/Stats"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"In your stats/skills window (bottom-left of your screen) you see red/blue lines below your skills. Blue indicates that you have enough experience points to raise the skill; red indicates that you don't have enough experience points. To raise a skill, CLICK on the blue orb next to the skill."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Spells"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To use a spell, hit ALT and the corresponding number of the spell that appears on your screen.  Mages - to cast the Heal spell on another player, rest your cursor on him and hit CTRL 7.  The Heal spell restores Hitpoints gradually over a period of time.  Warriors - hit ALT 8 to use Warcry."); y+=10;
			break;
		case 26:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Stones"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"A Stone has 3 qualities:");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Strength - This is added to an orb to make it stronger so it can be used to enhance equipment.");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Complexity - This is added to an orb and eventually determines how much it will cost to repair an item.");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Quality - A percentage showing strength compared to complexity. The higher the percentage the better.");
			break;
		case 27:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Tokens"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"You can pay for another player's account using the in-game payment system. After making the corresponding payment in your account management, you will receive a payment token.  Payment tokens are not 'items' in the game, like equipment or coins are, but are part of your /status listing.  You can see how many tokens you have by typing: /status.  You can then sell the payment token in game.  Any other form of payment (including cash, equipment trades, or items from other games) is illegal and may be punished.  Tokens can only be traded by using a Trader NPC in the game."); y+=10;
			
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Talking"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Everything you hear and say is displayed in the chat window at the bottom of your screen.  To talk to a character standing next to you, just type what you'd like to say and hit ENTER.  To say something in the general chat channel (the \"Gossip\" channel) which will be heard by all other players, type:  /c2 <your message> and hit ENTER.  Use the PAGE-UP and PAGE-DOWN keys on your keyboard to scroll the chat window up and down."); y+=10;
			break;
		case 28:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Transport System"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"You will find Teleport Stations, relics of the ancient culture, all over the world.  SHIFT + LEFT CLICK on the Teleport Station to see a map of all Teleport Stations.  CLICK on the destination of your choice.  You will only be able to teleport to a destination that you have reached at least once before by foot.  Touch any new Teleport Station on your way so that you can teleport there in times to come."); y+=10;
			break;
		case 29:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Walking"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To walk, move your cursor to the place where you'd like to go, then LEFT CLICK on your destination."); y+=10;
			break;		
	}
	return y;
}

/*int do_display_help(int nr)
{
	int y=45;

	switch(nr) {
		case 1:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Help Index"); y+=15;

                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Fast Help"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Walk: LEFT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Look on Ground:  RIGHT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Take/Drop/Use: SHIFT LEFT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Look at Item: SHIFT RIGHT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Attack/Give: CTRL LEFT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Look at Character: CTRL RIGHT-CLICK");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Use Item in Inventory: LEFT-CLICK or F1...F4");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Fast/Normal/Stealth: F5/F6/F7");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Scroll Chat Window: PAGE-UP/PAGE-DOWN");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Repeat last Tell: TAB");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Close Help: F11");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Show Walls: F8");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Quit Game: F12 - preferably on blue square");
			y=dd_drawtext_break(10,y,202,graycolor,0,"Assign Wheel Button: Use Wheel"); y+=10;

			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Walking and Transportation");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Talking and Commands");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Magic and Fighting");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Items");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Newbie Hints");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Money");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Misc");
                        y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Dying");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Advanced Commands");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Frequently Asked Questions");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Frequently Asked Questions 2");
			y=dd_drawtext_break(10,y,202,lightbluecolor,0,"* Frequently Asked Questions 3");
			break;

		case 2:		// walking and transportation
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Walking and Transportation"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Walking"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To walk, move your cursor to the place where you'd like to go, then LEFT CLICK on your destination."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Navigational Directions"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Compass directions (North, East, South, West) are the same in the game as in real life.  North, for example, would be 'up' (the top of your screen).  East would be to the direct right of your screen."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Transport System"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"You will find Teleport Stations, relics of the ancient culture, all over the world. SHIFT+LEFT CLICK on the Teleport Station, to see a map of all teleport stations. CLICK on the destination of your choice. You will only be able to teleport to a destination that you have reached at least once before by foot. Touch any new Teleport Station on your way so that you can teleport there in times to come."); y+=10;
			break;
		
		case 3:	// talking and commands
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Talking and Commands"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Talking"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Everything you hear and say is displayed in the chat window, at the bottom of your screen. To talk to a character standing next to you, just type what you'd like to say and hit ENTER. To say something in the general chat channel (the 'Gossip' channel) which will be heard by all other players, type: /c2 <your message> and hit ENTER. Use the PAGE-UP, PAGE_DOWN keys on your keyboard to scroll the chat window up and down."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Messaging a particular player"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To send a message to a particular player, type: /tell <player name> and then your message. Nobody else will hear what you said."); y+=10;

                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Commands"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Type: /help to see a list of all available commands. You can type: /status to see a list of optional toggle commands that may aid your character's performance."); y+=10;
			break;
		
		case 4:	// magic and fighting
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Magic and Fighting"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Casting spells"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To use a spell, hit ALT and the corresponding number of the spell that appears on your screen. To cast the Bless spell on another player, rest your cursor on him and hit CTRL 6. If you are a warrior, hit ALT 8 to use Warcry."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Fighting"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To attack somebody, place your cursor over the character you'd like to attack, and then hit CTRL+LEFT CLICK."); y+=10;
			break;
		
		case 5:	// items
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Items"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Looking at characters/items"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To look at a character, place your cursor over him and hit CTRL+RIGHT CLICK. To look at an item around you, place your cursor over the item and hit SHIFT+RIGHT CLICK. To look at an item in your Equipment/Inventory areas or in a trade window, RIGHT-CLICK."); y+=10;
		
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Using/Picking up items, looting bodies, giving items to others..."); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To use or pick up items around you, put your cursor over the item and hit SHIFT+LEFT CLICK. To use an item in your Inventory, LEFT CLICK on it. To give an item to another character, take the item by using SHIFT+LEFT CLICK, then pull it over the other character and hit CTRL+LEFT CLICK. To loot the corpses of slain enemies, place your cursor over the body and SHIFT+LEFT CLICK."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Reading books, signs, etc."); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To read books,  use SHIFT + LEFT CLICK.  To read signs, use SHIFT + RIGHT CLICK."); y+=10;
			break;
		
		case 6:		// newbie hints
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Newbie Hints"); y+=15;
                        y=dd_drawtext_break(10,y,202,whitecolor,0,"First Quest"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Your first quest will be to find Lydia, the daughter of Gwendylon the mage. She will ask you to find a potion which was stolen the night before. Lydia lives in the grey building across from the fortress (the place where you first arrived in the game)."); y+=10;

                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Finding New Quests"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"NPCs (Non-Player Characters) give the quests in the game.  Even if you talked to an NPC before, talk to him again; he may tell you something new or give you another quest.  Say 'hi' or '<name>, repeat' to get an NPC to talk to you."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Light"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Torches provide the main source of light in the game.  To use a torch, equip it, then LEFT CLICK on it to light it.  It is a good idea to carry extras with you at all times."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Leaving the game"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To leave the game, step on one of the blue tile rest areas and hit F12. You can also hit F12 when not on a blue tile, but your character will stay in that same spot for five minutes and risks being attacked."); y+=10;
			break;
		
		case 7:		// money
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Money"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Gold"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Gold and silver are the monetary units used in the game.  To give money to another person, place your cursor over your gold (bottom of your screen), LEFT CLICK, and slowly drag your mouse upwards until you have the amount on your cursor that you want. Then, let your cursor rest on the person you wish to give the money to, and hit CTRL+LEFT CLICK."); y+=10;
		
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Banking"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Money and items can be stored in the Imperial Bank.  You have one account per character, and you can access your account at any bank. SHIFT+LEFT CLICK on the cabinet in the bank to access your Depot (item storage locker)."); y+=10;
		
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Trading with Merchants"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"To open a trade window with a merchant, type: 'trade <merchant name>'. When trading with a merchant, the items for sale are shown in the bottom left of your screen (the view of your skills/stats is temporarily replaced by the trade window).  To read about the items, RIGHT CLICK on them.  To buy something, LEFT CLICK on it.  To sell an item from your inventory, LEFT CLICK on it."); y+=10;
			break;

		case 8:		//misc
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Misc"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Skills/Stats"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"In your stats/skills window (bottom-left of your screen) you see red/blue lines below your skills. Blue indicates that you have enough experience points to raise the skill; red indicates that you don't have enough experience points. To raise a skill, CLICK on the blue orb next to the skill."); y+=10;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Saves"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"With each new level you obtain as you play, you also receive a Save. A Save is a gift from Ishtar: if you die, your items stay with you instead of having them left on your corpse, and you will not get negative experience. The maximum number of saves that a player can have at any time is 10."); y+=10;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Karma"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Karma is a measurement of how well a player has obeyed game rules. When you receive a Punishment, you lose karma. All players enter the game with 0 karma. If you receive a Level 1 punishment, for example, your karma will drop to -1. Please review the Laws, Rules and Regulations section in the Game Manual to familiarize yourself with the punishment system. If a player receives -12 karma, his account is locked."); y+=10;
                        break;

                case  9:	// Death
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Dying"); y+=15;
			y=dd_drawtext_break(10,y,202,graycolor,0,"When you die, you will suddenly find yourself on a blue square - the last blue square that you stepped on.  You may see a message displayed on your screen telling you the name and level of the enemy that killed you."); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"If you were not saved, then your corpse will be at the place where you died and all of your items from your Equipment area and your Inventory will still be on your corpse. You have 30 minutes to make it back to your corpse to get these items. After 30 minutes, your corpse disappears, along with your items."); y+=10;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Allowing Access to your Corpse"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"You can allow another player to retrieve your items from your corpse by typing: /allow <playername>  Quest items and keys can only be retrieved from a corpse by the one who has died, even if you /allow someone to access to your corpse."); y+=10;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Negative Experience"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"When you die, you lose experience points. Too many deaths can result in negative experience. Negative experience is displayed in the upper left-hand top of your screen. Once this negative experience is made up, then experience points obtained will once again count towards leveling."); y+=10;
                        break;
		case 10:	// advanced
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Advanced"); y+=15;
                        y=dd_drawtext_break(10,y,202,whitecolor,0,"Alias Commands"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Text phrases that you use repeatedly can be stored as Alias commands and retrieved with a few characters. You can store up to 32 alias commands. To store an alias command, you first have to pick a phrase to store, then give that phrase a name. The alias command for storing text is: /alias <alias phrase name> <phrase>"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"For example, to get the phrase, 'Let's go penting today!', whenever you type 'p1' you'd type:"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"/alias p1 Let's go penting today!"); y+=10;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Changing Clothing/Hair Colors"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"All characters enter the game with a set of default colors for their clothing and hair, but you can change the color of your character's shirt, pants/skirt, and hair/cap if you choose. Type: /col1 to bring up the Color Toolbox. From left to right, the three circles at the bottom represent shirt color, pants/skirt color, and hair/cap color. Click on a circle, then move the color bars to find a color that you like. The model in the box displays your color choices. Click the Exit button to exit the Color Toolbox."); y+=5;			
			break;

		case 11:	// FAQ
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Frequently Asked Questions"); y+=15;
			
			y=dd_drawtext_break(10,y,202,whitecolor,0,"What's base value? What's mod value?"); y+=5;
                        y=dd_drawtext_break(10,y,202,graycolor,0,"All your attributes and skills show two values, the first one is the base value (it shows how much you raised it), the second is the modified (mod) value. It consists of the base value, plus bonusses from your base attributes and special items."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Why don't I get the full effect from Bless?"); y+=5;
                        y=dd_drawtext_break(10,y,202,graycolor,0,"The spell Bless raises your base attributes (Wisdom, Intuition, Agility, Strength) by 1/4 of your modified Bless value (rounded down), but by no more than 50%."); y+=10;
			
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Why doesn't the special item I just found increase my skill XXX? It says XXX+5 in the description!"); y+=5;
                        y=dd_drawtext_break(10,y,202,graycolor,0,"No skill or spell can be raised through items by more than 50% of its base value."); y+=10;
			
			y=dd_drawtext_break(10,y,202,whitecolor,0,"What is a mirror? What is the number I see next to my name when chatting? What does the M1...M26 on the teleport map mean?"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Each area can have up to 26 mirrors, to allow more players to be online at once. Your mirror number determines which mirror you use. If you'd like to meet a player on a different mirror, go to a teleport station, click on the corresponding mirror number (M1...M26) and teleport to the area the other player is in. You have to teleport, even if the other player is in the same area."); y+=10;
			break;
		case 12:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Frequently Asked Questions 2"); y+=15;
                        y=dd_drawtext_break(10,y,202,whitecolor,0,"I have killed all the monsters in my area and still can't find the right key!"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"Be sure to step all the way into a room or area as you quest; monsters, chests, and doors may be hidden in the shadows. Carry a torch to light your way, and always check the bodies of slain enemies (SHIFT + LEFT CLICK)."); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"What is the Pentagram Quest(tm)?"); y+=5;
			y=dd_drawtext_break(10,y,202,graycolor,0,"The Pentagram Quest(tm) is a game that all players (from about level 10 and up) can play together. It's an ongoing game that takes place in 'the pents' - a large, cavernous area partitioned off according to player levels. The walls and floors of this area are covered with 'stars' (pentagrams) and the object of the game is to touch as many pentagrams as possible as you fight off the evil demons that inhabit the area. Once a randomly chosen number of pentagrams have been touched, the pents are 'solved', and you receive experience points for the pentagrams you touched. The entrances to the Pentagram Quest(tm) are southeast of blue squares in Aston - be sure to SHIFT + RIGHT CLICK on the doors to determine which level area is right for you!"); y+=10;
			break;
		
		case 13:
			y=dd_drawtext_break(10,y,202,whitecolor,0,"Frequently Asked Questions 3"); y+=15;
			y=dd_drawtext_break(10,y,202,whitecolor,0,"How do I kill other players?"); y+=5;
                        y=dd_drawtext_break(10,y,202,graycolor,0,"A PKer (Player Killer) is one that has chosen to kill other PKers - which also means that he can be killed by other PKers too. If you are killed, the items in your Equipment area and your Inventory can be taken by the one who killed you. You must be level 10 or higher to become a PKer. To enable your PK status, type: /playerkiller. To attack someone who is a playerkiller and within your level range, type: /hate <name> To disable your PK status, you must wait four (4) weeks since you last killed someone, then type: /playerkiller"); y+=10;

			y=dd_drawtext_break(10,y,202,whitecolor,0,"Another player is harassing me/making me uncomfortable. What can I do?"); y+=5;
                        y=dd_drawtext_break(10,y,202,graycolor,0,"If another player harasses you, type: /complain <player> <reason> This command sends a screenshot of your chat window to Game Management. Replace <player> with the name of the player bothering you. The <reason> portion of the command is for you to enter your own comments regarding the situation. Please be aware that only the last 80 lines of text are sent, and that each server-change (teleport) erases this buffer. You can also type: /ignore <name> to ignore the things that player is saying to you."); y+=10;
			break;
	}
	return y;
}*/

void display_pents(void)
{
	extern char pent_str[7][80];
	int n,col;

	for (n=0; n<7; n++) {
		switch(pent_str[n][0]) {
                        case '0':	col=graycolor; break;
			case '1':	col=redcolor; break;
			case '2':	col=greencolor; break;
			case '3':	col=bluecolor; break;

			default:	continue;
		}
		dd_drawtext(600,350+n*10,col,DD_SMALL|DD_FRAME,pent_str[n]+1);
	}
}

void display_otext(void)
{
	int n,cnt;
	int col;

	for (n=cnt=0; n<MAXOTEXT; n++) {
		if (!otext[n].text) continue;
		if (otext[n].type<3 && tick-otext[n].time>TICKS*5) continue;
		if (tick-otext[n].time>TICKS*65) continue;
		if (otext[n].type>1) { if (n==0) col=redcolor; else col=darkredcolor; }
		else { if (n==0) col=greencolor; else col=darkgreencolor; }
		dd_drawtext(400,420-cnt*12,col,DD_LARGE|DD_FRAME|DD_CENTER,otext[n].text);
		cnt++;
	}
}

void display_game(void)
{
	extern int dd_tick;
	extern int x_offset;
	extern int display_help,display_quest;

	dd_tick++;
		
	if (display_help || display_quest) {
		dd_push_clip();
		dd_more_clip(110,0,800-110,600);
		x_offset+=110;
	}

	
        display_game_spells();
        display_game_spells2();
        display_game_map(map);
        display_game_names();

	if (display_help || display_quest) {
		x_offset-=110;
		dd_pop_clip();
	
		dd_copysprite(995,0,40,DDFX_NLIGHT,DD_NORMAL);
		
                if (display_help) do_display_help(display_help);
		if (display_quest) do_display_questlog(display_quest);
	}	
        display_pents();
	display_otext();

#ifdef DEVELOPER
//        dd_drawtext_fmt(2,72,whitecolor,DD_SMALL|DD_FRAME,"sorts=%d used=%d",stat_dlsortcalls,stat_dlused);
#endif
}

// make quick

void display_quick(int q)
{
        int i,ii,r,g,b,sx=XRES/2,sy=YRES/2;
        unsigned short int *ptr;

        ptr=dd_lock_ptr();
        if (!ptr) return;

        for (i=0; i<maxquick+1; i++) {

                r=i*31/maxquick;
                g=15;
                b=r;

                safepix(ptr,sx+quick[i].mapx,sy+quick[i].mapy,IRGB(r,g,b));
        }

        for (ii=0; ii<9; ii++) {
                safepix(ptr,sx+quick[quick[q].qi[ii]].mapx,sy+quick[quick[q].qi[ii]].mapy,IRGB(31,31,31));
        }


        dd_unlock_ptr();
}

int quick_qcmp(const void *va, const void *vb)
{
        const QUICK *a;
        const QUICK *b;
        int cmp;

        a=(QUICK *)va;
        b=(QUICK *)vb;

        if (a->mapx+a->mapy<b->mapx+b->mapy) return -1;
        else if (a->mapx+a->mapy>b->mapx+b->mapy) return 1;

        return a->mapx-b->mapx;
}

void make_quick(int game)
{
        int t,cnt;
        int x,y,xs,xe,ys,ye,i,ii,iii;
        int scrx,scry;
        FILE *file;
        int dist=DIST;

        if (game) {
#ifdef EDITOR
                if (editor) return;
#endif
                set_mapoff(400,270,MAPDX,MAPDY);
                set_mapadd(0,0);
        }
#ifdef EDITOR
        else {
                note("make_quick: winxres=%d winyres=%d",winxres,winyres);
                dist=max(winxres/FDX,winyres/FDY+5);
                emapdx=2*dist+1;
                emapdy=2*dist+1;
                emap=xrealloc(emap,emapdx*emapdy*sizeof(*emap),MEM_EDIT);
                set_mapoff(winxres/2,winyres/2+2*FDY-15,dist*2+1,dist*2+1);
                set_mapadd(0,0);
        }
#endif

        // calc maxquick
        for (i=y=0; y<=dist*2; y++) {
                if (y<dist) { xs=dist-y; xe=dist+y; } else { xs=y-dist; xe=dist*3-y; }
		for (x=xs; x<=xe; x++) {
                        i++;
                }
        }
        maxquick=i;

        // set quick (and mn[4]) in server order
        quick=xrealloc(quick,(maxquick+1)*sizeof(QUICK),MEM_GAME);
        for (i=y=0; y<=dist*2; y++) {
                if (y<dist) { xs=dist-y; xe=dist+y; } else { xs=y-dist; xe=dist*3-y; }
		for (x=xs; x<=xe; x++) {
                        quick[i].mn[4]=x+y*(dist*2+1);

                        quick[i].mapx=x;
                        quick[i].mapy=y;
                        mtos(x,y,&quick[i].cx,&quick[i].cy);
                        i++;
                }
        }

        // sort quick in client order
        t=GetTickCount();
        qsort(quick,maxquick,sizeof(QUICK),quick_qcmp);

        // set quick neighbours
        t=GetTickCount();
        cnt=0;
        for(i=0; i<maxquick; i++) {
                for (y=-1; y<=1; y++) {
                        for (x=-1; x<=1; x++) {

                                if (x==1 || (x==0 && y==1)) {
                                        for (ii=i+1; ii<maxquick; ii++) if (quick[i].mapx+x==quick[ii].mapx && quick[i].mapy+y==quick[ii].mapy) break; else cnt++;
                                }
                                else if (x==-1 || (x==0 && y==-1)) {
                                        for (ii=i-1; ii>=0; ii--) if (quick[i].mapx+x==quick[ii].mapx && quick[i].mapy+y==quick[ii].mapy) break; else cnt++;
                                        if (ii==-1) ii=maxquick;
                                }
                                else {
                                        ii=i;
                                }

                                // for (iii=0; iii<maxquick; iii++) if (quick[i].mapx+x==quick[iii].mapx && quick[i].mapy+y==quick[iii].mapy) break;
                                // if (iii!=ii) note("%d%+d=%d/%d",quick[i].mapx,x,ii==maxquick?-42:quick[ii].mapx,iii==maxquick?-42:quick[iii].mapx);

                                if (ii==maxquick) {
                                        quick[i].mn[(x+1)+(y+1)*3]=0;
                                        quick[i].qi[(x+1)+(y+1)*3]=maxquick;
                                }
                                else {
                                        quick[i].mn[(x+1)+(y+1)*3]=quick[ii].mn[4];
                                        quick[i].qi[(x+1)+(y+1)*3]=ii;
                                }
                        }

                }
        }
        // set values for quick[maxquick]
        for (y=-1; y<=1; y++) {
                for (x=-1; x<=1; x++) {
                        quick[maxquick].mn[(x+1)+(y+1)*3]=0;
                        quick[maxquick].qi[(x+1)+(y+1)*3]=maxquick;
                }
        }
}

// init, exit

void init_game(void)
{
        make_quick(1);
}

void exit_game(void)
{
        xfree(quick);
        quick=NULL;
        maxquick=0;
        xfree(dllist);
        dllist=NULL;
        xfree(dlsort);
        dlsort=NULL;
        dlused=0;
        dlmax=0;


}

//---------------------------

void prefetch_game(int attick)
{
	set_map_values(map2,attick);
	display_game_map(map2);
	dl_prefetch(attick);
}
