/*
 * $Id: act.c,v 1.6 2007/08/21 22:07:19 devel Exp $ (c) D.Brockhaus
 *
 * Act is the counterpart to Do. When a character wants to do something, he calls the
 * corresponding Do-function. This triggers the execution of the Act-function of the
 * same name some time later (exact time depends on character speed and the action).
 *
 * Act is the part which actually does something. act_walk() moves a character from
 * one tile to the next, act_attack hits (and possibly hurts) a character etc.
 *
 *
 * $Log: act.c,v $
 * Revision 1.6  2007/08/21 22:07:19  devel
 * *** empty log message ***
 *
 * Revision 1.5  2007/05/26 13:19:36  devel
 * can no longer give items to dying character
 *
 * Revision 1.4  2006/04/26 16:05:44  ssim
 * added hack for teufelheim arena: no bless other allowed
 *
 * Revision 1.3  2006/03/30 12:16:01  ssim
 * changed some elogs to xlogs to avoid cluttering the error log file with unimportant errors
 *
 * Revision 1.2  2005/11/06 14:06:07  ssim
 * added check for lifeshield<0 in regenerate()
 *
 *
 */

#include <stdlib.h>

#include "server.h"
#include "log.h"
#include "direction.h"
#include "notify.h"
#include "libload.h"
#include "light.h"
#include "tool.h"
#include "map.h"
#include "death.h"
#include "create.h"
#include "effect.h"
#include "death.h"
#include "timer.h"
#include "talk.h"
#include "drvlib.h"
#include "database.h"
#include "drdata.h"
#include "do.h"
#include "see.h"
#include "spell.h"
#include "container.h"
#include "path.h"
#include "sector.h"
#include "area.h"
#include "date.h"
#include "balance.h"
#include "player.h"
#include "consistency.h"
#include "act.h"


#define RAGELESS	25
#define LIFELESS	25
#define DURATION	35

static void reduce_rage(int cn)
{
	int rage_diff;
		
	if (ch[cn].value[1][V_RAGE]) {
		rage_diff=ch[cn].rage;
		ch[cn].rage-=rage_diff/RAGELESS;
	}
}

static void increase_rage(int cn)
{
	int rage_diff;
		
	if (ch[cn].value[1][V_RAGE]) {
		rage_diff=(ch[cn].value[0][V_RAGE]*POWERSCALE-ch[cn].rage);
		ch[cn].rage+=rage_diff/20;
	}
}

static int act_idle(int cn)
{
	int val,m;

	if (ch[cn].x<1 || ch[cn].x>MAXMAP-2 || ch[cn].y<1 || ch[cn].y>MAXMAP-2) return 0;
	
	m=ch[cn].x+ch[cn].y*MAXMAP;

	if (ticker>ch[cn].regen_ticker+REGEN_TIME && (!(map[m].flags&MF_NOREGEN) || !(ch[cn].flags&CF_PLAYER)) ) {
		
                if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE && areaID!=33) {
                        if (!(val=ch[cn].value[0][V_REGENERATE])) val=7;
			ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+ch[cn].act1*val*15);
			ch[cn].flags|=CF_SMALLUPDATE;
		}

		if (ch[cn].endurance<ch[cn].value[0][V_ENDURANCE]*POWERSCALE) {
                        val=150;
			ch[cn].endurance=min(ch[cn].value[0][V_ENDURANCE]*POWERSCALE,ch[cn].endurance+ch[cn].act1*val*15);
			ch[cn].flags|=CF_SMALLUPDATE;
		}
		
                if (ch[cn].mana<ch[cn].value[0][V_MANA]*POWERSCALE) {
                        if (!(val=ch[cn].value[0][V_MEDITATE])) val=7;
			ch[cn].mana=min(ch[cn].value[0][V_MANA]*POWERSCALE,ch[cn].mana+ch[cn].act1*val*15);
			ch[cn].flags|=CF_SMALLUPDATE;
		}

		if (!ch[cn].value[1][V_MAGICSHIELD] && ch[cn].lifeshield) {
			ch[cn].lifeshield=0;
			ch[cn].flags|=CF_SMALLUPDATE;
		}
	}

	reduce_rage(cn);

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);

	return 1;
}

static int act_walk(int cn)
{
	int m,in;

        if (ch[cn].tox<1 || ch[cn].tox>MAXMAP-2 || ch[cn].toy<1 || ch[cn].toy>MAXMAP-2) {
		elog("act_walk(): character %s (%d) has illegal to-position %d,%d.",ch[cn].name,cn,ch[cn].tox,ch[cn].toy);
		ch[cn].tox=ch[cn].toy=0;
		return 0;
	}

	m=ch[cn].x+ch[cn].y*MAXMAP;

        if (map[m].ch!=cn) {
		elog("act_walk(): character %s (%d) is about to leave position %d,%d, but is not in map.ch.",ch[cn].name,cn,ch[cn].x,ch[cn].y);
	} else {
		if (!(map[m].flags&MF_TMOVEBLOCK)) {
			elog("act_walk(): character %s (%d) is about to leave position %d,%d, but is MF_TMOVEBLOCK is not set in map.flags.",ch[cn].name,cn,ch[cn].x,ch[cn].y);
		}		
                map[m].ch=0;
		map[m].flags&=~MF_TMOVEBLOCK;
	}	
		
	remove_char_light(cn);
	set_sector(ch[cn].x,ch[cn].y);
	del_char_sector(cn);
	
	ch[cn].x=ch[cn].tox; ch[cn].tox=0;
	ch[cn].y=ch[cn].toy; ch[cn].toy=0;

	m=ch[cn].x+ch[cn].y*MAXMAP;

	if (map[m].ch) {
		elog("act_walk(): character %s (%d) is about to enter position %d,%d, but there's already someone in map.ch",ch[cn].name,cn,ch[cn].x,ch[cn].y);
	}

	map[m].ch=cn;

	if ((ch[cn].flags&CF_PLAYER) && (map[m].flags&MF_RESTAREA)) {
		if (ch[cn].resta!=areaID || abs(ch[cn].x-ch[cn].restx)+abs(ch[cn].y-ch[cn].resty)>10) {
			log_char(cn,LOG_SYSTEM,0,"Rest area reached. Re-enter place set.");
		}
		ch[cn].restx=ch[cn].x;
		ch[cn].resty=ch[cn].y;
		ch[cn].resta=areaID;
	}
	
	add_char_light(cn);
	add_char_sector(cn);
        if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);

	walk_section_msg(cn);

	if ((map[m].flags&MF_NOMAGIC) && !(ch[cn].flags&CF_NOMAGIC)) {
		ch[cn].flags|=CF_NOMAGIC;
		update_char(cn);
	}
	if (!(map[m].flags&MF_NOMAGIC) && (ch[cn].flags&CF_NOMAGIC)) {
		ch[cn].flags&=~CF_NOMAGIC;
		update_char(cn);
	}

	if ((in=map[m].it)) {
		if (in<1 || in>=MAXITEM) {
			elog("act_walk(): found illegal item %d at %d,%d. removing.",in,ch[cn].x,ch[cn].y);
			map[m].it=0;
			return 1;
		}		
		if (it[in].flags&IF_STEPACTION) {
			item_driver(it[in].driver,in,cn);
		}
	}

        reduce_rage(cn);

	return 1;
}

