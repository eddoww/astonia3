/*

$Id: sector.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: sector.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:41  sam
Added RCS tags


*/

#include <stdlib.h>

#include "server.h"
#include "mem.h"
#include "log.h"
#include "tool.h"
#include "sector.h"

#define MAXDOOR		1024

struct door
{
	int x,y;
	int nr1,nr2;
};

struct door2
{
	int x,y;
	int to;
};

struct sector2
{
	int door_cnt;
	struct door2 *door;
};

static int *sector[8];
static int *char_sector;
static int *sound_sector;
static int *shout_sector;

static int sec2_cnt=0;
static struct sector2 *sec2=NULL;

static struct door *door;

int init_sector(void)
{
	int n;

	for (n=0; n<8; n++) {
		sector[n]=xcalloc(MAXMAP*MAXMAP/((1<<n)*(1<<n))*sizeof(int),IM_BASE);
		xlog("Allocated sector %d: %.2fM (%d*%d)",n,MAXMAP*MAXMAP/((1<<n)*(1<<n))*sizeof(int)/1024.0/1024.0,sizeof(int),MAXMAP*MAXMAP/((1<<n)*(1<<n)));
		mem_usage+=MAXMAP*MAXMAP/((1<<n)*(1<<n))*sizeof(int);
	}
	
	char_sector=xcalloc(MAXMAP*MAXMAP/64*sizeof(int),IM_BASE);
	xlog("Allocated char sector: %.2fM (%d*%d)",MAXMAP*MAXMAP/64*sizeof(int)/1024.0/1024.0,sizeof(int),MAXMAP*MAXMAP/64);
	mem_usage+=MAXMAP*MAXMAP/64*sizeof(int);

	sound_sector=xcalloc(MAXMAP*MAXMAP*sizeof(int),IM_BASE);
	xlog("Allocated sound sector: %.2fM (%d*%d)",MAXMAP*MAXMAP*sizeof(int)/1024.0/1024.0,sizeof(int),MAXMAP*MAXMAP);
	mem_usage+=MAXMAP*MAXMAP*sizeof(int);

	shout_sector=xcalloc(MAXMAP*MAXMAP*sizeof(int),IM_BASE);
	xlog("Allocated shout sector: %.2fM (%d*%d)",MAXMAP*MAXMAP*sizeof(int)/1024.0/1024.0,sizeof(int),MAXMAP*MAXMAP);
	mem_usage+=MAXMAP*MAXMAP*sizeof(int);

        return 1;
}

void set_sector(int x,int y)
{
	int n,m;
	unsigned long long prof;

	prof=prof_start(21);

	for (n=0; n<8; n++) {
		m=(x>>n)+(y>>n)*(MAXMAP>>n);

		sector[n][m]=ticker;		
	}

	prof_stop(21,prof);
}

int skipx_sector(int x,int y)
{
	int n,m,size;

	for (n=7; n>=0; n--) {
		m=(x>>n)+(y>>n)*(MAXMAP>>n);
		
                if (sector[n][m]<ticker) {
			
			size=(1<<n);
			
                        return size-(x&(size-1));
		}
	}

	return 0;
}

void add_char_sector(int cn)
{
	int m,co;

	m=(ch[cn].x>>3)+(ch[cn].y>>3)*(MAXMAP>>3);
	
	co=ch[cn].sec_next=char_sector[m];
	ch[cn].sec_prev=0;
	
	if (co) ch[co].sec_prev=cn;
	
	char_sector[m]=cn;

	//xlog("added %d (%s) to sector %d",cn,ch[cn].name,m);
}

void del_char_sector(int cn)
{
	int prev,next,m;

	prev=ch[cn].sec_prev;
	next=ch[cn].sec_next;

	if (prev) ch[prev].sec_next=next;
        else {		
		m=(ch[cn].x>>3)+(ch[cn].y>>3)*(MAXMAP>>3);

		char_sector[m]=next;
	}
	
	
	if (next) ch[next].sec_prev=prev;

	//xlog("removed %d (%s) from sector %d",cn,ch[cn].name,m);
}

int getfirst_char_sector(int x,int y)
{
	int m;

	if (x<0 || x>=MAXMAP || y<0 || y>=MAXMAP) return 0;

	m=(x>>3)+(y>>3)*(MAXMAP>>3);

	return char_sector[m];
}

