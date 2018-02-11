/*
 * $Id: caligar.c,v 1.4 2008/03/24 11:19:29 devel Exp $
 *
 * $Log: caligar.c,v $
 * Revision 1.4  2008/03/24 11:19:29  devel
 * added arkhata
 *
 * Revision 1.3  2007/10/24 11:38:05  devel
 * fixed a typo
 *
 * Revision 1.2  2007/06/22 12:54:40  devel
 * some fixes
 *
 * Revision 1.1  2007/05/26 13:19:56  devel
 * Initial revision
 *
 * Revision 1.2  2006/09/28 11:58:38  devel
 * added questlog
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
#include "map.h"
#include "create.h"
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "skill.h"
#include "area1.h"
#include "consistency.h"
#include "database.h"
#include "sector.h"
#include "sleep.h"
#include "staffer_ppd.h"
#include "flower_ppd.h"
#include "questlog.h"
#include "player_driver.h"
#include "light.h"
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
	{{"yes","okay",NULL},NULL,3},
	{{"no","not","today",NULL},NULL,4},
	{{"pay","10000g",NULL},NULL,5}
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


//-----------------------


struct caligar_ppd
{
	int guard_state;
	int guard_last_talk;
	int glori_state;
	int glori_last_talk;
	int watch_flag;
	int obelisk_flag;
	int arquin_state;
	int arquin_last_talk;
	int smith_state;
	int smith_last_talk;
	int homden_state;
	int homden_last_talk;
	int amazon_flag;
	int guard2_last_talk;
	unsigned char door_flag[4];
};	

void guard_driver(int cn,int ret,int lastact)
{
        struct caligar_ppd *ppd;
        int co,in,me;
	struct msg *msg,*next;

	if (!strcmp(ch[cn].name,"Eulc")) me=0;
	else me=1;

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
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			// dont talk to people on our side of the fence
			if (ch[co].y>106) { remove_message(cn,msg); continue; }

			ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
			if (!ppd) { remove_message(cn,msg); continue; }

                        if (realtime-ppd->guard_last_talk<3) { remove_message(cn,msg); continue; }

                        switch(ppd->guard_state) {
				case 0:	if (me!=0) break;
					quiet_say(cn,"Human entry is not permitted! Leave at once!");
					ppd->guard_last_talk=realtime;
					ppd->guard_state++;
					break;
				case 1: if (me!=1) break;
					quiet_say(cn,"He let the bed in!");
					ppd->guard_last_talk=realtime;
					ppd->guard_state++;
					break;
				case 2: if (me!=0) break;
					quiet_say(cn,"Quiet you fool!");
					ppd->guard_last_talk=realtime;
					ppd->guard_state++;
					break;
				case 3: if (me!=1) break;
					quiet_say(cn,"Backwards is the key to entry!");
					ppd->guard_last_talk=realtime;
					ppd->guard_state++;
					break;
				case 4: if (me!=0) break;
					quiet_say(cn,"Ugh, I said quiet! We aren't supposed to let humans in!");
					ppd->guard_last_talk=realtime;
					ppd->guard_state++;
					break;
				case 5:	if (realtime-ppd->guard_last_talk>600) ppd->guard_state=0;
					break;
			}
			
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        if ((in=ch[cn].citem)) {	// we still have it
                                if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
                                ch[cn].citem=0;
			}
		}

		// talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
						if (ppd && ppd->guard_state==3) { ppd->guard_state=0; ppd->guard_last_talk=0; }
                                                break;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;

        do_idle(cn,TICKS);

}

void guard2_driver(int cn,int ret,int lastact)
{
        struct caligar_ppd *ppd;
        int co;
	struct msg *msg,*next;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { continue; }

                        // dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) {continue; }
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { continue; }

			ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
			if (!ppd) { continue; }

                        if (realtime-ppd->guard2_last_talk<15) { continue; }

                        say(cn,"Halt! You will die where you stand!");
			ppd->guard2_last_talk=realtime;
		}
	}
	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void skelly_driver(int cn,int ret,int lastact)
{
        char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void skelly_dead_driver(int cn,int co)
{
	struct caligar_ppd *ppd;
	int nr,bit,idx;

	if (!co) return;

	ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
	if (!ppd) return;

	if (ch[cn].tmpx==103) {
		if (ch[cn].tmpy==224) nr=0;
		else if (ch[cn].tmpy==211) nr=1;
		else if (ch[cn].tmpy==198) nr=2;
		else nr=-1;
		idx=0;
	} else if (ch[cn].tmpx==145) {
		if (ch[cn].tmpy==225) nr=0;
		else if (ch[cn].tmpy==212) nr=1;
		else if (ch[cn].tmpy==186) nr=2;
		else nr=-1;
		idx=1;
	} else if (ch[cn].tmpx==226 || ch[cn].tmpx==227) {
		if (ch[cn].tmpy==158) nr=0;
		else if (ch[cn].tmpy==145) nr=1;
		else if (ch[cn].tmpy==132) nr=2;
		else nr=-1;
		idx=2;
	} else { nr=-1; idx=0; }

	if (nr>=0) {
		bit=1<<nr;
		if (ppd->door_flag[idx]&bit) {
			log_char(co,LOG_SYSTEM,0,"You expect to hear a click, but nothing happens. Maybe you've been here before?");
		} else {
			ppd->door_flag[idx]|=bit;

			if (ppd->door_flag[idx]>=(1|2|4)) {
				log_char(co,LOG_SYSTEM,0,"You hear a \"click\" in the distance, as if a lock had opened.");
			} else {
				log_char(co,LOG_SYSTEM,0,"You hear a faint sound in the distance, as if a lock was partially opened.");
			}
		}
	} else log_char(co,LOG_SYSTEM,0,"You have found bug #9824w at %d,%d. Please report it.",ch[cn].tmpx,ch[cn].tmpy);
}

void glori_driver(int cn,int ret,int lastact)
{
        struct caligar_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

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
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
			if (!ppd) { remove_message(cn,msg); continue; }

                        if (realtime-ppd->glori_last_talk<4) { remove_message(cn,msg); continue; }

                        switch(ppd->glori_state) {
				case 0: 	quiet_say(cn,"Thank you for coming %s!",ch[co].name);
						questlog_done(co,54);
						questlog_open(co,55);
						ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 1: 	quiet_say(cn,"I am Glori, First in charge.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 2: 	quiet_say(cn,"We must find out what those mages intend to do with that plaque and retrieve it as soon as possible.");
						ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 3: 	quiet_say(cn,"We are currently working in secrecy with the guard outside of this library. He has informed me that the mages have set up three training facilities to train their minions.");
						ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 4: 	quiet_say(cn,"Travel to the three training facilities to the east and examine the minions fighting styles. Come back to me with your findings.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 5:		if (ppd->watch_flag<(1|2|4)) break;
						quiet_say(cn,"Hello, %s.",ch[co].name);
						ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 6:		log_char(co,LOG_SYSTEM,0,"You tell Glori what you have seen.");
						ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 7:		quiet_say(cn,"Ah, good work. Now that we know how the enemies fight, we can prepare ourselves for battle.");
						questlog_done(co,55);
						questlog_open(co,56);
						ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 8: 	quiet_say(cn,"I have gotten a report that there is a dungeon below one of the traders' shops. It is said to lead to a dungeon full of zombies, and I assume there will be two more like it containing skeletons and vampires.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 9: 	quiet_say(cn,"Please go and investigate this and report back with your findings, if any.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 10: 	if (has_item(co,IID_CALIGAROBELISK1) && has_item(co,IID_CALIGAROBELISK2) && has_item(co,IID_CALIGAROBELISK3)) {
							questlog_done(co,56);
							questlog_open(co,57);
							ppd->glori_last_talk=realtime;
							ppd->glori_state++;
							didsay=1;
						} else break;
				case 11:	quiet_say(cn,"Wow, these are most interesting. I suggest you speak with the guard outside and ask if he knows of anyone that may be able to tell you what these are for.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
                                case 12:	if (has_item(co,IID_CALIGARKEYP1) && has_item(co,IID_CALIGARKEYP2) && has_item(co,IID_CALIGARKEYP3)) {
							questlog_done(co,57);
							questlog_open(co,58);
							ppd->glori_last_talk=realtime;
							ppd->glori_state++;
							didsay=1;
						} else break;
				case 13:	quiet_say(cn,"Great job. I feel we are very close to putting an end to those evil mages ways.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 14:	quiet_say(cn,"Hmm. It might be a good idea to take the key parts to the blacksmith south west of this library, near the bar. He may be able to forge these together.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 15:	quiet_say(cn,"If he can make a complete key from these, he'll probably need some sort of payment. Once the key is made, take it Arquin out front. He should be able to tell you where to go with it.");
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 16:	if (has_item(co,IID_CALIGARDUNGEONKEY)) {
							questlog_done(co,58);
                                                        ppd->glori_last_talk=realtime;
							ppd->glori_state++;
							didsay=1;
						} else break;
				case 17:	quiet_say(cn,"Well done, %s. Did you talk to Homden yet?",ch[co].name);
                                                ppd->glori_last_talk=realtime;
						ppd->glori_state++;
						didsay=1;
						break;
				case 18:	break;

			}
			if (didsay) {
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				ch[cn].clan_serial=10;
			}
			
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_CALIGAROBELISK1 || it[in].ID==IID_CALIGAROBELISK2 || it[in].ID==IID_CALIGAROBELISK3) {
					if (!has_item(co,IID_CALIGAROBELISK3) && it[in].ID!=IID_CALIGAROBELISK3) quiet_say(cn,"You will need all three of them. I've heard a heavy drinker tell about another dungeon being hidden in his favorite place.");
					else if (!has_item(co,IID_CALIGAROBELISK1) && it[in].ID!=IID_CALIGAROBELISK1) quiet_say(cn,"You will need all three of them. One of them, so rumor has it, is behind a large building.");
					else if (!has_item(co,IID_CALIGAROBELISK2) && it[in].ID!=IID_CALIGAROBELISK2) quiet_say(cn,"You will need all three of them. As I said before, one should be accessible through a shop.");
				}
                                if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		// talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
						if (ppd && ppd->glori_state>=1 && ppd->glori_state<=5) { ppd->glori_state=1; ppd->glori_last_talk=0; }
						if (ppd && ppd->glori_state>=8 && ppd->glori_state<=10) { ppd->glori_state=8; ppd->glori_last_talk=0; }
						if (ppd && ppd->glori_state>=11 && ppd->glori_state<=12) { ppd->glori_state=11; ppd->glori_last_talk=0; }
						if (ppd && ppd->glori_state>=14 && ppd->glori_state<=16) { ppd->glori_state=14; ppd->glori_last_talk=0; }
						if (ppd && ppd->glori_state>=17 && ppd->glori_state<=18) { ppd->glori_state=17; ppd->glori_last_talk=0; }
                                                break;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].clan_serial>0) ch[cn].clan_serial--;
        else if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void arquin_driver(int cn,int ret,int lastact)
{
        struct caligar_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

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
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
			if (!ppd) { remove_message(cn,msg); continue; }

                        if (realtime-ppd->arquin_last_talk<4) { remove_message(cn,msg); continue; }

                        switch(ppd->arquin_state) {
				case 0:		if (ppd->glori_state==12) ppd->arquin_state++;	// fall thru
						else break;
				case 1: 	quiet_say(cn,"Obelisks? Well, they may be used as some type of key. Judging by the names of each one I would assume that they open the locked gates inside each training area.");
                                                ppd->arquin_last_talk=realtime;
						ppd->arquin_state++;
						didsay=1;
						break;
				case 2: 	quiet_say(cn,"If you can get into them you can kill the minions being trained there. Bring anything you might find to Glori.");
						ppd->arquin_last_talk=realtime;
						ppd->arquin_state++;
						didsay=1;
						break;
				case 3:		if (has_item(co,IID_CALIGARDUNGEONKEY)) {
							ppd->arquin_state++;
						} else break;
				case 4:		quiet_say(cn,"Aha, I see you have gotten a hold of the key. I know of someone who may be able to tell you what it unlocks. He is a brother of the Carmin Clan, named Homden.");
						ppd->arquin_last_talk=realtime;
						ppd->arquin_state++;
						didsay=1;
						break;
				case 5:		quiet_say(cn,"He was banished from this city by his brothers for not helping them with their plans of destruction, and being that this area is barricaded, he had no choice but to establish shelter in the forest.");
						ppd->arquin_last_talk=realtime;
						ppd->arquin_state++;
						didsay=1;
						break;
				case 6:         break;
			}
			if (didsay) {
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				ch[cn].clan_serial=10;
			}
			
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        if ((in=ch[cn].citem)) {	// we still have it
                                if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		// talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
						if (ppd && ppd->arquin_state>=1 && ppd->arquin_state<=3) { ppd->arquin_state=1; ppd->arquin_last_talk=0; }
						if (ppd && ppd->arquin_state>=4 && ppd->arquin_state<=6) { ppd->arquin_state=4; ppd->arquin_last_talk=0; }
                                                break;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].clan_serial>0) ch[cn].clan_serial--;
        else if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void smith_driver(int cn,int ret,int lastact)
{
        struct caligar_ppd *ppd;
	struct arkhata_ppd *appd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

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
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
			if (!ppd) { remove_message(cn,msg); continue; }

                        if (realtime-ppd->smith_last_talk<4) { remove_message(cn,msg); continue; }

                        switch(ppd->smith_state) {
				case 0:		if (ppd->glori_state==16 &&
						    has_item(co,IID_CALIGARKEYP1) &&
						    has_item(co,IID_CALIGARKEYP2) &&
						    has_item(co,IID_CALIGARKEYP3)) ppd->smith_state++;	// fall thru
						else break;
				case 1: 	quiet_say(cn,"Hello there. I hear you need a key made. Well, for a small fee of 5000 gold I would be more than willing to do it. °c4Yes, Okay°c0 / °c4No, not today°c0");
                                                ppd->smith_last_talk=realtime;
						ppd->smith_state++;
						didsay=1;
						break;
				case 2:         appd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (appd && appd->monk_state>20) ppd->smith_state++;
						else break;
				case 3:		quiet_say(cn,"This monk named Johnatan is wiser than he should be. He is correct. I do descend from the dwarfs, more closely related to the frawds.");
						ppd->smith_state++; ppd->smith_last_talk=realtime; didsay=1;
						break;
				case 4:		quiet_say(cn,"I don't know much of my family's history, but I know we lived up in the mountains for a long time.");
						ppd->smith_state++; ppd->smith_last_talk=realtime; didsay=1;
						break;
				case 5:		quiet_say(cn,"My father still lives up there somewhere. Some of us have later on built a life amongst you humans, and learned your ways and language.");
						ppd->smith_state++; ppd->smith_last_talk=realtime; didsay=1;
						break;
				case 6:		quiet_say(cn,"I even compiled a dictionary to help learning your common tounge, it should be most helpful to translate that book.");
						ppd->smith_state++; ppd->smith_last_talk=realtime; didsay=1;
						break;
				case 7:		quiet_say(cn,"Well I forged you a key for 5000g, a hand written dictionary like this must be worth at least the double. So °c04pay 10000g°c0 must be a fair price don't you think?");
						ppd->smith_state++; ppd->smith_last_talk=realtime; didsay=1;
						break;
				case 8:		break;
			}
			if (didsay) {
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				ch[cn].clan_serial=10;
			}
			
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        if ((in=ch[cn].citem)) {	// we still have it
                                if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		// talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
						if (ppd && ppd->smith_state>=1 && ppd->smith_state<=2) { ppd->smith_state=1; ppd->smith_last_talk=0; }	
						if (ppd && ppd->smith_state>=3 && ppd->smith_state<=8) { ppd->smith_state=3; ppd->smith_last_talk=0; }	
                                                break;
				case 3:		if (has_item(co,IID_CALIGARKEYP1) && has_item(co,IID_CALIGARKEYP2) && has_item(co,IID_CALIGARKEYP3)) {
							if (ch[co].gold<5000*100) {
								quiet_say(cn,"Sorry, it seems you cannot pay me.");
								break;
							}
							in=create_item("caligar_underground_key");
							if (!in) {
								quiet_say(cn,"Oops. You found bug #1635t. Please report it.");
								break;
							}
							if (!give_char_item(co,in)) {
								quiet_say(cn,"No space in inventory, please try again.");
								break;
							}
                                                        destroy_item_byID(co,IID_CALIGARKEYP1);
							destroy_item_byID(co,IID_CALIGARKEYP2);
							destroy_item_byID(co,IID_CALIGARKEYP3);
							ch[co].gold-=5000*100;
							ch[co].flags|=CF_ITEMS;
						} else quiet_say(cn,"You do not appear to have all the neccessary parts.");
						break;
				case 4:		quiet_say(cn,"Okay, come back if you change your mind.");
						break;
				case 5:		appd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
						if (!appd || appd->monk_state<20) {
							break;
						}
						if (ch[co].gold<10000*100) {
							quiet_say(cn,"Sorry, it seems you cannot pay me.");
							break;
						}
						in=create_item("dictionary");
						if (!in) {
							quiet_say(cn,"Oops. You found bug #1636t. Please report it.");
							break;
						}
						if (!give_char_item(co,in)) {
							quiet_say(cn,"No space in inventory, please try again.");
							break;
						}
						ch[co].gold-=10000*100;
						ch[co].flags|=CF_ITEMS;
						break;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].clan_serial>0) ch[cn].clan_serial--;
        else if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void homden_driver(int cn,int ret,int lastact)
{
        struct caligar_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

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
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
			if (!ppd) { remove_message(cn,msg); continue; }

                        if (realtime-ppd->homden_last_talk<4) { remove_message(cn,msg); continue; }

                        switch(ppd->homden_state) {
				case 0:		if (has_item(co,IID_CALIGARDUNGEONKEY)) ppd->homden_state++;	// fall thru
						else break;
				case 1: 	quiet_say(cn,"You come seeking my help? I'd be glad to help if it means my brothers will be put to a stop. However, I need your help first.");
						questlog_open(co,59);
                                                ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 2: 	quiet_say(cn,"A group of amazons keep invading my camp at night, and stealing my personal things. One item, a powerful ring, was taken and I am sure it was them.");
                                                ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 3: 	quiet_say(cn,"If you could please go and find it for me while I gather my thoughts on my brothers I would reward thee. There is a cave to the east, start your search there.");
                                                ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 4:		break;
				case 5:		quiet_say(cn,"Thank you, %s.",ch[co].name);
                                                ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 6:		quiet_say(cn,"Now, about my brothers. They are planning to resurrect the last Emporer. If they succeed, they hope to trick the citizens of Aston into thinking that the Emporer has returned and try to restore his royal status.");
						ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 7:		quiet_say(cn,"Once that happens, they will slowly begin destroying the town, and have said their first target would be the Labyrinths that Ishtar made to strengthen his army.");
						ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 8:		quiet_say(cn,"We can not let this happen. The fate of Astonia depends on those filthy brothers of mine being halted! The door that the key opens can be found down a hole to the north.");
						ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 9:		quiet_say(cn,"It's a passage that leads to the palace. There will be three levels in the palace to test you. Not even I am sure how to navigate it. I do know the plaque you seek is locked in a chest on the last floor of the palace.");
						ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 10:	quiet_say(cn,"If they do not have that plaque, they cannot raise the Emporer. But, I suggest you hurry. Once their army is complete, they will begin trying to raise the Emporer. Good luck adventurer!");
						ppd->homden_last_talk=realtime;
						ppd->homden_state++;
						didsay=1;
						break;
				case 11:	break;


			}
			if (didsay) {
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				ch[cn].clan_serial=10;
			}
			
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        if ((in=ch[cn].citem)) {	// we still have it
				ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
				if (it[in].ID==IID_CALIGARHOMDENRING && ppd && ppd->homden_state==4) {
					questlog_done(co,59);
					ppd->homden_state=5;

					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {
					if (!give_char_item(co,in)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		// talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
						if (ppd && ppd->homden_state>=2 && ppd->homden_state<=4) { ppd->homden_state=2; ppd->homden_last_talk=0; }
						if (ppd && ppd->homden_state>=6 && ppd->homden_state<=11) { ppd->homden_state=6; ppd->homden_last_talk=0; }
                                                break;				
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].clan_serial>0) ch[cn].clan_serial--;
        else if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void caligar_training(int in,int cn)
{
	struct caligar_ppd *ppd;

	if (!cn || !(ch[cn].flags&CF_PLAYER)) return;
	
	ppd=set_data(cn,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
	if (!ppd) return;
	
	switch(it[in].drdata[1]) {
		case 1:	if (ppd->watch_flag&1) break;
			ppd->watch_flag|=1;
			log_char(cn,LOG_SYSTEM,0,"You observe the skeletons fighting techniques: Melee.");
			break;
		case 2: if (ppd->watch_flag&4) break;
			ppd->watch_flag|=4;
			log_char(cn,LOG_SYSTEM,0,"You observe the vampires fighting techniques: Magic and Melee.");
			break;
		case 3: if (ppd->watch_flag&2) break;
			ppd->watch_flag|=2;
			log_char(cn,LOG_SYSTEM,0,"You observe the zombies fighting techniques: Magic.");
			break;		
	}
}

void caligar_weight(int in,int cn)
{
	int m,m2,dx,dy,dir,nr,badfloor=0;

	if (cn) {	// player using item
		m=it[in].x+it[in].y*MAXMAP;
		dir=ch[cn].dir;
		dx2offset(dir,&dx,&dy,NULL);
		m2=(it[in].x+dx)+(it[in].y+dy)*MAXMAP;

		if ((map[m2].gsprite<20797 || map[m2].gsprite>20823) &&
		    map[m2].gsprite!=59683 &&
		    (map[m2].gsprite<20291 || map[m2].gsprite>20299)) badfloor=1;

		if ((map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m2].it || badfloor) {
			log_char(cn,LOG_SYSTEM,0,"It won't move.");
			return;
		}
		map[m].flags&=~MF_TMOVEBLOCK;
		map[m].it=0;
		set_sector(it[in].x,it[in].y);

		map[m2].flags|=MF_TMOVEBLOCK;
		map[m2].it=in;
		it[in].x+=dx; it[in].y+=dy;
		set_sector(it[in].x,it[in].y);

		// hack to avoid using the chest over and over
		if ((nr=ch[cn].player)) player_driver_halt(nr);

		*(unsigned int*)(it[in].drdata+4)=ticker;

		return;
	} else {	// timer call
		if (!(*(unsigned int*)(it[in].drdata+8))) {	// no coords set, so its first call. remember coords
			*(unsigned short*)(it[in].drdata+8)=it[in].x;
			*(unsigned short*)(it[in].drdata+10)=it[in].y;
		}

		// if 5 minutes have passed without anyone touching the chest, beam it back.
		if (ticker-*(unsigned int*)(it[in].drdata+4)>TICKS*60*5 &&
		    (*(unsigned short*)(it[in].drdata+8)!=it[in].x ||
		     *(unsigned short*)(it[in].drdata+10)!=it[in].y)) {
			
			m=it[in].x+it[in].y*MAXMAP;
			m2=(*(unsigned short*)(it[in].drdata+8))+(*(unsigned short*)(it[in].drdata+10))*MAXMAP;

			if (!(map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) && !map[m2].it) {
				map[m].flags&=~MF_TMOVEBLOCK;
				map[m].it=0;
				set_sector(it[in].x,it[in].y);
	
				map[m2].flags|=MF_TMOVEBLOCK;
				map[m2].it=in;
				it[in].x=*(unsigned short*)(it[in].drdata+8);
				it[in].y=*(unsigned short*)(it[in].drdata+10);
				set_sector(it[in].x,it[in].y);
			}
                }
		call_item(it[in].driver,in,0,ticker+TICKS*5);
	}
}

int caligar_weight_door(int in,int cn)
{
	int x,y,oldx,oldy,dx,dy,in2,in3;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
		return 2;
	}

	dx=(ch[cn].x-it[in].x);
	dy=(ch[cn].y-it[in].y);
        if (dx && dy) return 2;

        x=it[in].x-dx;
	y=it[in].y-dy;

	if (dy>0) {
                in2=map[210+184*MAXMAP].it;
		in3=map[213+176*MAXMAP].it;
		if (!in2 || !in3 || it[in2].driver!=144 || it[in3].driver!=144) {
			log_char(cn,LOG_SYSTEM,0,"The door is locked.");
			if (ch[cn].player) player_driver_halt(ch[cn].player);
			return 2;
		}
	}

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return 2;
	}
        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char(cn,x,y,0)) {
                log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,0);		
	}

	switch(ch[cn].dir) {
		case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
		case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
		case DX_UP:	ch[cn].dir=DX_DOWN; break;
		case DX_DOWN:	ch[cn].dir=DX_UP; break;
	}
	if (ch[cn].player) player_driver_halt(ch[cn].player);

	return 1;
}

int caligar_skelly_door(int in,int cn)
{
	struct caligar_ppd *ppd;
	int x,y,oldx,oldy,dx,dy,idx;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
                return 2;
	}

	dx=(ch[cn].x-it[in].x);
	dy=(ch[cn].y-it[in].y);
        if (dx && dy) return 2;

        x=it[in].x-dx;
	y=it[in].y-dy;

	idx=it[in].drdata[1];

	ppd=set_data(cn,DRD_CALIGAR_PPD,sizeof(struct caligar_ppd));
	if (!ppd) return 2;

	if (ppd->door_flag[idx]<(1|2|4)) {
		log_char(cn,LOG_SYSTEM,0,"The door appears to be locked by some strange mechanism. It seems you need to open three seperate locks.");
		return 2;
	}

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return 2;
	}
        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char(cn,x,y,0)) {
                log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,0);		
	}

	switch(ch[cn].dir) {
		case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
		case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
		case DX_UP:	ch[cn].dir=DX_DOWN; break;
		case DX_DOWN:	ch[cn].dir=DX_UP; break;
	}
	if (ch[cn].player) player_driver_stop(ch[cn].player,0);

	return 1;
}

void caligar_gun(int in,int cn,int dir)
{
	int str=50,base=1;
	call_item(it[in].driver,in,0,ticker+12);
	switch(dir) {
		case 2:	create_edemonball(0,it[in].x,it[in].y+1,it[in].x,it[in].y+10,str,base); break;
		case 1:	create_edemonball(0,it[in].x+1,it[in].y,it[in].x+10,it[in].y,str,base); break;
		case 4:	create_edemonball(0,it[in].x,it[in].y-1,it[in].x,it[in].y-10,str,base); break;
		case 3:	create_edemonball(0,it[in].x-1,it[in].y,it[in].x-10,it[in].y,str,base); break;
		
		case 5:	create_edemonball(0,it[in].x,it[in].y+1,it[in].x,it[in].y+10,str,base);
			create_edemonball(0,it[in].x+1,it[in].y,it[in].x+10,it[in].y,str,base);
			create_edemonball(0,it[in].x,it[in].y-1,it[in].x,it[in].y-10,str,base);
			create_edemonball(0,it[in].x-1,it[in].y,it[in].x-10,it[in].y,str,base);
			break;
	}
}

void caligar_key_assembly(int in,int cn)
{
	int in2,sp1,sp2,in3;

	if (!cn) return;

	if (!(in2=ch[cn].citem) || it[in2].ID!=IID_CALIGARPALACEKEYPART) {
		log_char(cn,LOG_SYSTEM,0,"Nothing happens.");
		return;
	}

	sp1=min(it[in].sprite,it[in2].sprite);
	sp2=max(it[in].sprite,it[in2].sprite);

	if (sp1==13414 && sp2==13415) {
		destroy_item(in2);
		ch[cn].citem=0;
		it[in].sprite=13421;
		ch[cn].flags|=CF_ITEMS;
		return;
	}
	if (sp1==13415 && sp2==13416) {
		destroy_item(in2);
		ch[cn].citem=0;
		it[in].sprite=13420;
		ch[cn].flags|=CF_ITEMS;
		return;
	}
	if (sp1==13414 && sp2==13420 && (in3=create_item("caligar_palace_chest_key"))) {
		remove_item_char(in);
		destroy_item(in);
		destroy_item(in2);
		ch[cn].citem=in3;
		it[in3].carried=cn;
		ch[cn].flags|=CF_ITEMS;
		return;
	}
	if (sp1==13416 && sp2==13421 && (in3=create_item("caligar_palace_chest_key"))) {
		remove_item_char(in);
		destroy_item(in);
		destroy_item(in2);
		ch[cn].citem=in3;
		it[in3].carried=cn;
		ch[cn].flags|=CF_ITEMS;
		return;
	}

	log_char(cn,LOG_SYSTEM,0,"This does not seem to fit.");
	return;
}

void caligar_burn_char(int cn)
{
	int fn,n;

	// dont add effect twice
	for (n=0; n<4; n++) {
		if (!(fn=ch[cn].ef[n])) continue;
		if (ef[fn].type!=EF_BURN) continue;
		return;
	}
	
	create_show_effect(EF_BURN,cn,ticker,ticker+TICKS*60,250,1);
	hurt(cn,20*POWERSCALE,0,1,50,75);
}

void caligar_extinguish(int in,int cn)
{
        int fn,n;

	if (!cn) return;

	// is he on fire?
	for (n=0; n<4; n++) {
		if (!(fn=ch[cn].ef[n])) continue;
		if (ef[fn].type!=EF_BURN) continue;
		break;
	}
	if (n==4) {
		log_char(cn,LOG_SYSTEM,0,"Ahh. Sweet and refreshing.");
		return;
	}

	remove_effect(fn);
	free_effect(fn);

	log_char(cn,LOG_SYSTEM,0,"You extinguish the flames.");
}

void caligar_flamethrow(int in,int cn)
{
	int fire,dir,co,x,y,m;

	if (cn) return;

	fire=it[in].drdata[0];

	if (fire) {
                it[in].drdata[0]--;

		if (!it[in].drdata[2]) {
			it[in].sprite++;
			it[in].drdata[2]=1;

			remove_item_light(in);
			it[in].mod_index[4]=V_LIGHT;
			it[in].mod_value[4]=250;
			add_item_light(in);
		}

		dir=it[in].drdata[1];
		dx2offset(dir,&x,&y,NULL);

		x=it[in].x+x; y=it[in].y+y;
		if (x>0 && x<MAXMAP && y>0 && y<MAXMAP) {
			m=x+y*MAXMAP;
			if ((co=map[m].ch)) caligar_burn_char(co);
		}
		
		dx2offset(dir,&x,&y,NULL);
		x=it[in].x+x*2; y=it[in].y+y*2;
		if (x>0 && x<MAXMAP && y>0 && y<MAXMAP) {
			m=x+y*MAXMAP;
			if ((co=map[m].ch)) caligar_burn_char(co);						
		}
		
                call_item(it[in].driver,in,0,ticker+1);
	} else {
		it[in].sprite--;
		it[in].drdata[0]=TICKS;
		it[in].drdata[2]=0;
		
		remove_item_light(in);
		it[in].mod_index[4]=0;
		it[in].mod_value[4]=0;
		add_item_light(in);

		if (it[in].drdata[3]) call_item(it[in].driver,in,0,ticker+TICKS*it[in].drdata[3]);
	}
}


int caligar_item(int in,int cn)
{
	switch(it[in].drdata[0]) {
		case 1:		caligar_training(in,cn); break;
		case 2:		caligar_weight(in,cn); break;
		case 3:		return caligar_weight_door(in,cn);
		case 4:		caligar_weight(in,cn); break;
		case 5:		caligar_gun(in,cn,1); break;
		case 6:		caligar_gun(in,cn,2); break;
		case 7:		caligar_gun(in,cn,3); break;
		case 8:		caligar_gun(in,cn,4); break;
		case 9:		caligar_gun(in,cn,5); break;
		case 10: 	caligar_key_assembly(in,cn); break;
		case 11:	caligar_extinguish(in,cn); break;
		case 12:	return caligar_skelly_door(in,cn);
	}
	return 1;
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_CALIGARGUARD:	guard_driver(cn,ret,lastact); return 1;
		case CDR_CALIGARGUARD2:	guard2_driver(cn,ret,lastact); return 1;
		case CDR_CALIGARGLORI:	glori_driver(cn,ret,lastact); return 1;
		case CDR_CALIGARARQUIN:	arquin_driver(cn,ret,lastact); return 1;
		case CDR_CALIGARSMITH:	smith_driver(cn,ret,lastact); return 1;
		case CDR_CALIGARHOMDEN:	homden_driver(cn,ret,lastact); return 1;
		case CDR_CALIGARSKELLY:	skelly_driver(cn,ret,lastact); return 1;

                default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_CALIGAR:	return caligar_item(in,cn);
		case IDR_CALIGARFLAME:	caligar_flamethrow(in,cn); return 1;
                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_CALIGARGUARD2:	return 1;
		case CDR_CALIGARSKELLY:	skelly_dead_driver(cn,co); return 1;

                default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_CALIGARGUARD2:	return 1;
		case CDR_CALIGARSKELLY:	return 1;

		default:		return 0;
	}
}






