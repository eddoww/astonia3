/*
 * $Id: two.c,v 1.4 2007/08/13 18:50:38 devel Exp $
 *
 * $Log: two.c,v $
 * Revision 1.4  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.3  2006/10/04 17:30:57  devel
 * open quests higher than the re-opened one from the thief guild series will now close if still open
 *
 * Revision 1.2  2006/09/25 14:08:51  devel
 * added questlog to forest and exkordon
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
#include "store.h"
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
#include "bank.h"
#include "player_driver.h"
#include "consistency.h"
#include "questlog.h"
#include "two_ppd.h"

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

//-----------------------

struct qa
{
	char *word[20];
	char *answer;
	int answer_code;
};

struct qa qa[]={
	{{"how","are","you",NULL},"I'm fine!",0},
	{{"hello",NULL},"Hello, %s!",0},
	{{"hi",NULL},"Hi, %s!",0},
	{{"greetings",NULL},"Greetings, %s!",0},
	{{"hail",NULL},"And hail to you, %s!",0},
        {{"what's","up",NULL},"Everything that isn't nailed down.",0},
	{{"what","is","up",NULL},"Everything that isn't nailed down.",0},
	{{"repeat",NULL},NULL,2},
	{{"guest",NULL},NULL,5},
	{{"citizen",NULL},NULL,6},
	{{"honor",NULL},NULL,7},
	{{"enemy",NULL},NULL,12},
	{{"chat",NULL},NULL,8},
	{{"bribe",NULL},NULL,9},
	{{"threaten",NULL},NULL,10},
	{{"pay","bribe",NULL},NULL,11},
	{{"pay",NULL},NULL,3},
	{{"buy","pass",NULL},NULL,13},
	{{"status",NULL},NULL,14},
	{{"pay","a","fee",NULL},NULL,15},
	{{"i","am","done",NULL},NULL,16}
};

void lowerstrcpy(char *dst,char *src)
{
	while (*src) *dst++=tolower(*src++);
	*dst=0;
}

int analyse_text_driver(int cn,int type,char *text,int co)
{
	char word[256];
	char wordlist[20][256];
	int n,w,q,name=0;

	// ignore game messages
	if (type==LOG_SYSTEM || type==LOG_INFO) return 0;

	// ignore our own talk
	if (cn==co) return 0;

	if (!(ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE))) return 0;

	//if (char_dist(cn,co)>16) return 0;

	if (!char_see_char(cn,co)) return 0;

	while (isalpha(*text)) text++;
	while (isspace(*text)) text++;
	while (isalpha(*text)) text++;
	if (*text==':') text++;
	while (isspace(*text)) text++;
	if (*text=='"') text++;
	
	n=w=0;
	while (*text) {
		switch (*text) {
			case ' ':
			case ',':
			case ':':
			case '?':
			case '!':
			case '"':
			case '.':       if (n) {
						word[n]=0;
						lowerstrcpy(wordlist[w],word);
						if (strcasecmp(wordlist[w],ch[cn].name)) { if (w<20) w++; }
						else name=1;
					}					
					n=0; text++;
					break;
			default: 	word[n++]=*text++;
					if (n>250) return 0;
					break;
		}		
	}

	if (w) {
		for (q=0; q<sizeof(qa)/sizeof(struct qa); q++) {
			for (n=0; n<w && qa[q].word[n]; n++) {
				//say(cn,"word = '%s'",wordlist[n]);
				if (strcmp(wordlist[n],qa[q].word[n])) break;			
			}
			if (n==w && !qa[q].word[n]) {
				if (qa[q].answer) say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}

void call_guard(int cn,int co)
{
	int dist,bestdist=9999,bestcc=0,level,cc;

	level=max(ch[cn].level,ch[co].level);

	for (cc=getfirst_char(); cc; cc=getnext_char(cc)) {
		if (cc!=cn && ch[cc].group==ch[cn].group && ch[cc].level>level) {
			dist=abs(ch[cn].x-ch[cc].x)+abs(ch[cn].y-ch[cc].y)+abs(ch[cc].level-level);
			if (dist<bestdist) {
				bestdist=dist;
				bestcc=cc;
			}
		}
	}
	//say(cn,"best is %s (%d,%d)",ch[bestcc].name,ch[bestcc].x,ch[bestcc].y);
	if (bestcc) notify_char(bestcc,NT_NPC,NTID_TWOCITY,ch[co].level,ch[cn].x+ch[co].y*MAXMAP);
}

struct places
{
	int fx,fy;
	int tx,ty;
	int level;
};

struct places places[]={
	// palace
	{  1,  3,	 15, 15,	4},
        { 15,  7,	 21, 15,	4},
	{  1,  1,	 59, 37,	3},
	// shop
	{ 56, 57,	 63, 63,	4},
	// servant 5 house
	{ 73, 34,	 79, 46,	4},
	// whole city
	{  1,  1, 	255,100,	1}

};

int illegal_place(int x,int y)
{
	int n;

	for (n=0; n<ARRAYSIZE(places); n++) {
		if (places[n].fx<=x && places[n].fy<=y && places[n].tx>=x && places[n].ty>=y) return places[n].level;
	}

	return 0;
}

#define LS_CLEAN	0
#define LS_FINE		1
#define LS_DEAD		2

#define CS_ENEMY	0
#define CS_GUEST	1
#define CS_CITIZEN	2
#define CS_HONOR	3

#define MAXPAT	8
struct twoguard_data
{
	int current_victim;
	int victim_timeout;
	int fine_state;
	int fine_timeout;
	int lastsay;
	int tx,ty;
	int lastalert;
	int good_tx_try;	
	int nofight_timer;

	int patx[MAXPAT],paty[MAXPAT];
	int pi;

	int last_x,last_y,last_co;
	int leave_timeout;
	int leave_state;

	int busy;
};

void guard_parse(int cn,struct twoguard_data *dat)
{
	char *ptr,name[64],value[64];
	int pat=0;

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"patx")) { if (pat>=MAXPAT) elog("twoguard_parse: too many patrol stops"); else dat->patx[pat]=atoi(value); }
		else if (!strcmp(name,"paty")) { if (pat>=MAXPAT) elog("twoguard_parse: too many patrol stops"); else dat->paty[pat++]=atoi(value); }
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void guard_driver(int cn,int ret,int lastact)
{
	struct twoguard_data *dat;
	struct twocity_ppd *ppd;
	struct bank_ppd *bank;
        struct msg *msg,*next;
	int cc,co,dir,in,light,on=0,off=0,tlight,place;

        dat=set_data(cn,DRD_TWOGUARDDRIVER,sizeof(struct twoguard_data));
	if (!dat) return;	// oops...

	in=ch[cn].item[WN_LHAND];
	if (!in) {
		in=ch[cn].item[WN_LHAND]=create_item("torch");
		it[in].carried=cn;		
	}
	
	//if (it[in].drdata[0]) tlight=it[in].mod_value[0];
	//else tlight=0;
	tlight=ch[cn].value[0][V_LIGHT];
	
	light=check_dlight(ch[cn].x,ch[cn].y);
        if (light<40) on++;
	if (light>50) off++;
	
	light=check_light(ch[cn].x,ch[cn].y);
	if (light<10) on++;
	if (light-tlight>10) off++;

	//say(cn,"on=%d, off=%d,light=%d, tlight=%d",on,off,light,tlight);
	if (!it[in].drdata[0] && on==2) { use_item(cn,in); /* say(cn,"on! (%d)",cn); */ }
	if (it[in].drdata[0] && off) { use_item(cn,in); /* say(cn,"off! (%d)",cn); */ }
	

	if (it[in].drdata[0]) ch[cn].sprite=317;
	else ch[cn].sprite=318;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
                        if (ch[cn].arg) { guard_parse(cn,dat); ch[cn].arg=NULL; }
                }

		if (msg->type==NT_CHAR) {
			co=msg->dat1;

                        if (!dat->busy &&
			    (ch[co].flags&CF_PLAYER) &&
			    (!dat->current_victim || dat->current_victim==co) &&
			    (ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd))) &&
			    realtime-ppd->last_attack>2 &&
			    (!ppd->current_guard || ppd->current_guard==cn || realtime-ppd->current_guard_time>3) &&
			    char_see_char(cn,co) &&
			    (place=illegal_place(ch[co].x,ch[co].y))) {
				//say(cn,"%d - %d",illegal_place(ch[co].x,ch[co].y),ppd->citizen_status);
				if (ppd->legal_status==LS_DEAD) {
					dat->current_victim=co;
					dat->victim_timeout=ticker;
					fight_driver_add_enemy(cn,co,1,1);
					dat->last_x=ch[co].x;
					dat->last_y=ch[co].y;
					dat->last_co=co;
				} else if (place>ppd->citizen_status) {
					dat->current_victim=co;
					dat->victim_timeout=ticker;
					ppd->current_guard=cn;
					ppd->current_guard_time=realtime;
					dat->last_x=ch[co].x;
					dat->last_y=ch[co].y;
					dat->last_co=co;
					if (dat->leave_state==0) {
						say(cn,"Hey! %s! You have no business in there! Get out at once!",ch[co].name);
						dat->leave_state=1;
						dat->leave_timeout=ticker;
						dat->lastsay=ticker;
						dir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
						turn(cn,dir);
					}
					if (dat->leave_state==1) {
						int timeout;
						if (place==CS_GUEST) timeout=TICKS*60;
						else timeout=TICKS*30;

                                                if (dat->lastsay+TICKS*10<ticker) {
							say(cn,"Get out, %s, or I'll have to kill you!",ch[co].name);
							dat->lastsay=ticker;
						}
						if (dat->leave_timeout+timeout<ticker) {
							dat->leave_state=2;
							say(cn,"You had ample time to leave, now you die!");
						} else if (move_driver(cn,ch[co].x,ch[co].y,2)) { remove_message(cn,msg); return; }
					}
					if (dat->leave_state==2) {
						fight_driver_add_enemy(cn,co,1,1);
						ppd->last_attack=realtime;
					}					
				} else if (ppd->legal_status==LS_CLEAN) {
					if (ppd->guard_intro==0 && char_dist(cn,co)<16) {
						say(cn,"Listen carefully, %s. Thou art here on a guest pass. Any crime here, and thou wilt lose the right to enter our city.",ch[co].name);
						ppd->guard_intro=1;
					}
				} else if (ppd->legal_status==LS_FINE) {
					dat->current_victim=co;
					dat->victim_timeout=ticker;
					ppd->current_guard=cn;
					ppd->current_guard_time=realtime;
					dat->last_x=ch[co].x;
					dat->last_y=ch[co].y;
					dat->last_co=co;
					if (dat->fine_state==0) {
						say(cn,"Hey, %s, you owe the city %.2fG! Say °c4pay°c0 to pay it!",ch[co].name,ppd->legal_fine/100.0);
                                                dat->fine_state=1;
						dat->fine_timeout=ticker;
						dat->lastsay=ticker;
                                                dir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
						turn(cn,dir);
					}
					if (dat->fine_state==1) {
						if (dat->lastsay+TICKS*15<ticker) {
							say(cn,"Come on, %s, °c4pay°c0 or I'll have to kill you!",ch[co].name);
							dat->lastsay=ticker;
						}
                                                if (dat->fine_timeout+TICKS*60<ticker) {
							dat->fine_state=2;
							say(cn,"You had ample time to pay, now you die!");
						} else if (move_driver(cn,ch[co].x,ch[co].y,1)) { remove_message(cn,msg); return; }

						dir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
						turn(cn,dir);
					}
					if (dat->fine_state==2) {						
						fight_driver_add_enemy(cn,co,1,1);
						ppd->last_attack=realtime;
					}
				}
			}
		}

                if (msg->type==NT_TEXT) {
			co=msg->dat3;

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (!ppd) break;
						ppd->guard_intro=0;
						break;
				case 3:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (!ppd) break;
                                                if (ppd->legal_status==LS_FINE) {
							if (ch[co].gold>=ppd->legal_fine) {
								
								say(cn,"Wise choice, %s.",ch[co].name);
								
								ch[co].gold-=ppd->legal_fine;
								ch[co].flags|=CF_ITEMS;
								ppd->legal_status=LS_CLEAN;
								ppd->legal_fine=0;
							} else {
								bank=set_data(co,DRD_BANK_PPD,sizeof(struct bank_ppd));
								if (bank) {
									int need;

                                                                        need=ppd->legal_fine;
									need-=ch[co].gold;
									if (need<=bank->imperial_gold) {
										
										say(cn,"Wise choice, %s (took %.2fG from bank account).",ch[co].name,need/100.0);

										bank->imperial_gold-=need;
										ch[co].gold=0;
										ch[co].flags|=CF_ITEMS;

                                                                                ppd->legal_status=LS_CLEAN;
										ppd->legal_fine=0;										
									} else say(cn,"Gosh, %s's broke. Well, %s'll die then.",hename(co),hename(co));
								}
							}
						}
						if (ppd->legal_status==LS_CLEAN && illegal_place(ch[co].x,ch[co].y)<=ppd->citizen_status) {
							fight_driver_remove_enemy(cn,co);
							dat->nofight_timer=ticker;
							player_driver_stop(ch[co].player,1);
						}
						break;
				case 12:	ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && (ch[co].flags&CF_GOD)) {
							ppd->citizen_status=CS_ENEMY;
						}
						break;
				case 5:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && (ch[co].flags&CF_GOD)) {
							ppd->citizen_status=CS_GUEST;
							ppd->legal_status=LS_CLEAN;
							ppd->legal_fine=0;
						}
						break;
				case 6:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && (ch[co].flags&CF_GOD)) {
							ppd->citizen_status=CS_CITIZEN;
						}
						break;
				case 7:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && (ch[co].flags&CF_GOD)) {
							ppd->citizen_status=CS_HONOR;
						}
						break;
			}
			tabunga(cn,co,(char*)msg->dat2);
		}
		if (msg->type==NT_GOTHIT) {
                        co=msg->dat1;
                        if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 && (!dat->lastalert || ticker-dat->lastalert>TICKS*30)) {
				say(cn,"Help! Officer under attack!");
				call_guard(cn,co);
				dat->lastalert=ticker;
			}
			if ((ch[co].flags&CF_PLAYER) && (ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd)))) {
				if (ppd->legal_status!=LS_DEAD && realtime-ppd->last_attack>10) {
					ppd->legal_status=LS_FINE;
					ppd->legal_fine+=2000;
					if (ppd->citizen_status==CS_GUEST) {
						say(cn,"We do not allow strangers to commit any crime here. Leave at once!");
						ppd->citizen_status=CS_ENEMY;
					} else say(cn,"Hey %s! Fine for attacking a city guard: 20G! Say °c4pay°c0 to pay it!",ch[co].name);
					//charlog(cn,"fine for %s (1): 20G",ch[co].name);
				}
				ppd->last_attack=realtime;
			}
		}
		if (msg->type==NT_SEEHIT) {
			cc=msg->dat1;
			co=msg->dat2;
			if (co!=cn && ch[co].group==ch[cn].group && (ch[cc].flags&CF_PLAYER) && (ppd=set_data(cc,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd))) && char_see_char(cn,cc)) {
				if (ppd->legal_status!=LS_DEAD && realtime-ppd->last_attack>10) {
					ppd->legal_status=LS_FINE;
					ppd->legal_fine+=7500;
					if (ppd->citizen_status==CS_GUEST) {
						say(cn,"Protect the innocent! We do not allow strangers to commit any crime here. Leave at once!");
						ppd->citizen_status=CS_ENEMY;
					} else say(cn,"Protect the innocent! Stop at once and °c4pay°c0 your fine, %s!",ch[cc].name);
					//charlog(cn,"fine for %s (2): 75G",ch[cc].name);
				}
                                if  (ticker-dat->nofight_timer>TICKS*3) fight_driver_add_enemy(cn,cc,1,1);
				ppd->last_attack=realtime;
			}

		}

		if (msg->type==NT_NPC) {
			if (msg->dat1==NTID_TWOCITY) {
                                dat->tx=msg->dat3%MAXMAP;
				dat->ty=msg->dat3/MAXMAP;
				dat->good_tx_try=ticker;
				//charlog(cn,"got called to %d,%d",dat->tx,dat->ty);
			}
			if (msg->dat1==NTID_TWOCITY_PICK) {
				co=msg->dat2;
				if ((ch[co].flags&CF_PLAYER) && char_see_char(cn,co) && (ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd))) && realtime-ppd->last_attack>60) {
					if (illegal_place(ch[co].x,ch[co].y)>ppd->citizen_status) {
						say(cn,"Thou shalt not steal! Now thou wilt die!");
						fight_driver_add_enemy(cn,co,1,1);
						ppd->last_attack=realtime;
					}
					if (ppd->legal_status!=LS_DEAD) {
						ppd->legal_status=LS_FINE;
						ppd->legal_fine+=3000;
						if (ppd->citizen_status==CS_GUEST) {
							say(cn,"Hey! Stop thief! We do not allow strangers to commit any crime here. Leave at once!");
							ppd->citizen_status=CS_ENEMY;
						} else {
							say(cn,"Hey! Stop thief! Fine for breaking a lock: 30G.");
							ppd->last_attack=realtime;
						}
						//charlog(cn,"fine for %s (3): 30G",ch[co].name);
					}					
				}
			}
		}

                if (ticker-dat->nofight_timer>TICKS*3) standard_message_driver(cn,msg,0,0);
		remove_message(cn,msg);
	}

        if (dat->victim_timeout+TICKS*3<ticker) dat->victim_timeout=dat->current_victim=dat->fine_state=dat->leave_state=dat->leave_timeout=0;

        fight_driver_update(cn);
	
	if (fight_driver_attack_visible(cn,0)) { dat->busy=1; return; }
	if (fight_driver_follow_invisible(cn)) { dat->busy=1; return; }
	
        if (spell_self_driver(cn)) { dat->busy=1; return; }
	if (regenerate_driver(cn)) { dat->busy=1; return; }

	dat->busy=0;

	if (dat->current_victim) {
		if (dat->last_co==dat->current_victim && !char_see_char(cn,dat->last_co) && move_driver(cn,dat->last_x,dat->last_y,1)) return;
                do_idle(cn,TICKS/2);
		return;
	}

	if (dat->tx) {
                if (abs(dat->tx-ch[cn].x)+abs(dat->ty-ch[cn].y)<2) dat->tx=dat->ty=0;
		else if (swap_move_driver(cn,dat->tx,dat->ty,1)) { dat->good_tx_try=ticker; return; }
                else if (swap_move_driver(cn,dat->tx,dat->ty,3)) { dat->good_tx_try=ticker; return; }
		else if (ticker-dat->good_tx_try>TICKS*10) { dat->tx=dat->ty=0; }
	}

        if (dat->patx[0]) {
                if (abs(dat->patx[dat->pi]-ch[cn].x)<4 && abs(dat->paty[dat->pi]-ch[cn].y)<4) dat->pi++;
                if (!dat->patx[dat->pi]) dat->pi=0;

		if (swap_move_driver(cn,dat->patx[dat->pi],dat->paty[dat->pi],3)) return;		
		
		dat->pi++;
                if (!dat->patx[dat->pi]) dat->pi=0;
	}

	if (ch[cn].tmpx-ch[cn].x==0 && ch[cn].tmpy-ch[cn].y==0) { do_idle(cn,TICKS); }
	else if (swap_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;
	else if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	do_idle(cn,TICKS);

}

