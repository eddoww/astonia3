/*
 * $Id: area3.c,v 1.12 2007/09/21 10:58:36 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: area3.c,v $
 * Revision 1.12  2007/09/21 10:58:36  devel
 * gatekeeper door doesnt allow running heal any more
 *
 * Revision 1.11  2007/08/13 18:52:01  devel
 * fixed some warnings
 *
 * Revision 1.10  2007/08/09 11:10:45  devel
 * added statistics to reskin shrooms
 *
 * Revision 1.9  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.8  2007/06/22 13:00:53  devel
 * added caligar quest to kelly
 *
 * Revision 1.7  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
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
#include "statistics.h"
#include "player.h"

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
	{{"aye",NULL},NULL,3},
	{{"nay",NULL},NULL,4},
	{{"list",NULL},NULL,5},
	{{"money",NULL},NULL,6},	
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

struct seymour_driver_data
{
        int last_talk;
	int current_victim;
};

void seymour_driver(int cn,int ret,int lastact)
{
	struct seymour_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SEYMOURDRIVER,sizeof(struct seymour_driver_data));
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
				switch(ppd->seymour_state) {
					case 0:		say(cn,"Welcome to Aston, %s! I am %s, Staff Sergeant of the Seyan'Du, the late emperor's personal guard.",ch[co].name,ch[cn].name);
							questlog_open(co,10);
							ppd->seymour_state++; didsay=1;
							break;
					case 1:		say(cn,"There are but few of us left. Most died in the defense when the underworld opened and the monsters stormed the imperial palace.");
							ppd->seymour_state++; didsay=1;
							break;
					case 2:		say(cn,"Do be wary when thou approachest the palace. It is still in ruins, and monsters appear there from time to time.");
							ppd->seymour_state++; didsay=1;
							break;
					case 3:		say(cn,"Some greedy folks tried to loot it, but few have returned with their spoils. Ah, be that as it may. I talk too much.");
							ppd->seymour_state++; didsay=1;
							break;
					case 4:		say(cn,"My captain said I was to hire strong looking adventurers for Imperial service. Never before have the Seyan'Du asked for help. But now we do. Oh, what has become of this world?");
							ppd->seymour_state++; didsay=1;
							break;
					case 5:		say(cn,"Our emperor murdered, nine of ten killed. Oh my, oh my.");
							ppd->seymour_state++; didsay=1;
							break;
					case 6:		say(cn,"%s, the Seyan'Du are offering you rank and status, in exchange for some missions. The first of these missions is to find out more about a certain Loisan.",ch[co].name);
							ppd->seymour_state++; didsay=1;
							break;
					case 7:		say(cn,"It seems he lived in Cameron for a while and moved here a few weeks later. After he left Cameron, skeletons have been haunting that place. Now, he has left Aston, and we're having trouble with zombies.");
							ppd->seymour_state++; didsay=1;
							break;
					case 8:		say(cn,"As a first step, I want you to go back to Cameron and search Loisan's house there. I have heard rumors that he was working on strange human skulls, and I want you to acquire one of those and bring it to me.");
							ppd->seymour_state++; didsay=1;
							break;
					case 9:		break;
					case 10:	if (questlog_isdone(co,11)) { ppd->seymour_state=12; break; }
                                                        say(cn,"Your next mission, %s, is to search Loisan's house here in Aston. It is on this street, on the western side. As far as we know, he's been using silver skulls here, and I want you to bring me one of those. You might also want to talk to the Governor of Aston for additional missions.",get_army_rank_string(co));
							questlog_open(co,11);
							ppd->seymour_state++; didsay=1;
							break;
					case 11:	break;
					case 12:	ppd->seymour_state=14; //fall through intended
					case 13:	ppd->seymour_state=14; //fall through intended
					case 14:	if (questlog_isdone(co,12)) { ppd->seymour_state=16; break; }
							say(cn,"Alright, now that we have the skulls he was using, it would be nice to know what became of Loisan. If you can find him, his body, or proof of his whereabouts, bring it to me.");
							questlog_open(co,12);
							ppd->seymour_state++; didsay=1;
							break;
					case 15:	break;
					case 16:	say(cn,"Kelly, my superior, mentioned that she needs some fighters. Please go to her and offer your service. And do not forget to report to the Governor from time to time.");
							ppd->seymour_state++; didsay=1;
							break;
					case 17:	break;


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
						if (ppd && ppd->seymour_state<=9) { dat->last_talk=0; ppd->seymour_state=0; }
						if (ppd && ppd->seymour_state>=10 && ppd->seymour_state<=11) { dat->last_talk=0; ppd->seymour_state=10; }
						if (ppd && ppd->seymour_state>=12 && ppd->seymour_state<=13) { dat->last_talk=0; ppd->seymour_state=12; }
						if (ppd && ppd->seymour_state>=14 && ppd->seymour_state<=15) { dat->last_talk=0; ppd->seymour_state=14; }
						if (ppd && ppd->seymour_state>=16 && ppd->seymour_state<=17) { dat->last_talk=0; ppd->seymour_state=16; }
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

                                if (it[in].ID==IID_AREA2_LOISANNOTE && ppd->seymour_state==15) {
					int tmp;

					say(cn,"So he is dead. Ah, well. Thank you, %s.",ch[co].name);
					tmp=questlog_done(co,12);
					destroy_item_byID(co,IID_AREA2_LOISANNOTE);
					ppd->seymour_state=16;

					if (tmp==1) give_military_pts(cn,co,2,1);

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (it[in].ID==IID_AREA2_ZOMBIESKULL1 && ppd->seymour_state==9) {
					say(cn,"A strange skull indeed, %s.",ch[co].name);
					questlog_done(co,10);
					ppd->seymour_state=10;

					if (!get_army_rank_int(co)) {
						set_army_rank(co,1);
						say(cn,"Welcome to the Imperial Army, %s %s!",get_army_rank_string(co),ch[co].name);
					}

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (it[in].ID==IID_AREA2_ZOMBIESKULL2 && ppd->seymour_state==11) {
					int tmp;
					say(cn,"Ah. Well done, %s.",ch[co].name);
					tmp=questlog_done(co,11);
					ppd->seymour_state=12;

					if (tmp==1) give_military_pts(cn,co,1,1);

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

struct kelly_driver_data
{
        int last_talk;
	int current_victim;
};

int collect_heads(int cn,int co)
{
	int n,in,cnt=0,sum=0;

	for (n=30; n<INVENTORYSIZE; n++) {
		if ((in=ch[co].item[n]) && it[in].ID==IID_AREA15_HEAD) {
			cnt++;
			sum+=125+it[in].drdata[0]*75;
			ch[co].item[n]=0;
			destroy_item(in);
		}
	}
	if (cnt) {
		say(cn,"Ah. %d heads. I'll give you %.2fG for them.",cnt,sum/100.0);
		ch[co].gold+=sum; stats_update(co,0,sum);
		ch[co].flags|=CF_ITEMS;
		return 1;
	}
	return 0;
}

void kelly_driver(int cn,int ret,int lastact)
{
	struct kelly_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0,cnt;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_KELLYDRIVER,sizeof(struct kelly_driver_data));
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
				switch(ppd->kelly_state) {
					case 0:		quiet_say(cn,"Greetings, %s! I am %s, First Sergeant of the Seyan'Du, the late emperor's personal guard.",ch[co].name,ch[cn].name);
							ppd->kelly_state++; didsay=1;
							break;
					case 1:		if (ppd->seymour_state>=16) ppd->kelly_state++;	// fall through...
							else break;
					case 2:		quiet_say(cn,"Listen, %s. There have been some attacks from beings we call Stone Creepers. They come from the depths below the city. One known entrance is in the park.",get_army_rank_string(co));
							questlog_open(co,13);
							ppd->kelly_state++; didsay=1;
							break;
					case 3:		quiet_say(cn,"During the last raid, one huge creeper was seen. It did as much damage as all the others together. Go there, and slay it. As proof of thy deed, bring me its head.");
							ppd->kelly_state++; didsay=1;
							break;
					case 4:		quiet_say(cn,"That will be all, %s. Dismissed!",get_army_rank_string(co));
							ppd->kelly_state++; didsay=1;
							break;
					case 5:		break;	// waiting for creeper head
					case 6:		if (questlog_isdone(co,14)) { ppd->kelly_state=10; break; }
							quiet_say(cn,"I have another mission for thee, %s. Scouts have found an underground park, which can be entered through a hole in the northern part of the forest in the southern corner of Aston.",ch[co].name);
							questlog_open(co,14);
							ppd->kelly_state++; didsay=1;
							break;
					case 7:		quiet_say(cn,"They found a strange shrine there. I suspect there are several of them. Thou are to go there and find them all. Report back when thou hast found at least one.");
							ppd->kelly_state++; didsay=1;
							break;
					case 8:		quiet_say(cn,"Dismissed, %s. And good luck. Thou shalt need it.",get_army_rank_string(co));
							ppd->kelly_state++; didsay=1;
							break;
					case 9:		cnt=ppd->kelly_found1+ppd->kelly_found2+ppd->kelly_found3;
							if (cnt>ppd->kelly_found_cnt) {
								if (cnt!=1) quiet_say(cn,"Well done. I see thou hast discovered %d shrines, %s.",cnt,ch[co].name);
								else quiet_say(cn,"Well done. I see thou hast discovered %d shrine, %s.",cnt,ch[co].name);

                                                                give_military_pts(cn,co,(cnt-ppd->kelly_found_cnt)*2,(cnt-ppd->kelly_found_cnt)*EXP_AREA3_SHRINE);
								
								ppd->kelly_found_cnt=cnt;								
								didsay=1;
							}
							if (ppd->kelly_found_cnt==3) {
								quiet_say(cn,"I guess there are just three of them. Good work, %s.",get_army_rank_string(co));
								ppd->kelly_state++;
								didsay=1;
								questlog_done(co,14);
							}
							break;
					case 10:        quiet_say(cn,"I tell thee, %s, all these findings are most unsettling. I think we have but scratched on the surface of this underground world, and I fear what we are yet to find.",ch[co].name);
                                                        ppd->kelly_state++; didsay=1;
							break;
					case 11:	quiet_say(cn,"The Imperial Army, the little that is left of it, is exploring the underworld, but our losses are heavy. All of a sudden, holes open everywhere, and monsters start pouring out. We needst find out where they come from, and why they attack us all of a sudden.");
							ppd->kelly_state++; didsay=1;
							break;
					case 12:	quiet_say(cn,"The future seemed so bright when Seyan was still alive and Ishtar was here to teach us.");
							ppd->kelly_state++; didsay=1;
							break;
					case 13:	if (ch[co].level>=22) ppd->kelly_state++; // fall thru intended
							else break;
					case 14:	if (questlog_isdone(co,15)) { ppd->kelly_state=19; break; }
							quiet_say(cn,"We have lost contact with our outpost in the swamp north of Aston. I want thee to go there and deliver a full report when thou getst back. Dismissed, %s.",get_army_rank_string(co));
							questlog_open(co,15);
							ppd->kelly_state++; didsay=1;
							break;
					case 15:	if (ppd->clara_state>=5) ppd->kelly_state++;	// fall through...
							else break;
					case 16:	quiet_say(cn,"So Clara is well? 'Tis is good to hear, %s. I thank thee.",ch[co].name);
							questlog_done(co,15);
							give_military_pts(cn,co,3,1);
							ppd->kelly_state++; didsay=1;
							break;
					case 17:	quiet_say(cn,"I think Clara is right that we should not send reinforcements yet. But these swamp beasts worry me. I will pay thee for each swamp beast head thou bringst me. The larger the head, the larger the bounty.");
							ppd->kelly_state++; didsay=1;
							break;
					case 18:	quiet_say(cn,"Remember to report to Clara again, %s. Dismissed.",ch[co].name);
							ppd->kelly_state++; didsay=1;
							break;
					case 19:	if (collect_heads(cn,co)) didsay=1;
							if (ch[co].level>=56) ppd->kelly_state++;	// fall thru intended
                                                        else break;
					case 20:	if (questlog_count(co,60)) {
								ppd->kelly_state=26;
								break;
							}
							if (questlog_count(co,54)) {
								ppd->kelly_state=21;
								break;
							}
							quiet_say(cn,"Hello again, %s.",ch[co].name);
							questlog_open(co,54);
							questlog_open(co,60);
							ppd->kelly_state++; didsay=1;
							break;
					case 21:	quiet_say(cn,"I have another mission for you. An important plaque containing the signatures of every Emporer who has ruled over Aston has been stolen from Wesley's bank vault.");
							ppd->kelly_state++; didsay=1;
							break;
					case 22:	quiet_say(cn,"He found a note that was signed 'Grendom Carmin', a member of the Carmin Clan, who were a group of destructive mages, banished from Aston by the Imperial Army.");
							ppd->kelly_state++; didsay=1;
							break;
					case 23:	quiet_say(cn,"They settled in a forest to the south west of Aston and a wall was built so they could not come back.");
							ppd->kelly_state++; didsay=1;
							break;
					case 24:	quiet_say(cn,"Go to Gwendylon with this letter. He will teleport you there. I have a contact named Glori collecting information on the area. See what she knows, and get that plaque back at all costs! Dismissed!");
							if (!has_item(co,IID_CALIGARLETTER)) {
								in=create_item("caligar_letter");
								if (in && !give_char_item(co,in)) destroy_item(in);
							}
							ppd->kelly_state++; didsay=1;
							break;
					case 25:	break;	// waiting for caligar plaque
					case 26:	break;	// all done


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
						if (ppd && ppd->kelly_state<=5) { dat->last_talk=0; ppd->kelly_state=0; }
						if (ppd && ppd->kelly_state>5 && ppd->kelly_state<=9) { dat->last_talk=0; ppd->kelly_state=6; }
						if (ppd && ppd->kelly_state>=10 && ppd->kelly_state<=13) { dat->last_talk=0; ppd->kelly_state=10; }
						if (ppd && ppd->kelly_state>=14 && ppd->kelly_state<=15) { dat->last_talk=0; ppd->kelly_state=14; }
						if (ppd && ppd->kelly_state>=17 && ppd->kelly_state<=19) { dat->last_talk=0; ppd->kelly_state=17; }
						if (ppd && ppd->kelly_state>=21 && ppd->kelly_state<=25) { dat->last_talk=0; ppd->kelly_state=21; }
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

                                if (it[in].ID==IID_AREA2_CREEPERHEAD && ppd->kelly_state<=5) {
					int tmp;
					quiet_say(cn,"Ah. Well done, %s.",ch[co].name);
					ppd->kelly_state=6;

					tmp=questlog_done(co,13);
					destroy_item_byID(co,IID_AREA2_CREEPERHEAD);

					if (tmp==1) give_military_pts(cn,co,4,1);

					// let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				} else if (it[in].ID==IID_CALIGARPLAQUE && ppd->kelly_state==25) {
					
					questlog_done(co,60);
					
					quiet_say(cn,"Oh thank you so much, %s! I don't think I can ever repay you for your effort. However, I can give you these 5,000 gold coins.",ch[co].name);
					ch[co].gold+=5000*100; stats_update(co,0,5000*100);
					ch[co].flags|=CF_ITEMS;
					
					ppd->kelly_state=26;

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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

//----------------

struct astro1_driver_data
{
        int last_talk;
	int state;	
};

void astro1_driver(int cn,int ret,int lastact)
{
	struct astro1_driver_data *dat;
        struct msg *msg,*next;

        dat=set_data(cn,DRD_ASTRO1DRIVER,sizeof(struct astro1_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
        if (ticker>dat->last_talk+TICKS*10) {
		switch(dat->state) {
			case 0:		quiet_say(cn,"The moon, oh so bright and splendid it seemed. Oh, yes.");
					dat->state++; break;
			case 1:		quiet_say(cn,"From my starting point, I moved the telescope to the south-east.");
					dat->state++; break;
			case 2:		quiet_say(cn,"Then I noticed two triangle-shaped boulders. I continued moving south-east.");
					dat->state++; break;
			case 3:		quiet_say(cn,"Some time later, there were one triangle-shaped boulder and one round like a circle. There, I started moving the telescope south-west.");
					dat->state++; break;
			case 4:		quiet_say(cn,"A triangle-shaped boulder there was, and a square one. I turned the telescope south-east again.");
					dat->state++; break;
			case 5:		quiet_say(cn,"Soon after, the perfectly round boulders caught my eye. Perfect circles. I marvelled at their sight - and got hungry, so I stopped to get some food.");
					dat->state++; break;
			case 6:		quiet_say(cn,"After I got the food, I went back to the triangle-shaped and the square boulder. This time, I moved the telescope south-west.");
					dat->state++; break;
			case 7:		quiet_say(cn,"There, I saw two boulders, both perfectly round, like circles. I continued moving south-west.");
					dat->state++; break;
			case 8:		quiet_say(cn,"I moved the telescope past a triangle-shaped boulder, still going south-west.");
					dat->state++; break;
			case 9:		quiet_say(cn,"At the sight of a round boulder, I started moving the telescope south-east.");
					dat->state++; break;
			case 10:	quiet_say(cn,"After looking some more, I spotted the perfect squares. From there on, I moved north-east.");
					dat->state++; break;
			case 11:	quiet_say(cn,"Some time later, I noticed another square and turned south-east again.");
					dat->state++; break;
			case 12:	quiet_say(cn,"But then I got interrupted by my colleague, who wanted to have a heated discussion about something I don't remember. I offered him some food to quiet him and continued my observations.");
					dat->state++; break;
			case 13:	quiet_say(cn,"And there it was, the very thing I was looking for. Oh, it was so beautiful!");
					dat->state++; break;
			case 14:	quiet_say(cn,"Now what did I say last? I better start over at the beginning!");
					dat->state=0; break;
			default:	dat->state=0;
		}
		dat->last_talk=ticker;
	}

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		

	do_idle(cn,TICKS);
}


struct astro2_driver_data
{
        int last_talk;
	int current_victim;
};

void astro2_driver(int cn,int ret,int lastact)
{
	struct astro2_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_ASTRO2DRIVER,sizeof(struct astro2_driver_data));
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
				switch(ppd->astro2_state) {
					case 0:		say(cn,"Ah. Hello %s. I am %s, the astronomer.",ch[co].name,ch[cn].name);
							questlog_open(co,16);
							ppd->astro2_state++; didsay=1;
							break;
					case 1:		say(cn,"Me and my colleagues, we've been watching the moon from our big telescope in the garden south-east of here.");
							ppd->astro2_state++; didsay=1;
							break;
					case 2:		say(cn,"But a few days ago, some strange creatures invaded the garden, and drove us away. We had to leave our notes behind.");
							ppd->astro2_state++; didsay=1;
							break;
					case 3:		say(cn,"Could thou try to get those notes back? I'd... well, I'd pay thee handsomely!");
							ppd->astro2_state++; didsay=1;
							break;
					case 4:		break;
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
				case 2:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
						if (ppd && ppd->astro2_state<=4) ppd->astro2_state=0;
						break;
			}
			if (didsay) {
				dat->last_talk=ticker;
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				
				ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));

                                if (it[in].ID==IID_AREA2_ASTRONOTE && ppd->astro2_state<=4) {
					int tmp;
					say(cn,"Oh, jolly good! Thou gotst them back.");
					ppd->astro2_state=5;

                                        // let it vanish, then
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;

					tmp=questlog_done(co,16);
					destroy_item_byID(co,IID_AREA2_ASTRONOTE);

					if (tmp==1) {
						in=create_money_item(MONEY_AREA3_MOONIES);
						if (!give_char_item(co,in)) destroy_item(in);
					}
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

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

struct thomas_driver_data
{
        int last_talk;
	int current_victim;
};

void thomas_driver(int cn,int ret,int lastact)
{
	struct thomas_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_THOMASDRIVER,sizeof(struct thomas_driver_data));
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
				switch(ppd->crypt_state) {
					case 0:		if (ch[co].level>18) {
								say(cn,"Be greeted, %s. Please go inside, my master wishes to talk to thee.",ch[co].name);
								ppd->crypt_state++; didsay=1;
							}
							break;
					case 1:         break;					

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
						if (ppd && ppd->crypt_state<=1) ppd->crypt_state=0;
						break;
			}
			if (didsay) {
				dat->last_talk=ticker;
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

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

struct sir_jones_driver_data
{
        int last_talk;
	int current_victim;
};

void sir_jones_driver(int cn,int ret,int lastact)
{
	struct sir_jones_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SIRJONESDRIVER,sizeof(struct sir_jones_driver_data));
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

			//say(cn,"state=%d, bonus=%d",ppd->crypt_state,ppd->crypt_bonus);
			//ppd->crypt_state=10;

                        if (ppd) {
				switch(ppd->crypt_state) {
					case 0:         break;
					case 1:         say(cn,"Welcome to my humble home, %s.",ch[co].name);
							questlog_open(co,18);
							ppd->crypt_state++; didsay=1;
                                                        break;
					case 2:         say(cn,"Thou lookst like a tough %s. I guess thou wouldst be interested to hear about a fabulous opportunity.",(ch[co].flags&CF_WARRIOR) ? "warrior" : "mage");
							ppd->crypt_state++; didsay=1;
                                                        break;
					case 3:         say(cn,"One of my clerks found hints about a huge crypt located below the Aston graveyard. Being the coward he is, he rejected to explore it himself, though.");
							ppd->crypt_state++; didsay=1;
                                                        break;
					case 4:         say(cn,"Would thou be willing to go? Say °c4Aye°c0 or °c4Nay°c0!");
							ppd->crypt_state++; didsay=1;
                                                        break;
					case 5:		break;	// waiting for answer
					case 6:		say(cn,"And if I offered thee 30 gold pieces as reward? °c4Aye°c0 or °c4Nay°c0, %s!",ch[co].name);
							ppd->crypt_state++; didsay=1;
                                                        break;
					case 7: 	break;	// waiting for answer
					case 8:		say(cn,"Jolly good. I expect thee to slay the toughest creature thou canst find down there. Have a nice day, %s.",ch[co].name);
							ppd->crypt_state++; didsay=1;
							break;
					case 9:		break; // waiting for player to solve quest
					case 10:	say(cn,"It seems thou foundst quite a challenge down there. Well done, %s.",ch[co].name);
							ppd->crypt_state++; didsay=1;
							if (ppd->crypt_bonus && questlog_count(co,18)==1) {
								in=create_money_item(MONEY_AREA3_VAMPIRE1);
								if (!give_char_item(co,in)) destroy_item(in);
							}
					case 11:	// fall through intended
							ppd->crypt_state++;
							break;
					case 12:	if (questlog_isdone(co,19)) { ppd->crypt_state=14; break; }
							say(cn,"I have heard rumors that there is an even tougher creature down there, %s.",ch[co].name);
							questlog_open(co,19);
							ppd->crypt_state++;
							break;
					case 13:	say(cn,"I don't believe in these rumors, but it is said that thou canst gain entry to its lair by walking through the wall in the western corner of the Vampire Lords room.");
							ppd->crypt_state++;
							break;
					case 14:	break; // waiting for quest to be done
					case 15:	break; // quest is done

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
						if (ppd && ppd->crypt_state>=1 && ppd->crypt_state<=5) ppd->crypt_state=1;
						if (ppd && ppd->crypt_state>=12 && ppd->crypt_state<=14) ppd->crypt_state=12;
						break;
				case 3:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
						if (ppd && ppd->crypt_state>=1 && ppd->crypt_state<=5) ppd->crypt_state=8;
                                                if (ppd && ppd->crypt_state>=6 && ppd->crypt_state<=7) { ppd->crypt_state=8; ppd->crypt_bonus=1; }
						break;
				case 4:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
						if (ppd && ppd->crypt_state>=1 && ppd->crypt_state<=5) ppd->crypt_state=6;
						break;
			}
			if (didsay) {
				dat->last_talk=ticker;
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

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

struct carlos_driver_data
{
        int last_talk;
	int current_victim;
};

void carlos_driver(int cn,int ret,int lastact)
{
	struct carlos_driver_data *dat;
	struct staffer_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_CARLOSDRIVER,sizeof(struct carlos_driver_data));
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
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

			//say(cn,"state=%d",ppd->carlos_state);

                        if (ppd) {
				switch(ppd->carlos_state) {
                                        case 0:         quiet_say(cn,"Hello, %s. I am %s, Chief Investigator of the Occult in the Imperial Army.",ch[co].name,ch[cn].name);
							ppd->carlos_state++; didsay=1;
							questlog_open(co,20);
                                                        break;
					case 1:         quiet_say(cn,"I need thy help to investigate a strange noise coming from below the crypt. 200 years ago some strange creatures were found.");
							ppd->carlos_state++; didsay=1;
                                                        break;
					case 2:         quiet_say(cn,"I sent my top army down to kill theses creatures and take away a special Staff. The staff had magical properties that allowed these creatures to live.");
							ppd->carlos_state++; didsay=1;
                                                        break;
					case 3:         quiet_say(cn,"My army captured the head creature and brought his staff to me. It has been brought to my attention that the staff has gone missing.");
							ppd->carlos_state++; didsay=1;
                                                        break;
                                        case 4:		quiet_say(cn,"Please go below the crypt and find out what the strange noise is and if thou findst the staff bring it back to me.");
							ppd->carlos_state++; didsay=1;
							if (!has_item(co,IID_CARLOS_DOOR)) {
								in=create_item("carlos_key");
								if (give_char_item(co,in)) {
									quiet_say(cn,"Thou wilt need this key to unlock the door in front of the stairs down.");
								} else destroy_item(in);
							}
                                                        break;
					case 5: 	break;	// waiting for staff
					case 6:		break; // got staff... waiting forever here.

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
				case 2:		ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
						if (ppd && ppd->carlos_state>=0 && ppd->carlos_state<=5) ppd->carlos_state=0;
                                                break;
			}
			if (didsay) {
				dat->last_talk=ticker;
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
				if (ppd && ppd->carlos_state<=5 && it[in].ID==IID_STAFF_DRAGONSTAFF) {
					ppd->carlos_state=6;
					quiet_say(cn,"Well done, %s, that is the staff I wanted.",ch[co].name);
					questlog_done(co,20);
					destroy_item_byID(co,IID_STAFF_DRAGONSTAFF);
					destroy_item_byID(co,IID_STAFF_DRAGONKEY1);
					destroy_item_byID(co,IID_STAFF_DRAGONKEY2);
					destroy_item_byID(co,IID_STAFF_DRAGONKEY3);
					destroy_item_byID(co,IID_STAFF_DRAGONKEY4);
				} else {
					say(cn,"Thou hast better use for this than I do. Well, if there is a use for it at all.");
                                        if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
                                        ch[cn].citem=0;
				}
				
                                // let it vanish, then
				if (ch[cn].citem) {
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;
	}
	
	do_idle(cn,TICKS);
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

#define MAXLAMP		250
struct lamp
{
	int in;
	int cn;
	int cost;
};

struct lamp lamp[MAXLAMP];

void add_lamp(int in)
{
	int n;

	for (n=1; n<MAXLAMP; n++)
		if (!lamp[n].in) break;

	if (n==MAXLAMP) {
		elog("PANIC: too many lamps!");
		return;
	}

	lamp[n].in=in;
	lamp[n].cn=0;
}

struct lampghost_driver_data
{
	int ln;
};

void lampghost_driver(int cn,int ret,int lastact)
{
	struct lampghost_driver_data *dat;
        struct msg *msg,*next;
	int n,in,cost,bestcost=99999,bestn=0;

        dat=set_data(cn,DRD_LAMPGHOSTDRIVER,sizeof(struct lampghost_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,80,0,80);		
                        //sprintf(ch[cn].name,"%d/%d",cn,dat->nr);
                }

                standard_message_driver(cn,msg,1,0);
		remove_message(cn,msg);
	}

	ch[cn].speed_mode=SM_NORMAL;
	
        fight_driver_update(cn);
	
	if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;
	
	ch[cn].speed_mode=SM_STEALTH;

	if (spell_self_driver(cn)) return;
	if (regenerate_driver(cn)) return;

	if (dat->ln) {	// we got a job?
		in=lamp[dat->ln].in;
		if (!it[in].drdata[0]) {	// its already off, remove it
			lamp[dat->ln].cn=0;
			dat->ln=0;
		}
		if (lamp[dat->ln].cn!=cn) {	// somebody else took it
			dat->ln=0;
		}
	}

	if (!dat->ln) {
		for (n=1; n<MAXLAMP; n++) {
			if (!(in=lamp[n].in)) continue;
			if (!it[in].drdata[0]) continue;
			
			// guess cost
			cost=map_dist(ch[cn].x,ch[cn].y,it[in].x,it[in].y);
			if (cost>=bestcost) continue;
			if (lamp[n].cn && cost>=lamp[n].cost) continue;

                        if (cost<bestcost) {
				bestcost=cost;
				bestn=n;
			}
		}
		if (bestn) {		
			lamp[bestn].cn=cn;
			lamp[bestn].cost=bestcost;
			dat->ln=bestn;
		}
	}

	if (dat->ln) {
		in=lamp[dat->ln].in;
                if (use_driver(cn,in,0)) return;		
	}

        if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;

	do_idle(cn,TICKS);
}

int lampghost_respawn(int cn)
{
	int m;

        m=ch[cn].tmpx+ch[cn].tmpy*MAXMAP;

        if (map[m].light>4) return 2;

	return 1;
}

void lampghost_dead(int cn,int co)
{
	struct lampghost_driver_data *dat;
	
	dat=set_data(cn,DRD_LAMPGHOSTDRIVER,sizeof(struct lampghost_driver_data));
	if (!dat) return;	// oops...

	if (dat->ln && lamp[dat->ln].cn==cn) lamp[dat->ln].cn=0;
}

static int on=0,off=0;
static int keepopen=0;

void gate_driver(int in,int cn)
{
	int m;

	if (cn) return;

	call_item(it[in].driver,in,0,ticker+TICKS*10);

	m=it[in].x+it[in].y*MAXMAP;

        if (keepopen>realtime) {
		if (it[in].drdata[0]) return;	// already open
		
                // it is closed, open
		remove_lights(it[in].x,it[in].y);

		*(unsigned long long*)(it[in].drdata+30)=it[in].flags&(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		it[in].flags&=~(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		map[m].flags&=~(MF_TMOVEBLOCK|MF_TSIGHTBLOCK|MF_DOOR|MF_TSOUNDBLOCK);
		it[in].drdata[0]=1;
		it[in].sprite++;
        } else {
		if (!it[in].drdata[0]) return;	// already closed
		
		if (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) return;	// doorway is blocked
		
		// it is open, close
		remove_lights(it[in].x,it[in].y);

                it[in].flags|=*(unsigned long long*)(it[in].drdata+30);
		if (it[in].flags&IF_MOVEBLOCK) map[m].flags|=MF_TMOVEBLOCK;
		if (it[in].flags&IF_SIGHTBLOCK) map[m].flags|=MF_TSIGHTBLOCK;
		if (it[in].flags&IF_SOUNDBLOCK) map[m].flags|=MF_TSOUNDBLOCK;
		if (it[in].flags&IF_DOOR) map[m].flags|=MF_DOOR;
		it[in].drdata[0]=0;
		it[in].sprite--;
	}

	reset_los(it[in].x,it[in].y);
	if (!it[in].drdata[38] && !reset_dlight(it[in].x,it[in].y)) it[in].drdata[38]=1;
	add_lights(it[in].x,it[in].y);

}
void extinguish_lamps(void)
{
	int n;

	for (n=1; n<MAXLAMP; n++) {
		call_item(IDR_ONOFFLIGHT,lamp[n].in,0,ticker+n);
	}
}

void open_gates(int cn)
{
	keepopen=realtime+60*3;
}

void onofflight_driver(int in,int cn)
{
	int light;

	if (!cn) {	// automatic call
		if (!it[in].drdata[0]) return;	// no auto-lighting

		if (!it[in].drdata[6]) {	// add to list if not already there - done on auto-call
			add_lamp(in);
			it[in].drdata[6]=1;
			return;
		}
	}

        if (it[in].drdata[0]) {
		
		remove_item_light(in);

                it[in].drdata[0]=0;
                it[in].mod_value[0]=0;
		it[in].sprite--;
		
                off++;

                return;
	}

	if (!it[in].drdata[0]) {

		light=it[in].drdata[1];

		it[in].drdata[0]=1;
		it[in].mod_value[0]=light;
		it[in].sprite++;
		
		add_item_light(in);

		on++;
		
		if (off-on==0) {
			log_char(cn,LOG_SYSTEM,0,"The light has returned to the palace and the gates open.");
			open_gates(cn);
			extinguish_lamps();
		} else log_char(cn,LOG_SYSTEM,0,"%d remaining",off-on);
		
		return;
	}
}

void gatekeeper_door_driver(int in,int cn)
{
	int x,y,oldx,oldy,dx,dy,co;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
		it[in].max_level=it[in].drdata[1];
		return;
	}

	dx=(ch[cn].x-it[in].x);
	dy=(ch[cn].y-it[in].y);

	if (dx && dy) return;

	if (it[in].drdata[0]==1 && dx==1) return;
	if (it[in].drdata[0]==2 && dx==-1) return;
	if (it[in].drdata[0]==3 && dy==1) return;
	if (it[in].drdata[0]==4 && dy==-1) return;

        if (has_spell(cn,IDR_HEAL)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot use this door while heal is active.");
		return;
	}

	x=it[in].x-dx;
	y=it[in].y-dy;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

        if (!it[in].drdata[6]) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a magic object and vanishes.",ch[cn].name);

	oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char(cn,x,y,0)) {
		// door is blocked and its a door with level restriction --> clan spawner door
		if (it[in].drdata[1] && drop_char_extended(cn,x,y,7)) {
			;
		} else if (it[in].drdata[1] && (co=map[x+y*MAXMAP].ch) && (ch[co].flags&CF_PLAYER) && ch[co].level<=it[in].drdata[1]) {
			remove_char(co); ch[co].action=ch[co].step=ch[co].duration=0;
			if (!set_char(co,oldx,oldy,0)) exit_char_player(co);
			if (!set_char(cn,x,y,0)) exit_char_player(cn);
		} else {
			log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
			drop_char(cn,oldx,oldy,0);
		}
	}

	switch(ch[cn].dir) {
		case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
		case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
		case DX_UP:	ch[cn].dir=DX_DOWN; break;
		case DX_DOWN:	ch[cn].dir=DX_UP; break;
	}
	if (it[in].drdata[1]) update_char(cn);	// clan spawner door, update char since we've entered/left a clan area

	if (!it[in].drdata[6]) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s pops in.",ch[cn].name);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_SEYMOUR:	seymour_driver(cn,ret,lastact); return 1;
		case CDR_LAMPGHOST:	lampghost_driver(cn,ret,lastact); return 1;
		case CDR_KELLY:		kelly_driver(cn,ret,lastact); return 1;
		case CDR_ASTRO1:	astro1_driver(cn,ret,lastact); return 1;
		case CDR_ASTRO2:	astro2_driver(cn,ret,lastact); return 1;
		case CDR_THOMAS:	thomas_driver(cn,ret,lastact); return 1;
		case CDR_SIRJONES:	sir_jones_driver(cn,ret,lastact); return 1;
		case CDR_CARLOS:	carlos_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_ONOFFLIGHT:	onofflight_driver(in,cn); return 1;
		case IDR_PALACEGATE:	gate_driver(in,cn); return 1;
		case IDR_GATEKEEPER:	gatekeeper_door_driver(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_SEYMOUR:	immortal_dead(cn,co); return 1;
		case CDR_LAMPGHOST:	lampghost_dead(cn,co); return 1;
		case CDR_KELLY:		immortal_dead(cn,co); return 1;
		case CDR_ASTRO1:	immortal_dead(cn,co); return 1;
		case CDR_ASTRO2:	immortal_dead(cn,co); return 1;
		case CDR_THOMAS:	immortal_dead(cn,co); return 1;
		case CDR_SIRJONES:	immortal_dead(cn,co); return 1;
		case CDR_CARLOS:	immortal_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_SEYMOUR:	return 1;
		case CDR_KELLY:		return 1;
		case CDR_LAMPGHOST:	return lampghost_respawn(cn);
		case CDR_ASTRO1:	return 1;
		case CDR_ASTRO2:	return 1;
		case CDR_THOMAS:	return 1;
		case CDR_SIRJONES:	return 1;
		case CDR_CARLOS:	return 1;

		default:		return 0;
	}
}