static int act_take(int cn)
{
	int x,y,m,in;

        if (ch[cn].citem) return 0;	// already holding something	

	if (!dx2offset(ch[cn].dir,&x,&y,NULL)) {
		elog("act_take(): dx2offset(%d,...) returned error for character %s (%d)",ch[cn].dir,ch[cn].name,cn);
		return 0;
	}

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		elog("act_take(): character %s (%d) tried to take item from illegal position %d,%d.",ch[cn].name,cn,x,y);		
		return 0;
	}

	m=x+y*MAXMAP;

	if (!(in=map[m].it)) return 0;		// somebody else took it
	if (in!=ch[cn].act1) return 0;		// that's not what he wanted to take

	if (in<1 || in>=MAXITEM) {
		elog("act_take(): item number %d on map position %d,%d is out of bounds.",in,x,y);
		map[m].it=0;
		return 0;
	}

	if (!(it[in].flags&IF_TAKE)) return 0;	// item isn't takeable

	if (!can_carry(cn,in,1)) return 0;	// he may not take that

	if (!remove_item_map(in)) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"took %s",it[in].name);
	
        ch[cn].citem=in;
	it[in].carried=cn;
	ch[cn].flags|=CF_ITEMS;

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);

	reduce_rage(cn);

	return 1;	
}

static int act_use(int cn)
{
	int x,y,m,in;

        if (!dx2offset(ch[cn].dir,&x,&y,NULL)) {
		elog("act_use(): dx2offset(%d,...) returned error for character %s (%d)",ch[cn].dir,ch[cn].name,cn);
		return 0;
	}

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		elog("act_use(): character %s (%d) tried to take item from illegal position %d,%d.",ch[cn].name,cn,x,y);		
		return 0;
	}

	m=x+y*MAXMAP;

	if (!(in=map[m].it)) return 0;		// somebody else took it
	if (in!=ch[cn].act1) return 0;		// that's not what he wanted to use

	if (in<1 || in>=MAXITEM) {
		elog("act_use(): item number %d on map position %d,%d is out of bounds.",in,x,y);
		map[m].it=0;
		return 0;
	}

	if (!(it[in].flags&IF_USE)) return 0;	// item isn't useable

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);

	reduce_rage(cn);

	return use_item(cn,in);
}

static int act_drop(int cn)
{
	int x,y,m,in;

        if (!(in=ch[cn].citem)) return 0; 	// not holding anything
	if (in!=ch[cn].act1) return 0;		// that's not what he wanted to drop

	if (!dx2offset(ch[cn].dir,&x,&y,NULL)) {
		elog("act_drop(): dx2offset(%d,...) returned error for character %s (%d)",ch[cn].dir,ch[cn].name,cn);
		return 0;
	}

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		elog("act_drop(): character %s (%d) tried to drop item to illegal position %d,%d.",ch[cn].name,cn,x,y);		
		return 0;
	}

	m=x+y*MAXMAP;

	if (map[m].it) return 0; 		// no free space

	if (in<1 || in>=MAXITEM) {
		elog("act_drop(): item number %d in citem of character %s (%d) is out of bounds.",in,ch[cn].name,cn);
		ch[cn].citem=0;
		return 0;
	}

	if (it[in].flags&IF_QUEST) return 0;

	if (it[in].flags&IF_NODROP) return 0;

        if (!set_item_map(in,x,y)) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped %s",it[in].name);

	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_ITEM,in,0,0);

	reduce_rage(cn);

	return 1;	
}

static inline int check_lightm(int m)
{
        return min(255,max(map[m].light,(dlight*map[m].dlight)/256));
}

