/*

$Id: light.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: light.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:18  sam
Added RCS tags


*/

/*
light.c (C) 2001 D.Brockhaus

And there be light! This file contains all basic functions dealing with light
on the map. The map has two entries for light, .light and .dlight.

.light is the light emitted by items (like torches) and by characters (who are
wearing torches).

.dlight is short for daylight. This is the percentage (well, actually 0...255)
of daylight which can reach a map tile. This is needed for things like entrances
to dark places, to make the outside light diminis slowly, the deeper into the dark
area it gets.
*/

#include <stdlib.h>
#include <math.h>

#include "server.h"
#include "log.h"
#define NEED_FAST_LOS_LIGHT
#include "los.h"
#undef NEED_FAST_LOS_LIGHT
#include "sector.h"
#include "consistency.h"
#include "btrace.h"
#include "light.h"

static inline void map_add_light(int x,int y,int v)
{
        register unsigned int m;

        m=x+y*MAXMAP;

        map[m].light+=v;

        if (map[m].light<0) {
		//elog("Error in light computations at %d,%d (%d%+d=%d).",x,y,map[m].light-v,v,map[m].light);
		//btrace("map_add_light");
                map[m].light=0;
        }
}

// add a light with the power of stren to xc,yc. stren may be
// negative to remove a light.
// cn is optional and can be given to speed up LOS
static void add_light(int xc,int yc,int stren,int cn)
{
        int x,y,xs,ys,xe,ye,d,flag,dist;
        unsigned long long prof;

	if (!stren) return;

	//if (cn) xlog("light: %03d, %03d: %+04d (%d, %s)",xc,yc,stren,cn,cn ? ch[cn].name : "");

        prof=prof_start(16);

        // add light at center, we need to do this here as it blows the calculation below (divide by zero)
        map_add_light(xc,yc,stren);
	set_sector(xc,yc);

	if (stren<0) { flag=1; stren=-stren; }
        else flag=0;

	if (stren>100) stren=100;

	dist=sqrt(stren-1)+1;
	//xlog("stren=%d, dist=%d",stren,dist);

	// trigger recomputation of LOS
	los_can_see(cn,xc,yc,xc,yc,dist);

        xs=max(0,xc-dist);
        ys=max(0,yc-dist);
        xe=min(MAXMAP-1,xc+1+dist);
        ye=min(MAXMAP-1,yc+1+dist);

        for (y=ys; y<ye; y++) {
                for (x=xs; x<xe; x++) {
                        if (x==xc && y==yc) continue;
                        //if ((xc-x)*(xc-x)+(yc-y)*(yc-y)>(LIGHTDIST*LIGHTDIST+1)) continue;
				
                        if (fast_los_light(cn,x,y,dist)!=0) {
                                d=stren/((xc-x)*(xc-x)+(yc-y)*(yc-y)+1);
				if (d) {
					if (flag) map_add_light(x,y,-d);
					else map_add_light(x,y,d);
					set_sector(x,y);

				}
                        }
                }
        }
	
        prof_stop(16,prof);
}

// add the light emitted by a character to the map at his current position
void add_char_light(int cn)
{
	int light,m;

	if (ch[cn].x<1 || ch[cn].x>=MAXMAP-2 || ch[cn].y<1 || ch[cn].y>=MAXMAP-2) return;
	m=ch[cn].x+ch[cn].y*MAXMAP;

	light=ch[cn].value[0][V_LIGHT];

	if (light>0 && !(map[m].flags&MF_NOLIGHT)) add_light(ch[cn].x,ch[cn].y,light,cn);
}

// remove the light emitted by a character to the map at his current position
void remove_char_light(int cn)
{
	int light,m;

	if (ch[cn].x<1 || ch[cn].x>=MAXMAP-2 || ch[cn].y<1 || ch[cn].y>=MAXMAP-2) return;
	m=ch[cn].x+ch[cn].y*MAXMAP;

	light=ch[cn].value[0][V_LIGHT];

	if (light>0 && !(map[m].flags&MF_NOLIGHT)) add_light(ch[cn].x,ch[cn].y,-light,cn);	
}

// add the light emitted by an item to the map at its current position
void add_item_light(int in)
{
	int light=0,n,m;

	if (it[in].x<1 || it[in].x>=MAXMAP-2 || it[in].y<1 || it[in].y>=MAXMAP-2) return;
	m=it[in].x+it[in].y*MAXMAP;

	for (n=0; n<MAXMOD; n++) {
		if (it[in].mod_index[n]==V_LIGHT)
			light+=it[in].mod_value[n];
	}
	if (light>0 && (!(it[in].flags&IF_TAKE) || !(map[m].flags&MF_NOLIGHT))) add_light(it[in].x,it[in].y,light,0);
}

