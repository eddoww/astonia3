/*
 * $Id: clanmaster.c,v 1.26 2008/04/21 09:08:50 devel Exp $
 *
 * $Log: clanmaster.c,v $
 * Revision 1.26  2008/04/21 09:08:50  devel
 * added unlocked spawner logic
 *
 * Revision 1.25  2008/03/29 15:45:24  devel
 * moved automated clan messages from channel 5 to channel 10
 *
 * Revision 1.24  2008/02/04 10:12:38  devel
 * spawner message no longer go through walls
 *
 * Revision 1.23  2008/01/03 10:10:11  devel
 * changed spawner jewel take logic
 *
 * Revision 1.22  2007/12/30 12:49:37  devel
 * fixed cn/co bug
 *
 * Revision 1.21  2007/12/03 10:06:59  devel
 * changed formula for jewels stolen per raid
 *
 * Revision 1.20  2007/11/04 12:59:25  devel
 * new clan bonuses
 *
 * Revision 1.19  2007/10/29 13:57:16  devel
 * max jewel size 255
 *
 * Revision 1.18  2007/08/09 11:11:31  devel
 * clarified clan foundatio message
 *
 * Revision 1.17  2007/07/27 05:58:38  devel
 * spawn every 3 1/2 hours, 3 groups
 *
 * Revision 1.16  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.15  2007/07/09 11:20:08  devel
 * bugfix in clan founding logic
 *
 * Revision 1.14  2007/07/01 12:08:56  devel
 * killing the vault now gives the attacker 10% of the defenders stock but no more tan 50% of the attacker's stock but at least 10
 *
 * Revision 1.13  2007/06/29 10:19:30  devel
 * added warning to clanmaster not ot leave the area right away after founding a clan
 *
 * Revision 1.12  2007/06/28 12:10:52  devel
 * clan founding now has the ssame requirements as joining
 *
 * Revision 1.11  2007/06/22 13:01:45  devel
 * fixed item duplication bug
 *
 * Revision 1.10  2007/06/07 15:38:22  devel
 * re-added check for previous clan memberships
 *
 * Revision 1.9  2007/05/02 12:32:50  devel
 * removed paid player check
 *
 * Revision 1.8  2007/02/28 09:51:25  devel
 * removed potion usage from clanvault
 *
 * Revision 1.7  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.6  2005/12/01 16:31:09  ssim
 * added clan message
 *
 * Revision 1.5  2005/10/04 18:08:01  ssim
 * fixed yet another bug in clan jewel respawn logic
 * some cleanup of clanspawn_driver()
 *
 * Revision 1.4  2005/09/27 07:43:20  root
 * fixed bug in jewel spawn logic
 *
 * Revision 1.3  2005/09/25 15:37:08  ssim
 * added char_see_char call to text commands of clan clerk (avoids shouting commands accidentally reaching the wrong clerk)
 *
 * Revision 1.2  2005/09/24 15:00:23  ssim
 * removed debug logging from clan spawner
 *
 * Revision 1.1  2005/09/24 09:48:53  ssim
 * Initial revision
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
#include "club.h"
#include "lookup.h"
#include "task.h"
#include "database.h"
#include "chat.h"
#include "player.h"
#include "mem.h"
#include "consistency.h"
#include "map.h"
#include "player_driver.h"
#include "act.h"

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
	{{"clan",NULL},"If you wish to found a clan, tell me the name you want that clan to have, and hand me a Clan Jewel. If you wish to tell me the name, use: 'name: <clan name>', that is, to name your clan 'Black Rose', use: 'name: Black Rose'. Be aware that the game will use the phrase 'The <clan name> clan', ie. 'The Black Rose Clan', so avoid 'The' and 'Clan' in the name.",0},
	{{"jewels",NULL},NULL,2},
	{{"repeat",NULL},NULL,3},
	{{"raid",NULL},"I will enter the clan you name, kill any guards I see and try to steal a clan jewel. If I succeed I will transfer that jewel to your clan vault. I can only attack a clan if you are at war with that clan. If you want me to attack clan 2, say 'attack 2'.",0},
	{{"scout",NULL},"On a scouting mission, I will just take a peek into the clan you name and give you a report about its guards. Say 'sneak 2' if you want me to scout clan number 2.",0},
	{{"info",NULL},NULL,4}
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

struct clanmaster_driver_data
{
        int last_talk;
	int dir;

        char accept[80];
	int accept_clan;
	int accept_cn;
	char join[80];

        int memcleartimer;
};

void clanmaster_driver_parse(int cn,struct clanmaster_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"dir")) dat->dir=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

struct clan_found_data
{
	int state;

	char name[80];
};

void clanmaster_driver(int cn,int ret,int lastact)
{
	struct clanmaster_driver_data *dat;
	struct clan_found_data *fnd;
        int co,in,n,rank,cc,res,cnr;
        struct msg *msg,*next;
	char *ptr,tmp[80],errbuff[256];

        dat=set_data(cn,DRD_CLANMASTERDRIVER,sizeof(struct clanmaster_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
                clanmaster_driver_parse(cn,dat);
		ch[cn].arg=NULL;
	}

	cnr=((ch[cn].x-2)/49)*12+(ch[cn].y-2)/20+1;
	ch[cn].clan=cnr;
	ch[cn].clan_serial=clan_serial(cnr);

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
						
			if (!get_char_club(co) && !get_char_clan(co) && clanslot_free(cnr)) quiet_say(cn,"Hello %s! Would you like to found a °c4clan°c0?",ch[co].name);
			else if (get_char_clan(co)==cnr) quiet_say(cn,"Welcome to your clan hall, %s.",ch[co].name);
			else if (!clanslot_free(cnr)) quiet_say(cn,"Welcome to the %s clan hall, %s.",get_clan_name(cnr),ch[co].name);
			
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			
			co=msg->dat3;
			
			analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

			if (char_dist(cn,co)>12) { remove_message(cn,msg); continue; }

                        if ((msg->dat1==1 || msg->dat1==2) && (co=msg->dat3)!=cn) {	// talk, and not our talk				
                                if ((ptr=strcasestr((char*)msg->dat2,"name:")) && (fnd=set_data(co,DRD_CLANFOUND,sizeof(struct clan_found_data)))) {
					if (!clanslot_free(cnr)) {
						quiet_say(cn,"I'm sorry, %s, but this clan hall is already taken.",ch[co].name);
					} else if (!may_join_clan(co,errbuff) && !(ch[co].flags&CF_GOD)) {
						quiet_say(cn,"You cannot create a clan yet, %s. %s",ch[co].name,errbuff);
					} else if (!get_char_clan(co) && !get_char_club(co)) {
						ptr+=5;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"') break;
							fnd->name[n]=*ptr++;
						}
						fnd->name[n]=0;
						fnd->state=1;
						quiet_say(cn,"Your clan, %s, will be named '%s'. Try again if that is not what you want. Or hand me a Clan Jewel to proceed.",ch[co].name,fnd->name);
					} else quiet_say(cn,"You are already a member of a clan or club. You cannot found a new one.");
				}
				if ((ptr=strcasestr((char*)msg->dat2,"accept:"))) {
					if (!get_char_clan(co) || ch[co].clan_rank<2) {
                                                quiet_say(cn,"You are not a clan leader, %s.",ch[co].name);
					} else {
						ptr+=7;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"') break;
							dat->accept[n]=*ptr++;
						}
						dat->accept[n]=0;
						strcpy(dat->join,ch[co].name);
						dat->accept_clan=get_char_clan(co);
						dat->accept_cn=co;

						quiet_say(cn,"To join %s's clan %s, say: 'join: %s'",dat->join,dat->accept,dat->join);
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"join:"))) {
					if (get_char_clan(co) || get_char_club(co)) {
                                                quiet_say(cn,"You are already a clan member, %s.",ch[co].name);
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
                                                        if (may_join_clan(co,errbuff)) {
								add_member(co,dat->accept_clan,dat->join);
								quiet_say(cn,"%s, you are now a member of %s's clan.",ch[co].name,dat->join);
							} else {
								quiet_say(cn,"You cannot join another clan yet, %s. %s",ch[co].name,errbuff);
							}
							dat->accept[0]=0;
							dat->accept_clan=0;
							dat->join[0]=0;
						}
						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"leave!"))) {
					if (!get_char_clan(co)) {
                                                quiet_say(cn,"You are not a clan member, %s.",ch[co].name);
					} else {
						remove_member(co,co);
						quiet_say(cn,"You are no longer a member of any clan, %s",ch[co].name);
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"rank:"))) {
					if (!get_char_clan(co) || ch[co].clan_rank<4) {
                                                quiet_say(cn,"You are not a clan leader, %s.",ch[co].name);
					} else {
						ptr+=6;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"' || isspace(*ptr)) break;
							tmp[n]=*ptr++;
						}
						tmp[n]=0;
						
						rank=atoi(ptr);

						if (rank<0 || rank>4) {
							quiet_say(cn,"You must use a rank between 0 and 4.");
							remove_message(cn,msg);
							continue;
						}

						for (cc=getfirst_char(); cc; cc=getnext_char(cc)) {
							if (!strcasecmp(tmp,ch[cc].name) && (ch[cc].flags&CF_PLAYER)) break;
						}
						if (cc) {
							if (get_char_clan(cc)==get_char_clan(co)) {
								ch[cc].clan_rank=rank;
								add_clanlog(ch[cc].clan,clan_serial(ch[cc].clan),ch[cc].ID,30,"%s rank was set to %d by %s",ch[cc].name,rank,ch[co].name);
								quiet_say(cn,"Set %s's rank to %d.",ch[cc].name,rank);
							} else quiet_say(cn,"You cannot change the rank of those not belonging to your clan.");
						} else {
							int uID;

							uID=lookup_name(tmp,NULL);
							if (uID==0) continue;
							if (uID==-1) {
								quiet_say(cn,"Sorry, no player by the name %s found.",tmp);
							} else {
								task_set_clan_rank(uID,ch[co].ID,get_char_clan(co),rank,ch[co].name);
								quiet_say(cn,"Update scheduled (%s,%d).",tmp,rank);
							}
						}
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"fire:"))) {
					if (!get_char_clan(co) || ch[co].clan_rank<4) {
                                                quiet_say(cn,"You are not a clan leader, %s.",ch[co].name);
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
							if (get_char_clan(cc)==get_char_clan(co)) {
								remove_member(cc,co);
								quiet_say(cn,"Fired: %s.",ch[cc].name);
							} else quiet_say(cn,"You cannot fire those not belonging to your clan.");
						} else {
							int uID;

							uID=lookup_name(tmp,NULL);
							if (uID==0) continue;
							if (uID==-1) {
								quiet_say(cn,"Sorry, no player by the name %s found.",tmp);
							} else {
								task_fire_from_clan(uID,ch[co].ID,get_char_clan(co),ch[co].name);
								quiet_say(cn,"Update scheduled (%s).",tmp);
							}
						}
					}
				}
			}			
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_CLANJEWEL && (fnd=set_data(co,DRD_CLANFOUND,sizeof(struct clan_found_data)))) {
                                        if (fnd->state==1) {
						res=found_clan(fnd->name,co,cnr);
						if (!res) {
							quiet_say(cn,"So be it. There will be a new clan, named '%s', and you, %s, shall be its new master. Good luck, young master!",fnd->name,ch[co].name);
							quiet_say(cn,"°c3Stay with me for a few minutes till the paperwork is finished, %s. If you stray to far, news of your clan might not have reached there, and you might lose your clan membership. (Waiting 5 minutes will be sufficient. Please use your own clock, the clanmaster will not tell you when the time is up. Fixing this is on Ishtar's Todo-List)",ch[co].name);
							fnd->state=0;

							add_member(co,cnr,ch[co].name);
                                                        ch[co].clan_rank=4;

							destroy_item(ch[cn].citem);
							ch[cn].citem=0;
							remove_message(cn,msg);
							continue;
						} else {
							quiet_say(cn,"There was an error creating your clan. Please try again.");							
						}
					} else {
						quiet_say(cn,"You must name your clan first. Say: 'name: <clan-name>'.");
					}
				}
                                // try to give it back
                                if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
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

void clanclerk_driver(int cn,int ret,int lastact)
{
        int co,in,res,nr,level,n,cnr;
        struct msg *msg,*next;
	char *ptr,name[40];

	cnr=((ch[cn].x-2)/49)*12+(ch[cn].y-2)/20+1;
	ch[cn].clan=cnr;
	ch[cn].clan_serial=clan_serial(cnr);

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		// did we see someone?
		if (msg->type==NT_CHAR) {
			
                        ; //co=msg->dat1;			
		}

                // talk back
		if (msg->type==NT_TEXT) {
			res=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

			switch(res) {
				case 2:	say(cn,"Our clan has %d jewels.",cnt_jewels(cnr)); break;
			}
			
			if ((msg->dat1==1 || msg->dat1==2) && (co=msg->dat3)!=cn && char_see_char(cn,co)) {	// talk, and not our talk
				if ((ptr=strcasestr((char*)msg->dat2,"deposit"))) {
					
					ptr+=7;

					nr=atoi(ptr);

					if (nr<1) {
						say(cn,"You must name a positive amount.");

						remove_message(cn,msg);
						continue;
					}
					
					if (ch[co].gold<nr*100) {
						say(cn,"You do not have that much money, %s.",ch[co].name);
						remove_message(cn,msg);
						continue;
					}
					ch[co].gold-=nr*100;
					ch[co].flags|=CF_ITEMS;
					clan_money_change(cnr,nr,co);
					say(cn,"You have deposited %dG.",nr);
					dlog(co,0,"deposited %dG to clan %d.",nr,cnr);
				}
                                // members only
				if (get_char_clan(co)!=cnr) {
					remove_message(cn,msg);
					continue;
				}
				// treasurer or better
				if (ch[co].clan_rank<3) {
					remove_message(cn,msg);
					continue;
				}
				if ((ptr=strcasestr((char*)msg->dat2,"withdraw"))) {
					
					ptr+=8;

					nr=atoi(ptr);

					if (nr<1) {
						say(cn,"You must name a positive amount.");

						remove_message(cn,msg);
						continue;
					}
					
					if (get_clan_money(cnr)<nr) {
						say(cn,"Your clan does not have that much money, %s.",ch[co].name);
						remove_message(cn,msg);
						continue;
					}
					clan_money_change(cnr,-nr,co);
					ch[co].gold+=nr*100;
					ch[co].flags|=CF_ITEMS;
					say(cn,"You have withdrawn %dG.",nr);
					dlog(co,0,"withdrew %dG from clan %d.",nr,cnr);
				}

				// leader only
				if (ch[co].clan_rank<4) {
					remove_message(cn,msg);
					continue;
				}
                                if ((ptr=strcasestr((char*)msg->dat2,"set bonus"))) {

                                        ptr+=9;
					nr=atoi(ptr);
					
					while (isspace(*ptr)) ptr++;
					while (isdigit(*ptr)) ptr++;
					while (isspace(*ptr)) ptr++;
			
					level=atoi(ptr);

					if (nr<0 || nr>4) {
						say(cn,"Number must be between 0 and 4");

						remove_message(cn,msg);
						continue;
					}

					if (level<0 || level>20) {
						say(cn,"Level must be between 0 and 20");

						remove_message(cn,msg);
						continue;
					}

                                        res=set_clan_bonus(cnr,nr,level,co);
                                        if (!res) {
						say(cn,"Set bonus %d to %d.",nr,level);
						
                                                remove_message(cn,msg);
						continue;
					} else  {
						say(cn,"Failed to set bonus %d.",nr);

						remove_message(cn,msg);
						continue;
					}
				}
                                if ((ptr=strcasestr((char*)msg->dat2,"relation"))) {

                                        ptr+=8;
					nr=atoi(ptr);
					
					while (isspace(*ptr)) ptr++;
					while (isdigit(*ptr)) ptr++;
					while (isspace(*ptr)) ptr++;
			
					level=atoi(ptr);

					if (nr<1 || nr>63) {
						say(cn,"Number must be between 1 and 63");

						remove_message(cn,msg);
						continue;
					}

					if (level<1 || level>5) {
						say(cn,"Level must be between 1 and 5");

						remove_message(cn,msg);
						continue;
					}

                                        res=set_clan_relation(cnr,nr,level,co);
                                        if (!res) {
						say(cn,"Changed relation.");
						
                                                remove_message(cn,msg);
						continue;
					} else  {
						say(cn,"Failed to change relation.");						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"rank name"))) {
					
					ptr+=9;

					nr=atoi(ptr);

					if (nr<0 || nr>4) {
						say(cn,"Number must be between 0 and 4");

						remove_message(cn,msg);
						continue;
					}
					
					while (isspace(*ptr)) ptr++;
					while (isdigit(*ptr)) ptr++;
					while (isspace(*ptr)) ptr++;
			
                                        for (n=0; n<39; n++) {
						if (!*ptr || *ptr=='"') break;
						name[n]=*ptr++;
					}
					name[n]=0;

					res=set_clan_rankname(cnr,nr,name,co);

					if (!res) {
						say(cn,"Changed rank name.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed to change rank name.");						
					}
				}
                                if ((ptr=strcasestr((char*)msg->dat2,"website"))) {

                                        ptr+=7;
                                        while (isspace(*ptr)) ptr++;
			
                                        res=set_clan_website(cnr,ptr,co);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"message"))) {

                                        ptr+=7;
                                        while (isspace(*ptr)) ptr++;
			
                                        res=set_clan_message(cnr,ptr,co);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_CLANJEWEL) {
					res=add_jewel(cnr,co,it[in].drdata[4]);
					
					if (!res) {
						say(cn,"Added one jewel worth %d points to our storage.",it[in].drdata[4]);
						
						destroy_item(ch[cn].citem);
						ch[cn].citem=0;
						remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed to add jewel to storage, please try again.");						
					}
				}				
                                // try to give it back
                                if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHTDOWN,ret,lastact)) return;

        do_idle(cn,TICKS*2);
}

void clanmaster_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

#define TAKETIME 120
int clanspawn_reg(int in,int co)
{
	if (*(unsigned int*)(it[in].drdata+20)==co && *(unsigned int*)(it[in].drdata+24)==ch[co].ID) return 1;
	
	*(unsigned int*)(it[in].drdata+20)=co;
	*(unsigned int*)(it[in].drdata+24)=ch[co].ID;
	*(unsigned int*)(it[in].drdata+28)=ticker;

	return 1;
}

/* drdata usage:
+ 0: max level
+ 2: jewel count
+ 4: init done
+ 8: bigspawn start
+12: bigspawn end
+16: idx
+20: reserved for player cn
+24: reserved for player ID
+28: reserve timer
+32: last announce
+36: mode switch
*/
// spawns at levels: 5 15 18 20 24 28 30 34 36 38   40 43 42 44 45 46 48 50 52 60   64 68 76 84 92 100 108 160 200
void clanspawn_driver(int in,int cn)
{
	int in2,diff,value,co,cnt=0,t,mode=0;
	char *cname,buf[256];

	value=10+it[in].drdata[0]/10;
	if (realtime>=*(unsigned int*)(it[in].drdata+36)) mode=1;
	else mode=0;
	/*if (cn) {
		log_char(cn,LOG_SYSTEM,0,"%.2f min mode=%d",(*(unsigned int*)(it[in].drdata+36)-realtime)/60.0,mode);
	}*/

        if (!cn) {	// timer calls
		call_item(IDR_CLANSPAWN,in,0,ticker+TICKS*2);

		// -- jewel getting stuff --

		if ((co=map[it[in].x+it[in].y*MAXMAP+1].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-1].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+1+MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+1-MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-1+MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-1-MAXMAP].ch)) cnt+=clanspawn_reg(in,co);

		
		if ((co=map[it[in].x+it[in].y*MAXMAP+2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+2+MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+2-MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-2+MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-2-MAXMAP].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+1+MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+1-MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-1+MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-1-MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+2+MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP+2-MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-2+MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		if ((co=map[it[in].x+it[in].y*MAXMAP-2-MAXMAP*2].ch)) cnt+=clanspawn_reg(in,co);
		

		if (ticker-*(unsigned int*)(it[in].drdata+32)>TICKS*10) {
			co=*(unsigned int*)(it[in].drdata+20);
			t=(ticker-*(unsigned int*)(it[in].drdata+28))/TICKS;

			if (mode==1) {
				log_area(it[in].x,it[in].y,LOG_TALK,0,24,"Spawner unlocked.");
			} else if (co && (ch[co].flags&CF_PLAYER) && ch[co].ID==*(unsigned int*)(it[in].drdata+24)) {
				if (t<TAKETIME) {
					log_area(it[in].x,it[in].y,LOG_TALK,0,24,"Spawner locked; %s can get the jewel in %d seconds.",
						 ch[co].name,
						 TAKETIME-t);
				} else if (t>=TAKETIME && t<TAKETIME*2) {
					log_area(it[in].x,it[in].y,LOG_TALK,0,24,"Spawner free; %s can take the jewel.",
						 ch[co].name);
				} else {
					log_area(it[in].x,it[in].y,LOG_TALK,0,24,"Spawner locked.");
					*(unsigned int*)(it[in].drdata+20)=0;
					*(unsigned int*)(it[in].drdata+24)=0;
				}
			} else {
				log_area(it[in].x,it[in].y,LOG_TALK,0,24,"Spawner locked.");
			}

			*(unsigned int*)(it[in].drdata+32)=ticker;
		}
		
                // -- jewel spawning stuff --

		// init: determine first bigspawn time
		if (!*(unsigned int*)(it[in].drdata+4)) {
			*(unsigned int*)(it[in].drdata+4)=1;
			*(unsigned int*)(it[in].drdata+8)=(realtime/(BIGSPAWNFREQ))*BIGSPAWNFREQ+BIGSPAWNFREQ;
			*(unsigned int*)(it[in].drdata+12)=(realtime/(BIGSPAWNFREQ))*BIGSPAWNFREQ+BIGSPAWNFREQ+60*60;			
			*(unsigned int*)(it[in].drdata+16)=(realtime/BIGSPAWNFREQ)%3;
		}
		
                if (realtime>=*(unsigned int*)(it[in].drdata+8)) {
			int idx=*(unsigned int*)(it[in].drdata+16);
			
			if ((idx==0 && it[in].drdata[0]<40) ||
			    (idx==1 && it[in].drdata[0]>=40 && it[in].drdata[0]<64) ||
			    (idx==2 && it[in].drdata[0]>=64)) {
				it[in].drdata[2]=1;
				it[in].sprite++;
				set_sector(it[in].x,it[in].y);
			}

                        *(unsigned int*)(it[in].drdata+8)+=BIGSPAWNFREQ;
                        *(unsigned int*)(it[in].drdata+16)=(realtime/BIGSPAWNFREQ)%3;
			*(unsigned int*)(it[in].drdata+36)=realtime+15*60+RANDOM(15*60)+RANDOM(15*60);
		}

		if (realtime>=*(unsigned int*)(it[in].drdata+12)) {
			if (it[in].drdata[2]>0) {
				it[in].drdata[2]=0;
				it[in].sprite--;
				set_sector(it[in].x,it[in].y);
			}

			*(unsigned int*)(it[in].drdata+12)+=BIGSPAWNFREQ;
		}
                return;
	}

        if (it[in].drdata[0]<ch[cn].level) {
		log_char(cn,LOG_SYSTEM,0,"Thou mayest not use this clan spawner for thy level is too great.");
		return;
	}

        if (!it[in].drdata[2]) {
		int idx=*(unsigned int*)(it[in].drdata+16);

                diff=max(0,((*(int*)(it[in].drdata+8)-realtime))/60);

		if ((idx==0 && it[in].drdata[0]<40) ||
		    (idx==1 && it[in].drdata[0]>=40 && it[in].drdata[0]<64) ||
		    (idx==2 && it[in].drdata[0]>=64)) log_char(cn,LOG_SYSTEM,0,"%02d:%02d till the next spawn. A jewel will spawn here.",diff/60,diff%60);
		else log_char(cn,LOG_SYSTEM,0,"%02d:%02d till the next spawn. No jewel will spawn here.",diff/60,diff%60);
                return;
	}

	co=*(unsigned int*)(it[in].drdata+20);
	t=(ticker-*(unsigned int*)(it[in].drdata+28))/TICKS;

	if (mode==1) {
		if ((map[it[in].x+it[in].y*MAXMAP+1].ch && map[it[in].x+it[in].y*MAXMAP+1].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP-1].ch && map[it[in].x+it[in].y*MAXMAP-1].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP+MAXMAP].ch && map[it[in].x+it[in].y*MAXMAP+MAXMAP].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP-MAXMAP].ch && map[it[in].x+it[in].y*MAXMAP-MAXMAP].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP+1+MAXMAP].ch && map[it[in].x+it[in].y*MAXMAP+1+MAXMAP].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP-1+MAXMAP].ch && map[it[in].x+it[in].y*MAXMAP-1+MAXMAP].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP+1-MAXMAP].ch && map[it[in].x+it[in].y*MAXMAP+1-MAXMAP].ch!=cn) ||
		    (map[it[in].x+it[in].y*MAXMAP-1-MAXMAP].ch && map[it[in].x+it[in].y*MAXMAP-1-MAXMAP].ch!=cn)) {
			log_char(cn,LOG_SYSTEM,0,"Thou mayest not use this clan spawner while others can touch it.");
			return;
		}
	} else if (co!=cn || ch[cn].ID!=*(unsigned int*)(it[in].drdata+24) || t<TAKETIME || t>TAKETIME*2) {
		log_char(cn,LOG_SYSTEM,0,"The spawner is not reserved for you.");
		return;
	}

        in2=create_item("clan_jewel");
        if (!in2) {
		elog("clanspawn item driver: could not create item");
		log_char(cn,LOG_SYSTEM,0,"Oops. Bug in clanspawn driver!");
		return;			
	}
	it[in2].drdata[4]=value;
	sprintf(it[in2].description,"The jewel is worth %d points.",value);

	if (!can_carry(cn,in2,0)) {
		destroy_item(in2);
		return;
	}

	if (!give_char_item(cn,in2)) {
		destroy_item(in2);
		log_char(cn,LOG_SYSTEM,0,"No space in inventory.");
		return;
	}

	cname=get_char_clan_name(cn);
	
	if (cname) sprintf(buf,"0000000000°c15Clan: %s won a Jewel for %s from level %d!",ch[cn].name,cname,it[in].drdata[0]);
	else sprintf(buf,"0000000000°c15Clan: %s has won a Jewel from level %d!",ch[cn].name,it[in].drdata[0]);
        server_chat(10,buf);

	summary("spawns",0);

	it[in].drdata[2]=0;
	it[in].sprite--;
	set_sector(it[in].x,it[in].y);
}

