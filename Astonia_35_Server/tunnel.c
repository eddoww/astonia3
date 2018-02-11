/*
 * $Id: tunnel.c,v 1.5 2008/04/11 10:54:21 devel Exp $
 *
 * $Log: tunnel.c,v $
 * Revision 1.5  2008/04/11 10:54:21  devel
 * less hp for creepers
 *
 * Revision 1.4  2007/08/09 11:14:48  devel
 * balancing
 *
 * Revision 1.3  2007/06/11 10:06:48  devel
 * removed deleted mission_ppd.h from includes
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
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
#include "date.h"
#include "map.h"
#include "create.h"
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "skill.h"
#include "area1.h"
#include "consistency.h"
#include "database.h"
#include "sector.h"
#include "sleep.h"
#include "btrace.h"
#include "player_driver.h"

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
		default: 	return 0;
	}
}

int build_fighter(int x,int y,int diff,int level)
{
	int cn,n,val,in;

	cn=create_char("tunnel_creeper",0);

	for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost)	continue;
		if (!ch[cn].value[1][n]) continue;

		switch (n) {
			case V_HP:		val=max(10,diff/2); break;
			case V_ENDURANCE:	val=max(10,diff-30); break;
			case V_MANA:		val=max(10,diff/2); break;

			case V_WIS:		val=max(10,diff-25); break;
			case V_INT:		val=max(10,diff-5); break;
			case V_AGI:		val=max(10,diff-5); break;
			case V_STR:		val=max(10,diff-5); break;

			case V_HAND:		val=max(1,diff); break;
			case V_ATTACK:		val=max(1,diff); break;
			case V_PARRY:		val=max(1,diff); break;
			case V_IMMUNITY:	val=max(1,diff); break;

			case V_TACTICS:		val=max(1,diff-5); break;
			case V_WARCRY:		val=max(1,diff-15); break;
			case V_SURROUND:	val=max(1,diff-20); break;
			case V_SPEEDSKILL:	val=max(1,diff-5); break;
			case V_PERCEPT:		val=max(1,diff-10); break;

			case V_BLESS:		val=max(1,diff-5); break;
			case V_FIRE:		val=max(1,diff-5); break;
			case V_FREEZE:		val=max(1,diff); break;
			case V_MAGICSHIELD:	val=max(1,diff-5); break;

			default:		val=max(1,diff-30); break;
		}

		val=min(val,250);
		ch[cn].value[1][n]=val;
	}
	ch[cn].x=ch[cn].tmpx=x;
	ch[cn].y=ch[cn].tmpy=y;
	ch[cn].dir=DX_RIGHTDOWN;

	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);
	//if ((diff>100 && ch[cn].level<10) || ch[cn].level>200) ch[cn].level=200;
	ch[cn].level=level;

	// create special equipment bonus to equal that of the average player
	in=create_item("equip1");
	
        for (n=0; n<5; n++) it[in].mod_value[n]=1+ch[cn].level/3;
	ch[cn].item[13]=in; it[in].carried=cn;

	in=create_item("equip2");
        for (n=0; n<5; n++) it[in].mod_value[n]=1+ch[cn].level/3;
        ch[cn].item[14]=in; it[in].carried=cn;

        // create armor
	in=create_item("armor_spell");
	ch[cn].item[15]=in; it[in].carried=cn;
	it[in].mod_value[0]=17*20;

	in=create_item("weapon_spell");
	ch[cn].item[16]=in; it[in].carried=cn;
	it[in].mod_value[0]=17;

	update_char(cn);

	ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
	ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

	drop_char(cn,x,y,0);

	return cn;
}


void mission_fighter_driver(int cn,int ret,int lastact)
{
	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

struct tunnel_ppd
{
	int clevel;			// current level (resets with every entry)
	unsigned char used[204];	// already used teleport out X times
};

int creeper_tab[191]={
 13, 15, 16, 18, 19, 20, 22, 23, 25, 26,
 28, 29, 30, 31, 33, 34, 36, 37, 39, 40,
 41, 42, 44, 45, 46, 48, 49, 50, 51, 53,
 54, 55, 57, 58, 59, 60, 61, 62, 64, 65,
 66, 68, 69, 70, 71, 72, 73, 75, 76, 77,
 79, 80, 81, 82, 83, 84, 86, 87, 88, 89,
 90, 91, 92, 94, 95, 96, 97, 99,100,101,
102,103,104,105,107,108,109,110,111,112,
113,115,116,117,118,120,121,122,123,124,
125,126,127,129,130,131,132,133,134,135,
137,138,139,140,141,142,143,145,146,147,
148,149,150,151,152,154,155,156,157,158,
160,161,162,163,164,165,166,168,169,170,
171,172,173,174,175,177,178,179,180,181,
182,183,184,186,187,188,189,190,191,192,
193,195,196,197,198,199,200,201,202,204,
205,206,207,208,210,211,212,213,214,215,
216,217,219,220,221,222,223,224,225,226,
228,229,230,231,232,233,234,235,237,238,
239};

void tunneldoor(int in,int cn)
{
	int x,y,in2,m,b1,b2,c,b1cnt=0,b2cnt=0,co,xoff,yoff,used,n,value; //,want;
        struct tunnel_ppd *ppd;

	if (!cn) {	// automatic call
		return;
	}
	
	/*want=10;
	printf("\nint creeper_tab[191]={\n");
	for (m=10; m<400; m++) {
		co=build_fighter(100,100,m);
		if (ch[co].level>=want) {
			printf("%3d",m);
			want++;
			if (want==201) break;
			else printf(",");
			if (want>10 && want%10==0) printf("\n");			
		}
		remove_destroy_char(co);
	}
	printf("};\n");*/

	ppd=set_data(cn,DRD_TUNNEL_PPD,sizeof(struct tunnel_ppd));
	if (!ppd) return;	// oops...

        if (it[in].drdata[0]==2 || it[in].drdata[0]==3) {
		
		if (teleport_char_driver(cn,250,250)) {
			if (ppd->used[ppd->clevel]<20) {

				ppd->used[ppd->clevel]++;

				if (it[in].drdata[0]==2) {
					value=level_value(ppd->clevel)/10.0/ppd->used[ppd->clevel];
					//xlog("value=%f (level=%d)",value,ppd->clevel);
					log_char(cn,LOG_SYSTEM,0,"You have been given experience.");
					give_exp(cn,value);
					dlog(cn,0,"got %d exp for solving long tunnel section %d (%d)",value,ppd->clevel,ppd->used[ppd->clevel]);
				}

				if (it[in].drdata[0]==3) {
					value=50/ppd->used[ppd->clevel];
					//xlog("value=%f (level=%d)",value,ppd->clevel);
					log_char(cn,LOG_SYSTEM,0,"You have been given military rank.");
					give_military_pts_no_npc(cn,value,1);					
					dlog(cn,0,"got %d military rank for solving long tunnel section %d (%d)",value,ppd->clevel,ppd->used[ppd->clevel]);
				}
								
			} else log_char(cn,LOG_SYSTEM,0,"You cannot get any more experience for this level.");
			ppd->clevel=10;
		}
		return;

        } else if (!it[in].drdata[0]) {
		if (ch[cn].level<20) ppd->clevel=10;
                else if (ch[cn].level<=100) ppd->clevel=ch[cn].level-10;
		else {
			for (n=90; n<ch[cn].level-10; n++) {
				if (ppd->used[n]<20) break;
			}
			ppd->clevel=n;
		}
	} else ppd->clevel++;

	if (ppd->clevel<10) ppd->clevel=10;
	if (ppd->clevel>200) ppd->clevel=200;

	srand(ch[cn].ID*ppd->clevel);
	b1=RANDOM(3);
	b2=RANDOM(2);
	c=RANDOM(3);

	used=1;
	for (xoff=1; xoff<210; xoff+=31) {
                for (yoff=1; yoff<255; yoff+=127) {
			if (xoff==218 && yoff==128) continue;
			
			used=0;
			for (x=1+xoff; x<32+xoff && !used; x++) {
                                for (y=1+yoff; y<128+yoff && !used; y++) {
					m=x+y*MAXMAP;
					if ((co=map[m].ch) && (ch[co].flags&CF_PLAYER) && co!=cn) used=1;					
				}
			}
			if (!used) break;
		}
		if (!used) break;
	}
	if (used) {
		log_char(cn,LOG_SYSTEM,0,"All tunnels are busy. Please try again later.");
		return;
	}

	for (x=1+xoff; x<32+xoff; x++) {
		for (y=1+yoff; y<128+yoff; y++) {
			m=x+y*MAXMAP;
			
			if ((co=map[m].ch) && !(ch[co].flags&CF_PLAYER)) {
				remove_destroy_char(co);
			}
			
			if ((in2=map[m].it)) {
				if (it[in2].ID==IID_TUNNELDOOR1) {	// block marker 1
					it[in2].sprite=0;
                                        if (b1cnt==b1) {
						map[m].fsprite=0;
						map[m].flags&=~(MF_MOVEBLOCK|MF_SIGHTBLOCK);
					} else {
						map[m].fsprite=59791;
						map[m].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK;
					}
					b1cnt++;
				} else if (it[in2].ID==IID_TUNNELDOOR2) {	// block marker 2
					it[in2].sprite=0;
                                        if (b2cnt==b2) {
						map[m].fsprite=0;
						map[m].flags&=~(MF_MOVEBLOCK|MF_SIGHTBLOCK);
					} else {
						map[m].fsprite=59791;
						map[m].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK;
					}
					b2cnt++;
				} else if (it[in2].ID==IID_TUNNELENEMY1) {	// creeper marker 1
					it[in2].sprite=0;
					if (c==0) build_fighter(it[in2].x,it[in2].y,creeper_tab[ppd->clevel-10],ppd->clevel);
				} else if (it[in2].ID==IID_TUNNELENEMY2) {	// creeper marker 2
					it[in2].sprite=0;
					if (c==1) build_fighter(it[in2].x,it[in2].y,creeper_tab[ppd->clevel-10],ppd->clevel);
				} else if (it[in2].ID==IID_TUNNELENEMY3) {	// creeper marker 3
					it[in2].sprite=0;
					if (c==2) build_fighter(it[in2].x,it[in2].y,creeper_tab[ppd->clevel-10],ppd->clevel);
				} else if (it[in2].ID==IID_TUNNELENEMYALL) {	// creeper marker all
					it[in2].sprite=0;
					build_fighter(it[in2].x,it[in2].y,creeper_tab[ppd->clevel-10],ppd->clevel);
				} else if (it[in2].driver==IDR_TUNNELDOOR && (it[in2].drdata[0]==2 || it[in2].drdata[0]==3)) {
					sprintf(it[in2].name,"Column %d, used %d times",ppd->clevel,ppd->used[ppd->clevel]);
				} else if (it[in2].driver==IDR_TUNNELDOOR2) {
					it[in2].sprite=0;
					map[m].fsprite=59791;
					map[m].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK;
				}
			}
		}
	}

	if (!it[in].drdata[0]) ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	else ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+ch[cn].value[0][V_HP]*POWERSCALE/3*2);
	
	teleport_char_driver(cn,16+xoff,123+yoff);
}

