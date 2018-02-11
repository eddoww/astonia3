/*
 * $Id: palace.c,v 1.7 2008/04/21 09:09:19 devel Exp $
 *
 * $Log: palace.c,v $
 * Revision 1.7  2008/04/21 09:09:19  devel
 * fixed potion on ground islena bug
 *
 * Revision 1.6  2007/09/03 08:58:41  devel
 * fixed distance kill abuse against floating eyes
 *
 * Revision 1.5  2007/06/11 10:05:49  devel
 * fixed spelling error
 *
 * Revision 1.4  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.3  2005/12/01 14:44:38  ssim
 * made it possible to leave islena's room
 *
 * Revision 1.2  2005/11/27 18:04:22  ssim
 * added driver for door to islenas room (only one player at a time allowed)
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
#include "player_driver.h"
#include "sector.h"
#include "item_id.h"
#include "player.h"
#include "consistency.h"
#include "chat.h"

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

#define MAXPAT	20
struct palace_guard_data
{
	unsigned char patrolx[MAXPAT],patroly[MAXPAT];
	unsigned char alertx,alerty;
        unsigned char reserve;
	unsigned char patrol;
        unsigned char pat;
	unsigned char scream;
	unsigned char line;

	unsigned char doalert;
        unsigned char docheck;
	unsigned char dofreeze;
	unsigned char dox,doy;

	int doscout;
	int lastfight;
};

void palace_guard_parse(int cn,struct palace_guard_data *dat)
{
	char *ptr,name[64],value[64];
	int pat=0;

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"patrolx")) { if (pat>=MAXPAT) elog("palace_guard_parse: too many patrol stops"); else dat->patrolx[pat]=atoi(value); }
		else if (!strcmp(name,"patroly")) { if (pat>=MAXPAT) elog("palace_guard_parse: too many patrol stops"); else dat->patroly[pat++]=atoi(value); }
		else if (!strcmp(name,"patrol")) dat->patrol=atoi(value);
		else if (!strcmp(name,"alertx")) dat->alertx=atoi(value);
		else if (!strcmp(name,"alerty")) dat->alerty=atoi(value);
		else if (!strcmp(name,"reserve")) dat->reserve=atoi(value);
		else if (!strcmp(name,"scream")) dat->scream=atoi(value);
		else if (!strcmp(name,"line")) dat->line=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void palace_guard(int cn,int ret,int lastact)
{
        struct palace_guard_data *dat;
	struct msg *msg,*next;
        int co,in,m;

	dat=set_data(cn,DRD_PALACEGUARD,sizeof(struct palace_guard_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
                                palace_guard_parse(cn,dat);
				ch[cn].arg=NULL;
			}
                        fight_driver_set_dist(cn,0,20,0);
			
			if (ch[cn].item[30] && (ch[cn].flags&CF_NOBODY)) {
				ch[cn].flags&=~(CF_NOBODY);
				ch[cn].flags|=CF_ITEMDEATH;
				//xlog("transformed item %s",it[ch[cn].item[30]].name);
			}			
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;
			if ((in=ch[co].item[WN_HEAD]) && it[in].driver==IDR_PALACECAP && it[in].drdata[0]) {	// wearing activated palace cap
				fight_driver_remove_enemy(cn,co);
				remove_message(cn,msg);
				continue;
			}
			if (dat->alertx && !dat->doalert && char_see_char(cn,co) && can_attack(cn,co)) {
				if (!dat->dofreeze) say(cn,"Granishni?");
				dat->dofreeze=1;
				dat->dox=ch[co].x;
				dat->doy=ch[co].y;				
			}
			if (dat->dofreeze && tile_char_dist(cn,co)<4 && char_see_char(cn,co) && can_attack(cn,co)) {
				if (!dat->doalert) {
					dat->doalert=1;
					say(cn,"Granishni!");
					if (do_freeze(cn)) return;					
				}				
				dat->dofreeze=0;
				dat->dox=ch[co].x;
				dat->doy=ch[co].y;
			}
			if (dat->scream && ticker-dat->lastfight>TICKS*20 && char_dist(cn,co)<16 && char_see_char(cn,co) && can_attack(cn,co)) {
				say(cn,"Granishni kwalar!");
				notify_area_shout(ch[cn].x,ch[cn].y,NT_NPC,NTID_PALACE_ALERT,ch[co].x,ch[co].y);
				dat->lastfight=ticker;
			}
		}

		if (msg->type==NT_GOTHIT) {
			co=msg->dat1;
			if (dat->scream && ticker-dat->lastfight>TICKS*20 && co && can_attack(cn,co)) {
				say(cn,"Granishni kwalar!");
				notify_area_shout(ch[cn].x,ch[cn].y,NT_NPC,NTID_PALACE_ALERT,ch[co].x,ch[co].y);
				dat->lastfight=ticker;
			}
		}

		// got a NPC message?
		if (msg->type==NT_NPC) {
			if (dat->reserve && msg->dat1==NTID_PALACE_ALERT) {
				if (!dat->doscout) say(cn,"Schak'ko");
				dat->dox=msg->dat2;
				dat->doy=msg->dat3;
				dat->doscout=ticker;				
			}
		}

		if (dat->scream) standard_message_driver(cn,msg,0,0);
		else standard_message_driver(cn,msg,1,1);

                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (dat->doalert) {
		if (abs(dat->alertx-ch[cn].x)<2 && abs(dat->alerty-ch[cn].y)<2) {
			dat->doalert=0;
			dat->docheck=0;
			notify_area_shout(ch[cn].x,ch[cn].y,NT_NPC,NTID_PALACE_ALERT,dat->dox,dat->doy);
			say(cn,"Granishni kwalar!");
		} else {
			if (move_driver(cn,dat->alertx,dat->alerty,1)) return;
			if (tmove_driver(cn,dat->alertx,dat->alerty,1)) return;			
		}
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) { dat->lastfight=ticker; return; }
	if (fight_driver_follow_invisible(cn)) { dat->lastfight=ticker; return; }

	if (dat->doscout) {
		if (ticker-dat->doscout>TICKS*30) {
			dat->doscout=0;
			if (dat->patrolx[0]) {
				dat->docheck=1;
				dat->pat=0;
				say(cn,"Olk'ka?");
			}
		} else if (abs(dat->dox-ch[cn].x)<2 && abs(dat->doy-ch[cn].y)<2) {
			if (ticker-dat->lastfight>TICKS*5) {
				dat->doscout=0;
				if (dat->patrolx[0]) {					
					dat->docheck=1;
					dat->pat=0;
				}
				say(cn,"Nashterk'ka?");
			}
			
                        do_idle(cn,TICKS/2);
			return;
		} else {
			if (move_driver(cn,dat->dox,dat->doy,1)) return;
			if (tmove_driver(cn,dat->dox,dat->doy,1)) return;

			do_idle(cn,TICKS/2);
			return;
		}
	}

	if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

	if (dat->line) {
		m=ch[cn].x+ch[cn].y*MAXMAP;

		if ((map[m].gsprite>>16)==51050 && (map[m+1].gsprite>>16)==51052) dat->line=DX_RIGHT;
		if ((map[m].gsprite>>16)==51050 && (map[m-1].gsprite>>16)==51052) dat->line=DX_LEFT;
		if ((map[m].gsprite>>16)==51050 && (map[m+MAXMAP].gsprite>>16)==51051) dat->line=DX_DOWN;
		if ((map[m].gsprite>>16)==51050 && (map[m-MAXMAP].gsprite>>16)==51051) dat->line=DX_UP;

		switch((map[m].gsprite>>16)) {
			case 51050:
			case 51051:
			case 51052:	if (do_walk(cn,dat->line)) return;
			default:	if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;
		}
	}

        if (dat->patrol || dat->docheck) {
		if (abs(dat->patrolx[dat->pat]-ch[cn].x)<2 && abs(dat->patroly[dat->pat]-ch[cn].y)<2) {
			dat->pat++;
			if (dat->pat==MAXPAT || !dat->patrolx[dat->pat]) dat->pat=dat->docheck=0;
			if (dat->docheck) say(cn,"Nashterk'ka?");			
		}
                if (move_driver(cn,dat->patrolx[dat->pat],dat->patroly[dat->pat],1)) return;
		if (tmove_driver(cn,dat->patrolx[dat->pat],dat->patroly[dat->pat],1)) return;		
	} else {
		if (abs(ch[cn].x-ch[cn].tmpx)>0 || abs(ch[cn].y-ch[cn].tmpy)>0) {
			if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;
			if (tmove_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;			
		}
	}

        //say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}

void palace_bomb(int in,int cn)
{
	int fn,m,co;

	if (cn) {
		if (it[in].drdata[0]==2) {
			//log_char(cn,LOG_SYSTEM,0,"boom");
			
			fn=create_explosion(8,50050);
			add_explosion(fn,it[in].x,it[in].y);
                        sound_area(it[in].x,it[in].y,6);
			
			m=it[in].x+it[in].y*MAXMAP;
			if ((co=map[m].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m+1].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m-1].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m+MAXMAP].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m-MAXMAP].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m+1+MAXMAP].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m+1-MAXMAP].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m-1+MAXMAP].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			if ((co=map[m-1-MAXMAP].ch) && (!(ch[co].flags&CF_PLAYER) || *(unsigned long*)(it[in].drdata+1)==ch[co].ID) && strcmp(ch[co].name,"Islena")) create_show_effect(EF_BURN,co,ticker,ticker+TICKS*60,250,POWERSCALE*2);
			remove_item(in);
			destroy_item(in);
		} else if (it[in].drdata[0]==1 && it[in].carried) {
			it[in].drdata[0]=0;
			it[in].sprite--;
			ch[cn].flags|=CF_ITEMS;
		} else if (it[in].drdata[0]==0 && it[in].carried) {
			it[in].drdata[0]=1;
			it[in].sprite++;
			ch[cn].flags|=CF_ITEMS;
			*(unsigned long*)(it[in].drdata+1)=ch[cn].ID;
		}
                return;
	}
	
	if (!it[in].carried && it[in].drdata[0]==1) {
		it[in].drdata[0]=2;
		it[in].sprite++;
		it[in].flags|=IF_STEPACTION;
		it[in].flags&=~(IF_TAKE|IF_USE);
		set_sector(it[in].x,it[in].y);
	}
	call_item(it[in].driver,in,0,ticker+TICKS*5);
}

void palace_cap(int in,int cn)
{
	int n,fn;

        if (cn) return;

	call_item(it[in].driver,in,0,ticker+TICKS/4);
	
	if (!(cn=it[in].carried)) return;
	if (ch[cn].item[WN_HEAD]!=in) return;
	
	if (it[in].drdata[0] && ticker<ch[cn].regen_ticker+REGEN_TIME) {
		it[in].drdata[0]=0;
		it[in].sprite--;
		ch[cn].flags|=CF_ITEMS;		
	}
	if (ticker>ch[cn].regen_ticker+REGEN_TIME && ch[cn].action==AC_IDLE) {
		if (!it[in].drdata[0]) {
			it[in].drdata[0]=1;
			it[in].sprite++;
			ch[cn].flags|=CF_ITEMS;
		}
		
		for (n=0; n<4; n++) {
			if (!(fn=ch[cn].ef[n])) continue;
			if (ef[fn].type==EF_CAP) break;
		}
		if (n==4) create_show_effect(EF_CAP,cn,ticker,ticker+TICKS/4+1,0,1);
		else ef[fn].stop=ticker+TICKS/4+1;		
	}
}

void palace_door(int in,int cn)
{
	if (!cn) {
		if (it[in].drdata[1]==1) {
			if (!(map[it[in].x+it[in].y*MAXMAP].flags&MF_TMOVEBLOCK)) {
				map[it[in].x+it[in].y*MAXMAP].flags|=MF_TMOVEBLOCK;
				it[in].drdata[1]=2;
			}
			call_item(it[in].driver,in,cn,ticker+3);
		} else if (it[in].drdata[1]==2) {
                        it[in].drdata[0]--;
			it[in].sprite=15196+it[in].drdata[0];
			set_sector(it[in].x,it[in].y);
			if (it[in].drdata[0]) call_item(it[in].driver,in,0,ticker+3);
			else {
				it[in].drdata[0]=it[in].drdata[1]=0;
			}
		} else if (it[in].drdata[1]==3) {
			it[in].drdata[0]++;
			it[in].sprite=15196+it[in].drdata[0];
			set_sector(it[in].x,it[in].y);
			if (it[in].drdata[0]<15) call_item(it[in].driver,in,0,ticker+3);
			else {
				it[in].drdata[1]=1;
				call_item(it[in].driver,in,0,ticker+TICKS*10);
				map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TMOVEBLOCK;
			}
		}
		return;
	}
	if (it[in].drdata[1]) return;

	if (!has_item(cn,IID_AREA11_PALACEKEY)) {
		log_char(cn,LOG_SYSTEM,0,"You need a key to open this gate.");
		return;
	}

	it[in].drdata[1]=3;
	call_item(it[in].driver,in,0,ticker+2);
}


void islena_door(int in,int cn)
{
	int x,y,co,islena=0,in2;

	if (!cn) return;

        if (ch[cn].x==144 && ch[cn].y==56) {
		teleport_char_driver(cn,144,58);
		return;
	}
	
	for (x=138; x<147; x++) {
		for (y=49; y<58; y++) {
                        if ((co=map[x+y*MAXMAP].ch)) {
                                if ((ch[co].flags&CF_PLAYER)) {
					log_char(cn,LOG_SYSTEM,0,"You hear fighting behind the door. It seems Islena is killing somebody else at the moment. Please come back later so she can take care of you, too.");
					return;
				}
                                if (ch[co].driver==CDR_PALACEISLENA) islena=co;
			}
			if ((in2=map[x+y*MAXMAP].it)) {
                                if ((it[in2].flags&IF_TAKE)) {
					log_char(cn,LOG_SYSTEM,0,"It seems Islena is tidying up her room. You decide to wait till she is done.");
					return;
				}
			}
		}
	}
	if (!islena) {
		log_char(cn,LOG_SYSTEM,0,"Islena is being re-incarnated. Please try again soon.");
		return;
	}
	if (ch[islena].hp<ch[islena].value[0][V_HP]*POWERSCALE || ch[islena].mana<ch[islena].value[0][V_MANA]*POWERSCALE) {
		log_char(cn,LOG_SYSTEM,0,"Islena is resting after killing your predecessor. Being well mannered, you wait for her.");
		return;
	}

	teleport_char_driver(cn,143,55);
}

#define MAXTAKE	20
static int jx,jy;
struct islena_ppd
{
	int islena_state;
};

struct islena_data
{
	int last_talk;

	int last_hurt_time;
	int last_hurt_by;
	int last_power_msg;
	int take[MAXTAKE];
};

int takeadd(struct islena_data *dat,int in)
{
	int n;

	for (n=0; n<MAXTAKE; n++) {
		if (dat->take[n]==in) return 0;
		if (!dat->take[n]) {
			dat->take[n]=in;
			return 1;
		}
	}
	return 0;
}
int takecmp(const void *a,const void *b)
{
	int in1,in2;

	in1=*(int*)(a); in2=*(int*)(b);

	if (in1 && !in2) return -1;
	if (!in1 && in2) return 1;
	if (!in1 && !in2) return 0;

	if (it[in1].x && !it[in2].x) return 1;
	if (!it[in1].x && it[in2].x) return -1;

	return (abs(it[in1].x-jx)+abs(it[in1].y-jy))-(abs(it[in2].x-jx)+abs(it[in2].y-jy));
}

void palace_islena(int cn,int ret,int lastact)
{
	struct islena_data *dat;
	struct islena_ppd *ppd;
	struct msg *msg,*next;
        int co,in;
	int didsay=0,talkdir=0,washit=0;

	dat=set_data(cn,DRD_ISLENA,sizeof(struct islena_data));
	if (!dat) return;	// oops...
	jx=ch[cn].x; jy=ch[cn].y;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
			fight_driver_set_dist(cn,20,0,30);
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
                        co=msg->dat1;

                        if ((ch[co].flags&CF_PLAYER) && char_see_char(cn,co) && (ppd=set_data(co,DRD_ISLENA_PPD,sizeof(struct islena_ppd)))) {
				//ppd->islena_state=0;

                                if (ppd->islena_state>=10) {
					fight_driver_add_enemy(cn,co,1,1);
					remove_message(cn,msg);
					continue;
				}

				if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

                                switch(ppd->islena_state) {
					case 0:		say(cn,"Ah, %s. I have heard of thee. Thou hast quite a reputation among my minions.",ch[co].name);
							ppd->islena_state++; didsay=1;
							break;
					case 1:		say(cn,"Thou hast been led by the nose by Ishtar, %s. I was quite content living here among my creatures, but Ishtar had to stir up hatred among the human population against me.",ch[co].name);
							ppd->islena_state++; didsay=1;
							break;
					case 2:		say(cn,"It was never my wish to continue the war of the last eon. So why don't we set the enmity aside? I will forgive thee all the trouble thou hast caused me.");
							ppd->islena_state++; didsay=1;
							break;
					case 3:		say(cn,"None of us must die today if thou wilt just leave me be, %s.",ch[co].name);
							ppd->islena_state++; didsay=1;
							break;
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				}
			}
		}

		// got a NPC message?
		if (msg->type==NT_NPC) {			
		}

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}
		
		if (((msg->type==NT_GOTHIT) || (msg->type==NT_SPELL && msg->dat2==V_FREEZE)) && (co=msg->dat1)) {
			if (ch[co].flags&CF_PLAYER) {
				washit=1;
				if ((ppd=set_data(co,DRD_ISLENA_PPD,sizeof(struct islena_ppd)))) {
					if (ppd->islena_state<10) say(cn,"Thou wilt not hear? So be it. This is thy death!");
					ppd->islena_state=10;
				}
				if (ticker-dat->last_hurt_time>TICKS*30 || dat->last_hurt_by==ch[co].ID) {
					dat->last_hurt_time=ticker;
					dat->last_hurt_by=ch[co].ID;
				} else {
					if (ticker-dat->last_power_msg>TICKS*15) {
						say(cn,"I call on thee, the Power of Two! Save me from this treacherous attack!");
					}
					ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
					ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
					ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
					ch[cn].lifeshield=ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE/MAGICSHIELDMOD;
					dat->last_power_msg=ticker;
				}
			}
		}

		if (msg->type==NT_ITEM) {
                        in=msg->dat1;

			if (it[in].flags&IF_TAKE) {
				takeadd(dat,in);
			}
		}

                standard_message_driver(cn,msg,0,0);

                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
	if (ch[cn].citem) {
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) {
		//say(cn,"action=%d, washit=%d",ch[cn].action,washit);
		if (washit && ch[cn].action==AC_IDLE) {
			if (ticker-dat->last_power_msg>TICKS*15) {
				say(cn,"I call on thee, the Power of none! Save me from this treacherous attack!");
			}
			ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
			ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
			ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
			ch[cn].lifeshield=ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE/MAGICSHIELDMOD;
			dat->last_power_msg=ticker;
		}
		return;
	}
	if (fight_driver_follow_invisible(cn)) { return; }
	
	if (washit) {
		if (ticker-dat->last_power_msg>TICKS*15) {
			say(cn,"I call on thee, the Power of None! Save me from this treacherous attack!");
		}
		ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
		ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
		ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
		ch[cn].lifeshield=ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE/MAGICSHIELDMOD;
		dat->last_power_msg=ticker;
	}

	if (!ch[cn].citem) {
		qsort(dat->take,MAXTAKE,sizeof(int),takecmp);
		in=dat->take[0];
		if (take_driver(cn,in)) return;
                else dat->take[0]=0;
	}

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void islena_dead(int cn,int co)
{
	char buf[256];

	if (!co) return;

	if (ch[co].flags&CF_WON) {
		say(cn,"Why again? Why? Thou shalt die with me!");
		hurt(co,1500*POWERSCALE,cn,1,95,95);
	} else {
		ch[co].flags|=CF_WON;
		reset_name(co);
		say(cn,"So it must end? Why me?");		
		log_char(co,LOG_SYSTEM,0,"From now on, thou shalt be known as %s %s. Thou hast slain Islena and the purpose of thy days is fulfilled. All shall admire thine persistence, braveness and power. But still, some doubts remain. How can a mere human succeed where Ishtar failed?",Sirname(co),ch[co].name);

		sprintf(buf,"0000000000°c10Grats: %s is a %s now!",ch[co].name,Sirname(co));
		server_chat(6,buf);
	}
}

#include "ice_shared.c"

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_PALACEGUARD:	palace_guard(cn,ret,lastact); return 1;
		case CDR_PALACEISLENA:	palace_islena(cn,ret,lastact); return 1;

                default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_ITEMSPAWN:	itemspawn(in,cn); return 1;
		case IDR_MELTINGKEY:	meltingkey(in,cn); return 1;
		case IDR_WARMFIRE:	warmfire(in,cn); return 1;
		case IDR_BACKTOFIRE:	backtofire(in,cn); return 1;
		case IDR_PALACEBOMB:	palace_bomb(in,cn); return 1;
		case IDR_PALACECAP:	palace_cap(in,cn); return 1;
                case IDR_FREAKDOOR:	freakdoor(in,cn); return 1;
		case IDR_PALACEDOOR:	palace_door(in,cn); return 1;
		case IDR_ISLENADOOR:	islena_door(in,cn); return 1;

		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_PALACEGUARD:	return 1;
		case CDR_PALACEISLENA:	islena_dead(cn,co); return 1;

                default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_PALACEGUARD:	return 1;
		case CDR_PALACEISLENA:	return 1;

                default:		return 0;
	}
}