// remove the light emitted by an item to the map at its current position
void remove_item_light(int in)
{
	int light=0,n,m;

	if (it[in].x<1 || it[in].x>=MAXMAP-2 || it[in].y<1 || it[in].y>=MAXMAP-2) return;
	m=it[in].x+it[in].y*MAXMAP;

	for (n=0; n<MAXMOD; n++) {
		if (it[in].mod_index[n]==V_LIGHT)
			light+=it[in].mod_value[n];
	}
	if (light>0 && (!(it[in].flags&IF_TAKE) || !(map[m].flags&MF_NOLIGHT))) {
		add_light(it[in].x,it[in].y,-light,0);
	}
}

// add the light emitted by an effect to the map at the position given
void add_effect_light(int x,int y,int light)
{
	int m;

	if (x<1 || x>=MAXMAP-2 || y<1 || y>=MAXMAP-2) return;
	m=x+y*MAXMAP;

        if (light>0 && !(map[m].flags&MF_NOLIGHT)) add_light(x,y,light,0);
}

// remove the light emitted by an effect to the map at the position given
void remove_effect_light(int x,int y,int light)
{
	int m;

	if (x<1 || x>=MAXMAP-2 || y<1 || y>=MAXMAP-2) return;
	m=x+y*MAXMAP;

        if (light>0 && !(map[m].flags&MF_NOLIGHT)) add_light(x,y,-light,0);
}

void compute_groundlight(int x,int y)
{
	int m;

	m=y*MAXMAP+x;
	
	if ((map[m].gsprite&0xffff)==14361 || (map[m].gsprite&0xffff)==14353 ||
	    ((map[m].gsprite&0xffff)>=12163 && (map[m].gsprite&0xffff)<=12166)) {	// lava ground tiles
		add_light(x,y,64,0);
	}
}