void clanjewel_driver(int in,int cn)
{
	int co;

        if (cn) return;

	// must be timer call now
	if (!*(unsigned int*)(it[in].drdata+0)) {
		*(unsigned int*)(it[in].drdata+0)=realtime;	// remember current time if we havent already done so
	}
	if (realtime>*(unsigned int*)(it[in].drdata+0)+60*60) {	// time's up?
		
                if ((co=it[in].carried)) {
			log_char(co,LOG_SYSTEM,0,"Your %s expired.",it[in].name);
			if (ch[co].flags&CF_PLAYER) dlog(co,in,"dropped because it expired");
		}
		remove_item(in);
		destroy_item(in);

	} else call_item(IDR_CLANJEWEL,in,0,ticker+TICKS*30);	// please call again
}

void clanspawn_exit(int in,int cn)
{
	int oldx,oldy;

	if (!cn) return;
	
	if (ch[cn].resta!=areaID) {
                if (!change_area(cn,ch[cn].resta,ch[cn].restx,ch[cn].resty)) {
			log_char(cn,LOG_SYSTEM,0,"Nothing happens - target area server is down.");
		}
                return;
	}

	oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;
	player_driver_stop(ch[cn].player,0);

	if (!drop_char(cn,ch[cn].restx,ch[cn].resty,0)) {
		log_char(cn,LOG_SYSTEM,0,"Please try again soon. Target is busy");
		drop_char(cn,oldx,oldy,0);
	}
}

