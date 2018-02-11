/*
 * $Id: gwendylon.c,v 1.13 2007/08/21 22:04:50 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: gwendylon.c,v $
 * Revision 1.13  2007/08/21 22:04:50  devel
 * B and C mushroom need a min elvel now
 *
 * Revision 1.12  2007/08/13 18:52:01  devel
 * fixed some warnings
 *
 * Revision 1.11  2007/08/09 11:13:08  devel
 * sttaistics
 *
 * Revision 1.10  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.9  2007/07/01 13:26:55  devel
 * added caligar teleport
 *
 * Revision 1.8  2007/02/28 10:42:34  devel
 * made yoakin's quest appear later in the questlog
 *
 * Revision 1.7  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.6  2006/12/08 10:34:46  devel
 * fixed bug in logain driver
 *
 * Revision 1.5  2006/09/21 11:23:48  devel
 * more questlog fixes
 *
 * Revision 1.4  2006/09/14 09:55:22  devel
 * added questlog
 *
 * Revision 1.3  2006/05/29 15:28:28  ssim
 * made gwendylon destroy leftover items in citem
 *
 * Revision 1.2  2005/12/04 17:25:40  ssim
 * increased EXP for werewolf quest
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
	{{"repeat",NULL},NULL,2},
	{{"restart",NULL},NULL,2},
	{{"please","repeat",NULL},NULL,2},
	{{"please","restart",NULL},NULL,2},
        {{"advice",NULL},NULL,3},
	{{"buy","advice",NULL},NULL,4},
	{{"promise",NULL},NULL,9},
	{{"word",NULL},NULL,9},
	{{"oath",NULL},NULL,9}
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

struct gwendylon_driver_data
{
        int last_talk;
	int current_victim;
	int nighttime;
	int giveto;
};

void gwendylon_driver(int cn,int ret,int lastact)
{
	struct gwendylon_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_GWENDYLONDRIVER,sizeof(struct gwendylon_driver_data));
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
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
                                switch(ppd->gwendy_state) {
					case 0:		quiet_say(cn,"Welcome %s! I am %s the Mage.",ch[co].name,ch[cn].name);
							questlog_open(co,1);
							ppd->gwendy_state=1; didsay=1;
							break;
					case 1:		quiet_say(cn,"South-east of my tower lies an old ruin. 'Tis inhabitated by skeletons, raised by some evil magic.");
							ppd->gwendy_state=2; didsay=1;
							break;
					case 2:		quiet_say(cn,"I am trying to understand this magic. But I am too old to travel there, so I couldst use thine help, %s. Wouldst thou go thither and look for magical items?",ch[co].name);
							ppd->gwendy_state=3; didsay=1;
							break;
					case 3:		quiet_say(cn,"It could be anything: An enhanced bone, an ancient jewel, even a magical spoon of doom.");
							ppd->gwendy_state=4; didsay=1;
							break;
					case 4:		quiet_say(cn,"If thou couldst find it and bring it to me, I would reward thee.");
							ppd->gwendy_state=5; didsay=1;
							break;
					case 5:		if (realtime-ppd->gwendy_seen_timer>60) {
								quiet_say(cn,"Be greeted, %s! Didst thou find anything magical in the skeleton's ruin? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TUTORIAL,1,co);
								didsay=1;
							}
                                                        break;
					case 6:		if (questlog_isdone(co,2)) { ppd->gwendy_state=10; break; }
							quiet_say(cn,"I have analyzed the item thou brought me. It seems there are more places with skeletons close by.");
							questlog_open(co,2);
							ppd->gwendy_state++; didsay=1;
							break;
					case 7:         quiet_say(cn,"Maybe the villagers know something about it. Some of them go hunting in the forest, they might have seen something. Thou might inquire in the tavern.");
							ppd->gwendy_state++; didsay=1;
							break;		
					case 8:		quiet_say(cn,"Somewhere in that place, there must be another magical item. Couldst thou bring me that one as well? I would double thine reward.");
							ppd->gwendy_state++; didsay=1;
                                                        break;
					case 9:		if (realtime-ppd->gwendy_seen_timer>60) {					
								quiet_say(cn,"Be greeted, %s! Didst thou find anything magical in the other skeleton place? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
							}
                                                        break;
					case 10:	if (questlog_isdone(co,3)) { ppd->gwendy_state=13; break; }
							quiet_say(cn,"There must be another of these skulls, adorned with a green jewel. But I have no idea where to look for it. If I had it, I could use all three skulls to locate their maker.");
							questlog_open(co,3);
							ppd->gwendy_state++; didsay=1;
                                                        break;
					case 11:	quiet_say(cn,"Oh, if thou couldst find it, %s, I would be very grateful. Pray thee %s, locate this skull and bring it to me.",ch[co].name,ch[co].name);
							ppd->gwendy_state++; didsay=1;
                                                        break;
					case 12:	if (realtime-ppd->gwendy_seen_timer>60) {					
								quiet_say(cn,"Ah, %s! Didst thou find the skull? It really is of the utmost importance. Or dost thou want me to °c4repeat°c0  mine offer?",ch[co].name);
								didsay=1;
							}
                                                        break;
					case 13:	if (questlog_isdone(co,4)) { ppd->gwendy_state=17; break; }
							quiet_say(cn,"This is most disturbing. My analysis shows that the skulls were created in this very tower.");
							questlog_open(co,4);
							ppd->gwendy_state++; didsay=1;
							break;
					case 14:	quiet_say(cn,"But I know every room in here. How can this be? Thou hast been most clever so far, %s. If thou couldst search the tower?",ch[co].name);
							ppd->gwendy_state++; didsay=1;
							break;
					case 15:	quiet_say(cn,"If thou dost find that foul magician, make certain thou killest him. I am certain he does have a fourth skull. Please bring it to me so I can destroy it.");
							ppd->gwendy_state++; didsay=1;
							break;
					case 16:	if (realtime-ppd->gwendy_seen_timer>60) {
								quiet_say(cn,"Ah, %s! I am most concerned. Didst thou find anything? Or dost thou want me to °c4repeat°c0  what I said about it?",ch[co].name);
								didsay=1;
							}
							break;
					case 17:	quiet_say(cn,"I do not have any more work for thee, %s.",ch[co].name);
							ppd->gwendy_state++; didsay=1;
							break;
                                        case 18:	if (realtime-ppd->gwendy_seen_timer>60) {
								quiet_say(cn,"Nice to see you, %s.",ch[co].name);
								if (may_add_spell(co,IDR_BLESS) && do_bless(cn,co)) {
									ppd->gwendy_seen_timer=realtime;
									return;
								}
								didsay=1;
							}
                                                        break;
				}
				ppd->gwendy_seen_timer=realtime;
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
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd && ppd->gwendy_state<=5) { dat->last_talk=0; ppd->gwendy_state=1; }
						if (ppd && ppd->gwendy_state>=6 && ppd->gwendy_state<=9) { dat->last_talk=0; ppd->gwendy_state=6; }
						if (ppd && ppd->gwendy_state>=10 && ppd->gwendy_state<=12) { dat->last_talk=0; ppd->gwendy_state=10; }
						if (ppd && ppd->gwendy_state>=13 && ppd->gwendy_state<=16) { dat->last_talk=0; ppd->gwendy_state=13; }
						if (ppd && ppd->gwendy_state>=17) { dat->last_talk=0; ppd->gwendy_state=17; }
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
				
				ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                                if (it[in].ID==IID_AREA1_SKELSKULL && ppd && ppd->gwendy_state<=5) {
					int tmp;

					quiet_say(cn,"Ahh, yes, that might be the thing I was looking for. Thank thee, %s.",ch[co].name);
					tmp=questlog_done(co,1);
					destroy_item_byID(co,IID_AREA1_SKELSKULL);
					destroy_item_byID(co,IID_AREA1_SKELKEY1);
					destroy_item_byID(co,IID_AREA1_SKELKEY2);
					destroy_item_byID(co,IID_AREA1_SKELKEY3);
                                        ppd->gwendy_state=6;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_SKULL1);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else if (it[in].ID==IID_AREA1_WOODSKULL && ppd && ppd->gwendy_state>=6 && ppd->gwendy_state<=9) {
					int tmp;
					
					quiet_say(cn,"Ahh, yes, this is the thing I was looking for. Thank thee, %s.",ch[co].name);
					tmp=questlog_done(co,2);
					destroy_item_byID(co,IID_AREA1_WOODSKULL);
					destroy_item_byID(co,IID_AREA1_WOODKEY);
                                        ppd->gwendy_state=10;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_SKULL2);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else if (it[in].ID==IID_AREA1_MAGESKULL && ppd && ppd->gwendy_state>=10 && ppd->gwendy_state<=12) {
					int tmp;

					quiet_say(cn,"Ahh, yes, this is the third skull. Thank thee, %s, I appreciate thine efforts.",ch[co].name);
					tmp=questlog_done(co,3);
					destroy_item_byID(co,IID_AREA1_MAGESKULL);
                                        ppd->gwendy_state=13;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_SKULL3);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else if (it[in].ID==IID_AREA1_WARLOCKSKULL && ppd && ppd->gwendy_state>=13 && ppd->gwendy_state<=16) {
					int tmp;

					quiet_say(cn,"This is the last skull! The evil mage must be dead then. I thank thee, %s!",ch[co].name);
					tmp=questlog_done(co,4);
                                        ppd->gwendy_state=17;
					destroy_item_byID(co,IID_AREA1_WARLOCKSKULL);
					destroy_item_byID(co,IID_AREA1_WARLOCKKEY);

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_SKULL4);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else if (it[in].ID==IID_CALIGARLETTER) {
                                        quiet_say(cn,"Hmm, I see. Well, I can teleport you to the area but I am uncertain of what will be there waiting for you. Be prepared adventurer. I would not trust those mages as far as I could throw them!");
					log_char(co,LOG_SYSTEM,0,"While you are still trying to figure out how far Gwendylon might be able to throw those mages he quickly mutters a spell and teleports you.");
					
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (!change_area(co,36,240,10)) quiet_say(cn,"Uh-Oh. There seems to be a rift in the space-time continuum. Please come again later so we can try again.");					
				} else {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

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
	if (ch[cn].citem) {
		elog("gwendylon about to destroy item %d %s",ch[cn].citem,it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;
	}

        do_idle(cn,TICKS);
}

//-----------------------

struct yoakin_driver_data
{
        int last_talk;
	int current_victim;
	int nighttime;
};	

void yoakin_driver(int cn,int ret,int lastact)
{
	struct yoakin_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_YOAKINDRIVER,sizeof(struct yoakin_driver_data));
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

			//quiet_say(cn,"diff=%d, victim=%d (%s)",ticker-dat->last_talk,dat->current_victim,ch[dat->current_victim].name);

			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
				if (realtime-ppd->yoakin_seen_timer>120 && ppd->yoakin_state && ppd->yoakin_state<4) {
					ppd->yoakin_state=1;
				}
                                switch(ppd->yoakin_state) {
					case 0:		quiet_say(cn,"Hail %s! I am %s, the hunter.",ch[co].name,ch[cn].name);
                                                        ppd->yoakin_state=1; didsay=1;
							break;
					case 1:		quiet_say(cn,"Be careful in the forest around the village, %s. There are wolves and bears about which can be very aggressive.",ch[co].name);
							ppd->yoakin_state=2; didsay=1;
							break;
					case 2:		if (ppd->logain_state<6) break;		// dont give out quest before mad knights is done
							quiet_say(cn,"Greetings again, %s. Lately, there have been reports of a huge bear on the path to the city.",ch[co].name);
							questlog_open(co,5);
                                                        ppd->yoakin_state=3; didsay=1;
							break;
					case 3:		quiet_say(cn,"This bear has been killing several travellers, and I put a price on its head. So if thou happen to kill it, bring me its teeth as proof.");
							ppd->yoakin_state=4; didsay=1;
							break;
					case 4:		if (realtime-ppd->yoakin_seen_timer>60) {					
								quiet_say(cn,"Hail, %s! Didst thou find that big bear? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
							}
                                                        break;
					case 5:		break;
				}
				ppd->yoakin_seen_timer=realtime;

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
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd && ppd->yoakin_state<=4) { dat->last_talk=0; ppd->yoakin_state=2; }
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

                                ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                                if (it[in].ID==IID_AREA1_BIGBEAR_TOOTH && ppd && ppd->yoakin_state<=4) {
					int tmp;
					quiet_say(cn,"Thank thee, %s. Travelling in the forest will be safer now.",ch[co].name);
					tmp=questlog_done(co,5);
					destroy_item_byID(co,IID_AREA1_BIGBEAR_TOOTH);
                                        ppd->yoakin_state=5;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_BEARTOOTH);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else if (it[in].ID==IID_SHRIKE_TALISMAN && ppd && ppd->shrike_state==0) {
					emote(cn,"turns deadly pale and starts to tremble");
					quiet_say(cn,"I... I thank thee, %s. I'd have never thought... Thank thee!",ch[co].name);

					if (ppd->shrike_fails) {
						quiet_say(cn,"And I forgive thee trying to kill me. My wounds were almost fatal, but I survived.");
						give_exp(co,min(level_value(ch[co].level)/5,143462));	// 50000
					} else give_exp(co,min(level_value(ch[co].level)/5,286925));	// 100000
                                        ppd->shrike_state=1;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {				
                                        quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                        ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].citem) {
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;
	}

	if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;	
	}

        do_idle(cn,TICKS);
}

//-----------------------

struct terion_driver_data
{
        int last_talk;
	int last_walk;
	int pos;
	int current_victim;
};	

void terion_driver(int cn,int ret,int lastact)
{
	struct terion_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_TERIONDRIVER,sizeof(struct terion_driver_data));
	if (!dat) return;	// oops...

	// loop through our messages once
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
		if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;
	}

        // loop through our messages twice
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

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
                                switch(ppd->terion_state) {
					case 0:		if (ppd->gwendy_state<6) break;	// dont offer skelly stories before skull 2
							if (ppd->gwendy_state>9) {	// advance to second hint if player solved skull 2 already
								ppd->terion_state=4;
								break;
							}
							quiet_say(cn,"Be greeted, %s! My name is %s.",ch[co].name,ch[cn].name);
							ppd->terion_state=1; didsay=1;
                                                        break;
					case 1:		quiet_say(cn,"I have heard some stories about skeletons walking around in the western part of the forest lately. They were close to that small path which leads west directly behind the tavern.");
							ppd->terion_state=2; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"Some of the lads from the village went looking for them, but had no luck. Or they were lucky, depends on the way thou lookst at it I guess.");
							notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TERION,cn,1);
							ppd->terion_state=3; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"Anyway. They found nothing. Guess that is because they dare not go late at night.");
							ppd->terion_state=4; didsay=1;
                                                        break;
					
					
					case 4:		if (ppd->gwendy_state>=10 && ppd->gwendy_state<=12) {
								quiet_say(cn,"Be greeted again, %s. I hope this day finds thee well.",ch[co].name);
								ppd->terion_state++; didsay=1;
							}
							break;
					case 5:		quiet_say(cn,"I've been thinking about the skeletons, and the skulls Gwendylon is researching. It seems these skeletons are always seen near old ruins. And I remembered that Yoakin the Hunter once told me that his house was built on top of an old ruin.");
							notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TERION,cn,3);
							ppd->terion_state++; didsay=1;
							break;

					case 6:		if (ppd->gwendy_state>=13) {
								quiet_say(cn,"Ah, %s. 'Tis good to see there are brave %s about who will fight the evil which has been invading our lives lately.",ch[co].name,(ch[co].flags&CF_MALE) ? "men" : "women");
								ppd->terion_state++; didsay=1;
							}
							break;
					case 7:		quiet_say(cn,"Ever since the dark hordes attacked Aston, things have been going downhill. Some years ago, we frequently had visitors from Aston and beyond. But today no one dares to travel unless he must.");
							ppd->terion_state++; didsay=1;
                                                        break;
					case 8:		quiet_say(cn,"And those skeletons all around... We live in hard times.");
							ppd->terion_state++; didsay=1;
                                                        break;

					case 9:		if (ppd->reskin_state>=9) {
								quiet_say(cn,"Oh, what has become of this world? The two schools gone mad beyond cure, skeletons everywhere. And I've heard rumors that things all over the land are the same.");
								ppd->terion_state++; didsay=1;
							}
							break;
					case 10:	quiet_say(cn,"Undeads and strange beasts and even demons have been seen in Aston. Didst thou know that?");
							ppd->terion_state++; didsay=1;
                                                        break;
					case 11:        quiet_say(cn,"Some years ago those monsters attacked our capital and killed our emperor. Most of his honor guard, the Seyan'Du, died in his defense, but to no avail. With the emperor dead, and the imperial palace in ruins, there was no one to organize our defenses.");
							ppd->terion_state++; didsay=1;
                                                        break;
					case 12:	quiet_say(cn,"And so the dark hordes are spreading all over the land.");
							ppd->terion_state++; didsay=1;
                                                        break;
					case 13:        quiet_say(cn,"Hey! Reskin! Art thou certain that thou dost not have any beer? I couldst use a drink now.");
							notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TERION,cn,5);
							ppd->terion_state++; didsay=1;
                                                        break;
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DIDSAY,cn,0);
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:         if (ppd && ppd->terion_state<=4) { dat->last_talk=0; ppd->terion_state=0; }
						if (ppd && ppd->terion_state>=5 && ppd->terion_state<=6) { dat->last_talk=0; ppd->terion_state=4; }
						if (ppd && ppd->terion_state>=7 && ppd->terion_state<=9) { dat->last_talk=0; ppd->terion_state=6; }
						if (ppd && ppd->terion_state>=10 && ppd->terion_state<=14) { dat->last_talk=0; ppd->terion_state=9; }
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
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

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

//-----------------------

int has_empty_inventory(int cn)
{
	int n;

	for (n=0; n<12; n++)
		if (ch[cn].item[n]) return 0;

	for (n=30; n<INVENTORYSIZE; n++)
		if (ch[cn].item[n]) return 0;

	return 1;	
}

struct james_driver_data
{
        int last_talk;
	int current_victim;
	int nighttime;
};

int james_raisehint(int cn,int doraise);
void james_create_eq(int cn);

void james_driver(int cn,int ret,int lastact)
{
	struct james_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_JAMESDRIVER,sizeof(struct james_driver_data));
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
			if (char_dist(cn,co)>15) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
				if (has_empty_inventory(co) && !(ppd->flags&AF1_STORAGE_HINT)) {
					quiet_say(cn,"Shouldst thou need new equipment, %s, try the chests in the western corner. If thou hast not used them yet, they should contain all thou needst.",ch[co].name);
					ppd->flags|=AF1_STORAGE_HINT; didsay=1;
				}
                                switch(ppd->james_state) {
					case 0:         if (ppd->lydia_state>=6) { ppd->james_state=3; break; }
							quiet_say(cn,"Ah, hello there, %s. I am %s. Couldst thou do me a favor? I went to a party with Lydia last night, and I must admit that I drank too much.",ch[co].name,ch[cn].name);
							ppd->james_state++; didsay=1;
							questlog_open(co,0);
							break;
					case 1:		quiet_say(cn,"I cannot remember how I came back here, and if Lydia got home safely. So couldst thou visit Lydia and give her my regards and inquire about her health?");
							ppd->james_state++; didsay=1;
							break;
					case 2:		quiet_say(cn,"She lives with her father, Gwendylon the Mage. Thou canst find her in the mage's tower, north of here, %s.",ch[co].name);
							ppd->james_state++; didsay=1;							
							break;
					case 3:		if (ppd->lydia_state<6) break;
							quiet_say(cn,"Ah, %s. I am glad that thou could help Lydia.",ch[co].name);
							ppd->james_state++; didsay=1;
							break;
                                        case 4:		quiet_say(cn,"If you ever need °c4advice°c0 on how to raise your character, I'd be happy to help you - for a small fee.");
							ppd->james_state++; didsay=1;
							break;
					case 5:		break;
					
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
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
                                                if (ppd && ppd->james_state>=0 && ppd->james_state<=3) { ppd->james_state=0; dat->last_talk=0; }
						break;
				case 3:		if (ch[co].level>70) quiet_say(cn,"I'm afraid I cannot help thee, %s. Thou art much wiser than I am.",ch[co].name);
						else quiet_say(cn,"I'll help thee for the small fee of %.2fG, %s. Say °c4buy advice°c0 if thou wantst it.",ch[co].level*ch[co].level*ch[co].level/100.0,ch[co].name);
						break;
				case 4:		if (ch[co].level>70) quiet_say(cn,"I'm afraid I cannot help thee, %s. Thou art much wiser than I am.",ch[co].name);
						else if (take_money(co,ch[co].level*ch[co].level*ch[co].level)) james_raisehint(co,0);
						else quiet_say(cn,"Thou dost not have enough money, %s.",ch[co].name);
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
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHTDOWN,ret,lastact)) return;		
	}

        do_idle(cn,TICKS);
}

struct nook_driver_data
{
        int last_talk;
	int current_victim;
};	

void nook_driver(int cn,int ret,int lastact)
{
	struct nook_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_NOOKDRIVER,sizeof(struct nook_driver_data));
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

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
				switch(ppd->nook_state) {
					case 0:		quiet_say(cn,"Hullo, %s. I am %s, the judge.",ch[co].name,ch[cn].name);
							ppd->nook_state++; didsay=1;
							break;
					case 1:		quiet_say(cn,"Oh, wait. I am %s, the Knight of the Shining Armor.",ch[cn].name);
							ppd->nook_state++; didsay=1;
							break;
					case 2:		quiet_say(cn,"No, that's not right either. I am Jester, the %s.",ch[cn].name);
							ppd->nook_state++; didsay=1;
							break;
					case 3:		quiet_say(cn,"If thou art looking for Gwendylon, the mage, take the northernmost door. If thou wishest to talk to Lydia, his noble mageness' daughter, take this door.");
							ppd->nook_state++; didsay=1;
							break;
					case 4:		quiet_say(cn,"If thou hast news from James, the drunkard, thou'd best make haste and bring Lydia his apologies. Her daughterness wasn't too happy about James passing out at the party.");
							ppd->nook_state++; didsay=1;
							break;
					case 5:		if (ppd->gwendy_state<13) break;
							quiet_say(cn,"Oh, hello %s.",ch[co].name);
							questlog_open(co,6);
							ppd->nook_state++; didsay=1;
							break;
					case 6:         quiet_say(cn,"I've heard the mage is looking for a hiding place in this tower. When I started working here, before Gwendylon bought the tower, the old owner - he was a bit strange - used to murmur: 'Stand between torches, in the courtyard, stand between torches'.");
							ppd->nook_state++; didsay=1;
							break;
					case 7:         quiet_say(cn,"I could never make anything of it, but maybe thou art wise enough to understand it, %s.",ch[co].name);
							ppd->nook_state++; didsay=1;
							break;
					case 8:		quiet_say(cn,"But before thou searchest for that skull, I'd like to ask thee for a favor myself. A band of robbers has stolen my cap. It's just a normal cap, but I've inherited it from my father, and it is very dear to me.");
							ppd->nook_state++; didsay=1;
							break;
					case 9:		quiet_say(cn,"Now these robbers are demanding a ransom for that cap. But alas, I am poor and cannot pay. I am to meet that robber at midnight behind the 'Brotherhood of Knights', and I was hoping that thou couldst go there, hide, and follow that robber to his hideout and rescue my cap.");
							ppd->nook_state++; didsay=1;
							break;
					case 10:	quiet_say(cn,"Please, %s, help me. I cannot offer thee a reward, but I'd really, really appreciate thy help.",ch[co].name);
							ppd->nook_state++; didsay=1;
							break;
					case 11:	break;
                                        case 12:	if (ppd->gwendy_state>=17) { ppd->nook_state=16; break; }
							quiet_say(cn,"I have heard that another entrance to the ruin below Yoakin's house has been discovered. I do not think that the skull Gwendylon is looking for is there, but it might be worthwhile to visit it nonetheless.");
							ppd->nook_state++; didsay=1;
							break;
					case 13:	quiet_say(cn,"The entrance is behind Yoakin's house, close to the ruin.");
							ppd->nook_state++; didsay=1;
							break;
					case 14:	quiet_say(cn,"When I started working here, before Gwendylon bought the tower, the old owner - he was a bit strange - used to murmur: 'Stand between torches, in the courtyard, stand between torches'.");
							ppd->nook_state++; didsay=1;
							break;
					case 15:	break;	// repeat of yoakin/torches hint still possible
					case 16:	break;	// all gwendy quests done

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
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd && ppd->nook_state<=4) { dat->last_talk=0; ppd->nook_state=0; }
						if (ppd && ppd->nook_state>=5 && ppd->nook_state<=11) { dat->last_talk=0; ppd->nook_state=5; }
						if (ppd && ppd->nook_state>=12 && ppd->nook_state<=15) { dat->last_talk=0; ppd->nook_state=12; }
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
				ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                                if (it[in].ID==IID_AREA1_JESTERCAP && ppd && ppd->nook_state>=5 && ppd->nook_state<=11) {
					quiet_say(cn,"Ah. There it is! My cap! Oh, %s, I thank thee!",ch[co].name);
					questlog_done(co,6);
					destroy_item_byID(co,IID_AREA1_JESTERCAP);
					destroy_item_byID(co,IID_AREA1_ROBBERKEY1);
                                        ppd->nook_state=12;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else {				
                                        quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

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

//-----------------------

struct lydia_driver_data
{
        int last_talk;
	int current_victim;
};	

void lydia_driver(int cn,int ret,int lastact)
{
	struct lydia_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_LYDIADRIVER,sizeof(struct lydia_driver_data));
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

			//quiet_say(cn,"diff=%d, victim=%d (%s)",ticker-dat->last_talk,dat->current_victim,ch[dat->current_victim].name);

			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
				if (realtime-ppd->lydia_seen_timer>120 && ppd->lydia_state && ppd->lydia_state<4) {
					ppd->lydia_state=0;
				}
                                switch(ppd->lydia_state) {
					case 0:		quiet_say(cn,"Oohh, my head. Hu? Ah, hello %s. I am %s.",ch[co].name,ch[cn].name);
							ppd->lydia_state++; didsay=1;
							questlog_open(co,0);
							break;
					case 1:		quiet_say(cn,"I am sorry, I am no good company today. I went to a party in the village last night and now I got a horrible headache. I had a potion with me which should help cure the hangover.");
							ppd->lydia_state++; didsay=1;
							break;
					case 2:		quiet_say(cn,"But on my way back, I got ambushed. Some thieves stole the potion. James that drunkard was supposed to bring me home, but he passed out. Thou wouldn't happen to have the time to hunt them down and bring me the potion, %s?",ch[co].name);
							ppd->lydia_state++; didsay=1;
							break;
					case 3:		quiet_say(cn,"I would be grateful indeed. My head is killing me. They ambushed me right here in front of the tower and fled west with their loot.");
							ppd->lydia_state++; didsay=1;
							break;
                                        case 4:		if (realtime-ppd->lydia_seen_timer>60) {
								quiet_say(cn,"Hello again, %s! Didst thou find the potion? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
								notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TUTORIAL,0,co);
							}
                                                        break;
                                        case 5:		quiet_say(cn,"Here, this might save thy life.");
							ppd->lydia_state++; didsay=1;
							break;
					case 6:		quiet_say(cn,"Gwendylon, my father, is currently looking for help. If thou art looking for adventures, it might be wise to visit him. He lives next door.");
							ppd->lydia_state++; didsay=1;
							break;
					case 7:		break;
	


				}
				ppd->lydia_seen_timer=realtime;
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
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd && ppd->lydia_state<=4) { dat->last_talk=0; ppd->lydia_state=1; }
						if (ppd && ppd->lydia_state>=6 && ppd->lydia_state<=7) { dat->last_talk=0; ppd->lydia_state=6; }
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

                                ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                                if (it[in].ID==IID_AREA1_WOODPOTION && ppd && ppd->lydia_state<=4) {
					quiet_say(cn,"Ah. That feels so much better. Thank thee, %s.",ch[co].name);
					questlog_done(co,0);
					destroy_item_byID(co,IID_AREA1_WOODPOTION);
					destroy_item_byID(co,IID_AREA1_WOODKEY2);
                                        ppd->lydia_state=5;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

                                        if (ch[co].flags&CF_MAGE) in=create_item("mana_potion1");
					else in=create_item("healing_potion1");
					if (!give_char_item(co,in)) destroy_item(in);
				} else {				
                                        quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

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


//-----------------------

void gwendylon_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

struct balltrap_skelly_driver_data
{
	int last_fire;
};

void balltrap_skelly_driver(int cn,int ret,int lastact)
{
        struct balltrap_skelly_driver_data *dat;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_BALLTRAPSKELLY,sizeof(struct balltrap_skelly_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,20,0,40);		
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

	if (spell_self_driver(cn)) return;

	if (regenerate_driver(cn)) return;

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;

	if (ticker>dat->last_fire+TICKS*3) {
		dat->last_fire=ticker;
		
		if (do_use(cn,DX_LEFT,0)) return;		
	}
	
        //quiet_say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}

//------------

struct robber_driver_data
{
	int state;
};

void robber_driver(int cn,int ret,int lastact)
{
        struct robber_driver_data *dat;
	struct msg *msg,*next;
	int in;

        dat=set_data(cn,DRD_ROBBERDRIVER,sizeof(struct robber_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,20,0,40);		
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

	if (spell_self_driver(cn)) return;

	if (regenerate_driver(cn)) return;

	// make character wear a burning torch all the time
	in=ch[cn].item[WN_LHAND];
	if (!in) {
		in=create_item("torch");
		it[in].carried=cn;
		ch[cn].item[WN_LHAND]=in;
		update_char(cn);
	} else {
		if (!it[in].drdata[0]) use_item(cn,in);		
	}

	fight_driver_set_home(cn,ch[cn].x,ch[cn].y);

	// walk to meeting
	switch(dat->state) {
		case 0:         if (secure_move_driver(cn,30,242,DX_UP,ret,lastact)) return;
                                if (hour==23 && minute>45) dat->state++;
				break;

		case 1:		if (move_driver(cn,30,237,1)) return;
				dat->state++; break;
		case 2:		in=map[31+237*MAXMAP].it;
				if (!in) { charlog(cn,"my ladder is gone!"); dat->state=0; break; }
                                if (use_driver(cn,in,0)) return;
				dat->state++; break;
		case 3:		if (move_driver(cn,222,78,1)) return;
				dat->state++; break;
		case 4:		if (move_driver(cn,190,78,1)) return;
				dat->state++; break;
		case 5:		if (move_driver(cn,173,78,1)) return;
				dat->state++; break;
		case 6:		if (move_driver(cn,173,54,1)) return;
				dat->state++; break;
		case 7:		if (move_driver(cn,145,54,1)) return;
				dat->state++; break;
		case 8:		if (move_driver(cn,145,72,1)) return;		
                                if (ch[cn].dir!=DX_UP) turn(cn,DX_UP);
				if ((hour>0 || minute>15) && hour!=23) dat->state++;
				break;
		case 9:		if (move_driver(cn,145,72,1)) return;
				dat->state++; break;
		case 10:	if (move_driver(cn,145,54,1)) return;
				dat->state++; break;
		case 11:	if (move_driver(cn,173,54,1)) return;
				dat->state++; break;
		case 12:	if (move_driver(cn,173,78,1)) return;
				dat->state++; break;
		case 13:	if (move_driver(cn,190,78,1)) return;
				dat->state++; break;
		case 14:	if (move_driver(cn,222,78,1)) return;
				dat->state++; break;
                case 15:	in=map[244+78*MAXMAP].it;
				if (!in) { charlog(cn,"my hole is gone!"); dat->state=0; break; }
                                if (use_driver(cn,in,0)) return;
				dat->state=0; break;
	}
	

        do_idle(cn,TICKS);
}

//------------

struct sanoa_driver_data
{
	int state;
};

void sanoa_driver(int cn,int ret,int lastact)
{
        struct sanoa_driver_data *dat;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SANOADRIVER,sizeof(struct sanoa_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,20,0,40);		
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

	if (spell_self_driver(cn)) return;

	if (regenerate_driver(cn)) return;

	fight_driver_set_home(cn,ch[cn].x,ch[cn].y);

        // walk around in city
	switch(dat->state) {
		case 0:		if (secure_move_driver(cn,16,31,DX_RIGHT,ret,lastact)) return;
                                if (hour==7 && minute<30) dat->state++;
				else if (hour==10 && minute<30) dat->state++;
				else if (hour==12 && minute<30) dat->state++;
				else if (hour==15 && minute<30) dat->state++;
				else if (hour==18 && minute<30) dat->state++;
				break;

		case 1:		if (move_driver(cn,21,31,1)) return;
				dat->state++; break;
		case 2:         if (move_driver(cn,21,25,0)) return;
				dat->state++; break;
		case 3:		if (!is_closed(21,26) && use_item_at(cn,21,26,0)) return;
				dat->state++; break;
		case 4:		if (move_driver(cn,21,23,1)) return;
				dat->state++; break;
		case 5:		if (move_driver(cn,69,23,1)) return;
				dat->state++; break;
		case 6:		if (move_driver(cn,69,42,1)) return;
				if (minute>30) dat->state++;
				break;
		case 7:		if (move_driver(cn,69,23,1)) return;
				dat->state++; break;
		case 8:		if (move_driver(cn,21,23,1)) return;
				dat->state++; break;
		case 9:		if (move_driver(cn,21,27,0)) return;
				dat->state++; break;
		case 10:	if (!is_closed(21,26) && use_item_at(cn,21,26,0)) return;
				dat->state++; break;				
		case 11:	if (move_driver(cn,21,31,1)) return;
                                dat->state=0; break;
	}
	

        do_idle(cn,TICKS);
}

//------------

struct reskin_driver_data
{
        int last_talk;
	int last_walk;
	int pos;
	int current_victim;
};	

void reskin_driver(int cn,int ret,int lastact)
{
	struct reskin_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,didsay=0,talkdir=0,bit;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_RESKINDRIVER,sizeof(struct reskin_driver_data));
	if (!dat) return;	// oops...

	// loop through our messages once
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
		if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;
	}

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

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
                                switch(ppd->reskin_state) {
					case 0:		if (ppd->gwendy_state<6) break;	// dont offer alchemy quests before skull 2
							if (ppd->terion_state<4) break; // wait for terion to finish skelly2 story
							//if (hour>23 || hour<5) break;	// dont give out quest while player is supposed to do skelly 2
							quiet_say(cn,"Hello, %s! I am %s, the bartender.",ch[co].name,ch[cn].name);
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 1:		quiet_say(cn,"We have a shortage of beer at the moment, %s, so I'm spending some of my time doing alchemistical studies. So far I've found out that only a potion brewed of at least one flower, one berry and one mushroom will have any effect.",ch[co].name);
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"In spite of the beer shortage, people still visit my tavern, so I can't go out to find ingredients as often as I'd like. If thou happenst to come across any new flower, berry or mushroom and bring it to me, I'd pay thee handsomely.");
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 3:		if (realtime-ppd->reskin_seen_timer>600) {					
								quiet_say(cn,"Hello again, %s! Didst thou find any new ingredients? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
							} else if (ppd->logain_state>8) ppd->reskin_state++;
                                                        break;
					case 4:		quiet_say(cn,"Oh, %s, couldst thou do me another favor? I've rented the back room to some, uh, not so respectable members of the society, and they are giving me trouble.",ch[co].name);
							questlog_open(co,17);
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 5:		quiet_say(cn,"A few days ago, a new master took over their, uh, organization, and he's been threatening me. If thou couldst talk to him, I'd appreciate it. I'd even give thee a nice reward.");
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 6:		quiet_say(cn,"Thou canst find them by going through that fake wall there, in the western corner of the room, between these two barrels.");
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 7:		if (check_first_kill(co,16)) {
								quiet_say(cn,"Oh, thank you for talking to the Guild Master, %s.",ch[co].name);
                                                                questlog_done(co,17);
								ppd->reskin_state++;
							}
							break;
					case 8:		if (ch[co].flags&CF_WARRIOR) quiet_say(cn,"Thou canst make a potion to raise thine abilities by using Adygalah, Chrysado, Domari, Beelough and any mushroom.");
							else quiet_say(cn,"Thou canst make a potion to raise thine abilities by using two parts Elithah, one part Firuba, one part Beelough and any mushroom.");
							ppd->reskin_state++; didsay=1;
                                                        break;
					case 9:		break;
				}
				ppd->reskin_seen_timer=realtime;
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DIDSAY,cn,0);
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:         if (ppd && ppd->reskin_state<=4) { dat->last_talk=0; ppd->reskin_state=0; }
						if (ppd && ppd->reskin_state>=4 && ppd->reskin_state<=7) { dat->last_talk=0; ppd->reskin_state=4; }
						if (ppd && ppd->reskin_state>=8 && ppd->reskin_state<=9) { dat->last_talk=0; ppd->reskin_state=8; }
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

			ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_ALCHEMY_INGREDIENT) {	// alchemy ingredient
					if (!(ppd->reskin_got_bits&(bit=(1<<it[in].drdata[0])))) {
						
						if (it[in].drdata[0]==24 && ch[co].level<80) {	// earth stone
							quiet_say(cn,"Oh, a very nice stone, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);
						} else if (it[in].drdata[0]==23 && ch[co].level<10) {	// earth stone
							quiet_say(cn,"Oh, a very nice stone, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);
						} else if (it[in].drdata[0]==21 && ch[co].level<30) {	// fire stone
							quiet_say(cn,"Oh, a very nice stone, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);
						} else if (it[in].drdata[0]==22 && ch[co].level<60) {	// ice stone
							quiet_say(cn,"Oh, a very nice stone, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);
						} else if (it[in].drdata[0]==16 && ch[co].level<25) {	// shroom 9
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==15 && ch[co].level<23) {	// shroom 8
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==14 && ch[co].level<20) {	// shroom 7
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==13 && ch[co].level<18) {	// shroom 6
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==12 && ch[co].level<16) {	// shroom 5
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==11 && ch[co].level<14) {	// shroom 4
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==10 && ch[co].level<10) {	// shroom 3
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else if (it[in].drdata[0]==9 && ch[co].level<7) {	// shroom 2
							quiet_say(cn,"Oh, a very nice mushroom, %s. But I'm afraid I cannot pay for it at the moment.",ch[co].name);						
						} else {
							ppd->reskin_got_bits|=bit;
							quiet_say(cn,"Ah, a nice %s thou found there. Here, this is for thy trouble.",it[in].name);
	
							ch[co].gold+=it[in].value*5; stats_update(co,0,it[in].value*5);
							ch[co].flags|=CF_ITEMS;

							log_char(co,LOG_SYSTEM,0,"You received %.2fG.",(it[in].value*5)/100.0);

                                                        ch[cn].citem=0;
							destroy_item(in);
							return;
						}						
					} else {
                                                quiet_say(cn,"Oh, I'm sorry, %s, but thou brought me this one before.",ch[co].name);

						if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
						ch[cn].citem=0;
						return;
					}
				}
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}
		
		if (msg->type==NT_NPC) {
			if (msg->dat1==NTID_TERION) {
				co=msg->dat2;
				if (msg->dat3==5) {	// no more beer?
					quiet_say(cn,"No Terion, no beer. But what thou sayst is true.");
                                        talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->last_talk=ticker;
				}
			}
		}

		if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;

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

//------------

struct asturin_driver_data
{
	int state;
};

void asturin_driver(int cn,int ret,int lastact)
{
        struct asturin_driver_data *dat;
	struct area1_ppd *ppd;
	struct msg *msg,*next;
	int co;

        dat=set_data(cn,DRD_ASTURINDRIVER,sizeof(struct asturin_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,20,0,40);		
                }

		if (msg->type==NT_CHAR) {
                        co=msg->dat1;

			if ((ch[co].flags&CF_PLAYER) &&
			    ch[co].driver!=CDR_LOSTCON &&
			    char_dist(cn,co)<16 &&
			    char_see_char(cn,co) &&
			    (ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) {
				
				if (realtime-ppd->asturin_seen_timer>10 && ppd->asturin_state>=1 && ppd->asturin_state<=3) {
					ppd->asturin_state=0;
				}
				if (realtime-ppd->asturin_seen_timer>30 && ppd->asturin_state>=7 && ppd->asturin_state<=8) {
					ppd->asturin_state=8;
				}
				
                                if (ch[co].x<115) {
					if (ppd->asturin_state<3) {
						shout(cn,"GUARDS!");
						notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_ASTURIN,cn,co);
						ppd->asturin_state=3;
					}
				} else if (ch[co].x<118) {
					if (ppd->asturin_state<2) {
						quiet_say(cn,"Go back %s, you have no business here!",ch[co].name);
						ppd->asturin_state=2;
					}
					if (ppd->asturin_state>=4 && ppd->asturin_state<=5) {
						quiet_say(cn,"Alright, alright, %s, go ahead, just don't hit me again!",ch[co].name);
						ppd->asturin_state=6;
					}
				} else switch(ppd->asturin_state) {
					       case 0:	quiet_say(cn,"Hello %s. These rooms are private. Please go back.",ch[co].name);
							ppd->asturin_state++; break;
					       case 4:	quiet_say(cn,"Hello %s. These rooms are private. Please go back.",ch[co].name);
							ppd->asturin_state++; break;
					       case 7:	quiet_say(cn,"Be greeted, %s. Welcome.",ch[co].name);
							ppd->asturin_state++; break;

				}
				ppd->asturin_seen_timer=realtime;
                        }
		}

		if (msg->type==NT_TEXT) {
			co=msg->dat3;

                        switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd) ppd->asturin_state=0;
						break;
			}
		}

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

	if (spell_self_driver(cn)) return;

	if (regenerate_driver(cn)) return;

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

        do_idle(cn,TICKS/2);
}

void asturin_dead(int cn,int co)
{
	struct area1_ppd *ppd;

        if ((ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) {
		ppd->asturin_state=4;
		quiet_say(cn,"I'll remember that, %s!",ch[co].name);
	}
}

//-----------------------

struct guiwynn_driver_data
{
        int last_talk;
	int current_victim;
	int nighttime;
};

void guiwynn_driver(int cn,int ret,int lastact)
{
	struct guiwynn_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_GUIWYNNDRIVER,sizeof(struct guiwynn_driver_data));
	if (!dat) return;	// oops...

	// loop through our messages once
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
		if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;
	}

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
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
				if (realtime-ppd->guiwynn_seen_timer>120 && ppd->guiwynn_state && ppd->guiwynn_state<=4) {
					ppd->guiwynn_state=1;
				}
				if (realtime-ppd->guiwynn_seen_timer>120 && ppd->guiwynn_state>=6 && ppd->guiwynn_state<=7) {
					ppd->guiwynn_state=6;
				}
                                switch(ppd->guiwynn_state) {
					case 0:		if (ppd->gwendy_state<17) break;	// dont offer quest before skull 1-4 are done
							quiet_say(cn,"Hello there, %s, please wait a moment.",ch[co].name);
							questlog_open(co,7);
							ppd->guiwynn_state++; didsay=1;
							break;
					case 1:		quiet_say(cn,"I am %s, the town mage and I need your help.",ch[cn].name);
							ppd->guiwynn_state++; didsay=1;
							break;
					case 2:		quiet_say(cn,"I used to teach at the Order of Mages. That is the huge building to the south-east of here. But when I came there, they attacked me.");
							ppd->guiwynn_state++; didsay=1;
							break;
					case 3:		quiet_say(cn,"I could barely escape with my life. It seems they've all gone mad. I do not dare go back there, but I must know what is going on in the Order.");
							ppd->guiwynn_state++; didsay=1;
							break;
					case 4:		quiet_say(cn,"If thou couldst go there and find out, I would reward thee. I am suspecting some kind of disease, or a magical attack, or some kind of poisoning, maybe with the help of an alchemistical potion. If thou findst anything out of the ordinary, bring it to me.");
							ppd->guiwynn_state++; didsay=1;
							if (!has_item(co,IID_AREA1_MADKEY1)) {
								in=create_item("mad_key1");
								if (!give_char_item(co,in)) destroy_item(in);
								quiet_say(cn,"This key opens the front door of the Order.");
							}
                                                        break;
					case 5:		if (realtime-ppd->guiwynn_seen_timer>60) {
								quiet_say(cn,"Be greeted, %s! Didst thou find out anything about the Order? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
							}
                                                        break;

					case 6:         if (questlog_isdone(co,8)) { ppd->guiwynn_state=11; break; }
							quiet_say(cn,"A Potion of Happiness? I have never heard of such a thing before. It does seem to induce madness in those who drink it. But alas, I cannot tell what it is made of.");
							questlog_open(co,8);
							ppd->guiwynn_state++; didsay=1;
							break;
					case 7:		quiet_say(cn,"Couldst thou go back and try to find the recipe? I would double thine reward.");
							ppd->guiwynn_state++; didsay=1;
							if (!has_item(co,IID_AREA1_MADKEY1)) {
								in=create_item("mad_key1");
								if (!give_char_item(co,in)) destroy_item(in);
                                                                quiet_say(cn,"This key opens the front door of the Order.");
							}
                                                        break;
					case 8:		if (realtime-ppd->guiwynn_seen_timer>60) {					
								quiet_say(cn,"Be greeted, %s! Didst thou find the recipe? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
							}
                                                        break;
					case 9:		quiet_say(cn,"Oh, how sad. This madness is not curable. They will remain mad for the rest of their life. But at least I tried.");
							ppd->guiwynn_state++; didsay=1;
							break;
					case 10:	quiet_say(cn,"I thank thee, %s, for thine help. Mayest thou find happiness in thine life.",ch[co].name);
							ppd->guiwynn_state++; didsay=1;
							if (!has_item(co,IID_AREA1_MADKEY1)) {
								in=create_item("mad_key1");
								if (!give_char_item(co,in)) destroy_item(in);
								quiet_say(cn,"This key opens the front door of the Order.");
							}
                                                        break;
					case 11:	if (realtime-ppd->guiwynn_seen_timer>60) {

								quiet_say(cn,"Nice to see you, %s.",ch[co].name);
								didsay=1;
							}
                                                        break;
				}
				ppd->guiwynn_seen_timer=realtime;
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DIDSAY,cn,0);
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd && ppd->guiwynn_state<=5) { dat->last_talk=0; ppd->guiwynn_state=0; }
						if (ppd && ppd->guiwynn_state>=6 && ppd->guiwynn_state<=8) { dat->last_talk=0; ppd->guiwynn_state=6; }
						if (ppd && ppd->guiwynn_state>=9 && ppd->guiwynn_state<=11) { dat->last_talk=0; ppd->guiwynn_state=9; }
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
				
				ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                                if (it[in].ID==IID_AREA1_MADPOTION && ppd && ppd->guiwynn_state<=5) {
					int tmp;
					quiet_say(cn,"Ahh, yes, that might be what was looking for. Thank thee, %s.",ch[co].name);
					tmp=questlog_done(co,7);
                                        ppd->guiwynn_state=6;
					destroy_item_byID(co,IID_AREA1_MADPOTION);
					destroy_item_byID(co,IID_AREA1_MADKEY2);

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_MADMAGE1);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else if (it[in].ID==IID_AREA1_MADNOTE && ppd && ppd->guiwynn_state>=6 && ppd->guiwynn_state<=8) {
					int tmp;
					quiet_say(cn,"Ahh, yes, this is the recipe I was looking for. Thank thee, %s.",ch[co].name);
					tmp=questlog_done(co,8);
					destroy_item_byID(co,IID_AREA1_MADNOTE);
					destroy_item_byID(co,IID_AREA1_MADKEY2);
					destroy_item_byID(co,IID_AREA1_MADKEY3);
					destroy_item_byID(co,IID_AREA1_MADKEY4);
					destroy_item_byID(co,IID_AREA1_MADKEY5);
                                        ppd->guiwynn_state=10;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_MADMAGE2);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else {
                                        quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
                                        if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		if (msg->type==NT_NPC) {
			if (msg->dat1==NTID_TERION) {
				co=msg->dat2;
				if (msg->dat3==1) {	// lads in the woods
					quiet_say(cn,"Yeah, my lad went with them. Fools, to seek the danger.");
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TERION,cn,2);

                                        talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->last_talk=ticker;
				}
				if (msg->dat3==4) {	// yoakins nightmares
					quiet_say(cn,"Yes, he's been here drinking a lot a few weeks ago. Told us that the floor in his back room collapsed, and that he was having scary dreams for several nights.");

					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->last_talk=ticker;
				}
			}
		}
		
		if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
        if (talkdir) turn(cn,talkdir);
        if (ticker-dat->last_talk<TICKS*10) { do_idle(cn,TICKS); return; }

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;			

	do_idle(cn,TICKS);
}

//-----------------------

struct logain_driver_data
{
        int last_talk;
	int current_victim;
	int nighttime;
};


void logain_driver(int cn,int ret,int lastact)
{
	struct logain_driver_data *dat;
	struct area1_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_LOGAINDRIVER,sizeof(struct logain_driver_data));
	if (!dat) return;	// oops...

	// loop through our messages once
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
		if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;
	}

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
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                        if (ppd) {
				if (realtime-ppd->logain_seen_timer>120 && ppd->logain_state && ppd->logain_state<=4) {
					ppd->logain_state=1;
				}
				if (realtime-ppd->logain_seen_timer>120 && ppd->logain_state>=6 && ppd->logain_state<=8) {
					ppd->logain_state=6;
				}
                                switch(ppd->logain_state) {
					case 0:		if (ppd->guiwynn_state<11) break;	// dont offer quest before mad mages is done
							quiet_say(cn,"Hail thee, %s. Canst thou spare a moment?",ch[co].name);
							questlog_open(co,9);
							ppd->logain_state++; didsay=1;
							break;
					case 1:		quiet_say(cn,"My name is %s. I used to train the young knights from the Brotherhood of Knights on the eastern edge of town. But when I went there a few days ago, they started to behave very strangely.",ch[cn].name);
							ppd->logain_state++; didsay=1;
							break;
					case 2:		quiet_say(cn,"I gathered they had used some strength potions to increase their abilities. I was about to find out more, but they got aggressive and a fight was about to start, so I left.");
							ppd->logain_state++; didsay=1;
							break;
					case 3:		quiet_say(cn,"One day later, I tried talking to them again, but they attacked me on sight. Even though I teach fighting, I am old. I could not win against a score of young men. All I could do was keep them away and escape.");
							ppd->logain_state++; didsay=1;
							break;
					case 4:		quiet_say(cn,"I suspect these potions have been poisoned. If thou couldst go there and find out where they got them, I'd reward thee.");
							ppd->logain_state++; didsay=1;
							if (!has_item(co,IID_AREA1_MADKEY6)) {
								in=create_item("mad_key6");
                                                                if (!give_char_item(co,in)) destroy_item(in);
                                                                quiet_say(cn,"Thou willt need this key to gain entry.");
							}
                                                        break;
					case 5:		if (realtime-ppd->logain_seen_timer>60) {
								quiet_say(cn,"Hail thee, %s! Couldst thou find out who is responsible? Or dost thou want me to °c4repeat°c0 mine offer?",ch[co].name);
								didsay=1;
							}
                                                        break;
					case 6:         quiet_say(cn,"Loisan? This is strange indeed. Loisan used to live next door to this tavern. I cannot say I knew him very well, since he hardly ever left his home. He is gone now, anyway.");
							ppd->logain_state++; didsay=1;
                                                        break;
					case 7:		quiet_say(cn,"He left for Aston a few days... Wait, he left for Aston the very day the Knights started to behave strangely. He even left the key to his house with me, asked me to look after it.");
							ppd->logain_state++; didsay=1;
							if (!has_item(co,IID_AREA1_MADKEY9)) {
								in=create_item("mad_key9");
                                                                if (!give_char_item(co,in)) destroy_item(in);
								quiet_say(cn,"Here. I won't use it. But thou might want to search his house.");
							}
                                                        break;
					case 8:		quiet_say(cn,"Well, if you ever get to Aston, pay this Loisan a visit.");
							ppd->logain_state++; didsay=1;
							if (!has_item(co,IID_AREA1_MADKEY6)) {
								in=create_item("mad_key6");
                                                                if (!give_char_item(co,in)) destroy_item(in);
								quiet_say(cn,"Shouldst thou wish to visit the Brotherhood again, here's the key.");
							}
                                                        break;							
					case 9:		if (realtime-ppd->logain_seen_timer>60) {
								quiet_say(cn,"I am pleased to see thee, %s.",ch[co].name);
								didsay=1;
							}
                                                        break;
				}
				ppd->logain_seen_timer=realtime;
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DIDSAY,cn,0);
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));
						if (ppd && ppd->logain_state<=5) { dat->last_talk=0; ppd->logain_state=0; }
						if (ppd && ppd->logain_state>=6 && ppd->logain_state<=9) { dat->last_talk=0; ppd->logain_state=6; }
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
				
				ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd));

                                if (it[in].ID==IID_AREA1_MADNOTE2 && ppd && ppd->logain_state<=5) {
					int tmp;
					quiet_say(cn,"Now let's see. Ah. I thank thee, %s.",ch[co].name);
					tmp=questlog_done(co,9);
					destroy_item_byID(co,IID_AREA1_MADNOTE2);
					destroy_item_byID(co,IID_AREA1_MADKEY7);
					destroy_item_byID(co,IID_AREA1_MADKEY8);
                                        ppd->logain_state=6;

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					if (tmp==1) {
						in=create_money_item(MONEY_AREA1_MADKNIGHT);
						if (!give_char_item(co,in)) destroy_item(in);
					}
				} else {
                                        quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");

					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                        ch[cn].citem=0;
				}
			}
		}
		if (msg->type==NT_NPC) {
			if (msg->dat1==NTID_TERION) {
				co=msg->dat2;
				if (msg->dat3==2) {	// fools to seek danger
					quiet_say(cn,"Fools, yes fools they are. As if we didn't have enough problems already.");

                                        talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->last_talk=ticker;
				}
				if (msg->dat3==3) {	// yoakin mentioned
					quiet_say(cn,"Ah, Terion, thou art right! I remember Yoakin telling me about nightmares he's been having lately. Something about skeletons hunting him in a dark, moist place.");
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_TERION,cn,4);
                                        talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->last_talk=ticker;
				}
			}
			
			if (msg->type==NT_NPC && msg->dat1==NTID_DIDSAY && msg->dat2!=cn) dat->last_talk=ticker;
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);
        if (ticker-dat->last_talk<TICKS*10) { do_idle(cn,TICKS); return; }
	
        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;			

        do_idle(cn,TICKS);
}

void balltrap_skelly_dead(int cn,int co)
{
        ;
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_GWENDYLON:	gwendylon_driver(cn,ret,lastact); return 1;
		case CDR_YOAKIN:	yoakin_driver(cn,ret,lastact); return 1;
		case CDR_BALLTRAP:	balltrap_skelly_driver(cn,ret,lastact); return 1;
		case CDR_TERION:	terion_driver(cn,ret,lastact); return 1;
		case CDR_JAMES:		james_driver(cn,ret,lastact); return 1;
                case CDR_NOOK:		nook_driver(cn,ret,lastact); return 1;
		case CDR_LYDIA:		lydia_driver(cn,ret,lastact); return 1;
		case CDR_ROBBER:	robber_driver(cn,ret,lastact); return 1;
		case CDR_SANOA:		sanoa_driver(cn,ret,lastact); return 1;
		case CDR_ASTURIN:	asturin_driver(cn,ret,lastact); return 1;
		case CDR_RESKIN:	reskin_driver(cn,ret,lastact); return 1;
		case CDR_GUIWYNN:	guiwynn_driver(cn,ret,lastact); return 1;
		case CDR_LOGAIN:	logain_driver(cn,ret,lastact); return 1;

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
		case CDR_GWENDYLON:	gwendylon_dead(cn,co); return 1;
		case CDR_YOAKIN: 	gwendylon_dead(cn,co); return 1;
		case CDR_BALLTRAP:	balltrap_skelly_dead(cn,co); return 1;
		case CDR_TERION:	gwendylon_dead(cn,co); return 1;
		case CDR_JAMES:		gwendylon_dead(cn,co); return 1;
                case CDR_NOOK:		gwendylon_dead(cn,co); return 1;
		case CDR_LYDIA:		gwendylon_dead(cn,co); return 1;
		case CDR_ROBBER:	balltrap_skelly_dead(cn,co); return 1;
		case CDR_SANOA:		balltrap_skelly_dead(cn,co); return 1;
		case CDR_ASTURIN:	asturin_dead(cn,co); return 1;
		case CDR_RESKIN:	balltrap_skelly_dead(cn,co); return 1;
		case CDR_GUIWYNN:	gwendylon_dead(cn,co); return 1;
		case CDR_LOGAIN:	gwendylon_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_GWENDYLON:	return 1;
		case CDR_YOAKIN: 	return 1;
		case CDR_BALLTRAP:	return 1;
		case CDR_TERION:	return 1;
		case CDR_JAMES:		return 1;
                //case CDR_GEREWIN:	return 1;
		case CDR_NOOK:		return 1;
		case CDR_LYDIA:		return 1;
		case CDR_ROBBER:	return 1;
		case CDR_SANOA:		return 1;
		case CDR_ASTURIN:	return 1;
		case CDR_RESKIN:	return 1;
		case CDR_GUIWYNN:	return 1;
		case CDR_LOGAIN:	return 1;

		default:		return 0;
	}
}

int can_raise(int cn,int v)
{
	int seyan;

	if (!ch[cn].value[1][v]) return 0;
	
	if (!(ch[cn].flags&CF_ARCH) && ch[cn].value[1][v]>49) return 0;

	if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1;
	else seyan=0;

	if (seyan && ch[cn].value[1][v]>99) return 0;
	if (ch[cn].value[1][v]>114) return 0;

	if (v==V_PROFESSION && ch[cn].value[1][v]>99) return 0;

	return 1;
}

int get_fight_skill_skill(int cn)
{
	int in;

	if (cn<1 || cn>=MAXCHARS) {
		elog("get_fight_skill(): called with illegal character %d",cn);
		return 0;
	}

	in=ch[cn].item[WN_RHAND];

	if (in<0 || in>=MAXITEM) {
		elog("get_fight_skill(): character %s (%d) is wielding illegal item %d. Removing.",ch[cn].name,cn,in);
		ch[cn].item[WN_RHAND]=0;
		in=0;
	}

	if (!in || !(it[in].flags&IF_WEAPON)) {	// not wielding anything, or not wielding a weapon
		return V_HAND;
	}

        if (it[in].flags&IF_DAGGER) return V_DAGGER;
	if (it[in].flags&IF_STAFF) return V_STAFF;
	if (it[in].flags&IF_SWORD) return V_SWORD;
	if (it[in].flags&IF_TWOHAND) return V_TWOHAND;

	return 0;
}

int james_raisehint(int cn,int doraise)
{
	int v;
	double raise[V_MAX],mr=0,weight;
	int done[V_MAX],cnt=0,didraise=0;
	int offense=1,defense=1,immun=1,flash=1,fire=1,misc=1,warcry;
	double blessmod,sum=0;

	bzero(raise,sizeof(raise));
	bzero(done,sizeof(done));

	if (ch[cn].value[1][V_BLESS]) blessmod=1.4;
	else blessmod=1.0;

	if (ch[cn].value[1][V_FLASH]>1 && ch[cn].value[1][V_FLASH]>ch[cn].value[1][V_FIRE]) { fire=0; flash=1; }
	if (ch[cn].value[1][V_FIRE]>1 && ch[cn].value[1][V_FIRE]>ch[cn].value[1][V_FLASH]) { fire=1; flash=0; }

	if (flash && fire) {
		if (RANDOM(2)) { fire=1; flash=0; }
		else { fire=0; flash=1; }		
	}

	if (!(ch[cn].flags&CF_MAGE)) warcry=1;
	else warcry=0;

	if (offense) {
		if (can_raise(cn,V_ATTACK)) raise[V_ATTACK]+=1.0/(raise_cost(V_ATTACK,ch[cn].value[1][V_ATTACK]));
		
		if (!ch[cn].value[1][V_ATTACK]) {
			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*16);
			if (can_raise(cn,V_HEAL)) raise[V_HEAL]+=1.0/(raise_cost(V_HEAL,ch[cn].value[1][V_HEAL])*16);
			if (can_raise(cn,V_FREEZE)) raise[V_FREEZE]+=1.0/(raise_cost(V_FREEZE,ch[cn].value[1][V_FREEZE])*16);
			if (can_raise(cn,V_MAGICSHIELD)) raise[V_MAGICSHIELD]+=1.0/(raise_cost(V_MAGICSHIELD,ch[cn].value[1][V_MAGICSHIELD])*16);
			if (can_raise(cn,V_FLASH)) raise[V_FLASH]+=1.0/(raise_cost(V_FLASH,ch[cn].value[1][V_FLASH])*16);
			if (can_raise(cn,V_FIRE)) raise[V_FIRE]+=1.0/(raise_cost(V_FIRE,ch[cn].value[1][V_FIRE])*16);
		}
		
		v=get_fight_skill_skill(cn);
		if (can_raise(cn,v)) raise[v]+=1.0/(raise_cost(v,ch[cn].value[1][v])*2);
	
		if (can_raise(cn,V_TACTICS)) raise[V_TACTICS]+=1.0/(raise_cost(V_TACTICS,ch[cn].value[1][V_TACTICS])*4);
	
		if (!ch[cn].value[1][V_ATTACK]) {
			// skill
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/blessmod);

			// spells
			v=V_MAGICSHIELD;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/2/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/2/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/2/blessmod);

			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*40/3/3);
		} else {
			// skill
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/blessmod);

			// attack
			v=V_ATTACK;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/2/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/2/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/2/blessmod);

			// tactics
			v=V_TACTICS;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*20/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*20/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*20/blessmod);
		
			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*40/3.5/3);
		}
	}

	if (defense) {
		if (can_raise(cn,V_PARRY)) raise[V_PARRY]+=1.0/(raise_cost(V_PARRY,ch[cn].value[1][V_PARRY]));
		
		if (!ch[cn].value[1][V_PARRY]) {
			if (can_raise(cn,V_MAGICSHIELD)) raise[V_MAGICSHIELD]+=1.0/(raise_cost(V_MAGICSHIELD,ch[cn].value[1][V_MAGICSHIELD]));
		}
		
		v=get_fight_skill_skill(cn);
		if (can_raise(cn,v)) raise[v]+=1.0/(raise_cost(v,ch[cn].value[1][v])*2);
	
		if (can_raise(cn,V_TACTICS)) raise[V_TACTICS]+=1.0/(raise_cost(V_TACTICS,ch[cn].value[1][V_TACTICS])*4);
	
		if (!ch[cn].value[1][V_PARRY]) {
			// skill
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/blessmod);

			// MS
			v=V_MAGICSHIELD;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/2/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/2/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/2/blessmod);
		
			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*40/2/3);
		} else {
			// skill
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/blessmod);

			// parry
			v=V_PARRY;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/2/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/2/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/2/blessmod);

			// tactics
			v=V_TACTICS;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*20/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*20/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*20/blessmod);
		
			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*40/3.5/3);
		}
	}

	if (immun) {
		if (can_raise(cn,V_IMMUNITY)) raise[V_IMMUNITY]+=1.0/(raise_cost(V_IMMUNITY,ch[cn].value[1][V_IMMUNITY]));
		if (can_raise(cn,V_TACTICS)) raise[V_TACTICS]+=1.0/(raise_cost(V_TACTICS,ch[cn].value[1][V_TACTICS])*5);
		
		v=V_IMMUNITY;
		if (!ch[cn].value[1][V_TACTICS]) {
			// skill
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*5/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*5/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*5/blessmod);

			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*20/3);
		} else {
			// skill
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*5/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*5/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*5/blessmod);

			// tactics
			v=V_TACTICS;
			if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*25/blessmod);
			if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*25/blessmod);
			if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*25/blessmod);

			if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*20/1.2/3);
		}
	}

	if (flash && ch[cn].value[1][V_FLASH] && (ch[cn].flags&(CF_MAGE|CF_WARRIOR))!=(CF_MAGE|CF_WARRIOR)) {
		if (can_raise(cn,V_FLASH)) raise[V_FLASH]+=1.0/(raise_cost(V_FLASH,ch[cn].value[1][V_FLASH]));
		
		v=V_FLASH;
		if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*5/blessmod);
		if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*5/blessmod);
		if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*5/blessmod);

		if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*20/3);
	}

	if (fire && ch[cn].value[1][V_FIRE] && (ch[cn].flags&(CF_MAGE|CF_WARRIOR))!=(CF_MAGE|CF_WARRIOR)) {
		if (can_raise(cn,V_FIRE)) raise[V_FIRE]+=1.0/(raise_cost(V_FIRE,ch[cn].value[1][V_FIRE]));
		
		v=V_FIRE;
		if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*5/blessmod);
		if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*5/blessmod);
		if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*5/blessmod);

		if (can_raise(cn,V_BLESS)) raise[V_BLESS]+=1.0/(raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])*20/3);
	}

        if (warcry && ch[cn].value[1][V_WARCRY]) {
		if (can_raise(cn,V_WARCRY)) raise[V_WARCRY]+=1.0/(raise_cost(V_WARCRY,ch[cn].value[1][V_WARCRY])*2);
		
		v=V_WARCRY;
		if (can_raise(cn,skill[v].base1)) raise[skill[v].base1]+=1.0/(raise_cost(skill[v].base1,ch[cn].value[1][skill[v].base1])*10/blessmod);
		if (can_raise(cn,skill[v].base2)) raise[skill[v].base2]+=1.0/(raise_cost(skill[v].base2,ch[cn].value[1][skill[v].base2])*10/blessmod);
		if (can_raise(cn,skill[v].base3)) raise[skill[v].base3]+=1.0/(raise_cost(skill[v].base3,ch[cn].value[1][skill[v].base3])*10/blessmod);
	}
	if (misc) {
		for (v=0; v<V_MAX; v++) {
			switch(v) {
                                case V_HP:		if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==CF_WARRIOR) weight=2;
							else weight=5; break;
				case V_MANA:		if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==CF_MAGE) weight=1;
							else weight=3; break;
				case V_ENDURANCE:	if (warcry) weight=4; else weight=8; break;
				
                                case V_DURATION:	weight=4; break;
				case V_RAGE:		weight=4; break;
				case V_PROFESSION:	weight=2; break;

				case V_REGENERATE:	weight=4; break;
				case V_MEDITATE:	weight=4; break;


				default:		continue;
			}
			if (!raise[v] && can_raise(cn,v))
                                raise[v]=1.0/(raise_cost(v,ch[cn].value[1][v])*weight);
		}
	}

	for (v=0; v<V_MAX; v++) {
		mr=max(mr,raise[v]);		
	}
	if (mr==0) return 0;
	
	for (v=0; v<V_MAX; v++) {
		if (raise[v]/mr>0.90 && !done[v]) {
			if (doraise) didraise+=raise_value(cn,v);
			else log_char(cn,LOG_SYSTEM,0,"You should definitely raise %s.",skill[v].name);
			done[v]=1;
		}
	}
	if (doraise) return didraise;
	
	for (v=0; v<V_MAX; v++) {
		if (raise[v]/mr>0.80 && !done[v]) {
			log_char(cn,LOG_SYSTEM,0,"You should consider raising %s.",skill[v].name);
			done[v]=1;
		}
	}
	for (v=0; v<V_MAX; v++) {
		if (raise[v]/mr>0.65 && !done[v]) {
			log_char(cn,LOG_SYSTEM,0,"You might raise %s, but you probably shouldn't.",skill[v].name);
			done[v]=1;
		}
	}
	for (v=0; v<V_MAX; v++) {
		if (raise[v] && raise[v]/mr<0.30 && !done[v] && v!=V_FREEZE && v!=V_FIRE && v!=V_FLASH) {
			log_char(cn,LOG_SYSTEM,0,"You should not raise %s for a while.",skill[v].name);
			done[v]=1;
		}
	}

	for (v=0; v<V_MAX; v++) {
		if (raise[v] && v!=V_FREEZE) { sum+=raise[v]/mr; cnt++; } //v!=V_BALL &&
	}
	if (cnt) {
		sum/=cnt;
		if (sum>=0.90) log_char(cn,LOG_SYSTEM,0,"Your character seems to be very well balanced indeed.");
		else if (sum>0.80) log_char(cn,LOG_SYSTEM,0,"Your character seems to be very well balanced.");
		else if (sum>0.70) log_char(cn,LOG_SYSTEM,0,"Your character seems to be well balanced.");
		else if (sum>0.60) log_char(cn,LOG_SYSTEM,0,"Your character seems to be fairly well balanced.");
		else if (sum>0.30) log_char(cn,LOG_SYSTEM,0,"Your character seems to be somewhat unbalanced.");
		else log_char(cn,LOG_SYSTEM,0,"Your character seems to be very unbalanced.");
	}

	log_char(cn,LOG_SYSTEM,0,"Please rely on your own judgement, too. I am just James the drunkard, and I might very well be wrong...");

	return 0;
}

void james_create_eq(int cn)
{
	int as,ws,in;
	char buf[80];

	as=4;

        if (ch[cn].flags&CF_WARRIOR) ws=min(10,ch[cn].value[1][V_SWORD]/10+1);
	else ws=min(10,ch[cn].value[1][V_DAGGER]/10+1);

	if (as) sprintf(buf,"armor%dq3",as); else sprintf(buf,"vest");
	if ((in=ch[cn].item[WN_BODY])) destroy_item(in);
	in=ch[cn].item[WN_BODY]=create_item(buf);
	it[in].mod_index[3]=V_WIS; it[in].mod_value[3]=ch[cn].level/5;
	it[in].mod_index[4]=V_INT; it[in].mod_value[4]=ch[cn].level/5;
	it[in].mod_index[2]=V_AGI; it[in].mod_value[2]=ch[cn].level/5;
	it[in].carried=cn;

	if (as) sprintf(buf,"helmet%dq3",as); else sprintf(buf,"green_hat");
	if ((in=ch[cn].item[WN_HEAD])) destroy_item(in);
	in=ch[cn].item[WN_HEAD]=create_item(buf);
	it[in].mod_index[3]=V_STR; it[in].mod_value[3]=ch[cn].level/5;
	it[in].mod_index[4]=V_DAGGER; it[in].mod_value[4]=ch[cn].level/5;
	it[in].mod_index[2]=V_SWORD; it[in].mod_value[2]=ch[cn].level/5;
	it[in].carried=cn;

	if (as) sprintf(buf,"sleeves%dq3",as); else sprintf(buf,"bracelet");
	if ((in=ch[cn].item[WN_ARMS])) destroy_item(in);
	in=ch[cn].item[WN_ARMS]=create_item(buf);
	it[in].mod_index[3]=V_ATTACK; it[in].mod_value[3]=ch[cn].level/5;
	it[in].mod_index[4]=V_PARRY; it[in].mod_value[4]=ch[cn].level/5;
	it[in].mod_index[2]=V_TACTICS; it[in].mod_value[2]=ch[cn].level/5;
	it[in].carried=cn;

	if (as) sprintf(buf,"leggings%dq3",as); else sprintf(buf,"trousers");
	if ((in=ch[cn].item[WN_LEGS])) destroy_item(in);
	in=ch[cn].item[WN_LEGS]=create_item(buf);
	it[in].mod_index[3]=V_BLESS; it[in].mod_value[3]=ch[cn].level/5;
	it[in].mod_index[4]=V_FLASH; it[in].mod_value[4]=ch[cn].level/5;
	it[in].mod_index[2]=V_MAGICSHIELD; it[in].mod_value[2]=ch[cn].level/5;
	it[in].carried=cn;

	if (ch[cn].flags&CF_WARRIOR) sprintf(buf,"sword%dq3",ws); else sprintf(buf,"dagger%dq3",ws);
	if ((in=ch[cn].item[WN_RHAND])) destroy_item(in);
	in=ch[cn].item[WN_RHAND]=create_item(buf);
	it[in].mod_index[3]=V_IMMUNITY; it[in].mod_value[3]=ch[cn].level/5;
	it[in].mod_index[4]=V_HP; it[in].mod_value[4]=ch[cn].level/5;
	it[in].mod_index[2]=V_MANA; it[in].mod_value[2]=ch[cn].level/5;
	it[in].carried=cn;

	if ((in=ch[cn].item[WN_LHAND])) destroy_item(in);
	in=ch[cn].item[WN_LHAND]=create_item("torch");
	it[in].carried=cn;

	update_char(cn);
	ch[cn].flags|=CF_ITEMS;
}















