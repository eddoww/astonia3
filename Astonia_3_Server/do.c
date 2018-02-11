/*
 * $Id: do.c,v 1.3 2006/06/20 13:38:32 ssim Exp $
 *
 * $Log: do.c,v $
 * Revision 1.3  2006/06/20 13:38:32  ssim
 * added hack for teufel pk arena: no switching gear
 *
 * Revision 1.2  2006/04/26 16:05:44  ssim
 * added hack for teufelheim arena: no bless other allowed
 *
 */

#include <stdlib.h>

#include "server.h"
#include "tool.h"
#include "direction.h"
#include "act.h"
#include "error.h"
#include "create.h"
#include "drvlib.h"
#include "talk.h"
#include "see.h"
#include "container.h"
#include "log.h"
#include "notify.h"
#include "libload.h"
#include "database.h"
#include "spell.h"
#include "effect.h"
#include "map.h"
#include "statistics.h"
#include "area.h"
#include "los.h"
#include "date.h"
#include "balance.h"
#include "sector.h"
#include "do.h"
#include "drdata.h"
#include "misc_ppd.h"

#define DUR_COMBAT_ACTION	12	// ex 8
#define DUR_MISC_ACTION		12	// ex 8
#define DUR_USE_ACTION		8	// ex 8
#define DUR_MAGIC_ACTION	12	// ex 8

// ******** Part I: Actions which take at least one full tick ********

int do_idle(int cn,int dur)
{
	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (dur>TICKS*2) dur=TICKS*2;
	if (dur<2) dur=2;
	
	ch[cn].action=AC_IDLE;
	ch[cn].duration=dur;
	ch[cn].act1=dur;

	return 1;
}

int do_walk(int cn,int dir)
{
	int m,x,y,diag,n,cost,fn,dx,dy;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!dx2offset(dir,&dx,&dy,&diag)) { error=ERR_ILLEGAL_DIR; return 0; }

	m=ch[cn].x+ch[cn].y*MAXMAP;

	cost=8;
	if (!(ch[cn].flags&CF_EDEMON)) {	// slowdown for earth demon mud spell
		for (n=0; n<4; n++) {
			if ((fn=map[m].ef[n]) && ef[fn].type==EF_EARTHMUD) cost+=edemon_reduction(cn,ef[fn].strength)*2;
		}
	}

	// swamp sprites
	if (ch[cn].flags&CF_PLAYER) {
		int sprite;

		sprite=map[m].gsprite&0xffff;
		if (sprite>=59405 && sprite<=59413) cost=12;
		if (sprite>=59414 && sprite<=59422) cost=16;
		if (sprite>=59423 && sprite<=59431) cost=24;
		if (sprite>=20815 && sprite<=20823) cost=36;
		if (sprite>=59706 && sprite<=59709 && areaID==29) cost=48;

                if (map[m].flags&MF_UNDERWATER) cost=10;
        }

	x=ch[cn].x+dx;
	y=ch[cn].y+dy;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

	m=x+y*MAXMAP;

	if (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) { error=ERR_BLOCKED; return 0; }
	if (diag) {
		if (map[ch[cn].x+dx+(ch[cn].y)*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) { error=ERR_BLOCKED; return 0; }
		if (map[ch[cn].x+(ch[cn].y+dy)*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) { error=ERR_BLOCKED; return 0; }
	}


	if (map[m].ch) {
		elog("do_walk(): map.ch at position %d,%d is set, but MF_TMOVEBLOCK is not.",x,y);
		error=ERR_CONFUSED;
		return 0;
	}

	map[m].flags|=MF_TMOVEBLOCK;

        ch[cn].action=AC_WALK;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,cost+diag*cost/2);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].tox=x;
	ch[cn].toy=y;
	ch[cn].dir=dir;

	return 1;
}

int do_take(int cn,int dir)
{
	int m,x,y,in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!dx2offset(dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

	if (ch[cn].citem) { error=ERR_HAVE_CITEM; return 0; }

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

	m=x+y*MAXMAP;

	if (!(in=map[m].it)) { error=ERR_NO_ITEM; return 0; }

	if (in<1 || in>=MAXITEM) {
		elog("do_take(): item number on map position %d,%d is out of bounds.",x,y);
		map[m].it=0;
		error=ERR_CONFUSED;
		return 0;
	}

	if (!(it[in].flags&IF_TAKE)) { error=ERR_NOT_TAKEABLE; return 0; }

	if (!can_carry(cn,in,0)) { error=ERR_NOT_TAKEABLE; return 0; }

	ch[cn].action=AC_TAKE;
        ch[cn].act1=in;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MISC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].dir=dir;
	
	return 1;
}

