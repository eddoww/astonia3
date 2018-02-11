/*

$Id: merchant.c,v 1.3 2008/03/24 11:22:25 devel Exp $

$Log: merchant.c,v $
Revision 1.3  2008/03/24 11:22:25  devel
fixed gender bender bug

Revision 1.2  2007/08/13 18:50:38  devel
fixed some warnings

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:25  sam
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
#include "drvlib.h"
#include "consistency.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case 0:		return ch_driver(nr,obj,ret,lastact);
		case 1: 	return it_driver(nr,obj,ret);
		case 2:		return ch_died_driver(nr,obj,ret);
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
        {{"buy",NULL},"Hey %s, use 'trade %s'!",0},
	{{"sell",NULL},"Hey %s, use 'trade %s'!",0},
        {{"what's","your","name",NULL},NULL,1},
	{{"what","is","your","name",NULL},NULL,1},
        {{"who","are","you",NULL},NULL,1}
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
					     case 1:	quiet_say(cn,"I'm %s.",ch[cn].name); break;
				}
				break;
			}
		}
	}
	

        return 42;
}

int abuser(int ID)
{
	switch(ID) {
		case 676:
		case 761:
		case 3154:
		case 3411:
		case 6699:
		case 8406:
		case 10645:
		case 11237:
		case 11372:
		case 11503:
		case 12619:
		case 14917:
		case 16691:
		case 17145:
		case 21917:
		case 22503:
		case 28763:
		case 30580:
		case 34385:
                case 34901:	return 1;
		default: 	return 0;
	}
}

struct merchant_driver_data
{
        int last_talk;
	int dir;

	int dayx;
	int dayy;
	int daydir;

	int nightx;
	int nighty;
	int nightdir;

	int ignore;

	int storefx;
	int storefy;
	int storetx;
	int storety;

	int doorx;
	int doory;

	int open;
	int close;

	int special;
	int pricemulti;
	int lastadd;

	int memcleartimer;
};

void merchant_driver_parse(int cn,struct merchant_driver_data *dat)
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
		else if (!strcmp(name,"ignore")) dat->ignore=atoi(value);
		else if (!strcmp(name,"storefx")) dat->storefx=atoi(value);
		else if (!strcmp(name,"storefy")) dat->storefy=atoi(value);
		else if (!strcmp(name,"storetx")) dat->storetx=atoi(value);
		else if (!strcmp(name,"storety")) dat->storety=atoi(value);
		else if (!strcmp(name,"doorx")) dat->doorx=atoi(value);
		else if (!strcmp(name,"doory")) dat->doory=atoi(value);
		else if (!strcmp(name,"open")) dat->open=atoi(value);
		else if (!strcmp(name,"close")) dat->close=atoi(value);
		else if (!strcmp(name,"special")) dat->special=atoi(value);
		else if (!strcmp(name,"pricemulti")) dat->pricemulti=atoi(value);
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

void merchant_driver(int cn,int ret,int lastact)
{
	struct merchant_driver_data *dat;
        int co,in,n;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_MERCHANTDRIVER,sizeof(struct merchant_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
		dat->open=6; dat->close=23;
		merchant_driver_parse(cn,dat);
		ch[cn].arg=NULL;
	}
	if (!ch[cn].store) {
		if (dat->pricemulti) create_store(cn,dat->ignore,dat->pricemulti);
		else create_store(cn,dat->ignore,400);
		if (dat->special) {
			for (n=0; n<5; n++) add_special_store(cn);
		}
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

			quiet_say(cn,"Hello %s! If you'd like to trade, say: '°c4%s, trade°c0!",ch[co].name,ch[cn].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

			if ((msg->dat1==1 || msg->dat1==2) && (co=msg->dat3)!=cn) {	// talk, and not our talk
				if (strcasestr((char*)msg->dat2,ch[cn].name) &&
				    strcasestr((char*)msg->dat2,"trade")) {
					/*if (abuser(ch[co].ID)) {
						switch(RANDOM(3)) {
                                                        case 0:		murmur(cn,"I hate cheaters."); break;
							case 1:		emote(cn,"clenches his fists and stares at %s.",ch[co].name); break;
							case 2:		murmur(cn,"I wish the cheaters would leave me alone."); break;
						}
					}*/
					ch[co].merchant=cn;
				}
			}			
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				// let it vanish
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
			if (secure_move_driver(cn,dat->nightx,dat->nighty,dat->nightdir,ret,lastact)) return;
		} else {	// we're open
                        if (dat->doorx && is_closed(dat->doorx,dat->doory)) {	// door is still closed
                                if (use_item_at(cn,dat->doorx,dat->doory,0)) return;
				do_idle(cn,TICKS);
				return;			
			}

			if (secure_move_driver(cn,dat->dayx,dat->dayy,dat->daydir,ret,lastact)) return;			
		}
	} else {		// just one position
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;		
	}

        if (ticker>dat->last_talk+TICKS*60 && !RANDOM(25)) {
		switch(RANDOM(11)) {
			case 0:		murmur(cn,"My back itches."); break;
			case 1:		whisper(cn,"There's something stuck between your teeth."); break;
			case 2:		murmur(cn,"Oh yeah, those were the days."); break;
			case 3:		murmur(cn,"Now where did I put it?"); break;
			case 4:		murmur(cn,"Oh my, life is hard but unfair."); break;
			case 5:		murmur(cn,"Beware of the fire snails!"); break;	
			case 6:		murmur(cn,"Ishtar! Oh, what has become of us!"); break;
			case 7:		murmur(cn,"The demons will get you."); break;
			case 8:		emote(cn,"scratches %s back",hisname(cn)); break;
			case 9:		if (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_INDOORS) emote(cn,"stares at the ceiling");
					else emote(cn,"stares at the sky");
					break;
			case 10:	emote(cn,"twiddles %s thumbs",hisname(cn)); break;

			default:	break;
		}
		
		dat->last_talk=ticker;
	}
	if (dat->special && ticker>dat->lastadd+TICKS*60*60*12) {
		add_special_store(cn);
		dat->lastadd=ticker;
	}

	if (ticker>dat->memcleartimer) {
		mem_erase_driver(cn,7);
		dat->memcleartimer=ticker+TICKS*60*60*12;
	}

        do_idle(cn,TICKS*2);
}

void merchant_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_MERCHANT:	merchant_driver(cn,ret,lastact); return 1;		
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
		case CDR_MERCHANT:	merchant_dead(cn,co); return 1;
		default:		return 0;
	}
}

