static void add_door(int x,int y,int nr)
{
	int n;

        for (n=0; n<MAXDOOR; n++) {
		if (door[n].x==x && door[n].y==y) {
			if (door[n].nr1==nr) return;
			if (door[n].nr2==nr) return;
			break;
		}
	}
	if (n==MAXDOOR) {
		for (n=0; n<MAXDOOR; n++) {
			if (!door[n].nr1) break;
		}
		if (n==MAXDOOR) {
			elog("Increase MAXDOOR!");
			exit(1);
		}
	}
	door[n].x=x;
	door[n].y=y;
	if (!door[n].nr1) door[n].nr1=nr;
	else if (!door[n].nr2) door[n].nr2=nr;
	else elog("add_door: three sectors reaching door?? (%d,%d,%d)",
		  nr,door[n].nr1,door[n].nr2);

}

struct pos
{
	unsigned short x,y;
};

static int add_sound_pos(int xs,int ys,int nr,struct pos *pos,int s)
{
	if (xs<0 || xs>=MAXMAP || ys<0 || ys>=MAXMAP) return s;
        if (map[xs+ys*MAXMAP].flags&MF_SOUNDBLOCK) return s;
        if (map[xs+ys*MAXMAP].flags&MF_TSOUNDBLOCK) {
                sound_sector[xs+ys*MAXMAP]=nr;
                add_door(xs,ys,nr);
		return s;
	}
	if (sound_sector[xs+ys*MAXMAP]) return s;

	sound_sector[xs+ys*MAXMAP]=nr;

	pos[s].x=xs;
	pos[s].y=ys;

        return s+1;
}

int fill_sound_sector(int xs,int ys,int nr)
{
	struct pos *pos;
	int s=0,x,y;

        if (xs<0 || xs>=MAXMAP || ys<0 || ys>=MAXMAP) return 0;
        if (map[xs+ys*MAXMAP].flags&MF_SOUNDBLOCK) return 0;
        if (map[xs+ys*MAXMAP].flags&MF_TSOUNDBLOCK) {
                sound_sector[xs+ys*MAXMAP]=nr;
                add_door(xs,ys,nr);
		return 0;
	}
	if (sound_sector[xs+ys*MAXMAP]) return 0;

	pos=xmalloc(sizeof(struct pos)*MAXMAP*MAXMAP*4,IM_TEMP);

	s=add_sound_pos(xs,ys,nr,pos,s);

        while (s) {
		s--;
		x=pos[s].x;
		y=pos[s].y;

                s=add_sound_pos(x+1,y,nr,pos,s);
		s=add_sound_pos(x-1,y,nr,pos,s);
		s=add_sound_pos(x,y+1,nr,pos,s);
		s=add_sound_pos(x,y-1,nr,pos,s);
	}

	xfree(pos);

	return 1;
}

static int add_shout_pos(int xs,int ys,int nr,struct pos *pos,int s)
{
	if (xs<0 || xs>=MAXMAP || ys<0 || ys>=MAXMAP) return s;
	if (map[xs+ys*MAXMAP].flags&MF_SHOUTBLOCK) return s;
	if (shout_sector[xs+ys*MAXMAP]) return s;

	shout_sector[xs+ys*MAXMAP]=nr;

	pos[s].x=xs;
	pos[s].y=ys;

        return s+1;
}

int fill_shout_sector(int xs,int ys,int nr)
{
	struct pos *pos;
	int s=0,x,y;

        if (xs<0 || xs>=MAXMAP || ys<0 || ys>=MAXMAP) return 0;
	if (map[xs+ys*MAXMAP].flags&MF_SHOUTBLOCK) return 0;
	if (shout_sector[xs+ys*MAXMAP]) return 0;

	pos=xmalloc(sizeof(struct pos)*MAXMAP*MAXMAP*4,IM_TEMP);

	s=add_shout_pos(xs,ys,nr,pos,s);

        while (s) {
		s--;
		x=pos[s].x;
		y=pos[s].y;

                s=add_shout_pos(x+1,y,nr,pos,s);
		s=add_shout_pos(x-1,y,nr,pos,s);
		s=add_shout_pos(x,y+1,nr,pos,s);
		s=add_shout_pos(x,y-1,nr,pos,s);
	}

	xfree(pos);

        return 1;
}

