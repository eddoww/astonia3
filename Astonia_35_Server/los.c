/*

$Id: los.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: los.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:20  sam
Added RCS tags


*/

#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "log.h"
#include "light.h"
#include "mem.h"
#include "sector.h"
#include "los.h"

struct los *los;

static int dodoors=0,doorx,doory;

int init_los(void)
{
	los=xcalloc(sizeof(struct los)*MAXCHARS,IM_BASE);
	if (!los) return 0;
	xlog("Allocated los: %.2fM (%d*%d)",sizeof(struct los)*MAXCHARS/1024.0/1024.0,sizeof(struct los),MAXCHARS);
	mem_usage+=sizeof(struct los)*MAXCHARS;

	return 1;
}

static inline void add_los(int cn,int x,int y,int v)
{
        if (!los[cn].tab[x+los[cn].xoff][y+los[cn].yoff]) los[cn].tab[x+los[cn].xoff][y+los[cn].yoff]=v;
}

static inline int check_map(int x,int y)
{
        int m;

        if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) return 0;

        m=x+y*MAXMAP;

	if (dodoors && (map[m].flags&MF_DOOR) && (x!=doorx || y!=doory)) return 1;

        if (map[m].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) return 0;

        return 1;
}

static inline int is_close_los_down(int cn,int xc,int yc,int v)
{
	int x,y;

        x=xc+los[cn].xoff;
	y=yc+los[cn].yoff;

        if (los[cn].tab[x][y+1]==v && check_map(xc,yc+1)) return 1;
        if (los[cn].tab[x+1][y+1]==v && check_map(xc+1,yc+1)) return 1;
        if (los[cn].tab[x-1][y+1]==v && check_map(xc-1,yc+1)) return 1;

        return 0;
}

static inline int is_close_los_up(int cn,int xc,int yc,int v)
{
	int x,y;

        x=xc+los[cn].xoff;
	y=yc+los[cn].yoff;

        if (los[cn].tab[x][y-1]==v && check_map(xc,yc-1)) return 1;
        if (los[cn].tab[x+1][y-1]==v && check_map(xc+1,yc-1)) return 1;
        if (los[cn].tab[x-1][y-1]==v && check_map(xc-1,yc-1)) return 1;

        return 0;
}

static inline int is_close_los_left(int cn,int xc,int yc,int v)
{
	int x,y;

        x=xc+los[cn].xoff;
	y=yc+los[cn].yoff;

        if (los[cn].tab[x+1][y]==v && check_map(xc+1,yc)) return 1;
        if (los[cn].tab[x+1][y+1]==v && check_map(xc+1,yc+1)) return 1;
        if (los[cn].tab[x+1][y-1]==v && check_map(xc+1,yc-1)) return 1;

        return 0;
}

static inline int is_close_los_right(int cn,int xc,int yc,int v)
{
	int x,y;

        x=xc+los[cn].xoff;
	y=yc+los[cn].yoff;

        if (los[cn].tab[x-1][y]==v && check_map(xc-1,yc)) return 1;
        if (los[cn].tab[x-1][y+1]==v && check_map(xc-1,yc+1)) return 1;
        if (los[cn].tab[x-1][y-1]==v && check_map(xc-1,yc-1)) return 1;

        return 0;
}

static inline int check_los(int cn,int x,int y)
{
        x=x+los[cn].xoff;
        y=y+los[cn].yoff;

        return los[cn].tab[x][y];
}