int do_use(int cn,int dir,int spec)
{
	int m,x,y,in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!dx2offset(dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

        x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

	m=x+y*MAXMAP;

	if (!(in=map[m].it)) { error=ERR_NO_ITEM; return 0; }

	if (in<1 || in>=MAXITEM) {
		elog("do_use(): item number on map position %d,%d is out of bounds.",x,y);
		map[m].it=0;
		error=ERR_CONFUSED;
		return 0;
	}

	if (!(it[in].flags&IF_USE)) { error=ERR_NOT_USEABLE; return 0; }

        ch[cn].action=AC_USE;
	ch[cn].act1=in;
	ch[cn].act2=spec;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_USE_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

        ch[cn].dir=dir;
	
	return 1;
}

int do_drop(int cn,int dir)
{
	int m,x,y,in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!dx2offset(dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

	if (!(in=ch[cn].citem)) { error=ERR_NO_CITEM; return 0; }

	if (in<1 || in>=MAXITEM) {
		elog("do_drop(): item number %d in citem of %s (%d) is out of bounds.",in,ch[cn].name,cn);
		ch[cn].citem=0;
		error=ERR_CONFUSED;
		return 0;
	}

	if (!(it[in].flags&IF_TAKE)) {
		elog("do_drop(): character %s (%d) is dropping item %s (%d) which doesn't have IF_TAKE set",ch[cn].name,cn,it[in].name,in);
	}

	if (it[in].flags&IF_QUEST) { error=ERR_QUESTITEM; return 0; }

	if (it[in].flags&IF_NODROP) { error=ERR_QUESTITEM; return 0; }

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

	m=x+y*MAXMAP;

	if (map[m].it) { error=ERR_HAVE_ITEM; return 0; }

	if (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) { error=ERR_BLOCKED; return 0; }

        ch[cn].action=AC_DROP;
        ch[cn].act1=in;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MISC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].dir=dir;
	
	return 1;
}

int do_attack(int cn,int dir,int co)
{
	int m,x,y;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!dx2offset(dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

	m=x+y*MAXMAP;

	if (co!=map[m].ch &&
	    co!=map[m+1].ch &&
	    co!=map[m-1].ch &&
	    co!=map[m+MAXMAP].ch &&
	    co!=map[m-MAXMAP].ch &&
	    co!=map[m+MAXMAP+1].ch &&
	    co!=map[m+MAXMAP-1].ch &&
	    co!=map[m-MAXMAP+1].ch &&
	    co!=map[m-MAXMAP-1].ch) { error=ERR_NO_CHAR; return 0; }	// the intended victim isnt reachable

        if (ch[co].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!can_attack(cn,co)) { error=ERR_ILLEGAL_ATTACK; return 0; }

	ch[cn].action=AC_ATTACK1+RANDOM(3);
        ch[cn].act1=co;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_COMBAT_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn)*2;

	ch[cn].dir=dir;	

	return 1;
}

int do_give(int cn,int dir)
{
	int m,x,y,co,in;
	struct misc_ppd *ppd;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!(in=ch[cn].citem)) { error=ERR_NO_CITEM; return 0; }

	if (in<1 || in>=MAXITEM) {
		elog("do_give(): item number %d in citem of %s (%d) is out of bounds.",in,ch[cn].name,cn);
		ch[cn].citem=0;
		error=ERR_CONFUSED;
		return 0;
	}

        if (!dx2offset(dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) { error=ERR_ILLEGAL_COORDS; return 0; }

	m=x+y*MAXMAP;

        if (!(co=map[m].ch)) { error=ERR_NO_CHAR; return 0; }

	if (ch[co].flags&CF_DEAD) { error=ERR_DEAD; return 0; }

	if (ch[co].flags&CF_NOGIVE) { error=ERR_QUESTITEM; return 0; }	

        if ((it[in].flags&IF_QUEST) && !(ch[co].flags&(CF_QUESTITEM|CF_GOD)) && !(ch[cn].flags&(CF_QUESTITEM|CF_GOD))) { error=ERR_QUESTITEM; return 0; }

	if ((it[in].flags&IF_BONDTAKE) && ((ch[cn].flags&CF_GOD) || (ch[co].flags&CF_GOD))) {
		it[in].ownerID=ch[co].ID;		
	}

	if (!can_carry(co,in,1)) return 0;

	if ((ch[co].flags&CF_PLAYER) && ch[co].citem && cnt_free_inv(co)<1) { error=ERR_BLOCKED; return 0; }
	if (!(ch[co].flags&CF_PLAYER) && ch[co].citem) { error=ERR_BLOCKED; return 0; }

	if ((ch[cn].flags&CF_PLAYER) && (ch[co].flags&CF_PLAYER) && (ppd=set_data(co,DRD_MISC_PPD,sizeof(struct misc_ppd))) && realtime-ppd->swapped<20) {
		log_char(cn,LOG_SYSTEM,0,"°c3Give canceled: Your target has swapped recently.");
		error=ERR_ACCESS_DENIED;
		return 0;
	}

        ch[cn].action=AC_GIVE;
        ch[cn].act1=co;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MISC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].dir=dir;	

	return 1;
}