void init_sound_sector(void)
{
	int x,y,nr,n,to;

	xlog("Calculating shout sectors");
	door=xcalloc(sizeof(struct door)*MAXDOOR,IM_TEMP);

	nr=1;
	for (y=0; y<MAXMAP; y++) {
		for (x=0; x<MAXMAP; x++) {
			if (!sound_sector[x+y*MAXMAP]) {
				if (fill_shout_sector(x,y,nr)) {
					//xlog("shout sector %d at %d,%d",nr,x,y);
					nr++;
				}
			}
		}
	}

	xlog("%d shout sectors found",nr-1);

	xlog("Calculating talk sectors");
	nr=1;
	for (y=0; y<MAXMAP; y++) {
		for (x=0; x<MAXMAP; x++) {
			if (!sound_sector[x+y*MAXMAP]) {
				if (fill_sound_sector(x,y,nr)) {
					//xlog("sector %d at %d,%d",nr,x,y);
					nr++;
				}
			}
		}
	}
	xlog("%d talk sectors found",nr-1);

	sec2_cnt=nr;
	sec2=xcalloc(sizeof(struct sector2)*sec2_cnt,IM_BASE);
	xlog("Allocated sector2: %.2fM (%d*%d)",sec2_cnt*sizeof(struct sector2)/1024.0/1024.0,sizeof(struct sector2),sec2_cnt);
	mem_usage+=sec2_cnt*sizeof(struct sector2);

        for (n=0; n<MAXDOOR; n++) {
		if (door[n].nr1==0 && door[n].nr2==0) continue;
		
		//xlog("door %d: %d,%d connecting %d with %d",n,door[n].x,door[n].y,door[n].nr1,door[n].nr2);

                nr=door[n].nr1; to=door[n].nr2;
		sec2[nr].door_cnt++;
		sec2[nr].door=xrealloc(sec2[nr].door,sec2[nr].door_cnt*sizeof(struct door2),IM_BASE);
		sec2[nr].door[sec2[nr].door_cnt-1].to=to;
		sec2[nr].door[sec2[nr].door_cnt-1].x=door[n].x;
		sec2[nr].door[sec2[nr].door_cnt-1].y=door[n].y;

		nr=door[n].nr2; to=door[n].nr1;
		sec2[nr].door_cnt++;
		sec2[nr].door=xrealloc(sec2[nr].door,sec2[nr].door_cnt*sizeof(struct door2),IM_BASE);
		sec2[nr].door[sec2[nr].door_cnt-1].to=to;
		sec2[nr].door[sec2[nr].door_cnt-1].x=door[n].x;
		sec2[nr].door[sec2[nr].door_cnt-1].y=door[n].y;
	}

	xfree(door); door=NULL;
}

int sector_follow_door(int s1,int s2,int *stack,int s)
{
	int n;

        if (s1==s2) return 1;
	if (s==10) return 0;

	for (n=0; n<s; n++) {
		if (s1==stack[n]) return 0;
	}
	
	stack[s]=s1;

	for (n=0; n<sec2[s1].door_cnt; n++) {
		if (map[sec2[s1].door[n].x+sec2[s1].door[n].y*MAXMAP].flags&MF_TSOUNDBLOCK) continue;
		if (sector_follow_door(sec2[s1].door[n].to,s2,stack,s+1)) return 1;
	}

	return 0;
}

int sector_hear(int xf,int yf,int xt,int yt)
{
	int s1,s2,tmp;
	int stack[10]={0,0,0,0,0,0,0,0,0,0};
	unsigned long long prof;

	prof=prof_start(2);

	s1=sound_sector[xf+yf*MAXMAP];
	s2=sound_sector[xt+yt*MAXMAP];

        tmp=sector_follow_door(s1,s2,stack,0);

	prof_stop(2,prof);

	return tmp;
}

int sector_hear_shout(int xf,int yf,int xt,int yt)
{
	int s1,s2;

        s1=shout_sector[xf+yf*MAXMAP];
	s2=shout_sector[xt+yt*MAXMAP];

        return s1==s2;
}

