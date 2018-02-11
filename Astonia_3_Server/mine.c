/*

$Id: mine.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: mine.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:27  sam
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
#include "act.h"
#include "item_id.h"
#include "sector.h"
#include "player_driver.h"
#include "military.h"
#include "consistency.h"

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
				sprintf(it[in2].description,"%d units of %s.",*(unsigned int*)(it[in2].drdata+1),it[in2].name);

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

int enhance_item_sprite(int in)
{
	int spr=0;

	if (it[in].sprite>=10120 && it[in].sprite<=10159) spr=it[in].sprite-10120+59300;
	if (it[in].sprite>=10190 && it[in].sprite<=10199) spr=it[in].sprite-10190+59340;
	if (it[in].sprite>=10220 && it[in].sprite<=10229) spr=it[in].sprite-10220+59350;
	if (it[in].sprite>=10250 && it[in].sprite<=10259) spr=it[in].sprite-10250+59360;
	if (it[in].sprite>=10280 && it[in].sprite<=10289) spr=it[in].sprite-10280+59370;
	if (it[in].sprite>=59300 && it[in].sprite<=59399) spr=it[in].sprite-59300+59200;

	switch(it[in].sprite) {
		case 50510:	spr=59388; break;
		case 50025:     spr=59380; break;
		case 50026:	spr=59381; break;
		case 50122:	spr=59382; break;
		case 50123:	spr=59383; break;
		case 50124:	spr=59384; break;
		case 50125:	spr=59385; break;
		case 50126:	spr=59386; break;
		case 50141:	spr=59387; break;
		case 50512:	spr=59389; break;
		case 50513:	spr=59390; break;
		case 51084:	spr=59473; break;
		case 51617:	spr=59299; break;
		case 59299:	spr=59291; break;
		case 59473:	spr=59474; break;
	}


	return spr;
}

static int armorbonus(int in)
{
	if (it[in].flags&IF_WNHEAD) return 40;
	if (it[in].flags&IF_WNARMS) return 30;
	if (it[in].flags&IF_WNLEGS) return 30;
	if (it[in].flags&IF_WNBODY) return 100;

	return 0;
}

static int get_maxmod(int in)
{
	int maxmod=0,m;

	for (m=0; m<MAXMOD; m++) {
		switch(it[in].mod_index[m]) {
			case V_WEAPON:	break;
			case V_ARMOR:	break;
			case V_SPEED:	break;
			case V_DEMON:	break;
			case V_LIGHT:	break;
			default:	if (it[in].mod_index[m]>=0) maxmod=max(maxmod,it[in].mod_value[m]); break;
		}
	}
	return maxmod;
}

static int enhance_item_price(int in)
{
	return get_maxmod(in)*100+100;
}

static int enhance_cannot_use(int in,int cn)
{
	int m;

	for (m=0; m<MAXMOD; m++) {
		if (it[in].mod_value[m]) {
			switch(it[in].mod_index[m]) {
				case -V_ARMORSKILL:	if (ch[cn].value[1][V_ARMORSKILL]<it[in].mod_value[m]+10) return 1;
							else break;
				case -V_DAGGER:		if (ch[cn].value[1][V_DAGGER]<it[in].mod_value[m]+10) return 1;
							else break;
				case -V_STAFF:		if (ch[cn].value[1][V_STAFF]<it[in].mod_value[m]+10) return 1;
							else break;
				case -V_SWORD:		if (ch[cn].value[1][V_SWORD]<it[in].mod_value[m]+10) return 1;
							else break;
				case -V_TWOHAND:	if (ch[cn].value[1][V_TWOHAND]<it[in].mod_value[m]+10) return 1;
							else break;
			}
		}
	}
	return 0;
}

int enhance_item(int in,int cn)
{
	int in2,spr,m,need,price;

        if (!(in2=ch[cn].citem)) return 0;

	if (it[in2].flags&IF_NOENHANCE) return 0;

	if (!(spr=enhance_item_sprite(in2))) return 0;

	if ((spr>=59200 && spr<59299) || spr==59474) {	// silver item
		if (it[in].drdata[0]!=2) {	// needs gold
			log_char(cn,LOG_SYSTEM,0,"This item has already been enhanced once. For further enhancements, you need gold.");
			return 1;
		}
	} else {			// normal item
		if (it[in].drdata[0]!=1) {	// needs silver
			log_char(cn,LOG_SYSTEM,0,"To enhance this item, you need silver.");
			return 1;
		}
	}

	need=enhance_item_price(in2);
	if (need>*(unsigned int*)(it[in].drdata+1)) {
		log_char(cn,LOG_SYSTEM,0,"You do not have enough %s to enhance this item. You need %d units.",it[in].name,need);
		return 1;
	}

	if (enhance_cannot_use(in2,cn) && (*(unsigned int *)(it[in].drdata+8)!=in2 || realtime-*(unsigned int *)(it[in].drdata+12)>15)) {
		log_char(cn,LOG_SYSTEM,0,"Enhancing this item would make it unusable for you. Click again if this is what you want.");
		*(unsigned int *)(it[in].drdata+8)=in2;
		*(unsigned int *)(it[in].drdata+12)=realtime;
		return 1;
	}

	price=it[in].value*need/(*(unsigned int*)(it[in].drdata+1));
	*(unsigned int*)(it[in].drdata+1)-=need;
	if (*(unsigned int*)(it[in].drdata+1)<1) {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used to enhance item");
		remove_item_char(in);
		destroy_item(in);
	} else {
		it[in].value-=price;	//it[in].value*(*(unsigned int*)(it[in].drdata+1))/(need+(*(unsigned int*)(it[in].drdata+1)));
		sprintf(it[in].description,"%d units of %s.",*(unsigned int*)(it[in].drdata+1),it[in].name);
	}
	
	it[in2].sprite=spr;
	it[in2].value=it[in2].value+price;

	for (m=0; m<MAXMOD; m++) {
		if (it[in2].mod_value[m]) {
			switch(it[in2].mod_index[m]) {
				case -V_ARMORSKILL:	it[in2].mod_value[m]+=10; break;
				case -V_DAGGER:		it[in2].mod_value[m]+=10; break;
				case -V_STAFF:		it[in2].mod_value[m]+=10; break;
				case -V_SWORD:          it[in2].mod_value[m]+=10; break;
				case -V_TWOHAND:	it[in2].mod_value[m]+=10; break;
				case V_ARMOR:		it[in2].mod_value[m]+=armorbonus(in2); break;
				case V_WEAPON:		it[in2].mod_value[m]+=10; break;
				default:		if (it[in2].mod_index[m]>=0 && it[in2].mod_value[m]<20) it[in2].mod_value[m]++;
							break;
			}
		}
	}
	set_item_requirements(in2);
	ch[cn].flags|=CF_ITEMS;

	look_item(cn,it+in2);
	log_char(cn,LOG_SYSTEM,0,"You used %d units to enhance your %s.",need,it[in2].name);

	return 1;
}

void collect_item(int in,int cn)
{
	int in2;

	if (!cn) return;
	if (!it[in].carried) return;

	if (!(in2=ch[cn].citem)) {
		int a,b;

		a=*(unsigned int*)(it[in].drdata+1);
		if (a<2) {
			log_char(cn,LOG_SYSTEM,0,"You cannot split 1 unit.");
			return;
		}
		b=a/2;
		if (b>=10000) b=10000;
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

		it[in2].value=it[in].value*b/(a+b);
		it[in].value=it[in].value*a/(a+b);

		*(unsigned int*)(it[in].drdata+1)=a;
		*(unsigned int*)(it[in2].drdata+1)=b;

		sprintf(it[in].description,"%d units of %s.",*(unsigned int*)(it[in].drdata+1),it[in].name);
		sprintf(it[in2].description,"%d units of %s.",*(unsigned int*)(it[in2].drdata+1),it[in2].name);

		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from splitting metal (%d %d)",a,b);
		ch[cn].citem=in2; it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;

		log_char(cn,LOG_SYSTEM,0,"Split into %d units and %d units.",a,b);

		return;
	}
	if (it[in2].driver!=it[in].driver || it[in2].drdata[0]!=it[in].drdata[0]) {
		if (!enhance_item(in,cn)) log_char(cn,LOG_SYSTEM,0,"You cannot mix those.");
		return;
	}

	*(unsigned int*)(it[in].drdata+1)+=*(unsigned int*)(it[in2].drdata+1);
	it[in].value+=it[in2].value;
	sprintf(it[in].description,"%d units of %s.",*(unsigned int*)(it[in].drdata+1),it[in].name);
	log_char(cn,LOG_SYSTEM,0,"%d units of %s.",*(unsigned int*)(it[in].drdata+1),it[in].name);

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