static int can_hit(int in,int co,int frx,int fry,int tox,int toy)
{
	int dx,dy,x,y,n,cx,cy,m;

	x=frx*1024+512;
	y=fry*1024+512;

	dx=(tox-frx);
	dy=(toy-fry);

        if (abs(dx)<1 && abs(dy)<1) return 0;

	// line algorithm with a step of 0.5 tiles
	if (abs(dx)>abs(dy)) { dy=dy*256/abs(dx); dx=dx*256/abs(dx); }
	else { dx=dx*256/abs(dy); dy=dy*256/abs(dy); }
	
        for (n=0; n<48; n++) {	

		x+=dx;
		y+=dy;
		
		cx=x/1024;
		cy=y/1024;

		if (cx==tox && cy==toy) return 1;

		m=cx+cy*MAXMAP;

		if ((edemonball_map_block(m)) && map[m].it!=in) {
			if (map[m].ch!=co) return 0;
			else return 1;
		}
	}
	return 1;
}

static char offx[25]={ 0,-8, 8, 0, 0,-8,-8, 8, 8,-16, 16,  0,  0, -8, -8,  8,  8,-16,-16, 16, 16,-16,-16, 16, 16};
static char offy[25]={ 0, 0, 0,-8, 8,-8, 8,-8, 8,  0,  0,-16, 16,-16, 16,-16, 16, -8,  8, -8,  8,-16, 16,-16, 16};