void guard_dead(int cn,int co)
{
        struct twocity_ppd *ppd;

	if (!co) return;
	if (!(ch[co].flags&CF_PLAYER)) return;	
	
        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	if (!ppd) return;

	say(cn,"Thou shalt be punished for this misdeed, %s.",ch[co].name);
	if (ppd->legal_status==LS_DEAD) return;
	ppd->legal_status=LS_FINE;
	ppd->legal_fine+=5000;
	if (ppd->citizen_status==CS_GUEST) ppd->citizen_status=CS_ENEMY;
	//charlog(cn,"fine for %s (4): 50G",ch[co].name);
}

struct barkeeper_data
{
        int last_talk;
	int current_victim;		
};	

void barkeeper(int cn,int ret,int lastact)
{
	struct barkeeper_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0,cost;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_BARKEEPERDRIVER,sizeof(struct barkeeper_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

                        // only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                        if (ppd) {
				switch(ppd->barkeeper_state) {
					case 0:		say(cn,"Hello, %s. Welcome to the tavern of the Two Towns.",ch[co].name);
							ppd->barkeeper_state++; didsay=1;
							break;
					case 1:		if (ppd->citizen_status<CS_GUEST || ppd->legal_status==LS_DEAD) {
								if (ppd->legal_status==LS_FINE) {
									say(cn,"If thou needst go into Exkordon, I can help thee. Wouldst thou like to buy a guest pass? (°c4buy pass°c0 for 150G and pay %dG fines, for a total of %dG)",ppd->legal_fine/100,ppd->legal_fine/100+150);
								} else if (ppd->legal_status==LS_DEAD) {
									say(cn,"If thou needst go into Exkordon, I can help thee. But since thou hast killed the governor's double, it will be expensive. Wouldst thou like to buy a guest pass and the guard's forgiveness? (°c4buy pass°c0 for 2500G)");
								} else say(cn,"If thou needst go into Exkordon, I can help thee. Wouldst thou like to buy a guest pass? (°c4buy pass°c0 for 150G)");
								ppd->barkeeper_state++; didsay=1;
								ppd->barkeeper_last=realtime;
							}
							break;					
					case 2:		if (realtime-ppd->barkeeper_last>60*10) ppd->barkeeper_state=2;
							break;
				}
                                if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && ppd->barkeeper_state<=2) { dat->last_talk=0; ppd->barkeeper_state=0; }
						break;
				case 13:	ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd->citizen_status<CS_GUEST || ppd->legal_status==LS_DEAD) {
							if (ppd->legal_status==LS_FINE) cost=15000+ppd->legal_fine;
							else if (ppd->legal_status==LS_DEAD) cost=250000;
							else cost=15000;
							
							if (take_money(co,cost)) {
								say(cn,"Thou canst now enter Exkordon, %s. But do be careful there, they are most strict with their laws.",ch[co].name);
								ppd->citizen_status=CS_GUEST;
								ppd->legal_status=LS_CLEAN;
								ppd->legal_fine=0;
							} else say(cn,"Thou dost not have enough money.");
						} else say(cn,"But thou hast a pass already.");
						break;
			}

			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
                                // let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;				
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;	
	}

        do_idle(cn,TICKS);
}