int warcried(int cn)
{
	int n,in;

	for (n=12; n<30; n++) {	
		if ((in=ch[cn].item[n]) && it[in].driver==IDR_WARCRY) {
			if (it[in].mod_value[0]<-100) return 1;
                        else return 0;
		}
	}
	
	return 0;
}

int do_bless(int cn,int co)
{
	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[co].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (!char_see_char(cn,co)) { error=ERR_NOT_VISIBLE; return 0; }

	if (!ch[cn].value[0][V_BLESS]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (ch[cn].mana<BLESSCOST) { error=ERR_MANA_LOW; return 0; }

	if ((ch[cn].flags&CF_PLAYER) && !(ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE))) { error=ERR_NOT_PLAYER; return 0; }

	if (!may_add_spell(co,IDR_BLESS)) { error=ERR_ALREADY_WORKING; return 0; }

	if (!can_help(cn,co)) { error=ERR_ILLEGAL_ATTACK; return 0; }

	if (cn!=co && (ch[cn].flags&CF_PLAYER) && (ch[co].flags&CF_NOBLESS)) { error=ERR_ILLEGAL_ATTACK; return 0; }

	// dont allow bless other in teufelheim arena
	if (areaID==34 && cn!=co && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) { error=ERR_ILLEGAL_ATTACK; return 0; }

        ch[cn].mana-=BLESSCOST;
	ch[cn].dir=bigdir(ch[cn].dir);

	if (cn==co) {
		ch[cn].action=AC_BLESS_SELF;
		ch[cn].act1=co;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
		if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);		
	} else {
		ch[cn].action=AC_BLESS1;
		ch[cn].act1=co;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION/2);
		ch[cn].dir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
		if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);
	}

        return 1;
}

int do_freeze(int cn)
{
	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_FREEZE]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (ch[cn].mana<FREEZECOST) { error=ERR_MANA_LOW; return 0; }

	ch[cn].mana-=FREEZECOST;

        ch[cn].action=AC_FREEZE;
        ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);
	ch[cn].dir=bigdir(ch[cn].dir);

        return 1;
}

int do_pulse(int cn)
{
	int str;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_PULSE]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (ch[cn].mana<POWERSCALE) { error=ERR_MANA_LOW; return 0; }

	str=min(spellpower(cn,V_PULSE),ch[cn].mana*8/POWERSCALE);
	ch[cn].mana-=str*POWERSCALE/8;

        ch[cn].action=AC_PULSE;
	ch[cn].act1=str;
        ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);
	ch[cn].dir=bigdir(ch[cn].dir);

        return 1;
}

int do_heal(int cn,int co)
{
	int str;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[co].flags&CF_DEAD) { error=ERR_DEAD; return 0; }

	if (!char_see_char(cn,co)) { error=ERR_NOT_VISIBLE; return 0; }

	if ((ch[cn].flags&CF_PLAYER) && !(ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE))) { error=ERR_NOT_PLAYER; return 0; }

	if (!ch[cn].value[0][V_HEAL]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (ch[cn].mana<POWERSCALE) { error=ERR_MANA_LOW; return 0; }	// no heals for less than one point

	if (!can_help(cn,co)) { error=ERR_ILLEGAL_ATTACK; return 0; }

	if ((ch[cn].flags&CF_PLAYER) && areaID==33) { error=ERR_UNKNOWN_SPELL; return 0; }

	str=ch[cn].value[0][V_HEAL]*POWERSCALE/2;			// strength is spell strength
	str=min(str,(ch[co].value[0][V_HP]*POWERSCALE)-ch[co].hp);	// but only up to max hp
	str=min(str,ch[cn].mana*2);					// but only with as much mana as we have left

	if (str<1) { error=ERR_NO_EFFECT; return 0; }

	ch[cn].mana-=str/2;
	ch[cn].dir=bigdir(ch[cn].dir);

	if (cn==co) {
		ch[cn].action=AC_HEAL_SELF;
		ch[cn].act1=co;
		ch[cn].act2=str;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
		if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);
	} else {
		ch[cn].action=AC_HEAL1;
		ch[cn].act1=co;
		ch[cn].act2=str;
		ch[cn].dir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION/2);
		if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);
	}

        return 1;
}