struct targets {
	int co;
	int ox,oy;
	int tx,ty;
};

#define MAXT	50

void clandefense_driver(int in,int cn)
{
	int cnr,n,co,ox=0,oy=0,dir,dist,eta,left,step,tx=0,ty=0,m,dx,dy,cnt=0,idx;
	struct targets target[MAXT];

	if (cn) return;
        call_item(IDR_CLANDEFENSE,in,0,ticker+TICKS+RANDOM(TICKS*2));

	cnr=((it[in].x-2)/49)*12+(it[in].y-2)/20+1;
	if (cnr<1 || cnr>=MAXCLAN) {
		elog("defense for non existant clan %d at %d,%d",cnr,it[in].x,it[in].y);
		return;
	}
	
        if (!get_clanalive(cnr)) {
		if (it[in].sprite!=14160) { it[in].sprite=14160; set_sector(it[in].x,it[in].y); }
		return;
	}
	if (get_clanalive(cnr) && it[in].sprite!=14164) {
		it[in].sprite=14164;
		set_sector(it[in].x,it[in].y);
	}
	
	for (n=0; n<25;  n++) {
		for (co=getfirst_char_sector(it[in].x+offx[n],it[in].y+offy[n]); co; co=ch[co].sec_next) {
			//if (get_char_clan(co)==cnr) continue;
			if (clan_alliance_self(get_char_clan(co),cnr)) continue;
                        if (abs(ch[co].x-it[in].x)>15 || abs(ch[co].y-it[in].y)>15) continue;

			if (abs(ch[co].x-it[in].x)<2 && abs(ch[co].y-it[in].y)<2) {
				hurt(co,CLAN_CANNON_STRENGTH*POWERSCALE,0,6,75,50);
				continue;
			}
			if ((map[ch[co].x+ch[co].y*MAXMAP].flags&(MF_ARENA|MF_CLAN))!=(MF_ARENA|MF_CLAN)) continue;

			if (abs(ch[co].x-it[in].x)>abs(ch[co].y-it[in].y)) {
				if (ch[co].x>it[in].x) ox=1;
				if (ch[co].x<it[in].x) ox=-1;
				oy=0;
			} else {
				if (ch[co].y>it[in].y) oy=1;
				if (ch[co].y<it[in].y) oy=-1;
				ox=0;
			}
	
                        if (ch[co].action!=AC_WALK) { tx=ch[co].x; ty=ch[co].y; }
			else {
	
				dir=ch[co].dir;
				dx2offset(dir,&dx,&dy,NULL);
				dist=map_dist(it[in].x,it[in].y,ch[co].x,ch[co].y);
			
				eta=dist*1.5;
				
				left=ch[co].duration-ch[co].step;
				step=ch[co].duration;
			
				eta-=left;
				if (eta<=0) { tx=ch[co].tox; ty=ch[co].toy; }
				else {
					for (m=1; m<10; m++) {
						eta-=step;
						if (eta<=0) { tx=ch[co].x+dx*m; ty=ch[co].y+dy*m; break; }
					}
			
					// too far away, time-wise to make any prediction. give up.
					if (m==10) { tx=ch[co].x; ty=ch[co].y; }
				}
			}
		
                        if (can_hit(in,co,it[in].x+ox,it[in].y+oy,tx,ty)) {
				target[cnt].co=co;
				target[cnt].ox=ox;
				target[cnt].oy=oy;
				target[cnt].tx=tx;
				target[cnt].ty=ty;
				cnt++;
				if (cnt==MAXT) break;
			}
		}
	}
	if (cnt) {
		idx=RANDOM(cnt);
		create_edemonball(0,it[in].x+target[idx].ox,it[in].y+target[idx].oy,target[idx].tx,target[idx].ty,cnr,0);
	}
}

