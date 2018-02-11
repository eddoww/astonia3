/*

$Id: military.c,v 1.3 2007/12/10 10:11:54 devel Exp $

$Log: military.c,v $
Revision 1.3  2007/12/10 10:11:54  devel
fixed logging

Revision 1.2  2007/08/13 18:50:38  devel
fixed some warnings

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.6  2004/05/26 09:23:38  sam
increaded military pts for all missions by 50%

Revision 1.5  2004/02/18 10:57:47  sam
fixed bug with warlord rank being inaccessible

Revision 1.4  2003/11/27 13:53:19  sam
fixed spelling error

Revision 1.3  2003/11/19 16:23:10  sam
made military ranks>24 (warlord) unaccessible

Revision 1.2  2003/10/13 14:12:26  sam
Added RCS tags


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
	{{"easy",NULL},NULL,11},
	{{"normal",NULL},NULL,12},
	{{"hard",NULL},NULL,13},
	{{"impossible",NULL},NULL,14},
	{{"insane",NULL},NULL,15},
	{{"failed",NULL},NULL,16},
	{{"hear",NULL},NULL,17},
	{{"info",NULL},NULL,18},
	{{"reset",NULL},NULL,19},
	{{"raise",NULL},NULL,20},
	{{"promote",NULL},NULL,21}
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

struct military_master_storage
{
	int clan_pts[MAXCLAN];
	int quests_given[5];
	int quests_solved[5];
	int exp_given[5];
	int pts_given[5];
};

struct military_master_data
{
	int current_victim;
	int last_clan_update;
	int last_save;
	int last_recom;

	int storage_state;
	int storage_version;
	int storage_ID;

	struct military_master_storage storage_data;
};

static char *diff_name[5]={
	"easy",
	"normal",
	"hard",
	"impossible",
	"insane"
};

// base mission - must generate a mission all the time
void generate_demon_mission(int lvl,struct military_ppd *ppd)
{
	ppd->mis[0].type=1;						// slay pent demon
	ppd->mis[0].opt1=1+RANDOM(10);					// 1 to 10 of them
	ppd->mis[0].opt2=min(118,lvl);					// at level lvl
	ppd->mis[0].pts=1;						// worth one military point
	ppd->mis[0].exp=1*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth

	ppd->mis[1].type=1;						// slay pent demon
	ppd->mis[1].opt1=5+RANDOM(16);					// 5 to 20 of them
	ppd->mis[1].opt2=min(118,lvl);					// at level lvl
	ppd->mis[1].pts=2;						// worth one military point
	ppd->mis[1].exp=2*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth

	ppd->mis[2].type=1;						// slay pent demon
	ppd->mis[2].opt1=25+RANDOM(76);					// 25 to 100 of them
	ppd->mis[2].opt2=min(118,lvl);					// at level lvl
	ppd->mis[2].pts=4;						// worth one military point
	ppd->mis[2].exp=4*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth

	ppd->mis[3].type=1;						// slay pent demon
	ppd->mis[3].opt1=200+RANDOM(301);				// 200 to 500 of them
	ppd->mis[3].opt2=min(118,lvl+1);				// at level lvl+1
	ppd->mis[3].pts=10;						// worth one military point
	ppd->mis[3].exp=10*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth

	ppd->mis[4].type=1;						// slay pent demon
	ppd->mis[4].opt1=500+RANDOM(1501);				// 500 to 2000 of them
	ppd->mis[4].opt2=min(118,lvl+2);				// at level lvl+2
	ppd->mis[4].pts=25;						// worth one military point
	ppd->mis[4].exp=25*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth	
}

// additional misison - may overwrite one or more
void generate_sewer_mission(int lvl,struct military_ppd *ppd)
{	//9...39 step 2
	switch(RANDOM(5)) {
		case 0:
			if (lvl<9 || lvl>39 || !(lvl&1)) break;
                        ppd->mis[0].type=2;						// slay ratling
			ppd->mis[0].opt1=1+RANDOM(4);					// 1 to 4 of them
			ppd->mis[0].opt2=lvl;						// at level lvl
			ppd->mis[0].pts=1;						// worth one military point
			ppd->mis[0].exp=1*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;

		case 1:
			if (lvl<9 || lvl>39 || !(lvl&1)) break;
			ppd->mis[1].type=2;						// slay ratling
			ppd->mis[1].opt1=5+RANDOM(6);					// 5 to 10 of them
			ppd->mis[1].opt2=lvl;						// at level lvl
			ppd->mis[1].pts=2;						// worth one military point
			ppd->mis[1].exp=2*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;
		
		case 2:
			if (lvl<9 || lvl>39 || !(lvl&1)) break;
			ppd->mis[2].type=2;						// slay ratling
			ppd->mis[2].opt1=25+RANDOM(26);					// 25 to 50 of them
			ppd->mis[2].opt2=lvl;						// at level lvl
			ppd->mis[2].pts=4;						// worth one military point
			ppd->mis[2].exp=4*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;
		
		case 3:	
			lvl++;
			if (lvl<9 || lvl>39 || !(lvl&1)) break;
			ppd->mis[3].type=2;						// slay ratling
			ppd->mis[3].opt1=100+RANDOM(201);				// 100 to 300 of them
			ppd->mis[3].opt2=lvl;						// at level lvl+1
			ppd->mis[3].pts=10;						// worth one military point
			ppd->mis[3].exp=10*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;
		
		case 4:	
			lvl+=2;
			if (lvl<9 || lvl>39 || !(lvl&1)) break;
			ppd->mis[4].type=2;						// slay ratling
			ppd->mis[4].opt1=200+RANDOM(501);				// 200 to 700 of them
			ppd->mis[4].opt2=lvl;						// at level lvl+2
			ppd->mis[4].pts=25;						// worth one military point
			ppd->mis[4].exp=25*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth	
			break;
	}
}

// additional misison - may overwrite one or more
void generate_mine_mission(int lvl,struct military_ppd *ppd)
{
	int rank;

	rank=cbrt(ppd->military_pts);

	// starts at level 12, open end
	switch(RANDOM(5)) {
		case 0:
			if (lvl<12) break;
                        ppd->mis[0].type=3;						// find silver
			ppd->mis[0].opt1=10+rank*8+RANDOM(31+rank*5); 			//
			ppd->mis[0].opt2=0;						// no option
			ppd->mis[0].pts=1;						// worth one military point
			ppd->mis[0].exp=1*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;

		case 1:
			if (lvl<12) break;
			ppd->mis[1].type=3;						// find silver
			ppd->mis[1].opt1=50+rank*20+RANDOM(51+rank*10);			//
			ppd->mis[1].opt2=0;						// no option
			ppd->mis[1].pts=2;						// worth 2 military points
			ppd->mis[1].exp=2*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;
		
		case 2:
			if (lvl<12) break;
			ppd->mis[2].type=3;						// find silver
			ppd->mis[2].opt1=250+rank*60+RANDOM(251+rank*40);		//
			ppd->mis[2].opt2=0;						// no option
			ppd->mis[2].pts=4;						// worth 4 military points
			ppd->mis[2].exp=4*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;
		
		case 3:	
			lvl++;
			if (lvl<12) break;
			ppd->mis[3].type=3;						// find silver
			ppd->mis[3].opt1=1000+rank*200+RANDOM(1001+rank*150);		//
			ppd->mis[3].opt2=0;						// no option
			ppd->mis[3].pts=10;						// worth 10 military points
			ppd->mis[3].exp=10*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth
			break;
		
		case 4:	
			lvl+=2;
			if (lvl<12) break;
			ppd->mis[4].type=3;						// find silver
			ppd->mis[4].opt1=2000+rank*500+RANDOM(3001+rank*600);		//
			ppd->mis[4].opt2=0;						// no option
			ppd->mis[4].pts=25;						// worth 25 military points
			ppd->mis[4].exp=25*pow(cbrt(ppd->military_pts)+5,4)/16;		// normal experience worth	
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
	if (!RANDOM(3)) generate_sewer_mission(lvl,ppd);
	generate_mine_mission(lvl,ppd);

	ppd->mission_yday=yday+1;
}

void offer_missions(int cn,int co,struct military_ppd *ppd)
{
	int n;

	for (n=0; n<5; n++) {
		if (ppd->mis[n].pts>1 && ppd->mis[n].pts>ppd->current_pts) continue;

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

void military_master_parse(int cn,struct military_master_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"storage")) dat->storage_ID=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void military_master_driver(int cn,int ret,int lastact)
{
        struct military_master_data *dat;
	struct military_ppd *ppd;
	int co,res,size,n,rank,nr,pts;
        struct msg *msg,*next;	
	void *tmp;

        dat=set_data(cn,DRD_MILITARYMASTER,sizeof(struct military_master_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
				military_master_parse(cn,dat);
				ch[cn].arg=NULL;
				dat->last_clan_update=realtime;
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
			if ((nr=get_char_clan(co))) {
				if (dat->storage_data.clan_pts[nr]>12000 && dat->last_recom!=ch[co].ID) {
					ppd->current_pts+=5;
					dat->storage_data.clan_pts[nr]-=12000;
					dat->last_recom=ch[co].ID;
					say(cn,"Be greeted, %s. You've been recommended by your clan!",ch[co].name);
				}
			}
			if (ppd->recommend!=yday+1) {
				for (n=0; n<MAXADVISOR; n++) {
					if (ppd->advisor_last[n]==yday+1) {
						say(cn,"Be greeted, %s. You have been recommended by my trusty advisor %d",ch[co].name,n);
					}
				}
				ppd->recommend=yday+1;
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
				dat->storage_data.quests_solved[n]++;
				ppd->took_mission=0;
				give_exp_bonus(co,ppd->mis[n].exp);

				if (ch[co].prof[P_MERCENARY]) {
					ch[co].gold+=ppd->mis[n].exp/5;
					ch[co].flags|=CF_ITEMS;
					say(cn,"Well done, %s. You've solved your mission! Your pay is %.2fG.",ch[co].name,ppd->mis[n].exp/500.0);

					pts=ppd->mis[n].pts+ppd->mis[n].pts/2+ppd->mis[n].pts*ch[cn].prof[P_MERCENARY]*3/100+1;
                                        ppd->military_pts+=pts;
                                        dat->storage_data.pts_given[n]+=ppd->mis[n].pts;
				} else {
					say(cn,"Well done, %s. You've solved your mission!",ch[co].name);
					
					pts=ppd->mis[n].pts+ppd->mis[n].pts/2;
                                        ppd->military_pts+=pts;
					dat->storage_data.pts_given[n]+=ppd->mis[n].pts;
				}

				ppd->normal_exp+=ppd->mis[n].exp;
				dat->storage_data.exp_given[n]+=ppd->mis[n].exp;

				dlog(cn,0,"Got %d mil exp, %d normal exp for solving a mission",ppd->mis[n].exp,pts);

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
				case 11:	if (ppd->took_mission) {
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
						display_mission(cn,co,0,ppd);
						ppd->took_mission=1;
						ppd->took_yday=yday+1;
						dat->storage_data.quests_given[0]++;
						break;
				case 12:	if (ppd->took_mission) {
							say(cn,"You already have a mission, %s. Would you like to °c4hear°c0 it again?",ch[co].name);
							break;
						}
						if (ppd->solved_yday==yday+1) {
							say(cn,"I don't have another mission for you today, %s.",ch[co].name);
							break;
						}
						if (ppd->current_pts<ppd->mis[1].pts) {
							say(cn,"I have not offered you that kind of mission, %s.",ch[co].name);
							break;
						}
						if (ppd->mission_yday!=yday+1) {
							say(cn,"I haven't offered you that kind of mission today, %s.",ch[co].name);
							break;
						}
						display_mission(cn,co,1,ppd);
						ppd->took_mission=2;
						ppd->took_yday=yday+1;
						ppd->current_pts-=ppd->mis[1].pts;
						dat->storage_data.quests_given[1]++;
						break;
				case 13:	if (ppd->took_mission) {
							say(cn,"You already have a mission, %s. Would you like to °c4hear°c0 it again?",ch[co].name);
							break;
						}
						if (ppd->solved_yday==yday+1) {
							say(cn,"I don't have another mission for you today, %s.",ch[co].name);
							break;
						}
						if (ppd->current_pts<ppd->mis[2].pts) {
							say(cn,"I have not offered you that kind of mission, %s.",ch[co].name);
							break;
						}
						if (ppd->mission_yday!=yday+1) {
							say(cn,"I haven't offered you that kind of mission today, %s.",ch[co].name);
							break;
						}
						display_mission(cn,co,2,ppd);
						ppd->took_mission=3;
						ppd->took_yday=yday+1;
						ppd->current_pts-=ppd->mis[2].pts;
						dat->storage_data.quests_given[2]++;
						break;
				case 14:	if (ppd->took_mission) {
							say(cn,"You already have a mission, %s. Would you like to °c4hear°c0 it again?",ch[co].name);
							break;
						}
						if (ppd->solved_yday==yday+1) {
							say(cn,"I don't have another mission for you today, %s.",ch[co].name);
							break;
						}
						if (ppd->current_pts<ppd->mis[3].pts) {
							say(cn,"I have not offered you that kind of mission, %s.",ch[co].name);
							break;
						}
						if (ppd->mission_yday!=yday+1) {
							say(cn,"I haven't offered you that kind of mission today, %s.",ch[co].name);
							break;
						}
						display_mission(cn,co,3,ppd);
						ppd->took_mission=4;
						ppd->took_yday=yday+1;
						ppd->current_pts-=ppd->mis[3].pts;
						dat->storage_data.quests_given[3]++;
						break;
				case 15:	if (ppd->took_mission) {
							say(cn,"You already have a mission, %s. Would you like to °c4hear°c0 it again?",ch[co].name);
							break;
						}
						if (ppd->solved_yday==yday+1) {
							say(cn,"I don't have another mission for you today, %s.",ch[co].name);
							break;
						}
						if (ppd->current_pts<ppd->mis[4].pts) {
							say(cn,"I have not offered you that kind of mission, %s.",ch[co].name);
							break;
						}
						if (ppd->mission_yday!=yday+1) {
							say(cn,"I haven't offered you that kind of mission today, %s.",ch[co].name);
							break;
						}
						display_mission(cn,co,4,ppd);
						ppd->took_mission=5;
						ppd->took_yday=yday+1;
						ppd->current_pts-=ppd->mis[4].pts;
						dat->storage_data.quests_given[4]++;
						break;
				case 16:	if (!ppd->took_mission) {
							say(cn,"But you did not take any °c4mission°c0, %s.",ch[co].name);
							break;
						}
						say(cn,"So, you failed? Well, %s, I'll remove that mission from your record. Would you like to get another °c4mission°c0?",get_army_rank_string(co));
						ppd->took_mission=0;
						break;
				case 17:	if (!ppd->took_mission) {
						     say(cn,"But you do not have a °c4mission°c0 yet, %s.",get_army_rank_string(co));
						}
						display_mission(cn,co,ppd->took_mission-1,ppd);
						break;
				case 18:	if (!(ch[co].flags&CF_GOD)) break;
						say(cn,"You have %d pts and you have gained %d exp.",
						    ppd->military_pts,
						    ppd->normal_exp);						
						for (n=1; n<32; n++) {
							if (dat->storage_data.clan_pts[n]) {
								say(cn,"Clan %d has %d pts",n,dat->storage_data.clan_pts[n]);
							}
						}
						for (n=0; n<5; n++) {
							say(cn,"I have given %d %s quests, %d of these have been solved (%.2f%%) for a total of %d exp (%.2f exp per quest)",
							    dat->storage_data.quests_given[n],
							    diff_name[n],
							    dat->storage_data.quests_solved[n],
							    100.0/dat->storage_data.quests_given[n]*dat->storage_data.quests_solved[n],
							    dat->storage_data.exp_given[n],
							    (double)dat->storage_data.exp_given[n]/dat->storage_data.quests_solved[n]);

						}
						break;
				case 19:	if (!(ch[co].flags&CF_GOD)) break;
                                                ppd->solved_yday=0;
						ppd->mission_yday=0;
						break;
				case 20:	if (!(ch[co].flags&CF_GOD)) break;
						ppd->military_pts+=1000;
						break;
				case 21:	if (!(ch[co].flags&CF_GOD)) break;
						give_military_pts(cn,co,100,1);
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

	if (realtime-dat->last_clan_update>60) {
		for (n=0; n<MAXCLAN; n++) {
			res=get_clan_bonus(n,1)*20;
			if (res>0) {
				dat->storage_data.clan_pts[n]+=res;
			}
		}
		dat->last_clan_update+=60;
	}
	//say(cn,"diff=%d, state=%d",realtime-dat->last_save,dat->storage_state);
	if (realtime-dat->last_save>60*5) {
		dat->last_save=realtime;
		if (dat->storage_state==4) dat->storage_state++;
	}

	// storage management
	switch(dat->storage_state) {
		case 0:		if (create_storage(dat->storage_ID,"Master Data",&dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 1:		if (check_create_storage()) dat->storage_state++;
				break;
		case 2:		if (read_storage(dat->storage_ID,dat->storage_version)) dat->storage_state++;
				break;
		case 3:		res=check_read_storage(&dat->storage_version,&tmp,&size);
				if (res==1 && size==sizeof(dat->storage_data)) {
					memcpy(&dat->storage_data,tmp,sizeof(dat->storage_data));
					xfree(tmp);
					dat->storage_state++;
				} else if (res) {
					elog("check storage said %d (%d)",res,size);
					dat->storage_state++;
				}
				break;
		case 4:		break;	// no op
		case 5:		if (update_storage(dat->storage_ID,dat->storage_version,&dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 6:		if (check_update_storage()) {
					dat->storage_state++;
					dat->storage_version++;
				}
				break;
		case 7:		dat->storage_state=4;
				break;
		default:	dat->storage_state=4;
				break;
	}

	// ------------------

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

        do_idle(cn,TICKS/2);
}

struct military_advisor_data
{
        int storage_state;
	int storage_version;
	int storage_ID;

	struct cost_data storage_data[5];
};

void military_advisor_parse(int cn,struct military_advisor_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"storage")) dat->storage_ID=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

static void adv_introduction(int cn,int co,struct military_advisor_data *dat)
{
	char *ptr;

	switch(dat->storage_ID%4) {
		case 0:		ptr="I could do you a °c4favor°c0, %s, I could mention your name to the military governor of Aston. I'm sure that'd help you get that promotion early!"; break;
		case 1:		ptr="Say, %s, would you like to speed up your way up the rank ladder? I could speak to the military governor of Aston if you want me to do you that °c4favor°c0."; break;
		case 2:		ptr="Not getting promoted as fast as you want, %s? I could do you the °c4favor°c0 of talking to the military governor of Aston about you."; break;
		default:	ptr="Need a °c4favor°c0, %s?"; break;
	}

	quiet_say(cn,ptr,ch[co].name);
}

static void adv_favor_desc(int cn,int co,struct military_advisor_data *dat)
{
	char *ptr;

	switch(dat->storage_ID%2) {
		case 0:		ptr="My favors come in five sizes, °c4small°c0, °c4medium°c0, °c4big°c0, °c4huge°c0 and °c4vast°c0."; break;
		default:	ptr="I could do you a °c4small°c0, °c4medium°c0, °c4big°c0, °c4huge°c0 or °c4vast°c0 favor, %s."; break;
	}

	quiet_say(cn,ptr,ch[co].name);
}

int advisor_price(int level)
{
	if (level<25) return 400;
	if (level<45) return 800;
	if (level<65) return 1200;
	if (level<85) return 1500;
	
	return 2000;	
}

void military_advisor_driver(int cn,int ret,int lastact)
{
	struct military_advisor_data *dat;
	struct military_ppd *ppd;
        int co,size,res,cost,n,idx;
        struct msg *msg,*next;
	void *tmp;
	static char *fav_name[5]={
		"small",
		"normal",
		"big",
		"huge",
		"vast"
	};

        dat=set_data(cn,DRD_MILITARYADVISOR,sizeof(struct military_advisor_data));
	if (!dat) return;	// oops...

	if (dat->storage_ID<27) idx=dat->storage_ID-7;
	else idx=dat->storage_ID-31+3;
	if (idx<0 || idx>=MAXADVISOR) idx=0;
	
        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
				military_advisor_parse(cn,dat);
				ch[cn].arg=NULL;
				dat->storage_data[0].created=realtime;
				dat->storage_data[1].created=realtime;
				dat->storage_data[2].created=realtime;
				dat->storage_data[3].created=realtime;
				dat->storage_data[4].created=realtime;
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
			if (ppd->advisor_state==0 || ppd->current_advisor!=dat->storage_ID) {
				if (ppd->advisor_last[idx]==yday+1) {
					quiet_say(cn,"Ah, %s. I haven't forgotten you.",ch[co].name);
				} else {
					adv_introduction(cn,co,dat);
				}
				ppd->advisor_state=1;
				ppd->current_advisor=dat->storage_ID;
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
				case 2:		ppd->advisor_state=0; break;
				case 3:		if (ppd->advisor_last[idx]==yday+1) quiet_say(cn,"Mentioning your name twice a day won't accomplish much, %s.",ch[co].name);
						else adv_favor_desc(cn,co,dat);
						break;
				case 4:		if (ppd->advisor_last[idx]==yday+1) quiet_say(cn,"Mentioning your name twice a day won't accomplish much, %s.",ch[co].name);
						else {
							cost=advisor_price(ch[co].level)*1;	//max(500,calc_cost(100*100,dat->storage_data+0,30));
							quiet_say(cn,"You can get a small favor for the humble fee of %dG, %dS, %s. Say °c4pay°c0 if you want it.",cost/100,cost%100,ch[co].name);
							ppd->advisor_cost=cost;
							ppd->advisor_state=2;
							ppd->advisor_storage_nr=0;
						}
						break;
				case 5:		if (ppd->advisor_last[idx]==yday+1) quiet_say(cn,"Mentioning your name twice a day won't accomplish much, %s.",ch[co].name);
						else {
							cost=advisor_price(ch[co].level)*3;	//max(1500,calc_cost(200*100,dat->storage_data+1,20));
							quiet_say(cn,"You can get a medium favor for the humble fee of %dG, %dS, %s. Say °c4pay°c0 if you want it.",cost/100,cost%100,ch[co].name);
							ppd->advisor_cost=cost;
							ppd->advisor_state=2;
							ppd->advisor_storage_nr=1;
						}
						break;
				case 6:		if (ppd->advisor_last[idx]==yday+1) quiet_say(cn,"Mentioning your name twice a day won't accomplish much, %s.",ch[co].name);
						else {
							cost=advisor_price(ch[co].level)*10;	//max(4000,calc_cost(400*100,dat->storage_data+2,15));
							quiet_say(cn,"You can get a big favor for the humble fee of %dG, %dS, %s. Say °c4pay°c0 if you want it.",cost/100,cost%100,ch[co].name);
							ppd->advisor_cost=cost;
							ppd->advisor_state=2;
							ppd->advisor_storage_nr=2;
						}
						break;
				case 7:		if (ppd->advisor_last[idx]==yday+1) quiet_say(cn,"Mentioning your name twice a day won't accomplish much, %s.",ch[co].name);
						else {
							cost=advisor_price(ch[co].level)*20;	//max(10000,calc_cost(800*100,dat->storage_data+3,10));
							quiet_say(cn,"You can get a huge favor for the humble fee of %dG, %dS, %s. Say °c4pay°c0 if you want it.",cost/100,cost%100,ch[co].name);
							ppd->advisor_cost=cost;
							ppd->advisor_state=2;
							ppd->advisor_storage_nr=3;
						}
						break;
				case 8:		if (ppd->advisor_last[idx]==yday+1) quiet_say(cn,"Mentioning your name twice a day won't accomplish much, %s.",ch[co].name);
						else {
							cost=advisor_price(ch[co].level)*35;	//max(25000,calc_cost(1600*100,dat->storage_data+4,5));
							quiet_say(cn,"You can get a vast favor for the humble fee of %dG, %dS, %s. Say °c4pay°c0 if you want it.",cost/100,cost%100,ch[co].name);
							ppd->advisor_cost=cost;
							ppd->advisor_state=2;
							ppd->advisor_storage_nr=4;
						}
						break;
				case 9:		if (ppd->current_advisor!=dat->storage_ID ||
						    ppd->advisor_state!=2) {
							quiet_say(cn,"Pay for what? We haven't agreed on anything yet.");
							ppd->advisor_state=1;
							break;
						}
						if (ch[co].gold<ppd->advisor_cost) {
							quiet_say(cn,"Alas, you do not have enough money.");
							break;
						}
						ch[co].gold-=ppd->advisor_cost;
						ch[co].flags|=CF_ITEMS;
						
						add_cost(ppd->advisor_cost,dat->storage_data+ppd->advisor_storage_nr);
						if (dat->storage_state==4) dat->storage_state++;
						
						ppd->current_pts+=2+ppd->advisor_storage_nr*2;
						
						quiet_say(cn,"Alright, I'll mention your name to the military governor, %s.",ch[co].name);						

						ppd->advisor_state=1;
						ppd->advisor_last[idx]=yday+1;
						break;
				case 18:	if (!(ch[co].flags&CF_GOD)) break;
						for (n=0; n<5; n++) {
							quiet_say(cn,"I have earned %.2fG on %d %s favors (%.2fG per favor)",
							    dat->storage_data[n].earned/100.0,
							    dat->storage_data[n].sold,
							    fav_name[n],
							    dat->storage_data[n].earned/100.0/dat->storage_data[n].sold);
						}
						break;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
                        // didnt work, let it vanish, then
			destroy_item(ch[cn].citem);
			ch[cn].citem=0;
			quiet_say(cn,"That's junk.");			
		}
		
		standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

	// storage management

	switch(dat->storage_state) {
		case 0:		if (create_storage(dat->storage_ID,"Advisor Data",dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 1:		if (check_create_storage()) dat->storage_state++;
				break;
		case 2:		if (read_storage(dat->storage_ID,dat->storage_version)) dat->storage_state++;
				break;
		case 3:		res=check_read_storage(&dat->storage_version,&tmp,&size);
				if (res==1 && size==sizeof(dat->storage_data)) {
					memcpy(dat->storage_data,tmp,sizeof(dat->storage_data));
					xfree(tmp);
					dat->storage_state++;
					//charlog(cn,"read storage: earned=%lldG/%lldG/%lldG/%lldG/%lldG",dat->storage_data[0].earned/100,dat->storage_data[1].earned/100,dat->storage_data[2].earned/100,dat->storage_data[3].earned/100,dat->storage_data[4].earned/100);
				} else if (res) {
					elog("check storage said %d (%d)",res,size);
					dat->storage_state++;
				}
				break;
		case 4:		break;	// no op
		case 5:		if (update_storage(dat->storage_ID,dat->storage_version,dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 6:		if (check_update_storage()) {
					dat->storage_state++;
					dat->storage_version++;
					//charlog(cn,"update storage: earned=%lldG/%lldG/%lldG/%lldG/%lldG",dat->storage_data[0].earned/100,dat->storage_data[1].earned/100,dat->storage_data[2].earned/100,dat->storage_data[3].earned/100,dat->storage_data[4].earned/100);
				}
				break;
		case 7:		dat->storage_state=4;
				break;
		default:	dat->storage_state=4;
				break;
	}

	// ------------------


        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

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
		case CDR_MILITARY_ADVISOR:	military_advisor_driver(cn,ret,lastact); return 1;

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
		case CDR_MILITARY_ADVISOR:	immortal_dead(cn,co); return 1;
		
		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_MILITARY_MASTER:	return 1;
		case CDR_MILITARY_ADVISOR:	return 1;

		default:		return 0;
	}
}