struct servant_data
{
        int last_talk;
	int current_victim;		
	int current_state;
	int nr;
	int lastalert;
};

void servant_parse(int cn,struct servant_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"nr")) dat->nr=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}


void servant(int cn,int ret,int lastact)
{
	struct servant_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SERVANTDRIVER,sizeof(struct servant_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
				servant_parse(cn,dat);
				ch[cn].arg=NULL;
			}
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

                        // only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                        if (ppd) {
				if (dat->current_victim!=co) dat->current_state=0;
					switch(dat->current_state) {
						case 0:		if (dat->nr==4) {
									say(cn,"Now, what do we have here? I do not think thine presence here is appropriate. GUARDS!");
									call_guard(cn,co);
								} else say(cn,"Uh, hello, %s. Thou art not supposed to be here. (°c4chat°c0 °c4bribe°c0 °c4threaten°c0)",ch[co].name);
								dat->current_state++; didsay=1;
								break;
						case 1:		break;
					}
				
				if (illegal_place(ch[cn].x,ch[co].x)>ppd->citizen_status) {
				} else {
					switch(dat->current_state) {
						case 0:		say(cn,"My greetings, %s. How may I serve you? (°c4chat°c0 °c4bribe°c0 °c4threaten°c0)",ch[co].name);
								dat->current_state++; didsay=1;
								break;
						case 1:		break;
					}
				}
                                if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		//repeat
						dat->current_state=0;
						break;
				case 8:         //chat
						switch(dat->nr) {
							case 0:		say(cn,"I spend my days scrubbing pots and pans. Thou wouldst believe not how dirty they can get. Sometimes it takes me an hour to clean one of the pans.");
									didsay=1;
									break;
							case 1:		say(cn,"The governor, he is a cruel man. Do be very careful here in Exkordon.");
									didsay=1;
									break;
							case 2:		say(cn,"I am the governor's mistress. Oh, I wish I could leave him, oh, I wish.");
									didsay=1;
									break;
							case 3:		say(cn,"It looks like it'll rain soon, doesn't it? The farmers sure could use a good rain.");
									didsay=1;
									break;
							case 5:		if (hour>6 && hour<23) say(cn,"So nice of thee to come and visit, %s. Life has been dull, and thou art most interesting.",ch[co].name);
									else say(cn,"Why dost thou disturb me in the middle of the night, %s?",ch[co].name);
									didsay=1;
									break;
						}
						break;
				case 9:         // bribe
						switch(dat->nr) {
							case 0:		say(cn,"It is nice of thee to offer money, and I could use it, oh yes, I could, but I cannot give thee anything in return. (°c4pay bribe°c0 of 20G)");
									didsay=1;
									break;
							case 1:		say(cn,"Listen, %s, I know of a secret passage, which connects two store rooms. Thou couldst use it to avoid the guards. I even have the key, which unlocks this door. (°c4pay bribe°c0 of 50G)",ch[co].name);
									didsay=1;
									break;
							case 2:		if (ch[co].flags&CF_MALE) say(cn,"Thou art most handsome, %s. For a kiss, I would tell thee how thou canst reach the governor's private rooms through a secret passage. (°c4pay bribe°c0 - a kiss)",ch[co].name);
									else say(cn,"Thou darest offer me money? Thou art most common, wench.");
									didsay=1;
									break;
							case 3:		say(cn,"Well, there is something I could tell thee, %s, which thou mightst find worth thy money. (°c4pay bribe°c0 of 50G)",ch[co].name);
									didsay=1;
									break;
							case 5:		say(cn,"Ah, money. Money is always welcome! (°c4pay bribe°c0 of 20G)");
									didsay=1;
									break;
						}
						break;
				case 10:        // threaten
						switch(dat->nr) {
							case 0:		say(cn,"No, please, don't kill me! Please! Have mercy, I am just a poor scullery girl!");
									didsay=1;
									break;
							case 1:		say(cn,"I shall tell the guards about thee, %s. Now go.",ch[co].name);
									call_guard(cn,co);
									didsay=1;
									break;
							case 2:		if (ch[co].flags&CF_MALE) {
										say(cn,"Uh, thou likest it rough, don't thou, %s? Well, I do not.",ch[co].name);
										call_guard(cn,co);
									} else {
										say(cn,"Uh, thou seemest most determined, lady. I shall relent to thy wishes, then. There is a secret passage to the governors private rooms. It starts in the room behind the southern door leading north-west in the corridor in front of my room. Here's the key.");
										in=create_item("palace_key1");
										if (in && !give_char_item(co,in)) {
											destroy_item(in);
										}
									}
									didsay=1;
									break;
							case 3:		say(cn,"But, but... I'm just a cook. Why kill me? Please, have mercy!");
									didsay=1;
									break;
							case 5:		say(cn,"GUARDS!");
									call_guard(cn,co);
									didsay=1;
									break;
						}
						break;
				case 11:        // pay bribe
						switch(dat->nr) {
							case 0:		if (take_money(co,2000)) say(cn,"Ooh. I thank thee, noble %s, I thank thee! But wait. There is one thing I can tell thee: Avoid the governors study at all cost. It is behind the door leading south-east from the small hallway.",Sirname(co));
									else say(cn,"Oh, how mean! First thou offerest me money and now thou canst not pay!");
									didsay=1;
									break;
							case 1:		if (take_money(co,5000)) {
										say(cn,"The passage starts in the store room at the north-eastern end of the corridor in front of this room. Here's the key.");
										in=create_item("palace_key2");
										if (in && !give_char_item(co,in)) {
											destroy_item(in);
										}
									} else say(cn,"No money no key.");
									didsay=1;
									break;
							case 2:		if (ch[co].flags&CF_MALE) {
										say(cn,"Ooh, thou art so cute. I shall relent to thy wishes, then. There is a secret passage to the governors private rooms. It starts in the room behind the southern door leading north-west in the corridor in front of my room. Here's the key.");
										in=create_item("palace_key1");
										if (in && !give_char_item(co,in)) {
											destroy_item(in);
										}
									} else {
										say(cn,"Hu?");
									}
									didsay=1;
									break;
							case 3:		if (take_money(co,5000)) say(cn,"The governor, he likes to... Well... Eat strawberry pies.");
									else say(cn,"Uh, I'm afraid thou dost not have enough money.");
									didsay=1;
									break;
							case 5:		if (take_money(co,2000)) say(cn,"Gladly I accept thine noble gift, stranger. Didst thou know that there is a secret entrance to the palace in the sewers?");
									else say(cn,"Uh, I'm afraid thou dost not have enough money.");
									didsay=1;
									break;
						}
						break;
			}

			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
                                // let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;				
			}
		}
		if (msg->type==NT_GOTHIT) {
			co=msg->dat1;
			if (!dat->lastalert || ticker-dat->lastalert>TICKS*30) {
				say(cn,"Guards! HELP!");
				call_guard(cn,co);
				dat->lastalert=ticker;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;	
	}

        do_idle(cn,TICKS);
}

