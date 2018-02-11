/*
 * $Id: effect.c,v 1.7 2008/01/03 10:10:49 devel Exp $
 *
 * $Log: effect.c,v $
 * Revision 1.7  2008/01/03 10:10:49  devel
 * added damage reduction when walking to LF
 *
 * Revision 1.6  2007/12/30 12:50:42  devel
 * LF damage per enemy scaled down for many enemies
 *
 * Revision 1.5  2007/09/11 17:07:46  devel
 * LF stronger against multiple enemies
 *
 * Revision 1.4  2007/05/09 11:34:07  devel
 * fixed bug in fireball distance
 *
 * Revision 1.3  2007/05/02 13:05:49  devel
 * fireball will no longer hit targets next to caster
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include "server.h"
#include "log.h"
#include "notify.h"
#include "death.h"
#include "light.h"
#include "tool.h"
#include "spell.h"
#include "los.h"
#include "mem.h"
#include "sector.h"
#include "create.h"
#include "item_id.h"
#include "talk.h"
#include "balance.h"
#include "drvlib.h"
#include "map.h"
#include "consistency.h"
#include "database.h"
#include "effect.h"
#include "clan.h"
#include "act.h"

// ********** effect allocation and freeing *************

static struct effect *ueffect,*feffect;

int used_effects,efserial=1;

// get new effect from free list
// and add it to used list
int alloc_effect(int type)
{
	int fn;
	struct effect *tmp;

	// get free effect
	tmp=feffect;
	if (!tmp) { elog("alloc_effect: MAXEFFECT reached!"); return 0; }
	feffect=tmp->next;
	fn=tmp-ef;
	
	// flags effect as used
        ef[fn].type=type;
	ef[fn].start=ticker;

	// add effect to used list
	ef[fn].next=ueffect;
	ef[fn].prev=NULL;
	if (ueffect) ueffect->prev=ef+fn;
	ueffect=ef+fn;

	used_effects++;

	ef[fn].serial=efserial++;

        return fn;
}

// release (delete) effect to free list
// and remove it from used list
void free_effect(int fn)
{
	struct effect *next,*prev;

	if (!ef[fn].type) {
		elog("trying to free already free effect %d",fn);
		return;
	}
	
	// remove character from used list
        prev=ef[fn].prev;
	next=ef[fn].next;

        if (prev) prev->next=next;
	else ueffect=next;

	if (next) next->prev=prev;

	// add effect to free list
        ef[fn].next=feffect;
	feffect=ef+fn;

	// flag effect as unused
	ef[fn].type=0;

	ef[fn].serial=0;

	used_effects--;
}

// get first effect in used list
int getfirst_effect(void)
{
        if (!ueffect) return 0;	
	return ueffect-ef;
}

// get next effect in used list
// (following a getfirst)
int getnext_effect(int fn)
{
	struct effect *tmp;

        if (!(tmp=ef[fn].next)) return 0;

        return tmp-ef;	
}

// *********** basic effect helpers ***********************

// add a marker on the map on x,y which links back to effect fn
int set_effect_map(int fn,int x,int y)
{
	int m,n;

	if (fn<1 || fn>=MAXEFFECT) return 0;	
	if (!ef[fn].type) return 0;

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return 0;

	m=x+y*MAXMAP;

	if (ef[fn].field_cnt>=MAXFIELD) return 0;

	for (n=0; n<4; n++) {
		if (!map[m].ef[n]) break;
	}
	if (n==4) return 0;

        map[m].ef[n]=fn;

	if (ef[fn].light) add_effect_light(x,y,ef[fn].light);

	ef[fn].m[ef[fn].field_cnt]=m;
	ef[fn].field_cnt++;
	
	set_sector(x,y);

	return 1;
}

// remove link to effect fn from map tile m
int remove_effect_map(int fn,int m)
{
	int n;
	
	if (m<0 || m>=MAXMAP*MAXMAP) return 0;
	
	for (n=0; n<4; n++) {
		if (map[m].ef[n]==fn) break;
	}
	if (n==4) return 0;

	if (ef[fn].light) remove_effect_light(m%MAXMAP,m/MAXMAP,ef[fn].light);
	
	map[m].ef[n]=0;

	set_sector(m%MAXMAP,m/MAXMAP);

	return 1;
}

int add_effect_char(int fn,int cn)
{
	int n;

	if (fn<1 || fn>=MAXEFFECT) return 0;	
	if (!ef[fn].type) return 0;

	if (cn<1 || cn>=MAXCHARS) return 0;

	for (n=0; n<4; n++) {
		if (!ch[cn].ef[n]) break;
	}
	if (n==4) return 0;

        ch[cn].ef[n]=fn;
	ef[fn].efcn=cn;

	if (ef[fn].light) update_char(cn);
	else set_sector(ch[cn].x,ch[cn].y);

	return 1;
}

int remove_effect_char(int fn)
{
	int n,cn;
	
	cn=ef[fn].efcn;

	if (cn<1 || cn>=MAXCHARS) return 0;

	for (n=0; n<4; n++) {
		if (ch[cn].ef[n]==fn) break;
	}
	if (n==4) return 0;

	ch[cn].ef[n]=0;
	ef[fn].efcn=0;

	if (ef[fn].light) update_char(cn);
	else set_sector(ch[cn].x,ch[cn].y);

	return 1;
}

int destroy_effect_type(int cn,int type)
{
	int n,fn=0,flag=0;
	
        if (cn<1 || cn>=MAXCHARS) return 0;

	for (n=0; n<4; n++) {
		if ((fn=ch[cn].ef[n]) && ef[fn].type==type) {
			flag=1;
			ch[cn].ef[n]=0;
			ef[fn].efcn=0;
			free_effect(fn);
		}
	}
        if (flag) {
		if (ef[fn].light) update_char(cn);
		else set_sector(ch[cn].x,ch[cn].y);
		return 1;
	}

	return 0;
}

// completely remove effect fn from
// the map
int remove_effect(int fn)
{
	int n;

	if (fn<1 || fn>=MAXEFFECT) return 0;	
	if (!ef[fn].type) return 0;

	for (n=0; n<ef[fn].field_cnt; n++) {
		remove_effect_map(fn,ef[fn].m[n]);
	}

	ef[fn].field_cnt=0;

	if (ef[fn].efcn) remove_effect_char(fn);

	return 1;
}

int effect_changed(int fn)
{
	int n;

	if (fn<1 || fn>=MAXEFFECT) return 0;	
	if (!ef[fn].type) return 0;

	for (n=0; n<ef[fn].field_cnt; n++) {
		set_sector(ef[fn].m[n]%MAXMAP,ef[fn].m[n]/MAXMAP);
	}

        return 1;
}

// ****** Fireball and helpers ***********

static void ef_fireball_set(int fn,int x,int y)
{
	int m,co,cn,dam;

	if (fn<1 || fn>=MAXEFFECT) return;	
	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return;

	m=x+y*MAXMAP;
	
	if ((co=map[m].ch)) {
		cn=ef[fn].cn;
		
		if (!can_attack(cn,co)) return;

                if (ch[co].flags&CF_EDEMON) {	// earth demons shoot back
			create_fireball(co,ch[co].x,ch[co].y,ch[cn].x,ch[cn].y,ef[fn].strength-1);
		}
		dam=fireball_damage(cn,co,ef[fn].strength);
                hurt(co,dam,cn,10,60,85);
	}
}

int checkhit_fireball(int x,int y)
{
	int m;

	m=x+y*MAXMAP;

	if (map[m].ch) return map[m].ch;

	return 0;
}

static void ef_fireball_explode(int fn,int x,int y)
{
	int fn2;

        remove_effect(fn);

        ef_fireball_set(fn,x,y);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x+1,y,5)) ef_fireball_set(fn,x+1,y);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x-1,y,5)) ef_fireball_set(fn,x-1,y);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x,y+1,5)) ef_fireball_set(fn,x,y+1);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x,y-1,5)) ef_fireball_set(fn,x,y-1);

	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x+1,y+1,5)) ef_fireball_set(fn,x+1,y+1);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x+1,y-1,5)) ef_fireball_set(fn,x+1,y-1);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x-1,y+1,5)) ef_fireball_set(fn,x-1,y+1);
	if (los_can_see(0,ef[fn].lx,ef[fn].ly,x-1,y-1,5)) ef_fireball_set(fn,x-1,y-1);	

	free_effect(fn);

	fn2=create_explosion(8,50050);
	add_explosion(fn2,ef[fn].lx,ef[fn].ly);	

	sound_area(ef[fn].lx,ef[fn].ly,6);
}

int fire_map_block(int m)
{
	if (map[m].flags&MF_TMOVEBLOCK) return 1;
	if (map[m].flags&MF_FIRETHRU) return 0;
	if (map[m].flags&MF_MOVEBLOCK) return 1;

	return 0;
}

int ball_map_block(int m)
{
	if (map[m].flags&MF_TMOVEBLOCK) return 1;
	if (map[m].flags&MF_FIRETHRU) return 0;
	if (map[m].flags&MF_MOVEBLOCK) return 1;

	return 0;
}

static int fire_char(int m,int cn)
{
	if (map[m].ch==cn) return 1;
	if (ch[cn].tox+ch[cn].toy*MAXMAP==m) return 1;
	
	return 0;
}
static void ef_fireball(int fn)
{
	int m,dx,dy,n,x,y,cn,remo=0;

	cn=ef[fn].cn;
	if (cn && (!ch[cn].flags || ch[cn].serial!=ef[fn].sercn)) remo=1;	// different char

	// maximum age for a fireball is TICKS, which makes its range 24 tiles
	if (remo || ticker>=ef[fn].stop) {	
		remove_effect(fn);
		free_effect(fn);
		return;
	}

        while (ef[fn].field_cnt) {
		ef[fn].field_cnt--;
		remove_effect_map(fn,ef[fn].m[ef[fn].field_cnt]);
	}

        dx=(ef[fn].tox-ef[fn].frx);
	dy=(ef[fn].toy-ef[fn].fry);

	if (dx==0 && dy==0) {
		remove_effect(fn);
		free_effect(fn);
                return;
	}

        // line algorithm with a step of 0.5 tiles
        // this is needed to avoid jumping over obstacles
	if (abs(dx)>abs(dy)) { dy=dy*512/abs(dx); dx=dx*512/abs(dx); }
	else { dx=dx*512/abs(dy); dy=dy*512/abs(dy); }
	

	// two steps per tick, making the speed one tile per tick
	for (n=0; n<2; n++) {	
		ef[fn].lx=ef[fn].x/1024;
		ef[fn].ly=ef[fn].y/1024;

		ef[fn].x+=dx;
		ef[fn].y+=dy;

                m=(ef[fn].x/1024)+(ef[fn].y/1024)*MAXMAP;

                if ((fire_map_block(m)) && (ef[fn].cn==0 || !fire_char(m,ef[fn].cn))) {
			if ((cn=ef[fn].cn)) {
				int d1,d2;
				d1=abs(ch[cn].x-ef[fn].x/1024);
				d2=abs(ch[cn].y-ef[fn].y/1024);
				if (d1<2 && d2<2) {
					remove_effect(fn);
					free_effect(fn);
					return;
				}
			}
			ef_fireball_explode(fn,ef[fn].x/1024,ef[fn].y/1024);			
			return;
		}		
	}	

	x=ef[fn].x/1024;
	y=ef[fn].y/1024;

        set_effect_map(fn,x,y);
}

// helper for NCPs: would we hit using fireball?
int ishit_fireball(int cn,int frx,int fry,int tox,int toy,int (*isenemy)(int,int))
{
	int dx,dy,x,y,co,n,cx,cy,m,enemy=0;

	x=frx*1024+512;
	y=fry*1024+512;

	dx=(tox-frx);
	dy=(toy-fry);

	if (abs(dx)<2 && abs(dy)<2) return 0;

	// line algorithm with a step of 0.5 tiles
	if (abs(dx)>abs(dy)) { dy=dy*512/abs(dx); dx=dx*512/abs(dx); }
	else { dx=dx*512/abs(dy); dy=dy*512/abs(dy); }
	
	// 48 steps is whole range
	for (n=0; n<48; n++) {	
		
		cx=x/1024;
		cy=y/1024;

		m=cx+cy*MAXMAP;

		if ((fire_map_block(m)) && map[m].ch!=cn) {
			if ((co=checkhit_fireball(cx,cy))) {
				if (isenemy(cn,co)) enemy++;
			}
                        if ((co=checkhit_fireball(cx+1,cy))) {
				if (isenemy(cn,co)) enemy++;
			}
			if ((co=checkhit_fireball(cx,cy+1))) {
				if (isenemy(cn,co)) enemy++;
			}
			if ((co=checkhit_fireball(cx-1,cy))) {
				if (isenemy(cn,co)) enemy++;
			}
			if ((co=checkhit_fireball(cx,cy-1))) {
				if (isenemy(cn,co)) enemy++;
			}
                        if ((co=checkhit_fireball(cx+1,cy+1))) {
				if (isenemy(cn,co)) enemy++;
			}
			if ((co=checkhit_fireball(cx-1,cy+1))) {
				if (isenemy(cn,co)) enemy++;
			}
			if ((co=checkhit_fireball(cx+1,cy-1))) {
				if (isenemy(cn,co)) enemy++;
			}
			if ((co=checkhit_fireball(cx-1,cy-1))) {
				if (isenemy(cn,co)) enemy++;
			}
			
                        return enemy;
		}

                x+=dx;
		y+=dy;					
	}

	return 0;
}

int create_fireball(int cn,int frx,int fry,int tox,int toy,int str)
{
	int n,dx,dy;

	n=alloc_effect(EF_FIREBALL);
	if (!n) return 0;

        ef[n].strength=str;
	ef[n].light=200;
	ef[n].frx=frx;
	ef[n].fry=fry;
	ef[n].tox=tox;
	ef[n].toy=toy;
	ef[n].cn=cn;
	if (cn) ef[n].sercn=ch[cn].serial;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+TICKS;
	ef[n].number_of_enemies=0;
	
	dx=(ef[n].tox-ef[n].frx);
	dy=(ef[n].toy-ef[n].fry);

        ef[n].x=ef[n].frx*1024+512;
	ef[n].y=ef[n].fry*1024+512;

	return n;
}

// --------------- lightning ball and helpers ----------------------

static void ef_strike(int fn)
{
	if (ticker>=ef[fn].stop) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}
}

static void add_strike(int xc,int yc,int x,int y,int str,int cn,int cc,int cnt)
{
	int n,dam,cnt_multi,fn;

	for (n=0; n<4; n++) {
		if ((fn=ch[cn].ef[n]) &&
		    ef[fn].type==EF_STRIKE &&
		    ef[fn].x==xc &&
		    ef[fn].y==yc &&
		    ef[fn].strength==str) break;
	}
	if (n==4) {
                fn=alloc_effect(EF_STRIKE);		
		if (!fn) return;

		ef[fn].strength=str;
		ef[fn].light=50;
	
		ef[fn].field_cnt=0;

		ef[fn].x=xc;
		ef[fn].y=yc;

		add_effect_char(fn,cn);
	}
	
	ef[fn].stop=ticker+2;

        if (cnt<1) cnt=1;
	if (cnt>280) cnt=280;
	
	switch(cnt) {
		case 1:		cnt_multi=100; break;
		case 2:		cnt_multi= 90; break;
		case 3:		cnt_multi= 80; break;
		case 4:		cnt_multi= 70; break;
		default:	cnt_multi= 280/cnt; break;
	}

	if (cc && ch[cc].action==AC_WALK) cnt_multi=(cnt_multi*75)/100;

	if ((ticker&3)==0) {
                dam=strike_damage(cc,cn,str)*cnt_multi/(25*TICKS);
		if (ch[cn].flags&CF_EDEMON) {	// earth demons dont suffer as much damage
                        dam-=min(dam/4,(dam*max(0,ch[cn].value[1][V_DEMON]-ch[cc].value[1][V_DEMON]))/10);
		}
		hurt(cn,dam,cc,8,60,85);
                //say(cn,"hit by strike, str=%d (%d), cnt=%d, damage=%d (%.2f), tick=%d",str,immunity_reduction(cc,cn,str),cnt,dam,(double)dam/1000*TICKS*2,ticker);
	}	
}

static void check_strike_near(int fn,int xc,int yc)
{
	int x,y,xs,ys,xe,ye,co,cn,tmp,cnt=0;

        xs=max(1,xc-5);
	ys=max(1,yc-5);
	xe=min(MAXMAP-2,xc+5);
	ye=min(MAXMAP-2,yc+5);

	for (y=ys; y<ye; y++) {
		for (x=xs; x<xe; x++) {
			if ((co=map[x+y*MAXMAP].ch)) {
				cn=ef[fn].cn;

				if (!can_attack(cn,co)) continue;
				if (!is_visible(cn,co)) continue;

				// not through walls
				if (!(tmp=los_can_see(0,xc,yc,x,y,5))) continue;

                                add_strike(xc,yc,x,y,ef[fn].strength,co,ef[fn].cn,ef[fn].number_of_enemies);
				if (cnt==0 && (ticker&7)==0) {
					sound_area(ch[cn].x,ch[cn].y,30);
				}
				cnt++;
			}
		}
        }
	ef[fn].number_of_enemies=cnt;
}

static void ef_ball(int fn)
{
	int m,dx,dy,x,y,cn,remo=0,oldx,oldy;

	cn=ef[fn].cn;
	if (cn && (!ch[cn].flags || ch[cn].serial!=ef[fn].sercn)) remo=1;	// different char

	// maximum age for a lightning ball is TICKS*5
	if (remo || ticker>=ef[fn].stop) {	
		remove_effect(fn);
		free_effect(fn);
		return;
	}

        oldx=ef[fn].x/1024;
	oldy=ef[fn].y/1024;

        dx=(ef[fn].tox-ef[fn].frx);
	dy=(ef[fn].toy-ef[fn].fry);

	if (dx==0 && dy==0) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}

        // line algorithm with a step of 0.125 tiles
	if (abs(dx)>abs(dy)) { dy=dy*128/abs(dx); dx=dx*128/abs(dx); }
	else { dx=dx*128/abs(dy); dy=dy*128/abs(dy); }
	
        ef[fn].x+=dx;
	ef[fn].y+=dy;

	m=(ef[fn].x/1024)+(ef[fn].y/1024)*MAXMAP;

	//(map[m].flags&(MF_TMOVEBLOCK|MF_MOVEBLOCK))
	if (ball_map_block(m) && (ef[fn].cn==0 || map[m].ch!=ef[fn].cn)) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}

	x=ef[fn].x/1024;
	y=ef[fn].y/1024;

	if (oldx!=x || oldy!=y) {
		remove_effect(fn);
                set_effect_map(fn,x,y);
	} else set_sector(x,y);

	check_strike_near(fn,x,y);
}

int calc_steps_ball(int cn,int frx,int fry,int tox,int toy)
{
	int m,dx,dy,x,y,n;

	dx=(tox-frx);
	dy=(toy-fry);

	x=frx*1024+512;
	y=fry*1024+512;

	if (dx==0 && dy==0) return 0;

        // line algorithm with a step of 0.5 tiles
	if (abs(dx)>abs(dy)) { dy=dy*512/abs(dx); dx=dx*512/abs(dx); }
	else { dx=dx*512/abs(dy); dy=dy*512/abs(dy); }
	
	for (n=0; n<TICKS*5/4; n++) {
		x+=dx;
		y+=dy;
	
		m=(x/1024)+(y/1024)*MAXMAP;
	
		//(map[m].flags&(MF_TMOVEBLOCK|MF_MOVEBLOCK))
		if (ball_map_block(m) && map[m].ch!=cn) {
			return n;
		}
	}

	return n;
}

int create_ball(int cn,int frx,int fry,int tox,int toy,int str)
{
	int n;

	n=alloc_effect(EF_BALL);
	if (!n) return 0;

        ef[n].strength=str;
	ef[n].light=80;
	ef[n].frx=frx;
	ef[n].fry=fry;
	ef[n].tox=tox;
	ef[n].toy=toy;
	ef[n].cn=cn;
	if (cn) ef[n].sercn=ch[cn].serial;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+TICKS*5;
	ef[n].number_of_enemies=0;
	
        ef[n].x=ef[n].frx*1024+512;
	ef[n].y=ef[n].fry*1024+512;

	return n;
}

//---------------- lightning flash ---------------------------------

#if 0
static void check_strike_near_flash(int fn,int xc,int yc)
{
	int x,y,xs,ys,xe,ye,co,cn,cnt=0;

        xs=max(1,xc-1);
	ys=max(1,yc-1);
	xe=min(MAXMAP-2,xc+2);
	ye=min(MAXMAP-2,yc+2);

	for (y=ys; y<ye; y++) {
		for (x=xs; x<xe; x++) {
			if (x!=xc && y!=yc) continue;
			
			if ((co=map[x+y*MAXMAP].ch)) {
				cn=ef[fn].cn;

				if (!can_attack(cn,co)) continue;
				if (!is_visible(cn,co)) continue;

				// not through walls
				if (!(los_can_see(0,xc,yc,x,y,3))) continue;

                                add_strike(xc,yc,x,y,ef[fn].strength,co,ef[fn].cn,ef[fn].number_of_enemies);
				if (cnt==0 && (ticker&7)==0) {
					sound_area(ch[cn].x,ch[cn].y,30);
				}
				cnt++;
			}
		}
        }
	ef[fn].number_of_enemies=cnt;
}
#endif

static void ef_flash(int fn)
{
	int cn;

        cn=ef[fn].cn;
	if (ch[cn].serial!=ef[fn].sercn) cn=0;			// different char
	else if (ch[cn].flags&(CF_DEAD)) cn=0;			// dead or unconscious char

	// maximum age for a flash is TICKS*2
	if (!cn || ticker>=ef[fn].stop) {	
		remove_effect(fn);
		free_effect(fn);
		return;
	}

        /*while (ef[fn].field_cnt) {
		ef[fn].field_cnt--;
		remove_effect_map(fn,ef[fn].m[ef[fn].field_cnt]);
	}

	set_effect_map(fn,ch[cn].x,ch[cn].y);*/
	check_strike_near(fn,ch[cn].x,ch[cn].y);
	//set_sector(ch[cn].x,ch[cn].y);
}

