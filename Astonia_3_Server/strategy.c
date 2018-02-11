/*

$Id: strategy.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: strategy.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2004/04/28 10:02:35  sam
added safety check to not remove cinciac

Revision 1.2  2003/10/13 14:12:53  sam
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
#include "player_driver.h"
#include "task.h"
#include "sector.h"
#include "player.h"
#include "command.h"
#include "act.h"
#include "consistency.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);			// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies
int ch_respawn_driver(int nr,int cn);					// called when an NPC is about to respawn
int special_driver(int nr,int cn,char *ptr);

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case CDT_DRIVER:	return ch_driver(nr,obj,ret,lastact);
		case CDT_ITEM: 		return it_driver(nr,obj,ret);
		case CDT_DEAD:		return ch_died_driver(nr,obj,ret);
		case CDT_RESPAWN:	return ch_respawn_driver(nr,obj);
		case CDT_SPECIAL:	return special_driver(nr,obj,(void*)(ret));
		default: 		return 0;
	}
}

static int nosnow_item=0;

#define NPCPRICE	300
#define TRAINPRICE(cn)	((ch[cn].level-45)*10)
#define TRAINMULTI	3
#define MAXMISSIONTRY	3

#define OR_MINE		1
#define OR_FOLLOW	2
#define OR_GUARD	3
#define OR_FIGHTER	4
#define OR_TAKE		5
#define OR_TRANSFER	6
#define OR_TRAIN	7
#define OR_ETERNALGUARD	8

struct strategy_data
{
	int order;
	int or1,or2;

        int platin;
	int exp;
	int trainspeed;
	int max_level;

	char name[20];

	int lasthit;
	int restplace;
};

#define MAXMISSION	64

struct strategy_ppd
{
	int max_worker;		//OK
	int max_level;		//OK
	int trainspeed;		//OK
	int income;		//OK

	int endurance;		//OK
	int warcry;		//OK
        int speed;		//OK
	int eguards;
	int eguardlvl;

	int exp,boss_exp,boss_msg_exp;
	int mis_cnt;
	int won_cnt;
	int current_mission;
	int npc_color;

	int boss_stage,boss_timer;
	int init_done;
	unsigned char solve_cnt[MAXMISSION];
};

#define MAX_STR_AREA	16
#define MAX_STR_ITEM	256
#define MAX_STR_SPAWN	8

#define MAXQUEUE	4

struct str_area
{
	int used,busy;
	int spawn[MAX_STR_SPAWN],max_spawn;
	int item[MAX_STR_ITEM],max_item;

	int q_playerID[MAXQUEUE],q_playercn[MAXQUEUE];
};

struct str_area area[MAX_STR_AREA];
int area_init=0;

struct ai_preset
{
	char *name;
	struct strategy_ppd ppd;
};

struct ai_preset preset[64]={
	//name          worker level trspeed  income  endur   warc     speed	EGuards	EGuardlevel
	{"",		{0,	 0,	0,	 0,	 0,	 0,	 0,	0,	0}},
	{"Zakath",	{4,	60,	1,	 0,	 0,	 0,	 0,	1,	55}},	// 1  - area 1
	{"Mazian",	{4,	60,	1,	 0,	 0,	 0,	 0,	1,	55}},	// 2  - area 2 (mirror)

	{"Durnroth",	{4,	60,	1,	 0,	 0,	 5,	 0,	1,	55}},	// 3  - area 5

	{"Saphira",	{8,	60,	1,	 0,	 0,	15,	 5,	1,	65}},	// 4  - area 4
	{"Cleran",	{4,	60,	1,	 0,	 0,	 5,	 5,	1,	55}},	// 5  - area 4
	
	{"Dagdar",	{4,	65,	1,	 0,	 5,	15,	 5,	1,	65}},	// 6  - area 3
        {"Karkarath",	{4,	65,	1,	 0,	 5,	15,	 5,	1,	65}},	// 7  - area 3

	{"Vashini",	{8,	65,	2,	 0,	10,	25,	 5,	1,	65}},	// 8  - area 5
	
        {"Kurbatz",	{12,	70,	2,	 0,	25,	40,	15,	1,	70}},	// 9  - area 4
	{"Kalim",	{6,	70,	2,	 0,	10,	25,	 5,	1,	65}},	// 10  - area 4
	
	{"Sumpfbatz",	{6,	70,	2,	 0,	25,	25,	15,	1,	70}},	// 11 - area 3
        {"Umfrag",	{6,	70,	2,	 0,	25,	40,	15,	1,	70}},	// 12 - area 3

	{"Sickan",	{8,	75,	3,	 0,	30,	45,	20,	1,	75}},	// 13 - area 7
        {"Logasi",	{8,	75,	3,	 0,	30,	45,	20,	1,	75}},	// 14 - area 7

	{"Sumso",	{20,	80,	4,	 0,	35,	60,	30,	1,	90}},	// 15 - area 8

	{"Karka",	{4,	85,	4,	 0,	40,	65,	35,	1,	95}},	// 16 - area 9

	{"Rungan",	{12,	85,	4,	 0,	40,	65,	35,	1,	90}},	// 17 - area 10
	{"Kirlo",	{12,	85,	4,	 0,	40,	65,	35,	1,	90}},	// 18 - area 10
	{"Surgao",	{12,	85,	4,	 0,	40,	65,	35,	1,	90}},	// 19 - area 10

	{"Huwa",	{12,	90,	5,	 0,	45,	70,	45,	1,	95}},	// 20 - area 11
	{"Losaki",	{12,	90,	5,	 0,	45,	70,	45,	1,	95}},	// 21 - area 11

	{"Death",	{16,	115,	8,	20, 	115,	115,	115,	1,	115}},	// 22 - area 3
        {"Despair",	{16,	115,	8,	20,	115,	115,	115,	1,	115}}	// 23 - area 3
};

struct mission
{
	char *name;
        int area;		// area code (1...16) as used in items
	int mine_size;		// amount of platinum in mines
	int storage_size;       // amount of platinum in storages
	int enemy[4];		// index into preset for enemies for this area

	int set_solve;
	int need_solve,need_solve2;
	int exp;
};

struct mission mission[]={
	// name		area	mine	storage	enemies		set	need	need2	exp
	// area 23
	{"A-1",		1,	1000,	600,	{ 1, 0, 0, 0},	 1,	 0,	 0,	10},
	{"A-2",		2,	1000,	600,	{ 2, 0, 0, 0},	 1,	 0,	 0,	10},

	{"B",		5,	1000,	600,	{ 3, 0, 0, 0},	 2,	 1,	 1,	25},
        {"C",		4,	1000,	600,	{ 4, 5, 0, 0},	 3,	 1,	 1,	25},
	{"D",		3,	1500,	600,	{ 6, 7, 0, 0},	 4,	 2,	 3, 	25},
        {"E",		5,	2000,	900,	{ 8, 0, 0, 0},	 5,	 3,	 4, 	25},
        {"F",		4,	2000,	900,	{ 9,10, 0, 0},	 6,	 4,	 5, 	25},
	{"G",		3,	2000,	900,	{11,12, 0, 0},	 7,	 5,	 6, 	25},

	// area 24
	{"H",		7,	2000,	900,	{13,14, 0, 0},	 8,	 6,	 7, 	25},
	{"I",		8,	2000,	300,	{15, 0, 0, 0},	 9,	 7,	 8, 	25},

	{"J 2P",	9,	2000,	300,	{16, 0, 0, 0},	 0,	 0,	 0, 	 0},

	{"K",		10,	2000,	900,	{17,18,19, 0},	10,	 8,	 9, 	25},
	{"L",		11,	2000,	900,	{20,21, 0, 0},	11,	 9,	10, 	25},

	// area 23 (wech?)
        {"Z",		3,	2000,	900,	{22,23, 0, 0},	12,	10,	11, 	50}

};

void init_areas(void)
{
	int n,slot;

	for (n=1; n<MAXITEM; n++) {
		if (!it[n].flags) continue;
		
		slot=it[n].drdata[8];
		
		switch(it[n].driver) {
			case IDR_STR_SPAWNER:	it[n].drdata[10]=area[slot].max_spawn;		// set slot number for npc sprite calculation
						area[slot].spawn[area[slot].max_spawn++]=n;
			case IDR_STR_STORAGE:
			case IDR_STR_MINE:
			case IDR_STR_DEPOT:	area[slot].item[area[slot].max_item++]=n;
						area[slot].used=1;
                                                break;
		}
	}

	for (n=0; n<MAX_STR_AREA; n++) {
		if (area[n].used) {
			xlog("area %d: %d spawns, %d items",n,area[n].max_spawn,area[n].max_item);
		}
	}
}


int remove_party(int code,char *msg)
{
	int m,next,n,in;

	for (m=getfirst_char(); m; m=next) {
		next=getnext_char(m);
		if (ch[m].group==code) {
			if (ch[m].flags&CF_PLAYER) {
				elog("panic: about to destroy player %s!",ch[m].name);
			} else {
				if (strcmp(ch[m].name,"Cinciac")) remove_destroy_char(m);
			}
		}
		if (ch[m].ID==code) {
			if (!teleport_char_driver(m,15,15) &&
			    !teleport_char_driver(m,20,15) &&
			    !teleport_char_driver(m,15,20)) teleport_char_driver(m,20,20);
			if (msg) log_char(m,LOG_SYSTEM,0,msg);
		}
	}

	for (n=0; n<MAX_STR_AREA; n++) {
		for (m=0; m<area[n].max_spawn; m++) {
			if (*(unsigned int*)(it[area[n].spawn[m]].drdata+0)==code) break;			
		}
		if (m<area[n].max_spawn) break;		
	}
	if (n==MAX_STR_AREA) return 0;

	for (m=0; m<area[n].max_item; m++) {
		in=area[n].item[m];
		//xlog("item %s",it[in].name);

		if (*(unsigned int*)(it[in].drdata+0)==code) {
			if (it[in].driver==IDR_STR_SPAWNER) {
				if (*(unsigned int*)(it[in].drdata+0)<0xfffff000) *(unsigned int*)(it[in].drdata+0)=0;
				else *(unsigned int*)(it[in].drdata+0)=0xfffff000;
				sprintf(it[in].name,"Spawner (%d)",it[in].drdata[8]);
			}
			if (it[in].driver==IDR_STR_DEPOT) {
				*(unsigned int*)(it[in].drdata+0)=0;
				sprintf(it[in].name,"Depot (%d)",it[in].drdata[8]);
			}
			if (it[in].driver==IDR_STR_STORAGE) {
				*(unsigned int*)(it[in].drdata+0)=0;
				sprintf(it[in].name,"Storage (%d)",it[in].drdata[8]);
			}
		}
	}

        return 1;
}

int init_mission(int n)
{
	int m,ai=0,in;
	int ar;

	ar=mission[n].area;

	for (m=0; m<area[ar].max_item; m++) {
		in=area[ar].item[m];

                if (it[in].driver==IDR_STR_DEPOT) {
			*(unsigned int*)(it[in].drdata+0)=0;				// no owner
			*(unsigned int*)(it[in].drdata+4)=0;				// no gold
			sprintf(it[in].name,"Depot (%d)",it[in].drdata[8]);		// set name
		}
		if (it[in].driver==IDR_STR_STORAGE) {
			*(unsigned int*)(it[in].drdata+0)=0;				// no owner
			*(unsigned int*)(it[in].drdata+4)=mission[n].storage_size;	// set gold
			it[in].drdata[9]=0;						// no income
			sprintf(it[in].name,"Storage (%d)",it[in].drdata[8]);		// set name
		}
		if (it[in].driver==IDR_STR_MINE) {
			*(unsigned int*)(it[in].drdata+0)=0;				// no owner
			*(unsigned int*)(it[in].drdata+4)=mission[n].mine_size;		// set gold
                        sprintf(it[in].name,"Mine (%d)",it[in].drdata[8]);		// set name
		}
		if (it[in].driver==IDR_STR_SPAWNER) {
			// set AI ID for AI slots, or set to 0 for player slots
			if (*(unsigned int*)(it[in].drdata+0)>=0xfffff000) {			
				*(unsigned int*)(it[in].drdata+0)=0xfffff001+mission[n].enemy[ai++];
				it[in].drdata[9]=0;					// ai init not done
			} else {
				*(unsigned int*)(it[in].drdata+0)=0;
			}
			
			sprintf(it[in].name,"Spawner (%d)",it[in].drdata[8]);		// set name
		}
	}
	
	area[ar].busy=1;
	
	return 1;
}

int spawner2storage(int in)
{
	return map[it[in].x+it[in].y*MAXMAP-MAXMAP].it;
}

int did_party_lose(int spawn)
{
	int lost=1,code,storage,m,noplr=1;
	struct strategy_data *dat;

	code=*(unsigned int*)(it[spawn].drdata+0);
        storage=spawner2storage(spawn);

	if (code>=0xfffff000) noplr=0;

	if (*(unsigned int*)(it[storage].drdata+4)>=NPCPRICE) lost=0;
	
	for (m=getfirst_char(); m; m=getnext_char(m)) {
		if (!(dat=set_data(m,DRD_STRATEGYDRIVER,sizeof(struct strategy_data))) || dat->order==OR_ETERNALGUARD) continue;
		
		if (ch[m].group==code) lost=0;
		if (ch[m].ID==code) noplr=0;			
	}

	if (noplr) lost=1;

	return lost;
}

void close_area(int n)
{
	int in,m;

	for (m=0; m<area[n].max_spawn; m++) {				
		in=area[n].spawn[m];
		if (*(unsigned int*)(it[in].drdata+0) && *(unsigned int*)(it[in].drdata+0)!=0xfffff000)
			remove_party(*(unsigned int*)(it[in].drdata+0),NULL);
	}
}

void reward_winner(int code)
{
	struct strategy_ppd *ppd;
	int n,m;

	for (m=getfirst_char(); m; m=getnext_char(m)) {
                if (ch[m].ID==code) {
			if ((ppd=set_data(m,DRD_STRATEGY_PPD,sizeof(struct strategy_ppd)))) {			
				log_char(m,LOG_SYSTEM,0,"Congratulations, you won!");
				
				n=ppd->current_mission;
				if (n<0 || n>=ARRAYSIZE(mission)) {
					log_char(m,LOG_SYSTEM,0,"Please report bug #443f");
				} else {
					if (mission[n].exp) {
						ppd->won_cnt++;
						ppd->exp+=mission[n].exp;
						ppd->boss_exp+=mission[n].exp;
						ppd->eguards++;
						ppd->solve_cnt[mission[n].set_solve]++;
						log_char(m,LOG_SYSTEM,0,"You received %d strategy experience points.",mission[n].exp);
					}
				}
				ppd->current_mission=0;
			}
		}
	}
}


void str_ticker(int in,int cn)
{
	int n,m,ai,pl,winner=0;
	
	if (cn) return;
	
	call_item(it[in].driver,in,0,ticker+TICKS);

	if (!area_init) {
		init_areas();
		area_init=1;
	}

	for (n=0; n<MAX_STR_AREA; n++) {
		if (!area[n].used) continue;

                for (ai=pl=m=0; m<area[n].max_spawn; m++) {
			in=area[n].spawn[m];
			if (*(unsigned int*)(it[in].drdata+0) && *(unsigned int*)(it[in].drdata+0)!=0xfffff000) {
				if (did_party_lose(in)) remove_party(*(unsigned int*)(it[in].drdata+0),"You lose. Better luck next time!");
				
				if (*(unsigned int*)(it[in].drdata+0)<0xfffff000) { pl++; winner=*(unsigned int*)(it[in].drdata+0); }
				else ai++;
			}
		}
		if (pl==1 && ai==0) {
			xlog("area %d: player won",n);
			if (winner) reward_winner(winner);			
			close_area(n);
		} else if (ai && !pl) {
			 xlog("area %d: ai won",n);
			 close_area(n);
		}

		if (ai+pl) area[n].busy=1;

		if (!ai && !pl && area[n].busy) {
			xlog("area %d: closed",n);
			area[n].busy=0;
		}
	}
}

//-----------------------

int finditem(int cn,int drv)
{
	int dist,x,y,i,in,xc=ch[cn].x,yc=ch[cn].y;

	for (dist=1; dist<10; dist++) {
		for (i=-dist; i<=dist; i++) {
			x=xc+i; y=yc-dist;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && it[in].driver==drv) return in;
		}
		for (i=-dist; i<=dist; i++) {
			x=xc+i; y=yc+dist;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && it[in].driver==drv) return in;
		}
		for (i=-dist; i<=dist; i++) {
			x=xc-dist; y=yc+i;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && it[in].driver==drv) return in;
		}
		for (i=-dist; i<=dist; i++) {
			x=xc+dist; y=yc+i;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && it[in].driver==drv) return in;
		}
	}
	
	return 0;
}

int findstorage(int cn)
{
	int in;

	for (in=1; in<MAXITEM; in++) {
		if (!it[in].flags) continue;
		if (it[in].driver==IDR_STR_STORAGE && *(unsigned int*)(it[in].drdata+0)==ch[cn].group) return in;
	}

        return 0;
}

int finddepot(int xc,int yc,int group)
{
	int dist,x,y,i,in;

	for (dist=1; dist<20; dist++) {
		for (i=-dist; i<=dist; i++) {
			x=xc+i; y=yc-dist;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && (it[in].driver==IDR_STR_DEPOT || it[in].driver==IDR_STR_STORAGE)) return in;
		}
		for (i=-dist; i<=dist; i++) {
			x=xc+i; y=yc+dist;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && (it[in].driver==IDR_STR_DEPOT || it[in].driver==IDR_STR_STORAGE)) return in;
		}
		for (i=-dist; i<=dist; i++) {
			x=xc-dist; y=yc+i;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && (it[in].driver==IDR_STR_DEPOT || it[in].driver==IDR_STR_STORAGE)) return in;
		}
		for (i=-dist; i<=dist; i++) {
			x=xc+dist; y=yc+i;
			if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) continue;
			if ((in=map[x+y*MAXMAP].it) && (it[in].driver==IDR_STR_DEPOT || it[in].driver==IDR_STR_STORAGE)) return in;
		}
	}
	return 0;
}

void setname(int cn,struct strategy_data *dat)
{
	char buf[80];

	switch(dat->order) {
		case OR_MINE:		sprintf(buf,"%s's Miner %d",dat->name,cn);
                                        break;
		case OR_FOLLOW:		sprintf(buf,"%s's Minion %d",dat->name,cn);
                                        break;
		case OR_GUARD:		sprintf(buf,"%s's Guard %d",dat->name,cn);
                                        break;
		case OR_ETERNALGUARD:	sprintf(buf,"%s's E-Guard %d",dat->name,cn);
                                        break;
		case OR_FIGHTER:	
		case OR_TAKE:		sprintf(buf,"%s's Fighter %d",dat->name,cn);
                                        break;

		case OR_TRANSFER:	sprintf(buf,"%s's Transfer %d",dat->name,cn);
                                        break;

		case OR_TRAIN:		sprintf(buf,"%s's Trainee %d",dat->name,cn);
                                        break;

		default:		sprintf(buf,"%s's Worker %d",dat->name,cn);
                                        break;
	}
	if (strcmp(ch[cn].name,buf)) {
		strcpy(ch[cn].name,buf);
		reset_name(cn);
	}
	sprintf(ch[cn].description,"Carrying %d Platinum, %d of %d exp",dat->platin,dat->exp,TRAINPRICE(cn));
}

static int find_nosnow(void)
{
	int n;

	if (nosnow_item) return nosnow_item;
	for (n=1; n<MAXITEM; n++) {
		if (it[n].flags && it[n].driver==IDR_NOSNOW) {
			nosnow_item=n;
			return n;
		}
	}
	return 0;
}

int restplace(int cn,int m,struct strategy_data *dat)
{
	static int restlist[]={
		(-3-MAXMAP*5),
		(-4-MAXMAP*5),
		(-5-MAXMAP*5),
		(-6-MAXMAP*5),

		(-3+MAXMAP*5),
		(-4+MAXMAP*5),
		(-5+MAXMAP*5),
		(-6+MAXMAP*5),

		( 3-MAXMAP*5),
		( 4-MAXMAP*5),
		( 5-MAXMAP*5),
		( 6-MAXMAP*5),

		( 3+MAXMAP*5),
		( 4+MAXMAP*5),
		( 5+MAXMAP*5),
		( 6+MAXMAP*5),

		(-3-MAXMAP*3),
		(-4-MAXMAP*3),
		(-5-MAXMAP*3),
		(-6-MAXMAP*3),

		(-3+MAXMAP*3),
		(-4+MAXMAP*3),
		(-5+MAXMAP*3),
		(-6+MAXMAP*3),

		( 3-MAXMAP*3),
		( 4-MAXMAP*3),
		( 5-MAXMAP*3),
		( 6-MAXMAP*3),

		( 3+MAXMAP*3),
		( 4+MAXMAP*3),
		( 5+MAXMAP*3),
		( 6+MAXMAP*3)
		};
	int n;

        if (dat->restplace && (!(map[m+dat->restplace].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m+dat->restplace].ch==cn)) {
		return m+dat->restplace;
	}

	for (n=0; n<ARRAYSIZE(restlist); n++) {
		if (!(map[m+restlist[n]].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) {
			dat->restplace=restlist[n];
			return m+dat->restplace;
		}
	}
	return m;
}

void strategy_driver(int cn,int ret,int lastact)
{
	struct strategy_data *dat;
	struct strategy_ppd *ppd;
        int co,in,in2,me;
	struct msg *msg,*next;
	char *text;

        dat=set_data(cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			fight_driver_set_dist(cn,26,0,30);
			if (ch[cn].arg) {
				ch[cn].arg=NULL;
				strcpy(dat->name,"Neutral");
				dat->order=OR_GUARD;
				dat->or1=ch[cn].x;
				dat->or2=ch[cn].y;
				ch[cn].group=0xfffff000;				
			}
			ch[cn].level=ch[cn].value[1][V_WIS];
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			tabunga(cn,co,(char*)(msg->dat2));

			if ((ch[co].flags&CF_PLAYER) && ch[co].ID==ch[cn].group && char_see_char(cn,co) && dat->order!=OR_ETERNALGUARD) {
				ppd=set_data(co,DRD_STRATEGY_PPD,sizeof(struct strategy_ppd));
				
				text=(char*)(msg->dat2);
				
				while (isalpha(*text)) text++;
				while (isspace(*text)) text++;
				while (isalpha(*text)) text++;
				if (*text==':') text++;
				while (isspace(*text)) text++;
				if (*text=='"') text++;

                                me=atoi(text);

				if (!me && char_dist(cn,co)>30) {
					remove_message(cn,msg); continue;
				}

				if ((!me || me==cn) && strstr(text,"mine")) {
					if (!(in=finditem(co,IDR_STR_MINE))) {
						say(cn,"Sir, %s, sir, sorry sir, but I cannot find that mine.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
					if (!(in2=finddepot(ch[co].x,ch[co].y,ch[co].ID))) {
						say(cn,"Sir, %s, sir, sorry sir, but I cannot find a depot.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
					if (abs(it[in].x-it[in2].x)>18 || abs(it[in].y-it[in2].y)>18 || abs(it[in].x-it[in2].x)+abs(it[in].y-it[in2].y)>20) {
						say(cn,"Sir, %s, sir, sorry sir, but those are too far apart.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
					dat->order=OR_MINE;
					dat->or1=in;
					dat->or2=in2;
					say(cn,"%s, sir, yes, sir, mine, sir!",get_army_rank_string(co));
				}
				if ((!me || me==cn) && strstr(text,"follow")) {
					dat->order=OR_FOLLOW;
					dat->or1=co;
					dat->or2=0;
					say(cn,"%s, sir, yes, sir, follow, sir!",get_army_rank_string(co));
				}
				if ((!me || me==cn) && strstr(text,"guard")) {
					dat->order=OR_GUARD;
					dat->or1=ch[co].x;
					dat->or2=ch[co].y;
					say(cn,"%s, sir, yes, sir, guard, sir!",get_army_rank_string(co));
				}
				if ((!me || me==cn) && strstr(text,"fight")) {
					dat->order=OR_FIGHTER;
					dat->or1=co;
					dat->or2=0;
					say(cn,"%s, sir, yes, sir, fight, sir!",get_army_rank_string(co));
				}
				if ((!me || me==cn) && strstr(text,"home")) {
					dat->order=0;
					dat->or1=0;
					dat->or2=0;
					say(cn,"%s, sir, yes, sir, go home, sir!",get_army_rank_string(co));
				}
				if ((!me || me==cn) && strstr(text,"take") && (in=finditem(co,IDR_STR_DEPOT))) {
					dat->order=OR_TAKE;
					dat->or1=in;
					dat->or2=co;
					say(cn,"%s, sir, yes, sir, take, sir!",get_army_rank_string(co));
				}
				if ((!me || me==cn) && strstr(text,"transfer")) {
					int dx,dy;
					if (!(in=finditem(co,IDR_STR_DEPOT)) && !(in=finditem(co,IDR_STR_STORAGE))) {
						say(cn,"Sir, %s, sir, sorry sir, but I cannot find the first depot.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
					dx2offset(ch[co].dir,&dx,&dy,NULL);
					if (!(in2=finddepot(ch[co].x+dx*16,ch[co].y+dy*16,ch[co].ID))) {
						say(cn,"Sir, %s, sir, sorry sir, but I cannot find the second depot.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
					if (abs(it[in].x-it[in2].x)>18 || abs(it[in].y-it[in2].y)>18 || abs(it[in].x-it[in2].x)+abs(it[in].y-it[in2].y)>20) {
						say(cn,"Sir, %s, sir, sorry sir, but those are too far apart.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
                                        dat->order=OR_TRANSFER;
					dat->or1=in;
					dat->or2=in2;
					say(cn,"%s, sir, yes, sir, transfer, sir!",get_army_rank_string(co));
					//say(cn,"from %s to %s",it[in].name,it[in2].name);
				}
				if ((!me || me==cn) && strstr(text,"train")) {
                                        if (!(in=finditem(co,IDR_STR_STORAGE))) {
						say(cn,"Sir, %s, sir, sorry sir, but I cannot find a storage.",get_army_rank_string(co));
						remove_message(cn,msg); continue;
					}
                                        dat->order=OR_TRAIN;
					dat->or1=in;
					dat->or2=0;
					say(cn,"%s, sir, yes, sir, train, sir!",get_army_rank_string(co));
				}				
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
                        destroy_item(ch[cn].citem);
			ch[cn].citem=0;			
		}

		if (msg->type==NT_GOTHIT) dat->lasthit=ticker;

		if (dat->order==OR_GUARD || dat->order==OR_FIGHTER || dat->order==OR_TRAIN || dat->order==OR_ETERNALGUARD) standard_message_driver(cn,msg,1,1);
		else standard_message_driver(cn,msg,0,0);

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (lastact==AC_WALK && !RANDOM(10)) {
		int m;

		m=ch[cn].x+ch[cn].y*MAXMAP;
		if (map[m].gsprite>=21390 && map[m].gsprite<=21398) {
			map[m].gsprite-=10;
			map[m].gsprite|=(map[m].gsprite-21380+21400)<<16;
			set_sector(ch[cn].x,ch[cn].y);
                        call_item(IDR_NOSNOW,find_nosnow(),m,ticker+TICKS*120);
		} else if (map[m].gsprite&0xffff0000) {
			map[m].gsprite&=0x0000ffff;
			set_sector(ch[cn].x,ch[cn].y);
			call_item(IDR_NOSNOW,find_nosnow(),m,ticker+TICKS*120);
		}
	}

        setname(cn,dat);

	fight_driver_update(cn);
	if (fight_driver_attack_visible(cn,0)) return;
	if (ticker-dat->lasthit>TICKS*10 && regenerate_driver(cn)) return;

	switch(dat->order) {
		case OR_MINE:		if (!dat->platin) {
						if (!*(unsigned int*)(it[dat->or1].drdata+4)) {
							if (abs(ch[cn].x-it[dat->or1].x)!=3 || abs(ch[cn].y-it[dat->or1].y)!=3) {					
								if (move_driver(cn,it[dat->or1].x,it[dat->or1].y,3)) return;
							}
							do_idle(cn,TICKS);
							return;								
						}
						if (use_driver(cn,dat->or1,0)) return;
					} else {
						if (use_driver(cn,dat->or2,0)) return;
					}
					break;
		
		case OR_TRANSFER:	if (!dat->platin) {
						if (!*(unsigned int*)(it[dat->or1].drdata+4)) {
							if (abs(ch[cn].x-it[dat->or1].x)!=3 || abs(ch[cn].y-it[dat->or1].y)!=3) {					
								if (move_driver(cn,it[dat->or1].x,it[dat->or1].y,3)) return;
							}
							do_idle(cn,TICKS);
							return;	
						}
						if (use_driver(cn,dat->or1,0)) return;
					} else {
						if (use_driver(cn,dat->or2,0)) return;
					}
					break;
		
		case OR_TAKE:		if (*(unsigned int*)(it[dat->or1].drdata+0)==ch[cn].group) {
						if (dat->or2) {
							dat->order=OR_FIGHTER;
							dat->or1=dat->or2;
							dat->or2=0;
						} else {
							dat->order=OR_GUARD;
							dat->or1=ch[cn].x;
							dat->or2=ch[cn].y;
						}
						do_idle(cn,TICKS/4);
						return;
					}
                                        if (use_driver(cn,dat->or1,0)) return;
                                        break;

		case OR_FOLLOW:		
		case OR_FIGHTER:        if (!ch[dat->or1].flags || ch[dat->or1].ID!=ch[cn].group) {
						dat->order=dat->or1=dat->or2=0; break;
					}
					fight_driver_set_home(cn,ch[dat->or1].x,ch[dat->or1].y);
					if (abs(ch[cn].x-ch[dat->or1].x)>2 || abs(ch[cn].y-ch[dat->or1].y)>2) {
						if (move_driver(cn,ch[dat->or1].x,ch[dat->or1].y,2)) return;
					} else { do_idle(cn,TICKS); return; }
					break;
		
		case OR_GUARD:
		case OR_ETERNALGUARD:	fight_driver_set_home(cn,dat->or1,dat->or2);
					if (abs(ch[cn].x-dat->or1)+abs(ch[cn].y-dat->or2)>0) {
						if (move_driver(cn,dat->or1,dat->or2,0)) return;
                                                if (abs(ch[cn].x-dat->or1)+abs(ch[cn].y-dat->or2)>2) {
							if (move_driver(cn,dat->or1,dat->or2,2)) return;
							if (abs(ch[cn].x-dat->or1)+abs(ch[cn].y-dat->or2)>4) {
								if (move_driver(cn,dat->or1,dat->or2,4)) return;
							} else { do_idle(cn,TICKS); return; }
						} else { do_idle(cn,TICKS); return; }						
					} else { do_idle(cn,TICKS); return; }
					break;

		case OR_TRAIN:          if (ch[cn].level>=dat->max_level) {
						dat->order=OR_GUARD;
						dat->or1=ch[cn].x;
						dat->or2=ch[cn].y;
						break;
					}
					if (dat->platin>=dat->trainspeed) {
						int xx,yy;
						dat->platin-=dat->trainspeed;
						dat->exp+=dat->trainspeed*TRAINMULTI;
						if (dat->exp>=TRAINPRICE(cn)) {
							dat->exp-=TRAINPRICE(cn);

							ch[cn].level=min(ch[cn].level+1,115);
							ch[cn].value[1][V_WIS]=ch[cn].level;
							ch[cn].value[1][V_INT]=ch[cn].level;
							ch[cn].value[1][V_AGI]=ch[cn].level;
							ch[cn].value[1][V_STR]=ch[cn].level;
							ch[cn].value[1][V_HAND]=ch[cn].level;
							ch[cn].value[1][V_ATTACK]=ch[cn].level;
							ch[cn].value[1][V_PARRY]=ch[cn].level;
                                                        reset_name(cn);
							update_char(cn);
						}
						xx=restplace(cn,it[dat->or1].x+it[dat->or1].y*MAXMAP,dat)%MAXMAP;
						yy=restplace(cn,it[dat->or1].x+it[dat->or1].y*MAXMAP,dat)/MAXMAP;
							
						if (abs(ch[cn].x-xx)+abs(ch[cn].y-yy)>0) {
							if (move_driver(cn,xx,yy,0)) return;
							if (move_driver(cn,xx,yy,2)) return;
						} else fight_driver_set_home(cn,it[dat->or1].x,it[dat->or1].y);
						do_idle(cn,TICKS);
						return;
					}
					// get pay for training
					if (*(unsigned int*)(it[dat->or1].drdata+4)<dat->trainspeed) {
						int xx,yy;

						xx=restplace(cn,it[dat->or1].x+it[dat->or1].y*MAXMAP,dat)%MAXMAP;
						yy=restplace(cn,it[dat->or1].x+it[dat->or1].y*MAXMAP,dat)/MAXMAP;
						if (abs(ch[cn].x-xx)+abs(ch[cn].y-yy)>0) {
							if (move_driver(cn,xx,yy,0)) return;
							if (move_driver(cn,xx,yy,2)) return;
						}
						do_idle(cn,TICKS);
						return;	
					}
					if (use_driver(cn,dat->or1,0)) return;
					break;

		default:                if (!dat->or1) dat->or1=findstorage(cn);
					if (abs(ch[cn].x-it[dat->or1].x)>3 || abs(ch[cn].y-it[dat->or1].y)>3) {					
						if (move_driver(cn,it[dat->or1].x,it[dat->or1].y,3)) return;
					}
                                        do_idle(cn,TICKS);
					return;
	}
	
        do_idle(cn,TICKS);
}

void mine(int in,int cn)
{
	struct strategy_data *dat;
	int am;

        if (!cn) {
		sprintf(it[in].name,"Mine (%d)",it[in].drdata[8]);
		return;
	}
	if (ch[cn].flags&CF_PLAYER) {
		log_char(cn,LOG_SYSTEM,0,"There are %d units of Platinum left.",*(unsigned int*)(it[in].drdata+4));
		return;
	}
	dat=set_data(cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data));
	if (!dat) return;	// oops...

	am=min(ch[cn].value[0][V_STR],*(unsigned int*)(it[in].drdata+4));
	if (am==0) return;
	
	*(unsigned int*)(it[in].drdata+4)-=am;
	dat->platin+=am;

	//say(cn,"Got %d gold, %d left, carrying %d",am,*(unsigned int*)(it[in].drdata+4),dat->gold);
}

void storage(int in,int cn)
{
	struct strategy_data *dat;
	int am,in2;

        if (!cn) {
                if (*(unsigned int*)(it[in].drdata+0) && *(unsigned int*)(it[in].drdata+0)!=0xfffff000) {
			*(unsigned int*)(it[in].drdata+4)=min(*(unsigned int*)(it[in].drdata+4)+it[in].drdata[9],10000);
		}
		call_item(it[in].driver,in,0,ticker+TICKS*10);
		return;
	}
	if (ch[cn].flags&CF_PLAYER) {
		if ((in2=ch[cn].citem) && it[in2].driver==IDR_ENHANCE) {
			if (it[in2].drdata[0]==1) am=(*(unsigned int*)(it[in2].drdata+1))/50;
			else if (it[in2].drdata[0]==2) am=(*(unsigned int*)(it[in2].drdata+1))/5;
			else am=0;
			
			if (am) {
				*(unsigned int*)(it[in].drdata+4)+=am;

				log_char(cn,LOG_SYSTEM,0,"Converted to %d units of Platinum and added to storage.",am);
				
				dlog(cn,in2,"dropped into ice army depot");
				destroy_item(in2);
				ch[cn].citem=0;
				ch[cn].flags|=CF_ITEMS;
			} else {
				log_char(cn,LOG_SYSTEM,0,"You can only add mined gold or silver. The exchange rate is 5 to 1 for gold and 50 to 1 for silver.");
			}
		}
		log_char(cn,LOG_SYSTEM,0,"This storage contains %d units of Platinum.",*(unsigned int*)(it[in].drdata+4));
		return;
	}
	dat=set_data(cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data));
	if (!dat) return;	// oops...

	if (dat->platin) {
		*(unsigned int*)(it[in].drdata+4)+=(am=dat->platin);
		dat->platin=0;		
	} else {
		am=min(ch[cn].value[0][V_STR],*(unsigned int*)(it[in].drdata+4));
		*(unsigned int*)(it[in].drdata+4)-=am;
		dat->platin+=am;
	}
}

void depot(int in,int cn)
{
	struct strategy_data *dat;
	int am;

        if (!cn) {
		sprintf(it[in].name,"Depot (%d)",it[in].drdata[8]);
		return;
	}

	if (ch[cn].flags&CF_PLAYER) {
		log_char(cn,LOG_SYSTEM,0,"This depot contains %d units of Platinum.",*(unsigned int*)(it[in].drdata+4));
		return;
	}
	dat=set_data(cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data));
	if (!dat) return;	// oops...

	if (*(unsigned int*)(it[in].drdata+0)!=ch[cn].group) {
		say(cn,"Taking over depot.");
		*(unsigned int*)(it[in].drdata+0)=ch[cn].group;
		sprintf(it[in].name,"%.20s's Depot (%d)",dat->name,it[in].drdata[8]);
		return;
	}

	if (dat->platin) {
		*(unsigned int*)(it[in].drdata+4)+=(am=dat->platin);
		dat->platin=0;
	} else {
		am=min(ch[cn].value[0][V_STR],*(unsigned int*)(it[in].drdata+4));
		*(unsigned int*)(it[in].drdata+4)-=am;
		dat->platin+=am;
	}
}


void ai_main(int in,unsigned int code);
void ai_init(int in,unsigned int code);

int spawner_sub(int in,int in2,int group,char *name,struct strategy_ppd *ppd)
{
	int co,cnt;
	struct strategy_data *dat;

	for (cnt=0,co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].flags && ch[co].driver==CDR_STRATEGY && ch[co].group==group &&
		    (dat=set_data(co,DRD_STRATEGYDRIVER,sizeof(struct strategy_data))) &&
		    dat->order!=OR_ETERNALGUARD)
			cnt++;
	}
	//xlog("cnt=%d, max=%d",cnt,ppd->max_worker);
	if (cnt>=ppd->max_worker) return 0;

	*(unsigned int*)(it[in2].drdata+4)-=NPCPRICE;

	co=create_char("strategy_npc",0);
	if (item_drop_char(in,co)) {
		ch[co].tmpx=ch[co].x;
		ch[co].tmpy=ch[co].y;

		ch[co].value[1][V_WARCRY]+=ppd->warcry;
                ch[co].value[1][V_ENDURANCE]+=ppd->endurance;
		ch[co].value[1][V_SPEED]+=ppd->speed;

		update_char(co);
	
                ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
		ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
		ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;

		ch[co].dir=DX_RIGHTDOWN;
		ch[co].sprite=353+ppd->npc_color;

		ch[co].group=group;
		dat=set_data(co,DRD_STRATEGYDRIVER,sizeof(struct strategy_data));
		if (dat) {
			strncpy(dat->name,name,19); dat->name[19]=0;
			dat->trainspeed=ppd->trainspeed;
			dat->max_level=ppd->max_level;
		}
	} else {
		destroy_char(co);
		return 0;
	}
	return co;
}


void take_spawner(int in,int cn,struct strategy_ppd *ppd)
{
	int in2;

	if (!(ch[cn].flags&CF_PLAYER)) return;
	
        if (!*(unsigned int*)(it[in].drdata+0)) {
                in2=spawner2storage(in);
                if (!in2) {
			log_char(cn,LOG_SYSTEM,0,"Failed. Please report bug #25476g");
			return;
		}
		*(unsigned int*)(it[in].drdata+0)=ch[cn].ID;
		*(unsigned int*)(it[in2].drdata+0)=ch[cn].ID;
		sprintf(it[in].name,"%.20s's Spawner (%d)",ch[cn].name,it[in].drdata[8]);
		sprintf(it[in2].name,"%.20s's Storage (%d)",ch[cn].name,it[in2].drdata[8]);
		log_char(cn,LOG_SYSTEM,0,"You take control of this spawner. Use it again to create workers.");

		it[in2].drdata[9]=ppd->income;
		ppd->npc_color=it[in].drdata[10];
		return;
	}
}

void spawner(int in,int cn)
{
	int in2;
	struct strategy_ppd *ppd;

	if (!cn) {
		unsigned int code;
		code=*(unsigned int*)(it[in].drdata+0);

		if (code==0xfffff000) {
			call_item(it[in].driver,in,0,ticker+TICKS+RANDOM(TICKS));
			return;
		}

		if (code>0xfffff000) {
			call_item(it[in].driver,in,0,ticker+TICKS+RANDOM(TICKS));
			if (it[in].drdata[9] && code>0xfffff000) {
				ai_main(in,code);
				return;
			}
			in2=spawner2storage(in);
			if (!in2) {
				elog("Failed. Please report bug #25476g");
				return;
			}
                        *(unsigned int*)(it[in2].drdata+0)=code;
			sprintf(it[in].name,"%.20s's Spawner (%d)",preset[code-0xfffff001].name,it[in].drdata[8]);
			sprintf(it[in2].name,"%.20s's Storage (%d)",preset[code-0xfffff001].name,it[in2].drdata[8]);
			it[in].drdata[9]=1;	// init done

			it[in2].drdata[9]=preset[code-0xfffff001].ppd.income;
			preset[code-0xfffff001].ppd.npc_color=it[in].drdata[10];

			if (code>0xfffff000) ai_init(in,code);
		}
		return;
	}
	if (!(ch[cn].flags&CF_PLAYER)) return;
	
	ppd=set_data(cn,DRD_STRATEGY_PPD,sizeof(struct strategy_ppd));
	if (!ppd) return;

	if (*(unsigned int*)(it[in].drdata+0)!=ch[cn].ID) {
		log_char(cn,LOG_SYSTEM,0,"This spawner belongs to somebody else.");
		return;
	}

	in2=spawner2storage(in);
	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"Failed. Please report bug #25476e");
		return;
	}

        if (*(unsigned int*)(it[in2].drdata+4)<NPCPRICE) {
		log_char(cn,LOG_SYSTEM,0,"Not enough Platinum to create a worker.");
		return;
	}

	if (!spawner_sub(in,in2,ch[cn].ID,ch[cn].name,ppd))
		log_char(cn,LOG_SYSTEM,0,"No space to drop char or max worker reached.");
}

void nosnow(int in,int m)
{
        if (m<1 || m>=(MAXMAP-1)*(MAXMAP-1)) return;
	
	// original snow
	if (map[m].gsprite>=21390 && map[m].gsprite<=21398) return;

	// mid-step to original
	if (map[m].gsprite&0xffff0000) {
		map[m].gsprite&=0x0000ffff;
		map[m].gsprite+=10;
		set_sector(m%MAXMAP,m/MAXMAP);
		return;
	}
	
	if (map[m].gsprite>=21380 && map[m].gsprite<=21398) {	
                map[m].gsprite|=(map[m].gsprite-21380+21400)<<16;
		set_sector(m%MAXMAP,m/MAXMAP);
		//call_item(it[in].driver,in,m,ticker+TICKS*120);
	}
}

void strategy_boss(int cn,int ret,int lastact)
{
	struct strategy_ppd *ppd;
        struct msg *msg,*next;
	int co;

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
			    (ppd=set_data(co,DRD_STRATEGY_PPD,sizeof(struct strategy_ppd))) && realtime-ppd->boss_timer>5) {
				if (!ppd->init_done) {
					bzero(ppd,sizeof(struct strategy_ppd));
					ppd->max_level=60;
					ppd->max_worker=4;
					ppd->trainspeed=1;
					ppd->eguardlvl=50;
					ppd->init_done=1;					
				}
				//say(cn,"stage=%d",ppd->boss_stage);
				switch(ppd->boss_stage) {
					case 0:		if (get_army_rank_int(co)<8) {
								say(cn,"Ah, %s. The governer of Aston has some missions for you. You'd better head back there and do those first.",ch[co].name);
								ppd->boss_stage++;
							} else {
								say(cn,"Welcome, %s, to the Ice Army's Caves. I am %s, the commander in chief of the Ice Army Caves.",ch[co].name,ch[cn].name);
								ppd->boss_stage=2;
							}
							ppd->boss_timer=realtime;
							break;
					case 1:		if (get_army_rank_int(co)>=8) ppd->boss_stage++;
							else break;

					case 2:		say(cn,"We've discovered these caves a few weeks ago. Each cave seems to contain a network of depots, several castles and platinum mines. Some ancient magic is at work here, since each castle is able to create artificial creatures, which can be used as workers or fighters.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 3:		say(cn,"Islena's Lieutenants are trying to gain control over the castles and the platinum mines. If they succeed, they could spawn a vast army of these artificial creatures, and overrun our newly established defenses around Aston.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 4:		say(cn,"Unfortunately, we cannot spare any soldiers to fight the Lieutenants and their artificial armies. Therefore, we have to beat them using their own means.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 5:		say(cn,"Your mission, %s, is to find out how to use the castles, the mines and the workers to raise an army of your own, and to defeat Islena's Lieutenants.",get_army_rank_string(co));
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
                                        case 6:		say(cn,"We have collected some information about the caves, and the Lieutenants you will encounter there. Type /mission to get a list of these caves, and the missions currently available.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
                                        case 7:		say(cn,"You can use /enter <number> to start any of the missions listed. With /info, you'll be able to get some information about your understanding of the Castle's magic, and with /raise <number> you can choose to research one of the topics listed with /info.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
                                        case 8:		say(cn,"You can also /surrender, /list, /jp and /eguard once you've started a mission. The artificial creatures obey specific spoken commands. So far, we discovered 'transfer', 'mine', 'guard' and 'fight'. You will have to do some research of your own to utilize all the commands fully.");
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
                                                        break;
                                        case 9:		say(cn,"Good luck, %s. And report back from time to time.",ch[co].name);
							ppd->boss_stage++;
							ppd->boss_timer=realtime;
							break;
					case 10:	if (ppd->boss_exp>0) {
								say(cn,"Ah, %s. You made some progress defeating Islena's Lieutenants, and I have orders to reward you. Do you prefer °c4military rank°c0 or °c4levels and experience°c0?",ch[co].name);
								ppd->boss_stage++;								
								ppd->boss_msg_exp=ppd->boss_exp;
							}
							ppd->boss_timer=realtime;
							break;
					case 11:	if (ppd->boss_exp>ppd->boss_msg_exp) ppd->boss_stage=10;
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
                        if ((ch[co].flags&CF_PLAYER) && char_dist(cn,co)<16 && char_see_char(cn,co) &&
			    (ppd=set_data(co,DRD_STRATEGY_PPD,sizeof(struct strategy_ppd)))) {
				if (strcasestr((char*)msg->dat2,"repeat")) {
					switch(ppd->boss_stage) {
						case 0:
						case 1:
						case 2:
						case 3:
						case 4:		
						case 5:
						case 6:
						case 7:
						case 8:
						case 9:
						case 10:	
						case 11:	ppd->boss_stage=0; ppd->boss_timer=0; break;
					}					
				}
				if (strcasestr((char*)msg->dat2,"military rank")) {
					if (ppd->boss_exp>0) {
						say(cn,"So be it.");
						give_military_pts(cn,co,ppd->boss_exp,ppd->boss_exp);
						ppd->boss_exp=ppd->boss_msg_exp=0;
						ppd->boss_stage=10;
					}
				}
				if (strcasestr((char*)msg->dat2,"levels and experience")) {
					if (ppd->boss_exp>0) {
						say(cn,"So be it.");
						give_military_pts(cn,co,ppd->boss_exp/5+1,ppd->boss_exp*pow(get_army_rank_int(co)+5,4)/24);
						ppd->boss_exp=ppd->boss_msg_exp=0;
						ppd->boss_stage=10;
					}
				}
                        }			
		}

                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

	//say(cn,"i am %d",cn);
	turn(cn,DX_RIGHT);
        do_idle(cn,TICKS);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_STRATEGY:	strategy_driver(cn,ret,lastact); return 1;
		case CDR_STRATEGY_BOSS:	strategy_boss(cn,ret,lastact); return 1;


                default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_STR_MINE:	mine(in,cn); return 1;
		case IDR_STR_STORAGE:	storage(in,cn); return 1;
		case IDR_STR_SPAWNER:	spawner(in,cn); return 1;
		case IDR_STR_DEPOT:	depot(in,cn); return 1;
		case IDR_STR_TICKER:	str_ticker(in,cn); return 1;
		case IDR_NOSNOW:	nosnow(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_STRATEGY:	return 1;
		case CDR_STRATEGY_BOSS:	return 1;

                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_STRATEGY:	return 1;
		case CDR_STRATEGY_BOSS:	return 1;

		default:		return 0;
	}
}

#define MAX_AI		32

#define T_IDLE		0
#define T_MINE		1
#define T_TRANSFER	2

#define T_FIGHT		4
#define T_EGUARD	5
#define T_IGNORE	6
#define T_TAKE		7

#define PT_STORAGE	1
#define PT_DEPOT	2
#define PT_MINE		3

#define MAXWORKER	4
#define MAXGUARD	12

#define MAXNPC		256
#define MAXPLACE	256
#define MAXTHREAT	256
#define MAXDISTANCE	64
#define MAXPARTNER	4

#define WT_DIRECT	1
#define WT_UP		2
#define WT_DOWN		3

struct ai_npc
{
	int cn,cserial;
	int x,y;
	int platin;
	int level;

	int order,or1,or2;

	int task;
	int target,current;
	int walktype;

	int used;
	int ftarget;
};


struct ai_place
{
	int type;
	int in;

	int platin;
	int x,y;
	int dist;
	int parent;

	int wcnt;
	int worker[MAXWORKER];

	int eguard;

	int threat;
	int threatlevel;
	int threatnlevel;
	double threatcount;
	double threatncount;

	int owned;
	int enemy_possible;
};

struct ai_threat
{
	int place;

	int level;
	double count;
	int ticker;
};

struct ai_data
{
	int ai_init;

	int storage_in;
	int storage_x;
	int storage_y;

	int npc_cnt;
	int free_workers;
	int worklevel;
	int lastchange;

	int gcnt;
	int guard[MAXGUARD];

	// for panic defense
	int panic,pplace,pdist;

	// for nag attacks
	int lastnag,nagplace,nagguard;

	int max_an;
	struct ai_npc an[MAXNPC];
	
	int max_ap;
	struct ai_place ap[MAXPLACE];

	int max_at;
	struct ai_threat at[MAXTHREAT];

	int max_partner;
	int partner[MAXPARTNER];

	struct strategy_ppd ppd;

	int nogoldleft,ragnarok;
	int etguardcnt;
};

struct ai_data ai_data[MAX_AI],*ad=ai_data+0;

static int ai_check_target(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;

	return 1;
}

void update_npc_place(int n)
{
	int t,m;

	t=ad->an[n].current;

	// current place correct?
	if (abs(ad->an[n].x-ad->ap[t].x)<10 && abs(ad->an[n].y-ad->ap[t].y)<10) return;

	for (m=0; m<ad->max_ap; m++) {
		if (abs(ad->an[n].x-ad->ap[m].x)<10 && abs(ad->an[n].y-ad->ap[m].y)<10) {
			ad->an[n].current=m;
			return;
		}
	}
	xlog("could not determine place for NPC %d at %d,%d",n,ad->an[n].x,ad->an[n].y);
}

void subtask_move(int n)
{
	int t,m,last;

        t=ad->an[n].target;

	if (abs(ad->an[n].x-ad->ap[t].x)>5 || abs(ad->an[n].y-ad->ap[t].y)>5) {	// to far away from target

		// can we go there without using waypoints?
		if (abs(ad->an[n].x-ad->ap[t].x)<20 && abs(ad->an[n].y-ad->ap[t].y)<20 &&
		    pathfinder(ad->an[n].x,ad->an[n].y,ad->ap[t].x,ad->ap[t].y,1,ai_check_target,500)!=-1) {
			ad->an[n].order=OR_GUARD;
			ad->an[n].or1=ad->ap[t].x;
			ad->an[n].or2=ad->ap[t].y;
			ad->an[n].walktype=WT_DIRECT;
			//xlog("direct");
			return;
		}

		// we need waypoints

		// follow path from target to storage
		for (last=t,m=ad->ap[t].parent; m!=-1; m=ad->ap[m].parent) {
			if (m==ad->an[n].current) {	// NPC is at this place, make him go up one step
				ad->an[n].order=OR_GUARD;
				ad->an[n].or1=ad->ap[last].x;
				ad->an[n].or2=ad->ap[last].y;
				ad->an[n].walktype=WT_DOWN;
				//xlog("to %d,%d (1) target=%d, last=%d, NPC is at %d",ad->ap[last].x,ad->ap[last].y,t,last,ad->an[n].current);
				return;
			}
			last=m;
		}

		// NPC us not at any place on path from target to storage, make him go to storage
		t=ad->an[n].current; t=ad->ap[t].parent;
		if (t==-1) {
			xlog("NPC is lost: %d at %d,%d",n,ad->an[n].x,ad->an[n].y);
		}
		ad->an[n].order=OR_GUARD;
		ad->an[n].or1=ad->ap[t].x;
		ad->an[n].or2=ad->ap[t].y;
		ad->an[n].walktype=WT_UP;
		//xlog("to %d,%d (2) target=%d, NPC ist at %d",ad->ap[t].x,ad->ap[t].y,t,ad->an[n].current);
		return;
	}
}

void task_idle(int n)
{
	int t;
	struct strategy_data *dat;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current) { subtask_move(n); return; }

	if (!(dat=set_data(ad->an[n].cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data)))) return;

        // we are where we're supposed to be... guard:
	ad->an[n].order=OR_GUARD;
	ad->an[n].or1=restplace(ad->an[n].cn,ad->ap[t].x+ad->ap[t].y*MAXMAP,dat)%MAXMAP;
        ad->an[n].or2=restplace(ad->an[n].cn,ad->ap[t].x+ad->ap[t].y*MAXMAP,dat)/MAXMAP;

	//xlog("home");
}

void task_take(int n)
{
	int t;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current) { subtask_move(n); return; }

        // we are where we're supposed to be... guard:
	ad->an[n].order=OR_TAKE;
	ad->an[n].or1=ad->ap[t].in;
	ad->an[n].or2=0;

	//xlog("home");
}

void task_guard(int n)
{
	int t;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current) { subtask_move(n); return; }

        // we are where we're supposed to be... guard:
	ad->an[n].order=OR_GUARD;
	ad->an[n].or1=ad->ap[t].x;
	ad->an[n].or2=ad->ap[t].y;

	//xlog("home");
}

void task_mine(int n)
{
	int t;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current && ad->ap[t].parent!=ad->an[n].current) { subtask_move(n); return; }

        // we are where we're supposed to be... mine:
	ad->an[n].order=OR_MINE;
	ad->an[n].or1=ad->ap[t].in;
	ad->an[n].or2=ad->ap[ad->ap[t].parent].in;

	//xlog("mine");
}

void task_transfer(int n)
{
	int t;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current && ad->ap[t].parent!=ad->an[n].current) { subtask_move(n); return; }

        // we are where we're supposed to be... mine:
	ad->an[n].order=OR_TRANSFER;
	ad->an[n].or1=ad->ap[t].in;
	ad->an[n].or2=ad->ap[ad->ap[t].parent].in;

	//xlog("transfer");
}

void task_train(int n)
{
	int t;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current && ad->ap[t].parent!=ad->an[n].current) { subtask_move(n); return; }

        // we are where we're supposed to be... mine:
	ad->an[n].order=OR_TRAIN;
	ad->an[n].or1=ad->ap[t].in;
	ad->an[n].or2=0;

	//xlog("train");
}

void task_fight(int n)
{
	int t;

	t=ad->an[n].target;
	update_npc_place(n);
	if (t!=ad->an[n].current && ad->ap[t].parent!=ad->an[n].current) { subtask_move(n); return; }

        // we are where we're supposed to be... mine:
	ad->an[n].order=OR_GUARD;
	ad->an[n].or1=ad->ap[t].x;
	ad->an[n].or2=ad->ap[t].y;

	//xlog("train");
}

int assign_npc(int n)
{
	int m,i;

	for (m=0; m<ad->max_an; m++) {
		if (ad->an[m].used!=-1) continue;
		
		if (ad->ap[n].type==PT_MINE) ad->an[m].task=T_MINE;
		else ad->an[m].task=T_TRANSFER;

		ad->an[m].target=n;

		for (i=0; i<MAXWORKER; i++) {
			if (ad->ap[n].worker[i]==-1) {
				ad->ap[n].worker[i]=m;
				break;
			}
		}
		ad->ap[n].wcnt++;

		ad->an[m].used=n;
		ad->free_workers--;

		//xlog("assigned NPC %d to place %d",m,n);
		return 1;
	}
	return 0;
}

int add_worker(int task,int worker,int place)
{
	int i;

	ad->an[worker].task=task;
	ad->an[worker].target=place;

	for (i=0; i<MAXWORKER; i++) {
		if (ad->ap[place].worker[i]==-1) {
			ad->ap[place].worker[i]=worker;
			break;
		}
	}
	ad->ap[place].wcnt++;
	ad->an[worker].used=place;
	ad->free_workers--;
	return 1;
}

void add_etguard(int guard)
{
	int t;

	update_npc_place(guard);

	t=ad->an[guard].target=ad->an[guard].current;

	ad->ap[t].eguard=guard;
}

int add_guard(int guard)
{
	int i;

        for (i=0; i<MAXGUARD; i++) {
		if (ad->guard[i]==-1) {
			ad->guard[i]=guard;
			break;
		}
	}
	ad->gcnt++;
	ad->an[guard].used=0;
	ad->free_workers--;
	return 1;
}

int remove_guard(int guard)
{
	int i;

        for (i=0; i<MAXGUARD; i++) {
		if (ad->guard[i]==guard) {
			ad->guard[i]=-1;
			break;
		}
	}
	ad->gcnt--;
	ad->an[guard].used=-1;
	ad->free_workers++;
	ad->an[guard].task=T_IDLE;
	ad->an[guard].target=0;
	return 1;
}


int remove_worker(int worker)
{
	int i,place=ad->an[worker].target;

	ad->an[worker].task=T_IDLE;
	ad->an[worker].target=0;

	for (i=0; i<MAXWORKER; i++) {
		if (ad->ap[place].worker[i]==worker) {
			ad->ap[place].worker[i]=-1;
			break;
		}
	}
	ad->ap[place].wcnt--;
	ad->an[worker].used=0;
	ad->free_workers++;
	return 1;
}

#define WORKERPLATIN	200

#define THREAT(cn)	((double)ch[cn].level*ch[cn].level*ch[cn].level)

int assign_guards(int place,double count,int level,int ragnarok)
{
	int n,m,attarget=1;
	double have;
	char use[MAXGUARD];
	
	bzero(use,sizeof(use));

	have=0.0;
	// check for already assigned guards
	for (n=0; n<MAXGUARD; n++) {
		if ((m=ad->guard[n])==-1) continue;
                if (ad->nagguard==m) continue;

                //xlog("guard %d, target %d (%d %d), used %d",ad->an[m].cn,ad->an[m].target,ad->ap[place].parent,place,ad->an[m].used);
		if (ad->an[m].ftarget==place) {
			// we already have too many, or the level req went up
			if ((ad->an[m].level+5<level || have>count) && !ragnarok) {
				ad->an[m].target=ad->an[m].ftarget=0;
				ad->an[m].used=0;
			} else {
				have+=THREAT(ad->an[m].cn);
				use[n]=2;
				if (ad->an[m].current!=ad->ap[place].parent && ad->an[m].current!=place) attarget=0;
				//xlog("found guard %d going to target already",ad->an[m].cn);
			}
		}
	}
        // check for free guards
	for (n=0; (ragnarok || have<count) && n<MAXGUARD; n++) {
		if ((m=ad->guard[n])==-1) continue;

                if (ad->an[m].used==0 &&
                    ((ad->nagguard!=m &&
                      ad->an[m].level+5>=level &&
		      ch[ad->an[m].cn].hp>=ch[ad->an[m].cn].value[0][V_HP]*POWERSCALE)
		     || ragnarok)) {
			have+=THREAT(ad->an[m].cn);
			use[n]=1;
			if (ad->an[m].current!=ad->ap[place].parent && ad->an[m].current!=place) attarget=0;
		}
	}

	//xlog("place %d, count %.0f, have %.0f",place,count,have);

	// we have enough to attack
	if (have>count || ragnarok) {
		have=0.0;
                for (n=0; (ragnarok || have<count) && n<MAXGUARD; n++) {
			if ((m=ad->guard[n])!=-1 && use[n]) {
				ad->an[m].ftarget=place;;
				if (attarget) {
					ad->an[m].target=place;
					ad->an[m].used=place;
				} else {
					ad->an[m].target=ad->ap[place].parent;
					ad->an[m].used=ad->ap[place].parent;
				}
				have+=THREAT(ad->an[m].cn);
				//xlog("sending guard %d (%d) to %d",ad->an[m].cn,ad->an[m].current,ad->an[m].target);
			}
		}
		return 1;
	} else {	// not enough, recall the guards
		for (n=0; have<count && n<MAXGUARD; n++) {
			if ((m=ad->guard[n])!=-1 && use[n]==2) {
				ad->an[m].target=ad->an[m].ftarget=0;
				ad->an[m].used=0;
				//xlog("recalling guard %d",ad->an[m].cn);
			}
		}
	}
	return 0;
}

void remove_free_guards(void)
{
	int n,m;
	
	for (n=0; n<MAXGUARD; n++) {
		if ((m=ad->guard[n])!=-1 && m!=ad->nagguard && ad->an[m].used==0) {
			ad->an[m].target=0;
		}
	}
}

int tcomp(const void *a,const void *b)
{
	int pa,pb;

        pa=((struct ai_threat *)a)->place;
	pb=((struct ai_threat *)b)->place;

	if (!pa && !pb) return 0;
	if (!pa) return -1;
	if (!pb) return -1;

	if (ad->ap[pa].dist>ad->ap[pb].dist) return -1;
	if (ad->ap[pa].dist<ad->ap[pb].dist) return -1;

	return ((struct ai_threat *)b)->count-((struct ai_threat *)a)->count;
}

void nag_attack(void)
{
	int n,m,minlevel=115,cnt=0,guard=0,mindist=99,place=0;
	
        if (ticker-ad->lastnag<TICKS*60*5) return;
	
	// find lowest guard, remember count of free guards
	for (n=0; n<MAXGUARD; n++) {
		if ((m=ad->guard[n])!=-1 && !ad->an[m].target) {
			if (minlevel>ad->an[m].level) {
				minlevel=ad->an[m].level;
				guard=m;
			}
			cnt++;
		}
	}
	
	// find closest place with a threat
	for (n=0; n<ad->max_ap; n++) {
		if (ad->ap[n].threatcount && ad->ap[n].dist<mindist) {
			mindist=ad->ap[n].dist;
			place=n;
		}
	}

	// attack if possible
	if (cnt>1 && mindist<99) {
		//xlog("we nag with guard %d, level %d, at place %d",ad->an[guard].cn,ad->an[guard].level,place);
		ad->lastnag=ticker;
		ad->nagplace=place;
		ad->nagguard=guard;
		
		ad->an[guard].target=place;
		ad->an[guard].used=place;
	}
}

void ai_init(int in,unsigned int code)
{
	int n,m,cdepth,i;
	struct strategy_data *dat;

	ad=ai_data+(code-0xfffff001);

	bzero(ad,sizeof(struct ai_data));

	// find our storage
	ad->storage_in=spawner2storage(in);
	ad->storage_x=it[ad->storage_in].x;
	ad->storage_y=it[ad->storage_in].y;

	// set standard values
	ad->worklevel=1;
	for (n=0; n<MAXGUARD; n++) ad->guard[n]=-1;
	ad->lastnag=ticker;
	ad->nagguard=-1;
	ad->ppd=preset[code-0xfffff001].ppd;
	ad->pdist=3;

	// find all relevant items
	ad->max_ap=0;
	ad->ap[ad->max_ap].type=PT_STORAGE;
	ad->ap[ad->max_ap].in=ad->storage_in;
	ad->ap[ad->max_ap].dist=0;
	ad->ap[ad->max_ap].x=ad->storage_x;
	ad->ap[ad->max_ap].y=ad->storage_y;
	ad->ap[ad->max_ap].parent=-1;
	ad->ap[ad->max_ap].worker[0]=ad->ap[ad->max_ap].worker[1]=ad->ap[ad->max_ap].worker[2]=ad->ap[ad->max_ap].worker[3]=-1;
	ad->ap[ad->max_ap].eguard=-1;
	ad->max_ap++;

	for (n=1; n<MAXITEM; n++) {
		if (!it[n].flags) continue;
		if (it[n].drdata[8]!=it[in].drdata[8]) continue;
		
		if (it[n].driver==IDR_STR_DEPOT) {
			//xlog("%d: found depot %d %s at %d,%d",ad->max_ap,n,it[n].name,it[n].x,it[n].y);
			ad->ap[ad->max_ap].type=PT_DEPOT;
			ad->ap[ad->max_ap].in=n;
			ad->ap[ad->max_ap].dist=-1;
			ad->ap[ad->max_ap].x=it[n].x;
			ad->ap[ad->max_ap].y=it[n].y;
			ad->ap[ad->max_ap].worker[0]=ad->ap[ad->max_ap].worker[1]=ad->ap[ad->max_ap].worker[2]=ad->ap[ad->max_ap].worker[3]=-1;
			ad->ap[ad->max_ap].eguard=-1;
			ad->max_ap++;
		}
		if (it[n].driver==IDR_STR_MINE) {
			//xlog("%d: found mine %d %s at %d,%d",ad->max_ap,n,it[n].name,it[n].x,it[n].y);
			ad->ap[ad->max_ap].type=PT_MINE;
			ad->ap[ad->max_ap].in=n;
			ad->ap[ad->max_ap].dist=-1;
			ad->ap[ad->max_ap].x=it[n].x;
			ad->ap[ad->max_ap].y=it[n].y;
			ad->ap[ad->max_ap].worker[0]=ad->ap[ad->max_ap].worker[1]=ad->ap[ad->max_ap].worker[2]=ad->ap[ad->max_ap].worker[3]=-1;
			ad->ap[ad->max_ap].eguard=-1;
			ad->max_ap++;
		}
		if (it[n].driver==IDR_STR_STORAGE && n!=ad->storage_in) {
			//xlog("%d: found storage %d %s at %d,%d",ad->max_ap,n,it[n].name,it[n].x,it[n].y);
			ad->ap[ad->max_ap].type=PT_STORAGE;
			ad->ap[ad->max_ap].in=n;
			ad->ap[ad->max_ap].dist=-1;
			ad->ap[ad->max_ap].x=it[n].x;
			ad->ap[ad->max_ap].y=it[n].y;
			ad->ap[ad->max_ap].worker[0]=ad->ap[ad->max_ap].worker[1]=ad->ap[ad->max_ap].worker[2]=ad->ap[ad->max_ap].worker[3]=-1;
			ad->ap[ad->max_ap].enemy_possible=1;
			ad->ap[ad->max_ap].eguard=-1;

			if (it[n].drdata[8]==it[ad->storage_in].drdata[8]) {
				//xlog("found partner storage %d (%d)",ad->max_ap,it[n].drdata[8]);
				ad->partner[ad->max_partner++]=n;
			}
			ad->max_ap++;
		}
	}		

        // find depth & path to all places
	for (cdepth=0; cdepth<MAXDISTANCE; cdepth++) {
		for (n=0; n<ad->max_ap; n++) {
			if (ad->ap[n].dist==cdepth) {
				//xlog("checking place %d at %d,%d",n,ad->ap[n].x,ad->ap[n].y);
				for (i=0; i<ad->max_ap; i++) {
					if (ad->ap[i].dist!=-1) continue;
					if (abs(ad->ap[i].x-ad->ap[n].x)<20 && abs(ad->ap[i].y-ad->ap[n].y)<20 &&
					    abs(ad->ap[i].x-ad->ap[n].x)+abs(ad->ap[i].y-ad->ap[n].y)<25 &&
					    pathfinder(ad->ap[i].x,ad->ap[i].y,ad->ap[n].x,ad->ap[n].y,0,ai_check_target,200)!=-1) {
						//xlog("%d/%d: path to %d,%d found",cdepth,i,ad->ap[i].x,ad->ap[i].y);
						ad->ap[i].dist=cdepth+1;
						ad->ap[i].parent=n;
						//xlog("place %d, dist %d, parent %d",i,ad->ap[i].dist,ad->ap[i].parent);
					}
				}
			}
		}
	}

        // check for map errors
	for (n=0; n<ad->max_ap; n++) {
		if (ad->ap[n].dist==-1) {
			xlog("unconnected place %d at %d,%d",n,ad->ap[n].x,ad->ap[n].y);
		}
		//xlog("place %d, dist %d",n,ad->ap[n].dist);
	}

	// check where an enemy could come from
	for (n=0; n<ad->max_ap; n++) {
		if (ad->ap[n].enemy_possible) {
			for (m=n; m!=-1; m=ad->ap[m].parent) {
				ad->ap[m].enemy_possible=1;
				//xlog("enemy place %d at %d,%d",m,ad->ap[m].x,ad->ap[m].y);
			}
		}
	}

	// find all relevant NPCs
	for (m=0,n=getfirst_char(); n; n=getnext_char(n)) {
		if (ch[n].driver==CDR_STRATEGY && ch[n].group==code && (dat=set_data(n,DRD_STRATEGYDRIVER,sizeof(struct strategy_data)))) {
			ad->an[m].cn=n;
			ad->an[m].cserial=ch[n].serial;
			ad->an[m].order=dat->order;
			ad->an[m].or1=dat->or1;
			ad->an[m].or2=dat->or2;
			ad->an[m].target=0;
			ad->an[m].current=0;
			ad->an[m].x=ch[n].x;
			ad->an[m].y=ch[n].y;
			if (dat->order==OR_ETERNALGUARD) {
				add_etguard(m);
				ad->an[m].task=T_IGNORE;
				ad->etguardcnt++;
				//xlog("%d: found NPC %d %s (ignore)",m,n,ch[n].name);
			} else if (dat->exp || ch[n].level>50) {
				add_guard(m);
				ad->an[m].task=T_EGUARD;
				//xlog("%d: found NPC %d %s (eguard)",m,n,ch[n].name);
			} else {
				ad->an[m].task=T_IDLE;
				//xlog("%d: found NPC %d %s (worker)",m,n,ch[n].name);
			}
			ad->an[m].used=-1;
			m++;
		}
	}
	ad->ai_init=1;
}

int create_eguard(int x,int y,int group,int level,char *name,struct strategy_ppd *ppd);

int wantguardcnt(void)
{
	if (ad->npc_cnt<=3) return 0;	// 3 - 0
	if (ad->npc_cnt<=4) return 1;	// 3 - 1
	if (ad->npc_cnt<=5) return 2;	// 3 - 2
	if (ad->npc_cnt<=6) return 2;	// 4 - 2

	return ad->npc_cnt/2;
}

void ai_main(int in,unsigned int code)
{
	int n,m,i;
	struct strategy_data *dat;
	int cn,x,y,xs,ys,xe,ye,mindist=0,missing=0,ragnarok=1,nogoldleft=1,cantrain=0;
	unsigned char seen[MAXCHARS];

	ad=ai_data+(code-0xfffff001);

	if (!ad->ai_init) ai_init(in,code);		

        // update npc list
	for (ad->max_an=n=0; n<MAXNPC; n++) {
		if (ad->an[n].cn) {
			if (!ch[ad->an[n].cn].flags || ch[ad->an[n].cn].serial!=ad->an[n].cserial) {
				ad->an[n].cn=0;
				//xlog("deleted NPC %d from list",n);
			} else {
				ad->an[n].x=ch[ad->an[n].cn].x;
				ad->an[n].y=ch[ad->an[n].cn].y;
				ad->an[n].level=ch[ad->an[n].cn].level;
				ad->an[n].used=-1;
				if ((dat=set_data(ad->an[n].cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data)))) {
					ad->an[n].platin=dat->platin;
				}
				if (ad->an[n].task==T_EGUARD && ad->an[n].level<ad->ppd.max_level) cantrain=1;
				
				ad->max_an=n+1;
			}
		}
	}

	// update guard list
	for (ad->gcnt=m=0; m<MAXGUARD; m++) {
		if ((i=ad->guard[m])!=-1) {
                        if (ad->an[i].task==T_EGUARD && ad->an[i].used==-1) {
				ad->gcnt++;
				ad->an[i].used=0;
			} else ad->guard[m]=-1;
		}
	}

	// update nag guard
	if ((i=ad->nagguard)!=-1 && (!ad->an[i].cn || ad->an[i].task!=T_EGUARD || ad->an[i].target!=ad->nagplace || ticker-ad->lastnag>TICKS*90)) ad->nagguard=-1;

        // update worker/etguard counts on places
	// update platin count, find distance to closest mine
	// check for enemy presence
        ad->panic=0;
	ad->pplace=-1;
	bzero(seen,sizeof(seen));

        for (n=0,mindist=99; n<ad->max_ap; n++) {

		// update worker count
		for (ad->ap[n].wcnt=m=0; m<MAXWORKER; m++) {
			if ((i=ad->ap[n].worker[m])!=-1) {
				if (ad->an[i].target==n && ad->an[i].used==-1) {
					ad->ap[n].wcnt++;
					ad->an[i].used=n;
				} else ad->ap[n].worker[m]=-1;
			}
		}

		// update platin
                ad->ap[n].platin=ad->ap[n].platin/2+*(unsigned int*)(it[ad->ap[n].in].drdata+4);

		if (*(unsigned int *)(it[ad->ap[n].in].drdata+0)==code) ad->ap[n].owned=1;
		else ad->ap[n].owned=0;

		// update eternal guards
		if ((i=ad->ap[n].eguard)!=-1) {
                        if (ad->an[i].target==n && ad->an[i].used==-1) {
				ad->ap[n].eguard=i;
				ad->an[i].used=n;
			} else ad->ap[n].eguard=-1;
		}

		// search for enemy presence and update threats
		xs=max(0,ad->ap[n].x-12);
		xe=min(MAXMAP-1,ad->ap[n].x+12);
		ys=max(0,ad->ap[n].y-12);
		ye=min(MAXMAP-1,ad->ap[n].y+12);
	
		ad->ap[n].threat/=2;
		ad->ap[n].threatcount=0.0;
		ad->ap[n].threatncount=0.0;
		ad->ap[n].threatnlevel=0;
		if (!ad->ap[n].threat) ad->ap[n].threatlevel=0;
		
                for (y=ys; y<=ye; y+=8) {
			for (x=xs; x<=xe; x+=8) {
				for (cn=getfirst_char_sector(x,y); cn; cn=ch[cn].sec_next) {
					if (ch[cn].driver==CDR_STRATEGY && ch[cn].group!=code &&
                                            abs(ad->ap[n].x-ch[cn].x)<10 && abs(ad->ap[n].y-ch[cn].y)<10) {
						
						if (seen[cn]) { /* xlog("ignoring %s ad dist %d",ch[cn].name,ad->ap[n].dist); */ continue; }
						//else xlog("found %s at dist %d",ch[cn].name,ad->ap[n].dist);
						seen[cn]=1;

						ad->ap[n].threatcount+=THREAT(cn)*1.25;
						ad->ap[n].threatlevel=max(ad->ap[n].threatlevel,ch[cn].level);
						ad->ap[n].threat+=100+ad->ap[n].threatlevel;
                                                if (ad->ap[n].dist<=ad->pdist) {
							ad->panic=1;
							ad->pplace=n;
						}
					}
				}
			}
		}

		// move threat up the parent list
		if (ad->ap[n].parent!=-1) ad->ap[n].threat+=ad->ap[ad->ap[n].parent].threat/2;

		// move threat one down the parent list
		if (ad->ap[n].threatcount && ad->ap[n].parent!=-1) ad->ap[ad->ap[n].parent].threat=ad->ap[n].threat/2;
		
                /*if (ad->ap[n].threatcount) {
			xlog("place %d at %d,%d, threat=%d, level=%d, count=%.0fM",n,ad->ap[n].x,ad->ap[n].y,ad->ap[n].threat,ad->ap[n].threatlevel,ad->ap[n].threatcount/1000);
		}*/

		if (*(unsigned int*)(it[ad->ap[n].in].drdata+4)>0 && ad->ap[n].wcnt>0) {
			for (m=ad->ap[n].parent; m!=-1 && ad->ap[m].wcnt>0; m=ad->ap[m].parent) {
				ad->ap[m].platin=max(ad->ap[m].platin,50);
			}
		}

		// find distance to closest mine
		if (ad->ap[n].type==PT_MINE && ad->ap[n].platin && !ad->ap[n].threat) {
			if (ad->ap[n].dist<mindist) mindist=ad->ap[n].dist;
		}
		if (ad->ap[n].platin && !ad->ap[n].threat) {
			if (n>0) nogoldleft=0; //xlog("nogo: %d",n); }
                        if (n==0) {
				if (ad->ap[n].platin/2>ad->ppd.max_level && cantrain) { ragnarok=0; }
			} else { ragnarok=0; }
		}
	}
	ad->pdist=min(ad->pdist,mindist);

	// project threats to neighboring places
	for (n=0; n<ad->max_ap; n++) {
		if (ad->ap[n].threatcount && ad->ap[n].parent!=-1) {
			ad->ap[ad->ap[n].parent].threatncount+=ad->ap[n].threatcount;
			ad->ap[ad->ap[n].parent].threatnlevel=max(ad->ap[ad->ap[n].parent].threatnlevel,ad->ap[n].threatlevel);
		}
		if (ad->ap[n].parent!=-1 && ad->ap[ad->ap[n].parent].threatcount) {
			ad->ap[n].threatncount+=ad->ap[ad->ap[n].parent].threatcount;
			ad->ap[n].threatnlevel=max(ad->ap[n].threatnlevel,ad->ap[ad->ap[n].parent].threatlevel);
		}
	}

        // update free NPC count
	ad->npc_cnt=0;
	ad->free_workers=0;
	for (n=0; n<ad->max_an; n++) {
		if (ad->an[n].cn && ad->an[n].task!=T_IGNORE) {
			ad->npc_cnt++;
			if (ad->an[n].used==-1) ad->free_workers++;	// eguards have used!=-1 anyway
		}
	}

        // create new workers
        while ((ad->panic || !ad->free_workers) && ad->npc_cnt<min(ad->ppd.max_worker,16+(*(unsigned int*)(it[ad->storage_in].drdata+4))/500)) {
                if (*(unsigned int*)(it[ad->storage_in].drdata+4)>=NPCPRICE) {	// spawn new worker
                        m=spawner_sub(in,ad->storage_in,code,preset[code-0xfffff001].name,&ad->ppd);
			if (m==0) break;
	
			ad->free_workers++;
			// add new npc to list
			for (n=0; n<MAXNPC; n++) {
				if (!ad->an[n].cn) {
					ad->an[n].cn=m;
					ad->an[n].cserial=ch[m].serial;
					ad->an[n].order=0;
					ad->an[n].task=T_IDLE;
					ad->an[n].target=0;
					ad->an[n].current=0;
                                        ad->an[n].x=ch[m].x;
					ad->an[n].y=ch[m].y;
					ad->an[n].used=-1;
					break;
				}
			}			
		} else break;
	}

	if (ad->panic) {
		// assign tasks to workers
		for (n=0; n<ad->max_an; n++) {
                        if (ad->an[n].task!=T_EGUARD && ad->an[n].task!=T_IGNORE) {
				ad->an[n].task=T_FIGHT;
				if (ad->an[n].used!=-1) remove_worker(n);
			}
			ad->an[n].target=ad->pplace;
		}
	} else {
                // assign tasks to workers
		for (n=0; n<ad->max_an; n++) {
			int bm,bd,bdiff;

			if (ad->an[n].task==T_EGUARD && wantguardcnt()<ad->gcnt && !ad->nogoldleft && !ad->ragnarok) {
				remove_guard(n);
			}
	
			// never touch elite guards
			if (ad->an[n].task==T_EGUARD) continue;
			// we may not touch eternal guards
			if (ad->an[n].task==T_IGNORE) continue;

			if ((wantguardcnt()>ad->gcnt || ad->nogoldleft) && ad->gcnt<MAXGUARD) {
				if (ad->an[n].used!=-1) remove_worker(n);
				ad->an[n].task=T_EGUARD;
				ad->an[n].target=0;
				add_guard(n);
				continue;
			}
	
			m=ad->an[n].target;
			i=ad->ap[m].parent;
	
			if (ad->an[n].used!=-1 &&
			    ad->an[n].platin &&
			    (ad->an[n].task==T_TRANSFER || ad->an[n].task==T_MINE) &&
			    ad->ap[m].wcnt<=ad->worklevel &&
			    !ad->ap[m].threat &&
			    ad->ap[m].dist<=mindist) continue;

			if (ad->an[n].used!=-1 &&
                            (ad->an[n].task==T_TAKE && !ad->ap[m].owned) &&
                            !ad->ap[m].threat &&
			    ad->ap[m].dist<=mindist) continue;

                        if (ad->ap[m].threat || ad->ap[i].threat || ad->ap[m].dist>mindist) {
				if (ad->an[n].used!=-1) remove_worker(n);
			} else if (i>0 && ad->ap[m].wcnt>ad->ap[i].wcnt && ad->ap[i].platin>(ad->ap[i].wcnt+1)*WORKERPLATIN && ad->ap[i].wcnt<ad->worklevel) {
				if (ad->an[n].used!=-1) remove_worker(n);
				add_worker(T_TRANSFER,n,i);
				continue;
			} else if (ad->an[n].used!=-1 && ad->an[n].task==T_TRANSFER && ad->ap[m].platin && ad->ap[m].wcnt<=ad->worklevel) continue;
			else if (ad->an[n].used!=-1 && ad->an[n].task==T_MINE && ad->ap[m].platin && ad->ap[m].wcnt<=ad->worklevel) continue;

			bm=0; bd=99; bdiff=0;
			for (m=1; m<ad->max_ap; m++) {
				if (ad->ap[m].type==PT_DEPOT && !ad->ap[m].owned && !ad->ap[m].threat && ad->ap[m].wcnt==0 &&
				    (bd>ad->ap[m].dist || (bd==ad->ap[m].dist && abs(ad->an[n].x-ad->ap[m].x)+abs(ad->an[n].y-ad->ap[m].y)<bdiff))) {
					bm=m;
					bd=ad->ap[m].dist;
					bdiff=abs(ad->an[n].x-ad->ap[m].x)+abs(ad->an[n].y-ad->ap[m].y);
				}
			}
                        if (bm && bd<=mindist) {
				remove_worker(n);
				add_worker(T_TAKE,n,bm);
				//xlog("%d take %d",ad->an[n].cn,bm);
				continue;
			}

	
			bm=0; bd=99; bdiff=0;
			for (m=1; m<ad->max_ap; m++) {
				if (!ad->ap[m].threat && ad->ap[m].platin>(ad->ap[m].wcnt*WORKERPLATIN) && ad->ap[m].wcnt<ad->worklevel &&
				    (bd>ad->ap[m].dist || (bd==ad->ap[m].dist && abs(ad->an[n].x-ad->ap[m].x)+abs(ad->an[n].y-ad->ap[m].y)<bdiff))) {
					bm=m;
					bd=ad->ap[m].dist;
					bdiff=abs(ad->an[n].x-ad->ap[m].x)+abs(ad->an[n].y-ad->ap[m].y);
				}
			}
			if (bm && bd<=mindist) {
				remove_worker(n);
				if (ad->ap[bm].type==PT_MINE) add_worker(T_MINE,n,bm);
				else add_worker(T_TRANSFER,n,bm);
				continue;
			}
	
			if (ad->an[n].used!=-1) remove_worker(n);

			ad->an[n].task=T_IDLE;
			ad->an[n].target=0;
		}
	
		// find places with too little workers
		for (n=0; n<ad->max_ap; n++) {
			if (n!=0 && ad->ap[n].dist<=mindist && ad->ap[n].platin && !ad->ap[n].threat) {
				//xlog("place %d, %d platin, %d workers",n,ad->ap[n].platin,ad->ap[n].wcnt);
				if (ad->ap[n].wcnt<ad->worklevel-1 && ad->ap[n].wcnt*WORKERPLATIN<ad->ap[n].platin) {
					missing=1;
					//xlog("missing at %d (%d,%d)",n,ad->ap[n].x,ad->ap[n].y);
				}
			}
		}
	
                //....... threathandling ................

		// expire old entries
		for (m=0; m<ad->max_at; m++) {
			if (ad->at[m].place && ticker-ad->at[m].ticker>TICKS*20) {
				ad->at[m].place=0;
			}
		}

		// update/add current threats
		for (n=0; n<ad->max_ap; n++) {
			if ((ad->ap[n].dist<=mindist && ad->ap[n].threatcount)) {
				for (m=0; m<ad->max_at; m++) {
					if (ad->at[m].place==n) {
						ad->at[m].ticker=ticker;
                                                ad->at[m].count=ad->ap[n].threatcount+ad->ap[n].threatncount; //max(ad->at[m].count,ad->ap[n].threatcount+ad->ap[n].threatncount);
						ad->at[m].level=max(ad->ap[n].threatlevel,ad->ap[n].threatnlevel); //max(max(ad->at[m].level,ad->ap[n].threatlevel),ad->ap[n].threatnlevel);
                                                break;
					}
				}
				if (m==ad->max_at) {
					for (m=0; m<ad->max_at; m++)
						if (ad->at[m].place==0) break;
					ad->at[m].place=n;
					ad->at[m].ticker=ticker;
					ad->at[m].count=ad->ap[n].threatcount+ad->ap[n].threatncount; //max(ad->at[m].count,ad->ap[n].threatcount+ad->ap[n].threatncount);
						ad->at[m].level=max(ad->ap[n].threatlevel,ad->ap[n].threatnlevel); //max(max(ad->at[m].level,ad->ap[n].threatlevel),ad->ap[n].threatnlevel);
					ad->max_at=max(ad->max_at,m+1);
				}
			}
		}
		qsort(ad->at,ad->max_at,sizeof(ad->at[0]),tcomp);

		// reduce max_at if possible
		for (n=m=0; m<ad->max_at; m++) {
			if (ad->at[m].place) {
				for (i=ad->ap[ad->at[m].place].parent; i!=-1; i=ad->ap[i].parent) {
					if (ad->ap[i].threatcount) {
						/*xlog("skip place %d at %d,%d, count=%.0fM, level=%d",
							ad->at[m].place,
							ad->ap[ad->at[m].place].x,ad->ap[ad->at[m].place].y,
							ad->at[m].count/1000.0,
							ad->at[m].level);*/
						break;
					}
				}
				if (i==-1) {
					//xlog("place %d at %d,%d, count=%.0fM, level=%d, age=%.2fm",ad->at[m].place,ad->ap[ad->at[m].place].x,ad->ap[ad->at[m].place].y,ad->at[m].count/1000.0,ad->at[m].level,(ticker-ad->at[m].ticker)/((double)TICKS*60));
					if (assign_guards(ad->at[m].place,ad->at[m].count+1,ad->at[m].level,ad->ragnarok)) ragnarok=ad->ragnarok;
				}
				n=m;
			}
		}
		ad->max_at=n+1;
		remove_free_guards();

                // increase worklevel if resources permit
		if (!missing && ad->free_workers>0 && ticker-ad->lastchange>TICKS*20) { ad->worklevel=min(MAXWORKER,ad->worklevel+1); ad->lastchange=ticker; }
		if (missing && ad->free_workers==0 && ticker-ad->lastchange>TICKS*10) { ad->worklevel=max(1,ad->worklevel-1); ad->lastchange=ticker; }

		// place eternal guards
		if (ad->ppd.eguards>ad->etguardcnt) {
			for (n=0; n<ad->max_ap; n++) {
				if ((ad->ap[n].dist==ad->pdist && ad->ap[n].enemy_possible && ad->ap[n].eguard==-1 && *(unsigned int*)(it[ad->ap[n].in].drdata)==code)) {
					m=create_eguard(ad->ap[n].x+2,ad->ap[n].y+2,code,ad->ppd.eguardlvl,preset[code-0xfffff001].name,&preset[code-0xfffff001].ppd);
					for (i=0; i<MAXNPC; i++) {
						if (!ad->an[i].cn) {
							ad->an[i].cn=m;
							ad->an[i].cserial=ch[m].serial;
							ad->an[i].order=OR_ETERNALGUARD;
							ad->an[i].or1=ch[m].x;
							ad->an[i].or2=ch[m].y;
							ad->an[i].task=T_IGNORE;
							ad->an[i].target=n;
							ad->an[i].current=n;
							ad->an[i].x=ch[m].x;
							ad->an[i].y=ch[m].y;
							ad->an[i].used=n;
							break;
						}
					}
					add_etguard(i);
					ad->etguardcnt++;
					//xlog("created etguard for place %d",n);
				}
			}
		}

		// -------- nag attacks -----------
		nag_attack();
	}

        ad->ragnarok=ragnarok;
	ad->nogoldleft=nogoldleft;

	//xlog("%s: dist=%d, free=%d, cnt=%d, level=%d, guards=%d, panic=%d, nogo=%d, rag=%d, ctrain=%d",preset[code-0xfffff001].name,mindist,ad->free_workers,ad->npc_cnt,ad->worklevel,ad->gcnt,ad->panic,nogoldleft,ragnarok,cantrain);

        // make NPCs do their jobs
	for (n=0; n<ad->max_an; n++) {
		//xlog("NPC %d, target %d at %d,%d",n,ad->an[n].target,ad->ap[ad->an[n].target].x,ad->ap[ad->an[n].target].y);
		switch(ad->an[n].task) {
			case T_IDLE:		task_idle(n); break;
			case T_MINE:		task_mine(n); break;
			case T_TRANSFER:	task_transfer(n); break;
                        case T_FIGHT:		task_fight(n); break;
			case T_EGUARD:		if (ad->an[n].target==0) {
							 if (ch[ad->an[n].cn].level<ad->ppd.max_level &&
							     (ad->ap[0].platin>NPCPRICE*2 || ad->free_workers || ad->an[n].platin>ad->ppd.trainspeed*TRAINMULTI*2 || ad->npc_cnt>=ad->ppd.max_worker))
								 { task_train(n); }
							 else { task_idle(n); }
						} else task_guard(n);
						break;
			case T_IGNORE:		break;
			case T_TAKE:		task_take(n); break;

		}
		if ((dat=set_data(ad->an[n].cn,DRD_STRATEGYDRIVER,sizeof(struct strategy_data)))) {
			dat->order=ad->an[n].order;
			dat->or1=ad->an[n].or1;
			dat->or2=ad->an[n].or2;
		}
	}
}

