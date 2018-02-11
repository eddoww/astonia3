/*
 * $Id: arena.c,v 1.7 2007/07/13 15:47:16 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: arena.c,v $
 * Revision 1.7  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.6  2007/06/26 12:40:39  devel
 * fixed orientation
 *
 * Revision 1.5  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.4  2005/12/10 21:01:35  ssim
 * fixed another loophole and a bug in arena manager
 *
 * Revision 1.3  2005/12/10 14:59:30  ssim
 * changed arena policy to not re-rent while anybody is in it
 * made arena "owner" lose his status when he leaves the arena
 *
 * Revision 1.2  2005/12/01 14:43:30  ssim
 * made small arena manager kick out all currently inside his arena when executing 'rent' command
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
#include "consistency.h"

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
	{{"restart",NULL},NULL,2},
	{{"please","repeat",NULL},NULL,2},
	{{"please","restart",NULL},NULL,2},
	{{"register",NULL},NULL,3},
	{{"enter",NULL},NULL,4},
	{{"leave",NULL},NULL,5},
	{{"rent",NULL},NULL,6}
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
void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

struct manager_data
{
	int last_talk;
        int amgivingback;

	int renter;
	int timeout;
	char invite[80];

	int arena_x;
	int arena_y;

	int arena_fx;
	int arena_fy;

	int arena_tx;
	int arena_ty;
};

void manager_parse(int cn,struct manager_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"arenax")) dat->arena_x=atoi(value);
		else if (!strcmp(name,"arenay")) dat->arena_y=atoi(value);
		else if (!strcmp(name,"arenafx")) dat->arena_fx=atoi(value);
		else if (!strcmp(name,"arenafy")) dat->arena_fy=atoi(value);
		else if (!strcmp(name,"arenatx")) dat->arena_tx=atoi(value);
		else if (!strcmp(name,"arenaty")) dat->arena_ty=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

int is_anybody_in(int cn,int fx,int fy,int tx,int ty)
{
	int x,y,co;

	for (y=fy; y<=ty; y++) {
		for (x=fx; x<=tx; x++) {
			if ((co=map[x+y*MAXMAP].ch) && (ch[co].flags&CF_PLAYER)) {
				return 1;
			}
		}
	}
	return 0;
}

void manager_driver(int cn,int ret,int lastact)
{
	struct manager_data *dat;
        int co,in,talkdir=0,didsay=0,n;
	struct msg *msg,*next;	
	char *ptr;

        dat=set_data(cn,DRD_ARENAMANAGER,sizeof(struct manager_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) manager_parse(cn,dat);
			ch[cn].arg=NULL;
			dat->timeout=-TICKS*60*5;
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (co==dat->renter && (ch[co].x<232 || ch[co].x>238 || ch[co].y<dat->arena_fy || ch[co].y>dat->arena_ty)) {
				dat->renter=0;
				dat->invite[0]=0;
			}

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (ch[co].x<=dat->arena_fx || ch[co].x>=dat->arena_tx ||
			    ch[co].y<=dat->arena_fy || ch[co].y>=dat->arena_ty)
				{ remove_message(cn,msg); continue; }

                        switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 6:		// rent
                                                if (is_anybody_in(cn,232,dat->arena_fy,238,dat->arena_ty)) {
							say(cn,"Sorry, this arena is already occupied.");
							break;
						}
						dat->renter=co;
						dat->invite[0]=0;
						dat->timeout=ticker+TICKS*60*5;
						
						teleport_char_driver(co,dat->arena_x,dat->arena_y);
						say(cn,"Say 'invite: <name>' to let someone in, or say 'leave' to leave the arena.");
                                                break;
                                case 5:		//leave
						teleport_char_driver(co,ch[cn].x,ch[cn].y);
						if (co==dat->renter) {
							dat->renter=0;
							dat->invite[0]=0;
						}
						break;
				case 4:		// enter
						if (!strcasecmp(ch[co].name,dat->invite)) {
							teleport_char_driver(co,dat->arena_x,dat->arena_y);
							dat->invite[0]=0;
						} else say(cn,"You have not been invited, %s.",ch[co].name);
						break;
	
			}
			if ((ptr=strcasestr((char*)msg->dat2,"invite:"))) {				
				if (co!=dat->renter) {
					say(cn,"This is not your arena, %s.",ch[co].name);
				} else {
					ptr+=7;
					while (isspace(*ptr)) ptr++;
					for (n=0; n<79; n++) {
						if (!*ptr || *ptr=='"') break;
						dat->invite[n]=*ptr++;
					}
					dat->invite[n]=0;

					say(cn,"%s, say 'enter' if you wish to enter the arena",dat->invite);
				}
			}
                        if (didsay) {
				dat->last_talk=ticker;
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);				
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				if (!dat->amgivingback) {					
					say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					dat->amgivingback=1;
				} else dat->amgivingback++;
				
				if (dat->amgivingback<20 && give_driver(cn,co)) return;
				
				// let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	dat->amgivingback=0;

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*10<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_ARENAMANAGER:	manager_driver(cn,ret,lastact); return 1;

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
                case CDR_ARENAMANAGER:	immortal_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:		return 0;
	}
}