void compute_shadow(int xc,int yc)
{
	int x,y,xs,ys,xe,ye,m;
	int shadow=0,tmp,dorand=0;

	xs=max(0,xc-3);
        ys=max(0,yc-3);
        xe=min(MAXMAP-1,xc+1+3);
        ye=min(MAXMAP-1,yc+1+3);

	for (y=ys; y<ye; y++) {
		m=y*MAXMAP+xs;
                for (x=xs; x<xe; x++,m++) {
			//if (xc==x && yc==y) continue;
			
                        if (map[m].fsprite>=20276 && map[m].fsprite<=20282) { shadow+=7-abs(xc-x)-abs(yc-y); dorand=1; }
			if (map[m].fsprite>=21410 && map[m].fsprite<=21427) { shadow+=31-(abs(xc-x)+abs(yc-y))*5; dorand=1; }
			if (map[m].fsprite>=16000 && map[m].fsprite<=16007) { shadow+=31-(abs(xc-x)+abs(yc-y))*5; dorand=1; }
		}
	}
	for (x=xc-1,m=yc*MAXMAP+xc-1; x>=xs; x--,m--) {
		if ((map[m].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) { shadow+=47-abs(xc-x)*10; break; }
	}
        if (dorand) tmp=512/(shadow/2+RANDOM(shadow/2+1)+1);
	else tmp=512/(shadow+1);
	map[xc+yc*MAXMAP].dlight=min(63,tmp);
}

// compute the dlight value for the position xc,yc
int compute_dlight(int xc,int yc)
{
        int xs,ys,xe,ye,x,y,v,d,best=0,m;
        unsigned long long prof;

	if (!(map[xc+yc*MAXMAP].flags&MF_INDOORS)) return 0;	// nothing to compute, dude!

        prof=prof_start(30);

        xs=max(0,xc-LIGHTDIST);
        ys=max(0,yc-LIGHTDIST);
        xe=min(MAXMAP-1,xc+1+LIGHTDIST);
        ye=min(MAXMAP-1,yc+1+LIGHTDIST);

        for (y=ys; y<ye; y++) {
                m=y*MAXMAP+xs;
                for (x=xs; x<xe; x++,m++) {
                        if ((xc-x)*(xc-x)+(yc-y)*(yc-y)>(LIGHTDIST*LIGHTDIST+1)) continue;
                        if (!(map[m].flags&MF_INDOORS)) {
                                if ((v=los_can_see(0,xc,yc,x,y,LIGHTDIST))==0) continue;				
                                //d=256/(v*(abs(xc-x)+abs(yc-y)));
				d=256/((xc-x)*(xc-x)+(yc-y)*(yc-y)+1);
                                if (d>best) best=d;
				if (best>63) break;				
                        }			
                }
		if (best>63) break;
        }
        if (best>63) best=63;
	
	if (map[xc+yc*MAXMAP].dlight!=best) {
                map[xc+yc*MAXMAP].dlight=best;
                set_sector(xc,yc);
                prof_stop(30,prof);
                return 1;
	} else {
		prof_stop(30,prof);
                return 0;
	}
}

// compute the dlight values around xc,yc. this is used when the
// LOS (line of sight) changes, e.g. when a door was opened/closed.
int reset_dlight(int xc,int yc)
{
	int xs,ys,xe,ye,x,y,m,have_indoors=0,have_outdoors=0,update=0,change=0;
        unsigned long long prof;

        prof=prof_start(31);

        xs=max(0,xc-LIGHTDIST);
        ys=max(0,yc-LIGHTDIST);
        xe=min(MAXMAP-1,xc+1+LIGHTDIST);
        ye=min(MAXMAP-1,yc+1+LIGHTDIST);

	// we only have to update if the field contains both indoor and outdoor tiles
	// so we check if both are present
	for (y=ys; y<ye && !update; y++) {
		m=y*MAXMAP+xs;
                for (x=xs; x<xe; x++,m++) {
			if (map[m].flags&MF_INDOORS) have_indoors++;
			else have_outdoors++;

			if (have_indoors && have_outdoors) { update=1; break; }
		}
	}

	if (update) {	// re-compute only if needed
		for (y=ys; y<ye; y++) {
			m=y*MAXMAP+xs;
			for (x=xs; x<xe; x++,m++) {
				if (map[m].flags&MF_INDOORS) change|=compute_dlight(x,y);
			}
		}
	}

	//xlog("change=%d",change);

        prof_stop(31,prof);

	return change;
}

// remove all lights around xc,yc. you MUST call this routine BEFORE
// changing LOS. otherwise, the light on the map will be messed up.
int remove_lights(int xc,int yc)
{
	int xs,ys,xe,ye,x,y,m;
	int cn,in,fn;
        unsigned long long prof;

        prof=prof_start(32);

        xs=max(0,xc-LIGHTDIST);
        ys=max(0,yc-LIGHTDIST);
        xe=min(MAXMAP-1,xc+1+LIGHTDIST);
        ye=min(MAXMAP-1,yc+1+LIGHTDIST);

        for (y=ys; y<ye; y++) {
		m=y*MAXMAP+xs;
                for (x=xs; x<xe; x++,m++) {
			if ((cn=map[m].ch)) remove_char_light(cn);
			if ((in=map[m].it)) remove_item_light(in);
			if ((fn=map[m].ef[0]) && ef[fn].light) remove_effect_light(x,y,ef[fn].light);
			if ((fn=map[m].ef[1]) && ef[fn].light) remove_effect_light(x,y,ef[fn].light);
			if ((fn=map[m].ef[2]) && ef[fn].light) remove_effect_light(x,y,ef[fn].light);
			if ((fn=map[m].ef[3]) && ef[fn].light) remove_effect_light(x,y,ef[fn].light);
                }
        }

        prof_stop(32,prof);

	return 1;
}

// add all lights around xc,yc. has to be called after calling
// remove_lights() and changing LOS.
int add_lights(int xc,int yc)
{
	int xs,ys,xe,ye,x,y,m;
	int cn,in,fn;
        unsigned long long prof;

        prof=prof_start(33);

        xs=max(0,xc-LIGHTDIST);
        ys=max(0,yc-LIGHTDIST);
        xe=min(MAXMAP-1,xc+1+LIGHTDIST);
        ye=min(MAXMAP-1,yc+1+LIGHTDIST);

        for (y=ys; y<ye; y++) {
		m=y*MAXMAP+xs;
                for (x=xs; x<xe; x++,m++) {
			if ((cn=map[m].ch)) add_char_light(cn);
			if ((in=map[m].it)) add_item_light(in);
			if ((fn=map[m].ef[0]) && ef[fn].light) add_effect_light(x,y,ef[fn].light);
			if ((fn=map[m].ef[1]) && ef[fn].light) add_effect_light(x,y,ef[fn].light);
			if ((fn=map[m].ef[2]) && ef[fn].light) add_effect_light(x,y,ef[fn].light);
			if ((fn=map[m].ef[3]) && ef[fn].light) add_effect_light(x,y,ef[fn].light);
                }
        }

        prof_stop(33,prof);

	return 1;
}
