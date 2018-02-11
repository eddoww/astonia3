/*

$Id: fdemon.c,v 1.2 2007/08/13 18:50:38 devel Exp $

$Log: fdemon.c,v $
Revision 1.2  2007/08/13 18:50:38  devel
fixed some warnings

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2003/11/19 16:23:00  sam
made military ranks>24 (warlord) unaccessible

Revision 1.2  2003/10/13 14:12:13  sam
Added RCS tags


*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "database.h"
#include "map.h"
#include "create.h"
#include "container.h"
#include "drvlib.h"
#include "tool.h"
#include "spell.h"
#include "effect.h"
#include "light.h"
#include "date.h"
#include "los.h"
#include "skill.h"
#include "item_id.h"
#include "libload.h"
#include "task.h"
#include "sector.h"
#include "act.h"
#include "effect.h"
#include "player_driver.h"
#include "military.h"
#include "player.h"
#include "consistency.h"

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
		default: 		return 0;
	}
}

//-----------------------

struct qa
{
	char *word[20];
	char *answer;
	int needs_name;
	int answer_code;
};

#define QA_YES			20
#define QA_NO			21
#define QA_NICEDAY		22
#define QA_THANKS		23
#define QA_YOUSTINK		24
#define QA_GOAWAY		25
#define QA_DOSOMETHING		26
#define QA_FUNNYFACE		27
#define QA_LIKESMILE		28
#define QA_TURNBACK		29
#define QA_GREATEST		30
#define QA_GREATSOLDIER		31
#define QA_AFRAID		32
#define QA_WHYMEAN		33
#define QA_SMELLRATLING		34
#define QA_WHATSUP		35
#define QA_SHUTUP		36
#define QA_DOSOON		37
#define QA_STOPBOTHER		38
#define QA_NOTFIGHT		39
#define QA_BEQUIET		40
#define QA_ISTHATSO		41
#define QA_NOTTHATBAD		42
#define QA_YOUAFRAID		43
#define QA_TOUGHFELLOW		44
#define QA_QUIETBIGMOUTH	45
#define QA_ONEDAY		46
#define QA_DONTTHINKSO		47
#define QA_NONEED		48
#define QA_COWARD		49

struct qa qa[]={
	{{"how","are","you",NULL},"I'm fine!",1,0},
	{{"hello",NULL},"Hello, %s!",0,0},
	{{"hi",NULL},"Hi, %s!",0,0},
	{{"greetings",NULL},"Greetings, %s!",0,0},
	{{"hail",NULL},"And hail to you, %s!",0,0},
        {{"what's","your","name",NULL},NULL,0,1},
	{{"what","is","your","name",NULL},NULL,0,1},
        {{"who","are","you",NULL},NULL,0,1},
	{{"follow",NULL},NULL,0,2},
	{{"back",NULL},NULL,0,3},
	{{"retreat",NULL},NULL,0,4},
	{{"front",NULL},NULL,0,5},
	{{"behind",NULL},NULL,0,6},
	{{"emote",NULL},NULL,0,7},
	{{"repeat",NULL},NULL,0,8},

	{{"yes",NULL},NULL,1,QA_YES},
	{{"yes","they","are",NULL},NULL,1,QA_YES},
	{{"yes","it","is",NULL},NULL,1,QA_YES},
	{{"sure","they","are",NULL},NULL,1,QA_YES},
	{{"sure","it","is",NULL},NULL,1,QA_YES},

	{{"no",NULL},NULL,1,QA_NO},
	{{"no","they","are","not",NULL},NULL,1,QA_NO},
	{{"no","it","is","not",NULL},NULL,1,QA_NO},

	{{"oh","what","a","nice","day","it","is","isn't","it",NULL},NULL,1,QA_NICEDAY},
	{{"the","nights","here","are","scary","aren't","they",NULL},NULL,1,QA_NICEDAY},

	{{"thanks",NULL},NULL,1,QA_THANKS},
	{{"thank","you",NULL},NULL,1,QA_THANKS},
	{{"why","thank","you",NULL},NULL,1,QA_THANKS},
	{{"why","thanks",NULL},NULL,1,QA_THANKS},

	{{"you","stink",NULL},NULL,1,QA_YOUSTINK},
	{{"why","don't","you","go","away",NULL},NULL,1,QA_GOAWAY},
	{{"oh","come","on","do","something",NULL},NULL,1,QA_DOSOMETHING},
	{{"you","have","a","funny","face",NULL},NULL,1,QA_FUNNYFACE},
	{{"i","like","the","way","you","smile",NULL},NULL,1,QA_LIKESMILE},
	{{"shouldn't","we","turn","back","what","do","you","think",NULL},NULL,1,QA_TURNBACK},
	{{"ha","i'm","the","greatest","right",NULL},NULL,1,QA_GREATEST},
	{{"i'm","becoming","a","great","soldier",NULL},NULL,1,QA_GREATSOLDIER},
	{{"i'm","afraid",NULL},NULL,1,QA_AFRAID},

	{{"why","are","you","so","mean","to","me",NULL},NULL,1,QA_WHYMEAN},
	{{"and","you","small","like","a","ratling",NULL},NULL,1,QA_SMELLRATLING},
	{{"what's","up","with","you",NULL},NULL,1,QA_WHATSUP},
	{{"shut","up",NULL},NULL,1,QA_SHUTUP},
	{{"yeah","i","hope","we'll","do","something","soon",NULL},NULL,1,QA_DOSOON},
	{{"stop","bothering","me",NULL},NULL,1,QA_STOPBOTHER},
	{{"I'm","bored","too","let's","not","fight",NULL},NULL,1,QA_NOTFIGHT},
	{{"oh","boy","please","be","quiet",NULL},NULL,1,QA_BEQUIET},
	{{"is","that","so",NULL},NULL,1,QA_ISTHATSO},
	{{"it's","not","that","bad",NULL},NULL,1,QA_NOTTHATBAD},
	{{"are","you","afraid",NULL},NULL,1,QA_YOUAFRAID},
	{{"you're","a","tough","fellow",NULL},NULL,1,QA_TOUGHFELLOW},
	{{"be","quiet","you","bigmouth",NULL},NULL,1,QA_QUIETBIGMOUTH},
	{{"one","day","you'll","be","a","great","soldier",NULL},NULL,1,QA_ONEDAY},
	{{"i","don't","think","so",NULL},NULL,1,QA_DONTTHINKSO},
	{{"there's","no","need","to","be","afraid",NULL},NULL,1,QA_NONEED},
	{{"shut","up","you","you","coward",NULL},NULL,1,QA_COWARD}
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

	//if (!(ch[co].flags&CF_PLAYER)) return 0;

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
                                if (strcmp(wordlist[n],qa[q].word[n])) break;			
			}
			if (qa[q].needs_name && !name) continue;
			
                        if (n==w && !qa[q].word[n]) {
				if (qa[q].answer) say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else switch(qa[q].answer_code) {
					     case 1:	say(cn,"I'm %s.",ch[cn].name);
					     default:	return qa[q].answer_code;
				}
				break;
			}
		}
	}
	

        return 0;
}


//-----------------------


struct profile
{
	char *name;
	char gender;

	int cuddly;	// likes to cuddle :P	
	int angst;	// afraid of everything
	int bore;	// easily bored
	int bigmouth;	// needs constant praise

	int sprite;
};

struct profile profile[]={
	// name,gender	cuddly	angst	bore	bigmouth	sprite
	{"Bert",'M',	0,	5,	15,	10,		158},
	{"Josh",'M',	20,	10,	20,	5,		160},
	{"Will",'M',	10,	20,	5,	10,		162},
	{"James",'M',	0,	15,	10,	20,		164},
	{"Carl",'M',	25,	5,	5,	15,		166},
	{"Jim",'M',	5,	15,	5,	10,		168},
	{"Brad",'M',	0,	5,	15,	5,		170},
	{"Jenny",'F',	25,	25,	5,	5,		176},
	{"Sarah",'F',	10,	15,	15,	15,		178},
	{"Sue",'F',	0,	5,	10,	25,		180},
	{"Peggy",'F',	15,	10,	20,	5,		182},
	{"Mary",'F',	0,	20,	5,	10,		184},
	{"Clara",'F',	5,	5,	5,	15,		186},
	{"Beth",'F',	1,	10,	15,	10,		188},
};

#define MAXSOLDIER	3

struct emote
{
        int cuddly;	// tendency to cuddle
        int lonely;	// current lack of cuddling

	int angst;	// tendency to be afraid
	int fear;	// current fear

	int bore;	// tendency to be bored (wrong name :P)
	int boredom;	// current boredom

	int bigmouth;	// tendency to make big words after an achievement
	int praise;	// current lack of praise

	int likes[MAXSOLDIER+1];
	int talked[MAXSOLDIER+1];
	int answer_timer;
	int answer_cn;
	int answer_type;

	int last_emote;
};

struct soldier
{
	int type;	// 0=empty, 1=warrior, 2=mage
	
	int rank;	// army rank
        int base;	// strength base
	int profile;

        int exp;
	int cn;
	int serial;

	struct emote emote;	
};

struct farmy_ppd
{
	int boss_stage;
	int boss_timer;

	struct soldier soldier[MAXSOLDIER];

	int boss_counter;
	int boss_reported;
};

struct farmy_data
{
	int leader_cn;
	int lx,ly;	// last known position of leader

	int mission,opt1,opt2;
	int timer;

	int closeup;

	struct emote emote;

	int platoon[MAXSOLDIER+1];
};

void assign_profile(int slot,int nr,struct farmy_ppd *ppd)
{
	bzero(&ppd->soldier[slot].emote,sizeof(ppd->soldier[slot].emote));

	ppd->soldier[slot].profile=nr;
	ppd->soldier[slot].emote.cuddly=profile[nr].cuddly;
	ppd->soldier[slot].emote.angst=profile[nr].angst;
	ppd->soldier[slot].emote.bore=profile[nr].bore;
	ppd->soldier[slot].emote.bigmouth=profile[nr].bigmouth;
}

void update_soldier(int co,int n,struct farmy_ppd *ppd)
{
	int m,in,cc;
	char buf[80];

	if (ppd->soldier[n].type==1) cc=create_char("army1s",0);
	else cc=create_char("army2s",0);
	if (!cc) {
		elog("take_soldiers: could not create char");
		return;
	}

        ppd->soldier[n].base=43+ppd->soldier[n].rank*4;

        for (m=0; m<V_MAX; m++) {
		if (ch[cc].value[1][m]==1) {
			ch[co].value[1][m]=ppd->soldier[n].base/2;
		} else if (ch[cc].value[1][m]==2) {
			ch[co].value[1][m]=ppd->soldier[n].base-5;
		} else if (ch[cc].value[1][m]==3) {
			ch[co].value[1][m]=ppd->soldier[n].base;
		}
	}
	destroy_char(cc);

	ch[co].exp=ch[co].exp_used=calc_exp(co);
	ch[co].level=exp2level(ch[co].exp);

	destroy_items(co);
        if (ppd->soldier[n].type==1) {
		sprintf(buf,"sleeves%dq1",ch[co].value[1][V_ARMORSKILL]/10+1);
		in=ch[co].item[WN_ARMS]=create_item(buf);
		it[in].carried=co;
		sprintf(buf,"armor%dq1",ch[co].value[1][V_ARMORSKILL]/10+1);
		in=ch[co].item[WN_BODY]=create_item(buf);
		it[in].carried=co;
		sprintf(buf,"helmet%dq1",ch[co].value[1][V_ARMORSKILL]/10+1);
		in=ch[co].item[WN_HEAD]=create_item(buf);
		it[in].carried=co;
		sprintf(buf,"leggings%dq1",ch[co].value[1][V_ARMORSKILL]/10+1);
		in=ch[co].item[WN_LEGS]=create_item(buf);
		it[in].carried=co;
		sprintf(buf,"sword%dq1",ch[co].value[1][V_SWORD]/10+1);
		in=ch[co].item[WN_RHAND]=create_item(buf);
		it[in].carried=co;
	} else {
		sprintf(buf,"dagger%dq1",ch[co].value[1][V_DAGGER]/10+1);
		in=ch[co].item[WN_RHAND]=create_item(buf);
		it[in].carried=co;
	}
	update_char(co);

	reset_name(co);
}

void take_soldiers(int cn)
{
	int n,rank,m,co,pro;
	struct farmy_ppd *ppd;
        struct farmy_data *dat;

	ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd));
	if (!ppd) return;	// oops...

	//set_army_rank(cn,4);	//!!!!!!remove
        rank=get_army_rank_int(cn);
	//bzero(ppd->soldier,sizeof(ppd->soldier));	// !!!!!!!!remove
	//ppd->soldier[0].rank=1;
	//ppd->soldier[1].rank=1;

	for (n=0; n<MAXSOLDIER; n++) {
		if (n==0 && rank>0 && !ppd->soldier[n].type) {
			if (ch[cn].flags&CF_WARRIOR) {
				ppd->soldier[n].type=2;
			} else {
				ppd->soldier[n].type=1;
			}
                        ppd->soldier[n].rank=1;
                        if (ch[cn].flags&CF_MALE) assign_profile(n,RANDOM(ARRAYSIZE(profile))/2+ARRAYSIZE(profile)/2,ppd);
			else assign_profile(n,RANDOM(ARRAYSIZE(profile))/2,ppd);
		}
		if (n==1 && rank>4 && !ppd->soldier[n].type) {
			if (ch[cn].flags&CF_WARRIOR) {
				ppd->soldier[n].type=1;
			} else {
				ppd->soldier[n].type=2;
			}
                        ppd->soldier[n].rank=1;
                        do {
				if (ch[cn].flags&CF_MALE) pro=RANDOM(ARRAYSIZE(profile)/2);
				else pro=RANDOM(ARRAYSIZE(profile)/2)+ARRAYSIZE(profile)/2;
				for (m=0; m<n; m++)
					if (pro==ppd->soldier[n].profile) break;
			} while (m<n);
			assign_profile(n,pro,ppd);
		}
		if (n==2 && rank>6 && !ppd->soldier[n].type) {
                        ppd->soldier[n].type=1;
                        ppd->soldier[n].rank=1;
                        do {
				pro=RANDOM(ARRAYSIZE(profile));
				for (m=0; m<n; m++)
					if (pro==ppd->soldier[n].profile) break;
			} while (m<n);
			assign_profile(n,pro,ppd);			
		}
		if (ppd->soldier[n].type) {
			if (ppd->soldier[n].type==1) co=create_char("army1s",0);
			else co=create_char("army2s",0);
			if (!co) {
				elog("take_soldiers: could not create char");
				continue;
			}
			update_soldier(co,n,ppd);

			strcpy(ch[co].name,profile[ppd->soldier[n].profile].name);
			if (profile[ppd->soldier[n].profile].gender=='M') {
				ch[co].flags|=CF_MALE;
				if (ppd->soldier[n].type==1) ch[co].sprite=profile[ppd->soldier[n].profile].sprite;
				else ch[co].sprite=profile[ppd->soldier[n].profile].sprite+1;
			} else {
				ch[co].flags|=CF_FEMALE;
				if (ppd->soldier[n].type==1) ch[co].sprite=profile[ppd->soldier[n].profile].sprite;
				else ch[co].sprite=profile[ppd->soldier[n].profile].sprite+1;
			}

                        update_char(co);
			ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
			ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
			ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
	
			ch[co].dir=DX_RIGHTDOWN;
			if (!drop_char(co,ch[cn].x,ch[cn].y,0)) {
				destroy_char(co);
				continue;
			}

			dat=set_data(co,DRD_FARMYDATA,sizeof(struct farmy_data));
			if (dat) {
				ppd->soldier[n].emote.boredom=0;
				ppd->soldier[n].emote.fear=0;
				ppd->soldier[n].emote.praise=0;
				dat->emote=ppd->soldier[n].emote;				
			}

			set_army_rank(co,ppd->soldier[n].rank);

                        ch[co].group=ch[cn].group;			
			ppd->soldier[n].cn=co;
			ppd->soldier[n].serial=ch[co].serial;
		}
	}

	for (n=0; n<MAXSOLDIER; n++) {
		if (ppd->soldier[n].type) {
			co=ppd->soldier[n].cn;
			dat=set_data(co,DRD_FARMYDATA,sizeof(struct farmy_data));
			if (dat) {
				for (m=0; m<MAXSOLDIER; m++) {
					if (ppd->soldier[n].type) {
						dat->platoon[m]=ppd->soldier[m].cn;
						//say(co,"I am %s, slot %d is %s",ch[co].name,m,ch[dat->platoon[m]].name);
					} else dat->platoon[m]=0;

				}
				dat->platoon[MAXSOLDIER]=cn;
			}
		}
	}
	
}

void drop_soldiers(int cn)
{
	int co,next,n=0;
	struct farmy_ppd *ppd;
	struct farmy_data *dat;

	ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd));
	if (!ppd) return;	// oops...

	for (co=getfirst_char(); co; co=next) {
                next=getnext_char(co);

		if (ch[co].group==ch[cn].group && !(ch[co].flags&CF_PLAYER)) {
			for (n=0; n<MAXSOLDIER; n++) {
				if (ppd->soldier[n].serial==ch[co].serial) break;				
			}
			if (n<MAXSOLDIER) {
				//say(co,"slot=%d, group=%d, (%d/%d)",n,ch[co].group,ch[co].exp-ch[co].exp_used,ppd->soldier[n].exp);
				dat=set_data(co,DRD_FARMYDATA,sizeof(struct farmy_data));
				if (dat) {
					ppd->soldier[n].emote=dat->emote;
				}

				ppd->soldier[n].exp+=ch[co].exp-ch[co].exp_used;
				ppd->soldier[n].serial=0;
			}

			remove_destroy_char(co);
		}
	}
}

#define MIS_FOLLOW	1
#define MIS_BACK	2
#define MIS_RETREAT	3
#define MIS_BEHIND	4
#define MIS_FRONT	5

int army_follow_driver(int cn,struct farmy_data *dat,int dist)
{
	int dir,co;

	co=dat->leader_cn;

	if (char_see_char(cn,co)) {
		dat->lx=ch[co].x;
		dat->ly=ch[co].y;
		if (abs(ch[cn].x-dat->lx)+abs(ch[cn].y-dat->ly)<=dist || (dir=pathfinder(ch[cn].x,ch[cn].y,dat->lx,dat->ly,2,NULL,0))==-1) return 0;
	
	} else {		
		if (abs(ch[cn].x-dat->lx)+abs(ch[cn].y-dat->ly)==0 || (dir=pathfinder(ch[cn].x,ch[cn].y,dat->lx,dat->ly,0,NULL,0))==-1) return 0;
	}

        do_walk(cn,dir);
	return 1;
}

int army_front_driver(int cn,struct farmy_data *dat,int dist)
{
	int dir,co,x,y;

	co=dat->leader_cn;

	dx2offset(ch[co].dir,&x,&y,NULL);
	x=x*4+ch[co].x;
	y=y*4+ch[co].y;

	if (abs(ch[cn].x-x)+abs(ch[cn].y-y)<=dist || !char_see_char(cn,co) || (dir=pathfinder(ch[cn].x,ch[cn].y,x,y,2,NULL,0))==-1) {
		return 0;
	}

        do_walk(cn,dir);
	return 1;
}

int army_back_driver(int cn,struct farmy_data *dat)
{
	if (ch[cn].x==dat->opt1 && ch[cn].y==dat->opt2 && do_walk(cn,(ch[cn].dir+3)%8+1)) return 1;
	else {
		if (ticker-dat->timer>TICKS*5) dat->mission=MIS_FOLLOW;
		else return do_idle(cn,TICKS/2);
	}
	return 0;
}

int army_behind_driver(int cn,struct farmy_data *dat)
{
	int cc,co,x,y;

	cc=dat->leader_cn;

	dx2offset(ch[cc].dir,&x,&y,NULL);
	co=map[(ch[cc].x+x)+(ch[cc].y+y)*MAXMAP].ch;

        dx2offset((ch[co].dir+3)%8+1,&x,&y,NULL);
	if (ch[cn].x!=ch[co].x+x || ch[cn].y!=ch[co].y+y) {
		if (move_driver(cn,ch[co].x+x,ch[co].y+y,0)) return 1;
                say(cn,"cannot go there");
                dat->mission=MIS_FOLLOW;
	}
	return do_attack(cn,ch[co].dir,co);
}

int find_platoon(int cn,struct farmy_data *dat)
{
	int n;

	for (n=0; n<MAXSOLDIER+1; n++) {
		if (dat->platoon[n]==cn) return n;		
	}
	
	return -1;
}

void platoon_exp(int cn,int cm,int amount,int pts,struct farmy_ppd *ppd)
{
        int n,co,rank,plr_rank;
	struct military_ppd *ppd2;

	say(cn,"Well done, %s.",ch[cm].name);

	plr_rank=get_army_rank_int(cm);

        // soldiers
	for (n=0; n<MAXSOLDIER; n++) {
		if (!(co=ppd->soldier[n].cn)) continue;
		if (!ch[co].flags || ch[co].serial!=ppd->soldier[n].serial) continue;

		//say(cn,"giving %s exp",ch[co].name);
		ppd->soldier[n].exp+=amount;
		ppd->soldier[n].exp+=ch[co].exp-ch[co].exp_used;
		ch[co].exp=ch[co].exp_used;

		rank=cbrt((ppd->soldier[n].exp/1000));
		if (rank>=plr_rank) rank=plr_rank-1;

		if (rank<24 && rank>get_army_rank_int(co)) {
			set_army_rank(co,rank);
			ppd->soldier[n].rank=rank;
			update_soldier(co,n,ppd);
			say(cn,"You've been promoted to %s. Congratulations, %s!",get_army_rank_string(co),ch[co].name);
		}
	}
	
	// player
	give_exp(cm,min(level_value(ch[cm].level)/5,amount*4));

	ppd2=set_data(cm,DRD_MILITARY_PPD,sizeof(struct military_ppd));
	ppd2->military_pts+=pts;
	
	rank=cbrt(ppd2->military_pts);
	if (rank<24 && rank>get_army_rank_int(cm)) {
		set_army_rank(cm,rank);
		say(cn,"You've been promoted to %s. Congratulations, %s!",get_army_rank_string(cm),ch[cm].name);
	}
}

#define AT_YESNO	1
#define AT_THANKS	2
#define AT_INSULT	3
#define AT_RELAX	4
#define AT_ENCOURAGE	5
#define AT_AFFIRM	6

void do_emote(int cn,struct farmy_data *dat)
{
	int co,n,score,bestscore=-99999,bestco=0,bestn=0;

        if (dat->emote.lonely>5000) {
		for (n=0; n<MAXSOLDIER+1; n++) {
			co=dat->platoon[n];
			if (!co) continue;
			if ((ch[co].flags&(CF_MALE|CF_FEMALE))==(ch[cn].flags&(CF_MALE|CF_FEMALE))) continue;
			
   			score=dat->emote.likes[n]+dat->emote.talked[n];
			if (score>bestscore) {
				bestscore=score;
				bestco=co;
				bestn=n;
			}
		}
		if (!bestco) return;
		
                co=bestco;
		n=bestn;
		//say(cn,"lonely: score=%d, co=%d, n=%d",bestscore,bestco,n);

		if (dat->emote.likes[n]<10) {
			if (hour>6 && hour<20) {
				say(cn,"Oh, what a nice day it is, %s, isn't it?",ch[co].name);
			} else {
				say(cn,"The nights here are scary, %s, aren't they?",ch[co].name);
			}
			if (dat->emote.talked[n]>-2) dat->emote.likes[n]++;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_YESNO;
		} else if (dat->emote.likes[n]<20) {
                        say(cn,"I like the way you smile, %s.",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]++;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_THANKS;
		}
                dat->emote.lonely/=2;
		dat->emote.last_emote=ticker;		
	}

	if (dat->emote.boredom>10000) {
		for (n=0; n<MAXSOLDIER+1; n++) {
			co=dat->platoon[n];
			if (!co) continue;
			if (cn==co) continue;
			
   			score=dat->emote.talked[n]-dat->emote.likes[n];
			if (score>bestscore) {
				bestscore=score;
				bestco=co;
				bestn=n;
			}
		}
		if (!bestco) return;
		
                co=bestco;
		n=bestn;
		//say(cn,"bored: score=%d, co=%d, n=%d",bestscore,bestco,n);

		if (dat->emote.likes[n]<0) {
			say(cn,"You stink, %s.",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]--;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_INSULT;
		} else if (dat->emote.likes[n]<0) {
			say(cn,"Why don't you go away, %s?",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]--;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_INSULT;
		} else if (dat->emote.likes[n]<10) {
                        say(cn,"Oh, come on, do something, %s.",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]--;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_RELAX;
		} else if (dat->emote.likes[n]<20) {
                        say(cn,"You have a funny face, %s.",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]--;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_RELAX;
		}
                dat->emote.boredom/=2;
		dat->emote.last_emote=ticker;		
	}

	if (dat->emote.fear>1000) {
		for (n=0; n<MAXSOLDIER+1; n++) {
			co=dat->platoon[n];
			if (!co) continue;
			if (cn==co) continue;
			
   			score=dat->emote.likes[n]+dat->emote.talked[n];
			if (score>bestscore) {
				bestscore=score;
				bestco=co;
				bestn=n;
			}
		}
		if (!bestco) return;
		
                co=bestco;
		n=bestn;
		//say(cn,"afraid: score=%d, co=%d, n=%d",bestscore,bestco,n);

		if (dat->emote.likes[n]<10) {
                        say(cn,"Shouldn't we turn back? What do you think, %s?",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]++;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_ENCOURAGE;
		} else if (dat->emote.likes[n]<20) {
                        say(cn,"I'm afraid, %s.",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]++;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_ENCOURAGE;
		}
                dat->emote.fear/=2;
		dat->emote.last_emote=ticker;		
	}

	if (dat->emote.praise>100) {
		for (n=0; n<MAXSOLDIER+1; n++) {
			co=dat->platoon[n];
			if (!co) continue;
			if ((ch[co].flags&(CF_MALE|CF_FEMALE))==(ch[cn].flags&(CF_MALE|CF_FEMALE))) continue;
			
   			score=dat->emote.likes[n]+dat->emote.talked[n]+RANDOM(30);
			if (score>bestscore) {
				bestscore=score;
				bestco=co;
				bestn=n;
			}
		}
		if (!bestco) return;
		
                co=bestco;
		n=bestn;
		//say(cn,"praise: score=%d, co=%d, n=%d",bestscore,bestco,n);

		if (dat->emote.likes[n]<10) {
                        say(cn,"Ha! I'm the greatest, right, %s?",ch[co].name);
			if (dat->emote.talked[n]>-2) dat->emote.likes[n]++;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_AFFIRM;
		} else if (dat->emote.likes[n]<20) {
                        say(cn,"I'm becoming a great soldier, %s.",ch[co].name);
                        if (dat->emote.talked[n]>-2) dat->emote.likes[n]++;
			dat->emote.talked[n]--;
			dat->emote.answer_timer=ticker;
			dat->emote.answer_cn=co;
			dat->emote.answer_type=AT_AFFIRM;
		}
                dat->emote.praise/=2;
		dat->emote.last_emote=ticker;		
	}
}

void got_emote(int cn,int co,int slot,int nr,struct farmy_data *dat)
{
	switch(nr) {
		case QA_YES:	// yes etc.
			if (dat->emote.answer_type!=AT_YESNO || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Yes what, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=2;
			break;
		case QA_NO:	// no etc.
			if (dat->emote.answer_type!=AT_YESNO || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"No what, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			break;
		case QA_NICEDAY:	// nice days, scary nights
			if (dat->emote.likes[slot]>0) {
				say(cn,"Yes, %s.",ch[co].name);
				dat->emote.likes[slot]++;
			} else {
				say(cn,"No, %s.",ch[co].name);
                        }
			dat->emote.talked[slot]++;
			dat->emote.lonely-=200;
			break;
		case QA_THANKS:	// thanks etc
			if (dat->emote.answer_type!=AT_THANKS || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Thanks? Thanks for what, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=2;
			break;
		case QA_YOUSTINK:
			if (dat->emote.likes[slot]>0) {
				say(cn,"Why are you so mean to me, %s?",ch[co].name);
				dat->emote.likes[slot]-=5;
			} else {
				say(cn,"And you smell like a ratling, %s!",ch[co].name);
				dat->emote.likes[slot]--;
                        }
			dat->emote.talked[slot]++;
			dat->emote.boredom-=2000;
			break;
		case QA_GOAWAY:
			if (dat->emote.likes[slot]>0) {
				say(cn,"What's up with you, %s?",ch[co].name);
				dat->emote.likes[slot]-=2;
			} else {
				say(cn,"Shut up, %s!",ch[co].name);
				dat->emote.likes[slot]--;
                        }
			dat->emote.talked[slot]++;
			dat->emote.boredom-=2000;
			break;
		case QA_DOSOMETHING:
			if (dat->emote.likes[slot]>0) {
				say(cn,"Yeah, I hope we'll do something soon, %s.",ch[co].name);				
			} else {
				say(cn,"Stop bothering me, %s!",ch[co].name);
				dat->emote.likes[slot]--;
                        }
			dat->emote.talked[slot]++;
			dat->emote.boredom-=2000;
			break;
		case QA_FUNNYFACE:			
			if (dat->emote.likes[slot]>0) {
				say(cn,"I'm bored too, %s, let's not fight.",ch[co].name);				
			} else {
				say(cn,"Oh boy! Please be quiet, %s.",ch[co].name);
				dat->emote.likes[slot]--;
                        }
			dat->emote.talked[slot]++;
			dat->emote.boredom-=2000;
			break;
		case QA_LIKESMILE:
			if (dat->emote.likes[slot]>5) {
				say(cn,"Why, thank you, %s.",ch[co].name);
				dat->emote.likes[slot]++;
			} else {
				say(cn,"Is that so, %s?",ch[co].name);
                        }
			dat->emote.talked[slot]++;
			dat->emote.lonely-=200;
			break;
		case QA_TURNBACK:
			if (dat->emote.likes[slot]>-5) {
				say(cn,"It's not that bad, %s.",ch[co].name);
				dat->emote.likes[slot]++;
			} else {
				say(cn,"Are you afraid, %s?",ch[co].name);
				dat->emote.likes[slot]--;
                        }
			dat->emote.talked[slot]++;
			dat->emote.fear-=20;
			break;
		case QA_GREATEST:
			if (dat->emote.likes[slot]>5) {
				say(cn,"You're a tough fellow, %s!",ch[co].name);				
			} else {
				say(cn,"Oh, be quiet, %s, you bigmouth!",ch[co].name);
				dat->emote.likes[slot]-=2;
                        }
			dat->emote.talked[slot]++;
			dat->emote.praise+=10;
			break;
		case QA_GREATSOLDIER:
			if (dat->emote.likes[slot]>0) {
				say(cn,"One day you'll be a great soldier, %s.",ch[co].name);				
			} else {
				say(cn,"I don't think so, %s.",ch[co].name);
				dat->emote.likes[slot]-=2;
                        }
			dat->emote.talked[slot]++;
			dat->emote.praise+=10;
			break;
		case QA_AFRAID:
			if (dat->emote.likes[slot]>0) {
				say(cn,"There's no need to be afraid, %s.",ch[co].name);
				dat->emote.likes[slot]+=2;
			} else {
				say(cn,"Shut up you, %s, you coward!",ch[co].name);
				dat->emote.likes[slot]-=2;
                        }
			dat->emote.talked[slot]++;
			dat->emote.fear-=20;
			break;
		case QA_WHYMEAN:
			if (dat->emote.answer_type!=AT_INSULT || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Mean? Why am I mean, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=3;
			break;
		case QA_SMELLRATLING:
			if (dat->emote.answer_type!=AT_INSULT || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Oh yeah? Is that so, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]--;
			break;
		case QA_WHATSUP:
			if (dat->emote.answer_type!=AT_INSULT || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Oh, nothing, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]++;
			break;
		case QA_SHUTUP:
			if (dat->emote.answer_type!=AT_INSULT || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Why? I didn't say anything, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]--;
			break;
		case QA_DOSOON:
			if (dat->emote.answer_type!=AT_RELAX || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Oh? Oh, that's fine, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]++;
			break;
		case QA_STOPBOTHER:
			if (dat->emote.answer_type!=AT_RELAX || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"I am not bothering you, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]--;
			break;
		case QA_NOTFIGHT:
			if (dat->emote.answer_type!=AT_RELAX || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"I wasn't trying to pick a fight, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]++;
			break;
		case QA_BEQUIET:
			if (dat->emote.answer_type!=AT_RELAX || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"But I am quiet, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]--;
			break;
		case QA_ISTHATSO:
			if (dat->emote.answer_type!=AT_THANKS || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Hu? What is how, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]-=3;
			break;
		case QA_NOTTHATBAD:
			if (dat->emote.answer_type!=AT_ENCOURAGE || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"I didn't say it is, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=2;
			break;
		case QA_YOUAFRAID:
			if (dat->emote.answer_type!=AT_ENCOURAGE || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Afraid? Me? That's silly, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]-=2;
			break;
		case QA_NONEED:
			if (dat->emote.answer_type!=AT_ENCOURAGE || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Afraid? Me? That's silly, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=2;
			break;
		case QA_COWARD:
			if (dat->emote.answer_type!=AT_ENCOURAGE || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"Why are you calling me a coward, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]-=2;
			break;
		case QA_TOUGHFELLOW:
			if (dat->emote.answer_type!=AT_AFFIRM || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"That's nice to hear, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=2;
			break;
		case QA_QUIETBIGMOUTH:
			if (dat->emote.answer_type!=AT_AFFIRM || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"What? But I didn't say anything, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]-=2;
			break;
		case QA_ONEDAY:
			if (dat->emote.answer_type!=AT_AFFIRM || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"That's very nice to hear, %s.",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]+=2;
			break;
		case QA_DONTTHINKSO:
			if (dat->emote.answer_type!=AT_AFFIRM || dat->emote.answer_cn!=co || ticker-dat->emote.answer_timer>TICKS*30) {
				say(cn,"You don't think what, %s?",ch[co].name);
				break;
			}
			dat->emote.talked[slot]++;
                        dat->emote.answer_cn=0;
			dat->emote.answer_timer=0;
			dat->emote.likes[slot]--;
			break;
	}
}

void fdemon_army(int cn,int ret,int lastact)
{
	struct farmy_data *dat;
        struct msg *msg,*next;
	int co,bless=0,heal=0,res,friend;

	dat=set_data(cn,DRD_FARMYDATA,sizeof(struct farmy_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
                                ch[cn].arg=NULL;
			}
			dat->mission=MIS_FOLLOW;
			fight_driver_set_dist(cn,0,20,0);
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			co=msg->dat1;

			// keep leader_cn current
			if ((ch[co].flags&CF_PLAYER) && ch[co].group==ch[cn].group) {
				dat->leader_cn=co;				
			}

			if (ch[co].group==ch[cn].group && cn!=co) {
			
                            if (ch[cn].value[0][V_BLESS] &&
				ch[cn].value[1][V_BLESS]>ch[co].value[1][V_BLESS] &&
				ch[cn].mana>=BLESSCOST &&
				may_add_spell(co,IDR_BLESS) &&
				char_see_char(cn,co))
				    bless=co;
			    if (ch[cn].value[0][V_HEAL] &&
				ch[co].hp<ch[co].value[1][V_HP]*POWERSCALE/3 &&
				ch[cn].mana>=POWERSCALE*4 &&
				char_see_char(cn,co))
				    heal=co;
			}

		}

		if (msg->type==NT_TEXT) {
			
			res=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);

			co=msg->dat3;

			// emotes
			if ((friend=find_platoon(co,dat))==-1) {
				remove_message(cn,msg);
				continue;
			}
			if (res>=20) got_emote(cn,co,friend,res,dat);

			// commands
			if (co!=dat->leader_cn) {
				remove_message(cn,msg);
				continue;
			}
			switch(res) {
				case 2:
					say(cn,"Sir! Yes, Sir, %s, Sir!",get_army_rank_string(co));
					dat->mission=MIS_FOLLOW;
					break;
				case 3:
					say(cn,"Will do, %s.",get_army_rank_string(co));
					dat->mission=MIS_BACK;
					dat->opt1=ch[cn].x;
					dat->opt2=ch[cn].y;
					dat->timer=ticker;
					break;
				case 4:
					say(cn,"Aye Aye %s, Sir.",get_army_rank_string(co));
					dat->mission=MIS_RETREAT;
					break;
				case 5:
					say(cn,"So be it, %s.",get_army_rank_string(co));
					dat->mission=MIS_FRONT;
					break;
				case 6:
					say(cn,"I'll go rub his back, %s.",get_army_rank_string(co));
					dat->mission=MIS_BEHIND;
					break;				
				case 7:
					say(cn,"cuddly=%d, lonely=%d, angst=%d, fear=%d, bore=%d, boredom=%d, bigmouth=%d, praise=%d, like=%d/%d/%d %d, replied=%d/%d/%d %d",
					    dat->emote.cuddly,
					    dat->emote.lonely,
					    dat->emote.angst,
					    dat->emote.fear,
					    dat->emote.bore,
					    dat->emote.boredom,
					    dat->emote.bigmouth,
					    dat->emote.praise,
					    dat->emote.likes[0],
					    dat->emote.likes[1],
					    dat->emote.likes[2],
					    dat->emote.likes[3],
					    dat->emote.talked[0],
					    dat->emote.talked[1],
					    dat->emote.talked[2],
					    dat->emote.talked[3]);
					break;
			}
		}

		if (msg->type==NT_DEAD && msg->dat2==cn) {	// we killed someone
			dat->emote.praise+=dat->emote.bigmouth;
		}

		standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2) {
		dat->emote.fear+=dat->emote.angst;
		dat->emote.boredom-=500;
	}

	if (dat->leader_cn) {
		if (!(ch[dat->leader_cn].flags) || ch[dat->leader_cn].group!=ch[cn].group) {	// we lost our master
			//charlog(cn,"going to disintegrate - leader lost");
			remove_destroy_char(cn);
                        return;
		}
		switch(dat->mission) {
			case MIS_FOLLOW:	if (army_follow_driver(cn,dat,10)) {
							dat->closeup=1;
							return;
						} else break;
			case MIS_BACK:		if (army_back_driver(cn,dat)) {
							dat->emote.boredom-=10;
							return;
						} else break;
			case MIS_RETREAT:	if (army_follow_driver(cn,dat,3)) {
							return;
						} else break;
			case MIS_BEHIND:	if (army_behind_driver(cn,dat)) {
							dat->emote.boredom-=250;
							return;
						} else break;
			case MIS_FRONT:		if (army_front_driver(cn,dat,10)) {
							return;
						} else break;
		}
	}

        fight_driver_update(cn);

	if (heal && do_heal(cn,heal)) {
		dat->emote.boredom-=100;
		return;
	}
	if (bless && do_bless(cn,bless)) {
		return;
	}
	
        if (!dat->closeup && fight_driver_attack_visible(cn,0)) {
		dat->emote.boredom-=25;
		return;
	}

	if (dat->leader_cn) {
		switch(dat->mission) {
			case MIS_FOLLOW:	if (army_follow_driver(cn,dat,3)) {
							return;
						} else { dat->closeup=0; break; }
			case MIS_FRONT:		if (army_front_driver(cn,dat,3)) {
							return;
						} else break;
		}
	}

	dat->emote.lonely+=dat->emote.cuddly;
	dat->emote.boredom+=dat->emote.bore;
	if (ticker-dat->emote.last_emote>TICKS*10) {
		do_emote(cn,dat);
	}

        if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

	//say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}


void fdemon_boss(int cn,int ret,int lastact)
{
	struct farmy_ppd *ppd;
        struct msg *msg,*next;
	int co,res,n,cnt,bit,cnt2;
	char *ptr;

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

                        if ((ch[co].flags&CF_PLAYER) && char_dist(cn,co)<16 && char_see_char(cn,co) && ch[cn].driver!=CDR_LOSTCON &&
			    (ppd=set_data(co,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && realtime-ppd->boss_timer>5) {
				//say(cn,"stage=%d",ppd->boss_stage);
				switch(ppd->boss_stage) {
					case 0:		if (get_army_rank_int(co)<2) {
								say(cn,"Ah, %s. The governer of Aston has some missions for you. You'd better head back there and do those first.",ch[co].name);
							} else {
								say(cn,"Welcome, %s, to our underground headquarters. I am the commander of the underground army. We are trying to stop the demon's progress here, before they invade Aston again.",ch[co].name);
								ppd->boss_stage++;
							}
							ppd->boss_timer=realtime;
							break;
					case 1:		say(cn,"Unfortunately, I have a lack of good leaders. But you can °c4take°c0 some men to explore the underground and solve your missions. Just be sure to °c4drop°c0 them off here again before you leave.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 2:		say(cn,"These soldiers have been trained to obey some easy commands: 'Follow' makes them follow you. 'Front' makes them walk in front of you. With 'Back', they'll take one step back. They follow you more closely if you order a 'retreat'. And you can make them attack your enemy from 'behind'.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 3:		say(cn,"It is up to you if you want their help on your missions. I recommend you take them along, the enemies are numerous, and some are quite dangerous.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 4:		say(cn,"Alright, %s, your first mission is to reach the Ancient Defense Station number 1, and refuel it. You can find some of the power-crystals of the ancients in the big hall to the south-east. Take plenty, they're growing fast.",get_army_rank_string(co));
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 5:		break;	// waiting for mission solve
					case 6:		platoon_exp(cn,co,1000,2,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 7:		say(cn,"Your next mission is to activate Defense Station 3. That is the next station north-west of the one you activated in your last mission." );
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 8:		break;	// waiting for mission solve
					case 9:		platoon_exp(cn,co,1000,2,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 10:	say(cn,"I know, %s, you're tired of doing these simple missions, but they have to be done. We can't hold the demons back without those Defense Stations. %s, your mission is to activate Defense Station 2. It is located north-east of station 1.",ch[co].name,get_army_rank_string(co));
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 11:	break;	// waiting for solve
					case 12:	platoon_exp(cn,co,1000,2,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 13:	say(cn,"Our knowledge of the underground system here is pretty limited, %s. Your next mission is to find and activate the Defense Stations 4 and 5. I assume that these are pretty close to number 2 and 3.",ch[co].name);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							ppd->boss_counter=0;
							break;
					case 14:	break;	// waiting for solve
					case 15:	platoon_exp(cn,co,2000,4,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 16:	say(cn,"I've been getting reports about some beings we called 'Fire Golems', %s. It seems these beasts are very hard to kill. I've lost many good men to them. The few who made it back reported that only an attack in the back had any success. Your next mission is to slay one of those 'Fire Golems'. You can find them north-west of Defense Station 3. Oh, %s, may I remind you, that our soldiers have been trained to obey the commands 'follow', 'retreat', 'behind', 'front' and 'back'?",ch[co].name,get_army_rank_string(co));
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							ppd->boss_counter=0;
							break;
					case 17:	break;	// waiting for solve
					case 18:	platoon_exp(cn,co,3000,4,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 19:	say(cn,"Scouts have found a room made by the ancients where a lot of small containers are stored. It is located in the north-western part of the underground. I want you to go there, aquire some of these containers and find out what they do. Good luck, %s.",ch[co].name);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							ppd->boss_counter=0;
							break;
					case 20:	break;	// waiting for solve
					case 21:	platoon_exp(cn,co,4000,5,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 22:	say(cn,"Now that we know what these containers can hold, we need to find out what to do with that golem blood, %s. Your mission, %s, is to find a use for it.",ch[co].name,get_army_rank_string(co));
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							ppd->boss_counter=0;
							break;
					case 23:	break;	// waiting for solve
					case 24:	platoon_exp(cn,co,4000,5,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 25:	say(cn,"So that's how we can pass those lava fields. Well, since you can cross them now, %s, I want you to activate Defense Station 6. It is located north-east of number 5.",ch[co].name);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							ppd->boss_counter=0;
							break;
					case 26:	break;	// waiting for solve
					case 27:	platoon_exp(cn,co,4000,5,ppd);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 28:	ppd->boss_stage++;
							ppd->boss_counter=0;	// fall thru intended
					case 29:	say(cn,"I do not have a specific mission for you at the moment, %s, but I want you to scout the whole underground and find all the Defense Stations. When you find one, activate it, and report back from time to time. You don't have to come back for every single new station you find, though.",ch[co].name);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 30:        for (n=cnt=cnt2=0; n<32; n++) {
								bit=1<<n;
								if ((ppd->boss_counter&bit)) {
									 if (!(ppd->boss_reported&bit)) cnt++;
									 cnt2++;
								}
							}
							if (cnt2>=26) ppd->boss_stage++;
							if (!cnt) break;
                                                        say(cn,"Ah, %s. I hear you have found %d new Defense Stations. So you've found %d stations now.",ch[co].name,cnt,cnt2);
							platoon_exp(cn,co,2000*cnt,2*cnt,ppd);
							ppd->boss_timer=realtime;
							ppd->boss_reported=ppd->boss_counter;
                                                        break;
					case 31:	say(cn,"It seems we know all stations here now. Thou wert most helpful, %s.",ch[co].name);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 32:	say(cn,"I do not have any more missions for thee. But thou might want to explore the underworld further. This is but one part of it, and I feel that the cause of all this evil lies further to the north.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
	
	
					
				}
			}
		}

		if (msg->type==NT_TEXT) {
			if ((co=msg->dat3)==cn) {
				remove_message(cn,msg);
				continue;
			}
			res=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3);
			if (res==8 && (ch[co].flags&CF_PLAYER) && char_dist(cn,co)<16 && char_see_char(cn,co) &&
			    (ppd=set_data(co,DRD_FARMY_PPD,sizeof(struct farmy_ppd)))) {
                                switch(ppd->boss_stage) {
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:		
					case 5:		ppd->boss_stage=0; ppd->boss_timer=0; break;
					case 6: 	break;	// exp give-out
					case 7:
					case 8:		ppd->boss_stage=7; ppd->boss_timer=0; break;
					case 9: 	break;	// exp give-out
					case 10:
					case 11: 	ppd->boss_stage=10; ppd->boss_timer=0; break;
					case 12: 	break; // exp give-out
					case 13:
					case 14: 	ppd->boss_stage=13; ppd->boss_timer=0; break;
					case 15: 	break; // exp give-out
					case 16:
					case 17: 	ppd->boss_stage=16; ppd->boss_timer=0; break;
					case 18: 	break; // exp give-out
					case 19:
					case 20: 	ppd->boss_stage=19; ppd->boss_timer=0; break;
					case 21: 	break; // exp give-out
					case 22:
					case 23: 	ppd->boss_stage=22; ppd->boss_timer=0; break;
					case 24: 	break; // exp give-out
					case 25:
					case 26: 	ppd->boss_stage=25; ppd->boss_timer=0; break;
					case 27: 	break; // exp give-out
					case 28:	break; // just used to reset the counter
					case 29:
					case 30: 	ppd->boss_stage=29; ppd->boss_timer=0; break;
					case 31:
					case 32: 	ppd->boss_stage=31; ppd->boss_timer=0; break;
				}
                        }
			ptr=(char*)msg->dat2;
			while (*ptr && *ptr!='"') ptr++;
			if (*ptr!='"') {
				remove_message(cn,msg);
				continue;
			}
                        if (strcasestr((char*)msg->dat2,"take") && char_dist(cn,co)<16) {
				drop_soldiers(co);
                                take_soldiers(co);
			} else if (strcasestr((char*)msg->dat2,"drop") && char_dist(cn,co)<16) {
				drop_soldiers(co);
			}
		}

                //standard_message_driver(cn,msg,dat->aggressive,dat->helper);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

	//say(cn,"i am %d",cn);
        do_idle(cn,TICKS);
}

static int find_loader(int in,int nr)
{
	int in2,bestdist=99,bestin=0,dist;

	for (in2=1; in2<MAXITEM; in2++) {
		if (it[in2].driver==IDR_FDEMONLOADER && it[in2].drdata[0]==nr && (dist=abs(it[in].x-it[in2].x)+abs(it[in].y-it[in2].y))<bestdist) {
			bestdist=dist;
			bestin=in2;
		}
	}

	return bestin;
}

void fdemon_light(int in,int cn)
{
	int in2,in3,in4,light,sprite,pwr;

	if (cn) return;

	in2=*(unsigned int*)(it[in].drdata+0);
	in3=*(unsigned int*)(it[in].drdata+4);
	in4=*(unsigned int*)(it[in].drdata+8);

	if (!in2) {
                in2=*(unsigned int*)(it[in].drdata+0)=find_loader(in,1);
		in3=*(unsigned int*)(it[in].drdata+4)=find_loader(in,2);
		in4=*(unsigned int*)(it[in].drdata+8)=find_loader(in,3);

		//xlog("light at %d,%d found loader %d at %d,%d",it[in].x,it[in].y,in2,it[in2].x,it[in2].y);
	}
	if (!in2) return;

	pwr=max(max(*(unsigned short*)(it[in2].drdata+1),*(unsigned short*)(it[in3].drdata+1)),*(unsigned short*)(it[in4].drdata+1));

        if (pwr) {
		light=200;
		sprite=14192;
	} else {
		light=0;
		sprite=14189;
	}

	if (it[in].sprite!=sprite) {
		it[in].sprite=sprite;
		set_sector(it[in].x,it[in].y);
	}

	if (it[in].mod_value[0]!=light) {
		remove_item_light(in);
		it[in].mod_value[0]=light;
		add_item_light(in);
	}
	call_item(it[in].driver,in,cn,ticker+TICKS);
}

void fdemon_loader(int in,int cn)
{
	struct farmy_ppd *ppd;
	int pwr,sprite,in2,ani,m,gsprite,pwr2;

        pwr=*(unsigned short*)(it[in].drdata+1);	// power status
	ani=it[in].drdata[3];				// animation status
	pwr2=*(unsigned short*)(it[in].drdata+4);	// power to use once animiation runs out

	if (cn) {	// player using item
		if (pwr || ani) {
			if (ch[cn].flags&CF_FDEMON) {
				*(unsigned short*)(it[in].drdata+1)=0;
				it[in].drdata[3]=0;
				*(unsigned short*)(it[in].drdata+4)=0;
				return;
			}
			if (ch[cn].citem) log_char(cn,LOG_SYSTEM,0,"There is already a crystal, you cannot add another item.");				
			else log_char(cn,LOG_SYSTEM,0,"The crystal is stuck.");
			return;
		}
		if (!(in2=ch[cn].citem)) {
			log_char(cn,LOG_SYSTEM,0,"Nothing happens.");
			return;
		}
		if (it[in2].ID!=IID_AREA8_REDCRYSTAL) {
			log_char(cn,LOG_SYSTEM,0,"That doesn't fit.");
			return;
		}
		pwr2=(int)(it[in2].drdata[0])*100;
		ani=7;
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"dropped into fdemonloader");
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		destroy_item(in2);
		sound_area(it[in].x,it[in].y,41);

		if (it[in].drdata[6]==1 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=0 && ppd->boss_stage<=5) {
			log_char(cn,LOG_SYSTEM,0,"You've solved your mission. Now head back to the commander to claim your reward!");
			ppd->boss_stage=6;
		}
		if (it[in].drdata[6]==3 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=7 && ppd->boss_stage<=8) {
			log_char(cn,LOG_SYSTEM,0,"You've solved your mission. Now head back to the commander to claim your reward!");
			ppd->boss_stage=9;
		}
		if (it[in].drdata[6]==2 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=10 && ppd->boss_stage<=11) {
			log_char(cn,LOG_SYSTEM,0,"You've solved your mission. Now head back to the commander to claim your reward!");
			ppd->boss_stage=12;
		}
		if (it[in].drdata[6]==4 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=13 && ppd->boss_stage<=14) {
			ppd->boss_counter|=1;
			if ((ppd->boss_counter&3)==3) {
				log_char(cn,LOG_SYSTEM,0,"You've solved your mission. Now head back to the commander to claim your reward!");
				ppd->boss_stage=15;
			} else {
				log_char(cn,LOG_SYSTEM,0,"You've solved the first part of your mission. Now go find that other station.!");				
			}
		}
		if (it[in].drdata[6]==5 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=13 && ppd->boss_stage<=14) {
			ppd->boss_counter|=2;
			if ((ppd->boss_counter&3)==3) {
				log_char(cn,LOG_SYSTEM,0,"You've solved your mission. Now head back to the commander to claim your reward!");
				ppd->boss_stage=15;
			} else {
				log_char(cn,LOG_SYSTEM,0,"You've solved the first part of your mission. Now go find that other station.!");				
			}
		}
		if (it[in].drdata[6]==6 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=25 && ppd->boss_stage<=26) {
			log_char(cn,LOG_SYSTEM,0,"You've solved your mission. Now head back to the commander to claim your reward!");
			ppd->boss_stage=27;
		}
		if (it[in].drdata[6]>=7 && it[in].drdata[6]<=35 && (ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) && ppd->boss_stage>=28) {
			int bit;

			bit=1<<(it[in].drdata[6]-7);
			if (!(ppd->boss_counter&bit)) {
				ppd->boss_counter|=bit;
				log_char(cn,LOG_SYSTEM,0,"You've found Defense Station number %d.",it[in].drdata[6]);
			}
		}
	} else {	// timer call
                if (ani && --ani==0) pwr=pwr2;
		if (pwr) {
			pwr--;
			//notify_area(it[in].x,it[in].y,NT_NPC,NTID_FDEMON,MSG_LOADER,in);
		}
		
		call_item(it[in].driver,in,0,ticker+TICKS);
	}
	
	if (!ani) pwr2=pwr;
	
	*(unsigned short*)(it[in].drdata+4)=pwr2;
	it[in].drdata[3]=ani;
	*(unsigned short*)(it[in].drdata+1)=pwr;

	m=it[in].x+it[in].y*MAXMAP;
	gsprite=map[m].gsprite&0x0000ffff;
	
	if (ani) gsprite|=(59028-ani)<<16;
	else if (pwr2) gsprite|=59029<<16;
	else gsprite|=59021<<16;

	if (pwr2) sprite=59030+9-(min(2880,pwr2)/320);
	else sprite=14234;

	if (it[in].sprite!=sprite || map[m].gsprite!=gsprite) {
		it[in].sprite=sprite;
		map[m].gsprite=gsprite;
		set_sector(it[in].x,it[in].y);
		if (sprite==14234) sound_area(it[in].x,it[in].y,43);
	}
}

//------
// sector offset table
static char offx[25]={ 0,-8, 8, 0, 0,-8,-8, 8, 8,-16, 16,  0,  0, -8, -8,  8,  8,-16,-16, 16, 16,-16,-16, 16, 16};
static char offy[25]={ 0, 0, 0,-8, 8,-8, 8,-8, 8,  0,  0,-16, 16,-16, 16,-16, 16, -8,  8, -8,  8,-16, 16,-16, 16};

struct shot
{
	int co;
	int ox,oy;
	int tx,ty;
};

static int can_hit(int in,int co,int frx,int fry,int tox,int toy)
{
	int dx,dy,x,y,n,cx,cy,m;

	x=frx*1024+512;
	y=fry*1024+512;

	dx=(tox-frx);
	dy=(toy-fry);

        if (abs(dx)<2 && abs(dy)<2) return 0;

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

int find_target(int in,int x,int y,int dx,int dy,struct shot *shot)
{
	int n,co,dist,bestdist=99;
	int ox=0,oy=0,tx=0,ty=0,dir,eta,left,step,m,dx2,dy2;

	for (n=0; n<25;  n++) {
		for (co=getfirst_char_sector(x+offx[n],y+offy[n]); co; co=ch[co].sec_next) {
			if ((ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE))) continue;
                        if (abs(ch[co].x-x)>17 || abs(ch[co].y-y)>17) continue;

			if (abs(ch[co].x-x)>abs(ch[co].y-y)) {
				if (ch[co].x>x) ox=1;
				if (ch[co].x<x) ox=-1;
				oy=0;
			} else {
				if (ch[co].y>y) oy=1;
				if (ch[co].y<y) oy=-1;
				ox=0;
			}
			if (dx!=ox || dy!=oy) continue;

			if (ch[co].action!=AC_WALK) { tx=ch[co].x; ty=ch[co].y; }
			else {
	
				dir=ch[co].dir;
				dx2offset(dir,&dx2,&dy2,NULL);
				dist=map_dist(it[in].x,it[in].y,ch[co].x,ch[co].y);
			
				eta=dist*1.5;
				
				left=ch[co].duration-ch[co].step;
				step=ch[co].duration;
			
				eta-=left;
				if (eta<=0) { tx=ch[co].tox; ty=ch[co].toy; }
				else {
					for (m=1; m<10; m++) {
						eta-=step;
						if (eta<=0) { tx=ch[co].x+dx2*m; ty=ch[co].y+dy2*m; break; }
					}
			
					// too far away, time-wise to make any prediction. give up.
					if (m==10) { tx=ch[co].x; ty=ch[co].y; }
				}
			}

			if (!can_hit(in,co,it[in].x+ox,it[in].y+oy,ch[co].x,ch[co].y)) continue;

			dist=abs(ch[co].x-x)+abs(ch[co].y-y);
			if (dist<bestdist) {
				bestdist=dist;
				shot->co=co;
				shot->ox=ox;
				shot->oy=oy;
				shot->tx=tx;
				shot->ty=ty;
			}
		}
	}

	if (bestdist==99) return 0;
	
	return 1;
}

void fdemon_cannon(int in,int cn)
{
	int in2,in3,in4,pwr,spwr,lpwr;
        struct shot shot;

	in2=*(unsigned int*)(it[in].drdata+0);
	in3=*(unsigned int*)(it[in].drdata+4);
	in4=*(unsigned int*)(it[in].drdata+8);

	if (!in2) {
                in2=*(unsigned int*)(it[in].drdata+0)=find_loader(in,1);
		in3=*(unsigned int*)(it[in].drdata+4)=find_loader(in,2);
		in4=*(unsigned int*)(it[in].drdata+8)=find_loader(in,3);

		//xlog("light at %d,%d found loader %d at %d,%d",it[in].x,it[in].y,in2,it[in2].x,it[in2].y);
	}
	if (!in2) return;

	pwr=max(max(*(unsigned short*)(it[in2].drdata+1),*(unsigned short*)(it[in3].drdata+1)),*(unsigned short*)(it[in4].drdata+1));

        if (cn) {
		if (!pwr) {
			log_char(cn,LOG_SYSTEM,0,"It seems lifeless.");
			return;
		}
		return;
	} else {
                if (pwr) {
			int dx,dy;
			
			dx2offset(it[in].drdata[12],&dx,&dy,NULL);
			if (find_target(in,it[in].x,it[in].y,dx,dy,&shot)) {

				spwr=pwr/50+1;
				lpwr=spwr;
				
				create_edemonball(cn,it[in].x+shot.ox,it[in].y+shot.oy,shot.tx,shot.ty,spwr,2);
	
				if (*(unsigned short*)(it[in2].drdata+1)>lpwr) { *(unsigned short*)(it[in2].drdata+1)-=lpwr; lpwr=0; }
				else { lpwr-=*(unsigned short*)(it[in2].drdata+1); *(unsigned short*)(it[in2].drdata+1)=0; }
				if (*(unsigned short*)(it[in3].drdata+1)>lpwr) { *(unsigned short*)(it[in3].drdata+1)-=lpwr; lpwr=0; }
				else { lpwr-=*(unsigned short*)(it[in3].drdata+1); *(unsigned short*)(it[in3].drdata+1)=0; }
				if (*(unsigned short*)(it[in4].drdata+1)>lpwr) { *(unsigned short*)(it[in4].drdata+1)-=lpwr; lpwr=0; }
				else { lpwr-=*(unsigned short*)(it[in4].drdata+1); *(unsigned short*)(it[in4].drdata+1)=0; }
			}
			if (!(it[in].sprite&1)) {
				it[in].sprite|=1;
				set_sector(it[in].x,it[in].y);
			}
		} else {
			if (it[in].sprite&1) {
				it[in].sprite&=~1;
				set_sector(it[in].x,it[in].y);
			}
		}
		call_item(it[in].driver,in,0,ticker+TICKS);
	}
}

void fdemon_gate(int in,int cn)
{
	int n,co,ser,level,rate;
	char name[80];

	if (cn) return;

	level=it[in].drdata[0];
	rate=it[in].drdata[1];

	for (n=0; n<3; n++) {
		co=*(unsigned short*)(it[in].drdata+4+n*4);
		ser=*(unsigned short*)(it[in].drdata+6+n*4);
	
		if (!co || !ch[co].flags || (unsigned short)ch[co].serial!=(unsigned short)ser) {
			sprintf(name,"fdemon%ds",level);
			co=create_char(name,0);
			if (!co) break;
	
			if (item_drop_char(in,co)) {
				
				ch[co].tmpx=ch[co].x;
				ch[co].tmpy=ch[co].y;
				
				update_char(co);
				
				ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
				ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
				ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
				
				ch[co].dir=DX_RIGHTDOWN;
	
				*(unsigned short*)(it[in].drdata+4+n*4)=co;
				*(unsigned short*)(it[in].drdata+6+n*4)=ch[co].serial;
				break;	// only one spawn per call
			} else {
				destroy_char(co);
				break;
			}
		}
	}
	call_item(it[in].driver,in,0,ticker+rate*TICKS);
}

void fdemon_farm(int in,int cn)
{
	int str,step,size,crystal,m,in2,nr;
	char buf[80];

	step=it[in].drdata[0];
	size=it[in].drdata[1];
	str=it[in].drdata[2];

	if (str<size) { str+=step; crystal=0; nr=0; }
	else {
		if (size>=48) { crystal=59043; nr=5; }
		else if (size>=40) { crystal=59042; nr=4; }
		else if (size>=32) { crystal=59041; nr=3; }
		else if (size>=24) { crystal=59040; nr=2; }
		else { crystal=59020; nr=1; }
	}

	if (cn) {
		if (ch[cn].citem) {
			log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
			return;
		}
		if (!crystal) {
			log_char(cn,LOG_SYSTEM,0,"There's nothing to take yet (%d of %d).",str,size);
			return;
		}
		sprintf(buf,"fdemon_crystal%d",nr);

		in2=create_item(buf);
		if (!in2) {
			log_char(cn,LOG_SYSTEM,0,"BUG # 31992 mark %d",nr);
			return;
		}

		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from fdemon farm");
		ch[cn].citem=in2;
		it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;

		crystal=nr=str=0;
	}

        it[in].drdata[2]=str;

	m=it[in].x+it[in].y*MAXMAP;
	if (crystal!=((map[m].fsprite>>16)&0xffff)) {
		map[m].fsprite&=0x0000ffff;
		map[m].fsprite|=crystal<<16;
		set_sector(it[in].x,it[in].y);
	}

	if (!cn) call_item(it[in].driver,in,0,ticker+TICKS*2);
}

void fdemon_waypoint(int in,int cn)
{
	if (cn) {
		if (ch[cn].flags&CF_FDEMON) {
			it[in].drdata[0]=0;
			it[in].sprite=14202;
			set_sector(it[in].x,it[in].y);
		} else {
			it[in].drdata[0]=1;
			it[in].sprite=14200;
			set_sector(it[in].x,it[in].y);
			*(unsigned int*)(it[in].drdata+4)=cn;
			*(unsigned int*)(it[in].drdata+8)=ch[cn].ID;
		}
	}
	//notify_area(it[in].x,it[in].y,NT_NPC,NTID_FDEMON,MSG_WAYPOINT,in);
	
	call_item(it[in].driver,in,0,ticker+TICKS*3);
}

//----------------------------

#define MAXWAY		50

struct waypoint
{
	int x,y;

        int last_enemy;

        int left,right,up,down;

} wp[MAXWAY];

int maxway=1;

void find_waypoints(void)
{
	int in,n,m,dx,dy;

	// find waypoints
	for (in=1; in<MAXITEM; in++) {
		if (it[in].driver==IDR_FDEMONWAYPOINT && maxway<MAXWAY) {
			wp[maxway].x=it[in].x;
			wp[maxway].y=it[in].y;
                        wp[maxway].last_enemy=0;
                        //xlog("found %d at %d,%d",maxway,wp[maxway].x,wp[maxway].y);
                        maxway++;
		}
	}
	
	// find connections
	for (n=1; n<maxway; n++) {
		for (m=n+1; m<maxway; m++) {
			dx=wp[n].x-wp[m].x;
			dy=wp[n].y-wp[m].y;
			
			if (dx>35 && dx<45 && abs(dy)<10 && pathfinder(wp[n].x,wp[n].y,wp[m].x,wp[m].y,1,NULL,400)!=-1) {
				if (wp[n].left) {
					xlog("duplicated left %d,%d removed",wp[wp[n].left].x,wp[wp[n].left].y);
				}
				//xlog("%d %d,%d left to %d %d,%d",n,wp[n].x,wp[n].y,m,wp[m].x,wp[m].y);
				wp[n].left=m;
				wp[m].right=n;
				continue;
			}
			
			if (dy>35 && dy<45 && abs(dx)<10 && pathfinder(wp[n].x,wp[n].y,wp[m].x,wp[m].y,1,NULL,400)!=-1) {	
				if (wp[n].up) {
					xlog("duplicated up %d,%d removed",wp[wp[n].up].x,wp[wp[n].up].y);
				}
				//xlog("%d %d,%d up to %d %d,%d",n,wp[n].x,wp[n].y,m,wp[m].x,wp[m].y);
				wp[n].up=m;
				wp[m].down=n;
				continue;
			}
		}
	}
}

int dist_to_waypoint(int x,int y,int n)
{
	return abs(x-wp[n].x)+abs(y-wp[n].y);
}

int find_closest_waypoint(int x,int y)
{
	int bestdist=99,bestwp=0,n,dist;

	for (n=1; n<maxway; n++) {
		if ((dist=dist_to_waypoint(x,y,n))<bestdist) {
			bestdist=dist;
			bestwp=n;
		}
	}

	return bestwp;
}

#define MAXNODE	50
struct node
{
	int nr,cost;
};

int findwaycmp(const void *a,const void *b)
{
	return ((struct node*)a)->cost-((struct node*)b)->cost;
}

#define FWW_NOENEMY	1

int fww_flags(int next,int flags)
{
        if ((flags&FWW_NOENEMY) && wp[next].last_enemy && ticker-wp[next].last_enemy<TICKS*60) return 0;
	
	return 1;
}

int find_way_to_waypoint(int from,int to,int flags)
{
	struct node node[MAXNODE];
	int cnode=0,maxnode=0,cost=0,next;
	char seen[maxway];

	if (!fww_flags(to,flags)) return 0;

	bzero(seen,sizeof(seen));

        node[maxnode].nr=to;
	node[maxnode].cost=1;
	maxnode++;

	while (maxnode<MAXNODE-4) {
		to=node[cnode].nr;
		cost=node[cnode].cost;
		cnode++;

		//xlog("considering node %d, cost %d",to,cost);

		if ((next=wp[to].right) && fww_flags(next,flags)) {
			//xlog("right=%d. ok",next);
			if (from==next) return to;
                        if (!seen[next]) {
				seen[next]=1;

				node[maxnode].nr=next;
				node[maxnode].cost=cost+1;				
				maxnode++;
			}
		} //else xlog("right=%d, not ok",next);
		if ((next=wp[to].left) && fww_flags(next,flags)) {
			//xlog("left=%d. ok",next);
			if (from==next) return to;
                        if (!seen[next]) {
				seen[next]=1;

				node[maxnode].nr=next;
				node[maxnode].cost=cost+1;				
				maxnode++;
			}
		} //else xlog("left=%d, not ok",next);
		if ((next=wp[to].up) && fww_flags(next,flags)) {
			//xlog("up=%d. ok",next);
			if (from==next) return to;
                        if (!seen[next]) {
				seen[next]=1;

				node[maxnode].nr=next;
				node[maxnode].cost=cost+1;				
				maxnode++;
			}
		} //else xlog("up=%d, not ok",next);
		if ((next=wp[to].down) && fww_flags(next,flags)) {
			//xlog("down=%d. ok",next);
			if (from==next) return to;
                        if (!seen[next]) {
				seen[next]=1;

				node[maxnode].nr=next;
				node[maxnode].cost=cost+1;				
				maxnode++;
			}
		} //else xlog("down=%d, not ok",next);
		if (maxnode>cnode) qsort(node+cnode,maxnode-cnode,sizeof(struct node),findwaycmp);
		else break;
	}

	return 0;
}

struct fdemon_data
{
        int dir;
	int gohome;
};

void add_enemy_to_waypoint(int cn)
{
	int n;

	n=find_closest_waypoint(ch[cn].x,ch[cn].y);
	
	if (n && abs(ch[cn].x-wp[n].x)<30 && abs(ch[cn].y-wp[n].y)<30) {
		wp[n].last_enemy=ticker;
		//log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,20,"enemy at wp %d",n);
	}
}

static int may_hunt_there(int cn,int x,int y)
{
	if (x-(signed)ch[cn].tmpx>30) { return 0; }
	if (y-(signed)ch[cn].tmpy>30) { return 0; }

	if ((signed)ch[cn].tmpx-x>70) { return 0; }
	if ((signed)ch[cn].tmpy-y>70) { return 0; }

	return 1;
}

static int hunt_driver(int cn,struct fdemon_data *dat)
{
	int n,bestdiff=TICKS*60,bestn=0,diff;
	int current,target;

        current=find_closest_waypoint(ch[cn].x,ch[cn].y);

	for (n=1; n<maxway; n++) {
		if (wp[n].last_enemy &&
		    may_hunt_there(cn,wp[n].x,wp[n].y) &&
                    (diff=ticker-wp[n].last_enemy)<bestdiff) {
			bestdiff=diff;
			bestn=n;			
		}
	}
	if (bestn) {
                //say(cn,"going to waypoint %d (age %.2fmin)",bestn,bestdiff/24.0/60);
		if (current==bestn) {
			//say(cn,"already close");
			if (abs(ch[cn].x-wp[bestn].x)+abs(ch[cn].y-wp[bestn].y)>6 &&
			    move_driver(cn,wp[bestn].x,wp[bestn].y,6)) {
				//say(cn,"move succeeded");
				return 1;
			}
			//say(cn,"there");
			return 0;	// we're there
		}
		target=find_way_to_waypoint(current,bestn,0);
		if (target) {
			//say(cn,"moving to enemy at %d from %d via %d",bestn,current,target);
			if (move_driver(cn,wp[target].x,wp[target].y,6)) {
				//say(cn,"move succeeded");
				return 1;
			}
		}
	}

	return 0;
}

void fdemon_demon(int cn,int ret,int lastact)
{
        struct fdemon_data *dat;
	struct msg *msg,*next;
        int co,x,y;

	if (ch[cn].sprite==190) { char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return; }

	if (maxway==1) {
		find_waypoints();
	}

	dat=set_data(cn,DRD_FDEMONDATA,sizeof(struct fdemon_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
                                ch[cn].arg=NULL;
			}
                        fight_driver_set_dist(cn,0,30,0);
			
			if (ch[cn].item[30] && (ch[cn].flags&CF_NOBODY)) {
				ch[cn].flags&=~(CF_NOBODY);
				ch[cn].flags|=CF_ITEMDEATH;
				//xlog("transformed item %s",it[ch[cn].item[30]].name);
			}			
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			if ((ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE)) && char_see_char(cn,co)) {
				add_enemy_to_waypoint(co);
			}
		}

                standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (!may_hunt_there(cn,ch[cn].x,ch[cn].y)) {
		dat->gohome=1;
		//say(cn,"gohome=1 (%d,%d - %d,%d)",ch[cn].x-ch[cn].tmpx,ch[cn].y-ch[cn].tmpy,ch[cn].tmpx-ch[cn].x,ch[cn].tmpy-ch[cn].y);
	}

	if (abs(ch[cn].x-ch[cn].tmpx)<15 && abs(ch[cn].y-ch[cn].tmpy)<15) {
		dat->gohome=0;
		//say(cn,"gohome=0 (%d,%d)",abs(ch[cn].x-ch[cn].tmpx),abs(ch[cn].y-ch[cn].tmpy));
	}

	if (dat->gohome) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
	}

	if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

        if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

	if (!dat->gohome && hunt_driver(cn,dat)) return;

	// dont walk all the time
	if (!RANDOM(4)) {
		do_idle(cn,TICKS*2);
		return;
	}

	if (!dat->dir) dat->dir=RANDOM(8)+1;

	dx2offset(dat->dir,&x,&y,NULL);
	
	if (do_walk(cn,dat->dir)) return;
	else dat->dir=0;

        do_idle(cn,TICKS);
}

void fdemon_demon_dead(int cn,int co)
{
	struct farmy_ppd *ppd;
	struct farmy_data *dat;
	int cc;

	if (ch[cn].sprite!=190) return;

        if ((ch[co].flags&CF_PLAYERLIKE) && (dat=set_data(co,DRD_FARMYDATA,sizeof(struct farmy_data)))) {	// its a soldier
		cc=dat->platoon[MAXSOLDIER];
	} else cc=co;

        if (!(ch[cc].flags&CF_PLAYER)) return;
	
	if (!(ppd=set_data(cc,DRD_FARMY_PPD,sizeof(struct farmy_ppd)))) return;
	
        if (ppd->boss_stage>=16 && ppd->boss_stage<=17) {
		log_char(cc,LOG_SYSTEM,0,"Well done. Now go back to the Commander.");
		ppd->boss_stage=18;
	}
}

void fdemon_blood(int in,int cn)
{
	struct farmy_ppd *ppd;
	int in2;

	if (!cn) return;
	
	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You do not want to touch the liquid with your bare hands.");
		return;
	}
	if (it[in2].driver==IDR_FLASK) {
		log_char(cn,LOG_SYSTEM,0,"The liquid burns through the flask and shatters it.");
		it[in].sprite=14348;
		set_sector(it[in].x,it[in].y);

		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"dropped flask (destroyed by golem blood)");
		destroy_item(in2);
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		return;
	}
	if (it[in2].ID!=IID_AREA8_BLOOD) {
		log_char(cn,LOG_SYSTEM,0,"Hu?");
		return;
	}

	if (it[in2].drdata[0]>2) {
		log_char(cn,LOG_SYSTEM,0,"The container is full already.");
		return;
	}

	if ((ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) &&
	    ppd->boss_stage>=19 && ppd->boss_stage<=20) {
		log_char(cn,LOG_SYSTEM,0,"That's it. Now report to the commander.");
		ppd->boss_stage=21;
	}


	it[in2].drdata[0]++;
	it[in2].sprite++;
	sprintf(it[in2].description,"A container holding %d parts golem blood.",it[in2].drdata[0]);
	ch[cn].flags|=CF_ITEMS;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped into blood container");
        remove_item(in);
	destroy_item(in);
}

void fdemon_lava(int in,int cn)
{
	struct farmy_ppd *ppd;
	int m,co,in2;

	m=it[in].x+it[in].y*MAXMAP;

	if (!cn) {
		
		if (it[in].drdata[0]) it[in].drdata[0]--;

                co=map[m].ch;

		if (it[in].drdata[0]==0) {		// last stage - set original sprite, set moveblock, remove flame
			
			if (co) hurt(co,1000*POWERSCALE,0,1,0,0);

			it[in].sprite=14363;
                        map[m].flags|=(MF_MOVEBLOCK|MF_FIRETHRU);
			map[m].fsprite&=0xffff;
			set_sector(it[in].x,it[in].y);

                        return;	// stop timer calls
		} else if (it[in].drdata[0]<20) {	// stage two - set flame and hurt characters
			if (!(map[m].fsprite&(1024<<16))) {
				map[m].fsprite|=(1024<<16);
				it[in].sprite=14364;
				set_sector(it[in].x,it[in].y);
			}
			if (co) hurt(co,10*POWERSCALE,0,1,0,50);
		} else if (it[in].drdata[0]<60) {	// stage one - set next sprite and hurt characters
			if (it[in].sprite!=14365) {
				it[in].sprite=14365;
				set_sector(it[in].x,it[in].y);
			}
			if (co) hurt(co,1*POWERSCALE,0,1,0,50);
		} else if (it[in].drdata[0]<115) {	// stage zero - remove mist
			if (map[m].fsprite&(1034<<16)) {
				map[m].fsprite&=0xffff;
				set_sector(it[in].x,it[in].y);
			}
		}
		
                call_item(it[in].driver,in,0,ticker+TICKS);
	} else {
		if (!(in2=ch[cn].citem)) {
			log_char(cn,LOG_SYSTEM,0,"You do not want to touch burning lava with your bare hands, do you?");
			return;
		}
		if (it[in2].ID!=IID_AREA8_BLOOD) {
			log_char(cn,LOG_SYSTEM,0,"Hu?");
			return;
		}
		if (it[in2].drdata[0]<1) {
			log_char(cn,LOG_SYSTEM,0,"The container is empty, and it cannot hold lava.");
			return;
		}

		if ((ppd=set_data(cn,DRD_FARMY_PPD,sizeof(struct farmy_ppd))) &&
		    ppd->boss_stage>=22 && ppd->boss_stage<=23) {
			log_char(cn,LOG_SYSTEM,0,"You got it. Now report to the commander.");
			ppd->boss_stage=24;
		}


		it[in2].drdata[0]--;
		it[in2].sprite--;
		sprintf(it[in2].description,"A container holding %d parts golem blood.",it[in2].drdata[0]);
		ch[cn].flags|=CF_ITEMS;

		it[in].drdata[0]=120;
		it[in].sprite=14366;
		map[m].flags&=~MF_MOVEBLOCK;
		map[m].fsprite|=(1034<<16);			// show mist
		set_sector(it[in].x,it[in].y);
		
		call_item(it[in].driver,in,0,ticker+TICKS);
	}
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                case CDR_FDEMON_ARMY:	fdemon_army(cn,ret,lastact); return 1;
		case CDR_FDEMON_BOSS:	fdemon_boss(cn,ret,lastact); return 1;
		case CDR_FDEMON_DEMON:	fdemon_demon(cn,ret,lastact); return 1;

		default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_FDEMONLIGHT:		fdemon_light(in,cn); return 1;
		case IDR_FDEMONLOADER:  	fdemon_loader(in,cn); return 1;
		case IDR_FDEMONCANNON:		fdemon_cannon(in,cn); return 1;
		case IDR_FDEMONGATE:   		fdemon_gate(in,cn); return 1;
		case IDR_FDEMONWAYPOINT:	fdemon_waypoint(in,cn); return 1;
		case IDR_FDEMONFARM:		fdemon_farm(in,cn); return 1;
		case IDR_FDEMONBLOOD:		fdemon_blood(in,cn); return 1;
		case IDR_FDEMONLAVA:		fdemon_lava(in,cn); return 1;

                default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_FDEMON_ARMY:	return 1;
		case CDR_FDEMON_BOSS:	return 1;
		case CDR_FDEMON_DEMON:	fdemon_demon_dead(cn,co); return 1;

                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_FDEMON_ARMY:	return 1;
		case CDR_FDEMON_BOSS:	return 1;
		case CDR_FDEMON_DEMON:	return 1;

		default:		return 0;
	}
}