void servant_dead(int cn,int co)
{
	struct servant_data *dat;
	struct twocity_ppd *ppd;

	if (!co) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

	dat=set_data(cn,DRD_SERVANTDRIVER,sizeof(struct servant_data));
	if (dat && dat->nr==4) {
		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
		if (ppd) {
			ppd->citizen_status=CS_ENEMY;
			ppd->legal_status=LS_DEAD;
			say(cn,"Thou shalt pay dearly for this, %s. Even though I am just the governors double, he wilt have thine head just for trying to kill him!",ch[co].name);
		}
	} else say(cn,"Arrgh! GUARDS!");

	call_guard(cn,co);
}


int pick_door(int in,int cn)
{
	int m,in2;

	m=it[in].x+it[in].y*MAXMAP;

	if (!cn) {
		if (!it[in].drdata[0]) return 2; // already closed
		if ((map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m+1].ch || map[m-1].ch || map[m+MAXMAP].ch || map[m-MAXMAP].ch) {
			call_item(it[in].driver,in,0,ticker+TICKS);	// doorway is blocked
			return 2;
		}
		remove_lights(it[in].x,it[in].y);
		
		it[in].flags|=*(unsigned long long*)(it[in].drdata+30);
		if (it[in].flags&IF_MOVEBLOCK) map[m].flags|=MF_TMOVEBLOCK;
		if (it[in].flags&IF_SIGHTBLOCK) map[m].flags|=MF_TSIGHTBLOCK;
		if (it[in].flags&IF_SOUNDBLOCK) map[m].flags|=MF_TSOUNDBLOCK;
		if (it[in].flags&IF_DOOR) map[m].flags|=MF_DOOR;
		it[in].drdata[0]=0;
		it[in].sprite--;

		reset_los(it[in].x,it[in].y);
		if (!it[in].drdata[38] && !reset_dlight(it[in].x,it[in].y)) it[in].drdata[38]=1;
		add_lights(it[in].x,it[in].y);
		return 2;
	}

	if (it[in].drdata[0]) return 2;	// already open

        if (!(ch[cn].flags&CF_PLAYER)) {
		;
	} else if (!(in2=ch[cn].citem) || it[in2].ID!=IID_AREA17_LOCKPICK) {
		log_char(cn,LOG_SYSTEM,0,"The door is locked and you don't have the right key.");
		return 2;
	} else {
		log_char(cn,LOG_SYSTEM,0,"You pick the lock.");
		notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TWOCITY_PICK,cn,0);
	}

	remove_lights(it[in].x,it[in].y);
		
	*(unsigned long long*)(it[in].drdata+30)=it[in].flags&(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
	it[in].flags&=~(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
	map[m].flags&=~(MF_TMOVEBLOCK|MF_TSIGHTBLOCK|MF_DOOR|MF_TSOUNDBLOCK);
	it[in].drdata[0]=1;
	it[in].sprite++;

	reset_los(it[in].x,it[in].y);
	if (!it[in].drdata[38] && !reset_dlight(it[in].x,it[in].y)) it[in].drdata[38]=1;
	add_lights(it[in].x,it[in].y);

	call_item(it[in].driver,in,0,ticker+TICKS*20);

	return 1;
}

void pick_chest(int in,int cn)
{
	int in2;

	if (!cn) return;

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

        if (!has_item(cn,IID_AREA17_LOCKPICK)) {
		log_char(cn,LOG_SYSTEM,0,"The %s is locked and you don't have the right key.",lower_case(it[in].name));
		return;
	} else {
		log_char(cn,LOG_SYSTEM,0,"You pick the lock.");
		notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TWOCITY_PICK,cn,0);
	}

	switch(it[in].drdata[0]) {
		case 0:		in2=create_item("palace_note1"); break;
		case 1:		in2=create_item("palace_note2"); break;
		case 2:		in2=create_item("palace_note3"); break;
		case 3:		in2=create_item("merchant_note1"); break;
		default:	in2=0; break;
	}
        if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"You've found bug #8331.");
		return;
	}
	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from pick-chest");
	ch[cn].citem=in2;
	it[in2].carried=cn;
	ch[cn].flags|=CF_ITEMS;
	
	log_char(cn,LOG_SYSTEM,0,"You found a %s.",lower_case(it[in2].name));

        return;
}