// path distance calculation subroutine
// use above for assignments of workers/guards

#define MAXJUMP		256
struct jumppoint
{
	int in;
	int x,y;
};

struct jumppoint jp[MAXJUMP];
static int special_init=0,max_jp=1;

int create_eguard(int x,int y,int group,int level,char *name,struct strategy_ppd *ppd)
{
	int co;
	struct strategy_data *dat;

        co=create_char("strategy_npc",0);
	if (drop_char(co,x,y,0)) {
		ch[co].tmpx=x;
		ch[co].tmpy=y;
		
		ch[co].level=level;
		ch[co].value[1][V_WIS]=ch[co].level;
		ch[co].value[1][V_INT]=ch[co].level;
		ch[co].value[1][V_AGI]=ch[co].level;
		ch[co].value[1][V_STR]=ch[co].level;

		ch[co].value[1][V_WARCRY]+=ppd->warcry;
		ch[co].value[1][V_ENDURANCE]+=ppd->endurance;
		ch[co].value[1][V_SPEED]+=ppd->speed;
	
		update_char(co);
	
                ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
		ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
		ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;

		ch[co].dir=DX_RIGHTDOWN;
		ch[co].sprite=353+ppd->npc_color;

		ch[co].group=group;
		dat=set_data(co,DRD_STRATEGYDRIVER,sizeof(struct strategy_data));
		if (dat) {
			strncpy(dat->name,name,19); dat->name[19]=0;
			dat->order=OR_ETERNALGUARD;
			dat->or1=x;
			dat->or2=y;
		}
	} else {
		destroy_char(co);
		return 0;
	}
	return co;
}

