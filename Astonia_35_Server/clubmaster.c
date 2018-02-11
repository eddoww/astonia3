/*
 * $Id: clubmaster.c,v 1.4 2007/10/29 13:57:31 devel Exp $
 *
 * $Log: clubmaster.c,v $
 * Revision 1.4  2007/10/29 13:57:31  devel
 * removed paying player check
 *
 * Revision 1.3  2007/07/04 09:19:58  devel
 * removed need for payment to found clubs
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
#include "sector.h"
#include "item_id.h"
#include "libload.h"
#include "drvlib.h"
#include "create.h"
#include "clan.h"
#include "lookup.h"
#include "task.h"
#include "database.h"
#include "chat.h"
#include "player.h"
#include "mem.h"
#include "consistency.h"
#include "map.h"
#include "player_driver.h"
#include "club.h"

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
	{{"club",NULL},"Say 'found: <club name>' to found a club. The first weekly payment of 500g is due immediately.",0},
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
					     case 1:	quiet_say(cn,"I'm %s.",ch[cn].name);
					     default:	return qa[q].answer_code;
				}
				break;
			}
		}
	}
	

        return 42;
}


//-----------------------

struct clubmaster_driver_data
{
        int last_talk;
	int dir;

        char accept[80];
	int accept_clan;
	int accept_cn;
	char join[80];

	char new_name[80];
	int new_co;
	int new_ID;
	int new_timeout;

	int memcleartimer;
};

void clubmaster_driver_parse(int cn,struct clubmaster_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"dir")) dat->dir=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void clubmaster_driver(int cn,int ret,int lastact)
{
	struct clubmaster_driver_data *dat;
        int co,in,n,rank,cc,val;
        struct msg *msg,*next;
	char *ptr,tmp[80],name[80];

        dat=set_data(cn,DRD_CLUBMASTERDRIVER,sizeof(struct clubmaster_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
                clubmaster_driver_parse(cn,dat);
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

			if (!get_char_club(cn) && !get_char_clan(cn)) quiet_say(cn,"Hello %s! Would you like to found a °c4club°c0?",ch[co].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

                        if ((msg->dat1==1 || msg->dat1==2) && (co=msg->dat3)!=cn) {	// talk, and not our talk				
                                if ((ptr=strcasestr((char*)msg->dat2,"found:"))) {
					if (!get_char_clan(co) && !get_char_club(co)) {
						if (ch[co].gold>=500*100) {
							ptr+=6;
							while (isspace(*ptr)) ptr++;
							for (n=0; n<79; n++) {
								if (!(isalpha(*ptr) || *ptr==' ')) break;
								name[n]=*ptr++;
							}
							name[n]=0;
							
							if ((n=create_club(name))) {
								take_money(co,500*100);
								ch[co].clan=n+CLUBOFFSET;
								ch[co].clan_serial=club[n].serial;
								ch[co].clan_rank=2;
								quiet_say(cn,"Congratulations, %s, you are now the leader of the club %s.",ch[co].name,club[n].name);
								dlog(co,0,"created club %d %s",n,club[n].name);
							} else quiet_say(cn,"Something's wrong with the name.");
						} else quiet_say(cn,"You cannot pay the fee of 10,000 gold.");
					} else quiet_say(cn,"You are already a member of a clan or club. You cannot found a new one.");
				}
				if ((ptr=strcasestr((char*)msg->dat2,"accept:"))) {
					if (!get_char_club(co) || ch[co].clan_rank<1) {
                                                quiet_say(cn,"You are not a club leader, %s.",ch[co].name);
					} else {
						ptr+=7;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"') break;
							dat->accept[n]=*ptr++;
						}
						dat->accept[n]=0;
						strcpy(dat->join,ch[co].name);
						dat->accept_clan=get_char_club(co);
						dat->accept_cn=co;

						quiet_say(cn,"To join %s's club %s, say: 'join: %s'",dat->join,dat->accept,dat->join);
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"join:"))) {
					if (get_char_clan(co) || get_char_club(co)) {
                                                quiet_say(cn,"You are already a clan or club member, %s.",ch[co].name);
					} else {
						ptr+=5;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"') break;
							tmp[n]=*ptr++;
						}
						tmp[n]=0;
                                                if (strcasecmp(dat->accept,ch[co].name)) {
							quiet_say(cn,"You have not been invited, %s.",ch[co].name);
						} else if (strcasecmp(dat->join,tmp)) {
							quiet_say(cn,"%s has not invited you, %s.",tmp,ch[co].name);
						} else {
                                                        //add_member(co,dat->accept_clan,dat->join);
							ch[co].clan=dat->accept_clan+CLUBOFFSET;
							ch[co].clan_serial=club[dat->accept_clan].serial;
							ch[co].clan_rank=0;
							quiet_say(cn,"%s, you are now a member of %s's club.",ch[co].name,dat->join);
							dat->accept[0]=0;
							dat->accept_clan=0;
							dat->join[0]=0;
						}
						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"leave!"))) {
					if (!get_char_club(co)) {
                                                quiet_say(cn,"You are not a club member, %s.",ch[co].name);
					} else {
						remove_member(co,co);
						quiet_say(cn,"You are no longer a member of any club, %s",ch[co].name);
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"rank:"))) {
					if (!get_char_club(co) || ch[co].clan_rank<2) {
                                                quiet_say(cn,"You are not a club founder, %s.",ch[co].name);
					} else {
						ptr+=6;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"' || isspace(*ptr)) break;
							tmp[n]=*ptr++;
						}
						tmp[n]=0;
						
						rank=atoi(ptr);

						if (rank<0 || rank>1) {
							quiet_say(cn,"You must use a rank between 0 and 1.");
							remove_message(cn,msg);
							continue;
						}

						for (cc=getfirst_char(); cc; cc=getnext_char(cc)) {
							if (!strcasecmp(tmp,ch[cc].name) && (ch[cc].flags&CF_PLAYER)) break;
						}
						if (cc) {
							if (ch[cc].clan_rank==2) {
								quiet_say(cn,"%s is the club's founder, cannot change rank.",ch[cc].name);
							} else if (get_char_club(cc)==get_char_club(co)) {
								ch[cc].clan_rank=rank;
								quiet_say(cn,"Set %s's rank to %d.",ch[cc].name,rank);
							} else quiet_say(cn,"You cannot change the rank of those not belonging to your club.");
						} else {
							int uID;

							uID=lookup_name(tmp,NULL);
							if (uID==0) continue;
							if (uID==-1) {
								quiet_say(cn,"Sorry, no player by the name %s found.",tmp);
							} else {
								task_set_clan_rank(uID,ch[co].ID,get_char_club(co)+CLUBOFFSET,rank,ch[co].name);
								quiet_say(cn,"Update scheduled (%s,%d).",tmp,rank);
							}
						}
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"fire:"))) {
					if (!get_char_club(co) || ch[co].clan_rank<1) {
                                                quiet_say(cn,"You are not a club leader, %s.",ch[co].name);
					} else {
						ptr+=6;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"' || isspace(*ptr)) break;
							tmp[n]=*ptr++;
						}
						tmp[n]=0;
						
                                                for (cc=getfirst_char(); cc; cc=getnext_char(cc)) {
							if (!strcasecmp(tmp,ch[cc].name) && (ch[cc].flags&CF_PLAYER)) break;
						}
						if (cc) {
							if (get_char_club(cc)==get_char_club(co)) {
								if (ch[cc].clan_rank<2) {
									remove_member(cc,co);
									quiet_say(cn,"Fired: %s.",ch[cc].name);
								} else quiet_say(cn,"You cannot fire the founder of the club.");
							} else quiet_say(cn,"You cannot fire those not belonging to your club.");
						} else {
							int uID;

							uID=lookup_name(tmp,NULL);
							if (uID==0) continue;
							if (uID==-1) {
								quiet_say(cn,"Sorry, no player by the name %s found.",tmp);
							} else {
								task_fire_from_clan(uID,ch[co].ID,get_char_club(co)+CLUBOFFSET,ch[co].name);
								quiet_say(cn,"Update scheduled (%s).",tmp);
							}
						}
					}
				}

				if ((ptr=strcasestr((char*)msg->dat2,"deposit:"))) {
					if (!(n=get_char_club(co))) {
                                                quiet_say(cn,"You are not a club member, %s.",ch[co].name);
					} else {
						val=atoi(ptr+8)*100;

						if (val>0 && ch[co].gold>=val) {
							club[n].money+=val;
							take_money(co,val);
							quiet_say(cn,"You have deposited %dG, for a total of %dG, %s.",val/100,club[n].money/100,ch[co].name);
							dlog(co,0,"Deposited %dG into club %d, for a new total of %dG",val/100,n,club[n].money/100);
							db_update_club(n);
						} else quiet_say(cn,"You do not have that much gold, %s.",ch[co].name);
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"withdraw:"))) {
					if (!(n=get_char_club(co)) || ch[co].clan_rank<2) {
                                                quiet_say(cn,"You are not a club founder, %s.",ch[co].name);
					} else {
						val=atoi(ptr+9)*100;

						if (val>0 && club[n].money>=val) {
							club[n].money-=val;
							give_money(co,val,"club withdrawal");
							quiet_say(cn,"You have withdrawn %dG, money left in club %dG, %s.",val/100,club[n].money/100,ch[co].name);
							dlog(co,0,"Withdrew %dG from club %d, for a new total of %dG",val/100,n,club[n].money/100);
							db_update_club(n);
						} else quiet_say(cn,"The club does not have that much gold, %s.",ch[co].name);
					}
				}
			}			
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				// try to give it back
                                if (give_char_item(cn,co)) return;
				
				// didnt work, let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;

        if (ticker>dat->last_talk+TICKS*60 && !RANDOM(25)) {
		switch(RANDOM(8)) {
			case 0:		murmur(cn,"My back itches."); break;
			case 1:		whisper(cn,"There's something stuck between your teeth."); break;
			case 2:		murmur(cn,"Oh yeah, those were the days."); break;
			case 3:		murmur(cn,"Now where did I put it?"); break;
			case 4:		murmur(cn,"Oh my, life is hard but unfair."); break;
                        case 5:		murmur(cn,"Beware of the fire snails!"); break;
			case 6:         murmur(cn,"I love the clicking of coins."); break;
			case 7:		murmur(cn,"Gold and Silver, Silver and Gold."); break;
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

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_CLUBMASTER:	clubmaster_driver(cn,ret,lastact); return 1;

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
		case CDR_CLUBMASTER:	return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_CLUBMASTER:	return 1;

		default:		return 0;
	}
}















