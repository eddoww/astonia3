/*
 * $Id: sewers.c,v 1.4 2007/06/12 11:34:15 devel Exp $
 *
 * $Log: sewers.c,v $
 * Revision 1.4  2007/06/12 11:34:15  devel
 * re-added sewer items
 *
 * Revision 1.3  2007/02/28 09:43:39  devel
 * removed item creation, needs a substitute
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

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
#include "date.h"
#include "los.h"
#include "skill.h"
#include "item_id.h"
#include "libload.h"
#include "player_driver.h"
#include "task.h"
#include "poison.h"
#include "sector.h"
#include "lab.h"
#include "player.h"
#include "area.h"
#include "misc_ppd.h"
#include "consistency.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);			// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
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
		default: 		return 0;
	}
}

static int init_done=0;
static int chest_cnt[4]={0,0,0,0};
static int chest[4][300];

void count_chests(void)
{
	int in,nr;

	for (in=1; in<MAXITEM; in++) {
		if (!it[in].flags) continue;
		if (it[in].driver!=IDR_RATCHEST) continue;
		switch(it[in].drdata[0]) {
			case 10:	nr=0; break;
			case 20:	nr=1; break;
			case 30:	nr=2; break;
			case 40:	nr=3; break;
			default:	elog("unknown chest number %d in count_chests() in sewers.c",it[in].drdata[0]); continue;
		}
		chest[nr][chest_cnt[nr]]=in;
		chest_cnt[nr]++;
	}
	xlog("sewers: %d/%d/%d/%d crates.",chest_cnt[0],chest_cnt[1],chest_cnt[2],chest_cnt[3]);
}

#define MAXRATCHEST	100

struct ratchest_ppd
{
	int ID[MAXRATCHEST];
	int last_used[MAXRATCHEST];

	int treasure_x,treasure_y;	// treasure is hidden at pos x,y
	int last_treasure;		// last treasure was found at (realtime)
};

void set_treasure(int cn,struct ratchest_ppd *ppd)
{
	int nr,in,idx;

	if (!init_done) { count_chests(); init_done=1; }

	if (!(ch[cn].flags&CF_GOD) && realtime-ppd->last_treasure<60*60*24) return;	// max one per day
	if (ppd->treasure_x) return;				// treasure is already set

	if (ch[cn].level>=37) nr=3;		//33,35,37,39
	else if (ch[cn].level>=29) nr=2;	//25,27,29,31
	else if (ch[cn].level>=21) nr=1;	//17,19,21,23
        else if (ch[cn].level>=13) nr=0;	// 9,11,13,15
	else return;	// level too low

	idx=RANDOM(chest_cnt[nr]);
	in=chest[nr][idx];

	ppd->treasure_x=it[in].x;
	ppd->treasure_y=it[in].y;
}

void give_treasure(int cn)
{
	char *name;
        int skl,val,in;

        switch(RANDOM(3)) {
                case 0: name="sewer_ring";
                        if ((ch[cn].flags&(CF_MAGE|CF_WARRIOR))==CF_MAGE) skl=V_MAGICSHIELD;
                        else skl=V_PARRY;
                        break;

                case 1: name="sewer_ring";
                        if ((ch[cn].flags&(CF_MAGE|CF_WARRIOR))==CF_MAGE) {
                                if (ch[cn].value[1][V_FIRE]>ch[cn].value[1][V_FLASH]) skl=V_FIRE;
                                else skl=V_FLASH;
                        } else skl=V_ATTACK;
                        break;

                case 2: name="sewer_amulet";
                        skl=V_IMMUNITY;
                        break;
                default: elog("oops in give_treasure() in sewers.c");
                        return;
        }

        if (ch[cn].level<15) val=4;
        else if (ch[cn].level<17) val=5;
        else if (ch[cn].level<20) val=6;
        else if (ch[cn].level<23) val=7;
        else if (ch[cn].level<26) val=8;
        else if (ch[cn].level<30) val=9;
        else if (ch[cn].level<33) val=10;
        else if (ch[cn].level<36) val=11;
        else val=12;

        if (!(ch[cn].flags&CF_ARCH)) val=min(val,9);

        in=create_item(name);
        if (!in) {
                elog("give_treasure() in sewers.c: could not create %s",name);
                return;
                        }

	it[in].flags|=IF_NOENHANCE;
        it[in].mod_index[0]=skl;
        it[in].mod_value[0]=val;
        it[in].value=50*100;
        set_item_requirements(in);
	calculate_complexity(in);

        give_char_item(cn,in);
        log_char(cn,LOG_SYSTEM,0,"You found a %s.",lower_case(it[in].name));
}

void ratchest_driver(int in,int cn)
{
	int ID,n,old_n=0,old_val=0,in2=0,amount;
	struct ratchest_ppd *ppd;

	if (!init_done) { count_chests(); init_done=1; }

	if (!cn) return;
	
	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	ppd=set_data(cn,DRD_RATCHEST_PPD,sizeof(struct ratchest_ppd));
	if (!ppd) return;	// oops...

	set_treasure(cn,ppd);
	if (ch[cn].flags&CF_GOD) log_char(cn,LOG_SYSTEM,0,"x=%d, y=%d",ppd->treasure_x,ppd->treasure_y);

	if (it[in].x==ppd->treasure_x && it[in].y==ppd->treasure_y) {
		ppd->treasure_x=ppd->treasure_y=0;
		give_treasure(cn);
		ppd->last_treasure=realtime;
		return;
	}

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXRATCHEST; n++) {
		if (ppd->ID[n]==ID) break;
                if (realtime-ppd->last_used[n]>old_val) {
			old_val=realtime-ppd->last_used[n];
			old_n=n;
		}
	}

        if (n==MAXRATCHEST) n=old_n;
	else if (realtime-ppd->last_used[n]<60*60*24) {
		log_char(cn,LOG_SYSTEM,0,"You didn't find anything.");
		return;
	}

	ppd->ID[n]=ID;
	ppd->last_used[n]=realtime;

        if (RANDOM(4)) {
		log_char(cn,LOG_SYSTEM,0,"You didn't find anything.");
		return;	
	}
	
	amount=(RANDOM(it[in].drdata[0])+1)*(RANDOM(it[in].drdata[0])+1);
	in2=create_money_item(amount);
	if (!in2) {	// should never happen
		log_char(cn,LOG_SYSTEM,0,"You didn't find anything.");
		return;
	}
	log_char(cn,LOG_SYSTEM,0,"You found some money (%.2fG)!",amount/100.0);

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from ratchest");
        ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;
}

void ratling_died(int cn,int co)
{
	struct ratchest_ppd *ppd;

	if (!co) return;
        if (!(ch[co].flags&CF_PLAYER)) return;
	
	ppd=set_data(co,DRD_RATCHEST_PPD,sizeof(struct ratchest_ppd));
	if (!ppd) return;	// oops...

	set_treasure(co,ppd);

	//xlog("%d,%d",ppd->treasure_x,ppd->treasure_y);

	if (ppd->treasure_x && RANDOM(2)) {
		if (abs(ch[co].x-ppd->treasure_x)>abs(ch[co].y-ppd->treasure_y)) {
			if (ppd->treasure_x>ch[co].x) say(cn,"Arrgh. Not kill us. Is south-east. Go there. Take from grate. And leave.");
			if (ppd->treasure_x<ch[co].x) say(cn,"Arrgh. Not kill us. Is north-west. Go there. Take from grate. And leave.");
		} else {
			if (ppd->treasure_y>ch[co].y) say(cn,"Arrgh. Not kill us. Is south-west. Go there. Take from grate. And leave.");
			if (ppd->treasure_y<ch[co].y) say(cn,"Arrgh. Not kill us. Is north-east. Go there. Take from grate. And leave.");
		}
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_RATLING:	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_RATCHEST:	ratchest_driver(in,cn); return 1;		

		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_RATLING:      	ratling_died(cn,co); return 1;
                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_RATLING:	return 1;
		default:	return 0;
	}
}













