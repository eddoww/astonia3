/*
 * $Id: area2.c,v 1.5 2006/10/06 08:20:01 devel Exp $
 *
 * $Log: area2.c,v $
 * Revision 1.5  2006/10/06 08:20:01  devel
 * fixed bug in vampire2 driver
 *
 * Revision 1.4  2006/10/04 17:29:09  devel
 * made daggers and sun amulet vanish after completing the vampire quests
 *
 * Revision 1.3  2006/09/22 08:55:42  devel
 * added destruction of sun amulet to finish of vampire quest 1
 *
 * Revision 1.2  2006/09/21 11:22:52  devel
 * added questlog
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
#include "date.h"
#include "item_id.h"
#include "los.h"
#include "libload.h"
#include "quest_exp.h"
#include "sector.h"
#include "consistency.h"
#include "area3.h"
#include "questlog.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
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

struct superior_driver_data
{
	int nr;
	int stun;
	int mode;
};

#define M_FIGHT	1
#define M_RUN	2

void superior_driver(int cn,int ret,int lastact)
{
	struct superior_driver_data *dat;
        struct msg *msg,*next;

        dat=set_data(cn,DRD_SUPERIORDRIVER,sizeof(struct superior_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,40,0,200);
			dat->nr=atoi(ch[cn].arg);
			dat->mode=M_FIGHT;
                }

		// check talk for keyword
		if (msg->type==NT_TEXT) {
                        if ((msg->dat1==1 || msg->dat1==2) && msg->dat3!=cn) {	// talk, and not our talk
				if (strcasestr((char*)msg->dat2,"Nazimah") && dat->nr==1) dat->stun=ticker+TICKS*60;
				if (strcasestr((char*)msg->dat2,"Argatoth") && dat->nr==2) dat->stun=ticker+TICKS*60;
				if (strcasestr((char*)msg->dat2,"Lorganoth") && dat->nr==3) dat->stun=ticker+TICKS*60;
				if (strcasestr((char*)msg->dat2,"Markanoth") && dat->nr==4) dat->stun=ticker+TICKS*60;
			}
		}

                standard_message_driver(cn,msg,1,0);
		remove_message(cn,msg);
	}

	if (dat->stun>ticker) {	// we're stunned
		ch[cn].speed_mode=SM_STEALTH;
		do_idle(cn,TICKS);
		return;
	}

	ch[cn].speed_mode=SM_NORMAL;
	
        fight_driver_update(cn);

	if (ch[cn].hp<10*POWERSCALE ||
	    ch[cn].lifeshield<POWERSCALE*5)
		dat->mode=M_RUN;
	
	if (ch[cn].hp>=ch[cn].value[0][V_HP]*POWERSCALE &&
	    ch[cn].mana>=ch[cn].value[0][V_MANA]*POWERSCALE &&
	    ch[cn].lifeshield>=ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE)
		dat->mode=M_FIGHT;

	if (dat->mode==M_RUN) {
		if (fight_driver_flee(cn)) return;
	} else {
		if (fight_driver_attack_visible(cn,0)) return;
		if (fight_driver_follow_invisible(cn)) return;
	}	

	ch[cn].speed_mode=SM_STEALTH;

	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	do_idle(cn,TICKS);
}

//-------

void shrine_driver(int in,int cn)
{
	int r,in2,type;

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	type=it[in].drdata[0];
	in2=ch[cn].citem;

	if (type==0) {
		if (!in2 || it[in2].ID!=IID_AREA2_ZOMBIESKULL1) {
			log_char(cn,LOG_SYSTEM,0,"You sense that this ancient shrine used to receive gifts. Strange gifts. You feel a craving for bone.");
			return;
		}
	} else if (type==1) {
		if (!in2 || it[in2].ID!=IID_AREA2_ZOMBIESKULL2) {
			log_char(cn,LOG_SYSTEM,0,"You sense that this ancient shrine used to receive gifts. Strange gifts. You feel a craving for bone and silver.");
			return;
		}
	} else {
		if (!in2 || it[in2].ID!=IID_AREA2_ZOMBIESKULL3) {
			log_char(cn,LOG_SYSTEM,0,"You sense that this ancient shrine used to receive gifts. Strange gifts. You feel a craving for bone and gold.");
			return;
		}
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"dropped into zombie-shrine");

	destroy_item(in2);
	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;
	in2=0;

	if (type==0) {

		r=RANDOM(22);

		switch(r) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:		in2=create_item("torch"); break;
			
			case 10:
			case 11:        if (ch[cn].flags&CF_MAGE) in2=create_item("mana_potion1");
					else in2=create_item("healing_potion1");
					
					break;
			
			case 12:
			case 13:	if (ch[cn].flags&CF_WARRIOR) in2=create_item("healing_potion1");
					else in2=create_item("mana_potion1");
					
					break;
	
			case 14:	
			case 15:	log_char(cn,LOG_SYSTEM,0,"You have been blessed with experience.");
					give_exp(cn,250);
					break;
			
			case 16:	log_char(cn,LOG_SYSTEM,0,"You have been protected for a short while.");
					add_bonus_spell(cn,IDR_ARMOR,5*20,TICKS*60*5);
					break;
			
			case 17:	log_char(cn,LOG_SYSTEM,0,"You are more dangerous for a short while.");
					add_bonus_spell(cn,IDR_WEAPON,5,TICKS*60*5);
					break;
			
			case 18:	log_char(cn,LOG_SYSTEM,0,"Your capacity was increased for a short while.");
					if (ch[cn].flags&CF_WARRIOR) add_bonus_spell(cn,IDR_HP,5,TICKS*60*5);
					else add_bonus_spell(cn,IDR_MANA,5,TICKS*60*5);
					break;
	
			case 19:	log_char(cn,LOG_SYSTEM,0,"Your capacity was increased for a short while.");
					if (ch[cn].flags&CF_MAGE) add_bonus_spell(cn,IDR_MANA,5,TICKS*60*5);
					else add_bonus_spell(cn,IDR_HP,5,TICKS*60*5);
					break;
			
			case 20:	
			case 21:	in2=create_item("zombie_skull2"); break;

		}
	} else if (type==1) {

		r=RANDOM(13);

		switch(r) {
                        case 0:
			case 1:		if (ch[cn].flags&CF_MAGE) in2=create_item("mana_potion2");
					else in2=create_item("healing_potion2");
					
					break;
			
			case 2:
			case 3:		if (ch[cn].flags&CF_WARRIOR) in2=create_item("healing_potion2");
					else in2=create_item("mana_potion2");
					
					break;
	
			case 4:	
			case 5:	
			case 6:		log_char(cn,LOG_SYSTEM,0,"You have been blessed with experience.");
					give_exp(cn,750);
					break;

			
			case 7:		log_char(cn,LOG_SYSTEM,0,"You have been protected for a while.");
					add_bonus_spell(cn,IDR_ARMOR,10*20,TICKS*60*15);
					break;
			
			case 8:		log_char(cn,LOG_SYSTEM,0,"You are more dangerous for a while.");
					add_bonus_spell(cn,IDR_WEAPON,10,TICKS*60*15);
					break;
			
			case 9:		log_char(cn,LOG_SYSTEM,0,"Your capacity was increased for a while.");
					if (ch[cn].flags&CF_WARRIOR) add_bonus_spell(cn,IDR_HP,10,TICKS*60*15);
					else add_bonus_spell(cn,IDR_MANA,10,TICKS*60*15);
					break;
	
			case 10:	log_char(cn,LOG_SYSTEM,0,"Your capacity was increased for a while.");
					if (ch[cn].flags&CF_MAGE) add_bonus_spell(cn,IDR_MANA,10,TICKS*60*15);
					else add_bonus_spell(cn,IDR_HP,10,TICKS*60*15);
					break;

			case 11:	
			case 12:	in2=create_item("zombie_skull3"); break;
		}
	} else {	// type 2

		r=RANDOM(7);

		switch(r) {
                        case 0:
			case 1:		if (ch[cn].flags&CF_MAGE) in2=create_item("mana_potion3");
					else in2=create_item("healing_potion3");
					
					break;
			
                        case 2:
			case 3:		if (ch[cn].flags&CF_WARRIOR) in2=create_item("healing_potion3");
					else in2=create_item("mana_potion3");
					
					break;
	
			case 4:	
			case 5:	
			case 6:		log_char(cn,LOG_SYSTEM,0,"You have been blessed with experience.");
					give_exp(cn,2250);
					break;
		}
	}

	if (in2) {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from zombie-shrine");
		ch[cn].citem=in2;
		it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;
		
		log_char(cn,LOG_SYSTEM,0,"You received a gift.");
	}
}

//----------------

void fireball_machine(int in,int cn)
{
	int dx,dy,power,fn,dxs,dys,frequency;

        dx=it[in].drdata[0]-128;
	dy=it[in].drdata[1]-128;
	power=it[in].drdata[2];
	frequency=it[in].drdata[3];

	if (frequency && !cn) call_item(IDR_FIREBALL,in,0,ticker+frequency);

	if (dx>0) dxs=1;
	else if (dx<0) dxs=-1;
	else dxs=0;

	if (dy>0) dys=1;
	else if (dy<0) dys=-1;
	else dys=0;

	fn=create_fireball(0,it[in].x+dxs,it[in].y+dys,it[in].x+dx,it[in].y+dy,power);
	notify_area(it[in].x,it[in].y,NT_SPELL,0,V_FIREBALL,fn);
}

//----------------

struct moonie_data
{
	int want_it;
	int yummy;
	int lastmunch;
};

void moonie_driver(int cn,int ret,int lastact)
{
        struct moonie_data *dat;
	struct msg *msg,*next;
        int co,friend=0,in;
	
        dat=set_data(cn,DRD_MOONIE_DATA,sizeof(struct moonie_data));
	if (!dat) return;	// oops...

	scan_item_driver(cn);

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,10,0,40);
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

                        if (ch[co].group==ch[cn].group &&
                            cn!=co &&
                            ch[cn].value[0][V_BLESS] &&
                            ch[cn].mana>=BLESSCOST &&
                            may_add_spell(co,IDR_BLESS) &&
                            char_see_char(cn,co))
                                friend=co;
		}

		if (msg->type==NT_ITEM) {
			in=msg->dat1;

			if (char_see_item(cn,in) && it[in].ID==IID_AREA2_SMALLSPIDER) {
                                if (!dat->want_it && !dat->yummy) {
					say(cn,"Ohhh. Yummy spider! Want it! Want it!");
					dat->want_it=in;
					remove_message(cn,msg);					
				}
				continue;
			}
		}

		if (msg->type==NT_GOTHIT) {
			if (dat->yummy || dat->want_it) say(cn,"Ouch.");
			dat->yummy=0;		// stop eating if we get hit
			dat->want_it=0;		// stop going for item if we get hit			
		}

		standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if ((in=ch[cn].citem)) {
		if (it[in].ID==IID_AREA2_SMALLSPIDER) {
			say(cn,"Such a nice, yummy spider! Ahh. Mmmmh.");
			dat->yummy=ticker+TICKS*60;
			dat->lastmunch=ticker;
		}
		destroy_item(in);
		ch[cn].citem=0;
	}

        if (dat->yummy>ticker) {
		if (ticker>dat->lastmunch+TICKS*10) {
			say(cn,"Munch, munch. Ohhh, so good!");
			dat->lastmunch=ticker;
		}
		do_idle(cn,TICKS);
		return;
	} else dat->yummy=0;

	if (dat->want_it && !char_see_item(cn,dat->want_it)) {
		say(cn,"Oh. Gone.");
		dat->want_it=0;
	}

	if (dat->want_it) {
		if (take_driver(cn,dat->want_it)) return;
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;

	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

	// help friend by blessing him. all checks already done in message loop
	if (friend && do_bless(cn,friend)) return;
	
        //say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}

struct vampire_driver_data
{
	int killed;
	int amulet;
};

void vampire_driver(int cn,int ret,int lastact)
{
        struct vampire_driver_data *dat;
	struct msg *msg,*next;
	int co,in;
	
        dat=set_data(cn,DRD_VAMPIREDRIVER,sizeof(struct vampire_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
			dat->killed=ticker;
			fight_driver_set_dist(cn,30,0,60);
                }
		if (msg->type==NT_CHAR) {
			co=msg->dat1;

			if ((in=ch[co].item[WN_NECK]) && it[in].ID==IID_AREA2_SUN123) {
				if (ticker-dat->amulet>TICKS*10) {
					say(cn,"Oh no!");
				}
				dat->amulet=ticker;
			}
		}

                standard_message_driver(cn,msg,1,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ticker-dat->amulet<TICKS*5) ch[cn].flags&=~CF_NODEATH;
	else ch[cn].flags|=CF_NODEATH;

	if ((ch[cn].hp<POWERSCALE/2) && (ch[cn].flags&CF_NODEATH)) {
		create_mist(ch[cn].x,ch[cn].y);
                teleport_char_driver(cn,ch[cn].tmpx,ch[cn].tmpy);
		ch[cn].hp=POWERSCALE;		
		dat->killed=ticker;
		return;
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

	if (ticker-dat->killed<TICKS*120) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
		fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
	} else {
                if (secure_move_driver(cn,232,123,DX_DOWN,ret,lastact)) return;
		fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
	}


	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        do_idle(cn,TICKS);
}

void vampire2_driver(int cn,int ret,int lastact)
{
        struct msg *msg,*next;
	int co,in;
	
        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,30,0,60);
                }
		if (msg->type==NT_GOTHIT) {
			co=msg->dat1;

			if ((ch[co].flags) && (in=ch[co].item[WN_RHAND]) && it[in].ID==IID_AREA2_DAGGERRIGHT) {
				say(cn,"Arrrgh!");
				kill_char(cn,co);
				
				if (ch[co].flags&CF_PLAYER) dlog(co,in,"dropped because vampire was killed with it");
				remove_item_char(in);
				destroy_item(in);
                                log_char(co,LOG_SYSTEM,0,"Your dagger was destroyed in the blow.");
				destroy_item_byID(co,IID_AREA2_DAGGERRIGHT);
				destroy_item_byID(co,IID_AREA2_DAGGERWRONG);
				return;
			}
			
			if ((ch[co].flags) && (in=ch[co].item[WN_RHAND]) && it[in].ID==IID_AREA2_DAGGERWRONG) {
				say(cn,"Hahahaha!");

				if (ch[co].flags&CF_PLAYER) dlog(co,in,"dropped because vampire was not killed with it");
				remove_item_char(in);
				destroy_item(in);
                                log_char(co,LOG_SYSTEM,0,"Your dagger was destroyed in the blow.");
				return;
			}
		}

                standard_message_driver(cn,msg,1,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        do_idle(cn,TICKS);
}


void parkshrine_driver(int in,int cn)
{
	struct area3_ppd *ppd;
	int new;

	if (!cn) return;
	
	ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	if (!ppd) return;
	
	switch(it[in].drdata[0]) {
		case 1:		new=!ppd->kelly_found1; ppd->kelly_found1=1; break;
		case 2:		new=!ppd->kelly_found2; ppd->kelly_found2=1; break;
		case 3:		new=!ppd->kelly_found3; ppd->kelly_found3=1; break;
		default:	log_char(cn,LOG_SYSTEM,0,"BUG #55343, please report"); return;
	}
	if (new) {
		log_char(cn,LOG_SYSTEM,0,"You memorize the location of the shrine.");
	} else {
		log_char(cn,LOG_SYSTEM,0,"This shrine seems familar.");
	}
}

void burn_char(int cn)
{
	int fn,n;

	// dont add effect twice
	for (n=0; n<4; n++) {
		if (!(fn=ch[cn].ef[n])) continue;
		if (ef[fn].type!=EF_BURN) continue;
		return;
	}
	
	create_show_effect(EF_BURN,cn,ticker,ticker+TICKS*60,250,1);
	hurt(cn,20*POWERSCALE,0,1,50,75);
}

void extinguish_driver(int in,int cn)
{
        int fn,n;

	if (!cn) return;

	// is he on fire?
	for (n=0; n<4; n++) {
		if (!(fn=ch[cn].ef[n])) continue;
		if (ef[fn].type!=EF_BURN) continue;
		break;
	}
	if (n==4) {
		log_char(cn,LOG_SYSTEM,0,"Ahh. Sweet and refreshing.");
		return;
	}

	remove_effect(fn);
	free_effect(fn);

	log_char(cn,LOG_SYSTEM,0,"You extinguish the flames.");
}

void flamethrow_driver(int in,int cn)
{
	int fire,dir,co,x,y,m;

	if (cn) return;

	fire=it[in].drdata[0];

	if (fire) {
                it[in].drdata[0]--;

		if (!it[in].drdata[2]) {
			it[in].sprite++;
			it[in].drdata[2]=1;

			remove_item_light(in);
			it[in].mod_index[4]=V_LIGHT;
			it[in].mod_value[4]=250;
			add_item_light(in);
		}

		dir=it[in].drdata[1];
		dx2offset(dir,&x,&y,NULL);

		x=it[in].x+x; y=it[in].y+y;
		if (x>0 && x<MAXMAP && y>0 && y<MAXMAP) {
			m=x+y*MAXMAP;
			if ((co=map[m].ch)) burn_char(co);			
		}
		
		dx2offset(dir,&x,&y,NULL);
		x=it[in].x+x*2; y=it[in].y+y*2;
		if (x>0 && x<MAXMAP && y>0 && y<MAXMAP) {
			m=x+y*MAXMAP;
			if ((co=map[m].ch)) burn_char(co);						
		}
		
                call_item(it[in].driver,in,0,ticker+1);
	} else {
		it[in].sprite--;
		it[in].drdata[0]=TICKS;
		it[in].drdata[2]=0;
		
		remove_item_light(in);
		it[in].mod_index[4]=0;
		it[in].mod_value[4]=0;
		add_item_light(in);

		if (it[in].drdata[3]) call_item(it[in].driver,in,0,ticker+TICKS*it[in].drdata[3]);
		
	}
}

void spiketrap_driver(int in,int cn)
{
	if (cn && !it[in].drdata[0]) {
		it[in].sprite++;
		it[in].drdata[0]=1;
		set_sector(it[in].x,it[in].y);

		hurt(cn,it[in].drdata[1]*POWERSCALE,0,1,75,75);

		call_item(it[in].driver,in,0,ticker+TICKS);
		return;
	}
	if (!cn && it[in].drdata[0]) {
		it[in].sprite--;
		it[in].drdata[0]=0;
		set_sector(it[in].x,it[in].y);
		return;
	}
}

void chestspawn_driver(int in,int cn)
{
	int co;

	if (cn && !it[in].drdata[1]) {

		switch(it[in].drdata[0]) {
			case 0:		co=create_char("normal_vampire",0); break;
			default:	return;
		}
		ch[co].dir=DX_RIGHTDOWN;
		update_char(co);
		
		ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
		ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
		ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;

		if (!drop_char(co,it[in].x,it[in].y,0)) {
			destroy_char(co);
			return;
		}
		ch[co].tmpx=ch[co].x; ch[co].tmpy=ch[co].y;
		ch[co].flags&=~CF_RESPAWN;

		it[in].sprite++;
		it[in].drdata[1]=1;
		set_sector(it[in].x,it[in].y);
		
		*(unsigned *)(it[in].drdata+2)=co;
		*(unsigned *)(it[in].drdata+6)=ch[co].serial;

		call_item(it[in].driver,in,0,ticker+TICKS*10);
		return;
	}
	
	if (!cn && it[in].drdata[1]) {
		co=*(unsigned *)(it[in].drdata+2);
		if (!(ch[co].flags) || ch[co].serial!=*(unsigned *)(it[in].drdata+6)) {
			it[in].sprite--;
			it[in].drdata[1]=0;
			set_sector(it[in].x,it[in].y);
		} else {
			call_item(it[in].driver,in,0,ticker+TICKS*10);
		}
		
		return;
	}
}

void vampire_dead_driver(int cn,int co)
{
	struct area3_ppd *ppd;

	if (!co) return;

	ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	if (!ppd) return;

	if (ppd->crypt_state>=8 && ppd->crypt_state<=9) {
		log_char(co,LOG_SYSTEM,0,"Congratulations on slaying the toughest(?) creature down here!");
		questlog_done(co,18);
		destroy_item_byID(co,IID_AREA2_SUN1);
		destroy_item_byID(co,IID_AREA2_SUN2);
		destroy_item_byID(co,IID_AREA2_SUN3);
		destroy_item_byID(co,IID_AREA2_SUN12);
		destroy_item_byID(co,IID_AREA2_SUN13);
		destroy_item_byID(co,IID_AREA2_SUN23);
		destroy_item_byID(co,IID_AREA2_SUN123);
                ppd->crypt_state=10;
	}
}

void vampire2_dead_driver(int cn,int co)
{
	struct area3_ppd *ppd;

	if (!co) return;

	ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	if (!ppd) return;

	if (ppd->crypt_state>=12 && ppd->crypt_state<=14) {
		log_char(co,LOG_SYSTEM,0,"Congratulations on slaying the toughest creature down here!");
		questlog_done(co,19);		
                ppd->crypt_state=15;
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_SUPERIOR:	superior_driver(cn,ret,lastact); return 1;
		case CDR_MOONIE:	moonie_driver(cn,ret,lastact); return 1;
		case CDR_VAMPIRE:	vampire_driver(cn,ret,lastact); return 1;
		case CDR_VAMPIRE2:	vampire2_driver(cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_SHRINE:	shrine_driver(in,cn); return 1;
		case IDR_FIREBALL:	fireball_machine(in,cn); return 1;
		case IDR_PARKSHRINE:	parkshrine_driver(in,cn); return 1;
		case IDR_FLAMETHROW:	flamethrow_driver(in,cn); return 1;
		case IDR_SPIKETRAP:	spiketrap_driver(in,cn); return 1;
		case IDR_CHESTSPAWN:	chestspawn_driver(in,cn); return 1;
		case IDR_EXTINGUISH:	extinguish_driver(in,cn); return 1;

                default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_SUPERIOR:	return 1;
		case CDR_VAMPIRE:	vampire_dead_driver(cn,co); return 1;
		case CDR_MOONIE:	return 1;
		case CDR_VAMPIRE2:	vampire2_dead_driver(cn,co); return 1;

                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_VAMPIRE:	return 1;
		case CDR_MOONIE:	return 1;
		case CDR_SUPERIOR:	return 1;
		case CDR_VAMPIRE2:	return 1;

                default:	return 0;
	}
}