static int sub_attack(int cn,int co,int vcn,int vco,int direct)
{
	int diff,chance,dam,per,tmp,m;
	extern int showattack;

	if (!can_attack(cn,co)) return 0;	// attack is not allowed

	// characters in dark places are at a disadvantage
	if (!(ch[cn].flags&(CF_INFRARED|CF_INFRARED)) && check_lightm(ch[co].x+ch[co].y*MAXMAP)<2) vcn-=8;	
	if (!(ch[co].flags&(CF_INFRARED|CF_INFRARED)) && check_lightm(ch[cn].x+ch[cn].y*MAXMAP)<2) vco-=8;

	// fighting in swamp
	if (ch[cn].flags&CF_PLAYER) {
		m=ch[cn].x+ch[cn].y*MAXMAP;
		if (map[m].gsprite>=59405 && map[m].gsprite<=59413) vcn-=6;
		if (map[m].gsprite>=59414 && map[m].gsprite<=59422) vcn-=12;
		if (map[m].gsprite>=59423 && map[m].gsprite<=59431) vcn-=18;
	}
	if (ch[co].flags&CF_PLAYER) {
		m=ch[co].x+ch[co].y*MAXMAP;
		if (map[m].gsprite>=59405 && map[m].gsprite<=59413) vco-=6;
		if (map[m].gsprite>=59414 && map[m].gsprite<=59422) vco-=12;
		if (map[m].gsprite>=59423 && map[m].gsprite<=59431) vco-=18;
	}

	diff=vcn-vco;

	if (ch[cn].flags&CF_EDEMON) {	// earth demons hit harder...
		diff+=edemon_reduction(co,ch[cn].value[1][V_DEMON])*3;
	}
	if (ch[co].flags&CF_EDEMON) {	// ... and get hit less often
		diff-=max(0,ch[co].value[1][V_DEMON]-ch[cn].value[1][V_DEMON])*3;
	}

        if (diff<-146)      { chance=10; per=90; }
	else if (diff<-128) { chance=11; per=90; }
	else if (diff<-112) { chance=12; per=90; }
        else if (diff<-96) { chance=13; per=90; }
        else if (diff<-80) { chance=14; per=90; }
        else if (diff<-72) { chance=15; per=90; }
	else if (diff<-64) { chance=16; per=90; }
        else if (diff<-56) { chance=17; per=90; }
        else if (diff<-48) { chance=18; per=90; }
        else if (diff<-40) { chance=19; per=90; }
        else if (diff<-36) { chance=20; per=90; }
        else if (diff<-32) { chance=22; per=90; }
        else if (diff<-28) { chance=24; per=90; }
        else if (diff<-24) { chance=26; per=90; }
        else if (diff<-20) { chance=28; per=90; }
        else if (diff<-18) { chance=30; per=90; }
	else if (diff<-16) { chance=32; per=90; }
        else if (diff<-14) { chance=34; per=90; }
        else if (diff<-12) { chance=36; per=90; }
        else if (diff<-10) { chance=38; per=90; }
        else if (diff<- 8) { chance=40; per=90; }
        else if (diff< -6) { chance=42; per=90; }
        else if (diff< -4) { chance=44; per=90; }
        else if (diff< -2) { chance=46; per=90; }
        else if (diff<  0) { chance=48; per=90; }
        else if (diff== 0) { chance=50; per=90; }
        else if (diff<  2) { chance=52; per=90; }
        else if (diff<  4) { chance=54; per=90; }
        else if (diff<  6) { chance=56; per=90; }
        else if (diff<  8) { chance=58; per=90; }
        else if (diff< 10) { chance=60; per=90; }
        else if (diff< 12) { chance=62; per=90; }
        else if (diff< 14) { chance=64; per=90; }
        else if (diff< 16) { chance=66; per=85; }
        else if (diff< 18) { chance=68; per=80; }
        else if (diff< 20) { chance=70; per=75; }
	else if (diff< 24) { chance=72; per=70; }
        else if (diff< 28) { chance=74; per=65; }
        else if (diff< 32) { chance=76; per=60; }
	else if (diff< 36) { chance=78; per=55; }
        else if (diff< 40) { chance=80; per=50; }
        else if (diff< 44) { chance=81; per=45; }
	else if (diff< 48) { chance=82; per=40; }
        else if (diff< 52) { chance=83; per=35; }
        else if (diff< 56) { chance=84; per=30; }
	else if (diff< 60) { chance=85; per=25; }
        else if (diff< 64) { chance=86; per=20; }
        else if (diff< 68) { chance=87; per=15; }
	else if (diff< 72) { chance=89; per=10; }
        else               { chance=90; per= 5; }

	if ((tmp=die(1,100))<chance) {
		dam=ch[cn].value[0][V_WEAPON]+die(1,6);
		
		if (direct && ch[cn].prof[P_ASSASSIN] && is_back(co,cn) && ch[co].action==AC_IDLE) {
                        dam+=ch[cn].prof[P_ASSASSIN]*5;			
		}

                if (dam<0) dam=0;
		if (direct) sound_area(ch[cn].x,ch[cn].y,7);
	} else {
		dam=0;
		if (direct) {
			if (!ch[cn].item[WN_RHAND] || !ch[co].item[WN_RHAND]) sound_area(ch[cn].x,ch[cn].y,8);
			else if (!RANDOM(2)) sound_area(ch[cn].x,ch[cn].y,34);
			else sound_area(ch[cn].x,ch[cn].y,35);
		}
	}

        if (showattack) say(cn,"attack %s, diff=%d (%d %d), chan=%d, percent=%d, dam=%d",	//, vcn=%d (%s), vco=%d (%s)
			    ch[co].name,
			    diff,
			    vcn,
			    vco,
                            chance,
			    per,
			    dam);

	hurt(co,dam*POWERSCALE/ATTACK_DIV,cn,ATTACK_DIV,per,75+per/4);

	return dam;
}

static int sub_surround(int cn,int dx,int dy)
{
	int x,y,co,vcn,vco;

	x=ch[cn].x+dx;
	y=ch[cn].y+dy;

	if (!(co=map[x+y*MAXMAP].ch)) return 0;

	vcn=get_surround_attack_skill(cn);
        vco=get_parry_skill(co);

	//if (ch[cn].flags&CF_PLAYER) say(cn,"sub attack %s %s (surround)",ch[cn].name,ch[co].name);
        return sub_attack(cn,co,vcn,vco,0);
}

static int act_attack(int cn)
{
	int x,y,m,co,vcn,vco;

        if (!dx2offset(ch[cn].dir,&x,&y,NULL)) {
		elog("act_attack(): dx2offset(%d,...) returned error for character %s (%d)",ch[cn].dir,ch[cn].name,cn);
		return 0;
	}

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		elog("act_attack(): character %s (%d) tried to attack at illegal position %d,%d.",ch[cn].name,cn,x,y);		
		return 0;
	}

	m=x+y*MAXMAP;

	co=ch[cn].act1;

	if (co<1 || co>=MAXCHARS) {
		elog("act_attack: got illegal co: %d",co);
		return 0;
	}

	// the following searches for the intended victim in a 3x3 square around
	// the original target tile. this allows to hit characters who're running
	// away.
	if (co!=map[m].ch &&
	    co!=map[m+1].ch &&
	    co!=map[m-1].ch &&
	    co!=map[m+MAXMAP].ch &&
	    co!=map[m-MAXMAP].ch &&
	    co!=map[m+MAXMAP+1].ch &&
	    co!=map[m+MAXMAP-1].ch &&
	    co!=map[m-MAXMAP+1].ch &&
	    co!=map[m-MAXMAP-1].ch) return 0;		// not the one he intended to hit
	
        // ok, let the fun begin (the attack is legal)

	vcn=get_attack_skill(cn);
        vco=get_parry_skill(co);

	if (!is_facing(co,cn)) { // attacking someone from the side or back gets a bonus
		vco-=8;	
		if (ch[cn].prof[P_ASSASSIN]) vcn+=ch[cn].prof[P_ASSASSIN];	// assassins get special bonus
	}
	if (is_back(co,cn)) {	// attacking someone from the back gets an additional bonus
		vco-=8;	
		if (ch[cn].prof[P_ASSASSIN] && ch[co].action==AC_IDLE)
			vcn+=ch[cn].prof[P_ASSASSIN]*2;		// backstab
	}

        sub_attack(cn,co,vcn,vco,1);	
	if (!ch[cn].flags) return 0;	// sub_attack might kill cn or co

	if (ch[cn].value[0][V_SURROUND]) {
		if (ch[cn].dir==DX_LEFT || ch[cn].dir==DX_RIGHT) {
			sub_surround(cn, 0, 1);
			if (!ch[cn].flags) return 0; // sub_attack might kill cn or co
			sub_surround(cn, 0,-1);
			if (!ch[cn].flags) return 0;
		} else {
			sub_surround(cn, 1, 0);
			if (!ch[cn].flags) return 0;
			sub_surround(cn,-1, 0);
			if (!ch[cn].flags) return 0;
		}
	}

	increase_rage(cn);

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);

        return 1;
}