void mean_door(int in,int cn)
{
	int x,y,co;

	if (!cn) {
		call_item(it[in].driver,in,0,ticker+TICKS);

		for (x=it[in].x-4; x<=it[in].x+4; x++) {
			for (y=it[in].y+1; y<it[in].y+20; y++) {
				if ((co=map[x+y*MAXMAP].ch) && !(ch[co].flags&CF_PLAYER)) {
					//xlog("%d,%d %s",x,y,ch[co].name);
					return;
				}
			}
		}
		map[it[in].x+it[in].y*MAXMAP].fsprite=0;
		map[it[in].x+it[in].y*MAXMAP].flags&=~(MF_MOVEBLOCK|MF_SIGHTBLOCK);
		set_sector(it[in].x,it[in].y);
		//xlog("clear");
	}
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch (nr) {
		//case CDR_MISSIONGIVE:	mission_giver_driver(cn,ret,lastact); return 1;
		//case CDR_MISSIONFIGHT:	mission_fighter_driver(cn,ret,lastact); return 1;

		default:        return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch (nr) {
		case IDR_TUNNELDOOR:	tunneldoor(in,cn); return 1;
		case IDR_TUNNELDOOR2:	mean_door(in,cn); return 1;

		default:        return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch (nr) {
		default:        return 0;
		//case CDR_MISSIONFIGHT:	mission_fighter_dead(cn,co); return 1;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch (nr) {
		default:        return 0;
	}
}