void burndown(int in,int cn)
{
	struct twocity_ppd *ppd;
	int in2;

	if (!cn) {
		if (it[in].drdata[0]) {
                        it[in].drdata[0]--;
			
			if (it[in].drdata[0]>15) {
				it[in].sprite++;
				set_sector(it[in].x,it[in].y);
				call_item(it[in].driver,in,0,ticker+TICKS*5);
			} else if (it[in].drdata[0]==15) {
				remove_item_light(in);
                                map[it[in].x+it[in].y*MAXMAP].fsprite=0;
				it[in].mod_index[0]=V_LIGHT;
				it[in].mod_value[0]=0;
				//add_light(it[in].x,it[in].y,-200,0);
				call_item(it[in].driver,in,0,ticker+TICKS*5);
			} else if (it[in].drdata[0]==0) {
				it[in].sprite=21115;
			} else call_item(it[in].driver,in,0,ticker+TICKS*5);
		}
		return;
	}
	if (it[in].drdata[0]>15) {
		log_char(cn,LOG_SYSTEM,0,"It is too hot to touch.");
		return;
	}
	if (it[in].drdata[0]) {
		log_char(cn,LOG_SYSTEM,0,"It was burned down already.");
		return;
	}

	if (!(in2=ch[cn].citem) || it[in2].driver!=IDR_TORCH || !it[in2].drdata[0]) {
		log_char(cn,LOG_SYSTEM,0,"You touch the barrel.");
		return;
	}

	it[in].drdata[0]=20;
	it[in].sprite=51077;
	map[it[in].x+it[in].y*MAXMAP].fsprite=1024<<16;
	it[in].mod_index[0]=V_LIGHT;
	it[in].mod_value[0]=200;
	add_item_light(in);
	//add_light(it[in].x,it[in].y,200,0);
	call_item(it[in].driver,in,0,ticker+TICKS*5);

        notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TWOCITY_PICK,cn,0);


	ppd=set_data(cn,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	if (ppd && (ppd->thief_state==13 || ppd->thief_state==14)) {
		ppd->thief_state=14;
		ppd->thief_killed[0]++;
	}
	
        return;
}

struct thiefguard_data
{
        int last_talk;
	int current_victim;		
};	

void thiefguard(int cn,int ret,int lastact)
{
	struct thiefguard_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_THIEFGUARDDRIVER,sizeof(struct thiefguard_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

			if (ppd && ppd->thief_state<3 && ch[co].y<27 && char_see_char(cn,co)) {
				fight_driver_add_enemy(cn,co,1,1);
			}

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

                        // only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        if (ppd) {
				switch(ppd->thief_state) {
					case 0:		say(cn,"HALT! Who's there? Ah, a stranger in our wonderful town. Hello, %s, and welcome to the thieves guild. Thou mayest not enter...",ch[co].name);
							ppd->thief_state++; didsay=1;
							break;
					case 1:		say(cn,"...unless thou wert to become a member. If this is thy wish, thou wilt have to °c4pay a fee°c0 of 100G.");
							ppd->thief_state++; didsay=1;
							break;
					case 2:		break;	// waiting for player to pay the fee
					case 3:		say(cn,"Thou might want to talk to the guild master now, %s. He's in the room behind me.",ch[co].name);
							ppd->thief_state++; didsay=1;
							break;
					case 4:		break;

					case 50:	say(cn,"Ah, %s. I have heard of you. Thou art the one who killed the old guild master.",ch[co].name);
							ppd->thief_state++; didsay=1;
							break;
					case 51:	say(cn,"Well, thou hast done us and our new master a favor with that. Not that we'd pay thee anything for it, but we won't hold any grudges either.");
							ppd->thief_state=1; didsay=1;							
							break;
				}
                                if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}				
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && ppd->thief_state<=2) { dat->last_talk=0; ppd->thief_state=0; }
						break;
				case 15:	ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd->thief_state==2) {
							if (take_money(co,10000)) {
								say(cn,"I welcome thee, %s, as a member of the thieves guild. Thou shalt be as dear to me as my brother - whom I killed when he was cheating me with the winnings of our enterprise.",ch[co].name);
								ppd->thief_state=3;
							} else say(cn,"Thou dost not have enough money.");
						} else say(cn,"Hu?");
						break;
			}

			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
                                // let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;				
			}
		}

		standard_message_driver(cn,msg,0,0);
		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	fight_driver_update(cn);
	if (fight_driver_attack_visible(cn,0)) return;
	if (spell_self_driver(cn)) return;

        if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;	
	}

        do_idle(cn,TICKS);
}

struct thiefmaster_data
{
        int last_talk;
	int current_victim;		
};	