int str_exp_cost(struct strategy_ppd *ppd,int nr)
{
	switch(nr) {
		case 1:		if (ppd->income<20) return 25; else return 0;
		case 2:		if (ppd->max_level<115) return 4; else return 0;
                case 3:		if (ppd->max_worker<16) return 10; else return 0;
		case 4:		if (ppd->trainspeed<8) return 4; else return 0;
		case 5:		if (ppd->warcry<115) return 4; else return 0;
		case 6:		if (ppd->endurance<115) return 4; else return 0;
		case 7:		if (ppd->speed<115) return 6; else return 0;
		case 8:		if (ppd->eguardlvl<115) return 3; else return 0;
		
		default:	return 0;
	}
}

int str_increment(struct strategy_ppd *ppd,int nr)
{
	switch(nr) {
		case 1:		if (str_exp_cost(ppd,nr)) return 1; else return 0;
		case 2:		if (str_exp_cost(ppd,nr)) return 2; else return 0;
		case 3:		if (str_exp_cost(ppd,nr)) return 1; else return 0;
		case 4:		if (str_exp_cost(ppd,nr)) return 1; else return 0;
		case 5:		if (str_exp_cost(ppd,nr)) return 5; else return 0;
		case 6:		if (str_exp_cost(ppd,nr)) return 5; else return 0;
		case 7:		if (str_exp_cost(ppd,nr)) return 5; else return 0;
                case 8:		if (str_exp_cost(ppd,nr)) return 1; else return 0;
		default:	return 0;
	}
}

