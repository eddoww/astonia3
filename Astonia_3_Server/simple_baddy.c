/*

$Id: simple_baddy.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: simple_baddy.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:46  sam
Added RCS tags


*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "log.h"
#include "notify.h"
#include "direction.h"
#include "do.h"
#include "path.h"
#include "error.h"
#include "drdata.h"
#include "see.h"
#include "death.h"
#include "talk.h"
#include "effect.h"
#include "database.h"
#include "map.h"
#include "create.h"
#include "container.h"
#include "drvlib.h"
#include "tool.h"
#include "spell.h"
#include "effect.h"
#include "light.h"
#include "los.h"
#include "date.h"
#include "libload.h"
#include "poison.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);			// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies
int ch_respawn_driver(int nr,int cn);					// called when an NPC is about to respawn

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case CDT_DRIVER:	return ch_driver(nr,obj,ret,lastact);
		case CDT_ITEM: 		return it_driver(nr,obj,ret);
		case CDT_DEAD:		return ch_died_driver(nr,obj,ret);
		case CDT_RESPAWN:	return ch_respawn_driver(nr,obj);
		default: 	return 0;
	}
}

struct simple_baddy_driver_data
{
	int startdist;
	int chardist;
	int stopdist;

	int aggressive;
	int helper;
	int scavenger;

        int dir;

	int dayx;
	int dayy;
	int daydir;

	int nightx;
	int nighty;
	int nightdir;

	int teleport;

	int helpid;

	int creation_time;	

	int notsecure;
	int mindist;

	int lastfight;

	int poison_power;
	int poison_chance;
	int poison_type;
	int drinkspecial;
};

void simple_baddy_driver_parse(int cn,struct simple_baddy_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"aggressive")) dat->aggressive=atoi(value);
		else if (!strcmp(name,"scavenger")) dat->scavenger=atoi(value);
		else if (!strcmp(name,"helper")) dat->helper=atoi(value);
		else if (!strcmp(name,"startdist")) dat->startdist=atoi(value);
		else if (!strcmp(name,"chardist")) dat->chardist=atoi(value);
		else if (!strcmp(name,"stopdist")) dat->stopdist=atoi(value);
                else if (!strcmp(name,"dir")) dat->dir=atoi(value);
		else if (!strcmp(name,"dayx")) dat->dayx=atoi(value);
		else if (!strcmp(name,"dayy")) dat->dayy=atoi(value);
		else if (!strcmp(name,"daydir")) dat->daydir=atoi(value);
		else if (!strcmp(name,"nightx")) dat->nightx=atoi(value);
		else if (!strcmp(name,"nighty")) dat->nighty=atoi(value);
		else if (!strcmp(name,"nightdir")) dat->nightdir=atoi(value);
		else if (!strcmp(name,"teleport")) dat->teleport=atoi(value);
		else if (!strcmp(name,"helpid")) dat->helpid=atoi(value);
		else if (!strcmp(name,"notsecure")) dat->notsecure=atoi(value);
		else if (!strcmp(name,"mindist")) dat->mindist=atoi(value);
		else if (!strcmp(name,"poisonpower")) dat->poison_power=atoi(value);
		else if (!strcmp(name,"poisontype")) dat->poison_type=atoi(value);
		else if (!strcmp(name,"poisonchance")) dat->poison_chance=atoi(value);
		else if (!strcmp(name,"drinkspecial")) dat->drinkspecial=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void simple_baddy_driver(int cn,int ret,int lastact)
{
        struct simple_baddy_driver_data *dat;
	struct msg *msg,*next;
        int co,friend=0,x,y,n,in;

	dat=set_data(cn,DRD_SIMPLEBADDYDRIVER,sizeof(struct simple_baddy_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch(msg->type) {
			case NT_CREATE:
				if (ch[cn].arg) {
					dat->aggressive=0;
					dat->helper=0;
					dat->startdist=20;
					dat->chardist=0;
					dat->stopdist=40;
					dat->scavenger=0;
					dat->dir=DX_DOWN;
	
					simple_baddy_driver_parse(cn,dat);
					ch[cn].arg=NULL;
				}
				dat->creation_time=ticker;
				fight_driver_set_dist(cn,dat->startdist,dat->chardist,dat->stopdist);
				
				if (ch[cn].item[30] && (ch[cn].flags&CF_NOBODY)) {
					ch[cn].flags&=~(CF_NOBODY);
					ch[cn].flags|=CF_ITEMDEATH;
					//xlog("transformed item %s",it[ch[cn].item[30]].name);
				}
				break;

			case NT_CHAR:
				co=msg->dat1;
                                if (dat->helper &&
				    ch[co].group==ch[cn].group &&
				    cn!=co &&
				    ch[cn].value[0][V_BLESS] &&
				    ch[cn].mana>=BLESSCOST &&
				    may_add_spell(co,IDR_BLESS) &&
				    char_see_char(cn,co))
					friend=co;
                                break;

			case NT_TEXT:
				co=msg->dat3;
				tabunga(cn,co,(char*)msg->dat2);
				break;
			
			case NT_DIDHIT:
                                if (dat->poison_power && msg->dat1 && msg->dat2>0 && can_attack(cn,msg->dat1) && RANDOM(100)<dat->poison_chance)
					poison_someone(msg->dat1,dat->poison_power,dat->poison_type);
				break;

			case NT_NPC:
				if (dat->helpid && msg->dat1==dat->helpid && (co=msg->dat2)!=cn && ch[co].group==ch[cn].group)
					fight_driver_add_enemy(cn,msg->dat3,1,0);
				break;
		}
		

                standard_message_driver(cn,msg,dat->aggressive,dat->helper);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) {
		if (ticker-dat->lastfight>TICKS*10) {
			sound_area(ch[cn].x,ch[cn].y,1);			
		}
		dat->lastfight=ticker;
		return;
	}
        if (fight_driver_follow_invisible(cn)) { dat->lastfight=ticker; return; }

	// look around for a second if we've just been created
	if (ticker-dat->creation_time<TICKS) {
                do_idle(cn,TICKS/4);
		return;
	}

        if (dat->scavenger) {
                if (abs(ch[cn].x-ch[cn].tmpx)>=dat->scavenger || abs(ch[cn].y-ch[cn].tmpy)>=dat->scavenger) {
			dat->dir=0;
			if (dat->notsecure) {
				if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
			} else {
				if (ticker-dat->lastfight>TICKS*10) {
					if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
				} else {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;					
				}
			}
		}

		if (regenerate_driver(cn)) return;

		if (spell_self_driver(cn)) return;

		// help friend by blessing him. all checks already done in message loop
		if (friend && do_bless(cn,friend)) return;

                // dont walk all the time
		if (!RANDOM(2)) {
			do_idle(cn,TICKS);
			return;
		}
	
		if (!dat->dir) dat->dir=RANDOM(8)+1;
	
		dx2offset(dat->dir,&x,&y,NULL);
		
		if (abs(ch[cn].x+x-ch[cn].tmpx)<dat->scavenger && abs(ch[cn].y+y-ch[cn].tmpy)<dat->scavenger && do_walk(cn,dat->dir)) {
			fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
			return;
		} else dat->dir=0;		
	} else {
		if (dat->dayx) {
                        if (hour>19 || hour<6) {
                                if (dat->teleport && teleport_char_driver(cn,dat->nightx,dat->nighty)) {
					fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
					return;
				}
				if (dat->notsecure) {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
				} else {
					if (ticker-dat->lastfight>TICKS*10) {
						if (secure_move_driver(cn,dat->nightx,dat->nighty,dat->nightdir,ret,lastact)) return;
					} else {
						if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;						
					}
				}
                                fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
			} else {
                                if (dat->teleport && teleport_char_driver(cn,dat->dayx,dat->dayy)) return;
				if (dat->notsecure) {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
				} else {
					if (ticker-dat->lastfight>TICKS*10) {
						if (secure_move_driver(cn,dat->dayx,dat->dayy,dat->daydir,ret,lastact)) return;
					} else {
						if (move_driver(cn,dat->dayx,dat->dayy,0)) return;						
					}
				}
                                fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
			}
		} else {
                        if (dat->teleport && teleport_char_driver(cn,ch[cn].tmpx,ch[cn].tmpy)) return;
			
			if (dat->notsecure) {
				if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
				//say(cn,"move failed, %d, target=%d,%d",pathnodes(),ch[cn].tmpx,ch[cn].tmpy);
			} else {
				if (ticker-dat->lastfight>TICKS*10) {
					if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;
				} else {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;					
				}
			}
                        fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
		}		
	}

	if (dat->drinkspecial) {
		for (n=11; n<30; n++) {
			if ((in=ch[cn].item[n]) && it[in].driver==IDR_POISON0) {
				emote(cn,"drinks a potion");
				remove_all_poison(cn);
				break;
			}
		}
	}


	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

	// help friend by blessing him. all checks already done in message loop
	if (friend && do_bless(cn,friend)) return;
	
        //say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}

void simple_baddy_dead(int cn,int co)
{
        if ((ch[cn].flags&CF_EDEMON) && char_see_char(cn,co)) {
		if (ch[cn].value[1][V_DEMON]>5) create_earthmud(cn,ch[co].x,ch[co].y,ch[cn].value[1][V_DEMON]);
		create_earthrain(cn,ch[co].x,ch[co].y,ch[cn].value[1][V_DEMON]);
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_SIMPLEBADDY:	simple_baddy_driver(cn,ret,lastact); return 1;
                default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_SIMPLEBADDY:	simple_baddy_dead(cn,co); return 1;
                default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_SIMPLEBADDY:	return 1;
                default:		return 0;
	}
}