void thiefmaster(int cn,int ret,int lastact)
{
	struct thiefguard_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0,score,val,tmp;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_THIEFMASTERDRIVER,sizeof(struct thiefmaster_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

                        // only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                        if (ppd) {
				if (ppd->thief_state<4) ppd->thief_state=4;
				
				switch(ppd->thief_state) {
                                        case 4:		say(cn,"Ah. A new member. Welcome, %s.",ch[co].name);
							ppd->thief_state++; didsay=1;
							break;
					case 5:		if (has_item(co,IID_AREA17_LOCKPICK)) {
								ppd->thief_state=11;
								break;
							}
							say(cn,"Now, lets see... What jobs do I have for a young thief who hasn't earned his lockpick yet...");
							ppd->thief_state++; didsay=1;
							questlog_open(co,25); questlog_close(co,26); questlog_close(co,27); questlog_close(co,28);
							break;
					case 6:		say(cn,"Ah. This might be just right for thee. Listen, %s. A band of robbers has settled down in an abandoned section of Exkordon. They are committing crimes without our permission. I want thee to go there, and kill as many robbers as thou canst.",ch[co].name);
							ppd->thief_state++; didsay=1;
							break;
					case 7:		say(cn,"The robbers section is in the eastern part of Exkordon. Thou canst not miss it - just keep going east till you get attacked.");
							ppd->thief_state++; didsay=1;
							break;
					case 8:		say(cn,"If thou thinkst thou hast killed enough robbers, come back here and say: °c4I am done°c0");
							ppd->thief_state++; didsay=1;
							break;
					case 9:		// waiting for robber mission to finish
							if (realtime-ppd->thief_last_seen>60*2) {
								say(cn,"Well, art thou done, %s? (°c4I am done°c0)",ch[co].name);
								didsay=1;
							}
							break;
					case 10:	score=ppd->thief_killed[0]+
								ppd->thief_killed[1]*2+
								ppd->thief_killed[2]*3+
								ppd->thief_killed[3]*4+
								ppd->thief_killed[4]*5+
								ppd->thief_killed[5]*6;
							if (!score) {
								say(cn,"But thou hast not killed a single robber? I am disappointed.");
								ppd->thief_state=5; didsay=1;
								break;
							}
							if (score<50) {
								say(cn,"Well, thou didst what thine limited abilities allowed.");
								val=5000;
							} else if (score<200) {
								say(cn,"Nicely done, %s.",ch[co].name);
								val=10000;
							} else if (score<1000) {
								say(cn,"I am impressed, %s.",ch[co].name);
								val=15000;
							} else {
								say(cn,"All hail %s, the robber slayer!",ch[co].name);
								val=20000;
							}
							ppd->thief_killed[0]=ppd->thief_killed[1]=ppd->thief_killed[2]=ppd->thief_killed[3]=ppd->thief_killed[4]=ppd->thief_killed[5]=0;

							tmp=val;
							val=questlog_scale(questlog_count(co,25),val);
                                                        dlog(cn,0,"Received %d exp for doing quest Earning the Lockpick for the %d. time (nominal value %d exp)",val,questlog_count(co,25)+1,tmp);
                                                        give_exp(co,min(level_value(ch[co].level)/5,val));
                                                        ppd->thief_bits|=1;
							questlog_done(co,25);
							
							in=create_item("lockpick");
							if (in && !give_char_item(co,in)) destroy_item(in);
                                                        ppd->thief_state++; didsay=1;							
							break;
					case 11:	// start of next mission
							if (!has_item(co,IID_AREA17_LOCKPICK)) {
								say(cn,"What? Thou hast lost thine lockpick? Here we go again...");
								ppd->thief_state=5; didsay=1;
								break;
							}
							if (has_item(co,IID_AREA17_SEWERKEY1)) {
								ppd->thief_state=15;
								break;
							}
							say(cn,"Now that thou hast earned thine lockpick, %s, I want thee to punish a merchant who hast not paid his bills.",ch[co].name);
							questlog_open(co,26); questlog_close(co,27); questlog_close(co,28);
							ppd->thief_state++; didsay=1;
							break;
					case 12:	say(cn,"Next to the governor's palace is a barrel store. It belongs to this merchant. The name does not matter. Just go there, and burn those barrels down.");
							ppd->thief_state++; didsay=1;
							break;
					case 13:	break;	// waiting for barrels to burn down
					case 14:        score=ppd->thief_killed[0];
							if (score<10) {
								say(cn,"Ah, %s. I've heard about thine efforts burning down those barrels.",ch[co].name);
								val=5000;
							} else {
								say(cn,"Thou made a nice fire, indeed, %s.",ch[co].name);
								val=10000;
							}
							say(cn,"Here's a key that might come in handy. It opens most of the doors in the sewers.");
							ppd->thief_killed[0]=ppd->thief_killed[1]=ppd->thief_killed[2]=ppd->thief_killed[3]=ppd->thief_killed[4]=ppd->thief_killed[5]=0;
							
							tmp=val;
							val=questlog_scale(questlog_count(co,26),val);
                                                        dlog(cn,0,"Received %d exp for doing quest Extortion for the %d. time (nominal value %d exp)",val,questlog_count(co,26)+1,tmp);
                                                        give_exp(co,min(level_value(ch[co].level)/5,val));
                                                        ppd->thief_bits|=2;
							questlog_done(co,26);
							
							in=create_item("sewer_key1");
							if (in && !give_char_item(co,in)) destroy_item(in);
                                                        ppd->thief_state++; didsay=1;
							break;
					case 15:	// start of next mission
							if (!has_item(co,IID_AREA17_LOCKPICK)) {
								say(cn,"What? Thou hast lost thine lockpick? Here we go again...");
								ppd->thief_state=5; didsay=1;
								break;
							}
							if (!has_item(co,IID_AREA17_SEWERKEY1)) {
								say(cn,"What? Thou hast lost the sewer key? Go burn again...");
								ppd->thief_state=11;
								break;
							}
							if (has_item(co,IID_AREA17_SEWERKEY2)) {
								ppd->thief_state=18;
								break;
							}
							say(cn,"I have another job for thee, %s. Some of the merchants in Exkordon decided to fix the prices, and I want to know the exact figures. They all signed an agreement, and I want thee to obtain a copy.",ch[co].name);
							questlog_open(co,27); questlog_close(co,28);
							ppd->thief_state++; didsay=1;
							break;
					case 16:	say(cn,"One of those merchants is Culd. His shop is fairly close to the governors palace. I'd suggest thou try to sneak in at night and search his shop.");
							ppd->thief_state++; didsay=1;
							break;
					case 17:	break;	// waiting for player to deliver item
					case 18:	// start of next mission
							if (!has_item(co,IID_AREA17_LOCKPICK)) {
								say(cn,"What? Thou hast lost thine lockpick? Here we go again...");
								ppd->thief_state=5; didsay=1;
								break;
							}
							if (!has_item(co,IID_AREA17_SEWERKEY1)) {
								say(cn,"What? Thou hast lost the sewer key? Go burn again...");
								ppd->thief_state=11;
								break;
							}
							if (!has_item(co,IID_AREA17_SEWERKEY2)) {
								say(cn,"What? Thou hast lost the second sewer key? You're lucky I lost that agreement too...");
								ppd->thief_state=15;
								break;
							}
							if (has_item(co,IID_AREA17_PALACEKEY3)) {
								ppd->thief_state=20;
								break;
							}
							say(cn,"One last job for thee, %s. One of my thieves has lost his lockpick in the sewers, close to the Greenling King. This is a special lockpick, quite valuable, so I'd like thee to find it for me.",ch[co].name);
							questlog_open(co,28);
							ppd->thief_state++; didsay=1;
							break;
					case 19:	break;	// waiting for player to deliver item
					case 20:	if (!has_item(co,IID_AREA17_LOCKPICK)) {
								say(cn,"What? Thou hast lost thine lockpick? Here we go again...");
								ppd->thief_state=5; didsay=1;
								break;
							}
							if (!has_item(co,IID_AREA17_SEWERKEY1)) {
								say(cn,"What? Thou hast lost the sewer key? Go burn again...");
								ppd->thief_state=11;
								break;
							}
							if (!has_item(co,IID_AREA17_SEWERKEY2)) {
								say(cn,"What? Thou hast lost the second sewer key? You're lucky I lost that agreement too...");
								ppd->thief_state=15;
								break;
							}
							if (!has_item(co,IID_AREA17_PALACEKEY3)) {
								say(cn,"Uh, about that golden lockpick again, %s...",ch[co].name);
								ppd->thief_state=18;
								break;
							}
							say(cn,"I hope thou art enjoying thine stay here in Exkordon. I do not have any jobs for thee at the moment.");
							ppd->thief_state++; didsay=1;
							break;
					case 21:	break;	// all done.
				}
                                if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					ppd->thief_last_seen=realtime;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && ppd->thief_state<=9) { dat->last_talk=0; ppd->thief_state=4; }
						if (ppd && ppd->thief_state>=11 && ppd->thief_state<=13) { dat->last_talk=0; ppd->thief_state=11; }
						if (ppd && ppd->thief_state>=15 && ppd->thief_state<=17) { dat->last_talk=0; ppd->thief_state=15; }
						if (ppd && ppd->thief_state>=18 && ppd->thief_state<=19) { dat->last_talk=0; ppd->thief_state=18; }
						if (ppd && ppd->thief_state>=20 && ppd->thief_state<=21) { dat->last_talk=0; ppd->thief_state=20; }
						break;
				case 16:	ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd->thief_state==9) {
                                                        ppd->thief_state=10;
							say(cn,"Thou art done? Now, let's see...");
						} else say(cn,"Hu?");
						break;
			}

			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

				if (it[in].ID==IID_AREA17_MERCHANTNOTE1 && ppd && ppd->thief_state==17) {
					say(cn,"Ah, yes, that is the agreement I wanted. Nice job, %s. Here, this key will open the remaining sewer doors.",ch[co].name);
					
					questlog_done(co,27);
					destroy_item_byID(co,IID_AREA17_MERCHANTNOTE1);
                                        ppd->thief_bits|=4;
					
					in=create_item("sewer_key2");
					if (in && !give_char_item(co,in)) destroy_item(in);

                                        ppd->thief_state=18;
				} else if (it[in].ID==IID_AREA17_GOLDENLOCKPICK && ppd && ppd->thief_state==19) {
					say(cn,"There it is, my golden lockpick, given to me by the guild master in Aston, for extraordinary service. I thank thee, %s!",ch[co].name);

					questlog_done(co,28);
					destroy_item_byID(co,IID_AREA17_GOLDENLOCKPICK);
                                        ppd->thief_bits|=8;
					
					in=create_item("palace_key3");
					if (in && !give_char_item(co,in)) destroy_item(in);
                                        ppd->thief_state=20;
				}
                                // let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;				
			}
		}

		standard_message_driver(cn,msg,0,0);
		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	fight_driver_update(cn);
	if (fight_driver_attack_visible(cn,0)) return;
	if (spell_self_driver(cn)) return;

        if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;	
	}

        do_idle(cn,TICKS);
}

void thiefmaster_dead(int cn,int co)
{
        struct twocity_ppd *ppd;

	if (ch[cn].citem) {
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;
	}

	if (!co) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	if (ppd) {
		ppd->thief_state=50;
	}
}

void robber_dead(int cn,int co)
{
	struct twocity_ppd *ppd;

	if (!co) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	if (ppd && ppd->thief_state>=6 && ppd->thief_state<=9) {
		switch(ch[cn].level) {
                        case 35:	ppd->thief_killed[0]++; break;
			case 39:        ppd->thief_killed[1]++; break;
			case 43:	ppd->thief_killed[2]++; break;
			case 47:	ppd->thief_killed[3]++; break;
                        case 51:	ppd->thief_killed[4]++; break;
			case 55:	ppd->thief_killed[5]++; break;
			default:	elog("unlisted robber level %d in two.c, robber_dead()",ch[cn].level); break;
		}
	}
}

struct sanwyn_driver_data
{
        int last_talk;
	int current_victim;
};