int str_raise(int cn,struct strategy_ppd *ppd,int nr)
{
	int cost;

	if (!str_exp_cost(ppd,nr)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot raise this value any higher.");
		return 0;
	}
	
	cost=str_exp_cost(ppd,nr);
	if (cost>ppd->exp) {
		log_char(cn,LOG_SYSTEM,0,"You cannot afford to raise this value.");
		return 0;
	}

	switch(nr) {
		case 1:		ppd->income=min(ppd->income+str_increment(ppd,nr),20); break;
		case 2:		ppd->max_level=min(ppd->max_level+str_increment(ppd,nr),115); break;
                case 3:		ppd->max_worker=min(ppd->max_worker+str_increment(ppd,nr),24); break;
		case 4:		ppd->trainspeed=min(ppd->trainspeed+str_increment(ppd,nr),8); break;
		case 5:		ppd->warcry=min(ppd->warcry+str_increment(ppd,nr),115); break;
		case 6:		ppd->endurance=min(ppd->endurance+str_increment(ppd,nr),115); break;
                case 7:		ppd->speed=min(ppd->speed+str_increment(ppd,nr),115); break;		
		case 8:		ppd->eguardlvl=min(ppd->eguardlvl+str_increment(ppd,nr),115); break;
		default:	log_char(cn,LOG_SYSTEM,0,"Please report bug #4371g");
				return 0;
	}
	ppd->exp-=cost;
	log_char(cn,LOG_SYSTEM,0,"Done.");
	return 1;
}