struct clanvault_data {
	int last_hp;
	int last_shout;
};

void clanvault_driver(int cn,int ret,int lastact)
{
	int cnr,co;
	struct msg *msg,*next;
	struct clanvault_data *dat;
	char buf[256];

	cnr=((ch[cn].x-2)/49)*12+(ch[cn].y-2)/20+1;

	ch[cn].clan=cnr;
	ch[cn].clan_serial=clan_serial(cnr);

	sprintf(ch[cn].description,"Health: %.2f%%",100.0/POWERSCALE/100.0*ch[cn].hp);

	dat=set_data(cn,DRD_CLANVAULTDRIVER,sizeof(struct clanvault_data));
	if (!dat) return;	// oops...

	if (!dat->last_hp) dat->last_hp=ch[cn].hp;

	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch(msg->type) {
			case NT_GOTHIT:		// did we get hit?
				co=msg->dat1;
				if (!co) break;
				if (ch[cn].hp<dat->last_hp && ticker-dat->last_shout>TICKS*60) {
					sprintf(buf,"0000000000°c15The clanvault of %s is under attack by %s of %s. Health is at %.2f%%",
					    get_clan_name(cnr),
					    ch[co].name,
					    get_clan_name(get_char_clan(co)),
					    100.0/POWERSCALE/100.0*ch[cn].hp);
					server_chat(10,buf);
					dat->last_shout=ticker;
				}
                                dat->last_hp=ch[cn].hp;
				break;
			
			/*case NT_GIVE:
				co=msg->dat1;

				if ((in=ch[cn].citem)) {	// we still have it
					if (it[in].driver==1 && clan_alliance_self(ch[cn].clan,get_char_clan(co))) {
						str=min(it[in].drdata[1]*POWERSCALE/10,ch[cn].value[0][V_HP]*POWERSCALE-ch[cn].hp);
						ch[cn].hp+=str;
						emote(cn,"Uses a potion for %.2f%%",100.0/POWERSCALE/100.0*str);
					}
					destroy_item(in);
					ch[cn].citem=0;
				}
				break;*/
		}
		remove_message(cn,msg);
	}

	do_idle(cn,TICKS);
}

