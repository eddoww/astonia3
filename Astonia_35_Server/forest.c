/*
 * $Id: forest.c,v 1.8 2007/08/21 22:04:01 devel Exp $
 *
 * $Log: forest.c,v $
 * Revision 1.8  2007/08/21 22:04:01  devel
 * spelling errors
 * talking to william after doing imp bear hunt will no longer re-open the first forest quest (22)
 *
 * Revision 1.7  2007/08/13 18:52:01  devel
 * fixed some warnings
 *
 * Revision 1.6  2007/08/09 11:12:58  devel
 * statistics
 *
 * Revision 1.5  2007/07/11 12:37:52  devel
 * made imp understand new healing system
 *
 * Revision 1.4  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.3  2006/10/06 11:21:19  devel
 * fixed bug in william driver
 *
 * Revision 1.2  2006/09/25 14:07:57  devel
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
#include "consistency.h"
#include "questlog.h"
#include "statistics.h"

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
	{{"imp",NULL},"A nice little guy. He's got a peculiar sense of humor, but he's very helpful.",0},
	{{"repeat",NULL},NULL,2}
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

struct imp_driver_data
{
        int last_talk;
	int current_victim;
	int mode;
	int backtime;
};

// note: the ppd is borrowed from area3 - the missions interact...
void imp_driver(int cn,int ret,int lastact)
{
	struct imp_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_IMPDRIVER,sizeof(struct imp_driver_data));
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
			if (char_dist(cn,co)>20) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));

                        if (ppd) {
				switch(ppd->imp_state) {
					case 0:		say(cn,"A human. Now this is interesting. What could a human be doing here?");
							ppd->imp_state++; didsay=1;
							break;
					case 1:		say(cn,"Should I tell him about the treasure? Ah, let's wait and see.");
							ppd->imp_state++; didsay=1;
							break;
					case 2:		break;
					case 3:		say(cn,"Nicely done, %s. Thou might not be bright, but at least thou knowest how to fight.",ch[co].name);
							ppd->imp_state++; didsay=1;
							ppd->imp_kills=0;
							questlog_done(co,22);
                                                        break;
					case 4:		say(cn,"Now do be a dear and run back to William.");
							ppd->imp_state++; didsay=1;
							ppd->william_state=3;
							break;
					case 5:		if (questlog_isdone(co,23)) { ppd->imp_state=11; break; }
							break;
					case 6:		say(cn,"Hullo human! There is more to be done here I know thine worth. Find him who is old and in need of thy help.");
							ppd->imp_state++; didsay=1;
							break;
					case 7:		if (ppd->hermit_state>3) ppd->imp_state++; // fall thru...
							else break;
					case 8:		if (ppd->hermit_state==4 && (!(in=has_item(co,IID_HARDKILL)) || it[in].drdata[37]<38)) {
								if (in) say(cn,"Listen, human, for this might save thy life: The spider queen is beyond the strength of thine holy weapon. Thou needst find another stone circle. Find the skeleton ruin and go eastward.");
								else say(cn,"Listen, human, for this might save thy life: Thou needst a holy weapon, otherwise thy task will remain unfulfilled.");
								didsay=1;
							}
							ppd->imp_state++;
							break;
					case 9:		if (ppd->hermit_state>4) ppd->imp_state++; // fall thru...
							else break;
					case 10:	say(cn,"Thou art truly worthy, dear human called %s. Now, I shall tell thee how to find the treasure. Find the southernmost stone where the single skeleton lurks. Dig a hole and thou shalt find the key to a treasure.",ch[co].name);
							ppd->imp_state++; didsay=1;
							break;
					case 11:	break;
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					dat->mode=1;
					dat->backtime=ticker+TICKS*8;
				}
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

		if (msg->type==NT_SEEHIT) {
			co=msg->dat2;			
			if (co && (ch[co].flags&CF_PLAYER) && ch[co].hp<ch[co].value[1][V_HP]*POWERSCALE/2 && !RANDOM(4) && !has_spell(co,IDR_HEAL)) {
				if (ch[cn].flags&CF_INVISIBLE) {
					ch[cn].flags&=~CF_INVISIBLE;
					set_sector(ch[cn].x,ch[cn].y);
				}
				dat->mode=1;
				dat->backtime=ticker+TICKS*3;
				say(cn,"Ooh. Don't die, dear human.");
                                if (do_heal(cn,co)) { remove_message(cn,msg); return; }
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (dat->mode==1 && ticker>dat->backtime) dat->mode=0;

	if (dat->mode==0) {
		if (!(ch[cn].flags&CF_INVISIBLE)) {
			ch[cn].flags|=CF_INVISIBLE;
			set_sector(ch[cn].x,ch[cn].y);
		}
	} else if (dat->mode==1) {
		if ((ch[cn].flags&CF_INVISIBLE)) {
			ch[cn].flags&=~CF_INVISIBLE;
			set_sector(ch[cn].x,ch[cn].y);
		}
	}	

	if (talkdir) turn(cn,talkdir);

	if (spell_self_driver(cn)) return;

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

struct william_driver_data
{
        int last_talk;
	int current_victim;
};

// note: the ppd is borrowed from area3 - the missions interact...
void william_driver(int cn,int ret,int lastact)
{
	struct imp_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_WILLIAMDRIVER,sizeof(struct william_driver_data));
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
                        ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));

                        if (ppd) {
				switch(ppd->william_state) {
					case 0:		say(cn,"Greetings, %s. So nice of thee to visit. I am called %s.",ch[co].name,ch[cn].name);
							if (questlog_isdone(co,22)) { ppd->william_state=3; break; }
							questlog_open(co,22);
							ppd->william_state++; didsay=1;
							break;
					case 1:		say(cn,"The °c4imp°c0 asked me to tell you to go east and then northeast and hunt some bears.");
							ppd->william_state++; didsay=1;
							break;
					case 2:		break;	// waiting for imp
					case 3:		if (questlog_isdone(co,23)) { ppd->william_state=7; break; }
							say(cn,"Ah, hello %s. The °c4imp°c0 told me thou hast done him a favor. That's nice of thee.",ch[co].name);
							questlog_open(co,23);
							ppd->william_state++; didsay=1;
							break;
					case 4:		say(cn,"Now if I may be so bold as to make a request of my own? It might sound strange to thee, friend, but I can make a nice stew from praying mantisses. I'd pay thee handsomely if thou couldst hunt one of them down and bring it to me.");
							ppd->william_state++; didsay=1;
							break;
					case 5:		say(cn,"They live in the northern corner of the forest, close to a large clearing. Thou needst go east, then north and then north-west to get there.");
							ppd->william_state++; didsay=1;
							break;
					case 6:		break;	// waiting for mantiss
					case 7:		break;	// quest done
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
				case 2:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
                                                if (ppd && ppd->william_state<=2) { dat->last_talk=0; ppd->william_state=0; }
						if (ppd && ppd->william_state>=3 && ppd->william_state<=6) { dat->last_talk=0; ppd->william_state=3; }
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
				ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));

				if (it[in].ID==IID_AREA16_MANTIS && ppd && ppd->william_state==6) {
					int tmp;
                                        destroy_item(in);
					ch[cn].citem=0;

					ppd->william_state=7;
					ppd->imp_state=6;
					say(cn,"Ah. I thank thee, %s. This will make a nice stew. Here, take these 20 gold coins.",ch[co].name);
					tmp=questlog_done(co,23);
                                        destroy_item_byID(co,IID_AREA16_MANTIS);
					if (tmp==1) {
						ch[co].gold+=2000; stats_update(co,0,2000);
						ch[co].flags|=CF_ITEMS;
					}
				} else {
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
		if (hour>=20 || hour<6) {
			if (secure_move_driver(cn,176,120,DX_RIGHT,ret,lastact)) return;
		} else {
			if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
		}
	}
	
	do_idle(cn,TICKS);
}

struct hermit_driver_data
{
        int last_talk;
	int current_victim;
};

// note: the ppd is borrowed from area3 - the missions interact...
void hermit_driver(int cn,int ret,int lastact)
{
	struct imp_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_HERMITDRIVER,sizeof(struct hermit_driver_data));
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
                        ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));

                        if (ppd) {
				switch(ppd->hermit_state) {
					case 0:		say(cn,"My greetings to thee, %s. 'Tis most fortunate to see such a formidable hero as thyself. Be aware that I am in dire need of thy help.",ch[co].name);
							questlog_open(co,24);
							ppd->hermit_state++; didsay=1;
							break;
					case 1:		say(cn,"Not long ago, some foul demons invaded this once so peaceful forest. They did not linger for long, but after they left the spiders in the western part of the forest started to grow and grow and grow.");
							ppd->hermit_state++; didsay=1;
							break;
					case 2:		say(cn,"They did not only grow in size, but also in aggressiveness. Before, they used to feed on other insects, but now they lust human blood. Therefore I lay this quest upon thee, %s, to go to their lair and slay their queen.",ch[co].name);
							ppd->hermit_state++; didsay=1;
							break;
                                        case 3:		say(cn,"Be wary, and prepare thyself well, for the queen can only be slain by a holy weapon of sufficient strength. Now go, %s, and do what needs be done. Thou canst reach their lair by going south and turning north-west at the old ruin.",ch[co].name);
							ppd->hermit_state++; didsay=1;
							break;
					case 4:         break;
					case 5:		say(cn,"I thank thee, %s, for thy brave deed. Forever shall I keep the memory of thy courage in my heart.",ch[co].name);
							ppd->hermit_state++; didsay=1;
							questlog_done(co,24);
							break;
					case 6:		say(cn,"I know not why these demons have come, nor whence they came from. But I ask thee, %s, fight them whereever they show their ugly hides.",ch[co].name);
							ppd->hermit_state++; didsay=1;
							break;
					case 7:		break;
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
				case 2:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
                                                if (ppd && ppd->hermit_state<=4) { dat->last_talk=0; ppd->hermit_state=0; }
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
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

void monster_dead(int cn,int co)
{
	struct area3_ppd *ppd;
	int bit=0,in;

	if (!co) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

	if ((ch[cn].sprite==306) && (ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd))) && ppd->imp_state==2) {
		ppd->imp_kills++;
                if (ppd->imp_kills>20) ppd->imp_state=3;
	}

	if ((ch[cn].flags&CF_HARDKILL) && (ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd))) && ppd->hermit_state==4) {
		ppd->hermit_state=5;
		log_char(co,LOG_SYSTEM,0,"Thou hast slain the spider queen.");
	}
	
	if (ch[co].x>=182 && ch[co].y>=185 && ch[co].x<=192 && ch[co].y<=192) bit=8;

        if (hour==0 && bit && (in=ch[co].item[WN_RHAND]) && it[in].driver==0 && !(it[in].drdata[36]&bit)) {
		it[in].ID=IID_HARDKILL;
		it[in].drdata[37]+=6;
		it[in].drdata[36]|=bit;
		it[in].flags|=IF_QUEST;
		log_char(co,LOG_SYSTEM,0,"Your %s starts to glow.",it[in].name);
	}
}

void chest(int in,int cn)
{
	int in2;
	struct area3_ppd *ppd;
	
	if (!cn) return;

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	if (it[in].drdata[0]==0) {	
		if (!has_item(cn,IID_AREA16_ROBBERKEY)) {
			log_char(cn,LOG_SYSTEM,0,"The chest is locked and you don't have the right key.");
			return;
		}
	
		ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
		if (!ppd || (ppd->imp_flags&1) || !(in2=create_money_item(9733))) {
			log_char(cn,LOG_SYSTEM,0,"The chest is empty.");
			return;
		}
		ppd->imp_flags|=1;
	} else {
                if (!has_item(cn,IID_AREA16_SKELLYKEY)) {
			log_char(cn,LOG_SYSTEM,0,"The chest is locked and you don't have the right key.");
			return;
		}
	
		ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
		if (!ppd || (ppd->imp_flags&2) || !(in2=create_money_item(17587))) {
			log_char(cn,LOG_SYSTEM,0,"The chest is empty.");
			return;
		}
		ppd->imp_flags|=2;
	}
	
	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from forest chest");
	ch[cn].citem=in2;
	it[in2].carried=cn;
	ch[cn].flags|=CF_ITEMS;

	log_char(cn,LOG_SYSTEM,0,"You found a nice sum of money!");
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_FORESTIMP:	imp_driver(cn,ret,lastact); return 1;
		case CDR_FORESTMONSTER:	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;
		case CDR_FORESTWILLIAM:	william_driver(cn,ret,lastact); return 1;
		case CDR_FORESTHERMIT:	hermit_driver(cn,ret,lastact); return 1;
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {		
                case IDR_FORESTCHEST:	chest(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_FORESTIMP:	return 1;
		case CDR_FORESTMONSTER:	monster_dead(cn,co); return 1;
                default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_FORESTIMP:	return 1;
		case CDR_FORESTMONSTER:	return 1;
                default:		return 0;
	}
}







