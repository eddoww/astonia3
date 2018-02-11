/*

$Id: bank.c,v 1.2 2007/08/13 18:50:38 devel Exp $

$Log: bank.c,v $
Revision 1.2  2007/08/13 18:50:38  devel
fixed some warnings

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:48  sam
Added RCS tags


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
#include "store.h"
#include "date.h"
#include "sector.h"
#include "libload.h"
#include "drvlib.h"
#include "consistency.h"
#include "database.h"
#include "bank.h"

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
	{{"help",NULL},"Sorry, I'm just a merchant, %s!",0},
	{{"what's","up",NULL},"Everything that isn't nailed down.",0},
	{{"what","is","up",NULL},"Everything that isn't nailed down.",0},
        {{"what's","your","name",NULL},NULL,1},
	{{"what","is","your","name",NULL},NULL,1},
        {{"who","are","you",NULL},NULL,1},
	{{"account",NULL},"If you want to open an account, you must first deposit (°c4explain deposit°c0) some money in it. After that, you can inquire for your balance (°c4explain balance°c0) or withdraw (°c4explain withdraw°c0) money.",0},
	{{"explain","deposit",NULL},"To deposit 38 gold coins for example, just say: 'deposit 38'.",0},
	{{"explain","withdraw",NULL},"To withdraw 38 gold coins for example, just say: 'withdraw 38'.",0},
	{{"explain","balance",NULL},"To inquire about the balance of your account, just say: 'balance'",0}
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
				else switch(qa[q].answer_code) {
					     case 1:	quiet_say(cn,"I'm %s.",ch[cn].name);
				}
				break;
			}
		}
	}
	

        return 42;
}


//-----------------------

struct bank_driver_data
{
        int last_talk;
	int dir;

	int dayx;
	int dayy;
	int daydir;

	int nightx;
	int nighty;
	int nightdir;

        int storefx;
	int storefy;
	int storetx;
	int storety;

	int doorx;
	int doory;

	int open;
	int close;

	int memcleartimer;
};

void bank_driver_parse(int cn,struct bank_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"dir")) dat->dir=atoi(value);
		else if (!strcmp(name,"dayx")) dat->dayx=atoi(value);
		else if (!strcmp(name,"dayy")) dat->dayy=atoi(value);
		else if (!strcmp(name,"daydir")) dat->daydir=atoi(value);
		else if (!strcmp(name,"nightx")) dat->nightx=atoi(value);
		else if (!strcmp(name,"nighty")) dat->nighty=atoi(value);
		else if (!strcmp(name,"nightdir")) dat->nightdir=atoi(value);
                else if (!strcmp(name,"storefx")) dat->storefx=atoi(value);
		else if (!strcmp(name,"storefy")) dat->storefy=atoi(value);
		else if (!strcmp(name,"storetx")) dat->storetx=atoi(value);
		else if (!strcmp(name,"storety")) dat->storety=atoi(value);
		else if (!strcmp(name,"doorx")) dat->doorx=atoi(value);
		else if (!strcmp(name,"doory")) dat->doory=atoi(value);
		else if (!strcmp(name,"open")) dat->open=atoi(value);
		else if (!strcmp(name,"close")) dat->close=atoi(value);
		else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

static int opening_time(int from,int to)
{
	if (from>to) {
		if (hour>=from) return 1;
		if (hour<=to) return 1;
	} else {
		if (hour>=from && hour<=to) return 1;
	}
	return 0;
}

void bank_driver(int cn,int ret,int lastact)
{
	struct bank_driver_data *dat;
	struct bank_ppd *ppd;
        int co,in,val;
        struct msg *msg,*next;
	char *ptr;

        dat=set_data(cn,DRD_BANKDRIVER,sizeof(struct bank_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
		dat->open=6; dat->close=23;
		bank_driver_parse(cn,dat);
		ch[cn].arg=NULL;
	}

	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			quiet_say(cn,"Hello %s! Would you like to open an °c4account°c0 with the Imperial Bank?",ch[co].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

                        if ((msg->dat1==1 || msg->dat1==2) && (co=msg->dat3)!=cn) {	// talk, and not our talk
				ppd=set_data(co,DRD_BANK_PPD,sizeof(struct bank_ppd));

				if (ppd && (ptr=strcasestr((char*)msg->dat2,"deposit"))) {
					val=atoi(ptr+7)*100;
                                        if (val) {
						if (val>ch[co].gold || val<0) {
							quiet_say(cn,"Thou dost not have that much gold.");
						} else {
							ch[co].gold-=val;
							ppd->imperial_gold+=val;
							ch[co].flags|=CF_ITEMS;
							quiet_say(cn,"Thou hast deposited %d gold coins.",val/100);
							dlog(co,0,"deposited %.2fG, new balance %.2fG.",val/100.0,ppd->imperial_gold/100.0);
						}
					} else quiet_say(cn,"Thou must name an amount.");
				} else if (ppd && (ptr=strcasestr((char*)msg->dat2,"withdraw"))) {
					val=atoi(ptr+8)*100;
					if (val) {
						if (val>ppd->imperial_gold || val<0) {
							quiet_say(cn,"Thou dost not have that much gold in thine account.");
						} else {
							ppd->imperial_gold-=val;
							ch[co].gold+=val;
							ch[co].flags|=CF_ITEMS;
                                                        quiet_say(cn,"Thou hast withdrawn %d gold coins.",val/100);
							dlog(co,0,"withdrew %.2fG, balance left %.2fG.",val/100.0,ppd->imperial_gold/100.0);
						}
					} else quiet_say(cn,"Thou must name an amount.");
				} else if (ppd && strcasestr((char*)msg->dat2,"balance")) {
					if (ppd->imperial_gold>100) quiet_say(cn,"Thou hast %d gold and %d silver in thine account.",ppd->imperial_gold/100, ppd->imperial_gold%100);
					else if (ppd->imperial_gold) quiet_say(cn,"Thou hast %d silver in thine account.",ppd->imperial_gold);
					else quiet_say(cn,"Thou dost not have any money in thine account.");
				}
			}			
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				// try to give it back
                                if (give_driver(cn,co)) return;
				
				// didnt work, let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (dat->dayx) {	// we have day / night positions
		if (!opening_time(dat->open,dat->close)) {	// we're closed
                        if (dat->doorx && !is_closed(dat->doorx,dat->doory)) {	// door is still open
				if (!is_room_empty(dat->storefx,dat->storefy,dat->storetx,dat->storety)) {	// store is not empty
					quiet_say(cn,"We're closing, please leave now!");
					do_idle(cn,TICKS);
					return;
				} else {
					if (use_item_at(cn,dat->doorx,dat->doory,0)) return;
					do_idle(cn,TICKS);
					return;
				}
			}
			if (move_driver(cn,dat->nightx,dat->nighty,0)) return;
			if (ch[cn].dir!=dat->nightdir) turn(cn,dat->nightdir);
		} else {	// we're open
                        if (dat->doorx && is_closed(dat->doorx,dat->doory)) {	// door is still closed
                                if (use_item_at(cn,dat->doorx,dat->doory,0)) return;
				do_idle(cn,TICKS);
				return;			
			}

			if (move_driver(cn,dat->dayx,dat->dayy,0)) return;
			if (ch[cn].dir!=dat->daydir) turn(cn,dat->daydir);
		}
	} else {		// just one position
		if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;
		if (ch[cn].dir!=dat->dir) turn(cn,dat->dir);
	}

        if (ticker>dat->last_talk+TICKS*60 && !RANDOM(25)) {
		switch(RANDOM(9)) {
			case 0:		murmur(cn,"My back itches."); break;
			case 1:		whisper(cn,"There's something stuck between your teeth."); break;
			case 2:		murmur(cn,"Oh yeah, those were the days."); break;
			case 3:		murmur(cn,"Now where did I put it?"); break;
			case 4:		murmur(cn,"Oh my, life is hard but unfair."); break;
                        case 5:		murmur(cn,"Beware of the fire snails!"); break;
			case 6:         murmur(cn,"I love the clicking of coins."); break;
			case 7:		murmur(cn,"Gold and Silver, Silver and Gold."); break;
			case 8:		murmur(cn,"All those numbers! I love it."); break;
			default:	break;
		}
		
		dat->last_talk=ticker;
	}

	if (ticker>dat->memcleartimer) {
		mem_erase_driver(cn,7);
		dat->memcleartimer=ticker+TICKS*60*60*12;
	}

        do_idle(cn,TICKS*2);
}

void bank_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_BANK:		bank_driver(cn,ret,lastact); return 1;		
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_BANK:		bank_dead(cn,co); return 1;
		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_BANK:		return 1;
		
		default:		return 0;
	}
}
