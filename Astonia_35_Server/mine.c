/*
 * $Id: mine.c,v 1.6 2007/11/04 12:59:35 devel Exp $
 *
 * $Log: mine.c,v $
 * Revision 1.6  2007/11/04 12:59:35  devel
 * clan smith bonus
 *
 * Revision 1.5  2007/07/04 09:22:57  devel
 * added /support support
 *
 * Revision 1.4  2007/06/11 10:05:40  devel
 * items with IF_NOENHANCE set will no longer adjust their value
 *
 * Revision 1.3  2007/05/03 14:40:03  devel
 * made metal not usable on stones
 *
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
#include "act.h"
#include "item_id.h"
#include "sector.h"
#include "player_driver.h"
#include "military.h"
#include "consistency.h"
#include "prof.h"
#include "clan.h"

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

void check_military_silver(int cn,int amount)
{
	struct military_ppd *ppd;
	int nr;

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(ppd=set_data(cn,DRD_MILITARY_PPD,sizeof(struct military_ppd)))) return;

	if (ppd->took_mission && !ppd->solved_mission) {
		nr=ppd->took_mission-1;

                switch(ppd->mis[nr].type) {
			case 3:         if (amount<ppd->mis[nr].opt1) {
						ppd->mis[nr].opt1-=amount;
						log_char(cn,LOG_SYSTEM,0,"You fulfilled part of your mission, you still need %d silver.",ppd->mis[nr].opt1);
					} else {
                                                log_char(cn,LOG_SYSTEM,0,"You solved your mission. Talk to the governor to claim you reward.");
						ppd->solved_mission=1;
						ppd->mis[nr].opt1=0;
					}
					break;
		}
	}
}

void minewall(int in,int cn)
{
	int in2,amount,co;
	char buf[80];

        if (!cn) {
		if (!it[in].drdata[4]) {
                        it[in].drdata[4]=1;
			switch((it[in].x+it[in].y)%3) {
				case 0:		it[in].sprite=15070; break;
				case 1:		it[in].sprite=15078; break;
				case 2:		it[in].sprite=15086; break;
			}
			
		}

                if (it[in].drdata[3]==8) {
			if ((map[it[in].x+it[in].y*MAXMAP].flags&MF_TMOVEBLOCK) || map[it[in].x+it[in].y*MAXMAP].it) {
				call_item(it[in].driver,in,0,ticker+TICKS);
				return;
			}
			it[in].sprite-=8;
			it[in].drdata[3]=0;
			it[in].flags|=IF_USE;
			it[in].flags&=~IF_VOID;
			
			remove_lights(it[in].x,it[in].y);
			map[it[in].x+it[in].y*MAXMAP].it=in;
			map[it[in].x+it[in].y*MAXMAP].flags|=MF_TSIGHTBLOCK|MF_TMOVEBLOCK;
			it[in].flags|=IF_SIGHTBLOCK;
			reset_los(it[in].x,it[in].y);
			add_lights(it[in].x,it[in].y);
                        set_sector(it[in].x,it[in].y);
		}
		return;
	}

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		player_driver_dig_off(cn);
		return;
	}
	
	
	if (it[in].drdata[3]<9) {
		if (ch[cn].endurance<POWERSCALE) {
			log_char(cn,LOG_SYSTEM,0,"You're too exhausted to continue digging.");
			player_driver_dig_off(cn);
			return;
		}
		ch[cn].endurance-=POWERSCALE/4-(ch[cn].prof[P_MINER]*POWERSCALE/(4*25));

                it[in].drdata[3]++;
		it[in].drdata[5]=0;
		it[in].sprite++;
		
		if (it[in].drdata[3]==8) {
                        map[it[in].x+it[in].y*MAXMAP].it=0;
			map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TMOVEBLOCK;
			it[in].flags&=~IF_USE;
			it[in].flags|=IF_VOID;
			call_item(it[in].driver,in,0,ticker+TICKS*60*5+RANDOM(TICKS*60*25));

			if (!RANDOM(15)) {
				in2=create_item("silver");
				if (!in2) elog("silver not found");

				amount=RANDOM(it[in].drdata[0]*2+1)+it[in].drdata[0];
				//xlog("amount=%d",amount);
				if (ch[cn].prof[P_MINER]) amount+=amount*ch[cn].prof[P_MINER]/10;
				//xlog("amount=%d",amount);
				if (!amount && in2) {
					destroy_item(in2);
				}
			} else if (!RANDOM(50)) {
				in2=create_item("gold");
				if (!in2) elog("gold not found");

				amount=RANDOM(it[in].drdata[1]*2+1)+it[in].drdata[1];
				if (ch[cn].prof[P_MINER]) amount+=amount*ch[cn].prof[P_MINER]/10;
				if (!amount && in2) {
					destroy_item(in2);
				}
			} else amount=in2=0;

			if (amount && in2) {
				it[in2].value*=amount;
				*(unsigned int*)(it[in2].drdata+1)=amount;
				sprintf(it[in2].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in2].drdata+1)),it[in2].name);

				if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from minewall");
				ch[cn].citem=in2;
				it[in2].carried=cn;
				ch[cn].flags|=CF_ITEMS;

				log_char(cn,LOG_SYSTEM,0,"You found %d units of %s.",amount,it[in2].name);

				if (it[in2].drdata[0]==1) check_military_silver(cn,amount);
			}

			if (!RANDOM(10)) {
				sprintf(buf,"miner%d",it[in].drdata[2]);
				co=create_char(buf,0);
				if (!co) elog("%s not found",buf);

				if (co) {
					if (drop_char(co,it[in].x,it[in].y,0)) {
						ch[co].tmpx=ch[co].x;
						ch[co].tmpy=ch[co].y;
						
						update_char(co);
						
						ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
						ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
						ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
						
						ch[co].dir=DX_RIGHTDOWN;

						if (!ch[co].item[30]) {
							if (strstr(ch[co].name,"Gold")) in2=create_item("gold");
							else in2=create_item("silver");
							if (in2) {
								amount=RANDOM(ch[co].level)+1;
								it[in2].value*=amount;
								*(unsigned int*)(it[in2].drdata+1)=amount;
								sprintf(it[in2].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in2].drdata+1)),it[in2].name);
								ch[co].item[30]=in2;
								it[in2].carried=co;
							}
						}
					} else {
						destroy_char(co);
					}
				}
			}
		}
		
		set_sector(it[in].x,it[in].y);

		if (it[in].drdata[3]==3) {
			remove_lights(it[in].x,it[in].y);
			map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TSIGHTBLOCK;
			it[in].flags&=~IF_SIGHTBLOCK;
			reset_los(it[in].x,it[in].y);
			add_lights(it[in].x,it[in].y);
		}
	}
	
	if (it[in].drdata[3]<8) player_driver_dig_on(cn);
	else player_driver_dig_off(cn);
}

void collect_item(int in,int cn)
{
	int in2,base;

	if (!cn) return;
	if (!it[in].carried) return;

	if (!(in2=ch[cn].citem)) {
		int a,b;

		a=*(unsigned int*)(it[in].drdata+1);
		if (a<2) {
			log_char(cn,LOG_SYSTEM,0,"You cannot split 1 unit.");
			return;
		}
		base=it[in].value/a;

		b=a/2;
		if (b>=100000) b=100000;
		else if (b>=10000) b=10000;
		else if (b>=5000) b=5000;
		else if (b>=2500) b=2500;
		else if (b>=1000) b=1000;
		else if (b>=500) b=500;
		else if (b>=250) b=250;
		else if (b>=100) b=100;
		else if (b>=50) b=50;
		else if (b>=25) b=25;
		else if (b>=10) b=10;
		a=a-b;

                if (it[in].drdata[0]==1) in2=create_item("silver");
		if (it[in].drdata[0]==2) in2=create_item("gold");
                if (!in2) {
			log_char(cn,LOG_SYSTEM,0,"You need to use another item with this one.");
			return;
		}


		it[in2].value=base*b;
		it[in].value=base*a;

		*(unsigned int*)(it[in].drdata+1)=a;
		*(unsigned int*)(it[in2].drdata+1)=b;

		sprintf(it[in].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in].name);
		sprintf(it[in2].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in2].drdata+1)),it[in2].name);

		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from splitting metal (%s %s)",bignumber(a),bignumber(b));
		ch[cn].citem=in2; it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;

		log_char(cn,LOG_SYSTEM,0,"Split into %s units and %s units.",bignumber(a),bignumber(b));

		return;
	}
	if (it[in2].complexity>0 && it[in2].quality>0 && it[in2].driver==0 && it[in2].ID!=IID_ALCHEMY_INGREDIENT) {
		int maxsilver,maxgold;
		int givesilver,givegold;
                int pv,val;

		givesilver=maxsilver=(int)(it[in2].complexity*1.0500001)-it[in2].quality;
		givegold=maxgold=(int)(it[in2].complexity*  1.2000001)-it[in2].quality;
		base=it[in].value/(*(unsigned int*)(it[in].drdata+1));

		pv=get_support_prof(cn,P_SMITH);
		pv+=clan_smith_bonus(cn);

		if (pv) {
			maxsilver=maxsilver-(maxsilver*pv/100);
			maxgold=maxgold-(maxgold*pv/100);
		}

		if (it[in].drdata[0]==1) {	// silver
			if (maxsilver<=0) {
				log_char(cn,LOG_SYSTEM,0,"You cannot enhance it further with silver - unless it takes damage.");
				return;
			}
			if (*(unsigned int*)(it[in].drdata+1)<=maxsilver) {
				val=(*(unsigned int*)(it[in].drdata+1))*(1.0+pv/100.0);
				it[in2].quality+=val;
				if (!(it[in2].flags&IF_NOENHANCE)) it[in2].value=it[in2].quality*10;
                                log_char(cn,LOG_SYSTEM,0,"You use %s units of silver to enhance your %s by %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in2].name,bignumber(val));
				dlog(cn,in2,"Used %s units of silver to enhance (quality to %s).",bignumber(*(unsigned int*)(it[in].drdata+1)),bignumber(it[in2].quality));
				remove_item_char(in);
				destroy_item(in);
                                return;
			} else {
				log_char(cn,LOG_SYSTEM,0,"You use %s of the %s silver to enhance your %s by %s.",bignumber(maxsilver),bignumber(*(unsigned int*)(it[in].drdata+1)),it[in2].name,bignumber(givesilver));
                                *(unsigned int*)(it[in].drdata+1)-=maxsilver;
				it[in].value=(*(unsigned int*)(it[in].drdata+1))*base;
				it[in2].quality+=givesilver;
				if (!(it[in2].flags&IF_NOENHANCE)) it[in2].value=it[in2].quality*10;
				sprintf(it[in].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in].name);
				dlog(cn,in2,"Used %s units of silver to enhance (quality to %s).",bignumber(maxsilver),bignumber(it[in2].quality));
                                return;
			}
		}
		if (it[in].drdata[0]==2) {	// gold
			if (maxgold<=0) {
				log_char(cn,LOG_SYSTEM,0,"You cannot enhance it further with gold - unless it takes damage.");
				return;
			}
			if (*(unsigned int*)(it[in].drdata+1)<=maxgold) {
				val=(*(unsigned int*)(it[in].drdata+1))*(1.0+pv/100.0);
				it[in2].quality+=val;
				if (!(it[in2].flags&IF_NOENHANCE)) it[in2].value=it[in2].quality*10;
                                log_char(cn,LOG_SYSTEM,0,"You use %s units of gold to enhance your %s by %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in2].name,bignumber(val));
				dlog(cn,in2,"Used %s units of gold to enhance (quality to %s).",bignumber(*(unsigned int*)(it[in].drdata+1)),bignumber(it[in2].quality));
				remove_item_char(in);
				destroy_item(in);
				return;
			} else {
				log_char(cn,LOG_SYSTEM,0,"You use %s of the %s gold to enhance your %s by %s.",bignumber(maxgold),bignumber(*(unsigned int*)(it[in].drdata+1)),it[in2].name,bignumber(givegold));
                                *(unsigned int*)(it[in].drdata+1)-=maxgold;
                                it[in].value=(*(unsigned int*)(it[in].drdata+1))*base;
				it[in2].quality+=givegold;
				if (!(it[in2].flags&IF_NOENHANCE)) it[in2].value=it[in2].quality*10;
				sprintf(it[in].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in].name);
				dlog(cn,in2,"Used %s units of gold to enhance (quality to %s).",bignumber(maxgold),bignumber(it[in2].quality));
                                return;
			}
		}

		return;
	}
	if (it[in2].driver!=it[in].driver || it[in2].drdata[0]!=it[in].drdata[0]) {
		log_char(cn,LOG_SYSTEM,0,"You cannot mix those.");
		return;
	}

	*(unsigned int*)(it[in].drdata+1)+=*(unsigned int*)(it[in2].drdata+1);
	it[in].value+=it[in2].value;
	sprintf(it[in].description,"%s units of %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in].name);
	log_char(cn,LOG_SYSTEM,0,"%s units of %s.",bignumber(*(unsigned int*)(it[in].drdata+1)),it[in].name);

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"dropped by combining metal");
	destroy_item(in2);
	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;
}

#define MAXDOOR	30
static int tdoor[MAXDOOR];
static int sdoor[MAXDOOR];
static int door_init=0;

// drdata:	
//[0]		number
//[1]		istarget
//[2]		direction
//[3]		usable (source only)

void minedoor(int in,int cn)
{
	int in2,nr,ret;

        // gather information about door system
	if (!door_init) {
		for (in2=1; in2<MAXITEM; in2++) {
			if (!it[in2].flags) continue;
			if (it[in2].driver!=IDR_MINEDOOR) continue;

			nr=it[in2].drdata[0];
			//xlog("found something at %d,%d",it[in2].x,it[in2].y);

			if (!it[in2].drdata[1]) {	// source door
				if (it[in2].drdata[3]) {	// usable source door
					if (sdoor[nr]) xlog("confused: two source doors");
                                        sdoor[nr]=in2;
					//xlog("source %d is at %d,%d",it[in2].drdata[0],it[in2].x,it[in2].y);
				}
			} else {	// target door
				if (tdoor[nr]) xlog("confused: two target doors");
				tdoor[nr]=in2;
				//xlog("target %d is at %d,%d",it[in2].drdata[0],it[in2].x,it[in2].y);
			}
		}
		door_init=1;
	}

	nr=it[in].drdata[0];
	
	if (!cn) {	// timer call
		call_item(it[in].driver,in,0,ticker+TICKS*30);

		if (it[in].drdata[3]) return;	// we're already open, dont do anything

		// dont change it when the surrounding walls have been dug away
		if (!(map[it[in].x+it[in].y*MAXMAP+1].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP-1].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP+1+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP+1-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP-1+MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) ||
		    !(map[it[in].x+it[in].y*MAXMAP-1-MAXMAP].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK))) {
			//xlog("dug out 1");
                        return;
		}
                if ((in2=sdoor[nr])) {	// there is a source door already, close it
			if (!(map[it[in2].x+it[in2].y*MAXMAP+1].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP-1].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP+MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP-MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP+1+MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP+1-MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP-1+MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) ||
			    !(map[it[in2].x+it[in2].y*MAXMAP-1-MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) {
				//xlog("dug out 2");
				return;
			}
			if (RANDOM(20)) return;	// some randomness

			it[in2].sprite=15000;
			it[in2].flags&=~IF_USE;
			it[in2].drdata[3]=0;
			set_sector(it[in2].x,it[in2].y);
			//xlog("closed source door %d at %d,%d",nr,it[in2].x,it[in2].y);
		}

		switch(it[in].drdata[2]) {
			case DX_UP:
			case DX_DOWN:	it[in].sprite=20124; break;
			case DX_RIGHT:
			case DX_LEFT:	it[in].sprite=20122; break;
		}
		it[in].flags|=IF_USE;
		it[in].drdata[3]=1;
		set_sector(it[in].x,it[in].y);
		sdoor[nr]=in;
		//xlog("opened source door %d at %d,%d",nr,it[in].x,it[in].y);

		return;
	}
	
	// find target
	if (it[in].drdata[1]) in2=sdoor[nr];
	else in2=tdoor[nr];

	// teleport character to target
	ret=0;
	switch(it[in2].drdata[2]) {
		case DX_UP:	ret=teleport_char_driver(cn,it[in2].x,it[in2].y-1); break;
		case DX_DOWN:	ret=teleport_char_driver(cn,it[in2].x,it[in2].y+1); break;
		case DX_RIGHT:	ret=teleport_char_driver(cn,it[in2].x-1,it[in2].y); break;
		case DX_LEFT:	ret=teleport_char_driver(cn,it[in2].x+1,it[in2].y); break;
	}
	if (!ret) {
		log_char(cn,LOG_SYSTEM,0,"The mine has collapsed behind this door. Some friendly spirit grabs you and teleports you somewhere else.");
		if (areaID!=31) teleport_char_driver(cn,230,240);
		else teleport_char_driver(cn,211,231);
	}
}

void keyholder_door(int in,int cn)
{
	int x,y,n,in2,flag,co,nr;
	char buf[80];

	if (!cn) return;

	nr=it[in].drdata[0];

	for (n=0; n<9; n++) {
		flag=0;
		for (x=2+(n%3)*8; x<9+(n%3)*8; x++) {
			for (y=231+(n/3)*8; y<238+(n/3)*8; y++) {
				if ((in2=map[x+y*MAXMAP].it) && (it[in2].flags&IF_TAKE)) { flag=1; break; }
				if (map[x+y*MAXMAP].ch) { flag=1; break; }
			}
		}
		if (!flag) break;
	}	
	if (flag) {
		log_char(cn,LOG_SYSTEM,0,"You hear fighting noises from behind the door. It won't open while the fight lasts.");
		return;
	}
        if (!(in2=ch[cn].citem) || it[in2].driver!=IDR_ENHANCE || it[in2].drdata[0]!=2 || *(unsigned int*)(it[in2].drdata+1)!=2000) {
		log_char(cn,LOG_SYSTEM,0,"You'll need to use 2000 gold units as a key to open the door.");
		return;
	}
	if (!teleport_char_driver(cn,2+(n%3)*8+1,231+(n/3)*8+3)) {
		log_char(cn,LOG_SYSTEM,0,"You hear fighting noises from behind the door. It won't open while the fight lasts.");
		return;
	}
	destroy_item(in2);
	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;

	sprintf(buf,"keyholder_golem%d",nr);
	co=create_char(buf,0);
	if (co) {
		update_char(co);
		ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
		ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
		ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
		ch[co].dir=DX_LEFTUP;
		ch[co].tmpx=2+(n%3)*8+5;
		ch[co].tmpy=231+(n/3)*8+3;

		drop_char(co,2+(n%3)*8+5,231+(n/3)*8+3,1);
	}
}

struct gate_fight_driver_data
{
	int creation_time;
	int victim;
};

void keyhold_fight_driver(int cn,int ret,int lastact)
{
        struct gate_fight_driver_data *dat;
	struct msg *msg,*next;

	dat=set_data(cn,DRD_GATE_FIGHT,sizeof(struct gate_fight_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        dat->creation_time=ticker;
                        fight_driver_set_dist(cn,10,0,20);
                }

                standard_message_driver(cn,msg,1,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	// self destruct eventually
	if (ticker-dat->creation_time>TICKS*60*5) {
		say(cn,"Thats all folks!");
		remove_destroy_char(cn);
		return;
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;
			
	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

        do_idle(cn,TICKS);
}

void minegateway(int in,int cn)
{
	int x,y,a;

        if (!cn) return;	// always make sure its not an automatic call if you don't handle it
	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!has_item(cn,IID_MINEGATEWAY)) {
		log_char(cn,LOG_SYSTEM,0,"The door won't open. You notice an inscription: \"This door leads to the Dwarven town Grimroot. Only those who have proven their abilities as miners and fighters may enter.");
		return;
	}

	x=*(unsigned short*)(it[in].drdata+0);
	y=*(unsigned short*)(it[in].drdata+2);
	a=*(unsigned short*)(it[in].drdata+4);

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2 || !a) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d,%d).",ch[cn].name,x,y,a);
		return;
	}

        if (!change_area(cn,a,x,y)) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - target area server is down.",ch[cn].name);
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_GOLEMKEYHOLDER:	keyhold_fight_driver(cn,ret,lastact); return 1;
                default:			return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_MINEWALL:	minewall(in,cn); return 1;
		case IDR_ENHANCE:	collect_item(in,cn); return 1;
		case IDR_MINEDOOR:	minedoor(in,cn); return 1;
		case IDR_MINEKEYDOOR:	keyholder_door(in,cn); return 1;
		case IDR_MINEGATEWAY:	minegateway(in,cn); return 1;

		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_GOLEMKEYHOLDER:	return 1;
                default:			return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:		return 0;
	}
}