static int act_give(int cn)
{
	int x,y,m,co,in;

        if (!dx2offset(ch[cn].dir,&x,&y,NULL)) {
		elog("act_give(): dx2offset(%d,...) returned error for character %s (%d)",ch[cn].dir,ch[cn].name,cn);
		return 0;
	}

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		elog("act_give(): character %s (%d) tried to attack at illegal position %d,%d.",ch[cn].name,cn,x,y);		
		return 0;
	}

	m=x+y*MAXMAP;

	co=ch[cn].act1;

	if (co<1 || co>=MAXCHARS) {
		elog("act_give: got illegal co: %d",co);
		return 0;
	}

	if (map[m].ch!=co) return 0;	// he moved away

        if (ch[co].flags&(CF_NOGIVE|CF_DEAD)) return 0;

	if (!(in=ch[cn].citem)) return 0;

	if ((it[in].flags&IF_QUEST) && !(ch[co].flags&(CF_QUESTITEM|CF_GOD)) && !(ch[cn].flags&(CF_QUESTITEM|CF_GOD))) return 0;

        if (!can_carry(co,in,1)) return 0;

	if (ch[co].flags&CF_PLAYER) {
		if (!give_char_item(co,in)) return 0;
	} else {
		if (ch[co].citem) return 0;
		
		ch[co].citem=in;
		it[in].carried=co;
		ch[co].flags|=CF_ITEMS;
	}

	ch[cn].citem=0; ch[cn].flags|=CF_ITEMS;

        if (ch[cn].flags&CF_PLAYER) {
		dlog(cn,in,"gave %s to %s",it[in].name,ch[co].name);
		log_char(cn,LOG_SYSTEM,0,"You gave a %s (%s) to %s.",it[in].name,it[in].description,ch[co].name);
	}
	if (ch[co].flags&CF_PLAYER) {
		dlog(co,in,"was given %s from %s",it[in].name,ch[cn].name);
		log_char(co,LOG_SYSTEM,0,"You received a %s (%s) from %s.",it[in].name,it[in].description,ch[cn].name);
	}

        notify_char(co,NT_GIVE,cn,in,0);

        if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);	

	reduce_rage(cn);

        return 1;
}

static int act_firering(int cn)
{
	int co,fn,dam,fre,in;
        int xs,xe,ys,ye,x,y,maxdist=1;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	// create spell item to block re-cast for 1 sec
	if (!(fre=may_add_spell(cn,IDR_FIRERING))) return 0;

        in=create_item("firering_spell");
	if (!in) return 0;

        it[in].driver=IDR_FIRERING;

	*(signed long*)(it[in].drdata)=ticker+TICKS;	// one second
	*(signed long*)(it[in].drdata+4)=ticker;

	it[in].carried=cn;

	ch[cn].item[fre]=in;

	create_spell_timer(cn,in,fre);

	// actual spell effect
        fn=create_show_effect(EF_FIRERING,cn,ticker,ticker+7,20,50);

	xs=max(1,ch[cn].x-maxdist);
	xe=min(MAXMAP-2,ch[cn].x+maxdist);
	ys=max(1,ch[cn].y-maxdist);
	ye=min(MAXMAP-2,ch[cn].y+maxdist);

	for (y=ys; y<=ye; y++) {
		for (x=xs; x<=xe; x++) {
			if (!(co=map[x+y*MAXMAP].ch) || cn==co) continue;
			if (!can_attack(cn,co)) continue;
			
			create_show_effect(EF_BURN,co,ticker,ticker+8,20,0);

			dam=fireball_damage(cn,co,spellpower(cn,V_FIREBALL));
                        hurt(co,dam,cn,10,30,85);			
		}
	}

	if (ch[cn].flags) {	// need to check here since hurt might kill cn (indirectly, via a char-dead driver)
		if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
		notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FIREBALL,fn);
		sound_area(ch[cn].x,ch[cn].y,5);
	}

	return 1;
}

static int act_fireball(int cn)
{
	int fn;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

        fn=create_fireball(cn,ch[cn].x,ch[cn].y,ch[cn].act1,ch[cn].act2,spellpower(cn,V_FIREBALL));

	if (fn) {
		if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
		notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FIREBALL,fn);
		sound_area(ch[cn].x,ch[cn].y,5);
	}

        ch[cn].action=AC_FIREBALL2;
	ch[cn].step=0;

	return 1;
}

static int act_earthrain(int cn)
{
	int fn;

        fn=create_earthrain(cn,ch[cn].act1%MAXMAP,ch[cn].act1/MAXMAP,ch[cn].act2);

	//notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FIREBALL,fn);

        return 1;
}

static int act_earthmud(int cn)
{
	int fn;

        fn=create_earthmud(cn,ch[cn].act1%MAXMAP,ch[cn].act1/MAXMAP,ch[cn].act2);

	//notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FIREBALL,fn);

        return 1;
}

static int act_flash(int cn)
{
	int fre,in,fn,duration;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	if (!(fre=may_add_spell(cn,IDR_FLASH))) return 0;

	duration=FLASHDURATION;
	if (ch[cn].value[1][V_DURATION]) duration+=FLASHDURATION*ch[cn].value[0][V_DURATION]/DURATION;
	else if (ch[cn].flags&CF_ARCH) duration+=FLASHDURATION*ch[cn].level/DURATION/2;

	in=create_item("flash_spell");
	if (!in) return 0;

        it[in].mod_value[0]=100;

	it[in].driver=IDR_FLASH;

	*(signed long*)(it[in].drdata)=ticker+duration;
	*(signed long*)(it[in].drdata+4)=ticker;

	it[in].carried=cn;

        ch[cn].item[fre]=in;

        create_spell_timer(cn,in,fre);

        fn=create_flash(cn,spellpower(cn,V_FLASH),duration);

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FLASH,fn);

	return 1;
}


static int act_ball(int cn)
{
	int fn;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

        fn=create_ball(cn,ch[cn].x,ch[cn].y,ch[cn].act1,ch[cn].act2,spellpower(cn,V_FLASH));
	if (fn) {	
		if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
		notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FLASH,fn);
	}

        ch[cn].action=AC_BALL2;
	ch[cn].step=0;

	return 1;
}

static int act_ok(int cn)
{
        return 1;
}

static int act_magicshield(int cn)
{
	int str;

	str=ch[cn].act1;
	if (str<1) return 0;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	ch[cn].lifeshield=min(ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE,ch[cn].lifeshield+str);

	create_show_effect(EF_MAGICSHIELD,cn,ticker,ticker+3,25,0);

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_MAGICSHIELD,0);

	return 1;
}

static int act_die(int cn)
{
	if (die_char(cn,ch[cn].act1,ch[cn].act2)) 	// character is really dead?
		ch[cn].step=0;				// dont allow another driver call then

        return 1;
}