int do_fireball(int cn,int x,int y)
{
	int dir;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_FIREBALL]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) { error=ERR_ILLEGAL_COORDS; return 0; }

        if (ch[cn].mana<FIREBALLCOST) { error=ERR_MANA_LOW; return 0; }

        dir=offset2dx(ch[cn].x,ch[cn].y,x,y);

	if (!dir) {
		if (!may_add_spell(cn,IDR_FIRERING)) { error=ERR_ALREADY_WORKING; return 0; }
		ch[cn].action=AC_FIRERING;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
		ch[cn].dir=bigdir(ch[cn].dir);
	} else {
		ch[cn].action=AC_FIREBALL1;
		ch[cn].act1=x;
		ch[cn].act2=y;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION/2);
		ch[cn].dir=dir;
	}

	ch[cn].mana-=FIREBALLCOST;
	
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	return 1;
}

int do_earthrain(int cn,int x,int y,int strength)
{
	int dir;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

        if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) { error=ERR_ILLEGAL_COORDS; return 0; }

	dir=offset2dx(ch[cn].x,ch[cn].y,x,y);
	if (!dir) { error=ERR_SELF; return 0; }

	if (ch[cn].hp-POWERSCALE<strength*100) { error=ERR_MANA_LOW; return 0; }

	ch[cn].hp-=strength*100;

	ch[cn].action=AC_EARTHRAIN;
	ch[cn].act1=x+y*MAXMAP;
	ch[cn].act2=strength;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].dir=dir;

	return 1;
}

int do_earthmud(int cn,int x,int y,int strength)
{
	int dir;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

        if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) { error=ERR_ILLEGAL_COORDS; return 0; }

	dir=offset2dx(ch[cn].x,ch[cn].y,x,y);
	if (!dir) { error=ERR_SELF; return 0; }

	if (ch[cn].hp-POWERSCALE<strength*100) { error=ERR_MANA_LOW; return 0; }

	ch[cn].hp-=strength*100;

	ch[cn].action=AC_EARTHMUD;
	ch[cn].act1=x+y*MAXMAP;
	ch[cn].act2=strength;
	ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].dir=dir;

	return 1;
}

int do_ball(int cn,int x,int y)
{
	int dir;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_FLASH]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) { error=ERR_ILLEGAL_COORDS; return 0; }

	if (ch[cn].mana<FLASHCOST) { error=ERR_MANA_LOW; return 0; }

        dir=offset2dx(ch[cn].x,ch[cn].y,x,y);
	if (!dir) {
		if (!may_add_spell(cn,IDR_FLASH)) { error=ERR_ALREADY_WORKING; return 0; }
		ch[cn].action=AC_FLASH;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
		ch[cn].dir=bigdir(ch[cn].dir);
	} else {
		ch[cn].action=AC_BALL1;
		ch[cn].act1=x;
		ch[cn].act2=y;
		ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION/2);
		ch[cn].dir=dir;
	}

	ch[cn].mana-=FLASHCOST;

	
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

        return 1;
}

int do_magicshield(int cn)
{
	int str;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_MAGICSHIELD]) { error=ERR_UNKNOWN_SPELL; return 0; }

	if (ch[cn].mana<POWERSCALE) { error=ERR_MANA_LOW; return 0; }	// no magic shield with less than one point
	
	if ((ch[cn].flags&CF_PLAYER) && areaID==33) { error=ERR_UNKNOWN_SPELL; return 0; }

	str=ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE;
	str=min(str,ch[cn].mana*2);
	str=min(str,ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE-ch[cn].lifeshield);

	if (str<1) { error=ERR_NO_EFFECT; return 0; }

        ch[cn].mana-=str/2;

	ch[cn].action=AC_MAGICSHIELD;
	ch[cn].act1=str;
        ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);
	ch[cn].dir=bigdir(ch[cn].dir);

        return 1;
}

