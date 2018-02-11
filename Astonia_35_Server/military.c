/*
 * $Id: military.c,v 1.11 2007/09/21 11:01:20 devel Exp $
 *
 * $Log: military.c,v $
 * Revision 1.11  2007/09/21 11:01:20  devel
 * raiseme gives 100 pts now
 *
 * Revision 1.10  2007/08/13 18:52:01  devel
 * fixed some warnings
 *
 * Revision 1.9  2007/08/09 11:13:36  devel
 * statistics
 *
 * Revision 1.8  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.7  2007/07/09 11:23:31  devel
 * *** empty log message ***
 *
 * Revision 1.6  2007/06/07 15:04:10  devel
 * changed max level for pent missions to 102
 *
 * Revision 1.5  2007/05/02 12:33:52  devel
 * fixed minor logging bug
 *
 * Revision 1.4  2007/04/12 11:52:19  devel
 * increased EXP per mission
 *
 * Revision 1.3  2007/02/26 14:32:31  devel
 * small bugfix
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
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
#include "sector.h"
#include "item_id.h"
#include "libload.h"
#include "drvlib.h"
#include "create.h"
#include "clan.h"
#include "lookup.h"
#include "task.h"
#include "database.h"
#include "mem.h"
#include "military.h"
#include "consistency.h"
#include "chat.h"
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
	{{"help",NULL},"Sorry, I'm just a merchant, %s!",0},
	{{"what's","up",NULL},"Everything that isn't nailed down.",0},
	{{"what","is","up",NULL},"Everything that isn't nailed down.",0},
        {{"what's","your","name",NULL},NULL,1},
	{{"what","is","your","name",NULL},NULL,1},
        {{"who","are","you",NULL},NULL,1},
	{{"repeat",NULL},NULL,2},
        {{"favor",NULL},NULL,3},
	{{"small",NULL},NULL,4},
	{{"medium",NULL},NULL,5},
	{{"big",NULL},NULL,6},
	{{"huge",NULL},NULL,7},
	{{"vast",NULL},NULL,8},
	{{"pay",NULL},NULL,9},
	{{"mission",NULL},NULL,10},
	{{"very","easy",NULL},NULL,11},
	{{"easy",NULL},NULL,12},
	{{"normal",NULL},NULL,13},
	{{"hard",NULL},NULL,14},
	{{"very","hard",NULL},NULL,15},
	{{"impossible",NULL},NULL,16},
	{{"insane",NULL},NULL,17},
	{{"failed",NULL},NULL,18},
	{{"hear",NULL},NULL,19},
        {{"reset",NULL},NULL,20},
        {{"promote",NULL},NULL,21},
	{{"finish",NULL},NULL,22}
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
				//quiet_say(cn,"word = '%s'",wordlist[n]);
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

struct military_master_data
{
	int current_victim;
        int last_save;
};

static char *diff_name[7]={
	"very easy",
	"easy",
	"normal",
	"hard",
	"very hard",
	"impossible",
	"insane"
};

int maxpentlevel=102;

// base mission - must generate a mission all the time
void generate_demon_mission(int lvl,struct military_ppd *ppd)
{
	int step;

	if (lvl<20) step=1;
	else if (lvl<50) step=2;
	else step=3;

        ppd->mis[0].type=1;						// slay pent demon
	ppd->mis[0].opt1=75+RANDOM(126);				// 75 to 200 of them
	ppd->mis[0].opt2=min(maxpentlevel,lvl-step*3);			// at level lvl-3
	ppd->mis[0].pts=7;						// worth one military point
	ppd->mis[0].exp=ppd->mis[0].opt1*ppd->mis[0].opt2;		// number of demons * demonlevel / 2

	ppd->mis[1].type=1;			
	ppd->mis[1].opt1=75+RANDOM(126);	
	ppd->mis[1].opt2=min(maxpentlevel,lvl-step*2);
	ppd->mis[1].pts=8;			
	ppd->mis[1].exp=ppd->mis[1].opt1*ppd->mis[1].opt2;

	ppd->mis[2].type=1;			
	ppd->mis[2].opt1=75+RANDOM(126);	
	ppd->mis[2].opt2=min(maxpentlevel,lvl-step*1);
	ppd->mis[2].pts=9;			
	ppd->mis[2].exp=ppd->mis[2].opt1*ppd->mis[2].opt2;

	ppd->mis[3].type=1;			
	ppd->mis[3].opt1=75+RANDOM(126);	
	ppd->mis[3].opt2=min(maxpentlevel,lvl+step*0);
	ppd->mis[3].pts=10;			
	ppd->mis[3].exp=ppd->mis[3].opt1*ppd->mis[3].opt2;

	ppd->mis[4].type=1;				
	ppd->mis[4].opt1=75+RANDOM(126);		
	ppd->mis[4].opt2=min(maxpentlevel,lvl+step*1);	
	ppd->mis[4].pts=11;			
	ppd->mis[4].exp=ppd->mis[4].opt1*ppd->mis[4].opt2;

	ppd->mis[5].type=1;				
	ppd->mis[5].opt1=75+RANDOM(126);		
	ppd->mis[5].opt2=min(maxpentlevel,lvl+step*2);	
	ppd->mis[5].pts=12;			
	ppd->mis[5].exp=ppd->mis[5].opt1*ppd->mis[5].opt2;

	ppd->mis[6].type=1;				
	ppd->mis[6].opt1=75+RANDOM(126);		
	ppd->mis[6].opt2=min(maxpentlevel,lvl+step*3);	
	ppd->mis[6].pts=13;			
	ppd->mis[6].exp=ppd->mis[6].opt1*ppd->mis[6].opt2;
}

// additional misison - may overwrite one or more
void generate_mine_mission(int lvl,struct military_ppd *ppd)
{
	double level;

	level=min(maxpentlevel,lvl);
	level=level/10.0;

	// starts at level 12, open end
	switch(RANDOM(5)) {
		case 0:
			if (lvl<12) break;
                        ppd->mis[0].type=3;						// find silver
			ppd->mis[0].opt1=(int)((10+RANDOM(11))*level);
			ppd->mis[0].opt2=0;						// no option
			ppd->mis[0].pts=1;						// worth one military point
			ppd->mis[0].exp=ppd->mis[0].opt1;
			break;

		case 1:
			if (lvl<12) break;
			ppd->mis[1].type=3;						// find silver
			ppd->mis[1].opt1=(int)((50+RANDOM(51))*level);
			ppd->mis[1].opt2=0;						// no option
			ppd->mis[1].pts=2;						// worth 2 military points
			ppd->mis[1].exp=ppd->mis[1].opt1;
			break;
		
		case 2:
			if (lvl<12) break;
			ppd->mis[2].type=3;						// find silver
			ppd->mis[2].opt1=(int)((250+RANDOM(251))*level);
			ppd->mis[2].opt2=0;						// no option
			ppd->mis[2].pts=4;						// worth 4 military points
			ppd->mis[2].exp=ppd->mis[2].opt1;
			break;
		
		case 3:	
			lvl++;
			if (lvl<12) break;
			ppd->mis[3].type=3;						// find silver
			ppd->mis[3].opt1=(int)((1000+RANDOM(1001))*level);
			ppd->mis[3].opt2=0;						// no option
			ppd->mis[3].pts=10;						// worth 10 military points
			ppd->mis[3].exp=ppd->mis[3].opt1;				// normal experience worth
			break;
		
		case 4:	
			lvl+=2;
			if (lvl<12) break;
			ppd->mis[4].type=3;						// find silver
			ppd->mis[4].opt1=(int)((2000+RANDOM(2001))*level);
			ppd->mis[4].opt2=0;						// no option
			ppd->mis[4].pts=25;						// worth 25 military points
			ppd->mis[4].exp=ppd->mis[4].opt1;
			break;
	}
}

void generate_mission(int cn,struct military_ppd *ppd)
{
	int lvl,rank;

	// adjust military exp for rank if the player gained a rank elsewhere
	rank=get_army_rank_int(cn);
	if (rank*rank*rank>ppd->military_pts) ppd->military_pts=rank*rank*rank;

	lvl=cbrt(ppd->military_pts)*5+5;
	if (lvl<7) lvl=7;

	generate_demon_mission(lvl,ppd);
        //generate_mine_mission(lvl,ppd);

	ppd->mission_yday=yday+1;
}

void offer_missions(int cn,int co,struct military_ppd *ppd)
{
	int n;

	for (n=0; n<7; n++) {
		//if (ppd->mis[n].pts>1 && ppd->mis[n].pts>ppd->current_pts) continue;

		switch(ppd->mis[n].type) {
			case 1:		say(cn,"I have an °c4%s°c0 mission for you, %s. It is to slay %d level %d demons in the Pentagram Quest.",
                                            diff_name[n],
					    ch[co].name,
					    ppd->mis[n].opt1,
					    ppd->mis[n].opt2);
					break;
			case 2:		say(cn,"I have an °c4%s°c0 mission for you, %s. It is to slay %d level %d ratlings in the Sewers.",
                                            diff_name[n],
					    ch[co].name,
					    ppd->mis[n].opt1,
					    ppd->mis[n].opt2);
					break;
			case 3:		say(cn,"I have an °c4%s°c0 mission for you, %s. It is to find %d units of silver in the Mine.",
                                            diff_name[n],
					    ch[co].name,
					    ppd->mis[n].opt1);
					break;
		}
	}
}

void display_mission(int cn,int co,int nr,struct military_ppd *ppd)
{
        switch(ppd->mis[nr].type) {
		case 1:		say(cn,"Your mission is to slay %d level %d demons in the Pentagram Quest.",
				    ppd->mis[nr].opt1,
				    ppd->mis[nr].opt2);				
				break;	
		case 2:		say(cn,"Your mission is to slay %d level %d ratlings in the Sewers.",
				    ppd->mis[nr].opt1,
				    ppd->mis[nr].opt2);				
				break;
		case 3:		say(cn,"Your mission is to find %d units of silver in the Mine.",
				    ppd->mis[nr].opt1);
				break;
	}
}

void military_master_driver(int cn,int ret,int lastact)
{
        struct military_master_data *dat;
	struct military_ppd *ppd;
	int co,res,n,rank,pts;
        struct msg *msg,*next;	

        dat=set_data(cn,DRD_MILITARYMASTER,sizeof(struct military_master_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
                                ch[cn].arg=NULL;
			}
                }

		// did we see someone?
		if (msg->type==NT_CHAR) {
                        co=msg->dat1;
                        if (char_dist(cn,co)>10 || !(ch[co].flags&CF_PLAYER) || !char_see_char(cn,co)) {
				remove_message(cn,msg);
				continue;
			}			
			if (!(ppd=set_data(co,DRD_MILITARY_PPD,sizeof(struct military_ppd)))) {
				remove_message(cn,msg);
				continue;
			}
                        if (ppd->master_state==0) {
				if (ppd->took_mission) {
					say(cn,"Ah, hello %s. Any luck with your mission? Or would you like to °c4hear°c0 it again? Or have you °c4failed°c0 to complete it?",ch[co].name);
					ppd->master_state=2;
				} else if (ppd->solved_yday==yday+1) {
					say(cn,"I don't have another mission for you today, %s.",ch[co].name);
					ppd->master_state=2;
				} else if (get_army_rank_int(co)) {
					say(cn,"Hello, %s. I might have a °c4mission°c0 for you.",ch[co].name);
					ppd->master_state=2;
				} else {
					say(cn,"Greetings, %s.",ch[co].name);
					ppd->master_state=1;
				}			
			} else if (ppd->master_state==1 && get_army_rank_int(co)) {
					say(cn,"Hello again, %s. I might have a °c4mission°c0 for you.",ch[co].name);
					ppd->master_state=2;
			}
			if (ppd->solved_mission) {
                                ppd->solved_mission=0;
				ppd->solved_yday=ppd->took_yday;
				ppd->took_yday=0;
				n=ppd->took_mission-1;
				ppd->took_mission=0;
				give_exp_bonus(co,ppd->mis[n].exp);

				if (ch[co].prof[P_MERCENARY]) {
					int pay;

					pay=(ppd->mis[n].exp/5+2000)*ch[co].prof[P_MERCENARY]/6;
					ch[co].gold+=pay; stats_update(co,0,pay);
					ch[co].flags|=CF_ITEMS;
					say(cn,"Well done, %s. You've solved your mission! Your pay is %.2fG.",ch[co].name,pay/100.0);
				} else {
					say(cn,"Well done, %s. You've solved your mission!",ch[co].name);
				}

				pts=ppd->mis[n].pts;
				ppd->military_pts+=pts;
				dlog(co,0,"mil exp=%d from governor",pts);

				ppd->normal_exp+=ppd->mis[n].exp;

				rank=cbrt(ppd->military_pts);
				if (rank<25 && rank>get_army_rank_int(co)) {
					char buf[256];

					set_army_rank(co,rank);
					say(cn,"You've been promoted to %s. Congratulations, %s!",get_army_rank_string(co),ch[co].name);
					if (get_army_rank_int(co)>9) {
						sprintf(buf,"0000000000°c10Grats: %s is a %s now!",ch[co].name,get_army_rank_string(co));
						server_chat(6,buf);
					}
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			if (!(ch[co].flags&CF_PLAYER) || !(ppd=set_data(co,DRD_MILITARY_PPD,sizeof(struct military_ppd)))) {
				remove_message(cn,msg);
				continue;
			}
			res=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

			switch(res) {
				case 2:		ppd->master_state=0; break;
				case 10:	if (ppd->took_mission) {
							say(cn,"You already have a mission. Would you like to °c4hear°c0 it again?");
							break;
						}
						if (ppd->solved_yday==yday+1) {
							say(cn,"I don't have another mission for you today, %s.",ch[co].name);
							break;
						}
						if (!get_army_rank_int(co)) {
							say(cn,"But you don't even belong to the army, %s. Talk to Seymour about enrollment.",ch[co].name);
							break;
						}
						if (ppd->mission_yday!=yday+1) generate_mission(co,ppd);
						offer_missions(cn,co,ppd);
						break;
				case 11:	
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				case 17:	if (ppd->took_mission) {
							say(cn,"You already have a mission, %s. Would you like to °c4hear°c0 it again?",ch[co].name);
							break;
						}
						if (ppd->solved_yday==yday+1) {
							say(cn,"I don't have another mission for you today, %s.",ch[co].name);
							break;
						}
						if (ppd->mission_yday!=yday+1) {
							say(cn,"I haven't offered you that kind of mission today, %s.",ch[co].name);
							break;
						}
						display_mission(cn,co,res-11,ppd);
						ppd->took_mission=res-10;
						ppd->took_yday=yday+1;
						break;
				case 18:	if (!ppd->took_mission) {
							say(cn,"But you did not take any °c4mission°c0, %s.",ch[co].name);
							break;
						}
						say(cn,"So, you failed? Well, %s, I'll remove that mission from your record. Would you like to get another °c4mission°c0?",get_army_rank_string(co));
						ppd->took_mission=0;
						break;
				case 19:	if (!ppd->took_mission) {
						     say(cn,"But you do not have a °c4mission°c0 yet, %s.",get_army_rank_string(co));
						}
						display_mission(cn,co,ppd->took_mission-1,ppd);
						break;
				case 20:	if (!(ch[co].flags&CF_GOD)) break;
                                                ppd->solved_yday=0;
						ppd->mission_yday=0;
						ppd->took_mission=0;
						break;
				case 21:	if (!(ch[co].flags&CF_GOD)) break;
						give_military_pts(cn,co,100,1);
						break;
				case 22:	if (!(ch[co].flags&CF_GOD)) break;
						ppd->solved_mission=1;
						break;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			// didnt work, let it vanish, then
			destroy_item(ch[cn].citem);
			ch[cn].citem=0;
			say(cn,"That's junk.");
		}
		
		standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

        // ------------------

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

        do_idle(cn,TICKS/2);
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I DIED! IMMORTAL!");
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_MILITARY_MASTER:	military_master_driver(cn,ret,lastact); return 1;

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
		case CDR_MILITARY_MASTER:	immortal_dead(cn,co); return 1;
		
		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_MILITARY_MASTER:	return 1;

		default:		return 0;
	}
}