void queue_validate(int ar)
{
	int n,cn,m;

	for (n=0; n<MAXQUEUE; n++) {
		if ((cn=area[ar].q_playercn[n]) && (!ch[cn].flags || ch[cn].ID!=area[ar].q_playerID[n])) {
			area[ar].q_playercn[n]=area[ar].q_playerID[n]=0;
		}
	}
	for (n=m=0; n<MAXQUEUE; n++) {
		area[ar].q_playercn[m]=area[ar].q_playercn[n];
		area[ar].q_playerID[m]=area[ar].q_playerID[n];
		if (area[ar].q_playercn[m]) m++;		
	}
	for (; m<MAXQUEUE; m++) {
		area[ar].q_playercn[m]=area[ar].q_playerID[m]=0;
	}
}

void queue_remove(int cn)
{
	int ar,n;

	for (ar=0; ar<MAX_STR_AREA; ar++) {
		for (n=0; n<MAXQUEUE; n++) {
			if (area[ar].q_playerID[n]==ch[cn].ID) {
				area[ar].q_playercn[n]=area[ar].q_playerID[n]=0;
			}
		}
	}
}

void queue_mission(int cn,int ar)
{
	int n;

	queue_validate(ar);

	// already in queue?
	for (n=0; n<MAXQUEUE; n++) {
		if (area[ar].q_playerID[n]==ch[cn].ID) return;		
	}
	
	queue_remove(cn);
	
	for (n=0; n<MAXQUEUE; n++) {
		if (!area[ar].q_playercn[n]) {
			area[ar].q_playercn[n]=cn;
			area[ar].q_playerID[n]=ch[cn].ID;
			return;	
		}
	}
}