int bless_someone(int co,int strength,int duration)
{
	int in,fre=-1;

        if (!(fre=may_add_spell(co,IDR_BLESS))) return 0;

	if ((in=ch[co].item[fre])) {
		destroy_effect_type(co,EF_BLESS);
                destroy_item(in);
		ch[co].item[fre]=0;
		update_char(co);
	}

        in=create_item("bless_spell");
	if (!in) return 0;

        it[in].mod_value[0]=strength/4;
	it[in].mod_value[1]=strength/4;
	it[in].mod_value[2]=strength/4;
	it[in].mod_value[3]=strength/4;

	it[in].driver=IDR_BLESS;

	*(signed long*)(it[in].drdata)=ticker+duration;
	*(signed long*)(it[in].drdata+4)=ticker;
	*(signed long*)(it[in].drdata+8)=strength;

	it[in].carried=co;

        ch[co].item[fre]=in;

	create_spell_timer(co,in,fre);

	update_char(co);

	return 1;
}

int bless_self(int cn)
{
	int in,fre=-1,duration;

        if (!(fre=may_add_spell(cn,IDR_BLESS))) return 0;

	if ((in=ch[cn].item[fre])) {
		destroy_effect_type(cn,EF_BLESS);
                destroy_item(in);
		ch[cn].item[fre]=0;
		update_char(cn);
	}

        in=create_item("bless_spell");
	if (!in) return 0;

        it[in].mod_value[0]=ch[cn].value[0][V_BLESS]/4;
	it[in].mod_value[1]=ch[cn].value[0][V_BLESS]/4;
	it[in].mod_value[2]=ch[cn].value[0][V_BLESS]/4;
	it[in].mod_value[3]=ch[cn].value[0][V_BLESS]/4;

	it[in].driver=IDR_BLESS;

	duration=BLESSDURATION;
	if (ch[cn].value[1][V_DURATION]) duration+=BLESSDURATION*ch[cn].value[0][V_DURATION]/DURATION;
	else if (ch[cn].flags&CF_ARCH) duration+=BLESSDURATION*ch[cn].level/DURATION/2;

	*(signed long*)(it[in].drdata)=ticker+duration;
	*(signed long*)(it[in].drdata+4)=ticker;
	*(signed long*)(it[in].drdata+8)=ch[cn].value[0][V_BLESS];

	it[in].carried=cn;

        ch[cn].item[fre]=in;

	create_spell_timer(cn,in,fre);

	update_char(cn);

	return 1;
}

static int act_bless(int cn,int flag)
{
	int co,duration;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	co=ch[cn].act1;
	if (co<1 || co>=MAXCHARS || !char_see_char(cn,co)) return 0;

	if (!can_help(cn,co)) return 0;

	if (cn!=co && (ch[cn].flags&CF_PLAYER) && (ch[co].flags&CF_NOBLESS)) return 0;

	// dont allow bless other in teufelheim arena
	if (areaID==34 && cn!=co && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) return 0;

	if (cn==co) {
		if (!bless_self(cn)) return 0;
	} else {
		duration=BLESSDURATION;
		if (ch[cn].value[1][V_DURATION]) duration+=BLESSDURATION*ch[cn].value[0][V_DURATION]/DURATION;
		else if (ch[cn].flags&CF_ARCH) duration+=BLESSDURATION*ch[cn].level/DURATION/2;

                if (!bless_someone(co,ch[cn].value[0][V_BLESS],duration)) return 0;
	}

        if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_BLESS,0);
	sound_area(ch[cn].x,ch[cn].y,29);

	if (flag) {
		ch[cn].action=AC_BLESS2;
		ch[cn].step=0;
	}

        return 1;
}

static void warcry_someone(int cn,int co,int pwr)
{
	int fre,in,endtime,dam,str;

	if (!can_attack(cn,co)) return;

	if (!(fre=may_add_spell(co,IDR_WARCRY))) return;

	str=warcry_value(cn,co,pwr);
        //say(cn,"warcry of pwr %d giving str %d",pwr,str);
        if (str>=0) return;	// no speed ups!

	in=create_item("freeze_spell");
	if (!in) return;

        it[in].mod_value[0]=str;
	
	it[in].driver=IDR_WARCRY;

        if (ch[cn].flags&CF_ARCH) endtime=ticker+WARCRYDURATION+WARCRYDURATION*ch[cn].level/DURATION/2;
	else endtime=ticker+WARCRYDURATION;

	*(signed long*)(it[in].drdata)=endtime;
	*(signed long*)(it[in].drdata+4)=ticker;

	it[in].carried=co;

        ch[co].item[fre]=in;

	//set_timer(endtime,remove_spell,co,in,fre,ch[co].serial,it[in].serial);
	create_spell_timer(co,in,fre);

	dam=warcry_damage(cn,co,pwr);

	if (dam>POWERSCALE/2) {
		destroy_effect_type(co,EF_FLASH);
		if (ch[co].action==EF_FLASH) { ch[co].action=AC_IDLE; ch[co].step=1; ch[co].duration=2; } // !!! bug? !!!
		switch(ch[co].action) {
			case AC_FIREBALL1:
			case AC_FIREBALL2:
			case AC_MAGICSHIELD:
			case AC_HEAL_SELF:
			case AC_HEAL1:
			case AC_HEAL2:
			case AC_BLESS_SELF:
			case AC_BLESS1:
			case AC_BLESS2:
			case AC_FREEZE:
			case AC_BALL1:
			case AC_BALL2:
			case AC_FLASH:
			case AC_PULSE:		ch[co].action=AC_IDLE; ch[co].step=1; ch[co].duration=2; break;
		}
	}

	update_char(co);

        hurt(co,dam,cn,1,0,0);

        if (ch[co].flags) {
		notify_char(co,NT_GOTHIT,cn,0,0);
		notify_area(ch[co].x,ch[co].y,NT_SEEHIT,cn,co,0);

		log_char(co,LOG_SYSTEM,0,"You hear %s's warcry.",ch[cn].name);
	}
}

// pathfinder block check routines
static int warcry_check_target(int m)
{
	if (map[m].flags&(MF_SOUNDBLOCK|MF_TSOUNDBLOCK|MF_SHOUTBLOCK)) return 0;

	return 1;
}


static int act_warcry(int cn)
{
	int co;
	int xs,xe,ys,ye,x,y,maxdist=10;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	xs=max(1,ch[cn].x-maxdist);
	xe=min(MAXMAP-2,ch[cn].x+maxdist);
	ys=max(1,ch[cn].y-maxdist);
	ye=min(MAXMAP-2,ch[cn].y+maxdist);

	for (y=ys; y<=ye; y++) {
		for (x=xs; x<=xe; x++) {
			if (!(co=map[x+y*MAXMAP].ch) || cn==co) continue;
			
			if (pathfinder(ch[cn].x,ch[cn].y,x,y,0,warcry_check_target,50)==-1) continue;

                        warcry_someone(cn,co,spellpower(cn,V_WARCRY));
		}
	}

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_WARCRY,0);

	if (!ch[cn].value[1][V_MAGICSHIELD] && areaID!=33) {
                ch[cn].lifeshield=min(get_lifeshield_max(cn)*POWERSCALE,ch[cn].lifeshield+ch[cn].value[0][V_WARCRY]*POWERSCALE/2);
	}

	return 1;
}