int do_flash(int cn)
{
	int cost,mod;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_FLASH]) { error=ERR_UNKNOWN_SPELL; return 0; }

	mod=ch[cn].value[0][V_FLASH]/20;
        cost=FLASHCOST;	//+mod*POWERSCALE;

	if (ch[cn].mana<cost) { error=ERR_MANA_LOW; return 0; }

        if (ch[cn].x<1 || ch[cn].x>=MAXMAP || ch[cn].y<1 || ch[cn].y>=MAXMAP) { error=ERR_ILLEGAL_COORDS; return 0; }

	if (!may_add_spell(cn,IDR_FLASH)) { error=ERR_ALREADY_WORKING; return 0; }

        ch[cn].mana-=cost;

	ch[cn].action=AC_FLASH;
	ch[cn].act1=mod;
        ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

	ch[cn].dir=bigdir(ch[cn].dir);

        return 1;
}

int do_warcry(int cn)
{
        if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) { error=ERR_UNCONCIOUS; return 0; }

	if (warcried(cn)) { error=ERR_UNCONCIOUS; return 0; }

	if (!ch[cn].value[0][V_WARCRY]) { error=ERR_UNKNOWN_SPELL; return 0; }

        if (ch[cn].endurance<ch[cn].value[0][V_WARCRY]*POWERSCALE/3) { error=ERR_MANA_LOW; return 0; }
	ch[cn].endurance-=ch[cn].value[0][V_WARCRY]*POWERSCALE/3;

        ch[cn].action=AC_WARCRY;
        ch[cn].duration=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,DUR_MAGIC_ACTION);
	if (ch[cn].speed_mode==SM_FAST) ch[cn].endurance-=end_cost(cn);

        return 1;
}

// ******** Part II: Instant Actions ********

// swap citem with inventory pos
int swap(int cn,int pos)
{
	int in,price;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	// no switching equipment while in teufel PK arena
	if (areaID==34 && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA) && pos!=WN_LHAND && pos<12) { error=ERR_ILLEGAL_ATTACK; return 0; }
	
	if ((in=ch[cn].citem)) {
		if (pos<0) {		// <0, illegal
			error=ERR_ILLEGAL_INVPOS;
			return 0;
		} else if (pos<12) {	// wear position
			if (!can_wear(cn,in,pos)) {
				error=ERR_REQUIREMENTS;
				return 0;
			}
		} else if (pos<30) {	// spell position
			error=ERR_ILLEGAL_INVPOS;
			return 0;
		} else if (pos<INVENTORYSIZE) {	// backpack
			;
		} else {		// >=INVENTORYSIZE, illegal
			error=ERR_ILLEGAL_INVPOS;
			return 0;
		}
	} else {
		if (pos<0 || (pos>=12 && pos<30) || pos>=INVENTORYSIZE) {
			error=ERR_ILLEGAL_INVPOS;
			return 0;
		}
	}

	// swap items
	ch[cn].citem=ch[cn].item[pos];
	
	if (it[in].flags&IF_MONEY) {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped into goldbag");
		price=destroy_money_item(in);
		ch[cn].gold+=price; stats_update(cn,0,price);
                ch[cn].item[pos]=0;
	} else ch[cn].item[pos]=in;

	ch[cn].flags|=CF_ITEMS;

	if (pos<12) update_char(cn);

        return 1;
}