static void build_los(int cn,int xc,int yc,int maxdist)
{
        int x,y,dist,found;
	unsigned long long prof;

        prof=prof_start(15);

        bzero(los[cn].tab,sizeof(los[cn].tab));

	los[cn].xoff=MAXDIST-xc;
	los[cn].yoff=MAXDIST-yc;
	los[cn].maxdist=maxdist;

        add_los(cn,xc,yc,1);

	add_los(cn,xc+1,yc,2);
	add_los(cn,xc-1,yc,2);
	add_los(cn,xc,yc+1,2);
	add_los(cn,xc,yc-1,2);

	add_los(cn,xc+1,yc+1,2);
	add_los(cn,xc+1,yc-1,2);
	add_los(cn,xc-1,yc+1,2);
	add_los(cn,xc-1,yc-1,2);

        for (dist=2; dist<maxdist; dist++) {
		found=0;
                for (x=xc-dist; x<=xc+dist; x++) {
                        if (is_close_los_down(cn,x,yc-dist,dist)) { add_los(cn,x,yc-dist,dist+1); found=1; }
                        if (is_close_los_up(cn,x,yc+dist,dist)) { add_los(cn,x,yc+dist,dist+1); found=1; }
                }
                for (y=yc-dist; y<=yc+dist; y++) {
                        if (is_close_los_left(cn,xc-dist,y,dist)) { add_los(cn,xc-dist,y,dist+1); found=1; }
                        if (is_close_los_right(cn,xc+dist,y,dist)) { add_los(cn,xc+dist,y,dist+1); found=1; }
                }
		if (!found) break;		
        }
	prof_stop(15,prof);
}

void reset_los(int xc,int yc)
{
        int x,y,cn;
	unsigned long long prof;

        prof=prof_start(14);

        for (y=max(0,yc-MAXDIST); y<=min(MAXMAP-1,yc+MAXDIST); y++) {	
                for (x=max(0,xc-MAXDIST); x<=min(MAXMAP-1,xc+MAXDIST); x++) {
			if ((cn=map[x+y*MAXMAP].ch)!=0) {
				los[cn].x=los[cn].y=0;				
			}
		}
	}

	if (abs(los[0].x-xc)<=LIGHTDIST && abs(los[0].y-yc)<=LIGHTDIST)
		los[0].x=los[0].y=0;

	prof_stop(14,prof);
}

// updates the LOS cache for character cn
// returns true if los had to be rebuild, false otherwise
int update_los(int cn,int xc,int yc,int maxdist)
{
	if (maxdist>MAXDIST) maxdist=MAXDIST;	

	if (xc!=los[cn].x || yc!=los[cn].y || maxdist>los[cn].maxdist) {
		build_los(cn,xc,yc,maxdist);
		los[cn].x=xc;
		los[cn].y=yc;
		return 1;
	}
	return 0;
}

int los_can_see(int cn,int xc,int yc,int tx,int ty,int maxdist)
{
        int tmp;
        unsigned long long prof;

        prof=prof_start(13);

	if (abs(xc-tx)>=maxdist || abs(yc-ty)>=maxdist) { prof_stop(13,prof); return 0; }

	if (maxdist>MAXDIST) maxdist=MAXDIST;	

	if (xc!=los[cn].x || yc!=los[cn].y || maxdist>los[cn].maxdist) {
		build_los(cn,xc,yc,maxdist);
		los[cn].x=xc;
		los[cn].y=yc;
	}

        tmp=check_los(cn,tx,ty);
	if (tmp>=maxdist) tmp=0;

	prof_stop(13,prof);

        return tmp;
}

int door_los(int cn,int xc,int yc,int tx,int ty,int maxdist,int dx,int dy)
{
	int tmp;

	dodoors=1;
	doorx=dx;
	doory=dy;

	tmp=los_can_see(cn,xc,yc,tx,ty,maxdist);

	dodoors=0;

	return tmp;
}

//--------------------------

/*
int los_can_see2(int cn,int sx,int sy,int tx,int ty,int maxdist)
{
	int x,y,dx,dy,start,stop,start1,start2,stop1,stop2,state,firstvis,lastvis;

        dx=abs(sx-tx); dy=abs(sy-ty);

	if (dx>=dy) {

	} else {
		if (sy>ty) { y=ty; ty=sy; sy=y; x=tx; tx=sx; dx=x; }

		for (y=sy; y<=ty; y++) {
			start1=sx-(y-sy);
                        start2=tx-(ty-y);
			
			stop1=sx+(y-sy);
                        stop2=tx+(ty-y);

			start=max(start1,start2);
			stop=min(stop1,stop2);

			//xlog("y=%d, start=%d, stop=%d",y,start,stop);

			firstvis=lastvis=0;
                        for (x=start; x<=stop; x++) {				
				if (!(map[m].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) {	// visible
					if (!firstvis) firstvis=x;
                                        lastvis=x;
				}
			}
			if (!firstvis) return 0;

		}
	}
	
	return 42;
}*/