static int ice_curse(int co,int str,int max)
{
	int in,fre=-1,duration;

        if (!(fre=add_same_spell(co,IDR_CURSE))) return 0;

	duration=30*60*TICKS;

	if (!(in=ch[co].item[fre])) {
		in=create_item("bless_spell");
		if (!in) return 0;
		
		it[in].mod_value[0]=-str;
		it[in].mod_value[1]=-str;
		it[in].mod_value[2]=-str;
		it[in].mod_value[3]=-str;

		it[in].driver=IDR_CURSE;

		*(signed long*)(it[in].drdata)=ticker+duration;
		*(signed long*)(it[in].drdata+4)=ticker;

		it[in].carried=co;

		ch[co].item[fre]=in;

		create_spell_timer(co,in,fre);
	} else {
		int n,fn;

		if (-it[in].mod_value[0]>=max) return 0;
		if (-it[in].mod_value[0]+str>max) {
			str=max+it[in].mod_value[0];
			log_char(co,LOG_SYSTEM,0,"reduced str to %d",str);
		}

		it[in].mod_value[0]-=str;
		it[in].mod_value[1]-=str;
		it[in].mod_value[2]-=str;
		it[in].mod_value[3]-=str;

		for (n=0; n<4; n++) {
			if ((fn=ch[co].ef[n]) && ef[fn].type==EF_CURSE) {
				ef[fn].strength+=str;				
				break;				
			}
		}
		if (n==4) {
			create_spell_timer(co,in,fre);
		}
	}

        update_char(co);

        return 1;
}

static void freeze_someone(int cn,int co)
{
	int in,fre=-1,endtime,str,duration;

        if (!can_attack(cn,co)) return;
	if (!char_see_char(cn,co)) return;

	str=freeze_value(cn,co);
	//say(cn,"freeze of pwr %d giving str %d",ch[cn].value[0][V_FREEZE],str);
        if (str>=0) return;	// no speed ups!

	if (!(fre=may_add_spell(co,IDR_FREEZE))) return;

	duration=FREEZEDURATION;
	if (ch[cn].value[1][V_DURATION]) duration+=FREEZEDURATION*ch[cn].value[0][V_DURATION]/DURATION;
	else if (ch[cn].flags&CF_ARCH) duration+=FREEZEDURATION*ch[cn].level/DURATION/2;

	in=create_item("freeze_spell");
	if (!in) return;

        it[in].mod_value[0]=str;
	
	it[in].driver=IDR_FREEZE;

	endtime=ticker+duration;

	*(signed long*)(it[in].drdata)=endtime;
	*(signed long*)(it[in].drdata+4)=ticker;

	it[in].carried=co;

        ch[co].item[fre]=in;

        create_spell_timer(co,in,fre);

	update_char(co);

        if ((ch[cn].flags&CF_IDEMON) && ch[cn].value[1][V_DEMON]>ch[co].value[0][V_COLD] &&
	    ice_curse(co,ch[cn].value[1][V_DEMON]-ch[co].value[0][V_COLD],(ch[cn].value[1][V_DEMON]-ch[co].value[0][V_COLD])*50)) {
                log_char(co,LOG_SYSTEM,0,"You have been frozen by %s. You feel like you'll never thaw again.",ch[cn].name);
	} //else log_char(co,LOG_SYSTEM,0,"You have been frozen by %s.",ch[cn].name);

        return;
}

static int act_freeze(int cn)
{
	int co;
	int xs,xe,ys,ye,x,y,maxdist=3;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	xs=max(1,ch[cn].x-maxdist);
	xe=min(MAXMAP-2,ch[cn].x+maxdist);
	ys=max(1,ch[cn].y-maxdist);
	ye=min(MAXMAP-2,ch[cn].y+maxdist);

	for (y=ys; y<=ye; y++) {
		for (x=xs; x<=xe; x++) {
			if (!(co=map[x+y*MAXMAP].ch) || cn==co) continue;
			
                        freeze_someone(cn,co);
		}
	}

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_FREEZE,0);
	sound_area(ch[cn].x,ch[cn].y,31);

	return 1;
}

static void pulse_someone(int cn,int co)
{
	int str,has,fn,total;

        if (!can_attack(cn,co)) return;
	if (!char_see_char(cn,co)) return;

        //say(cn,"pulse of pwr %d giving str %.2f against %s (%d,%d,%d)",ch[cn].value[0][V_PULSE],(double)str/POWERSCALE,ch[co].name,ch[co].value[0][V_IMMUNITY],ch[co].value[0][V_TACTICS],ch[co].value[0][V_IMMUNITY]+ch[co].value[0][V_TACTICS]/5);

	str=pulse_damage(cn,co,ch[cn].act1);
	//say(cn,"hurts with %.2f (%d)",(double)str/POWERSCALE,ch[cn].act1);

	has=ch[co].hp+ch[co].lifeshield;
	total=ch[co].value[0][V_HP]*POWERSCALE+ch[co].value[0][V_MAGICSHIELD]*POWERSCALE+1;
	if (has*100/total>75) return;

	if (str<has) return;

	ch[cn].mana+=min(str,has);
	if (ch[cn].mana>ch[cn].value[0][V_MANA]*POWERSCALE) ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
	//say(cn,"got %.2f mana.",(double)min(str,has)/POWERSCALE);
	
        fn=create_show_effect(EF_PULSEBACK,co,ticker,ticker+7,20,str);
	if (fn) {
		ef[fn].x=ch[cn].x;
		ef[fn].y=ch[cn].y;
	}
	hurt(co,str,cn,1,0,100);

	if (ch[co].flags) {
                notify_char(co,NT_GOTHIT,cn,0,0);
		notify_area(ch[co].x,ch[co].y,NT_SEEHIT,cn,co,0);
	}

        return;
}

static int act_pulse(int cn)
{
	int co;
	int xs,xe,ys,ye,x,y,maxdist=2;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

	xs=max(1,ch[cn].x-maxdist);
	xe=min(MAXMAP-2,ch[cn].x+maxdist);
	ys=max(1,ch[cn].y-maxdist);
	ye=min(MAXMAP-2,ch[cn].y+maxdist);

	for (y=ys; y<=ye; y++) {
		for (x=xs; x<=xe; x++) {
			if (!(co=map[x+y*MAXMAP].ch) || cn==co) continue;
			
                        pulse_someone(cn,co);
		}
	}

	create_pulse(ch[cn].x,ch[cn].y,ch[cn].value[1][V_PULSE]);

	if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_PULSE,0);
	//sound_area(ch[cn].x,ch[cn].y,31);

	return 1;
}