int turn(int cn,int dir)
{
	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (dir<1 || dir>8) { error=ERR_ILLEGAL_DIR; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (ch[cn].dir!=dir) set_sector(ch[cn].x,ch[cn].y);

	ch[cn].dir=dir;

	return 1;
}

int container(int cn,int pos,int flag,int fast)
{
	int in,in2,ct,x,y,noquest=0;

        if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (!dx2offset(ch[cn].dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	x+=ch[cn].x;
	y+=ch[cn].y;
	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

	in=map[x+y*MAXMAP].it;
	if (!in) { error=ERR_NO_ITEM; return 0; }

        if (in<1 || in>=MAXITEM) {
		elog("container(): found illegal item %d on map pos %d,%d, removing.",in,x,y);
		map[x+y*MAXMAP].it=0;
		
		error=ERR_CONFUSED;
		return 0;
	}

	ct=it[in].content;
	if (!ct) { error=ERR_NOT_CONTAINER; return 0; }
	
	if (ct<1 || ct>=MAXCONTAINER) {
		elog("container(): found illegal container %d in item %s (%d), removing.",ct,it[in].name,in);
		it[in].content=0;
		
		error=ERR_CONFUSED;
		return 0;
	}

	if (pos<0 || pos>=INVENTORYSIZE) { error=ERR_ILLEGAL_POS; return 0; }

	if ((con[ct].owner && charID(cn)!=con[ct].owner)) {
		if (charID(cn)==con[ct].killer) {
			if (it[in].flags&IF_PLAYERBODY) noquest=1;			
		} else if (charID(cn)==con[ct].access) {
			noquest=1;
		} else {
			error=ERR_ACCESS_DENIED;
			return 0;
		}
	}
	if (con[ct].owner==charID(cn) && con[ct].owner_not_seyan==1 && (ch[cn].flags&(CF_MAGE|CF_WARRIOR))==(CF_MAGE|CF_WARRIOR)) noquest=1;

	if (flag) {	// swap
		in2=ch[cn].citem;
                if (it[in2].flags&IF_QUEST) { error=ERR_QUESTITEM; return 0; }

		in=con[ct].item[pos];

		if (noquest && in && (it[in].flags&IF_QUEST)) { error=ERR_QUESTITEM; return 0; }

		if (ch[cn].flags&CF_PLAYER) {
			if (in) {
				if (con[ct].in) dlog(cn,in,"took %s from container (item %d/%s/%s)",it[in].name,con[ct].in,it[con[ct].in].name,it[con[ct].in].description);
				else if (con[ct].cn) dlog(cn,in,"took %s from container (char %d/%s/%s)",it[in].name,con[ct].cn,ch[con[ct].cn].name,ch[con[ct].cn].description);
				else dlog(cn,in,"took %s from container (unspec?)",it[in].name);
			}

			if (in2) {
				if (con[ct].in) dlog(cn,in2,"dropped %s into container (item %d/%s/%s)",it[in2].name,con[ct].in,it[con[ct].in].name,it[con[ct].in].description);
				else if (con[ct].cn) dlog(cn,in2,"took %s from container (char %d/%s/%s)",it[in2].name,con[ct].cn,ch[con[ct].cn].name,ch[con[ct].cn].description);
				else dlog(cn,in2,"dropped %s into container (unspec?)",it[in2].name);
			}
		}

                ch[cn].citem=in; it[in].carried=cn; it[in].contained=0;
                con[ct].item[pos]=in2; it[in2].contained=ct; it[in2].carried=0;

		if (fast) store_citem(cn);
	
		ch[cn].flags|=CF_ITEMS;
	} else {
		if ((in=con[ct].item[pos]))
			look_item(cn,it+in);
	}

	return 1;
}

int use_item(int cn,int in)
{
	int ct;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	
	if (in<1 || in>=MAXITEM) { error=ERR_ILLEGAL_ITEMNO; return 0; }
	
	if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if ((ct=it[in].content)) {
		if (ct<1 || ct>=MAXCONTAINER) {
			elog("item %s (%d) has illegal container %d. resetting.",it[in].name,in,ct);
			it[in].content=0;
			error=ERR_CONFUSED;
			return 0;
		}
		if (con[ct].owner && charID(cn)!=con[ct].owner && charID(cn)!=con[ct].killer && charID(cn)!=con[ct].access) {	// access denied
			error=ERR_ACCESS_DENIED;
			log_char(cn,LOG_SYSTEM,0,"Permission denied.");
			return 0;
		}
		ch[cn].con_in=in;
		return 1;
	}

	if (it[in].flags&IF_DEPOT) {
		ch[cn].con_in=in;
		return 1;
	}

	return item_driver(it[in].driver,in,cn);
}

int look_map(int cn,int x,int y)
{
	int dir,m;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
        if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }
	
	if (x<1 || y<1 || x>=MAXMAP || y>=MAXMAP) { error=ERR_ILLEGAL_COORDS; return 0; }
	
	m=x+y*MAXMAP;

	dir=offset2dx(ch[cn].x,ch[cn].y,x,y);
	if (dir) turn(cn,dir);


	if (!los_can_see(cn,ch[cn].x,ch[cn].y,x,y,DIST)) {
		log_char(cn,LOG_SYSTEM,0,"Too far away or hidden.");
		return 0;
	}
	
	show_section(x,y,cn);

	if (map[m].flags&MF_RESTAREA) {
		log_char(cn,LOG_SYSTEM,0,"This place is a rest area.");
	}
	if (map[m].flags&MF_CLAN) {
		log_char(cn,LOG_SYSTEM,0,"This is a clan area.");
	}
	if (map[m].flags&MF_ARENA) {
		log_char(cn,LOG_SYSTEM,0,"This place is an arena.");
	}
	if (map[m].flags&MF_PEACE) {
		log_char(cn,LOG_SYSTEM,0,"This place is a peaceful zone.");
	}
	//log_char(cn,LOG_SYSTEM,0,"Light=%d.",map[m].light);

        return 1;
}

int look_inv(int cn,int pos)
{
	int in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
        if (ch[cn].flags&(CF_DEAD)) { error=ERR_DEAD; return 0; }

	if (pos<0 || pos>=INVENTORYSIZE) { error=ERR_ILLEGAL_POS; return 0; }

	in=ch[cn].item[pos];
	if (!in) {
		log_char(cn,LOG_SYSTEM,0,"Empty spaces...");
		return 1;
	}

	look_item(cn,it+in);

	return 1;
}

int char_swap(int cn)
{
	int co,x,y,m,xt,yt,m2;
	struct misc_ppd *ppd;

	if (!dx2offset(ch[cn].dir,&x,&y,NULL)) { error=ERR_ILLEGAL_DIR; return 0; }

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) { error=ERR_ILLEGAL_COORDS; return 0; }

        m=x+y*MAXMAP;

	if (!(co=map[m].ch)) { error=ERR_NO_CHAR; return 0; }	

	if (!(ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE|CF_ALLOWSWAP))) { error=ERR_NOT_PLAYER; return 0; }	

	if (ch[co].flags&CF_INVISIBLE) { error=ERR_NOT_PLAYER; return 0; }	

	if (ch[cn].action!=AC_IDLE) { error=ERR_NOT_IDLE; return 0; }
	if (ch[co].action!=AC_IDLE) { error=ERR_NOT_IDLE; return 0; }

	m2=ch[cn].x+ch[cn].y*MAXMAP;

	if ((map[m].flags&MF_PEACE) && !(map[m2].flags&MF_PEACE)) { error=ERR_ACCESS_DENIED; return 0; }
	if (!(map[m].flags&MF_PEACE) && (map[m2].flags&MF_PEACE)) { error=ERR_ACCESS_DENIED; return 0; }

	if ((map[m].flags&MF_UNDERWATER) && !(map[m2].flags&MF_UNDERWATER)) { error=ERR_ACCESS_DENIED; return 0; }
	if (!(map[m].flags&MF_UNDERWATER) && (map[m2].flags&MF_UNDERWATER)) { error=ERR_ACCESS_DENIED; return 0; }

	xt=ch[cn].x;
	yt=ch[cn].y;

	remove_char(cn);
	remove_char(co);

	set_char(cn,x,y,0);
	set_char(co,xt,yt,0);

	dlog(co,0,"was swapped by %s",ch[cn].name);

	if ((ch[cn].flags&CF_PLAYER) && (ppd=set_data(cn,DRD_MISC_PPD,sizeof(struct misc_ppd)))) ppd->swapped=realtime;

	return 1;
}

int equip_item(int cn,int in,int pos)
{
	int n;

	if (ch[cn].citem) return 0;
	
	if (pos<12) {	// item is worn
		if (!ch[cn].item[pos]) return 0;
                if (!swap(cn,pos)) return 0;
		return store_citem(cn);
	}
	// item is in inventory
	for (n=0; n<12; n++)
		if (!ch[cn].item[n] && can_wear(cn,in,n)) break;
	if (n<12) {
		if (!swap(cn,pos)) return 0;
		return swap(cn,n);
	}
	// special case for two-handed weapons. yuck
	if ((it[in].flags&IF_TWOHAND) && ch[cn].item[WN_LHAND]) {
		swap(cn,WN_LHAND);
		store_citem(cn);
	}

	for (n=0; n<12; n++)
		if (can_wear(cn,in,n)) break;
	if (n<12) {
		if (!swap(cn,pos)) return 0;
		if (!swap(cn,n)) return 0;
                return swap(cn,pos);
	}

	return 0;
}
