int create_flash(int cn,int str,int duration)
{
	int fn;

	fn=alloc_effect(EF_FLASH);
	if (!fn) return 0;

        ef[fn].strength=str;
	ef[fn].light=50;
        ef[fn].cn=cn;
	ef[fn].sercn=ch[cn].serial;
	ef[fn].field_cnt=0;
	ef[fn].start=ticker;
	ef[fn].stop=ticker+duration;
	ef[fn].number_of_enemies=0;

	add_effect_char(fn,cn);

	return fn;
}

//---------------- explosion ---------------------------------

static void ef_explode(int fn)
{
        if (ticker>=ef[fn].stop) {	
		remove_effect(fn);
		free_effect(fn);
		return;
	}
}

int add_explosion(int fn,int x,int y)
{
	return set_effect_map(fn,x,y);
}

int create_explosion(int maxage,int base)
{
	int n;

	n=alloc_effect(EF_EXPLODE);
	if (!n) return 0;

        ef[n].strength=maxage;
	ef[n].light=200;
        ef[n].cn=0;
	ef[n].sercn=0;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+maxage;
	ef[n].number_of_enemies=0;
	ef[n].base_sprite=base;

	return n;
}

int create_mist(int x,int y)
{
	int n;

	n=alloc_effect(EF_MIST);
	if (!n) return 0;

        ef[n].light=0;
        ef[n].cn=0;
	ef[n].sercn=0;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+24;
	ef[n].number_of_enemies=0;

	return set_effect_map(n,x,y);
}