static int act_heal(int cn,int flag)
{
	int co,str;

	if ((ch[cn].flags&CF_NOMAGIC) && !(ch[cn].flags&CF_NONOMAGIC)) return 0;

        co=ch[cn].act1;
	if (co<1 || co>=MAXCHARS || !char_see_char(cn,co)) return 0;

	if (!can_help(cn,co)) return 0;

	str=ch[cn].act2;
	if (str<1) return 0;

        ch[co].hp=min(ch[co].value[0][V_HP]*POWERSCALE,ch[co].hp+str);

	create_show_effect(EF_HEAL,co,ticker,ticker+8,0,0);

        if (!(ch[cn].flags&CF_NONOTIFY)) notify_area(ch[cn].x,ch[cn].y,NT_CHAR,cn,0,0);
	notify_area(ch[cn].x,ch[cn].y,NT_SPELL,cn,V_HEAL,0);

	if (flag) {
		ch[cn].action=AC_HEAL2;
		ch[cn].step=0;
	}

	return 1;
}

void check_container_item(int cn)
{
	int x,y,m,in,ct;

        if (ch[cn].action!=AC_IDLE && ch[cn].action!=AC_BLESS_SELF) {
		ch[cn].con_in=0;
                return;
	}

        if (!dx2offset(ch[cn].dir,&x,&y,NULL)) {
		elog("check_container_item(): dx2offset(%d,...) returned error for character %s (%d)",ch[cn].dir,ch[cn].name,cn);

		ch[cn].con_in=0;
		return;
	}

	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		elog("check_container_item(): character %s (%d) tried to use item from illegal position %d,%d.",ch[cn].name,cn,x,y);		
		
		ch[cn].con_in=0;
		return;
	}

	m=x+y*MAXMAP;

	if (!(in=map[m].it)) { ch[cn].con_in=0; return; }	// somebody else took it

	if (in<1 || in>=MAXITEM) {
		elog("check_container_item(): item number %d on map position %d,%d is out of bounds.",in,x,y);
		map[m].it=0;
		
		ch[cn].con_in=0;
		return;
	}

	if (!(it[in].flags&IF_USE)) { 	// item isn't useable		
		ch[cn].con_in=0;
		return;	
	}

	if (it[in].flags&IF_DEPOT) {	// may use his own depot...
		return;
	}

        if (!(ct=it[in].content)) {	// not a container
		ch[cn].con_in=0;
		return;
	}

	if (ct<1 || ct>=MAXCONTAINER) {
		elog("check_container_item(): container number %d is out of bounds",ct);
		it[in].content=0;
		ch[cn].con_in=0;
		return;
	}

	if (con[ct].owner && charID(cn)!=con[ct].owner && charID(cn)!=con[ct].killer && charID(cn)!=con[ct].access) {	// access denied
		ch[cn].con_in=0;
		return;
	}
}

void check_merchant(int cn)
{
	int co;

        if (ch[cn].action!=AC_IDLE && ch[cn].action!=AC_BLESS_SELF) {
		ch[cn].merchant=0;
                return;
	}

	co=ch[cn].merchant;
	if (co<1 || co>=MAXCHARS || !ch[co].flags) {
		ch[cn].merchant=0;
		return;
	}

	if (!ch[co].store) {
		ch[cn].merchant=0;
                return;
	}

	if (!char_see_char(cn,co)) {
		ch[cn].merchant=0;
                return;
	}
}

static void check_endurance(int cn)
{
        if (ch[cn].endurance<POWERSCALE && ch[cn].speed_mode==SM_FAST) {
		ch[cn].speed_mode=SM_NORMAL;
		log_char(cn,LOG_SYSTEM,0,"You're exhausted.");
	}
}

static int act(int cn)
{
	if (cn<1 || cn>=MAXCHARS) {
		elog("act(): character number %d is out of bounds",cn);
		return 0;
	}

	if (ch[cn].x<1 || ch[cn].x>MAXMAP-2 || ch[cn].y<1 || ch[cn].y>MAXMAP-2) {
		elog("act(): character %s (%d) has illegal position %d,%d.",ch[cn].name,cn,ch[cn].x,ch[cn].y);
		if (ch[cn].flags&CF_PLAYER) kick_char(cn);
                else die_char(cn,0,0);
		return 0;
	}

	switch(ch[cn].action) {
		case AC_IDLE:
		case AC_MAGICSHIELD:
                case AC_BLESS_SELF:
                case AC_HEAL_SELF:      break;
		default:		ch[cn].regen_ticker=ticker; break;
	}
        // note that some action are split into two parts for display reasons.
	// it looks a lot better if, for example, a fireball goes off when the
	// caster has his hand still pointing at the target.

        switch(ch[cn].action) {
		case AC_IDLE:		return act_idle(cn);
		case AC_WALK:		return act_walk(cn);
		case AC_TAKE:		return act_take(cn);
		case AC_DROP:		return act_drop(cn);
		case AC_ATTACK1:	return act_attack(cn);
		case AC_ATTACK2:	return act_attack(cn);
		case AC_ATTACK3:	return act_attack(cn);
		case AC_FIREBALL1:	return act_fireball(cn);
		case AC_FIREBALL2:	return act_ok(cn);
		case AC_MAGICSHIELD:	return act_magicshield(cn);		
		case AC_HEAL_SELF:	return act_heal(cn,0);
		case AC_HEAL1:		return act_heal(cn,1);
		case AC_HEAL2:		return act_ok(cn);
		case AC_BLESS_SELF:	return act_bless(cn,0);
		case AC_BLESS1:		return act_bless(cn,1);
		case AC_BLESS2:		return act_ok(cn);
		case AC_DIE:		return act_die(cn);
                case AC_USE:		return act_use(cn);
		case AC_FREEZE:		return act_freeze(cn);
                case AC_BALL1:		return act_ball(cn);
		case AC_BALL2:		return act_ok(cn);
		case AC_FLASH:		return act_flash(cn);
		case AC_GIVE:		return act_give(cn);
		case AC_WARCRY:		return act_warcry(cn);
		case AC_EARTHRAIN:	return act_earthrain(cn);
		case AC_EARTHMUD:	return act_earthmud(cn);
		case AC_PULSE:		return act_pulse(cn);
		case AC_FIRERING:	return act_firering(cn);

		default:	elog("act(): unknown action %d for character %s (%d)",ch[cn].action,ch[cn].name,cn);
				return 0;
	}	
}


void demon_protect_check(int cn)
{
	int m;

	if (!(ch[cn].flags&CF_PLAYER) || !ch[cn].player) return;

	m=ch[cn].x+ch[cn].y*MAXMAP;
	if (!(map[m].flags&MF_INDOORS) && ch[cn].value[0][V_DEMON] && sunlight>200) {
		ch[cn].value[0][V_DEMON]=0;
		log_char(cn,0,LOG_SYSTEM,"The full sunlight destroyed your demonic protection.");
	}
	ch[cn].flags|=CF_UPDATE;
}