void sanwyn(int cn,int ret,int lastact)
{
	struct sanwyn_driver_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SANWYNDRIVER,sizeof(struct sanwyn_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                        if (ppd) {
				switch(ppd->sanwyn_state) {
					case 0:		say(cn,"Welcome, %s, to the noble and honorable city of Exkordon. I am %s, Sergeant Major in the Imperial Army.",ch[co].name,ch[cn].name);
							questlog_open(co,29);
							ppd->sanwyn_state++; didsay=1;
							break;
                                        case 1:		say(cn,"They are so noble and honorable that they decided to defect from the Empire. Exkordon is, for all we care, a lawless town, %s. Be careful in there, but be aware that thou canst do as thou wilt in Exkordon - we don't care.",get_army_rank_string(co));
							ppd->sanwyn_state++; didsay=1;
							break;
					case 2:		say(cn,"Inofficially, if thou wert to burn the whole city down, the whole Imperial army would applaud thee, even though we'd have to apologize, officially. Anyway.");
							ppd->sanwyn_state++; didsay=1;
							break;
					case 3:		say(cn,"The Imperial army suspects that the current governor of Exkordon - the one who decided to defect from the Empire - has dirty hands. Shouldst thou happen to find any incriminating documents, %s, I'd be very grateful if thou wouldst bring them to me.",ch[co].name);
							ppd->sanwyn_state++; didsay=1;
							break;
                                        case 4:		say(cn,"The thieves guild might be helpful in thy search, %s. Thou canst find their headquarter in the sewers. One entrance is a bit east of the city gate, behind a guard house. They won't have those documents, of course, but they might be able to help thee enter the palace.",ch[co].name);
							ppd->sanwyn_state++; didsay=1;
							break;
					case 5:		say(cn,"That will be all, %s. May thy stay in Exkordon be... destructive.",get_army_rank_string(co));
							ppd->sanwyn_state++; didsay=1;
							break;
					case 6:		break; // waiting for documents
					case 7:		say(cn,"Dirty hands indeed! Well, well, well. We'll be able to stir up quite a bit of trouble with these.");
							ppd->sanwyn_state++; didsay=1;
							questlog_done(co,29);
							break;
					case 8:		break; // done

				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && ppd->sanwyn_state<=6) { dat->last_talk=0; ppd->sanwyn_state=0; }
						if (ppd && ppd->sanwyn_state==8) { dat->last_talk=0; ppd->sanwyn_state=7; }
                                                break;
			}
			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				
				ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                                if (it[in].ID==IID_AREA17_PALACENOTE1 && ppd->sanwyn_state<=6 && !(ppd->sanwyn_bits&1)) {
					say(cn,"Ah. Well done, %s.",ch[co].name);
					ppd->sanwyn_bits|=1;
					if (ppd->sanwyn_bits==7) ppd->sanwyn_state=7;

					give_military_pts(cn,co,15,min(level_value(ch[co].level)/5,15000));

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (it[in].ID==IID_AREA17_PALACENOTE2 && ppd->sanwyn_state<=6 && !(ppd->sanwyn_bits&2)) {
					say(cn,"Ah. Well done, %s.",ch[co].name);
					ppd->sanwyn_bits|=2;
					if (ppd->sanwyn_bits==7) ppd->sanwyn_state=7;

					give_military_pts(cn,co,15,min(level_value(ch[co].level)/5,15000));

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (it[in].ID==IID_AREA17_PALACENOTE3 && ppd->sanwyn_state<=6 && !(ppd->sanwyn_bits&4)) {
					say(cn,"Ah. Well done, %s.",ch[co].name);
					ppd->sanwyn_bits|=4;
					if (ppd->sanwyn_bits==7) ppd->sanwyn_state=7;

					give_military_pts(cn,co,15,min(level_value(ch[co].level)/5,15000));

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void randomize_tiles(struct twocity_ppd *ppd)
{
	int n;
	
	for (n=0; n<ARRAYSIZE(ppd->goodtile); n++) {
		ppd->goodtile[n]=RANDOM(6)+1;
	}
}

void bookcase(int in,int cn)
{
	int r;
	
	struct twocity_ppd *ppd;
	static char *standard={"After reading the title you put the book back."};
	char *name="Lady Manners' Guide to Decent Behaviour",*text=standard,buf[120];
	static char *colorname[]={
		"null",
		"Red",
		"Green",
		"Blue",
		"Yellow",
		"Black",
		"White"
	};

	if (!cn) {
		if (it[in].drdata[0]==0) it[in].sprite+=RANDOM(4);	// randomize looks
		return;
	}
	
	if (!(ch[cn].flags&CF_PLAYER)) return;

        ppd=set_data(cn,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	if (!ppd) return;	// oops

	if (!ppd->goodtile[0]) randomize_tiles(ppd);

        if (!it[in].drdata[0]) {	// random books to hide the important ones
                r=RANDOM(26);
		switch(r) {
			case 0:		name="Tales of Two Towns by Karl Dicker"; break;
			case 1:		name="The Art of Warfare by Hun Yu"; break;
			case 2:		name="Chris Maas visits Carol by Karl Dicker"; break;
			case 3:		name="Secrets of Adygalah Alchemy by Leonarda"; text="One recipe most mages will find useful uses Adygalah, Bhalkissa and Firuba, plus one berry and one or two mushrooms."; break;
			case 4:		name="The rise and fall of the Seyan Empire by Takitus"; break;
                        case 5:		name="History of Ancient Astonia by Chiasmaphora"; break;
                        case 6:		name="Treatise on the Mastery of Mana by Mage Niuma"; break;
			case 7:		name="The Song of the Warrior by Sir Regis Le Voleir"; break;
			case 8:		name="The Book of Ishtar, Anonymous"; break;
			case 9:		name="Concessions to Fear by Kentindher"; break;
			case 10:	name="Poems of War and Homecoming by Melthold of Anten"; break;
			case 11:	name="Memoires of a Lady-in-Waiting by Dame Sakanor"; break;
			case 12:	name="Comprehension and Expression by Master Getsades"; break;
			case 13:	name="Great Astonian Thinkers by Master Riotan"; break;
			case 14:	name="A Portrait of the Seyan'Du as A Young Mage by Esjamocey"; break;
			case 15:	name="Critique of Pure Courage by Imanel Dique"; break;
			case 16:	name="Collected Essays by Lindmar the Elder"; break;
			case 17:	name="The Reforming of Curves by Master Elyosod"; break;
			case 18:	name="Advanced Agility in Forty-two Steps by Seyan'Du Bartoshi"; break;
			case 19:	name="The Oath by Sheney"; break;
			case 20:	name="The Strife for Light by Father Ignato"; break;
			case 21:	name="The Aston Years by Lord Ironborn"; break;
			case 22:	name="Luctim - Superstition or Reality? by Mintu the Enlightened"; break;
			case 23:	name="I Have, Alas by Goytila"; break;
			case 24:	name="A Midwinter Day's Wake by Pearshaks"; break;
			case 25:	name="Fama Fraternitatis by Valentin Andreae"; break;			
		}		
	} else {			// special books
		switch(it[in].drdata[0]) {
			case 1:         if (!has_item(cn,IID_AREA17_LIBRARYKEY)) {
						log_char(cn,LOG_SYSTEM,0,"The bookcase is locked and you do not have the right key.");
						log_char(cn,LOG_SYSTEM,0,"There is a note attached to the lock: A statue stole the key and vanished with it in the northern part of the library.");
						return;
					}

					name="The Knowledge of Ages by Ishtar";
					if (!ppd->solved_library) {
						ppd->solved_library=1;
						text="You read the book and absorb the knowledge contained therein.";
						give_exp(cn,min(level_value(ch[cn].level)/5,80000));
					}
                                        break;
			case 2:		sprintf(buf,"How to Raise %s Orchids by Klark",colorname[ppd->goodtile[0]]);
					name=buf;
					break;
			case 3:		sprintf(buf,"A %s Day in the Life of a Warrior by C. O. Nan",colorname[ppd->goodtile[1]]);
					name=buf;
					break;
			case 4:		sprintf(buf,"Dancing in Ten Easy Lessons by James %s",colorname[ppd->goodtile[2]]);
					name=buf;
					break;
			case 5:		sprintf(buf,"Help! I Have Been Visited by Little %s Man! by Meier",colorname[ppd->goodtile[3]]);
					name=buf;
					break;
			case 6:		sprintf(buf,"The Day the World turned %s by Casaldra",colorname[ppd->goodtile[4]]);
					name=buf;
					break;
		}
	}
	
	log_char(cn,LOG_SYSTEM,0,"°c2%s.°c0 %s",name,text);
}

void colortile(int in,int cn)
{
	struct twocity_ppd *ppd;
	int row,nr;

	if (!cn) return;

	if (!(ch[cn].flags&CF_PLAYER)) return;

        ppd=set_data(cn,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	if (!ppd) return;	// oops

	if (!ppd->goodtile[0]) randomize_tiles(ppd);
	
	row=it[in].drdata[0];
	nr=it[in].drdata[1];

	if (ppd->goodtile[row]!=nr) {
		log_char(cn,LOG_SYSTEM,0,"You see colors dancing before your eyes, and you sense that something has changed.");
		randomize_tiles(ppd);
		teleport_char_driver(cn,5,250);
		return;
	}
}

void skelraise(int in,int cn)
{
	int co,in2;

	if (!cn) {
		if (it[in].drdata[2]) {
			co=*(unsigned *)(it[in].drdata+4);
			if (co<1 || co>=MAXCHARS || !(ch[co].flags) || ch[co].serial!=*(unsigned *)(it[in].drdata+8)) {
				it[in].drdata[2]=0;
				it[in].sprite--;
				set_sector(it[in].x,it[in].y);
			} else {
				call_item(it[in].driver,in,0,ticker+TICKS*10);
			}			
		}
		return;
	}
	
	if (it[in].drdata[2]) {
		log_char(cn,LOG_SYSTEM,0,"You touch the chair.");
		return;
	}

	if (!(in2=ch[cn].citem) || it[in2].ID!=IID_AREA17_BLOODBOWL) {
		log_char(cn,LOG_SYSTEM,0,"The skeleton crumbles to dust as you touch it.");
		*(unsigned *)(it[in].drdata+4)=0;
		*(unsigned *)(it[in].drdata+8)=0;
	} else {
		log_char(cn,LOG_SYSTEM,0,"The skeleton comes to life as you pour the blood over it.");
		
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"dropped over skeleton");
		destroy_item(in2);
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		
		switch(it[in].drdata[0]) {
                        case 0:         co=create_drop_char("raised_skeleton_green",it[in].x,it[in].y);
					*(unsigned *)(it[in].drdata+4)=co;
					*(unsigned *)(it[in].drdata+8)=ch[co].serial;
					break;
			case 1:         co=create_drop_char("raised_skeleton_red",it[in].x,it[in].y);
					*(unsigned *)(it[in].drdata+4)=co;
					*(unsigned *)(it[in].drdata+8)=ch[co].serial;
					break;
			case 2:         co=create_drop_char("raised_skeleton_green_key",it[in].x,it[in].y);
					*(unsigned *)(it[in].drdata+4)=co;
					*(unsigned *)(it[in].drdata+8)=ch[co].serial;
					break;
			case 3:         co=create_drop_char("raised_skeleton_red_key",it[in].x,it[in].y);
					*(unsigned *)(it[in].drdata+4)=co;
					*(unsigned *)(it[in].drdata+8)=ch[co].serial;
					break;
			case 4:         co=create_drop_char("raised_skeleton_nolight",it[in].x,it[in].y);
					*(unsigned *)(it[in].drdata+4)=co;
					*(unsigned *)(it[in].drdata+8)=ch[co].serial;
					break;
			case 5:         co=create_drop_char("quest_skeleton",it[in].x,it[in].y);
					*(unsigned *)(it[in].drdata+4)=co;
					*(unsigned *)(it[in].drdata+8)=ch[co].serial;
					break;
		}
	}
	
	it[in].drdata[2]=1;
	it[in].sprite++;

	set_sector(it[in].x,it[in].y);
	call_item(it[in].driver,in,0,ticker+TICKS*10);
}

struct skelly_driver_data
{
        int last_talk;
	int current_victim;
	int alive;
};

void skelly(int cn,int ret,int lastact)
{
	struct skelly_driver_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SKELLYDRIVER,sizeof(struct skelly_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			dat->alive=ticker;
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                        if (ppd) {
				switch(ppd->skelly_state) {
					case 0:		say(cn,"My greetings, %s. I am %s, Governor of Exkordon. Former governor, I should say.",ch[co].name,ch[cn].name);
							ppd->skelly_state++; didsay=1;
							questlog_open(co,30);
							break;
					case 1:		emote(cn,"speaks in a different voice now");
							say(cn,"Pass the green, go to the red, find the cross, bring me peace.");
							ppd->skelly_state++; didsay=1;
							break;
					case 2:		break;
					case 3:		break;
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && ppd->skelly_state<=2) { dat->last_talk=0; ppd->skelly_state=0; }
                                                break;
			}
			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				
				ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                                if (it[in].ID==IID_AREA17_CROSS && ppd->skelly_state<=2) {
					say(cn,"My cross, the insignia of my office. Now, I may rest in peace. I thank thee, %s.",ch[co].name);
                                        ppd->skelly_state=3;

					questlog_done(co,30);
					destroy_item_byID(co,IID_AREA17_CROSS);
					destroy_item_byID(co,IID_AREA17_GREENKEY);
					destroy_item_byID(co,IID_AREA17_REDKEY);

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;		
	}
	if (ticker-dat->alive>TICKS*30) {
		kill_char(cn,0);
		return;
	}
	
	do_idle(cn,TICKS);
}

void skelly_dead(int cn,int co)
{
	if (ch[cn].citem) {
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;
	}
}

struct alchemist_driver_data
{
        int last_talk;
	int current_victim;
};

void alchemist(int cn,int ret,int lastact)
{
	struct alchemist_driver_data *dat;
	struct twocity_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_ALCHEMISTDRIVER,sizeof(struct alchemist_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                        if (ppd) {
				switch(ppd->alchemist_state) {
					case 0:		say(cn,"Too much sulphur. Yes, too much sulphur. Oh, hello, %s. I am %s, the alchemist.",ch[co].name,ch[cn].name);
							questlog_open(co,31);
							ppd->alchemist_state++; didsay=1;
							break;
					case 1:		say(cn,"If thou art not too busy, %s, thou couldst do me a favor. I need spider poison for my experiments, but I am too busy to go looking for it. If thou wouldst bring me some, I'd reward thee.",ch[co].name);
							ppd->alchemist_state++; didsay=1;
							break;
					case 2:		say(cn,"I know of some poisonous spiders who live underground, in a plantage the ancients built to grow food. When I went there some years ago, the lighting was beginning to fail. Anyway. The entrance is close to the transport portal.");
							ppd->alchemist_state++; didsay=1;
							break;
					case 3:		say(cn,"Thou wilt need to look for bright red spiders, those are the only ones who have the poison I need.");
							ppd->alchemist_state++; didsay=1;
							break;
					case 4:		break;
					case 5:		break;
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
						if (ppd && ppd->alchemist_state<=4) { dat->last_talk=0; ppd->alchemist_state=0; }
                                                break;
			}
			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				
				ppd=set_data(co,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

                                if (it[in].ID==IID_AREA17_POISON && ppd->alchemist_state<=4) {
					int tmp;

                                        ppd->alchemist_state=5;

					tmp=questlog_done(co,31);
					destroy_item_byID(co,IID_AREA17_POISON);

                                        // let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1 || tmp==3 || tmp==7 || tmp==10) {
						say(cn,"Too little sulphur this time. I will... Oh, the poison! Very well, %s. Here, take this potion for thy trouble.",ch[co].name);
						if (ch[co].level<30) in=create_item("combo_potion3");
						else in=create_item("security_potion");
						if (in) {
							if (!give_char_item(co,in)) destroy_item(in);
						}
					} else say(cn,"Too little sulphur this time. I will... Oh, the poison! Very well, %s, I thank thee.",ch[co].name);
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;		
	}
        do_idle(cn,TICKS);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_TWOGUARD:		guard_driver(cn,ret,lastact); return 1;
		case CDR_TWOBARKEEPER:		barkeeper(cn,ret,lastact); return 1;
		case CDR_TWOSERVANT:		servant(cn,ret,lastact); return 1;
		case CDR_TWOTHIEFGUARD:		thiefguard(cn,ret,lastact); return 1;
		case CDR_TWOTHIEFMASTER:	thiefmaster(cn,ret,lastact); return 1;
		case CDR_TWOROBBER:		char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;
		case CDR_TWOSANWYN:		sanwyn(cn,ret,lastact); return 1;
		case CDR_TWOSKELLY:		skelly(cn,ret,lastact); return 1;
		case CDR_TWOALCHEMIST:		alchemist(cn,ret,lastact); return 1;

		default:			return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {		
		case IDR_PICKDOOR:	return pick_door(in,cn);
		case IDR_PICKCHEST:	pick_chest(in,cn); return 1;
		case IDR_BURNDOWN:	burndown(in,cn); return 1;
		case IDR_BOOKCASE:	bookcase(in,cn); return 1;
		case IDR_COLORTILE:	colortile(in,cn); return 1;
		case IDR_SKELRAISE:	skelraise(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_TWOGUARD:		guard_dead(cn,co); return 1;
		case CDR_TWOBARKEEPER:		return 1;
		case CDR_TWOSERVANT:		servant_dead(cn,co); return 1;
		case CDR_TWOTHIEFGUARD:		return 1;
		case CDR_TWOTHIEFMASTER:	thiefmaster_dead(cn,co); return 1;
		case CDR_TWOROBBER:		robber_dead(cn,co); return 1;
		case CDR_TWOSANWYN:		return 1;
		case CDR_TWOSKELLY:		skelly_dead(cn,co); return 1;
		case CDR_TWOALCHEMIST:		return 1;

                default:			return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_TWOGUARD:		return 1;
		case CDR_TWOBARKEEPER:		return 1;
		case CDR_TWOSERVANT:		return 1;
		case CDR_TWOTHIEFGUARD:		return 1;
		case CDR_TWOTHIEFMASTER:	return 1;
		case CDR_TWOROBBER:		return 1;
		case CDR_TWOSKELLY:		return 1;
		case CDR_TWOALCHEMIST:		return 1;

                default:			return 0;
	}
}