void clanvault_dead(int cn,int co)
{
	int in,cnr,v,cno;
	char buf[256];

	cnr=((ch[cn].x-2)/49)*12+(ch[cn].y-2)/20+1;

	if (co && (cno=get_char_clan(co))) {
		
		v=(cnt_jewels(cnr)+5)/10;
                v=min(v,(cnt_jewels(cno)+3)/5);
		v=max(v,10);
		v=min(v,200);

		v=del_jewel(cnr,co,v);

		if (v>0) {
			in=create_item("clan_jewel");
			it[in].drdata[4]=v;
			sprintf(it[in].description,"The jewel is worth %d points.",v);
			if (!give_char_item(co,in)) destroy_item(co);
			sprintf(buf,"0000000000°c15The clanvault of %s is was destroyed by %s of %s. A jewel worth %d points was stolen.",
				get_clan_name(cnr),
				ch[co].name,
				get_clan_name(get_char_clan(co)),
				v);
			server_chat(10,buf);
			summary("raids",0);
		}
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_CLANMASTER:	clanmaster_driver(cn,ret,lastact); return 1;
		case CDR_CLANCLERK:	clanclerk_driver(cn,ret,lastact); return 1;
		case CDR_CLANVAULT:	clanvault_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_CLANSPAWN:	clanspawn_driver(in,cn); return 1;
		case IDR_CLANJEWEL:	clanjewel_driver(in,cn); return 1;
                case IDR_CLANSPAWNEXIT:	clanspawn_exit(in,cn); return 1;
		case IDR_CLANDEFENSE:	clandefense_driver(in,cn); return 1;

		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_CLANMASTER:	clanmaster_dead(cn,co); return 1;
		case CDR_CLANCLERK:	clanmaster_dead(cn,co); return 1;
		case CDR_CLANVAULT:	clanvault_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_CLANMASTER:	return 1;
		case CDR_CLANCLERK: 	return 1;
		case CDR_CLANVAULT:	return 1;

		default:		return 0;
	}
}



