static void ef_show(int fn)
{
	if (ticker>=ef[fn].stop) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}	
}

static void ef_earthrain(int fn)
{
	int n,cn,m,dam,per;

	for (n=0; n<ef[fn].field_cnt; n++) {
		m=ef[fn].m[n];
                if (!(cn=map[m].ch)) continue;
		//if (!can_attack(ef[fn].cn,cn)) continue;
		if (!(ch[cn].flags&CF_PLAYER)) continue;

		dam=edemon_reduction(cn,ef[fn].strength)*150;
		if (!dam) continue;

		per=50-min(50,edemon_reduction(cn,ef[fn].strength));
		
		if (!RANDOM(10)) hurt(cn,dam,0,8,per,per+25); //hurt(cn,dam,ef[fn].cn,8,per,per+25);
	}
        if (ticker>=ef[fn].stop) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}	
}

static int add_earthrain_map(int fn,int x,int y)
{
	int n,fn2,m;

	m=x+y*MAXMAP;

	for (n=0; n<4; n++) {
		if ((fn2=map[m].ef[n]) && ef[fn2].type==EF_EARTHRAIN) return 0;
	}
	return set_effect_map(fn,x,y);
}

int create_earthrain(int cn,int x,int y,int strength)
{
	int n;

	n=alloc_effect(EF_EARTHRAIN);
	if (!n) return 0;

        ef[n].light=10;
        ef[n].cn=0;	//ef[n].cn=cn;
	ef[n].sercn=0;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+TICKS*60;
	ef[n].number_of_enemies=0;
	ef[n].strength=strength;

	set_effect_map(n,x,y);
	
	if (!(map[x+y*MAXMAP+1].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x+1,y);
	if (!(map[x+y*MAXMAP-1].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x-1,y);
	if (!(map[x+y*MAXMAP+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x,y+1);
	if (!(map[x+y*MAXMAP-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x,y-1);

	if (!(map[x+y*MAXMAP+1+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x+1,y+1);
	if (!(map[x+y*MAXMAP-1+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x-1,y+1);
	if (!(map[x+y*MAXMAP+1-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x+1,y-1);
	if (!(map[x+y*MAXMAP-1-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthrain_map(n,x-1,y-1);


	return 1;
}

static void ef_earthmud(int fn)
{
        if (ticker>=ef[fn].stop) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}	
}

static int add_earthmud_map(int fn,int x,int y)
{
	int n,fn2,m;

	m=x+y*MAXMAP;

	for (n=0; n<4; n++) {
		if ((fn2=map[m].ef[n]) && ef[fn2].type==EF_EARTHMUD) return 0;
	}
	return set_effect_map(fn,x,y);
}

int create_earthmud(int cn,int x,int y,int strength)
{
	int n;

	n=alloc_effect(EF_EARTHMUD);
	if (!n) return 0;

        ef[n].light=0;
        ef[n].cn=cn;
	ef[n].sercn=0;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+TICKS*60;
	ef[n].number_of_enemies=0;
	ef[n].strength=strength;

	set_effect_map(n,x,y);
	
	if (!(map[x+y*MAXMAP+1].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x+1,y);
	if (!(map[x+y*MAXMAP-1].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x-1,y);
	if (!(map[x+y*MAXMAP+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x,y+1);
	if (!(map[x+y*MAXMAP-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x,y-1);

	if (!(map[x+y*MAXMAP+1+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x+1,y+1);
	if (!(map[x+y*MAXMAP-1+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x-1,y+1);
	if (!(map[x+y*MAXMAP+1-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x+1,y-1);
	if (!(map[x+y*MAXMAP-1-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) add_earthmud_map(n,x-1,y-1);


	return 1;
}


static void ef_burn(int fn)
{
	int cn;

	cn=ef[fn].cn;
	if (!cn || !(ch[cn].flags) || ch[cn].serial!=ef[fn].sercn || ticker>=ef[fn].stop) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}

	if (ef[fn].strength) hurt(cn,POWERSCALE/6+ef[fn].strength,0,30,50,75);
}

int create_show_effect(int type,int cn,int start,int stop,int light,int strength)
{
	int fn;

	fn=alloc_effect(type);
        if (!fn) return 0;

	ef[fn].light=light;
	ef[fn].cn=cn;
	ef[fn].efcn=cn;
	ef[fn].sercn=ch[cn].serial;
	ef[fn].field_cnt=0;
	ef[fn].start=start;
	ef[fn].stop=stop;
	ef[fn].strength=strength;

	add_effect_char(fn,cn);

	return fn;
}

int create_map_effect(int type,int x,int y,int start,int stop,int light,int strength)
{
	int fn;

	fn=alloc_effect(type);
        if (!fn) return 0;

	ef[fn].light=light;
	ef[fn].field_cnt=0;
	ef[fn].start=start;
	ef[fn].stop=stop;
	ef[fn].strength=strength;

	set_effect_map(fn,x,y);

	return fn;
}

// ------------- end of magic shield -----------------

void destroy_chareffects(int cn)
{
	int n,fn;
	
	for (n=0; n<4; n++) {
		if ((fn=ch[cn].ef[n])) {
			remove_effect(fn);
			free_effect(fn);
		}
	}
}

// ------ edemon ball --------------

int edemonball_map_block(int m)
{
	if (map[m].ch || (map[m].it && (map[m].flags&MF_TMOVEBLOCK))) return 1;
	if (map[m].flags&MF_FIRETHRU) return 0;
	if (map[m].flags&MF_MOVEBLOCK) return 1;

	return 0;
}

static void ef_edemonball_explode(int fn,int x,int y)
{
	int m,co,cn,dam,n,in,sprite,base,cnr;

	if (fn<1 || fn>=MAXEFFECT) return;	
	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return;

	// would we hit someone we're not allowed to hit?
	cn=ef[fn].cn;
	base=ef[fn].base_sprite;
	
	if (areaID==13) cnr=ef[fn].strength; else cnr=0;

	remove_effect(fn);
        free_effect(fn);

	m=x+y*MAXMAP;
	
	// is used for all areas but the clan halls
	if (areaID!=13 && (co=map[m].ch) && can_attack(cn,co) && !(base==2 && (ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE)))) {
		
		cn=ef[fn].cn;
                dam=ef[fn].strength;

		for (n=29; n<INVENTORYSIZE; n++) {
			
			if (n>29) in=ch[co].item[n];
			else in=ch[co].citem;
			
			if (in && base==0 && it[in].ID==IID_AREA6_GREENCRYSTAL) {
				
				if (dam>it[in].drdata[0]) {
					
					dam-=it[in].drdata[0];
					
					if (n>29) ch[co].item[n]=0;
					else ch[co].citem=0;
					
					ch[co].flags|=CF_ITEMS;

					if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped as shield against edemonball");
					log_char(co,LOG_SYSTEM,0,"Your %s was destroyed.",it[in].name);
					
					destroy_item(in);
					continue;
				}
				
				it[in].drdata[0]-=dam;
				dam=0;
				
				sprite=50318+5-(it[in].drdata[0]/42);
				
				if (sprite!=it[in].sprite) {
					it[in].sprite=sprite;
					ch[co].flags|=CF_ITEMS;
				}
				break;
			}
		}

                hurt(co,dam*POWERSCALE,0,6,75,50);
		
	}

	// is used for clan halls only
	if (cnr) {
		dam=CLAN_CANNON_STRENGTH;
		if ((co=map[m].ch)        && (map[m].flags&MF_ARENA)        && !clan_alliance_self(cnr,get_char_clan(co))) hurt(co,dam*POWERSCALE,0,6,75,50);
		if ((co=map[m+1].ch)      && (map[m+1].flags&MF_ARENA)      && !clan_alliance_self(cnr,get_char_clan(co))) hurt(co,dam*POWERSCALE,0,6,75,50);
		if ((co=map[m-1].ch)      && (map[m-1].flags&MF_ARENA)      && !clan_alliance_self(cnr,get_char_clan(co))) hurt(co,dam*POWERSCALE,0,6,75,50);
		if ((co=map[m+MAXMAP].ch) && (map[m+MAXMAP].flags&MF_ARENA) && !clan_alliance_self(cnr,get_char_clan(co))) hurt(co,dam*POWERSCALE,0,6,75,50);
		if ((co=map[m-MAXMAP].ch) && (map[m-MAXMAP].flags&MF_ARENA) && !clan_alliance_self(cnr,get_char_clan(co))) hurt(co,dam*POWERSCALE,0,6,75,50);
	}

	fn=create_explosion(8,50450+base);
	add_explosion(fn,x,y);
	sound_area(x,y,6);
}

static void ef_edemonball(int fn)
{
	int m,dx,dy,x,y,cn,remo=0;

	cn=ef[fn].cn;
	if (cn && ch[cn].serial!=ef[fn].sercn) remo=1;			// different char

	// maximum age for a fireball is TICKS, which makes its range 24 tiles
	if (remo || ticker>=ef[fn].stop) {	
		remove_effect(fn);
		free_effect(fn);
		return;
	}

        while (ef[fn].field_cnt) {
		ef[fn].field_cnt--;
		remove_effect_map(fn,ef[fn].m[ef[fn].field_cnt]);
	}

        dx=(ef[fn].tox-ef[fn].frx);
	dy=(ef[fn].toy-ef[fn].fry);

	if (dx==0 && dy==0) {
		ef_edemonball_explode(fn,ef[fn].x/1024,ef[fn].y/1024);		
		return;
	}

        // line algorithm with a step of 0.5 tiles
        // this is needed to avoid jumping over obstacles
	if (abs(dx)>abs(dy)) { dy=dy*256/abs(dx); dx=dx*256/abs(dx); }
	else { dx=dx*256/abs(dy); dy=dy*256/abs(dy); }
	
        ef[fn].lx=ef[fn].x/1024;
	ef[fn].ly=ef[fn].y/1024;

	x=ef[fn].x+dx;
	y=ef[fn].y+dy;

	m=(x/1024)+(y/1024)*MAXMAP;

	if ((edemonball_map_block(m)) && (ef[fn].cn==0 || map[m].ch!=ef[fn].cn)) {
		if (map[m].ch) ef_edemonball_explode(fn,x/1024,y/1024);
		else ef_edemonball_explode(fn,ef[fn].x/1024,ef[fn].y/1024);
		return;
	}
	if (areaID==13 && (map[m].flags&(MF_ARENA|MF_CLAN))!=(MF_ARENA|MF_CLAN)) {
		remove_effect(fn);
		free_effect(fn);
		return;
	}
	
	ef[fn].x=x;
	ef[fn].y=y;

	x=ef[fn].x/1024;
	y=ef[fn].y/1024;

        set_effect_map(fn,x,y);
}

int create_edemonball(int cn,int frx,int fry,int tox,int toy,int str,int base)
{
	int n,dx,dy;

	n=alloc_effect(EF_EDEMONBALL);
	if (!n) return 0;

        ef[n].strength=str;
	ef[n].light=0;
	ef[n].frx=frx;
	ef[n].fry=fry;
	ef[n].tox=tox;
	ef[n].toy=toy;
	ef[n].cn=cn;
	if (cn) ef[n].sercn=ch[cn].serial;
	ef[n].field_cnt=0;
	ef[n].stop=ticker+TICKS*4;
	ef[n].number_of_enemies=0;
	ef[n].base_sprite=base;
	
	dx=(ef[n].tox-ef[n].frx);
	dy=(ef[n].toy-ef[n].fry);
	
	ef[n].x=ef[n].frx*1024+512;
	ef[n].y=ef[n].fry*1024+512;

	return n;
}

void create_pulse(int x,int y,int str)
{
	int n;

	n=alloc_effect(EF_PULSE);
	if (!n) return;

        ef[n].strength=str;
        ef[n].field_cnt=0;
	ef[n].stop=ticker+6;

	set_effect_map(n,x,y);
	
        return;

}

void tick_effect(void)
{
	int n,next;
	/*static int cnt=0;

	if (++cnt<TICKS) return;
	cnt=0;*/

	for (n=getfirst_effect(); n; n=next) {

		next=getnext_effect(n);

		switch(ef[n].type) {
			case EF_NONE:		break;
			case EF_FIREBALL:	ef_fireball(n); break;
			case EF_MAGICSHIELD:	ef_show(n); break;
			case EF_BALL:		ef_ball(n); break;
			case EF_STRIKE:		ef_strike(n); break;
			case EF_FLASH:		ef_flash(n); break;
			case EF_EXPLODE:	ef_explode(n); break;
                        case EF_WARCRY:		ef_show(n); break;
			case EF_BLESS:		ef_show(n); break;
			case EF_FREEZE:		ef_show(n); break;
			case EF_HEAL:		ef_show(n); break;
			case EF_BURN:		ef_burn(n); break;
			case EF_MIST:		ef_show(n); break;
			case EF_POTION:		ef_show(n); break;
			case EF_EARTHRAIN:	ef_earthrain(n); break;
			case EF_EARTHMUD:	ef_earthmud(n); break;
			case EF_EDEMONBALL:	ef_edemonball(n); break;
			case EF_CURSE:		ef_show(n); break;
			case EF_CAP:		ef_show(n); break;
			case EF_LAG:		ef_show(n); break;
			case EF_PULSE:		ef_show(n); break;
			case EF_PULSEBACK:	ef_show(n); break;
			case EF_FIRERING:	ef_show(n); break;
			case EF_BUBBLE:		ef_show(n); break;

			default:	elog("unknown effect type %d for ef %d",ef[n].type,n);
		}
	}
}

int init_effect(void)
{
	int n;

	ef=xcalloc(sizeof(struct effect)*MAXEFFECT,IM_BASE);
	if (!ef) return 0;
	xlog("Allocated effects: %.2fM (%d*%d)",sizeof(struct effect)*MAXEFFECT/1024.0/1024.0,sizeof(struct effect),MAXEFFECT);
	mem_usage+=sizeof(struct effect)*MAXEFFECT;

	for (n=1; n<MAXEFFECT; n++) { ef[n].type=42; free_effect(n); }

	used_effects=0;

	return 1;
	
}