int queue_check(int cn,int ar)
{
	queue_validate(ar);

	if (area[ar].q_playerID[0] && area[ar].q_playerID[0]!=ch[cn].ID) return 0;
	
	return 1;
}

void show_queue(int cn,int ar)
{
	int n;

	log_char(cn,LOG_SYSTEM,0,"Queue:");

	queue_validate(ar);
	for (n=0; n<MAXQUEUE; n++) {
		if (area[ar].q_playercn[n]) log_char(cn,LOG_SYSTEM,0,"%d: %s",n+1,ch[area[ar].q_playercn[n]].name);
	}
}

int special_driver(int nr,int cn,char *ptr)
{
	int len,val,in;
	struct strategy_ppd *ppd;

        if (nr!=CDR_STRATEGY_PARSER) return 0;

	ppd=set_data(cn,DRD_STRATEGY_PPD,sizeof(struct strategy_ppd));
	if (!ppd) return 0;

        if (!special_init) {
		int n;

		for (n=1; n<MAXITEM; n++) {
			if (it[n].flags && (it[n].driver==IDR_STR_DEPOT || it[n].driver==IDR_STR_STORAGE)) {
				jp[max_jp].in=n;
				jp[max_jp].x=it[n].x;
				jp[max_jp].y=it[n].y;
				if (it[n].driver==IDR_STR_DEPOT) sprintf(it[n].description,"JP %d. A depot is used to store Platinum temporarily.",max_jp);
				else sprintf(it[n].description,"JP %d. The storage contains all Platinum collected so far.",max_jp);
				max_jp++;
				if (max_jp==MAXJUMP) {
					elog("too many jump points");
					break;
				}
			}
		}
		special_init=1;
	}

        if (*ptr=='#' || *ptr=='/') {
		ptr++;
                if ((len=cmdcmp(ptr,"jp",2))) {
			
			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}
			
			ptr+=len;

			val=atoi(ptr);
			if (val<1 || val>=max_jp) {
				log_char(cn,LOG_SYSTEM,0,"Jump point out of bounds.");
				return 2;
			}
			if (*(unsigned int*)(it[jp[val].in].drdata+0)!=ch[cn].ID) {
				log_char(cn,LOG_SYSTEM,0,"You can only jump to points you control.");
				return 2;
			}
			teleport_char_driver(cn,jp[val].x,jp[val].y);

			return 2;
		}

		if ((len=cmdcmp(ptr,"list",4))) {
			int flag=0;

			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			for (val=1; val<max_jp; val++) {
				if (*(unsigned int*)(it[jp[val].in].drdata+0)==ch[cn].ID) {
					log_char(cn,LOG_SYSTEM,0,"JP %d: \03%s \020%d Platinum.",val,it[jp[val].in].name,*(unsigned int*)(it[jp[val].in].drdata+4));
					flag=1;
				}
			}
			if (flag) log_char(cn,LOG_SYSTEM,0,"Use /jp <nr> to teleport.");

			return 2;
		}

		if ((len=cmdcmp(ptr,"eguard",2))) {
			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			ptr+=len;

			val=atoi(ptr);
			if (val<1 || val>=max_jp) {
				log_char(cn,LOG_SYSTEM,0,"Jump point out of bounds.");
				return 2;
			}
			
			in=jp[val].in;
			
			if (*(unsigned int*)(it[in].drdata+0)!=ch[cn].ID) {
				log_char(cn,LOG_SYSTEM,0,"You can only create guards for points you control.");
				return 2;
			}
			
			if (map_dist(ch[cn].x,ch[cn].y,it[in].x,it[in].y)>10) {
				log_char(cn,LOG_SYSTEM,0,"You must be closer to the jump point you wish to create a guard for.");
				return 2;
			}

			if (ppd->eguards<1) {
				log_char(cn,LOG_SYSTEM,0,"No extra guards left.");
				return 2;
			}
			
			create_eguard(ch[cn].x,ch[cn].y,ch[cn].ID,ppd->eguardlvl,ch[cn].name,ppd);
			ppd->eguards--;

			return 2;
		}

		if ((len=cmdcmp(ptr,"info",4))) {
			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			log_char(cn,LOG_SYSTEM,0,"\001Name \010Value \015Exp Cost\21Increment");
                        log_char(cn,LOG_SYSTEM,0,"1 \001Base Income: \011%d \016%d \022%d",ppd->income,str_exp_cost(ppd,1),str_increment(ppd,1));
			log_char(cn,LOG_SYSTEM,0,"2 \001Max Level: \011%d \016%d \022%d",ppd->max_level,str_exp_cost(ppd,2),str_increment(ppd,2));
			log_char(cn,LOG_SYSTEM,0,"3 \001Max Worker: \011%d \016%d \022%d",ppd->max_worker,str_exp_cost(ppd,3),str_increment(ppd,3));
			log_char(cn,LOG_SYSTEM,0,"4 \001Training Speed: \011%d \016%d \022%d",ppd->trainspeed,str_exp_cost(ppd,4),str_increment(ppd,4));
			log_char(cn,LOG_SYSTEM,0,"5 \001Warcry Bonus: \011%d \016%d \022%d",ppd->warcry,str_exp_cost(ppd,5),str_increment(ppd,5));
			log_char(cn,LOG_SYSTEM,0,"6 \001Endurance: \011%d \016%d \022%d",ppd->endurance,str_exp_cost(ppd,6),str_increment(ppd,6));
			log_char(cn,LOG_SYSTEM,0,"7 \001Speed Bonus: \011%d \016%d \022%d",ppd->speed,str_exp_cost(ppd,7),str_increment(ppd,7));
                        log_char(cn,LOG_SYSTEM,0,"- \001Extra Guards: \011%d \016- \022-",ppd->eguards);
                        log_char(cn,LOG_SYSTEM,0,"8 \001Guard Level: \011%d \016%d \022%d",ppd->eguardlvl,str_exp_cost(ppd,8),str_increment(ppd,8));

			log_char(cn,LOG_SYSTEM,0,"- \001Missions: \011%d \016-\022-",ppd->mis_cnt);
			log_char(cn,LOG_SYSTEM,0,"- \001Victories: \011%d \016-\022-",ppd->won_cnt);
			log_char(cn,LOG_SYSTEM,0,"- \001Experience: \011%d \016-\022-",ppd->exp);
			
                        return 2;
		}

		if ((len=cmdcmp(ptr,"raise",4))) {
			int n;

			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			n=atoi(ptr+len);
			if (n<1 || n>9) {
				log_char(cn,LOG_SYSTEM,0,"Number is out of bounds.");
				return 2;
			}
			str_raise(cn,ppd,n);
			return 2;
		}

		if ((ch[cn].flags&CF_GOD) && (len=cmdcmp(ptr,"reset",4))) {
			ppd->init_done=0;
			
			return 2;
		}

		if ((len=cmdcmp(ptr,"mission",4))) {
			int n,m,cnt,self;

			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			log_char(cn,LOG_SYSTEM,0,"°c3Nr \002Name \005Busy \007Solved \013Enemies ");
			for (n=0; n<ARRAYSIZE(mission); n++) {
                                if (mission[n].need_solve && !ppd->solve_cnt[mission[n].need_solve] && !ppd->solve_cnt[mission[n].need_solve2]) continue;
				if (mission[n].set_solve && ppd->solve_cnt[mission[n].set_solve]>=MAXMISSIONTRY) continue;
				if (!area[mission[n].area].max_spawn) continue;

				queue_validate(mission[n].area);
				for (m=cnt=self=0; m<MAXQUEUE; m++) {
					if (area[mission[n].area].q_playercn[m]) cnt++;
					if (area[mission[n].area].q_playerID[m]==ch[cn].ID) self=cnt;
				}

				log_char(cn,LOG_SYSTEM,0,"°c2 %d \002%s \006%d%c%c \010%d \013%s %s %s %s",
					 n+1,
					 mission[n].name,
                                         area[mission[n].area].busy+cnt,
					 self ? '/' : ' ',
					 self ? self+'0' : ' ',
					 mission[n].set_solve ? ppd->solve_cnt[mission[n].set_solve] : 0,
					 preset[mission[n].enemy[0]].name,
					 preset[mission[n].enemy[1]].name,
					 preset[mission[n].enemy[2]].name,
					 preset[mission[n].enemy[3]].name);

			}
			log_char(cn,LOG_SYSTEM,0,"Use /enter <nr> to start a mission. If that mission is busy your request will be queued.");
                        return 2;
		}

		if ((len=cmdcmp(ptr,"enter",4))) {
			int n,spawn,twoparty=0,ar;

			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			n=atoi(ptr+len)-1;
			if (n<0 || n>=ARRAYSIZE(mission)) {
				log_char(cn,LOG_SYSTEM,0,"Mission number is out of bounds.");
				return 2;
			}
                        if (mission[n].need_solve && !ppd->solve_cnt[mission[n].need_solve] && !ppd->solve_cnt[mission[n].need_solve2]) {
				log_char(cn,LOG_SYSTEM,0,"Mission number is out of bounds.");
				return 2;
			}
			if (mission[n].set_solve && ppd->solve_cnt[mission[n].set_solve]>=MAXMISSIONTRY) {
				log_char(cn,LOG_SYSTEM,0,"Mission number is out of bounds.");
				return 2;
			}
			if (!area[mission[n].area].max_spawn) {
				log_char(cn,LOG_SYSTEM,0,"Mission number is out of bounds.");
				return 2;
			}

			ar=mission[n].area;

			for (spawn=0,in=1; in<MAXITEM; in++) {
				if (!it[in].flags) continue;
				if (it[in].driver!=IDR_STR_SPAWNER) continue;
				
                                if (it[in].drdata[8]==mission[n].area) {
					if (*(unsigned int*)(it[in].drdata+0)==0) spawn=in;
					else if (*(unsigned int*)(it[in].drdata+0)<0xfffff000) twoparty++;
				}
				
				if (*(unsigned int*)(it[in].drdata+0)==ch[cn].ID) {
					queue_remove(cn);
                                        teleport_char_driver(cn,it[in].x,it[in].y);
					log_char(cn,LOG_SYSTEM,0,"Re-entering mission.");					
					return 2;
				}
			}
			if (!spawn) {
				queue_mission(cn,ar);
				log_char(cn,LOG_SYSTEM,0,"Mission area is busy. Request has been queued.");
				show_queue(cn,ar);
				return 2;
			}
			if (!queue_check(cn,ar)) {
				log_char(cn,LOG_SYSTEM,0,"You are not the next one in the queue.");
				queue_mission(cn,ar);
				show_queue(cn,ar);
				return 2;
			}
			queue_remove(cn);

			if (!twoparty) init_mission(n);

			ppd->mis_cnt++;
			ppd->current_mission=n;

                        teleport_char_driver(cn,it[spawn].x,it[spawn].y);
			take_spawner(spawn,cn,ppd);

                        return 2;
		}
		if ((len=cmdcmp(ptr,"surrender",9))) {

			if (ppd->boss_stage<9) {
				log_char(cn,LOG_SYSTEM,0,"You have to talk to Cinciac first.");
				return 2;
			}

			if (!remove_party(ch[cn].ID,NULL)) {
				log_char(cn,LOG_SYSTEM,0,"You are not doing any mission.");
			}

                        return 2;
		}

		if ((len=cmdcmp(ptr,"queue",5)) && (ch[cn].flags&CF_GOD)) {
			int ar,n;

                        for (ar=0; ar<MAX_STR_AREA; ar++) {
				for (n=0; n<MAXQUEUE; n++) {
					if (!area[ar].q_playercn[n]) continue;
                                        log_char(cn,LOG_SYSTEM,0,"Area %d, queue %d, cn=%d, ID=%d",ar,n,area[ar].q_playercn[n],area[ar].q_playerID[n]);
				}
			}

                        return 2;
		}
	}

	return 1;
}

/* todo:

- respawn logic for neutral NPCs

- */