void tile_special_check(int cn)
{
	int m;

        if (ch[cn].flags&CF_PLAYER) {
		
		m=ch[cn].x+ch[cn].y*MAXMAP;

		if (map[m].flags&MF_SLOWDEATH) {
			if (map[m].flags&MF_UNDERWATER) {
				if (!(ch[cn].flags&CF_OXYGEN)) hurt(cn,50,0,1,0,0);
                                else {
                                        int tmp=ticker+ch[cn].serial*32;

                                        if (!(tmp%6) && !((tmp/TICKS)%3)) {
                                                create_map_effect(EF_BUBBLE,ch[cn].x,ch[cn].y,ticker,ticker+1,0,45);
                                                if (!(tmp%12)) sound_area(ch[cn].x,ch[cn].y,44+RANDOM(3));
                                        }
                                }
			} else {
				if ((map[m].gsprite&0xffff)>=59706 && (map[m].gsprite&0xffff)<=59709) hurt(cn,250,0,1,25,66);
				else hurt(cn,100,0,1,25,66);
			}
		}
	}
}

void stealth_special(int cn)
{
	if (!ch[cn].prof[P_THIEF]) return;
	if (ch[cn].speed_mode!=SM_STEALTH) return;
	if (!(ch[cn].flags&CF_THIEFMODE)) return;

	ch[cn].regen_ticker=ticker;
	
	switch(ch[cn].action) {
		case AC_IDLE:
		case AC_USE:
		case AC_TAKE:
		case AC_BLESS_SELF:
		case AC_DROP:
		case AC_WALK:		if ((ticker&7)==7) { ch[cn].endurance-=POWERSCALE/5; ch[cn].flags|=CF_SMALLUPDATE; } break;
		default:		{ ch[cn].endurance=0; ch[cn].flags|=CF_SMALLUPDATE; } break;
	}
	
	if (ch[cn].endurance<POWERSCALE/2) {
		ch[cn].endurance=POWERSCALE/2;
		//ch[cn].speed_mode=SM_NORMAL;
		ch[cn].flags&=~CF_THIEFMODE;
		ch[cn].flags|=CF_SMALLUPDATE;
		update_char(cn);
		log_char(cn,LOG_SYSTEM,0,"Your concentration was broken. You can no longer hide.");
	}
}

void regenerate(int cn)
{
	int diff,m;

	if (ch[cn].x<1 || ch[cn].x>MAXMAP-2 || ch[cn].y<1 || ch[cn].y>MAXMAP-2) return;
	
	m=ch[cn].x+ch[cn].y*MAXMAP;

        diff=(ticker-ch[cn].last_regen)/TICKS;
	
	if (diff>0 && (!(map[m].flags&MF_NOREGEN) || !(ch[cn].flags&CF_PLAYER)) ) {
		if (ch[cn].speed_mode!=SM_FAST && (ch[cn].speed_mode!=SM_STEALTH || !(ch[cn].flags&CF_THIEFMODE))) {
                        if (ch[cn].value[1][V_REGENERATE] && ch[cn].endurance<ch[cn].value[0][V_ENDURANCE]*POWERSCALE) {
				ch[cn].endurance=min(ch[cn].value[0][V_ENDURANCE]*POWERSCALE,ch[cn].endurance+(ch[cn].value[0][V_REGENERATE]+ch[cn].value[1][V_REGENERATE])*diff*5);
				ch[cn].flags|=CF_SMALLUPDATE;
			}
			if (ch[cn].value[1][V_MAGICSHIELD] && ch[cn].value[1][V_MEDITATE] && ch[cn].lifeshield<ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE && areaID!=33) {
				ch[cn].lifeshield=min(ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE,ch[cn].lifeshield+(ch[cn].value[0][V_MEDITATE]+ch[cn].value[1][V_MEDITATE])*diff*4);
				ch[cn].flags|=CF_SMALLUPDATE;
			}
		}
		if (areaID==33) ch[cn].lifeshield=0;
                ch[cn].last_regen+=diff*TICKS;
	}

	if (ch[cn].lifeshield<0) {
		xlog("character %d (%s) lifeshield=%d (%X). fixed.",cn,ch[cn].name,ch[cn].lifeshield,ch[cn].lifeshield);
		ch[cn].lifeshield=0;
	}
}

void payment_check(int cn)
{
        if (ch[cn].paid_till && ch[cn].paid_till<time_now) {
		dlog(cn,0,"Kicked because payment ran out.");
		if (ch[cn].player) kick_player(ch[cn].player,"Your payment ran out.");
		else kick_char(cn);		
	}
}

// gets called once per tick
// if act sets step to zero, the action will not get reset
// and the driver wont be called. this allows act-functions
// to force the execution of another action later.
void tick_char(void)
{
	int n,ret,lastact,next,lastdur;
	unsigned long long prof;

	for (n=getfirst_char(); n; n=next) {

		next=getnext_char(n);	// we need to get next here since act might free (kill) the current char
		
		if (!ch[n].flags) { xlog("tick_char: got unused char %d (%s) from getfirst/getnext (%d)!!",n,ch[n].name,next); break; }

		if (ch[n].flags&CF_PLAYER) {
			sanity_check(n);
			tile_special_check(n);
			if (!ch[n].flags) continue; // it killed him...
                        payment_check(n);
			if (!ch[n].flags) continue; // it killed him...
		}
		stealth_special(n);
		if ((ticker&0x2ff)==(n&0xff)) update_char(n);

		ch[n].step++;		
		if (ch[n].duration>ch[n].step) continue;

                lastact=ch[n].action;
		lastdur=ch[n].duration;
                prof=prof_start(23); ret=act(n); prof_stop(23,prof);

		demon_protect_check(n);

                if (ch[n].step && ch[n].flags) {
			ch[n].duration=ch[n].step=ch[n].action=0;
			prof=prof_start(19); char_driver(ch[n].driver,CDT_DRIVER,n,ret,lastact); prof_stop(19,prof);			
		}

		if (ch[n].flags) {	// char_driver or act might have removed the character
			if (ch[n].con_in) check_container_item(n);
			if (ch[n].merchant) check_merchant(n);
			check_endurance(n);
			regenerate(n);
		}

		if (lastact!=AC_IDLE || ch[n].action!=AC_IDLE || !lastdur || (ch[n].flags&CF_SMALLUPDATE)) {
			set_sector(ch[n].x,ch[n].y);
			ch[n].flags&=~CF_SMALLUPDATE;
			//if (ch[n].flags&CF_PLAYER) xlog("up");
		} //else if (ch[n].flags&CF_PLAYER) xlog("no");
	}
}







