/*
 * $Id: bones.c,v 1.2 2007/02/24 14:09:54 devel Exp $
 *
 * $Log: bones.c,v $
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "tool.h"
#include "date.h"
#include "map.h"
#include "create.h"
#include "light.h"
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "database.h"
#include "mem.h"
#include "player.h"
#include "skill.h"
#include "expire.h"
#include "clan.h"
#include "chat.h"
#include "los.h"
#include "shrine.h"
#include "area3.h"
#include "sector.h"
#include "consistency.h"
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

#define MAXRUNE	1024

struct rune_ppd
{
	unsigned int used[MAXRUNE/32];

	int special_exec[25];
};

void create_special_execs(struct rune_ppd *ppd)
{
	int n,m,v,i;
	char buf[40];

	static int badlist[]={
		555,55,5,
		666,66,6,
		777,77,7,
		888,88,8,
		999,99,9
	};

	for (n=5; n<10; n++) {
		for (m=0; m<5; m++) {
			while (42) {
				v=RANDOM(n*111);
				
				if (v<100) continue;
				
				for (i=0; i<ARRAYSIZE(badlist); i++)	// none from bad list
					if (v==badlist[i]) break;
				if (i<ARRAYSIZE(badlist)) continue;
				
				for (i=0; i<m; i++)	// no duplicates
					if (v==ppd->special_exec[(n-5)*5+i]) break;
				if (i<m) continue;

				sprintf(buf,"%03d",v);
				
				for (i=0; buf[i]; i++)	// no zero's and no digits > n
					if (buf[i]=='0' || buf[i]>n+'0') break;
				if (buf[i]) continue;

				for (i=0; buf[i]; i++)	// at least one digit n
					if (buf[i]==n+'0') break;
				if (!buf[i]) continue;
				
				ppd->special_exec[(n-5)*5+m]=v;
				break;
			}
		}
	}
}

void bonebridge(int in,int cn)
{
	int in2,x,y,dir,m,flag;

	if (!cn) {	// timer call: remove old bridges
		if (!it[in].drdata[1]) return;
		
		if (it[in].carried) {
			elog("bonebridge: bug 77");
			return;
		}

		m=it[in].x+it[in].y*MAXMAP;
		
		if (map[m].flags&MF_TMOVEBLOCK) {
			call_item(it[in].driver,in,0,ticker+TICKS);
			return;
		}

		map[m].flags|=MF_MOVEBLOCK;

		it[in].drdata[1]++;
		it[in].sprite++;
		set_sector(it[in].x,it[in].y);
		
		if (it[in].drdata[1]>9) {
                        remove_item_map(in);
			destroy_item(in);
		} else call_item(it[in].driver,in,0,ticker+3);
		
		return;
	}

	if (!it[in].drdata[0] || it[in].drdata[1]) {	// basis or bones on ground
		if (!(in2=ch[cn].citem) || it[in2].ID!=IID_AREA18_BONE || it[in2].drdata[0]!=5) {
			log_char(cn,LOG_SYSTEM,0,"Hu?");
			return;
		}
		dir=offset2dx(ch[cn].x,ch[cn].y,it[in].x,it[in].y);
		dx2offset(dir,&x,&y,NULL);

		if (x) flag=0;
		else flag=1;

		x+=it[in].x;
		y+=it[in].y;
		if (x<2 || y<2 || x>=MAXMAP-2 || y>=MAXMAP-2) {
			log_char(cn,LOG_SYSTEM,0,"You have found bug #1532.");
			return;
		}
		m=x+y*MAXMAP;
		if (map[m].it || !(map[m].flags&MF_MOVEBLOCK)) {
			log_char(cn,LOG_SYSTEM,0,"It doesn't fit.");
			return;
		}

		map[m].it=in2;
		map[m].flags&=~MF_MOVEBLOCK;
		
                ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;

		it[in2].carried=0;
		it[in2].contained=0;
		it[in2].x=x;
		it[in2].y=y;
		it[in2].flags&=~IF_TAKE;
		it[in2].drdata[1]=1;
		
		if (flag) it[in2].sprite=13045;
		else it[in2].sprite=13035;
		
		set_sector(x,y);
		
		call_item(it[in2].driver,in2,0,ticker+TICKS*60);
		return;
	}

        // bones in inventory
	if ((in2=ch[cn].citem)) {
		if (it[in2].ID==IID_AREA18_BONE) {
			if (it[in].drdata[0]>4) {
				log_char(cn,LOG_SYSTEM,0,"The bridge is finished. You cannot add more bones.");
				return;
			}
			it[in].drdata[0]++;
			it[in].sprite=13030+it[in].drdata[0];
			ch[cn].flags|=CF_ITEMS;
			
			it[in2].drdata[0]--;
			it[in2].sprite=13030+it[in2].drdata[0];
			if (it[in2].drdata[0]==0) {
                                destroy_item(in2);
				ch[cn].citem=0;
			}
			return;
		} else {
			log_char(cn,LOG_SYSTEM,0,"Hu?");
			return;
		}
	} else {
		if (it[in].drdata[0]<2) {
			log_char(cn,LOG_SYSTEM,0,"Hu?");
			return;
		} else {
			it[in].drdata[0]--;
			it[in].sprite=13030+it[in].drdata[0];
			ch[cn].flags|=CF_ITEMS;
			
			in2=create_item("bone");
			ch[cn].citem=in2;
			it[in2].carried=cn;
                        }
	}
}

void boneladder(int in,int cn)
{
	if (!cn) return;
	
	if (it[in].drdata[0]) {
		teleport_char_driver(cn,it[in].x-4,it[in].y-3);
	} else {
		teleport_char_driver(cn,it[in].x+4,it[in].y+3);
	}
}

int rune_check(int cn,int nr,struct rune_ppd *ppd)
{
	int i,bit;

	if (nr<0 || nr>=MAXRUNE) {
		log_char(cn,LOG_SYSTEM,0,"You have found bug #5136a.");
		return 0;
	}
	i=nr/32;
	bit=1u<<(nr&31);
	if (ppd->used[i]&bit) {
		log_char(cn,LOG_SYSTEM,0,"You cannot use this combination again.");
		return 0;
	}
	return 1;
}

void rune_set(int nr,struct rune_ppd *ppd)
{
	int i,bit;
	
	if (nr<0 || nr>=MAXRUNE) {
		elog("Bug #5136b.");
		return;
	}

	i=nr/32;
	bit=1u<<(nr&31);

	ppd->used[i]|=bit;
}

void rune_raise(int cn,int war_skill,int mage_skill)
{
	if (ch[cn].flags&CF_WARRIOR) {
		if (raise_value_exp(cn,war_skill)) log_char(cn,LOG_SYSTEM,0,"You gained %s.",skill[war_skill].name);
	} else {
		if (raise_value_exp(cn,mage_skill)) log_char(cn,LOG_SYSTEM,0,"You gained %s.",skill[mage_skill].name);
	}
}

void exec_rune(int cn,int nr,int lastholder)
{
	struct rune_ppd *ppd;
	int flag=0,n;

	ppd=set_data(cn,DRD_RUNE_PPD,sizeof(struct rune_ppd));
	if (!ppd) return;

	if (!ppd->special_exec[0]) create_special_execs(ppd);

	if (!rune_check(cn,nr,ppd)) return;

	switch(nr) {
		// level 1
		case 1:		teleport_char_driver(cn,90,25); break;		// teleport to next area
		case 11:        give_exp(cn,level_value(min(ch[cn].level+5,50))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 111:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,72,52);
				break;

		// level 2
		case 2:		teleport_char_driver(cn,175,4); break;		// teleport to next area
		case 22:	give_exp(cn,level_value(min(ch[cn].level+5,51))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 222:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,105,40);
				break;
                case 212:	if (raise_value_exp(cn,V_ENDURANCE)) log_char(cn,LOG_SYSTEM,0,"You gained endurance.");
				flag=1;
				break;

		// level 3
		case 3:		teleport_char_driver(cn,4,69); break;		// teleport to next area
		case 33:	give_exp(cn,level_value(min(ch[cn].level+5,52))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 333:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,236,25);
				break;
		case 231:	if (raise_value_exp(cn,V_HP)) log_char(cn,LOG_SYSTEM,0,"You gained hitpoints.");
				flag=1;
				break;
		case 133:	if (raise_value_exp(cn,V_MANA)) log_char(cn,LOG_SYSTEM,0,"You gained mana.");
				flag=1;
				break;

		// level 4
		case 4:		teleport_char_driver(cn,90,95); break;		// teleport to next area
		case 44:	give_exp(cn,level_value(min(ch[cn].level+5,54))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 444:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,67,85);
				break;
		case 143:	if (raise_value_exp(cn,V_PARRY)) log_char(cn,LOG_SYSTEM,0,"You gained parry.");
				flag=1;
				break;
		case 442:	if (raise_value_exp(cn,V_MAGICSHIELD)) log_char(cn,LOG_SYSTEM,0,"You gained magic shield.");
				flag=1;
				break;
		case 241:	if (raise_value_exp(cn,V_IMMUNITY)) log_char(cn,LOG_SYSTEM,0,"You gained immunity.");
				flag=1;
				break;
				
		// level 5
		case 5:		teleport_char_driver(cn,176,110); break;		// teleport to next area
		case 55:	give_exp(cn,level_value(min(ch[cn].level+5,55))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 555:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,107,105);
				break;

		// level 6
		case 6:		teleport_char_driver(cn,4,146); break;		// teleport to next area
		case 66:	give_exp(cn,level_value(min(ch[cn].level+5,57))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 666:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,216,115);
				break;

		// level 7
		case 7:		teleport_char_driver(cn,91,171); break;		// teleport to next area
		case 77:	give_exp(cn,level_value(min(ch[cn].level+5,58))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 777:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,9,172);
				break;

		// level 8
		case 8:		teleport_char_driver(cn,187,152); break;		// teleport to next area
		case 88:	give_exp(cn,level_value(min(ch[cn].level+5,59))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 888:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,91,133);
				break;

		// level 9
		case 9:		teleport_char_driver(cn,137,222); break;	// teleport to next area
		case 99:	give_exp(cn,level_value(min(ch[cn].level+5,61))/6); flag=1; 	// bonus
				log_char(cn,LOG_SYSTEM,0,"You gained experience."); break;
		case 999:	log_char(cn,LOG_SYSTEM,0,"You hear a sinister laugh echo through the emptiness.");
				teleport_char_driver(cn,235,152);
				break;

		// special execs and wrong combos
		default:	for (n=0; n<25; n++) {
					if (ppd->special_exec[n]==nr) {
						switch(n) {
							// level 5
							case 0:	rune_raise(cn,V_ATTACK,V_FLASH); flag=1; break;
							case 1:	rune_raise(cn,V_SWORD,V_FREEZE); flag=1; break;
							case 2: rune_raise(cn,V_TWOHAND,V_FIRE); flag=1; break;
							case 3: rune_raise(cn,V_HAND,V_HEAL); flag=1; break;
							case 4: rune_raise(cn,V_PERCEPT,V_PERCEPT); flag=1; break;

							// level 6
							case 5: rune_raise(cn,V_PROFESSION,V_PROFESSION); flag=1; break;
							case 6: rune_raise(cn,V_INT,V_INT); flag=1; break;
							case 7: rune_raise(cn,V_STR,V_WIS); flag=1; break;
							case 8: rune_raise(cn,V_HP,V_MANA); flag=1; break;
							case 9: rune_raise(cn,V_STEALTH,V_STEALTH); flag=1; break;

							// level 7
							case 10: rune_raise(cn,V_SWORD,V_FREEZE); flag=1; break;
							case 11: rune_raise(cn,V_ATTACK,V_FLASH); flag=1; break;
							case 12: rune_raise(cn,V_PARRY,V_MAGICSHIELD); flag=1; break;
							case 13: rune_raise(cn,V_SURROUND,V_DAGGER); flag=1; break;
							case 14: rune_raise(cn,V_SPEEDSKILL,V_STAFF); flag=1; break;

							// level 8
							case 15: rune_raise(cn,V_TACTICS,V_BLESS); flag=1; break;
							case 16: rune_raise(cn,V_AGI,V_AGI); flag=1; break;
							case 17: rune_raise(cn,V_HP,V_MANA); flag=1; break;
							case 18: rune_raise(cn,V_ENDURANCE,V_ENDURANCE); flag=1; break;
							case 19: rune_raise(cn,V_BARTER,V_BARTER); flag=1; break;

							// level 9
							case 20: if (!lastholder) log_char(cn,LOG_SYSTEM,0,"This combination seems to be right, but it does not work here.");
								 else { teleport_char_driver(cn,14,213); log_char(cn,LOG_SYSTEM,0,"Uh-oh"); }
								 break;		// teleport to bonus area
							case 21: rune_raise(cn,V_INT,V_INT); flag=1; break;
							case 22: rune_raise(cn,V_IMMUNITY,V_IMMUNITY); flag=1; break;
							case 23: rune_raise(cn,V_PROFESSION,V_PROFESSION); flag=1; break;
							case 24: rune_raise(cn,V_STR,V_WIS); flag=1; break;

							default:	log_char(cn,LOG_SYSTEM,0,"You found bug #1926"); break;
						}
						break;
					}
				}
				if (n==25) log_char(cn,LOG_SYSTEM,0,"Nothing happened."); break;
	}
        if (flag) rune_set(nr,ppd);	
}

void update_holder(int in)
{
	int m;

	m=it[in].x+it[in].y*MAXMAP;

	if (it[in].drdata[0]==0) {
		if (it[in].drdata[1]==0) it[in].sprite=13103;
		else it[in].sprite=13104;
		
		map[m].fsprite=0;
	} else {
		it[in].sprite=13104+it[in].drdata[0];
		
		if (it[in].drdata[1]==0) map[m].fsprite=13103;
		else map[m].fsprite=13104;
	}
	set_sector(it[in].x,it[in].y);
}

int create_rune_from_holder(int in)
{
	int in2;
	char buf[80];

	sprintf(buf,"rune%d",it[in].drdata[0]);
	in2=create_item(buf);

	return in2;
}

void remove_rune_from_holder(int in,int cn)
{
	int in2;

	in2=create_rune_from_holder(in);
	it[in].drdata[0]=0;
	update_holder(in);
	
	if (in2 && !give_char_item(cn,in2)) {
		destroy_item(in2);
	}
}

void boneholder(int in,int cn)
{
	int in2,nr;

	if (it[in].drdata[1]==2 || it[in].drdata[1]==3) {	// activation holder
		if (!cn) return;

                nr=0;

		in2=map[it[in].x+it[in].y*MAXMAP-3].it;
                if (in2 && it[in2].drdata[0] && *(unsigned int*)(it[in2].drdata+8)==charID(cn)) {
			nr=nr*10+it[in2].drdata[0];
			remove_rune_from_holder(in2,cn);
		}

		in2=map[it[in].x+it[in].y*MAXMAP-2].it;
                if (in2 && it[in2].drdata[0] && *(unsigned int*)(it[in2].drdata+8)==charID(cn)) {
			nr=nr*10+it[in2].drdata[0];
			remove_rune_from_holder(in2,cn);
		}

		in2=map[it[in].x+it[in].y*MAXMAP-1].it;
                if (in2 && it[in2].drdata[0] && *(unsigned int*)(it[in2].drdata+8)==charID(cn)) {
			nr=nr*10+it[in2].drdata[0];
			remove_rune_from_holder(in2,cn);
		}

		if (!nr) {
			log_char(cn,LOG_SYSTEM,0,"You sense that you must place something on the stand before you can activate it.");
			return;
		}
		exec_rune(cn,nr,it[in].drdata[1]==3);

		return;
	}

	if (!cn) {	// expire runes after 120 seconds
		if (ticker-*(unsigned int*)(it[in].drdata+12)<TICKS*120) return;
		it[in].drdata[0]=0;
	} else if ((in2=ch[cn].citem)) {	// add rune to holder
		if (it[in2].ID<IID_AREA18_RUNE1 || it[in2].ID>IID_AREA18_RUNE9) {
			log_char(cn,LOG_SYSTEM,0,"That does not fit.");
			return;
		}
		if (it[in].drdata[0]) {
			log_char(cn,LOG_SYSTEM,0,"There is a rune already.");
			return;
		}
		it[in].drdata[0]=it[in2].ID-IID_AREA18_RUNE1+1;
		*(unsigned int*)(it[in].drdata+8)=charID(cn);
		*(unsigned int*)(it[in].drdata+12)=ticker;
                destroy_item(in2);
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		call_item(it[in].driver,in,0,ticker+TICKS*120+1);
	} else {	// remove rune from holder
		if (!it[in].drdata[0]) {
			log_char(cn,LOG_SYSTEM,0,"You touch the stand.");
			return;
		}
		if (*(unsigned int*)(it[in].drdata+8)!=charID(cn)) {
			log_char(cn,LOG_SYSTEM,0,"This rune does not belong to you. You cannot take it.");
			return;
		}
		
		in2=create_rune_from_holder(in);
                if (!in2) {
			log_char(cn,LOG_SYSTEM,0,"You found bug #11970");
			return;
		}
		it[in].drdata[0]=0;

                ch[cn].citem=in2;
		it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;
	}

	update_holder(in);
}

void bonewall(int in,int cn)
{
	int in2;

	if (!cn && !it[in].drdata[0]) return;
	if (cn && it[in].drdata[0]) return;

        if (it[in].drdata[0]==0) {
		if ((in2=map[it[in].x+it[in].y*MAXMAP+1].it) && it[in2].driver==IDR_BONEWALL && it[in2].drdata[0]==0) call_item(it[in2].driver,in2,1,ticker+4);
		if ((in2=map[it[in].x+it[in].y*MAXMAP-1].it) && it[in2].driver==IDR_BONEWALL && it[in2].drdata[0]==0) call_item(it[in2].driver,in2,1,ticker+4);
		if ((in2=map[it[in].x+it[in].y*MAXMAP+MAXMAP].it) && it[in2].driver==IDR_BONEWALL && it[in2].drdata[0]==0) call_item(it[in2].driver,in2,1,ticker+4);
		if ((in2=map[it[in].x+it[in].y*MAXMAP-MAXMAP].it) && it[in2].driver==IDR_BONEWALL && it[in2].drdata[0]==0) call_item(it[in2].driver,in2,1,ticker+4);
	}

	if (it[in].drdata[0]<5) {
                it[in].drdata[0]++;
		it[in].sprite++;		
		set_sector(it[in].x,it[in].y);
		call_item(it[in].driver,in,0,ticker+2);
		return;
	}
	if (it[in].drdata[0]==5) {
		map[it[in].x+it[in].y*MAXMAP].it=0;
		map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TMOVEBLOCK;
		it[in].flags&=~IF_USE;
		it[in].flags|=IF_VOID;

		remove_lights(it[in].x,it[in].y);
		map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TSIGHTBLOCK;
		reset_los(it[in].x,it[in].y);
		add_lights(it[in].x,it[in].y);

		it[in].drdata[0]=6;
		call_item(it[in].driver,in,0,ticker+TICKS*60+RANDOM(TICKS*30));
		return;
	}

	if (it[in].drdata[0]==6) {
		if ((map[it[in].x+it[in].y*MAXMAP].flags&MF_TMOVEBLOCK) || map[it[in].x+it[in].y*MAXMAP].it) {
			call_item(it[in].driver,in,0,ticker+TICKS);
			return;
		}
		it[in].sprite-=5;
		it[in].drdata[0]=0;
		it[in].flags|=IF_USE;
		it[in].flags&=~IF_VOID;
		
		remove_lights(it[in].x,it[in].y);
		map[it[in].x+it[in].y*MAXMAP].it=in;
		map[it[in].x+it[in].y*MAXMAP].flags|=MF_TSIGHTBLOCK|MF_TMOVEBLOCK;
		reset_los(it[in].x,it[in].y);
		add_lights(it[in].x,it[in].y);
		set_sector(it[in].x,it[in].y);
		return;
	}
}

void bonehint(int in,int cn)
{
	int level,nr,pos,val,res;
	char buf[40];
	struct rune_ppd *ppd;
	
	static char *rune_name[10]={
		"none",
		"Ansuz",
		"Berkano",
		"Dagaz",
		"Ehwaz",
		"Fehu",
		"Hagalaz",
		"Isa",
		"Ingwaz",
		"Raidho"
	};

	static char *pos_name[3]={
		"first",
		"second",
		"third"
	};

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	ppd=set_data(cn,DRD_RUNE_PPD,sizeof(struct rune_ppd));
	if (!ppd) return;

	if (!ppd->special_exec[0])
		create_special_execs(ppd);

        if (!it[in].drdata[1]) {
		it[in].drdata[1]=1;
		it[in].drdata[2]=sqrt(RANDOM(25));
		it[in].drdata[3]=RANDOM(3);
	}
	
	level=it[in].drdata[0];
	nr=it[in].drdata[2];
	pos=it[in].drdata[3];

	val=ppd->special_exec[(level-5)*5+nr];
	sprintf(buf,"%d",val);
	res=buf[pos]-'0';
	if (res<0 || res>9) {
		log_char(cn,LOG_SYSTEM,0,"You found bug #197-%d-%d-%d-%d",level,nr,pos,val);
		return;
	}

	log_char(cn,LOG_SYSTEM,0,"Rune Diary, Page %d:",level*10+nr);
	log_char(cn,LOG_SYSTEM,0,"Used the rune %s in the %s position.",rune_name[res],pos_name[pos]);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		default:			return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {		
		case IDR_BONEBRIDGE:	bonebridge(in,cn); return 1;
		case IDR_BONELADDER:	boneladder(in,cn); return 1;
		case IDR_BONEHOLDER:	boneholder(in,cn); return 1;
		case IDR_BONEWALL:	bonewall(in,cn); return 1;
		case IDR_BONEHINT:	bonehint(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                default:			return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:			return 0;
	}
}


