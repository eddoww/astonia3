/*
 * $Id: lostcon.c,v 1.3 2007/08/13 18:50:38 devel Exp $
 *
 * $Log: lostcon.c,v $
 * Revision 1.3  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.2  2006/06/20 13:27:40  ssim
 * made area 34 exempt from immediate logout when in arena
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
#include "tool.h"
#include "drvlib.h"
#include "player.h"
#include "lostcon.h"
#include "spell.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn);					// called when a character dies

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case 0:		return ch_driver(nr,obj,ret,lastact);
		case 1: 	return it_driver(nr,obj,ret);
		case 2:		return ch_died_driver(nr,obj);
		default: 	return 0;
	}
}

//-----------------------

struct lostcon_driver_data
{
	int timeout;
	//int last_say;
};

void lostcon_driver(int cn,int ret,int lastact)
{
	struct lostcon_driver_data *dat;
	struct lostcon_ppd *ppd;
	int co,n,in,fn,m;
	struct msg *msg,*next;

	dat=set_data(cn,DRD_LOSTCONDRIVER,sizeof(struct lostcon_driver_data));
	if (!dat) return;	// oops...

	ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd));
	if (!ppd) return;	// oops...

	if (!dat->timeout) {
                dat->timeout=ticker+LAGOUT_TIME;
                fight_driver_reset(cn);	
	}

	
	
	if (ch[cn].x>1 && ch[cn].x<MAXMAP && ch[cn].y>1 && ch[cn].y<MAXMAP) {
		m=ch[cn].x+ch[cn].y*MAXMAP;
		if (!ch[cn].player && (map[m].flags&MF_RESTAREA)) {	// leave at once if at rest area
			exit_char(cn);
			return;
		}
		if (!ch[cn].player && (map[m].flags&MF_ARENA) && areaID!=34) {	// leave at once if disconnected in an arena
			exit_char(cn);
			return;
		}
		if (ticker-10*TICKS+LAGOUT_TIME>dat->timeout && (map[m].flags&MF_ARENA) && areaID!=34) {	// leave after 10s if lagging in an arena
			if (ch[cn].player) kick_player(ch[cn].player,"Lagging in Arena");
                        exit_char(cn);
			return;
		}
	}
	// leave at once if karma is -12 or worse
	if (ch[cn].karma<=-12 || (!(ch[cn].flags&CF_PAID) && ch[cn].karma<=-5)) {
		exit_char(cn);
		return;
	}

	// leave if timeout is reached
	if (!ch[cn].player && ticker>dat->timeout) {
		exit_char(cn);
		return;
	}
	
	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
		
		// did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			//if (co && can_attack(cn,co)) fight_driver_add_enemy(cn,co,1,1);	// !!!!!!!!!!
		}

		// did we get hit?
		if (msg->type==NT_GOTHIT) {

			fight_driver_note_hit(cn);
			
			co=msg->dat1;
			if (co) fight_driver_add_enemy(cn,co,1,1);
		}

		// did we hear something?
		if (msg->type==NT_TEXT) {
			if (msg->dat1==1 && msg->dat3!=cn) {	// someone (not us) is talking
				//xlog("got (%s)",(char*)msg->dat2);
			}			
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	for (n=0; n<4; n++) {
		if (!(fn=ch[cn].ef[n])) continue;
		if (ef[fn].type==EF_LAG) break;
	}
	if (n==4) create_show_effect(EF_LAG,cn,ticker,ticker+TICKS,0,1);
	else ef[fn].stop=ticker+TICKS;

	// low on hp?
	if (ch[cn].hp<ch[cn].value[1][V_HP]*(int)(POWERSCALE*0.75)) {

		// try heal
		if (!ppd->noheal && ch[cn].mana>ch[cn].value[1][V_MANA]*POWERSCALE/2 && do_heal(cn,cn)) return;		
	}

	// low on mana?
	if (ch[cn].mana<ch[cn].value[1][V_MANA]*POWERSCALE/4) {

		// use potion
		if (!ppd->nolife || !ppd->nocombo) {
			for (n=30; n<INVENTORYSIZE; n++) {
				if ((in=ch[cn].item[n]) && it[in].driver==IDR_POTION && it[in].drdata[2]) {
					if (it[in].drdata[1] && !ppd->nocombo) { use_item(cn,in); break; }
					if (!it[in].drdata[1] && !ppd->nomana) { use_item(cn,in); break; }
				}
			}
		}
	}

	// low on magic shield?
	if (ch[cn].lifeshield<ch[cn].value[1][V_MAGICSHIELD]*POWERSCALE/4) {

		// try respell
		if (!ppd->noshield && ch[cn].mana>ch[cn].value[1][V_MANA]*POWERSCALE/2 && do_magicshield(cn)) return;
	}

	fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,ppd->nomove)) return;
        if (!ppd->nomove && fight_driver_follow_invisible(cn)) return;

	if (ch[cn].value[0][V_BLESS] && ch[cn].mana>=BLESSCOST && !ppd->nobless && may_add_spell(cn,IDR_BLESS) && do_bless(cn,cn)) return;
        if (ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE>ch[cn].lifeshield && ch[cn].mana>=POWERSCALE*3 && !ppd->noshield && do_magicshield(cn)) return;
	if (ch[cn].value[0][V_HEAL] && ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 && ch[cn].mana>=POWERSCALE*3 && !ppd->noheal && do_heal(cn,cn)) return;

	do_idle(cn,TICKS);
}

void lostcon_dead(int cn)
{
	struct lostcon_driver_data *dat;

	dat=set_data(cn,DRD_LOSTCONDRIVER,sizeof(struct lostcon_driver_data));
	if (!dat) return;	// oops...

        dat->timeout=0;

	//charlog(cn,"reset lostcon timer");
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_LOSTCON:	lostcon_driver(cn,ret,lastact); return 1;		
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_LOSTCON:	lostcon_dead(cn); return 1;
		default:		return 0;
	}
}





