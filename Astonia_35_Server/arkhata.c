/*
 * $Id: arkhata.c,v 1.3 2008/04/14 14:37:52 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: arkhata.c,v $
 * Revision 1.3  2008/04/14 14:37:52  devel
 * you -> thou
 * added some navigational aids for players
 *
 * Revision 1.2  2008/03/29 15:43:59  devel
 * conversion to v3.5
 *
 * Revision 1.1  2008/03/28 12:41:47  devel
 * Initial revision
 *
 * Revision 1.3  2008/03/25 09:53:15  devel
 * small fixes
 *
 * Revision 1.2  2008/03/24 11:18:57  devel
 * arkhata
 *
 * Revision 1.1  2007/12/10 10:07:50  devel
 * Initial revision
 *
 * Revision 1.9  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.8  2007/06/22 12:48:30  devel
 * added caligar quest to kelly
 *
 * Revision 1.7  2007/05/26 13:19:56  devel
 * added caligar hook to kelly
 *
 * Revision 1.6  2006/12/08 10:33:55  devel
 * fixed bug in carlos driver
 *
 * Revision 1.5  2006/09/23 10:01:21  devel
 * made kelly give military exp for creeper quest onl once
 *
 * Revision 1.4  2006/09/21 11:22:52  devel
 * added questlog
 *
 * Revision 1.3  2006/09/14 09:55:22  devel
 * added questlog
 *
 * Revision 1.2  2005/12/04 17:34:24  ssim
 * increased exp for carlos' quest
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
#include "area3.h"
#include "los.h"
#include "military.h"
#include "consistency.h"
#include "staffer_ppd.h"
#include "misc_ppd.h"
#include "skill.h"
#include "database.h"
#include "questlog.h"
#include "act.h"
#include "spell.h"
#include "poison.h"
#include "sector.h"
#include "arkhata.h"

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
	{{"enter",NULL},NULL,5},
	{{"aye",NULL},NULL,6},
	{{"watch",NULL},NULL,7},
	{{"iamdone",NULL},NULL,8},
	{{"onceagain",NULL},NULL,9},

	{{"raise","wisdom",NULL},NULL,V_WIS+100},
	{{"raise","intuition",NULL},NULL,V_INT+100},
	{{"raise","agility",NULL},NULL,V_AGI+100},
        {{"raise","strength",NULL},NULL,V_STR+100},
	{{"raise","dagger",NULL},NULL,V_DAGGER+100},
	{{"raise","hand",NULL},NULL,V_HAND+100},
	{{"raise","hand","to","hand",NULL},NULL,V_HAND+100},
	{{"raise","sword",NULL},NULL,V_SWORD+100},
	{{"raise","staff",NULL},NULL,V_STAFF+100},
	{{"raise","twohanded",NULL},NULL,V_TWOHAND+100},
	{{"raise","two","handed",NULL},NULL,V_TWOHAND+100},
	{{"raise","two-handed",NULL},NULL,V_TWOHAND+100},
	{{"raise","attack",NULL},NULL,V_ATTACK+100},
	{{"raise","parry",NULL},NULL,V_PARRY+100},
	{{"raise","warcry",NULL},NULL,V_WARCRY+100},
	{{"raise","tactics",NULL},NULL,V_TACTICS+100},
	{{"raise","surround",NULL},NULL,V_SURROUND+100},
	{{"raise","surround","hit",NULL},NULL,V_SURROUND+100},
	{{"raise","speed",NULL},NULL,V_SPEEDSKILL+100},
	{{"raise","barter",NULL},NULL,V_BARTER+100},
	{{"raise","bartering",NULL},NULL,V_BARTER+100},
	{{"raise","perception",NULL},NULL,V_PERCEPT+100},
	{{"raise","stealth",NULL},NULL,V_STEALTH+100},
	{{"raise","bless",NULL},NULL,V_BLESS+100},
	{{"raise","heal",NULL},NULL,V_HEAL+100},
	{{"raise","freeze",NULL},NULL,V_FREEZE+100},
	{{"raise","magic",NULL},NULL,V_MAGICSHIELD+100},
	{{"raise","magic","shield",NULL},NULL,V_MAGICSHIELD+100},
	{{"raise","lightning",NULL},NULL,V_FLASH+100},
	{{"raise","fire",NULL},NULL,V_FIRE+100},
	{{"raise","regenerate",NULL},NULL,V_REGENERATE+100},
	{{"raise","meditate",NULL},NULL,V_MEDITATE+100},
	{{"raise","immunity",NULL},NULL,V_IMMUNITY+100},
	{{"raise","duration",NULL},NULL,V_DURATION+100},
	{{"raise","rage",NULL},NULL,V_RAGE+100}
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
				if (qa[q].answer) say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}

struct std_npc_driver_data
{
        int last_talk;
	int current_victim;
	int misc;
};

void rammy_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
	struct staffer_ppd *sppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
			

                        if (ppd) {
				switch(ppd->rammy_state) {
					case 0:		sppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
							if (sppd && sppd->guardbran_state>=2) ;
							else break;
							say(cn,"Hold! Stranger, where art thou from?");
							ppd->rammy_state++; didsay=1;
							break;
					case 1:		say(cn,"Oh... I see that thou art a messenger from the Count Brannington, that is a most pleasent surprise to learn that he still holds the city.");
							ppd->rammy_state++; didsay=1;
							break;
					case 2:		say(cn,"We haven't heard from the outside world in ages, are the demons still roaming wild out there?");
							ppd->rammy_state++; didsay=1;
							break;
					case 3:		say(cn,"So the Imperial Army has managed to keep them at bay for now? That is good to know.");
							ppd->rammy_state++; didsay=1;
							break;
					case 4:		say(cn,"Perhaps it is time for Arkhata to re-establish contact with the outside world. Please report back to the guard that we wish to open our passages to the city of Brannington.");
							ppd->rammy_state++; didsay=1;
							break;
					case 5:		say(cn,"And come back here afterwards, I'm in need of thy help with another problem.");
							ppd->rammy_state++; didsay=1;
							break;
					case 6:		sppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
							if (sppd && sppd->guardbran_state>=7) ppd->rammy_state++;
							else break;
					case 7:		say(cn,"Welcome back, friend!");
							questlog_open(co,65);
							ppd->rammy_state++; didsay=1;
							break;
					case 8:		say(cn,"A gang of bandits robbed me of my most precious item, my crown! They traveled south, past the fortress.");
							ppd->rammy_state++; didsay=1;
							break;
					case 9:		say(cn,"Now, dear Adventurer, I ask thee to go there and return my crown. I would reward thee!");
							ppd->rammy_state++; didsay=1;
							break;
					case 10:	break;	// waiting for crown
					
					case 11:	say(cn,"May Ishtar be with thee, I am forever grateful.");
							ppd->rammy_state++; didsay=1;
							break;
					case 12:	say(cn,"My friend Jaz in the town has asked for assistance, but I have no guards to spare. Wouldst thou be kind enough to visit him?");
							ppd->rammy_state++; didsay=1;
							break;
					case 13:	if (ch[co].level>=54 && ppd->monk_state>=20) {
								questlog_open(co,71);
								ppd->rammy_state++;
							} else break;
					case 14:	say(cn,"Hello again, %s! We have a problem with the guards in the fortress. They wont let people just pass through of course, but they are attacking everyone!",ch[co].name);
							ppd->rammy_state++; didsay=1;
							break;
					case 15:	say(cn,"It is not their fault as they have been trained for that. They know only citizens of Arkhata, and enemies.");
							ppd->rammy_state++; didsay=1;
							break;
					case 16:	say(cn,"Thou must go see the captain in the fortress, hand him this letter. It will tell him who thou art, and to find a solution to this issue");
                                                        if (!has_item(co,IID_ARKHATA_FORTRESSKEY)) {
								in=create_item("key14_13_main");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
							if (!has_item(co,IID_ARKHATA_LETTER1)) {
								in=create_item("letter1");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
							ppd->rammy_state++; didsay=1;
							break;
					case 17:	if (ppd->letter_bits==(2|4|8)) ppd->rammy_state++;
							else break;
					case 18:	say(cn,"Yet again thou hast been of great aid. The trade route is open and safe thanks to thee.");
							questlog_done(co,71);
							ppd->rammy_state++; didsay=1;
							break;
					case 19:	if (ch[co].level>=60) {
								say(cn,"There is one last favor I must ask of thee, please go aid Ramin yet again.. It is most urgent!");
								ppd->rammy_state++; didsay=1;
							} else break;
					case 20:	break;
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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->rammy_state<=6) { dat->last_talk=0; ppd->rammy_state=0; }
						if (ppd && ppd->rammy_state>=7 && ppd->rammy_state<=10) { dat->last_talk=0; ppd->rammy_state=7; }
						if (ppd && ppd->rammy_state>=12 && ppd->rammy_state<=13) { dat->last_talk=0; ppd->rammy_state=12; }
						if (ppd && ppd->rammy_state>=14 && ppd->rammy_state<=17) { dat->last_talk=0; ppd->rammy_state=14; }
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
				
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                                if (it[in].ID==IID_ARKHATA_CROWN && ppd->rammy_state==10) {
                                        questlog_done(co,65);
					destroy_item_byID(co,IID_ARKHATA_CROWN);
					ppd->rammy_state=11;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void jaz_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->jaz_state) {
					case 0:		if (ppd->rammy_state>=12) ppd->jaz_state++;
                                                        else break;
					case 1:		say(cn,"Welcome to my home, fellow adventurer. Couldst thou please spare a moment?");
							questlog_open(co,66);
							ppd->jaz_state++; didsay=1;
							break;
					case 2:		say(cn,"While I was traveling, I found a bug house. It was full of strong Knogers. When I reached the main room, the Knogers attacked me.");
							ppd->jaz_state++; didsay=1;
							break;
					case 3:		say(cn,"I had to run for my life, but accidently, my bracelet fell off my hand. It holds the insignia of Ishtar and has been passed down through generations in my family.");
							ppd->jaz_state++; didsay=1;
							break;
					case 4:		say(cn,"Now, dear fellow, I'll ask thee to cross bridge to the north-east and go into the hut. Defeat the Knoger who has my bracelet and return it to me.");
							ppd->jaz_state++; didsay=1;
							break;
					case 5:		break; // waiting for bracelet
					case 6:		say(cn,"Thank thee so much, I will call thee my %s from now on.",(ch[co].flags&CF_MALE) ? "brother" : "sister");
							ppd->jaz_state++; didsay=1;
							break;
					case 7:		if (ch[co].level<50) break;
							say(cn,"I hear that Queen Fiona in the fighting academy is looking for capable challengers to her students, she speaks of great rewards. Thou shouldst go there and test thy skills.");
							ppd->jaz_state++; didsay=1;
							break;
					case 8:		say(cn,"Cross the river, follow the path to the south-west, cross the river again using the long and winding bridge to reach the academy.");
							ppd->jaz_state++; didsay=1;
							break;
					case 9: 	break; // all done
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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->jaz_state<=5) { dat->last_talk=0; ppd->jaz_state=0; }
						if (ppd && ppd->jaz_state>=6 && ppd->jaz_state<=9) { dat->last_talk=0; ppd->jaz_state=6; }
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
				
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                                if (it[in].ID==IID_ARKHATA_BRACELET && ppd->jaz_state==5) {
                                        questlog_done(co,66);
					destroy_item_byID(co,IID_ARKHATA_BRACELET);
					ppd->jaz_state=6;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void fight_student(int cc,int cn,int nr)
{
	int x,y,co;
	char buf[80];

	for (x=9; x<=24; x++) {
		for (y=238; y<=252; y++) {
			if ((co=map[x+y*MAXMAP].ch)) {
				say(cc,"The arena is busy. Please try again later.");
				return;
			}
		}
	}
	
	sprintf(buf,"Gladiator_%d",nr);
	co=create_char(buf,0);
	if (!co) {
		say(cc,"Oops. Bug #5317a");
		return;
	}
	update_char(co);
        ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
	ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
	ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
	ch[co].lifeshield=ch[co].value[0][V_MAGICSHIELD]*POWERSCALE;
        ch[co].dir=DX_RIGHTDOWN;
	ch[co].tmpx=14;
	ch[co].tmpy=244;
	ch[co].driver=135;
	ch[co].flags&=~(CF_RESPAWN|CF_NOATTACK|CF_IMMORTAL);

	if (!drop_char(co,14,244,0)) {
		destroy_char(co);
		say(cc,"Oops. Bug #5317b");
		return;
	}

	teleport_char_driver(cn,16,244);
}

int fiona_raise(int cn,int co,int v)
{
	if (ch[co].gold<10000*100) {
		quiet_say(cn,"Sorry, it seems thou canst not pay me.");
		return 0;
	}
        if (raise_value_exp(co,v)) {
		raise_value_exp(co,v);
		log_char(co,LOG_SYSTEM,0,"You gained %s.",skill[v].name);
		ch[co].gold-=10000*100;
		ch[co].flags|=CF_ITEMS;
		return 1;
	}
	say(cn,"You cannot raise the skill %s. Please choose a different one, %s.",skill[v].name,ch[co].name);
	return 0;
}

void fiona_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0,cc;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_NPC && msg->dat1==NTID_GLADIATOR) {
                        cc=msg->dat3;
			if (cc && (ch[cc].flags&CF_PLAYER)) {
				ppd=set_data(cc,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
				if (ppd && ppd->fiona_state>=7 && ppd->fiona_state<=16) {
					ppd->fiona_state++;
					say(cn,"Well done, %s.",ch[cc].name);
					teleport_char_driver(cc,15,235);
				}
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

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->fiona_state) {
					case 0:		if (ch[co].level>=50) ppd->fiona_state++;
							else break;
					case 1:		say(cn,"Hello there stranger, and welcome to my Academy of the Fighting Arts. I can train thy abillities here if thou provest thine worth first.");
							questlog_open(co,67);
							ppd->fiona_state++; didsay=1;
							break;
					case 2:		say(cn,"I ventured into the vampire lair a few days ago, and suddenly my ring disappeared. And my students need me here so I can't retrieve it myself. Please bring it back to me. The lair is on the cemetery east of the Knoger residence.");
							ppd->fiona_state++; didsay=1;
							break;
					case 3:		break;	// waiting for ring
					case 4:		say(cn,"Thank thee ever so much.");
							ppd->fiona_state++; didsay=1;
							break;
					case 5:		if (ch[co].level>80) {
								say(cn,"I was going to offer thee a chance to prove thyself against my students, but they are no challenge for thee.");
								ppd->fiona_state=19;
							} else {
								say(cn,"I will now allow thee to test thy skills against my students. If thou defeatst them all I will raise one of thy skills by 2 points for the price of 10000 gold.");
								ppd->fiona_state++;
							}
							didsay=1;
							break;
					case 6:		say(cn,"To fight my student say °c4enter°c0! (Offer valid only for level 80 and below). Or thou might go back to town and give Ramin a hand.");
							ppd->fiona_state++;
							didsay=1;
							break;
					case 7:		break;	// fighting gladiator 1
					case 8:		break;	// fighting gladiator 2
					case 9:		break;	// fighting gladiator 3
					case 10:	break;	// fighting gladiator 4
					case 11:	break;	// fighting gladiator 5
					case 12:	break;	// fighting gladiator 6
					case 13:	break;	// fighting gladiator 7
					case 14:	break;	// fighting gladiator 8
					case 15:	break;	// fighting gladiator 9
					case 16:	break;	// fighting gladiator 10
					case 17:	if (ch[co].level<=80) {
								say(cn,"Well done, %s. So what skill does thou want raised? Please say 'raise skill name' (like, for example, 'raise attack'). This will not increase thy skills past the usual maxes.",ch[co].name);
								ppd->fiona_state=18;
							} else {
								say(cn,"Nicely done, %s. But then, it wasn't a big challenge, now, was it?",ch[co].name);
								ppd->fiona_state=19;
							}
							didsay=1;
							break;
					case 18:	break;	// waiting for skill choice;
					case 19:	say(cn,"Fare thee well, %s.",ch[co].name);
							ppd->fiona_state++; didsay=1;
							break;
					case 20:	break; // all done
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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->fiona_state<=3) { dat->last_talk=0; ppd->fiona_state=0; }
						if (ppd && ppd->fiona_state>=5 && ppd->fiona_state<=7) { dat->last_talk=0; ppd->fiona_state=5; }
						if (ppd && ppd->fiona_state>=17 && ppd->fiona_state<=18) { dat->last_talk=0; ppd->fiona_state=17; }
						if (ppd && ppd->fiona_state>=19 && ppd->fiona_state<=20) { dat->last_talk=0; ppd->fiona_state=19; }
						break;
				case 5:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->fiona_state>=7 && ppd->fiona_state<=16) fight_student(cn,co,ppd->fiona_state-6);
						break;
			}
			if (didsay>100 && didsay<200) {
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
				if (ppd && ppd->fiona_state==18) {
					if (fiona_raise(cn,co,didsay-100)) ppd->fiona_state=19;
				}
				else say(cn,"That offer is not open at the moment.");
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
				
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_RING && ppd->fiona_state==3) {
                                        questlog_done(co,67);
					destroy_item_byID(co,IID_ARKHATA_RING);
					ppd->fiona_state=4;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	if (spell_self_driver(cn)) return;
	
	do_idle(cn,TICKS);
}

void gladiator_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct msg *msg,*next;
	int co,cc;

	dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
	if (!dat) return;	// oops...

	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
	
		switch(msg->type) {
			case NT_CREATE:
				dat->last_talk=ticker;
				break;
	
			case NT_CHAR:
                                break;
	
			case NT_TEXT:
				co=msg->dat3;
				tabunga(cn,co,(char*)msg->dat2);
				break;
	
			case NT_DIDHIT:
				break;
	
			case NT_NPC:
				break;

			case NT_DEAD:
				co=msg->dat1;
				cc=msg->dat2;
				if (cc==cn && (ch[co].flags&CF_PLAYER)) {
					remove_destroy_char(cn);
					return;
				}
				break;

		}
	
	
		standard_message_driver(cn,msg,1,1);
		remove_message(cn,msg);
	}
	
	// do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ticker-dat->last_talk>TICKS*60*3) {
		int x,y;

		say(cn,"That's all folks!");
		for (x=9; x<=24; x++) {
			for (y=238; y<=252; y++) {
				if ((co=map[x+y*MAXMAP].ch) && (ch[co].flags&CF_PLAYER)) {
					teleport_char_driver(co,15,235);
				}
			}
		}
		remove_destroy_char(cn);
		return;
	}
	
	fight_driver_update(cn);
	
	if (fight_driver_attack_visible(cn,0)) return;
	
	if (fight_driver_follow_invisible(cn)) return;
	
	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	
	if (regenerate_driver(cn)) return;
	
	if (spell_self_driver(cn)) return;
	
	//say(cn,"i am %d",cn);
	do_idle(cn,TICKS);
}

void gladiator_dead(int cn,int co)
{
	if (!co || !(ch[co].flags&CF_PLAYER)) return;
	notify_area(15,232,NT_NPC,NTID_GLADIATOR,cn,co);
}

void bridgeguard_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct msg *msg,*next;
	int co,dist;

	dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
	if (!dat) return;	// oops...
	
	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
	
		switch(msg->type) {
			case NT_CREATE:
				break;
	
			case NT_CHAR:
				co=msg->dat1;
				dist=map_dist(ch[cn].tmpx,ch[cn].tmpy,ch[co].x,ch[co].y);
				if (dist>=16) break;
				
				if (!(ch[co].flags&CF_PLAYER)) {
					if (ch[cn].group!=ch[co].group) fight_driver_add_enemy(cn,co,0,1);
					break;
				}
				if (ch[cn].value[0][V_BLESS]) break; // only one of them should talk
				if (!char_see_char(cn,co)) break;
				
				if (dist<16) {
					if (!mem_check_driver(cn,co,7) && ticker-dat->last_talk>TICKS*20) {
						if (ch[co].level<50) say(cn,"Hold! This is no place for such inexperienced travellers as thyself. Return here when thou art stronger, %s!",ch[co].name);
						else say(cn,"Greetings %s, thou mayest pass the bridge.",ch[co].name);
						dat->last_talk=ticker;
						mem_add_driver(cn,co,7);
					}
				}
                                break;
	
			case NT_TEXT:
				co=msg->dat3;
				tabunga(cn,co,(char*)msg->dat2);
				break;
	
			case NT_DIDHIT:
				break;
	
			case NT_NPC:
				if ((co=msg->dat2)!=cn && ch[co].group==ch[cn].group)
					fight_driver_add_enemy(cn,msg->dat3,1,0);
				break;
		}
	
	
		standard_message_driver(cn,msg,0,1);
		remove_message(cn,msg);
	}
	
	// do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
	
	fight_driver_update(cn);
	
	if (fight_driver_attack_visible(cn,0)) return;
	
	if (fight_driver_follow_invisible(cn)) return;
	
	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	
	if (regenerate_driver(cn)) return;
	
	if (spell_self_driver(cn)) return;

	if (ticker>dat->misc) {
		mem_erase_driver(cn,7);
		dat->misc=ticker+TICKS*60*60;
	}
	
	//say(cn,"i am %d",cn);
	do_idle(cn,TICKS);
}

void nop_driver_parse(int cn,struct std_npc_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"dir")) dat->current_victim=atoi(value);
		else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void nop_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct msg *msg,*next;
	int co;

	dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
		nop_driver_parse(cn,dat);
		ch[cn].arg=NULL;
	}

	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co);
		}

                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->current_victim,ret,lastact)) return;
        if (spell_self_driver(cn)) return;

	do_idle(cn,TICKS*2);
}

void ramin_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->ramin_state) {
					case 0:		if (ppd->fiona_state>=4) ppd->ramin_state++;
							else break;
					case 1:		say(cn,"Hello Great Adventurer! Tidings of thy deed of returning Queen Fiona's ring have reached me. I believe thou couldst help me too.");
							questlog_open(co,68);
							ppd->ramin_state++; didsay=1;
							break;
					case 2:		say(cn,"As a direct ancestor of High Counsellor Regnior who brough us here, I am in charge of the civil life of the city.");
							ppd->ramin_state++; didsay=1;
							break;
					case 3:		say(cn,"One of the traders has reported that he found a strange hole in his bedroom corner a few days ago.");
							ppd->ramin_state++; didsay=1;
							break;
					case 4:		say(cn,"It seems it appeared out of nowhere and he is now afraid to even enter his own bedroom.");
							ppd->ramin_state++; didsay=1;
							break;
					case 5:		say(cn,"I would have sent a soldier for this but Rammy says he doesn't have any to spare from the fortress. Mayest thou go instead, explore this hole and destroy any dangers within it?");
							ppd->ramin_state++; didsay=1;
							break;
					case 6:		break; // waiting for player to kill all skellies
					case 7:		say(cn,"The trader and I thank thee for thy help. Now he may sleep at night again.");
							ppd->ramin_state++; didsay=1;
							break;
					case 8:		say(cn,"Also, I hear there is something going on in the library, wouldst thou please visit the monks there?");
							ppd->ramin_state++; didsay=1;
							break;
					case 9:		if (ch[co].level>=54 && ppd->monk_state>=20) ppd->ramin_state++;
							else break;
					case 10:	if (ppd->rammy_state<14) say(cn,"Greetings my friend! I hear that thou have helped the monks. Now Rammy has sent me news about trouble opening the fortress for a trade route, he is in need of thy help again. Please go and talk to him.");
							ppd->ramin_state++; didsay=1;
							break;
					case 11:	if (ch[co].level>=60 && ppd->rammy_state>=18) ppd->ramin_state++;
							else break;
					case 12:	say(cn,"Ah, it is good to see thee again, %s!",ch[co].name);
							ppd->ramin_state++; didsay=1;
							break;
					case 13:	say(cn,"I know from the library books, diaries of the early years here, that when we first arrived it was a peaceful and uninhabited place.");
							ppd->ramin_state++; didsay=1;
							break;
					case 14:	say(cn,"Now we have monsters everywhere, it is odd. Seems almost as if a source of some kind generates evil.");
							ppd->ramin_state++; didsay=1;
							break;
					case 15:	say(cn,"Go speak to Jada in the house next door. She is my most trusted officer in matters of the mystical.");
							ppd->ramin_state++; didsay=1;
							break;
					case 16:	break;

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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->ramin_state<=6) { dat->last_talk=0; ppd->ramin_state=0; }
						if (ppd && ppd->ramin_state>=7 && ppd->ramin_state<=9) { dat->last_talk=0; ppd->ramin_state=7; }
						if (ppd && ppd->ramin_state>=10 && ppd->ramin_state<=11) { dat->last_talk=0; ppd->ramin_state=10; }
						if (ppd && ppd->ramin_state>=12 && ppd->ramin_state<=16) { dat->last_talk=0; ppd->ramin_state=12; }
						break;

				case 8:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && (ch[co].flags&CF_GOD)) { questlog_done(co,68); ppd->ramin_state=7; }
						break;
				case 9:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && (ch[co].flags&CF_GOD)) { questlog_open(co,68); ppd->ramin_state=6; }
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
			
			ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

			if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_ARKHATA_LETTER2 && ppd && !(ppd->letter_bits&2)) {
					quiet_say(cn,"Thou bring comfort and solution. My friend I am most grateful.");
                                        destroy_item_byID(co,IID_ARKHATA_LETTER2);

                                        ppd->letter_bits|=2;
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
					say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

int skelly_cnt=0;
unsigned short skelly_ID[80];
unsigned short skelly_cn[80];
void arkhataskelly_driver(int cn,int ret,int lastact)
{
	int n;
	unsigned short hash;

        if (lastact==AC_IDLE) {
		hash=ch[cn].tmpx+ch[cn].tmpy*256;

		for (n=0; n<skelly_cnt; n++) {
			if (skelly_ID[n]==hash) break;
		}
		if (n==skelly_cnt) {
			if (skelly_cnt>=80) {
				elog("too many arkhataskellies!");
			} else {
				skelly_ID[n]=hash;
				skelly_cnt++;
			}
		}
		skelly_cn[n]=cn;
	}
	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void arkhataskelly_dead(int cn,int co)
{
	struct arkhata_ppd *ppd;
	int n,cc,undead=0;

	if (!co || !(ch[co].flags&CF_PLAYER)) { ch[cn].respawn=TICKS*15; return; }

	ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
	if (!ppd) { ch[cn].respawn=TICKS*15; return; }

	if (ppd->ramin_state!=6) { ch[cn].respawn=TICKS*15; return; }

	for (n=0; n<80; n++) {
		if ((cc=skelly_cn[n]) && cc!=cn && ch[cc].flags && ch[cc].driver==CDR_ARKHATASKELLY) undead++;
	}
	if (undead) {
		if ((undead%5)==0 || undead<10) log_char(co,LOG_SYSTEM,0,"%d down, %d to go. Beware of respawns!",58-undead,undead);
	} else {
		questlog_done(co,68);
		ppd->ramin_state=7;
		log_char(co,LOG_SYSTEM,0,"Well done, you've solved Ramin's quest. Now report back to him.");
	}
}

void arkhatamonk_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0,nr=0;
	struct msg *msg,*next;

	if (!strcmp(ch[cn].name,"Gregor")) nr=1;
	if (!strcmp(ch[cn].name,"Johan")) nr=2;
	if (!strcmp(ch[cn].name,"Johnatan")) nr=3;
	if (!strcmp(ch[cn].name,"Tracy")) nr=4;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->monk_state) {
					case 0:		if (ppd->ramin_state>=7) ppd->monk_state++;
							else break;
					case 1:		if (nr==1) {
								say(cn,"Dried leaves of lavender, and rose buttons, cinnamon powder and three drops eucalyptus extract is all thou needst for a fragrance so refreshing it will open thy air channels even when cought by the worst cold.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 2:		if (nr==1) {
								say(cn,"Sleep on a pillow filled with acorns of wheat and drink a glass of milk before going to bed for a good nights sleep with calm dreams.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 3:		if (nr==1) {
								say(cn,"Twice a week have a bath in water with one teaspoon of coconut oil and scrub thy skin with a saltwater spunge untill it turns red to maintain a soft and youthful skin.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 4:		if (nr==1) {
								say(cn,"Avoid garlic before proposing to thy loved one...");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 5:		if (nr==1) {
								emote(cn,"pauses");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 6:		if (nr==1) {
								say(cn,"Now that I should have read before.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 7:		if (nr==2) {
								say(cn,"Greetings stranger, Ramin must have sent thee. Please go speak with my brother Johnatan there in the corner. I am busy writing up the family tree of a harpy.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 8:		if (nr==3) {
								say(cn,"Greetings! Thou have come just in time. We usually work here so late at night that we fall asleep by our books right here in the library.");
								questlog_open(co,69);
								ppd->monk_state++; didsay=1;
							}
							break;
					case 9:		if (nr==3) {
								say(cn,"Last night someone must have entered the library while we were sleeping. We woke up and the three parts of the key to that closed shelf with books were gone.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 10:	if (nr==3) {
								say(cn,"We have always kept one part each. The books in there are old and hold many secrets, some we have not yet been able to understand.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 11:	if (nr==3) {
								say(cn,"I believe it must have been some of those monsters who occupy the old building across the bridge and then south-west of here. Follow the marble path through the garden. Please find the keyparts and return them, and we will share with thee some of the wisdom that is stored in those books.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 12:	break; // waiting for keyparts
					case 13:	if (nr==3) {
								say(cn,"Thou hast been of great aid to us, let me share with thee some of our wisdom from these books.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 14:	if (nr==3) {
								say(cn,"I will now continue my research to uncover the secrets held within these books. Why don't thou go and talk to Tracy in the meantime?");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 15:	if (nr==4) {
								say(cn,"I see thou have assisted the monks, perhaps thou can help me too?");
								questlog_open(co,70);
								ppd->monk_state++; didsay=1;
							}
							break;
					case 16:	if (nr==4) {
								say(cn,"There is a hidden backroom down in the southern corner of the library, it seems like a huge book eater has occupied the basement floor there, and is chewing up my most precious novels.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 17:	if (nr==4) {
								say(cn,"The  teleport system is enough to contain the small book eaters, but this huge one must be smarter as well.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 18:	if (nr==4) {
								say(cn,"Please go down there and slay it for me, those novels are of great value to me and I can't stand the thought of losing another page! I will reward thee handsomely");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 19:	break; // waiting for player to kill the bad book eater
					case 20:	if (nr==4) {
								say(cn,"Thank thee, my novels should be safe now. Here is thy reward.");
								give_money(co,200*100,"solved Tracy's quest");
								say(cn,"Ramin is a busy man, he needs thine aid once more. Go speak to him please.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 21:	if (nr==3) {
								say(cn,"It is nice to see thee again my friend. This book I'm attempting to translate is proving to be quite a challenge.");
								questlog_open(co,78);
								ppd->monk_state++; didsay=1;
							}
							break;
					case 22:	if (nr==3) {
								say(cn,"It was found down in the caves, and I believe it's written in the Frawd's language. It is cryptic and ancient.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 23:	if (nr==3) {
								say(cn,"I believe they are related to the dwarfs, but the languages are less similar then deers and fire snails.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 24:	if (nr==3) {
								say(cn,"I need someone related to the Frawds who can help translate this language.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 25:	if (nr==3) {
								say(cn,"Dwarves and their descendants are the only ones who can master the blacksmith craft properly. And they also have a great love for gold.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 26:	if (nr==3) {
								say(cn,"If thou have seen anyone above ground who forges metal items or weapons for a rather ridiculus salary, he is most likely of dwarf blood.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 27:	if (nr==3) {
								say(cn,"If thou know such a person seek him out for me please.");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 28:	break; // waiting for corby
					case 29:	if (nr==3) {
								say(cn,"I can not cover all thy expenses I'm afraid, but here is 3000 gold Let me at least repay some of my debt to thee.");
								give_money(co,3000*100,"Monk Dictionary Quest");
								ppd->monk_state++; didsay=1;
							}
							break;
					case 30:	break; // all done


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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                                if (ppd && ppd->monk_state>=8 && ppd->monk_state<=12) { dat->last_talk=0; ppd->monk_state=8; }
						if (ppd && ppd->monk_state>=15 && ppd->monk_state<=19) { dat->last_talk=0; ppd->monk_state=15; }
						if (ppd && ppd->monk_state>=21 && ppd->monk_state<=28) { dat->last_talk=0; ppd->monk_state=21; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && nr==1 && it[in].ID==IID_ARKHATA_MONKPART1 && ppd->monk_state==12) {
                                        destroy_item_byID(co,IID_ARKHATA_MONKPART1);
					ppd->monk_bits|=1;
					say(cn,"Vanilla and... My key-part! I thank thee, %s.",ch[co].name);
					if (ppd->monk_bits==7) { questlog_done(co,69); ppd->monk_state=13; }

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (ppd && nr==2 && it[in].ID==IID_ARKHATA_MONKPART3 && ppd->monk_state==12) {
                                        destroy_item_byID(co,IID_ARKHATA_MONKPART3);
					ppd->monk_bits|=2;
					say(cn,"I shall remember thy herosim. Perhaps I shall write down thy family tree next, %s?",ch[co].name);
					if (ppd->monk_bits==7) { questlog_done(co,69); ppd->monk_state=13; }

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (ppd && nr==3 && it[in].ID==IID_ARKHATA_MONKPART2 && ppd->monk_state==12) {
                                        destroy_item_byID(co,IID_ARKHATA_MONKPART2);
					ppd->monk_bits|=4;
					say(cn,"My key-part! I thank thee, %s.",ch[co].name);
					if (ppd->monk_bits==7) { questlog_done(co,69); ppd->monk_state=13; }

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (ppd && nr==3 && it[in].ID==IID_ARKHATA_DICTIONARY && ppd->monk_state==28) {
                                        destroy_item_byID(co,IID_ARKHATA_DICTIONARY);
                                        say(cn,"This is much more then I had hoped for, I'm now able to learn and translate the language in it's whole. Let us study together and share this knowledge.");
					give_exp(co,15000);
                                        log_char(co,LOG_SYSTEM,0,"You learn the ancient language and gain some experience.");
					ppd->monk_state++;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (nr==1 || nr==3) if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
		if (nr==2) if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;
		if (nr==4) if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}
	
	do_idle(cn,TICKS);
}

void bookeater_driver(int cn,int ret,int lastact)
{
	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void captain_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->captain_state) {
					case 0:		break;
					case 1:		say(cn,"I see, so Rammy sent thee. Well if he trusts in thee then so shall I!");
							ppd->captain_state++; didsay=1;
							break;
					case 2:		say(cn,"I believe we will need an entrance pass system to allow travellers to pass through the fortress safely. I can inform the guards of this.");
							ppd->captain_state++; didsay=1;
							break;
					case 3:		say(cn,"I need thee to speak with the judge so that he can write the formal authorization letters for this. He will also tell thee who needs a copy.");
							ppd->captain_state++; didsay=1;
							break;
					case 4:		if (ch[co].level>=53 && ppd->judge_state>=6 && ppd->letter_bits==(2|4|8)) ppd->captain_state++;
							else break;
					case 5:		say(cn,"This fortress holds many secrets, known only to me and a select few of my guards for safety reasons. But now there is a leak.");
							ppd->captain_state++; didsay=1;
							break;
					case 6:		say(cn,"A traitor was caught last night, on his way to the bandits carrying classified notes stolen from our archive.");
							ppd->captain_state++; didsay=1;
							break;
					case 7:		say(cn,"He was caught in the act, but even under torture we have failed to make him speak.");
							ppd->captain_state++; didsay=1;
							break;
					case 8:		say(cn,"The clerk has reported more notes missing, so he was not alone in this treachery. Thou shouldst speak with the clerk.");
							ppd->captain_state++; didsay=1;
							break;
					case 9:		say(cn,"And a little warning. Thy entrance pass is only valid in open areas of the fortress, some places my guard will attack thee still.");
							ppd->captain_state++; didsay=1;
							break;
					case 10:	break;


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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->captain_state>0 && ppd->captain_state<=4 && ppd->judge_state==0) { dat->last_talk=0; ppd->captain_state=1; }
						if (ppd && ppd->captain_state>=5 && ppd->captain_state<=10) { dat->last_talk=0; ppd->captain_state=5; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_LETTER1 && ppd->captain_state==0) {
                                        destroy_item_byID(co,IID_ARKHATA_LETTER1);
					ppd->captain_state=1;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (it[in].ID==IID_ARKHATA_LETTER4 && ppd && !(ppd->letter_bits&8)) {
					quiet_say(cn,"The judge has not left anything out, perfect! And thank thee; Rammy was right to trust thee.");
                                        destroy_item_byID(co,IID_ARKHATA_LETTER4);

                                        ppd->letter_bits|=8;
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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

void judge_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->judge_state) {
					case 0:		if (ppd->captain_state>0) ppd->judge_state++;
							else break;
					case 1:		say(cn,"A, hello %s! So the captain needs a system of authorization letters for people to pass through the fortress?",get_army_rank_string(co));
                                                        ppd->judge_state++; didsay=1;
							break;
					case 2:		say(cn,"I'm not surprised that this would become an issue. I'll write the formal agreements up right away, please wait a moment.");
                                                        ppd->judge_state++; didsay=1;
							break;
					case 3:		say(cn,"Here, take these three agreements, one for Ramin, one for Count Brannington and one for the fortress Captain.");
							if (!(ppd->letter_bits&2) && !has_item(co,IID_ARKHATA_LETTER2)) {
								in=create_item("letter2");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
							if (!(ppd->letter_bits&4) && !has_item(co,IID_ARKHATA_LETTER3)) {
								in=create_item("letter3");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
							if (!(ppd->letter_bits&8) && !has_item(co,IID_ARKHATA_LETTER4)) {
								in=create_item("letter4");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
                                                        ppd->judge_state++; didsay=1;
							break;
					case 4:		say(cn,"And for thee, an entrance pass to the fortress, whilst carrying it, no guard will attack thee.");
							if (!has_item(co,IID_ARKHATA_LETTER5)) {
								in=create_item("letter5");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
                                                        ppd->judge_state++; didsay=1;
							break;
					case 5:		if (ppd->letter_bits!=(2|4|8)) say(cn,"Now please deliver those agreements for me, and speak with Rammy again afterwards. I'm sure he will be happy to hear that this problem is solved.");
                                                        ppd->judge_state++; didsay=1;
							break;
					case 6:		break;
					

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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->judge_state>0 && ppd->judge_state<=6 && ppd->letter_bits!=(2|4|8)) { dat->last_talk=0; ppd->judge_state=1; }
						if (ppd && ppd->judge_state>0 && ppd->judge_state<=6 && ppd->letter_bits==(2|4|8)) { dat->last_talk=0; ppd->judge_state=4; }
						//if (ppd && ppd->judge_state>=7 && ppd->judge_state<=9) { dat->last_talk=0; ppd->judge_state=7; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_LETTER1 && ppd->judge_state==0) {
                                        destroy_item_byID(co,IID_ARKHATA_LETTER1);
					ppd->judge_state=1;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

struct fortressguard_driver_data
{
	int startdist;
	int chardist;
	int stopdist;

	int aggressive;
	int helper;
	int scavenger;

        int dir;

	int dayx;
	int dayy;
	int daydir;

	int nightx;
	int nighty;
	int nightdir;

	int teleport;

	int helpid;

	int creation_time;	

	int notsecure;
	int mindist;

	int lastfight;

	int poison_power;
	int poison_chance;
	int poison_type;
	int drinkspecial;
};

void fortressguard_driver_parse(int cn,struct fortressguard_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"aggressive")) dat->aggressive=atoi(value);
		else if (!strcmp(name,"scavenger")) dat->scavenger=atoi(value);
		else if (!strcmp(name,"helper")) dat->helper=atoi(value);
		else if (!strcmp(name,"startdist")) dat->startdist=atoi(value);
		else if (!strcmp(name,"chardist")) dat->chardist=atoi(value);
		else if (!strcmp(name,"stopdist")) dat->stopdist=atoi(value);
                else if (!strcmp(name,"dir")) dat->dir=atoi(value);
		else if (!strcmp(name,"dayx")) dat->dayx=atoi(value);
		else if (!strcmp(name,"dayy")) dat->dayy=atoi(value);
		else if (!strcmp(name,"daydir")) dat->daydir=atoi(value);
		else if (!strcmp(name,"nightx")) dat->nightx=atoi(value);
		else if (!strcmp(name,"nighty")) dat->nighty=atoi(value);
		else if (!strcmp(name,"nightdir")) dat->nightdir=atoi(value);
		else if (!strcmp(name,"teleport")) dat->teleport=atoi(value);
		else if (!strcmp(name,"helpid")) dat->helpid=atoi(value);
		else if (!strcmp(name,"notsecure")) dat->notsecure=atoi(value);
		else if (!strcmp(name,"mindist")) dat->mindist=atoi(value);
		else if (!strcmp(name,"poisonpower")) dat->poison_power=atoi(value);
		else if (!strcmp(name,"poisontype")) dat->poison_type=atoi(value);
		else if (!strcmp(name,"poisonchance")) dat->poison_chance=atoi(value);
		else if (!strcmp(name,"drinkspecial")) dat->drinkspecial=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void fortressguard_driver(int cn,int ret,int lastact)
{
        struct fortressguard_driver_data *dat;
	struct msg *msg,*next;
        int co,friend=0,x,y,n,in;

	dat=set_data(cn,DRD_SIMPLEBADDYDRIVER,sizeof(struct fortressguard_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch(msg->type) {
			case NT_CREATE:
				if (ch[cn].arg) {
					dat->aggressive=0;
					dat->helper=0;
					dat->startdist=20;
					dat->chardist=0;
					dat->stopdist=40;
					dat->scavenger=0;
					dat->dir=DX_DOWN;
	
					fortressguard_driver_parse(cn,dat);
					ch[cn].arg=NULL;
				}
				dat->creation_time=ticker;
				fight_driver_set_dist(cn,dat->startdist,dat->chardist,dat->stopdist);
				
				if (ch[cn].item[30] && (ch[cn].flags&CF_NOBODY)) {
					ch[cn].flags&=~(CF_NOBODY);
					ch[cn].flags|=CF_ITEMDEATH;
					//xlog("transformed item %s",it[ch[cn].item[30]].name);
				}
				break;

			case NT_CHAR:
				co=msg->dat1;
                                if (dat->helper &&
				    ch[co].group==ch[cn].group &&
				    cn!=co &&
				    ch[cn].value[0][V_BLESS] &&
				    ch[cn].mana>=BLESSCOST &&
				    may_add_spell(co,IDR_BLESS) &&
				    char_see_char(cn,co))
					friend=co;
				if (!dat->aggressive || !is_valid_enemy(cn,(co=msg->dat1),-1)) break;
				if (has_item(co,IID_ARKHATA_LETTER5)) break;
				
				fight_driver_add_enemy(cn,co,0,1);
                                break;

			case NT_TEXT:
				co=msg->dat3;
				tabunga(cn,co,(char*)msg->dat2);
				break;
			
			case NT_DIDHIT:
                                if (dat->poison_power && msg->dat1 && msg->dat2>0 && can_attack(cn,msg->dat1) && RANDOM(100)<dat->poison_chance)
					poison_someone(msg->dat1,dat->poison_power,dat->poison_type);
				break;

			case NT_NPC:
				if (dat->helpid && msg->dat1==dat->helpid && (co=msg->dat2)!=cn && ch[co].group==ch[cn].group)
					fight_driver_add_enemy(cn,msg->dat3,1,0);
				break;
		}
		

                standard_message_driver(cn,msg,0,dat->helper);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) {
		if (ticker-dat->lastfight>TICKS*10) {
			sound_area(ch[cn].x,ch[cn].y,1);			
		}
		dat->lastfight=ticker;
		return;
	}
        if (fight_driver_follow_invisible(cn)) { dat->lastfight=ticker; return; }

	// look around for a second if we've just been created
	if (ticker-dat->creation_time<TICKS) {
                do_idle(cn,TICKS/4);
		return;
	}

        if (dat->scavenger) {
                if (abs(ch[cn].x-ch[cn].tmpx)>=dat->scavenger || abs(ch[cn].y-ch[cn].tmpy)>=dat->scavenger) {
			dat->dir=0;
			if (dat->notsecure) {
				if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
			} else {
				if (ticker-dat->lastfight>TICKS*10) {
					if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
				} else {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;					
				}
			}
		}

		if (regenerate_driver(cn)) return;

		if (spell_self_driver(cn)) return;

		// help friend by blessing him. all checks already done in message loop
		if (friend && do_bless(cn,friend)) return;

                // dont walk all the time
		if (!RANDOM(2)) {
			do_idle(cn,TICKS);
			return;
		}
	
		if (!dat->dir) dat->dir=RANDOM(8)+1;
	
		dx2offset(dat->dir,&x,&y,NULL);
		
		if (abs(ch[cn].x+x-ch[cn].tmpx)<dat->scavenger && abs(ch[cn].y+y-ch[cn].tmpy)<dat->scavenger && do_walk(cn,dat->dir)) {
			fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
			return;
		} else dat->dir=0;		
	} else {
		if (dat->dayx) {
                        if (hour>19 || hour<6) {
                                if (dat->teleport && teleport_char_driver(cn,dat->nightx,dat->nighty)) {
					fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
					return;
				}
				if (dat->notsecure) {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
				} else {
					if (ticker-dat->lastfight>TICKS*10) {
						if (secure_move_driver(cn,dat->nightx,dat->nighty,dat->nightdir,ret,lastact)) return;
					} else {
						if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;						
					}
				}
                                fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
			} else {
                                if (dat->teleport && teleport_char_driver(cn,dat->dayx,dat->dayy)) return;
				if (dat->notsecure) {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
				} else {
					if (ticker-dat->lastfight>TICKS*10) {
						if (secure_move_driver(cn,dat->dayx,dat->dayy,dat->daydir,ret,lastact)) return;
					} else {
						if (move_driver(cn,dat->dayx,dat->dayy,0)) return;						
					}
				}
                                fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
			}
		} else {
                        if (dat->teleport && teleport_char_driver(cn,ch[cn].tmpx,ch[cn].tmpy)) return;
			
			if (dat->notsecure) {
				if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->mindist)) return;
				//say(cn,"move failed, %d, target=%d,%d",pathnodes(),ch[cn].tmpx,ch[cn].tmpy);
			} else {
				if (ticker-dat->lastfight>TICKS*10) {
					if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;
				} else {
					if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;					
				}
			}
                        fight_driver_set_home(cn,ch[cn].x,ch[cn].y);
		}		
	}

	if (dat->drinkspecial) {
		for (n=11; n<30; n++) {
			if ((in=ch[cn].item[n]) && it[in].driver==IDR_POISON0) {
				emote(cn,"drinks a potion");
				remove_all_poison(cn);
				break;
			}
		}
	}


	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

	// help friend by blessing him. all checks already done in message loop
	if (friend && do_bless(cn,friend)) return;
	
        //say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}

void jada_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->jada_state) {
					case 0:		if (ppd->ramin_state>=12) ppd->jada_state++;
							else break;
					case 1:		say(cn,"Hello there, %s. Thou hast been sent from Ramin I see.",ch[co].name);
							questlog_open(co,72);
                                                        ppd->jada_state++; didsay=1;
							break;
					case 2:		say(cn,"I have discovered that the source of this evil that seems to penetrate our fortress is placed somewhere below it, in the cave system.");
                                                        ppd->jada_state++; didsay=1;
							break;
					case 3:		say(cn,"The hole in the corner there is our safe entrance to the cave system, we have only been able to search a small part of the caves. I ask thee to go down there and find the source of this evil and bring it to me.");
                                                        ppd->jada_state++; didsay=1;
							break;
					case 4:		break; // waiting for blade
					case 5:		break; // got blade, all done
					

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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->jada_state>0 && ppd->jada_state<=4) { dat->last_talk=0; ppd->jada_state=1; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_BLADE && ppd->jada_state>0 && ppd->jada_state<=4) {
                                        destroy_item_byID(co,IID_ARKHATA_BLADE);
					questlog_done(co,72);
					ppd->jada_state=5;
					say(cn,"By the bless-swirls, this thing is a concentration of evil! I will have to ask the monks for help to contain it. Thank thee, thou hast most certainly saved us all!");

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void potmaker_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0,in2;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->pot_state) {
					case 0:		if (ch[co].level>=48) ppd->pot_state++;
							else break;
					case 1:		say(cn,"Hello Stranger, I'm afraid someone stole a rather special pot I made on order from the Monk Thai Pan. I made it from iron blessed with holy water from a spring in the mountains outside the fortress.");
							questlog_open(co,73);
                                                        ppd->pot_state++; didsay=1;
							break;
					case 2:		say(cn,"It is quite a valuable pot, it can hold water in temperatures far below freezing without it turning to ice. Thai Pan has told me he could sense it's magic south of his temple, perhaps thou should search the forest in that direction.");
                                                        ppd->pot_state++; didsay=1;
							break;
                                        case 3:		break; // waiting for pot
					case 4:		break; // got pot, all done
					

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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->pot_state>0 && ppd->pot_state<=3) { dat->last_talk=0; ppd->pot_state=1; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_IRONPOT && ppd->pot_state>0 && ppd->pot_state<=3) {
                                        destroy_item_byID(co,IID_ARKHATA_IRONPOT);
					questlog_done(co,73);
					ppd->pot_state=4;
					say(cn,"May thou be blessed by all that is good in this world, I'm in thy debt. Here, take this smaller pot which holds the same holy water. Thou might find need for it some time.");

					in2=create_item("infravision_pot");
					if (in2 && !give_char_item(co,in2)) destroy_item(in2);

                                        // let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void hunter_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->hunter_state) {
					case 0:		if (ppd->pot_state>=4) ppd->hunter_state=4;
							else if (ppd->pot_state>0) ppd->hunter_state++;
							else break;
					case 1:		say(cn,"Hail adventurer! I see thou are seeking for a ceremonial pot. Well that is an odd coincidence.");
                                                        ppd->hunter_state++; didsay=1;
							break;
					case 2:		say(cn,"Last night I heard the strangest noises from the bandit's hideout, one of them came running out yelling about a frozen toe.");
                                                        ppd->hunter_state++; didsay=1;
							break;
					case 3:		say(cn,"He made the wolf I was about to slay run away from me with all that screaming.");
                                                        ppd->hunter_state++; didsay=1;
							break;
                                        case 4:		if (ch[co].level>=58) ppd->hunter_state++;
							else break;
					case 5:		say(cn,"It's great to see thee again adventurer! News of thy deeds in Arkhata has reached even me.");
							questlog_open(co,77);
                                                        ppd->hunter_state++; didsay=1;
							break;
					case 6:		say(cn,"Along with several complaints of a troublesome blue harpy. I have searched for it many a night.");
                                                        ppd->hunter_state++; didsay=1;
							break;
					case 7:		say(cn,"Following hints from people who's seen it. Yet I have failed to seek out and slay the beast, it appears to move across a vast territory.");
                                                        ppd->hunter_state++; didsay=1;
							break;
					case 8:		say(cn,"I will reward thee for slaying it, but its skin and the honor should be mine.");
                                                        ppd->hunter_state++; didsay=1;
							break;
					case 9:		break; // waiting for harpy
					case 10:	break; // all done
					

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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->hunter_state>0 && ppd->hunter_state<=4) { dat->last_talk=0; ppd->hunter_state=1; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_HARPY && ppd->hunter_state>=5 && ppd->hunter_state<=9) {
                                        destroy_item_byID(co,IID_ARKHATA_HARPY);
					questlog_done(co,77);
					ppd->hunter_state=10;
					say(cn,"Ah thou did it! I knew I could count on thee. Here take these 150 gold coins and say nothing of this to anyone. Farwell!");
					give_money(co,150*100,"Solved Hunter Quest");

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
                                        say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void thaipan_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->thai_state) {
					case 0:		if (ch[co].level>=49 && ppd->pot_state>=4) ppd->thai_state++;
							else break;
					case 1:		say(cn,"Aaaaaaaaaaooooommmm... Oh, hello there friend. I'm Thai Pan, monk in this small place of worship. Thou are welcome to have a cup of green tea with me.");
							questlog_open(co,74);
                                                        ppd->thai_state++; didsay=1;
							break;
					case 2:		say(cn,"I know many stories and there is one in particular I think will interest thee.");
                                                        ppd->thai_state++; didsay=1;
							break;
					case 3:		say(cn,"It is said that the ruins on the shore north of here was once a sanctuary for scholars of my order.");
                                                        ppd->thai_state++; didsay=1;
							break;
					case 4:		say(cn,"And that they were researching some ancient scrolls that somehow should lead them to a higher level of understanding.");
                                                        ppd->thai_state++; didsay=1;
							break;
					case 5:		say(cn,"But their search was powered by greed, not by the will to do good. And as they discovered some of the ancient secrets it corrupted their minds and lead do their downfall.");
                                                        ppd->thai_state++; didsay=1;
							break;
					case 6:		say(cn,"Now the ruins are occupied by zombies, who have inhabited the place for as long as I have records of.");
                                                        ppd->thai_state++; didsay=1;
							break;
					case 7:		say(cn,"Very few have ventured there in recent years, but I am told that the place is filled with magic and holds a strange shrine.");
                                                        ppd->thai_state++; didsay=1;
							break;
					case 8:		break; // waiting for scroll
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

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->thai_state>0 && ppd->thai_state<=8) { dat->last_talk=0; ppd->thai_state=1; }
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_SCROLL2 && ppd->thai_state>0 && ppd->thai_state<=8) {
                                        destroy_item_byID(co,IID_ARKHATA_SCROLL2);
					questlog_done(co,74);
					ppd->thai_state=9;
					say(cn,"So the story is true then, may the secrets within this scroll be thine.");

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (ppd && it[in].ID==IID_ARKHATA_BUDDA && ppd->thai_state>0 && ch[co].exp_used>ch[co].exp && realtime-ppd->last_budda>60*60*24) {
					int v,w;

					say(cn,"May thou find peace and recover from any discomfort thou have had.");
					v=ch[co].exp_used-ch[co].exp;
					
					ppd->last_budda=realtime;
					w=ch[co].exp_used/200;
					v=min(v,w);
					dlog(co,0,"got %d exp for thai pan quest 2",v);
					give_exp(co,v);

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
					if (ppd && it[in].ID==IID_ARKHATA_BUDDA && ppd->thai_state>0) {
						if (ch[co].exp_used<=ch[co].exp) say(cn,"Thou doest not have any negative experience.");
						else if (realtime-ppd->last_budda<=60*60*24) say(cn,"Thou canst only do this once per day.");
					} else say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

#define CLERKTIME	(60*15)

void clerk_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->clerk_state) {
					case 0:		if (ppd->captain_state>=5) ppd->clerk_state++;
							else break;
					case 1:		say(cn,"So the Captain sent thee to me? That is good.");
							questlog_open(co,76);
                                                        ppd->clerk_state++; didsay=1;
							break;
					case 2:		say(cn,"Three notes containing information about a transport from Brannington to Arkhata due tomorow night are missing. Most likely the traitors will pass these notes to the robbers tonight.");
                                                        ppd->clerk_state++; didsay=1;
							break;
					case 3:		say(cn,"Are thou able to find these traitors in time? Thou will have three hours (Astonia time)! Say °c04Aye°c0 when thou are ready!");
                                                        ppd->clerk_state++; didsay=1;
							break;
					case 4:		break; // waiting for answer
                                        case 5:		if (realtime-ppd->clerk_time>CLERKTIME) {
								say(cn,"Thou art too late, %s. Thou hast failed me.",ch[co].name);
								ppd->clerk_state=6;
								didsay=1;
							}
							break; // waiting for scrolls
					case 6:		break; // all done

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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->clerk_state>0 && ppd->clerk_state<=4) { dat->last_talk=0; ppd->clerk_state=1; }
						break;
				case 6:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && (ppd->clerk_state==4 || (ch[co].flags&CF_GOD))) {
							ppd->clerk_time=realtime;
							ppd->clerk_state=5;
							say(cn,"Very good, thou have three hours (astonia time) to retrieve the notes. Good Luck!");

                                                        log_char(co,LOG_SYSTEM,0,"Your stopwatch will vanish if you leave the area or log off. The quest, however, will still be open and the three hours will continue to run out. The clerk will give you a new watch if you ask him for a 'watch'.");
							
							in=create_item("stopwatch");
							if (in && !give_char_item(co,in)) destroy_item(in);
							destroy_item_byID(co,IID_ARKHATA_NOTE1);
							destroy_item_byID(co,IID_ARKHATA_NOTE2);
							destroy_item_byID(co,IID_ARKHATA_NOTE3);
						}
						break;
				case 7:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->clerk_state==5) {
                                                        log_char(co,LOG_SYSTEM,0,"Your stopwatch will vanish if you leave the area or log off. The quest, however, will still be open and the three hours will continue to run out. The clerk will give you a new watch if you ask him for a 'watch'.");
							
							in=create_item("stopwatch");
							if (in && !give_char_item(co,in)) destroy_item(in);
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
				ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
                                if (ppd && it[in].ID==IID_ARKHATA_NOTE1 && realtime-ppd->clerk_time<CLERKTIME && ppd->clerk_state==5 && !(ppd->clerk_bits&1)) {
                                        destroy_item_byID(co,IID_ARKHATA_NOTE1);
					ppd->clerk_bits|=1;
					if (ppd->clerk_bits==(1|2|4)) {
						say(cn,"Thou have done a great job. Now the transport will be safe, thank thee.");
						questlog_done(co,76);
					} else say(cn,"Oh there might be hope after all then.");

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (ppd && it[in].ID==IID_ARKHATA_NOTE2 && realtime-ppd->clerk_time<CLERKTIME && ppd->clerk_state==5 && !(ppd->clerk_bits&2)) {
                                        destroy_item_byID(co,IID_ARKHATA_NOTE2);
					ppd->clerk_bits|=2;
					if (ppd->clerk_bits==(1|2|4)) {
						say(cn,"Thou have done a great job, Now the transport will be safe, thank thee.");
						questlog_done(co,76);
					} else say(cn,"Oh there might be hope after all then.");

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (ppd && it[in].ID==IID_ARKHATA_NOTE3 && realtime-ppd->clerk_time<CLERKTIME && ppd->clerk_state==5 && !(ppd->clerk_bits&4)) {
                                        destroy_item_byID(co,IID_ARKHATA_NOTE3);
					ppd->clerk_bits|=4;
					if (ppd->clerk_bits==(1|2|4)) {
						say(cn,"Thou have done a great job, Now the transport will be safe, thank thee.");
						questlog_done(co,76);
						ppd->clerk_state=6;
					} else say(cn,"Oh there might be hope after all then.");

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
					say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void trainer_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->trainer_state) {
					case 0:		if (ch[co].level>=53 && ppd->fiona_state>=4) ppd->trainer_state++;
							else break;
					case 1:		say(cn,"Greetings, adventurer who returned the ring to my Queen Fiona. As one who may train in this academy now, thy loyalty is expected in return.");
							questlog_open(co,75);
                                                        ppd->trainer_state++; didsay=1;
							break;
					case 2:		say(cn,"And I'm in need of thine services. One of my students has been kidnapped by the gang who has their guild east of our fine establishment.");
                                                        ppd->trainer_state++; didsay=1;
							break;
					case 3:		say(cn,"Usually the evil gang doesn't trouble us, but one of my more reckless students ventured too far on his own.");
                                                        ppd->trainer_state++; didsay=1;
							break;
					case 4:		say(cn,"Now I believe the gang leader is torturing him, or worse, to learn our fighting secrets.");
                                                        ppd->trainer_state++; didsay=1;
							break;
					case 5:		say(cn,"I need thee to go get the student back, before our secrets are given away. Time is short, so go now!");
                                                        ppd->trainer_state++; didsay=1;
							break;
                                        case 6:		if (ppd->kid_state==5) { // waiting for student to be rescued
								questlog_done(co,75);
								ppd->trainer_state++;
							} else break;
					case 7:		say(cn,"Thank thee great fighter. Now that my student is safe I can sleep at night. Thou have proven thy skills once again. The door to the academy will always be open to thee.");
							ppd->trainer_state++; didsay=1;
							break;
					case 8:		break;



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
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (ppd && ppd->trainer_state>0 && ppd->trainer_state<=6) { dat->last_talk=0; ppd->trainer_state=1; }
						if (ppd && ppd->trainer_state>=7 && ppd->trainer_state<=8) { dat->last_talk=0; ppd->trainer_state=7; }
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
				say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
                                if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                ch[cn].citem=0;
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

void kidnappee_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk while invisible
			if ((ch[cn].flags&CF_INVISIBLE)) { remove_message(cn,msg); continue; }

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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->kid_state) {
					case 0:		if (ppd->trainer_state>0) ppd->kid_state++;
							else break;
					case 1:		say(cn,"Thou must have been sent to rescue me! Oh how glad I am to see thee! Please open this cage and let me out!");
							ppd->kid_state++; didsay=1;
							break;
					case 2:		if ((in=has_item(co,IID_ARKHATA_IRONPOTION))) {
								emote(co,"uses the bend iron potion to open the cage.");
								remove_item_char(in);
								destroy_item(in);
								ppd->kid_state=4;
							} else {
								say(cn,"Only their leader could seal my cage, only he gets close to me. He must have the secret to open this cage.");
								ppd->kid_state=3;
							}
							didsay=1;
							break;
					case 3:		if (has_item(co,IID_ARKHATA_IRONPOTION)) ppd->kid_state=2;
							break;
					case 4:		say(cn,"Thank thee so much for rescuing me.");
							log_char(co,LOG_SYSTEM,0,"You've rescued the student. Now go back to the trainer to claim your reward.");
                                                        ppd->kid_state++; didsay=1;

							dat->misc=ticker;
							ch[cn].flags|=CF_INVISIBLE;
							set_sector(ch[cn].x,ch[co].y);
							break;
					case 5:		break; // all done
	

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

			if ((ch[cn].flags&CF_INVISIBLE)) { remove_message(cn,msg); continue; }

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						//if (ppd && ppd->kid_state>0 && ppd->kid_state<=6) { dat->last_talk=0; ppd->kid_state=1; }
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
				say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
                                if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                ch[cn].citem=0;
			}
		}
		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if ((ch[cn].flags&CF_INVISIBLE) && ticker-dat->misc>TICKS*60) {
		ch[cn].flags&=~CF_INVISIBLE;
		set_sector(ch[cn].x,ch[cn].y);
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void krenach_driver(int cn,int ret,int lastact)
{
	struct std_npc_driver_data *dat;
	struct arkhata_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_STDNPCDRIVER,sizeof(struct std_npc_driver_data));
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
                        ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

                        if (ppd) {
				switch(ppd->krenach_state) {
					case 0:		if (ppd->monk_state>=29) ppd->krenach_state++;
							else {
								if (realtime-ppd->krenach_time>6*50) {
									say(cn,"Mrec amil groowah! Giln morg awastu.");
									ppd->krenach_time=realtime;
								}
								break;
							}
					case 1:		say(cn,"So thou have met my grandson? And he is well? Oh by the pickaxe's tip thou are one blessed human.");
							questlog_done(co,78);
                                                        ppd->krenach_state++; didsay=1;
							break;
					case 2:		say(cn,"I am very happy to hear news of him. And to finally have someone to talk with.");
							ppd->krenach_state++; didsay=1;
							break;
					case 3:		say(cn,"10000g for a book is alot of money. I see thou have taken good care of it. Here take these 5000g from me. All in all, the price of thine adventure should be more reasonable now.");
							give_money(co,5000*100,"Krenach Dictionary Quest");
							ppd->krenach_state++; didsay=1;
							break;
					case 4:		say(cn,"Go in peace, human.");
							ppd->krenach_state++; didsay=1;
							break;
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
			if ((in=ch[cn].citem)) {	// we still have it
				say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
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

void prisoner_driver(int cn,int ret,int lastact)
{
	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void bookeater_dead(int cn,int co)
{
	struct arkhata_ppd *ppd;

	if (!co || !(ch[co].flags&CF_PLAYER)) return;

	ppd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
	if (!ppd) return;

	if (ppd->monk_state!=19) return;

        questlog_done(co,70);
	ppd->monk_state=20;
	log_char(co,LOG_SYSTEM,0,"Well done, you've solved Tracy's quest. Now report back to her.");
}

void pool_driver(int in,int cn)
{
	int in2,r;

	if (!cn) {	// automatic call
		return;
	}
	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You sense that you have to use the pool with another item (put it on your mouse cursor).");
		return;
	}
	if (it[in2].ID==IID_ARKHATA_SCROLL1) {
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		destroy_item(in2);
		
		r=RANDOM(70);
		if (r==22 || r==33) in2=create_item("Red_Scroll");
		else if (r==42) in2=create_item("Buddah_Statue");
		else in2=0;
		
		if (in2) {
		       if (!give_char_item(cn,in2)) destroy_item(in2);
		       else log_char(cn,LOG_SYSTEM,0,"You got a %s.",it[in2].name);
		} else log_char(cn,LOG_SYSTEM,0,"It vanished in the pool. You sense that the idea was right, but more of the same is needed for a result.");
	} else log_char(cn,LOG_SYSTEM,0,"Strangely, the %s floats on the surface of the pool. Since nothing happens to it, you take it back.",it[in2].name);
}

void stopwatch_driver(int in,int cn)
{
	struct arkhata_ppd *ppd;
	int diff;

	if (cn) return;

	call_item(it[in].driver,in,0,ticker+10);
	
	cn=it[in].carried;
	if (!cn) return;

	ppd=set_data(cn,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
	
	if (ppd) {
		if (ppd->clerk_state==5) {
			diff=CLERKTIME-realtime+ppd->clerk_time;
			if (diff>0) log_char(cn,LOG_SYSTEM,0,"#91 Time: %d Astonian Minutes",diff/5);
			else log_char(cn,LOG_SYSTEM,0,"#92 YOU FAILED!");
		} else log_char(cn,LOG_SYSTEM,0,"#92 ");
	}
}

void key_assemble_driver(int in,int cn)
{
	int in2;

        if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You can only use this item with another item.");
		return;
	}
	if ((it[in].ID==IID_ARKHATA_AKEY1 && it[in2].ID==IID_ARKHATA_AKEY2) ||
	    (it[in].ID==IID_ARKHATA_AKEY2 && it[in2].ID==IID_ARKHATA_AKEY1)) {
		destroy_item(in2);
		ch[cn].citem=0;
		it[in].sprite=13421;
		it[in].ID=IID_ARKHATA_AKEY12;
		ch[cn].flags|=CF_ITEMS;
	} else if ((it[in].ID==IID_ARKHATA_AKEY2 && it[in2].ID==IID_ARKHATA_AKEY3) ||
	           (it[in].ID==IID_ARKHATA_AKEY3 && it[in2].ID==IID_ARKHATA_AKEY2)) {
		destroy_item(in2);
		ch[cn].citem=0;
		it[in].sprite=13420;
		it[in].ID=IID_ARKHATA_AKEY23;
		ch[cn].flags|=CF_ITEMS;
	} else if ((it[in].ID==IID_ARKHATA_AKEY1 && it[in2].ID==IID_ARKHATA_AKEY23) ||
	           (it[in].ID==IID_ARKHATA_AKEY23 && it[in2].ID==IID_ARKHATA_AKEY1) ||
		   (it[in].ID==IID_ARKHATA_AKEY12 && it[in2].ID==IID_ARKHATA_AKEY3) ||
	           (it[in].ID==IID_ARKHATA_AKEY3 && it[in2].ID==IID_ARKHATA_AKEY12)) {
		destroy_item(in2);
		ch[cn].citem=0;
		it[in].sprite=13413;
		it[in].ID=IID_ARKHATA_AKEY;
		sprintf(it[in].name,"Knoger Key 1");
		sprintf(it[in].description,"A finished key. Should open something now. A door, perhaps.");
		ch[cn].flags|=CF_ITEMS;
	} else log_char(cn,LOG_SYSTEM,0,"This doesn't seem to fit.");
}

void arkhata_item_driver(int in,int cn)
{
	switch(it[in].drdata[0]) {
		case 0:		pool_driver(in,cn); break;
		case 1:		stopwatch_driver(in,cn); break;
		case 2:		key_assemble_driver(in,cn); break;
	}
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

void prisoner_dead(int cn,int co)
{
	say(cn,"I know the secret, it's right here!");
}

void madhermit_driver(int cn,int ret,int lastact)
{
	struct msg *msg,*next;
        int co,in;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,30,0,60);			
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			if (ch[co].action==AC_USE && (in=ch[co].act1) && in>0 && in<MAXITEM && it[in].driver==IDR_FLOWER) {
				if (fight_driver_add_enemy(cn,co,1,1)) shout(cn,"Hey! %s! Those flowers are mine!",ch[co].name);
			}
		}

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                standard_message_driver(cn,msg,0,0);
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

        do_idle(cn,TICKS/2);
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_RAMMY:		rammy_driver(cn,ret,lastact); return 1;
		case CDR_JAZ:		jaz_driver(cn,ret,lastact); return 1;
		case CDR_FIONA:		fiona_driver(cn,ret,lastact); return 1;
		case CDR_BRIDGEGUARD:	bridgeguard_driver(cn,ret,lastact); return 1;
		case CDR_GLADIATOR:	gladiator_driver(cn,ret,lastact); return 1;
		case CDR_NOP:		nop_driver(cn,ret,lastact); return 1;
		case CDR_RAMIN:		ramin_driver(cn,ret,lastact); return 1;
		case CDR_ARKHATASKELLY:	arkhataskelly_driver(cn,ret,lastact); return 1;
		case CDR_ARKHATAMONK:	arkhatamonk_driver(cn,ret,lastact); return 1;
		case CDR_BOOKEATER:	bookeater_driver(cn,ret,lastact); return 1;
		case CDR_CAPTAIN:	captain_driver(cn,ret,lastact); return 1;
		case CDR_JUDGE:		judge_driver(cn,ret,lastact); return 1;
		case CDR_FORTRESSGUARD:	fortressguard_driver(cn,ret,lastact); return 1;
		case CDR_JADA:		jada_driver(cn,ret,lastact); return 1;
		case CDR_POTMAKER:	potmaker_driver(cn,ret,lastact); return 1;
		case CDR_HUNTER:	hunter_driver(cn,ret,lastact); return 1;
		case CDR_THAIPAN:	thaipan_driver(cn,ret,lastact); return 1;
		case CDR_TRAINER:	trainer_driver(cn,ret,lastact); return 1;
		case CDR_KIDNAPPEE:	kidnappee_driver(cn,ret,lastact); return 1;
		case CDR_ARKHATACLERK:	clerk_driver(cn,ret,lastact); return 1;
		case CDR_ARKHATAPRISON:	prisoner_driver(cn,ret,lastact); return 1;
		case CDR_KRENACH:	krenach_driver(cn,ret,lastact); return 1;
		case CDR_MADHERMIT:	madhermit_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_ARKHATA:	arkhata_item_driver(in,cn); return 1;
		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_RAMMY:		immortal_dead(cn,co); return 1;
		case CDR_JAZ:		immortal_dead(cn,co); return 1;
		case CDR_FIONA:		immortal_dead(cn,co); return 1;
		case CDR_BRIDGEGUARD:	return 1;
		case CDR_GLADIATOR:	gladiator_dead(cn,co); return 1;
		case CDR_NOP:		immortal_dead(cn,co); return 1;
		case CDR_ARKHATASKELLY:	arkhataskelly_dead(cn,co); return 1;
		case CDR_ARKHATAMONK:	immortal_dead(cn,co); return 1;
		case CDR_BOOKEATER:	bookeater_dead(cn,co); return 1;
		case CDR_CAPTAIN:	immortal_dead(cn,co); return 1;
		case CDR_JUDGE:		immortal_dead(cn,co); return 1;
		case CDR_FORTRESSGUARD:	return 1;
		case CDR_JADA:		immortal_dead(cn,co); return 1;
		case CDR_POTMAKER:	immortal_dead(cn,co); return 1;
		case CDR_HUNTER:	immortal_dead(cn,co); return 1;
		case CDR_THAIPAN:	immortal_dead(cn,co); return 1;
		case CDR_TRAINER:	immortal_dead(cn,co); return 1;
		case CDR_KIDNAPPEE:	immortal_dead(cn,co); return 1;
		case CDR_ARKHATACLERK:	immortal_dead(cn,co); return 1;
		case CDR_ARKHATAPRISON:	prisoner_dead(cn,co); return 1;
		case CDR_KRENACH:	immortal_dead(cn,co); return 1;
		case CDR_MADHERMIT:	return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_RAMMY:		return 1;
		case CDR_JAZ:		return 1;
		case CDR_FIONA:		return 1;
		case CDR_BRIDGEGUARD:	return 1;
		case CDR_GLADIATOR:	return 1;
		case CDR_RAMIN:		return 1;
		case CDR_NOP:		return 1;
		case CDR_ARKHATASKELLY:	return 1;
		case CDR_ARKHATAMONK:	return 1;
		case CDR_BOOKEATER:	return 1;
		case CDR_CAPTAIN:	return 1;
		case CDR_JUDGE:		return 1;
		case CDR_FORTRESSGUARD:	return 1;
		case CDR_JADA:		return 1;
		case CDR_POTMAKER:	return 1;
		case CDR_HUNTER:	return 1;
		case CDR_THAIPAN:	return 1;
		case CDR_TRAINER:	return 1;
		case CDR_KIDNAPPEE:	return 1;
		case CDR_ARKHATACLERK:	return 1;
		case CDR_ARKHATAPRISON:	return 1;
		case CDR_KRENACH:	return 1;
		case CDR_MADHERMIT:	return 1;

		default:		return 0;
	}
}













