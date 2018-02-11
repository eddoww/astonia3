/*
 * $Id: staffer3.c,v 1.4 2006/10/04 17:30:38 devel Exp $
 *
 * $Log: staffer3.c,v $
 * Revision 1.4  2006/10/04 17:30:38  devel
 * added a lot of items to be destroyed on quest solve
 *
 * Revision 1.3  2006/09/27 11:40:43  devel
 * added questlog to brannington
 *
 * Revision 1.2  2006/09/26 10:59:25  devel
 * added questlog to nomad plains and brannington forest
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "staffer_ppd.h"
#include "questlog.h"

#define BRANFO_EXP_BASE	10000

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
	{{"restart",NULL},NULL,2},
	{{"please","repeat",NULL},NULL,2},
	{{"please","restart",NULL},NULL,2},
	{{"reset","me",NULL},NULL,3}
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

	if (!(ch[co].flags&CF_PLAYER)) return 0;

	if (char_dist(cn,co)>12) return 0;

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
				if (qa[q].answer) quiet_say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}

void underwater_berry(int in,int cn)
{
	if (!cn) return;

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!add_spell(cn,IDR_OXYGEN,TICKS*30,"nonomagic_spell")) return;

	remove_item(in);
	destroy_item(in);
}

void staffer3_item(int in,int cn)
{
	switch(it[in].drdata[0]) {
		case 1:		underwater_berry(in,cn); break;
		default:	elog("unknown staffer item type %d",it[in].drdata[0]);
	}
}

struct aristocrat_data
{
        int last_talk;
        int current_victim;
	int amgivingback;
};	

void aristocrat_driver(int cn,int ret,int lastact)
{
	struct aristocrat_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_ARISTOCRATDRIVER,sizeof(struct aristocrat_data));
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
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->aristocrat_state) {
					case 0:         quiet_say(cn,"Greetings stranger!");
							questlog_open(co,38);
							ppd->aristocrat_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"Say! You look like quite a buoyant adventurer.");
							ppd->aristocrat_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"Oh no, I didn't mean it that way! Please don't growl at me!");
                                                        ppd->aristocrat_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"I was watching the local wildlife at a large lake north of here...");
							ppd->aristocrat_state++; didsay=1;
                                                        break;
                                        case 4:		quiet_say(cn,"When one of the larger natives suddenly lurched out of the water and attacked me.");
							ppd->aristocrat_state++; didsay=1;
                                                        break;
					case 5:		quiet_say(cn,"I managed to escape with my life, but alas my Amulet was lost.");
							ppd->aristocrat_state++; didsay=1;
                                                        break;
					case 6:		quiet_say(cn,"I would reward you well if you could retrieve this family heirloom for me.");
							ppd->aristocrat_state++; didsay=1;
                                                        break;
					case 7:		break; // waiting for amulet
					case 8:		break; // all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->aristocrat_state<=7) { dat->last_talk=0; ppd->aristocrat_state=0; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->aristocrat_state=0; }
							break;
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it

				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

				if (it[in].ID==IID_STAFF_ARIAMULET && ppd && ppd->aristocrat_state<=7) {
					int tmp;
                                        quiet_say(cn,"Yes! Many thanks adventurer! Please accept this reward.");
					tmp=questlog_done(co,38);
					destroy_item_byID(co,IID_STAFF_ARIAMULET);
					destroy_item_byID(co,IID_STAFF_ARIKEY);
                                        ppd->aristocrat_state=8;
					if (tmp==1 && (in=create_money_item(1000*100))) {
						if (!give_char_item(co,in)) destroy_item(in);						
					}
				} else {
					say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
                                        if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                        ch[cn].citem=0;
				}
				
				// let it vanish, then
				if (ch[cn].citem) {
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	dat->amgivingback=0;

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct yoatin_data
{
        int last_talk;
        int current_victim;
	int amgivingback;
};	

void yoatin_driver(int cn,int ret,int lastact)
{
	struct yoatin_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_YOATINDRIVER,sizeof(struct yoatin_data));
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
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->yoatin_state) {					
					case 0:         quiet_say(cn,"Greetings stranger!");
							questlog_open(co,39);
							ppd->yoatin_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"Wait...I recognize you from the description my brother gave - you must be %s!",ch[co].name);
							ppd->yoatin_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"My brother's name is Yoakin. It seems you did him a great service slaying the bears of Cameron.");
                                                        ppd->yoatin_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"Mayhap you could assist me with a problem I have?");
							ppd->yoatin_state++; didsay=1;
                                                        break;
					case 4:		quiet_say(cn,"A family from the town beyond this forest has asked me to hunt down the bear that killed their son.");
							ppd->yoatin_state++; didsay=1;
                                                        break;
					case 5:		quiet_say(cn,"I am not quite the hunter my brother is and well... to be frank, bears scare the living daylights out of me.");
							ppd->yoatin_state++; didsay=1;
                                                        break;
					case 6:		quiet_say(cn,"If you could fetch me proof of the bear being slain, I would reward thee greatly.");
							ppd->yoatin_state++; didsay=1;
                                                        break;
					case 7:		quiet_say(cn,"Take care as you travel! The whole forest is full of bears and bear caves.");
							ppd->yoatin_state++; didsay=1;
                                                        break;
					case 8:		break; // waiting for bear to die
					case 9:		break; // all done
					
					
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->yoatin_state<=8) { dat->last_talk=0; ppd->yoatin_state=0; }
                                                        break;				
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->yoatin_state=0; }
							break;
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it

				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

				if (it[in].ID==IID_STAFF_BEARHEAD && ppd && ppd->yoatin_state<=8) {
					quiet_say(cn,"Thank you %s! This will be perfect proof. Here, take my belt, you are clearly the greater hunter!",ch[co].name);
					questlog_done(co,39);
					destroy_item_byID(co,IID_STAFF_BEARHEAD);
                                        if ((in=create_item("WS_Hunter_Belt"))) {
						if (!give_char_item(co,in)) destroy_item(in);						
					}
                                        ppd->yoatin_state=9;
				} else {
					say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
                                        if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                        ch[cn].citem=0;
				}
				
				// let it vanish, then
				if (ch[cn].citem) {
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	dat->amgivingback=0;

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

void robberboss_dead(int cn,int co)
{
	struct staffer_ppd *ppd;

	if (!co) return;
	
	if (!(ch[co].flags&CF_PLAYER)) return;
	if (!(ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) return;

	if (ppd->broklin_state>=5 && ppd->broklin_state<=10) {
		ppd->broklin_state=11;
		log_char(co,LOG_SYSTEM,0,"Well done. You've killed the head robber! Now go see Broklin...");
		questlog_done(co,46);
		destroy_item_byID(co,IID_STAFF_BOSSMASTER);
		destroy_item_byID(co,IID_STAFF_BOSSLAIR);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY1);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY2);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY3);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY4);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY5);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY6);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY7);
		destroy_item_byID(co,IID_STAFF_ROBBERKEY8);

	}
	
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_STAFFER3:	staffer3_item(in,cn); return 1;

                default:	return 0;
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                case CDR_ARISTOCRAT:		aristocrat_driver(cn,ret,lastact); return 1;
		case CDR_YOATIN:		yoatin_driver(cn,ret,lastact); return 1;
		case CDR_WHITEROBBERBOSS:       char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_WHITEROBBERBOSS:	robberboss_dead(cn,co); return 1;

                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_WHITEROBBERBOSS:	return 1;

                default:	return 0;
	}
}












