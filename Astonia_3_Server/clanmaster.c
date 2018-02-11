/*
 *
 * $Id: clanmaster.c,v 1.10 2008/03/24 11:20:42 devel Exp devel $
 *
 * $Log: clanmaster.c,v $
 * Revision 1.10  2008/03/24 11:20:42  devel
 * bugfix in logging
 *
 * Revision 1.9  2007/12/10 10:08:12  devel
 * clan changes
 *
 * Revision 1.8  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.7  2007/02/07 13:45:27  devel
 * fixed bug with clanmaster greeting
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
#include "store.h"
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

	int give_try;

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
	int nr;

	char name[80];
};

void clanmaster_driver(int cn,int ret,int lastact)
{
	struct clanmaster_driver_data *dat;
	struct clan_found_data *fnd;
        int co,in,n,rank,cc,res;
        struct msg *msg,*next;
	char *ptr,tmp[80];

        dat=set_data(cn,DRD_CLANMASTERDRIVER,sizeof(struct clanmaster_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
                clanmaster_driver_parse(cn,dat);
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
						
			if (!get_char_club(co) && !get_char_clan(co)) quiet_say(cn,"Hello %s! Would you like to found a °c4clan°c0?",ch[co].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

                        if ((msg->dat1==1 || msg->dat1==2) && (co=msg->dat3)!=cn) {	// talk, and not our talk				
                                if ((ptr=strcasestr((char*)msg->dat2,"name:")) && (fnd=set_data(co,DRD_CLANFOUND,sizeof(struct clan_found_data)))) {
					if (!(ch[co].flags&CF_PAID)) {
						quiet_say(cn,"I'm sorry, %s, but only paying players may found clans.",ch[co].name);
					} else if (!get_char_clan(co) && !get_char_club(co)) {
						ptr+=5;
						while (isspace(*ptr)) ptr++;
						for (n=0; n<79; n++) {
							if (!*ptr || *ptr=='"') break;
							fnd->name[n]=*ptr++;
						}
						fnd->name[n]=0;
						fnd->state=1;
						quiet_say(cn,"Your clan, %s, will be named '%s'. Try again if that is not what you want. Or hand me a Clan Jewel to proceed. You can buy them at Jeremy's",ch[co].name,fnd->name);
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
                                                        add_member(co,dat->accept_clan,dat->join);
							quiet_say(cn,"%s, you are now a member of %s's clan.",ch[co].name,dat->join);
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
							if (!(ch[cc].flags&CF_PAID) && rank>1) {
                                                                quiet_say(cn,"%s is not a paying player, you cannot set the rank higher than 1.",ch[cc].name);
							} else if (get_char_clan(cc)==get_char_clan(co)) {
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
						res=found_clan(fnd->name,co,&fnd->nr);
						if (!res) {
							quiet_say(cn,"So be it. There will be a new clan, named '%s', and you, %s, shall be its new master. Good luck, young master!",fnd->name,ch[co].name);
							fnd->state=0;

							add_member(co,fnd->nr,ch[co].name);
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
				dat->give_try++;
                                if (dat->give_try<20 && give_driver(cn,co)) return;
				
				// didnt work, let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
			dat->give_try=0;
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

struct clanclerk_driver_data
{
	int clan;	
};

void clanclerk_driver(int cn,int ret,int lastact)
{
	struct clanclerk_driver_data *dat;
	int co,in,res,nr,level,n,tmp1,tmp2;
        struct msg *msg,*next;
	char *ptr,name[40];

        dat=set_data(cn,DRD_CLANCLERKDRIVER,sizeof(struct clanclerk_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
		dat->clan=atoi(ch[cn].arg);
		ch[cn].arg=NULL;
	}

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
				case 2:	say(cn,"Our clan has %d jewels.",cnt_jewels(dat->clan)); break;
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
					clan_money_change(dat->clan,nr,co);
					say(cn,"You have deposited %dG.",nr);
					dlog(co,0,"deposited %dG to clan %d.",nr,dat->clan);
				}
				if ((ptr=strcasestr((char*)msg->dat2,"add potions"))) {
					
                                        res=add_simple_potion(dat->clan,co);
					say(cn,"Added %d potions.",res);
				}
				// members only
				if (get_char_clan(co)!=dat->clan) {
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
					
					if (get_clan_money(dat->clan)<nr) {
						say(cn,"Your clan does not have that much money, %s.",ch[co].name);
						remove_message(cn,msg);
						continue;
					}
					clan_money_change(dat->clan,-nr,co);
					ch[co].gold+=nr*100;
					ch[co].flags|=CF_ITEMS;
					say(cn,"You have withdrawn %dG.",nr);
					dlog(co,0,"withdrew %dG from clan %d.",nr,dat->clan);
				}
				if ((ptr=strcasestr((char*)msg->dat2,"buy"))) {
					say(cn,"Buying has been disabled, you have infinite stock.");

					remove_message(cn,msg);
					continue;

                                        ptr+=3;
					nr=atoi(ptr);
					
					while (isspace(*ptr)) ptr++;
					while (isdigit(*ptr)) ptr++;
					while (isspace(*ptr)) ptr++;
			
					level=atoi(ptr);

					if (nr<0 || nr>21) {
						say(cn,"Type must be between 1 and 21");

						remove_message(cn,msg);
						continue;
					}

					if (level<1 || level>100) {
						say(cn,"Number must be between 1 and 100");

						remove_message(cn,msg);
						continue;
					}
					if ((tmp1=get_clan_dungeon_cost(nr,level))>(tmp2=get_clan_money(dat->clan))) {
						say(cn,"This order amounts to %dg while your treasury only holds %dg.",tmp1,tmp2);

						remove_message(cn,msg);
						continue;
					}

                                        //res=set_clan_dungeon_buy(dat->clan,nr,level,co);	
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}
                                if ((ptr=strcasestr((char*)msg->dat2,"use"))) {

                                        ptr+=3;
					nr=atoi(ptr);
					
					while (isspace(*ptr)) ptr++;
					while (isdigit(*ptr)) ptr++;
					while (isspace(*ptr)) ptr++;
			
					level=atoi(ptr);

					if (nr<0 || nr>21) {
						say(cn,"Type must be between 1 and 21");

						remove_message(cn,msg);
						continue;
					}

					if (level<0 || level>100) {
						say(cn,"Number must be between 0 and 100");

						remove_message(cn,msg);
						continue;
					}

                                        res=set_clan_dungeon_use(dat->clan,nr,level,co);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						if (res==-1) {
							say(cn,"You can only use 0 to 10 guards of each kind, 0 to 25 teleport traps, 0 to 1 fake walls and 0 to 2 locked doors.");
						} else {
							say(cn,"That configuration would cost %d points, but you may only spend 400 points.",res);
						}
					}
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

					if (nr<0 || nr>13) {
						say(cn,"Number must be between 0 and 13");

						remove_message(cn,msg);
						continue;
					}

					if (level<0 || level>20) {
						say(cn,"Level must be between 0 and 20");

						remove_message(cn,msg);
						continue;
					}

                                        res=set_clan_bonus(dat->clan,nr,level,co);
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

					if (nr<1 || nr>31) {
						say(cn,"Number must be between 1 and 31");

						remove_message(cn,msg);
						continue;
					}

					if (level<1 || level>5) {
						say(cn,"Level must be between 1 and 5");

						remove_message(cn,msg);
						continue;
					}

					if (level>CS_NEUTRAL && !get_clan_raid(dat->clan)) {
						say(cn,"Your clan cannot go to war or feud unless you turn °c4raiding on°c0.");

						remove_message(cn,msg);
						continue;
					}

					if (level>CS_NEUTRAL && !get_clan_raid(nr)) {
						say(cn,"Your clan cannot go to war or feud unless your opponent has raiding on.");

						remove_message(cn,msg);
						continue;
					}

                                        res=set_clan_relation(dat->clan,nr,level,co);
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

					res=set_clan_rankname(dat->clan,nr,name,co);

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
			
                                        res=set_clan_website(dat->clan,ptr,co);
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
			
                                        res=set_clan_message(dat->clan,ptr,co);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"raiding on"))) {

                                        res=set_clan_raid(dat->clan,co,1);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"raiding off"))) {

                                        res=set_clan_raid(dat->clan,co,0);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}

				if ((ptr=strcasestr((char*)msg->dat2,"raiding god on")) && (ch[co].flags&CF_GOD)) {

                                        res=set_clan_raid_god(dat->clan,co,1);
                                        if (!res) {
						say(cn,"Done.");
						
                                                remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed.");						
					}
				}
				if ((ptr=strcasestr((char*)msg->dat2,"raiding god off")) && (ch[co].flags&CF_GOD)) {

                                        res=set_clan_raid_god(dat->clan,co,0);
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
                                        say(cn,"You can no longer add jewels directly.");

				}
				if (it[in].driver==IDR_FLASK) {
					res=add_alc_potion(dat->clan,in);
					
					if (!res) {
						say(cn,"Added one potion to our storage.");
						
                                                destroy_item(ch[cn].citem);
						ch[cn].citem=0;
						remove_message(cn,msg);
						continue;
					} else {
						say(cn,"Failed to add potion to storage, please try again.");						
					}
				}

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

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHTDOWN,ret,lastact)) return;

        do_idle(cn,TICKS*2);
}

void clanmaster_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

void clanspawn_driver(int in,int cn)
{
	int diff,freq,cnr;
	char *cname,buf[256];

	freq=it[in].drdata[1];
	if (freq==0) freq=48;

        if (!cn) {	// timer calls
		call_item(IDR_CLANSPAWN,in,0,ticker+TICKS*60);

		// preset: after an average of 1/2 freq after reboot
		if (!*(unsigned int*)(it[in].drdata+4)) {
			*(unsigned int*)(it[in].drdata+4)=((realtime+RANDOM(60*60*freq/2)+60*60*freq/4)/1800)*1800;
			//*(unsigned int*)(it[in].drdata+4)=realtime+30;
			it[in].max_level=it[in].drdata[0];
		}
		
                if (realtime>=*(unsigned int*)(it[in].drdata+4)) {
			if (!it[in].drdata[2]) { it[in].sprite++; set_sector(it[in].x,it[in].y); }
			it[in].drdata[2]++;

			// a new one at every freq hours on average
			*(unsigned int*)(it[in].drdata+4)=((realtime+RANDOM(60*60*freq)+60*60*freq/2)/1800)*1800;
		}
                return;
	}

        if (it[in].drdata[0]<ch[cn].level) {
		log_char(cn,LOG_SYSTEM,0,"Thou mayest not use this clan spawner for thy level is too great.");
		return;
	}

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
	
	if (!it[in].drdata[2]) {
		diff=max(0,((*(int*)(it[in].drdata+4)-realtime))/60);
		log_char(cn,LOG_SYSTEM,0,"%02d:%02d to go, about one jewel every %d hours.",diff/60,diff%60,freq);
                return;
	}

        cname=get_char_clan_name(cn);
	cnr=get_char_clan(cn);
	
	if (cname && cnr) {
		sprintf(buf,"0000000000°c15Clan: %s won a Jewel for %s from level %d!",ch[cn].name,cname,it[in].max_level);
		server_chat(5,buf);

		sprintf(buf,"%02d:X:%02d:%10u:%s",cnr,it[in].max_level,ch[cn].ID,ch[cn].name);
		server_chat(1028,buf);
	} else {
		sprintf(buf,"0000000000°c15Clan: %s has won the spawn at level %d! Not being in a clan it didn't do %s much good, though.",ch[cn].name,it[in].max_level,himname(cn));
		server_chat(5,buf);
	}

	it[in].drdata[2]--;
	if (!it[in].drdata[2]) { it[in].sprite--; set_sector(it[in].x,it[in].y); }
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

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_CLANMASTER:	clanmaster_driver(cn,ret,lastact); return 1;
		case CDR_CLANCLERK:	clanclerk_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_CLANSPAWN:	clanspawn_driver(in,cn); return 1;
		case IDR_CLANJEWEL:	clanjewel_driver(in,cn); return 1;
		case IDR_CLANVAULT:	return 1;
		case IDR_CLANSPAWNEXIT:	clanspawn_exit(in,cn); return 1;

		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_CLANMASTER:	clanmaster_dead(cn,co); return 1;
		case CDR_CLANCLERK:	clanmaster_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_CLANMASTER:	return 1;
		case CDR_CLANCLERK: 	return 1;

		default:		return 0;
	}
}




















