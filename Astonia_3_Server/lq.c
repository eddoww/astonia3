/*
 * $Id: lq.c,v 1.4 2007/08/13 18:50:38 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: lq.c,v $
 * Revision 1.4  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.3  2005/12/13 23:08:06  ssim
 * added missing 125 -> 200 change
 *
 * Revision 1.2  2005/12/04 17:47:39  ssim
 * changed allowed max level from 120 to 200
 * allowed NPCs to have skills up to 200 (ex. 125)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

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
#include "bank.h"
#include "player_driver.h"
#include "command.h"
#include "misc_ppd.h"
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
		default: 	return 0;
	}
}


void update_lqdoor(int nr);
int remove_npc(int n);
int spawn_npc(int n,int isthrall,int tx,int ty,char *thrallname);

#define MAXLQNPC	512
#define MAXLQDOOR	256
#define MAXLQMARK	10

struct lq_item
{
	char base[40];
	char name[sizeof(it[0].name)];
	char description[sizeof(it[0].description)];
	unsigned int keyID;
};

struct lq_npc
{
	char basename[40];
	int x,y,dir;
	int level;
	char mode;
	int sprite;
	int respawn;
	char name[sizeof(ch[0].name)];
	char description[sizeof(ch[0].description)];

	char nick[2][40];
	
	char greeting[256];
	char trigger[5][40];
	char reply[5][256];

	unsigned int want_keyID;
	struct lq_item reward_item;
	unsigned int reward_markID;

	unsigned int kill_markID;
	unsigned int hurt_markID;

	struct lq_item carry_item;
	int carry_gold;

	int cn,cserial;
};

struct lq_door
{
        char nick[40];

	int keyID;
	int in;
};

struct lq_chest
{
	char name[sizeof(it[0].name)];
	char description[sizeof(it[0].description)];

	int keyID;

	struct lq_item item;
};

struct lq_data
{
	int min_level;
	int max_level;
	int reward[MAXLQMARK];
	char reward_desc[MAXLQMARK][80];
	int open;
	int entrance_x,entrance_y;
};

struct lq_npc lq_npc[MAXLQNPC];
struct lq_door lq_door[MAXLQDOOR];
struct lq_data lq_data;

static int init_done=0;

struct lq_npc_data
{
	int n;		// index into lq_npc

	char mode;
	char greeting[256];
	char trigger[5][40];
	char reply[5][256];

	char thrallname[40];

	unsigned int want_keyID;
	struct lq_item reward_item;
	unsigned int reward_markID;

	unsigned int kill_markID;
	unsigned int hurt_markID;

	int usurp,udx,udy;
	int dir;
	int follow;
};

int lq_respawn[MAXLQNPC];

struct lq_plr_data
{
	char mark[MAXLQMARK];

	// lq master data
	int usurp;
};

#define log_sys(cn,format,args...) log_char(cn,LOG_SYSTEM,0,format,## args)

int create_lq_item(struct lq_item *i)
{
	int in;
	char buf[256];

	sprintf(buf,"lq_%s",i->base);
	in=create_item(buf);
	if (!in) return 0;
	
	if (i->name[0]) strcpy(it[in].name,i->name);
	if (i->description[0]) strcpy(it[in].description,i->description);
	it[in].ID=MAKE_ITEMID(DEV_ID_LQ,i->keyID);
	it[in].flags|=IF_LABITEM;

	return in;
}

int find_free_npc(void)
{
	int n;

	for (n=1; n<MAXLQNPC; n++) {
		if (!lq_npc[n].basename[0]) return n;		
	}
	return 0;
}

void free_npc(int n)
{
	if (n>0 && n<MAXLQNPC) lq_npc[n].basename[0]=0;
}

char *get_int(char *ptr,int *pval)
{
	while (isspace(*ptr)) ptr++;
        if (!isdigit(*ptr) && !(*ptr=='-' && isdigit(*(ptr+1)))) return NULL;
        if (pval) *pval=atoi(ptr);
	if (*ptr=='-') ptr++;
        while (isdigit(*ptr)) ptr++;
	return ptr;
}

char *get_str(char *ptr,char *dst,int len)
{
	int n,flag=0;

	while (isspace(*ptr)) ptr++;
	if (!*ptr) return NULL;

	if (*ptr=='"' || *ptr=='\'') {
		ptr++;
		flag=1;
	}
	for (n=0; n<len-1; n++) {
		if (flag && (*ptr=='"')) break;		
		if (!flag && isspace(*ptr)) break;
                if (!*ptr) break;
		
		*dst++=*ptr++;
	}
	*dst=0;
	while (*ptr && !isspace(*ptr)) ptr++;

	return ptr;
}

char *get_chr(char *ptr,char *pval)
{
	while (isspace(*ptr)) ptr++;
	if (!isalpha(*ptr)) return NULL;
	if (pval) *pval=*ptr;
	while (*ptr && !isspace(*ptr)) ptr++;
	return ptr;
}

int check_anything(char *ptr)
{
	while (isspace(*ptr)) ptr++;
	if (*ptr) return 1;

	return 0;
}


int get_lq_item(int cn,char *ptr,struct lq_item *li,char *usage)
{
	char basename[40],modbasename[80],name[sizeof(it[0].name)],desc[sizeof(it[0].description)];
	int keyID,base;

	if (!(ptr=get_str(ptr,basename,sizeof(basename)))) { log_sys(cn,"°c3Missing base. Usage is: %s.",usage); return 0; }
	if (!(ptr=get_int(ptr,&keyID))) keyID=0;
        if (!ptr || !(ptr=get_str(ptr,name,sizeof(name)))) name[0]=0;
	if (!ptr || !(ptr=get_str(ptr,desc,sizeof(desc)))) desc[0]=0;
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return 0; }

	sprintf(modbasename,"lq_%s",basename);
	base=lookup_item(modbasename);
	if (!base) { log_sys(cn,"°c3Base \"%s\" does not exist.",basename); return 0; }

        strcpy(li->base,basename);
	li->keyID=keyID;

	strcpy(li->name,name);
	strcpy(li->description,desc);

	return 1;
}

void cmd_npc(int cn,char *ptr)
{	
	int base,level,respawn,n;
	char mode,nick[2][40],basename[40],modbasename[80];
	static char *usage="/npc <base:str> <level:int> <mode:chr> <respawn:int> [nick1:str] [nick2:str]";

	if (!(ptr=get_str(ptr,basename,sizeof(basename)))) { log_sys(cn,"°c3Missing base. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&level))) { log_sys(cn,"°c3Missing level. Usage is: %s.",usage); return; }
	if (!(ptr=get_chr(ptr,&mode))) { log_sys(cn,"°c3Missing mode. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&respawn))) { log_sys(cn,"°c3Missing respawn. Usage is: %s.",usage); return; }

	if (!(ptr=get_str(ptr,nick[0],sizeof(nick[0])))) nick[0][0]=0;
	if (!ptr || !(ptr=get_str(ptr,nick[1],sizeof(nick[1])))) nick[1][0]=0;

	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }
	
	//log_sys(cn,"basename=\"%s\", level=%d, mode=%c, respawn=%d, nick1=\"%s\", nick2=\"%s\".",basename,level,mode,respawn,nick[0],nick[1]);

	sprintf(modbasename,"lq_%s",basename);
	base=lookup_char(modbasename);
	if (!base) { log_sys(cn,"°c3Base \"%s\" does not exist.",basename); return; }

	for (n=1; n<MAXLQNPC; n++) {
		if (lq_npc[n].basename[0] && lq_npc[n].x==ch[cn].x && lq_npc[n].y==ch[cn].y) {
			log_sys(cn,"°c3 %d %s %s is already at this position",n,lq_npc[n].nick[0],lq_npc[n].nick[1]); return;
		}
	}

	n=find_free_npc();
	if (!n) { log_sys(cn,"No free NPC slots left."); return; }

        strcpy(lq_npc[n].basename,basename);
	lq_npc[n].level=level;
        lq_npc[n].mode=tolower(mode);
	lq_npc[n].respawn=respawn;
	strcpy(lq_npc[n].nick[0],nick[0]);
	strcpy(lq_npc[n].nick[1],nick[1]);
	lq_npc[n].x=ch[cn].x;
	lq_npc[n].y=ch[cn].y;
	lq_npc[n].dir=ch[cn].dir;

	log_sys(cn,"Added NPC %d",n);
}

void cmd_thrall(int cn,char *ptr)
{	
	int count,n,i,dx,dy;
	char nick[40],name[40];
	static char *usage="/thrall <nick|ID> <count:int> [thrallname:str]";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing nick. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&count))) { log_sys(cn,"°c3Missing count. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,name,sizeof(name)))) name[0]=0;
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (MAXCHARS-used_chars<150) {
		log_sys(cn,"Sorry, the server cannot handle more characters.");
		return;
	}

	if (count>20) {
		log_sys(cn,"Sorry, maximum number of NPCs you can spawn in one call is 20.");
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (!lq_npc[n].basename[0]) { log_sys(cn,"Template not found"); return; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {				
				break;
			}
		}
		if (n==MAXLQNPC) { log_sys(cn,"Template not found"); return; }
	}
	dx2offset(ch[cn].dir,&dx,&dy,NULL);
	for (i=0; i<count; i++) {
		spawn_npc(n,1,ch[cn].x+dx*3,ch[cn].y+dy*3,name);
	}
}

void cmd_killthrall(int cn,char *ptr)
{	
	int count=0,n,next;
	char name[40];
	struct lq_npc_data *dat;
	static char *usage="/killthrall <thrallname:str>";

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }


	for (n=getfirst_char(); n; n=next) {
		next=getnext_char(n);
		
		if (ch[n].driver!=CDR_LQNPC) continue;
		
		if ((dat=set_data(n,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data))) && !strcasecmp(dat->thrallname,name)) {
			remove_destroy_char(n);
			count++;
		}

	}
	log_sys(cn,"Killed %d thralls.",count);
}

void cmd_npcname(int cn,char *ptr)
{	
	int cnt=0,n;
        char nick[40],name[sizeof(ch[0].name)];
	static char *usage="/npcname <npcID|nick> <name:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { strcpy(lq_npc[n].name,name); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				strcpy(lq_npc[n].name,name); cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set name of %d NPCs",cnt);	
}

void cmd_npcgold(int cn,char *ptr)
{	
	int cnt=0,n,gold;
        char nick[40];
	static char *usage="/npcgold <npcID|nick> <gold:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&gold))) { log_sys(cn,"°c3Missing gold. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (gold>2000) {
		log_sys(cn,"°c3Too much gold.");
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { lq_npc[n].carry_gold=gold; cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].carry_gold=gold; cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set gold of %d NPCs",cnt);	
}

void cmd_npcsprite(int cn,char *ptr)
{	
	int cnt=0,n,sprite;
        char nick[40];
	static char *usage="/npcgold <npcID|nick> <sprite:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&sprite))) { log_sys(cn,"°c3Missing gold. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (sprite==313 || sprite==305 || sprite==58) {
		log_sys(cn,"Sorry, Islena is not available for Life Quests.");
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { lq_npc[n].sprite=sprite; cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].sprite=sprite; cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set sprite of %d NPCs",cnt);	
}

void cmd_npcdesc(int cn,char *ptr)
{	
	int cnt=0,n;
        char nick[40],desc[sizeof(ch[0].description)];
	static char *usage="/npcdesc <npcID|nick> <description:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,desc,sizeof(desc)))) { log_sys(cn,"°c3Missing description. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { strcpy(lq_npc[n].description,desc); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				strcpy(lq_npc[n].description,desc); cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set description of %d NPCs",cnt);	
}

void cmd_npcgreet(int cn,char *ptr)
{	
	int cnt=0,n;
        char nick[40],text[256];
	static char *usage="/npcgreet <npcID|nick> <text:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,text,sizeof(text)))) { log_sys(cn,"°c3Missing text. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { strcpy(lq_npc[n].greeting,text); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				strcpy(lq_npc[n].greeting,text); cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set greeting of %d NPCs",cnt);
}

void cmd_npckillmark(int cn,char *ptr)
{	
	int cnt=0,n,mark;
        char nick[40];
	static char *usage="/npckillmark <npcID|nick> <mark:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&mark))) { log_sys(cn,"°c3Missing mark. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (mark<1 || mark>=MAXLQMARK) {
		log_sys(cn,"Mark is out of bounds (1-%d)",MAXLQMARK-1);
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { lq_npc[n].kill_markID=mark; cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].kill_markID=mark; cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set killmark of %d NPCs",cnt);
}

void cmd_npchurtmark(int cn,char *ptr)
{	
	int cnt=0,n,mark;
        char nick[40];
	static char *usage="/npchurtmark <npcID|nick> <mark:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&mark))) { log_sys(cn,"°c3Missing mark. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (mark<1 || mark>=MAXLQMARK) {
		log_sys(cn,"Mark is out of bounds (1-%d)",MAXLQMARK-1);
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { lq_npc[n].hurt_markID=mark; cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].hurt_markID=mark; cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set hurtmark of %d NPCs",cnt);
}

void cmd_npcmodlevel(int cn,char *ptr)
{	
	int cnt=0,n,mod;
        char nick[40];
	static char *usage="/npcmodlevel <npcID|nick|all> <mod:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&mod))) { log_sys(cn,"°c3Missing mod. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			lq_npc[n].level+=mod; cnt++;
			if (lq_npc[n].level<1) {
				lq_npc[n].level=1;
				log_sys(cn,"NPC %d (%s %s %s) set to level 1 to avoid negative level.",n,lq_npc[n].name,lq_npc[n].nick[0],lq_npc[n].nick[1]);
			}
			if (lq_npc[n].level>200) {
				lq_npc[n].level=200;
				log_sys(cn,"NPC %d (%s %s %s) set to level 200 to avoid too high levels.",n,lq_npc[n].name,lq_npc[n].nick[0],lq_npc[n].nick[1]);
			}
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick) || !strcasecmp("all",nick))) {
				lq_npc[n].level+=mod; cnt++;
				if (lq_npc[n].level<1) {
					lq_npc[n].level=1;
					log_sys(cn,"NPC %d (%s %s %s) set to level 1 to avoid negative level.",n,lq_npc[n].name,lq_npc[n].nick[0],lq_npc[n].nick[1]);
				}
				if (lq_npc[n].level>200) {
					lq_npc[n].level=200;
					log_sys(cn,"NPC %d (%s %s %s) set to level 200 to avoid too high levels.",n,lq_npc[n].name,lq_npc[n].nick[0],lq_npc[n].nick[1]);
				}
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Changed level of %d NPCs",cnt);
}

void cmd_npcrespawn(int cn,char *ptr)
{	
	int cnt=0,n,mod;
        char nick[40];
	static char *usage="/npcrespawn <npcID|nick|all> <mod:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&mod))) { log_sys(cn,"°c3Missing respawn time. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			lq_npc[n].respawn=mod; cnt++;
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick) || !strcasecmp("all",nick))) {
				lq_npc[n].respawn=mod; cnt++;				
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Changed respawn time of %d NPCs to %d",cnt,mod);
}

void cmd_npcpos(int cn,char *ptr)
{	
        char nick[40];
	int x,y,n,m;
	static char *usage="/npcpos <npcID|nick> [x:int] [y:int]";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&x))) x=0;
	if (!ptr || !(ptr=get_int(ptr,&y))) y=0;
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (x==0 && y==0) { x=ch[cn].x; y=ch[cn].y; }

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) {
		log_sys(cn,"Position %d,%d is out of bounds.",x,y);
		return;
	}

	n=atoi(nick);
	if (n<1 || n>=MAXLQNPC || !lq_npc[n].basename[0]) {	
		for (n=0,m=1; m<MAXLQNPC; m++) {
			if (lq_npc[m].basename[0] && (!strcasecmp(lq_npc[m].nick[0],nick) || !strcasecmp(lq_npc[m].nick[1],nick))) {
				if (n) {
					log_sys(cn,"°c3Cannot set the same position for multiple NPCs.");
					return;
				}
				n=m;
			}
		}
	}
	if (!n) {
		log_sys(cn,"°c3NPC not found.");
		return;
	}

	for (m=1; m<MAXLQNPC; m++) {
                if (m==n || !lq_npc[m].basename[0]) continue;
		if (lq_npc[m].x==x && lq_npc[m].y==y) {
			log_sys(cn,"°c3 %d %s %s is already at this position",m,lq_npc[m].nick[0],lq_npc[m].nick[1]); return;
		}
	}
	lq_npc[n].x=x;
	lq_npc[n].y=y;
	lq_npc[n].dir=ch[cn].dir;
	log_sys(cn,"Set position to %d,%d.",x,y);
}

void cmd_npcreply(int cn,char *ptr)
{	
	int nr,n,cnt=0;
        char nick[40],trigger[40],reply[256];
	static char *usage="/npcreply <npcID|nick> <nr:int> <trigger:str> <reply:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&nr))) { log_sys(cn,"°c3Missing nr. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,trigger,sizeof(trigger)))) { log_sys(cn,"°c3Missing trigger. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,reply,sizeof(reply)))) { log_sys(cn,"°c3Missing reply. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	nr--;
	if (nr<0 || nr>=ARRAYSIZE(lq_npc[n].trigger)) {
		log_sys(cn,"Nr %d it out of bounds.",nr+1);
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			strcpy(lq_npc[n].trigger[nr],trigger);
			strcpy(lq_npc[n].reply[nr],reply);
			cnt++;
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				strcpy(lq_npc[n].trigger[nr],trigger);
				strcpy(lq_npc[n].reply[nr],reply);
				cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set trigger/reply of %d NPCs",cnt);
}

void cmd_npcwantitem(int cn,char *ptr)
{	
	int n,cnt=0,ID;
        char nick[40];
	static char *usage="/npcwantitem <npcID|nick> <ID:int>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&ID))) { log_sys(cn,"°c3Missing ID. Usage is: %s.",usage); return; }
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			lq_npc[n].want_keyID=ID;
                        cnt++;
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].want_keyID=ID;
				cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set wantitem of %d NPCs",cnt);
}

void show_npc(int cn,int n)
{
	int i;

	log_sys(cn,"Base: %s",lq_npc[n].basename);
	log_sys(cn,"Nicks: %s/%s",lq_npc[n].nick[0],lq_npc[n].nick[1]);
	log_sys(cn,"Level: %d",lq_npc[n].level);
	log_sys(cn,"Mode: %c",lq_npc[n].mode);
	log_sys(cn,"Respawn: %d",lq_npc[n].respawn);

	if (lq_npc[n].name[0]) log_sys(cn,"Name: %s",lq_npc[n].name);
	if (lq_npc[n].description[0]) log_sys(cn,"Desc: %s",lq_npc[n].description);
	if (lq_npc[n].greeting[0]) log_sys(cn,"Greeting: %s",lq_npc[n].greeting);
	for (i=0; i<5; i++)
		if (lq_npc[n].trigger[i][0]) log_sys(cn,"Trigger/Reply %d: %s/%s",i,lq_npc[n].trigger[i],lq_npc[n].reply[i]);
	
	if (lq_npc[n].carry_gold) log_sys(cn,"Gold: %.2fG",lq_npc[n].carry_gold/100.0);
	if (lq_npc[n].carry_item.base[0]) log_sys(cn,"Carry Item: %s ID: %d",lq_npc[n].carry_item.base,lq_npc[n].carry_item.keyID);

	if (lq_npc[n].want_keyID) log_sys(cn,"Wants ID: %d",lq_npc[n].want_keyID);
	if (lq_npc[n].reward_item.base[0]) log_sys(cn,"Reward Item: %s ID: %d",lq_npc[n].reward_item.base,lq_npc[n].reward_item.keyID);
	
	if (lq_npc[n].hurt_markID) log_sys(cn,"Hurtmark ID: %s (%d), %d exp",lq_data.reward_desc[lq_npc[n].hurt_markID],lq_npc[n].hurt_markID,lq_data.reward[lq_npc[n].hurt_markID]);
	if (lq_npc[n].kill_markID) log_sys(cn,"Killmark ID: %s (%d), %d exp",lq_data.reward_desc[lq_npc[n].kill_markID],lq_npc[n].kill_markID,lq_data.reward[lq_npc[n].kill_markID]);
	
}

void cmd_npcshow(int cn,char *ptr)
{	
	int n,cnt=0;
        char nick[40];
	static char *usage="/npcshow <npcID|nick>";	

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			show_npc(cn,n);
                        cnt++;
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				show_npc(cn,n);
				cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Showed %d NPCs",cnt);
}

void cmd_npcitem(int cn,char *ptr)
{	
	int n,cnt=0;
        char nick[40];
	static char *usage="/npcitem <npcID|nick> <base:str> [keyID:int] [name:str] [description:str]";
	struct lq_item li;

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!get_lq_item(cn,ptr,&li,usage)) return;

        n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			lq_npc[n].carry_item=li;
                        cnt++;
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].carry_item=li;
				cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set item of %d NPCs",cnt);
}

void cmd_npcrewarditem(int cn,char *ptr)
{	
	int n,cnt=0;
        char nick[40];
	static char *usage="/npcrewarditem <npcID|nick> <base:str> [keyID:int] [name:str] [description:str]";
	struct lq_item li;

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!get_lq_item(cn,ptr,&li,usage)) return;

        n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) {
			lq_npc[n].reward_item=li;
                        cnt++;
		}
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				lq_npc[n].reward_item=li;
				cnt++;
			}
		}
	}

	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set item of %d NPCs",cnt);
}

void cmd_npclist(int cn,char *ptr)
{
	int n,cnt=0,start;
	char nick[40];
	static char *usage="/npclist <nick|start>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) nick[0]=0;
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	start=atoi(nick);
	if (start) nick[0]=0;

	for (n=max(1,start); n<MAXLQNPC; n++) {
		if (!lq_npc[n].basename[0]) continue;		
		if (nick[0] && strcasecmp(lq_npc[n].nick[0],nick) && strcasecmp(lq_npc[n].nick[1],nick)) continue;
                log_sys(cn,"NPC %3d: base %s, level %d, nicks: %s %s, pos: %d,%d",
			n,
			lq_npc[n].basename,
			lq_npc[n].level,
			lq_npc[n].nick[0],
			lq_npc[n].nick[1],
			lq_npc[n].x,
			lq_npc[n].y);
		cnt++;
		if (cnt>99) break;		
	}
	log_sys(cn,"%d of %d NPCs (%d%%)",cnt,MAXLQNPC-1,100*cnt/(MAXLQNPC-1));
}

void cmd_npcdel(int cn,char *ptr)
{
	int n,cnt=0;
	char nick[40];
	static char *usage="/npcdel <npcID|nick>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { remove_npc(n); bzero(&lq_npc[n],sizeof(lq_npc[n])); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				remove_npc(n); bzero(&lq_npc[n],sizeof(lq_npc[n])); cnt++;
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Deleted %d NPCs.",cnt);
}

void cmd_questend(int cn,char *ptr)
{
	int co,n,sum,cnt=0,val;
	struct lq_plr_data *pdat;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!(ch[co].flags&CF_PLAYER)) continue;
		pdat=set_data(co,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data));
		if (!pdat) continue;		
                for (sum=0,n=1; n<MAXLQMARK; n++) {
			if (pdat->mark[n]) {
				sum+=lq_data.reward[n];	
				pdat->mark[n]=0;
			}
		}
		val=level_value(ch[co].level)/(ch[co].level/10+1);
                //xlog("sum=%d, val=%d, level=%d, res=%d",sum,val,ch[co].level,val*min(100,sum)/100);
		val=val/100.0*min(100,sum);
		//xlog("val=%d (%d)",val,level_value(ch[co].level)/(ch[co].level/10+1));
		//sum=min(sum,level_value(ch[co].level)/(ch[co].level/10+1));
		if (sum) {
			give_exp(co,val);
			log_sys(co,"You have been rewarded for your participation in this quest.");
			cnt++;
		}
	}
	log_sys(cn,"Rewarded %d players.",cnt);
}

void cmd_questsave(int cn,char *ptr)
{
	int n,handle;
	char name[40],password[40],file[80];
	char xpassword[40];
	static char *usage="/questsave <name> [password]";

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,password,sizeof(password)))) password[0]=0;
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	for (n=0; name[n]; n++) if (!isalpha(name[n])) { log_sys(cn,"°c3Name contains illegal character %c.",name[n]); return; }

	sprintf(file,"quest/%s.qst",name);
	handle=open(file,O_RDWR);
	if (handle!=-1) {
		read(handle,xpassword,40);
		if (xpassword[0] && strcmp(xpassword,password)) {
			close(handle);
			log_sys(cn,"°c3Cannot overwrite file with differing password.");
			return;
		}
		lseek(handle,0,SEEK_SET);
	} else handle=open(file,O_RDWR|O_CREAT,0600);
	if (handle==-1) {
		log_sys(cn,"°c3Cannot create file.");
		return;
	}
	write(handle,password,40);
	write(handle,&lq_data,sizeof(lq_data));
	write(handle,lq_npc,sizeof(lq_npc));
	write(handle,lq_door,sizeof(lq_door));
	close(handle);

	log_sys(cn,"Saved as %s, password \"%s\".",name,password);
}

void cmd_questdel(int cn,char *ptr)
{
	int n,handle;
	char name[40],password[40],file[80];
	char xpassword[40];
	static char *usage="/questdel <name> [password]";

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,password,sizeof(password)))) password[0]=0;
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	for (n=0; name[n]; n++) if (!isalpha(name[n])) { log_sys(cn,"°c3Name contains illegal character %c.",name[n]); return; }

	sprintf(file,"quest/%s.qst",name);
	handle=open(file,O_RDWR);
	
	if (handle==-1) { log_sys(cn,"°c3File not found."); return; }
	
	read(handle,xpassword,40);
        close(handle);

	if (xpassword[0] && strcmp(xpassword,password)) {
		log_sys(cn,"°c3Cannot delete file with differing password.");
		return;
	}
	if (unlink(file)) log_sys(cn,"°c3Delete failed at system level.");
	else log_sys(cn,"Deleted quest %s.",name);
}

void cmd_questload(int cn,char *ptr)
{
	int n,handle,len;
	char name[40],password[40],file[80];
	char xpassword[40];
	static char *usage="/questload <name> [password]";

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,password,sizeof(password)))) password[0]=0;
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	for (n=0; name[n]; n++) if (!isalpha(name[n])) { log_sys(cn,"°c3Name contains illegal character %c.",name[n]); return; }

	sprintf(file,"quest/%s.qst",name);
	handle=open(file,O_RDWR);
	
	if (handle==-1) { log_sys(cn,"°c3File not found."); return; }

	len=lseek(handle,0,SEEK_END);
	if (len!=40+sizeof(lq_data)+sizeof(lq_npc)+sizeof(lq_door)) {
		close(handle);
		log_sys(cn,"°c3The file appears to be a different version. Cannot load.");
		return;
	}
	lseek(handle,0,SEEK_SET);
	
	read(handle,xpassword,40);

	if (xpassword[0] && strcmp(xpassword,password)) {
		close(handle);
		log_sys(cn,"°c3Cannot load file with differing password.");
		return;
	}

	read(handle,&lq_data,sizeof(lq_data));
        read(handle,lq_npc,sizeof(lq_npc));
	read(handle,lq_door,sizeof(lq_door));
	close(handle);

	init_done=0;	//for (n=0; n<MAXLQDOOR; n++) update_lqdoor(n);
	lq_data.open=0;

	log_sys(cn,"Loaded quest %s.",name);
}

int cost2skill(int v,int cost,int seyan)
{
	int sum,n;

	for (sum=0,n=skill[v].start+1; n<200; n++) {
		sum+=raise_cost(v,n,seyan);
		if (sum>cost) return n-1;
	}
	return n;
}

void lq_raise(int cn,int level)
{
	int spend,sum,cost;
	double weight;
	int n;

	spend=level2exp(level+2)-1;

	for (sum=n=0; n<V_MAX; n++) {
		switch(n) {
			case V_PROFESSION:	
			case V_COLD:
			case V_DEMON:
			case V_SPEED:
			case V_LIGHT:
			case V_WEAPON:
			case V_ARMOR:
				continue;
		}
		if (!ch[cn].value[1][n]) continue;
                sum+=ch[cn].value[1][n];
	}
	for (n=0; n<V_MAX; n++) {
		switch(n) {
			case V_PROFESSION:	
			case V_COLD:
			case V_DEMON:
			case V_SPEED:
			case V_LIGHT:
			case V_WEAPON:
			case V_ARMOR:
				weight=0;
				continue;
		}
		if (!ch[cn].value[1][n]) continue;

                cost=(int)((double)spend/(double)sum*(double)ch[cn].value[1][n]);
		ch[cn].value[1][n]=cost2skill(n,cost,(ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE));
		//charlog(cn,"set %s to %d (%d exp)",skill[n].name,ch[cn].value[1][n],cost);
	}
	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);
	//charlog(cn,"expected cost: %d exp, actual cost: %d exp",spend,ch[cn].exp);
}

void lq_equipment(int cn)
{
	int base,flag,in;
	char buf[80];

	flag=0;
	if ((base=ch[cn].value[1][V_DAGGER])) {
		base=min(10,base/10+1);
		sprintf(buf,"dagger%dq1",base);
		flag=1;
	}
	if ((base=ch[cn].value[1][V_STAFF])) {
		base=min(10,base/10+1);
		sprintf(buf,"staff%dq1",base);
		flag=1;
	}
	if ((base=ch[cn].value[1][V_SWORD])) {
		base=min(10,base/10+1);
		sprintf(buf,"sword%dq1",base);
		flag=1;
	}
	if ((base=ch[cn].value[1][V_TWOHAND])) {
		base=min(10,base/10+1);
		sprintf(buf,"twohand%dq1",base);
		flag=1;
	}
	if (flag) {
		in=ch[cn].item[WN_RHAND]=create_item(buf); it[in].carried=cn;
	}
	if ((base=ch[cn].value[1][V_ARMORSKILL])) {
		base=min(10,base/10+1);
		sprintf(buf,"helmet%dq1",base); in=ch[cn].item[WN_HEAD]=create_item(buf); it[in].carried=cn;
		sprintf(buf,"armor%dq1",base); in=ch[cn].item[WN_BODY]=create_item(buf); it[in].carried=cn;
		sprintf(buf,"leggings%dq1",base); in=ch[cn].item[WN_LEGS]=create_item(buf); it[in].carried=cn;
		sprintf(buf,"sleeves%dq1",base); in=ch[cn].item[WN_ARMS]=create_item(buf); it[in].carried=cn;
	}	
}

void lq_statboost(int cn)
{
	int in,n,flag=0;

	in=create_item("lqx_spell");

	// weapon skill
	if (ch[cn].value[1][V_DAGGER]) it[in].mod_index[0]=V_DAGGER;
	else if (ch[cn].value[1][V_STAFF]) it[in].mod_index[0]=V_STAFF;
	else if (ch[cn].value[1][V_SWORD]) it[in].mod_index[0]=V_SWORD;
	else if (ch[cn].value[1][V_TWOHAND]) it[in].mod_index[0]=V_TWOHAND;
	else it[in].mod_index[0]=V_HAND;

	it[in].mod_value[0]=ch[cn].level/5+1;

	// warrior stuff
	if (ch[cn].value[1][V_ATTACK]) {	// warrior skills
		it[in].mod_index[1]=V_ATTACK; it[in].mod_value[1]=ch[cn].level/5+1;
		it[in].mod_index[2]=V_PARRY; it[in].mod_value[2]=ch[cn].level/5+1;
		it[in].mod_index[3]=V_TACTICS; it[in].mod_value[3]=ch[cn].level/5+1;
	}
	if (ch[cn].value[1][V_WARCRY]) {
		it[in].mod_index[4]=V_WARCRY; it[in].mod_value[4]=ch[cn].level/5+1;
	}

	for (n=12; n<30; n++) {
		if (!ch[cn].item[n]) {
			ch[cn].item[n]=in; it[in].carried=cn;
			break;
		}
	}
	if (n==30) {
		destroy_item(in);
	}

	in=create_item("lqx_spell");

	// mage stuff
	if (ch[cn].value[1][V_BLESS]) {
		it[in].mod_index[0]=V_BLESS; it[in].mod_value[0]=ch[cn].level/5+1; flag=1;
	}
	if (ch[cn].value[1][V_LIGHT]) {
		it[in].mod_index[1]=V_LIGHT; it[in].mod_value[1]=ch[cn].level/5+1; flag=1;
	}
	if (ch[cn].value[1][V_FIREBALL]) {
		it[in].mod_index[2]=V_FIREBALL; it[in].mod_value[2]=ch[cn].level/5+1; flag=1;
	}
        if (ch[cn].value[1][V_MAGICSHIELD]) {
		it[in].mod_index[3]=V_MAGICSHIELD; it[in].mod_value[3]=ch[cn].level/5+1; flag=1;
	}
	if (ch[cn].value[1][V_FREEZE]) {
		it[in].mod_index[4]=V_FREEZE; it[in].mod_value[4]=ch[cn].level/5+1; flag=1;
	}
	if (flag) {
		for (n=12; n<30; n++) {
			if (!ch[cn].item[n]) {
				ch[cn].item[n]=in; it[in].carried=cn;
				break;
			}
		}
		if (n==30) {
			destroy_item(in);
		}
	} else {
		destroy_item(in);
	}

	in=create_item("lqx_spell");

	// misc
        it[in].mod_index[0]=V_IMMUNITY; it[in].mod_value[0]=ch[cn].level/5+1;
	it[in].mod_index[1]=V_WIS; it[in].mod_value[1]=ch[cn].level/3+1;
	it[in].mod_index[2]=V_INT; it[in].mod_value[2]=ch[cn].level/3+1;
	it[in].mod_index[3]=V_AGI; it[in].mod_value[3]=ch[cn].level/3+1;
	it[in].mod_index[4]=V_STR; it[in].mod_value[4]=ch[cn].level/3+1;
	
	for (n=12; n<30; n++) {
		if (!ch[cn].item[n]) {
			ch[cn].item[n]=in; it[in].carried=cn;
			break;
		}
	}
	if (n==30) {
		destroy_item(in);
	}
}

int spawn_npc(int n,int isthrall,int tx,int ty,char *thrallname)
{
	int cn,i,in;
	char modbasename[256];
	struct lq_npc_data *dat;

	if (!lq_npc[n].basename[0]) return 0;
	
	// check if npc is already there
	if (!isthrall) {
		cn=lq_npc[n].cn;
		if (cn && ch[cn].flags && ch[cn].serial==lq_npc[n].cserial) return 0;
	
		if (lq_respawn[n]>=ticker) return 0;
	}

	sprintf(modbasename,"lq_%s",lq_npc[n].basename);
	cn=create_char(modbasename,0);
	if (!cn) return 0;

	ch[cn].gold=lq_npc[n].carry_gold;
	if (lq_npc[n].sprite) ch[cn].sprite=lq_npc[n].sprite;

	if (lq_npc[n].name[0]) strcpy(ch[cn].name,lq_npc[n].name);
	if (lq_npc[n].description[0]) strcpy(ch[cn].description,lq_npc[n].description);

	ch[cn].driver=CDR_LQNPC;

	if (lq_npc[n].mode=='n') ch[cn].flags|=CF_IMMORTAL|CF_NOATTACK;

	lq_raise(cn,lq_npc[n].level);
	lq_statboost(cn);
	lq_equipment(cn);
	
	if (!isthrall) {
		ch[cn].tmpx=lq_npc[n].x;
		ch[cn].tmpy=lq_npc[n].y;
	} else {
		ch[cn].tmpx=tx+2-RANDOM(4);
		ch[cn].tmpy=ty+2-RANDOM(4);
	}

	if (!isthrall) {
		if (!drop_char(cn,lq_npc[n].x,lq_npc[n].y,0)) {
			destroy_char(cn);
			return 0;
		}
	} else {
		if (!drop_char(cn,ch[cn].tmpx,ch[cn].tmpy,0)) {
			destroy_char(cn);
			return 0;
		}
	}
	
	update_char(cn);
		
	ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
	ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
	
	ch[cn].dir=DX_RIGHTDOWN;

	dat=set_data(cn,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data));
	if (!dat) {
		remove_destroy_char(cn);
		return 0;	// oops...
	}

	if (lq_npc[n].carry_item.base[0] && (in=create_lq_item(&lq_npc[n].carry_item)) && !give_char_item(cn,in)) {
		destroy_item(in);
	}

	if (!isthrall) {
		lq_npc[n].cn=cn;
		lq_npc[n].cserial=ch[cn].serial;
	
		dat->n=n;
	} else {
		dat->n=0;
		strcpy(dat->thrallname,thrallname);
	}

	dat->mode=lq_npc[n].mode;
	if (!isthrall) {
		strcpy(dat->greeting,lq_npc[n].greeting);
		for (i=0; i<5; i++) {
			strcpy(dat->trigger[i],lq_npc[n].trigger[i]);
			strcpy(dat->reply[i],lq_npc[n].reply[i]);
		}
	}
	dat->hurt_markID=lq_npc[n].hurt_markID;
	dat->kill_markID=lq_npc[n].kill_markID;
	dat->reward_markID=lq_npc[n].reward_markID;
	dat->want_keyID=lq_npc[n].want_keyID;
	dat->reward_item=lq_npc[n].reward_item;
	dat->dir=lq_npc[n].dir;

        return 1;
}

int remove_npc(int n)
{
	int cn,flag=0;

	if (!lq_npc[n].basename[0]) return 0;
	cn=lq_npc[n].cn;

	if (lq_respawn[n]>ticker) flag=1;
        lq_respawn[n]=0;
	
	if (!cn || !ch[cn].flags || ch[cn].serial!=lq_npc[n].cserial) return flag;

        remove_destroy_char(cn);
	
	lq_npc[n].cn=lq_npc[n].cserial=0;

	return 1;
}

void cmd_nspawn(int cn,char *ptr)
{
	int n,cnt=0;
	char nick[40];
	static char *usage="/nspawn <npcID|nick|all>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { if (spawn_npc(n,0,0,0,NULL)) cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(nick,"all") || !strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				if (spawn_npc(n,0,0,0,NULL)) cnt++;
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Spawned %d NPCs.",cnt);
}

void cmd_nremove(int cn,char *ptr)
{
	int n,cnt=0;
	char nick[40];
	static char *usage="/nremove <npcID|nick|all>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0]) { if (remove_npc(n)) cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(nick,"all") || !strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick))) {
				if (remove_npc(n)) cnt++;
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Removed %d NPCs.",cnt);
}

int get_lq_char(int n)
{
	int cn;

	cn=lq_npc[n].cn;
	if (!ch[cn].flags) return 0;
	if (ch[cn].serial!=lq_npc[n].cserial) return 0;
	
	return cn;
}

void cmd_nsay(int cn,char *ptr)
{
	int n,cnt=0,co;
	char nick[40],text[256];
	static char *usage="/nsay <npcID|nick> <text:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,text,sizeof(text)))) { log_sys(cn,"°c3Missing text. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0] && (co=get_lq_char(n))) { say(co,"%s",text); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick)) && (co=get_lq_char(n))) {
				say(co,"%s",text);
				cnt++;
				if (cnt>10) {
					log_sys(cn,"°c3Cancelled, too many NPCs.");
					break;
				}
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
}

void cmd_nimmortal(int cn,char *ptr)
{
	int n,cnt=0,co,onoff;
	char nick[40];
	static char *usage="/nimmortal <npcID|nick> <0|1>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&onoff))) { log_sys(cn,"°c3Missing 0|1. Usage is: %s.",usage); return; }
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0] && (co=get_lq_char(n))) { if (onoff) ch[co].flags|=(CF_IMMORTAL|CF_NOATTACK); else ch[co].flags&=~(CF_IMMORTAL|CF_NOATTACK); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick)) && (co=get_lq_char(n))) {
				if (onoff) ch[co].flags|=(CF_IMMORTAL|CF_NOATTACK); else ch[co].flags&=~(CF_IMMORTAL|CF_NOATTACK);
				cnt++;				
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"Set immortal to %s on %d NPCs",onoff ? "ON" : "OFF",cnt);	
}

void cmd_nemote(int cn,char *ptr)
{
	int n,cnt=0,co;
	char nick[40],text[256];
	static char *usage="/nemote <npcID|nick> <text:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,text,sizeof(text)))) { log_sys(cn,"°c3Missing text. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0] && (co=get_lq_char(n))) { emote(co,"%s",text); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick)) && (co=get_lq_char(n))) {
				emote(co,"%s",text);
				cnt++;
				if (cnt>10) {
					log_sys(cn,"°c3Cancelled, too many NPCs.");
					break;
				}
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
}

void cmd_nattack(int cn,char *ptr)
{
	int n,cnt=0,co,cc;
	char nick[40],plr[40];
	static char *usage="/nattack <npcID|nick> <player:str>";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing npcID. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,plr,sizeof(plr)))) { log_sys(cn,"°c3Missing text. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(plr,ch[co].name)) break;		
	}
	if (!co) {
		log_sys(cn,"°c3Player %s not found.",plr);
		return;
	}

	n=atoi(nick);
	if (n>0 && n<MAXLQNPC) {
		if (lq_npc[n].basename[0] && (cc=get_lq_char(n))) { fight_driver_add_enemy(cc,co,0,1); cnt++; }
	} else {
		for (n=1; n<MAXLQNPC; n++) {
			if (lq_npc[n].basename[0] && (!strcasecmp(lq_npc[n].nick[0],nick) || !strcasecmp(lq_npc[n].nick[1],nick)) && (cc=get_lq_char(n))) {
				fight_driver_add_enemy(cc,co,1,1);
				cnt++;				
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3NPC not found.");
	else log_sys(cn,"%d NPCs attacking.",cnt);
}

void cmd_exit(int cn,char *ptr)
{
	struct lq_plr_data *pdat;
	struct lq_npc_data *dat;
	int co=0;

	if ((pdat=set_data(cn,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data)))) { co=pdat->usurp; pdat->usurp=0; }
	if (co && (dat=set_data(co,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data)))) dat->usurp=0;

	if (ptr) log_sys(cn,"Done.");
}


void cmd_usurp(int cn,char *ptr)
{
	int co;
	char name[40];
	static char *usage="/usurp <name:str>";
	int x,y,xs,xe,ys,ye,tmp;
	int bdist=999,bco=0;
	struct lq_plr_data *pdat;
	struct lq_npc_data *dat;

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        xs=max(0,ch[cn].x-12);
	xe=min(MAXMAP-1,ch[cn].x+12);
	ys=max(0,ch[cn].y-12);
	ye=min(MAXMAP-1,ch[cn].y+12);

        for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (co=getfirst_char_sector(x,y); co; co=ch[co].sec_next) {
				if (ch[co].driver==CDR_LQNPC && strcasestr(ch[co].name,name) && (tmp=char_dist(cn,co))<bdist) {
					bdist=tmp;
					bco=co;
                                }					
			}
		}
	}
	if (bco) {
		cmd_exit(cn,NULL);
		if ((pdat=set_data(cn,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data)))) pdat->usurp=bco;
		if ((dat=set_data(bco,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data)))) {
			dat->usurp=cn;
			dat->udx=ch[cn].x-ch[bco].x;
			dat->udy=ch[cn].y-ch[bco].y;
		}
		log_sys(cn,"Done.");
	} else log_sys(cn,"NPC not found.");
}

void cmd_follow(int cn,char *ptr)
{
	int co;
	char name[40];
	static char *usage="/follow <name:str>";
	int x,y,xs,xe,ys,ye,cnt=0;
	struct lq_npc_data *dat;

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        xs=max(0,ch[cn].x-12);
	xe=min(MAXMAP-1,ch[cn].x+12);
	ys=max(0,ch[cn].y-12);
	ye=min(MAXMAP-1,ch[cn].y+12);

        for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (co=getfirst_char_sector(x,y); co; co=ch[co].sec_next) {
				if (ch[co].driver==CDR_LQNPC && strcasestr(ch[co].name,name)) {
					if ((dat=set_data(co,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data)))) {
						dat->follow=cn;
						cnt++;
					}
                                }					
			}
		}
	}
	log_sys(cn,"Set %d NPCs to follow.",cnt);
}

void cmd_stop(int cn,char *ptr)
{
	int co;
	char name[40];
	static char *usage="/stop <name:str>";
	int x,y,xs,xe,ys,ye,cnt=0;
	struct lq_npc_data *dat;

	if (!(ptr=get_str(ptr,name,sizeof(name)))) { log_sys(cn,"°c3Missing name. Usage is: %s.",usage); return; }
        if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

        xs=max(0,ch[cn].x-12);
	xe=min(MAXMAP-1,ch[cn].x+12);
	ys=max(0,ch[cn].y-12);
	ye=min(MAXMAP-1,ch[cn].y+12);

        for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (co=getfirst_char_sector(x,y); co; co=ch[co].sec_next) {
				if (ch[co].driver==CDR_LQNPC && strcasestr(ch[co].name,name)) {
					if ((dat=set_data(co,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data)))) {
						dat->follow=0;
						cnt++;
					}
                                }					
			}
		}
	}
	log_sys(cn,"Set %d NPCs to stop.",cnt);
}

void cmd_questreset(int cn,char *ptr)
{
	int n;

	for (n=1; n<MAXCHARS; n++) {
		if (ch[n].flags && ch[n].driver==CDR_LQNPC) {
			remove_destroy_char(n);
		} else if (ch[n].flags&CF_PLAYER) {
			if (!teleport_char_driver(n,240,240) &&
			    !teleport_char_driver(n,235,240) &&
			    !teleport_char_driver(n,240,235) &&
			    !teleport_char_driver(n,235,235) &&
			    !teleport_char_driver(n,245,240) &&
			    !teleport_char_driver(n,240,245) &&
			    !teleport_char_driver(n,245,245)) log_sys(cn,"°c3Could not remove all players, please try again soon.");
		}
	}
	for (n=1; n<MAXITEM; n++) {
		if (!it[n].x) continue;
		
		if (it[n].flags&IF_PLAYERBODY) {
			remove_item_map(n);
			if (!drop_item(n,240,240) &&
			    !drop_item(n,235,240) &&
			    !drop_item(n,240,235) &&
			    !drop_item(n,235,235) &&
			    !drop_item(n,245,240) &&
			    !drop_item(n,240,245) &&
			    !drop_item(n,245,245))
				log_sys(cn,"°c3Could not remove all player bodies, please try again soon.");
			else set_expire(n,PLAYER_BODY_DECAY_TIME);
		} else if (it[n].flags&IF_TAKE) {		
                        remove_item_map(n);
			destroy_item(n);
		}
	}

	// reset data
        bzero(lq_npc,sizeof(lq_npc));
	bzero(lq_respawn,sizeof(lq_respawn));
	bzero(lq_door,sizeof(lq_door)); init_done=0;
	bzero(&lq_data,sizeof(lq_data));
	
	log_sys(cn,"Done.");
}

void cmd_wimp(int cn)
{
	struct misc_ppd *ppd;
	if (!teleport_char_driver(cn,240,240) &&
	    !teleport_char_driver(cn,235,240) &&
	    !teleport_char_driver(cn,240,235) &&
	    !teleport_char_driver(cn,235,235) &&
	    !teleport_char_driver(cn,245,240) &&
	    !teleport_char_driver(cn,240,245))
	    teleport_char_driver(cn,245,245);
	log_char(cn,LOG_SYSTEM,0,"You wimped out.");
        if ((ppd=set_data(cn,DRD_MISC_PPD,sizeof(struct misc_ppd)))) ppd->last_lq_death=realtime;
}

void cmd_questentrance(int cn,char *ptr)
{
	lq_data.entrance_x=ch[cn].x;
	lq_data.entrance_y=ch[cn].y;

	log_sys(cn,"Set quest entrance.");
}

void cmd_queststart(int cn,char *ptr)
{
	if (!lq_data.min_level || !lq_data.max_level) {
		log_sys(cn,"You have to set min/max levels first (/questlevel)");
		return;
	}
	if (!lq_data.entrance_x || !lq_data.entrance_y) {
		log_sys(cn,"You have to set entrance position first (/questentrance)");
		return;
	}
        lq_data.open=1;

	log_sys(cn,"Quest starts...");
}

void cmd_questreward(int cn,char *ptr)
{
	int nr,amount;
	char desc[sizeof(lq_data.reward_desc[0])];
        static char *usage="/questreward <nr:int> <amount:int> <desc:str>";

	if (!(ptr=get_int(ptr,&nr))) { log_sys(cn,"°c3Missing nr. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&amount))) { log_sys(cn,"°c3Missing amount. Usage is: %s.",usage); return; }
	if (!(ptr=get_str(ptr,desc,sizeof(desc)))) { log_sys(cn,"°c3Missing description. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (nr<1 || nr>=MAXLQMARK) {
		log_sys(cn,"°c3Nr is out of bounds (1-%d)",MAXLQMARK-1);
		return;
	}
	if (amount<1 || amount>100) {
		log_sys(cn,"°c3Amount is out of bounds. It must be in the range 1..100. (Percentage of maximum allowed reward).");
		return;
	}
	lq_data.reward[nr]=amount;
	strcpy(lq_data.reward_desc[nr],desc);
	log_sys(cn,"Set reward for mark %s (%d) to %d exp.",desc,nr,amount);
}

void cmd_questlevel(int cn,char *ptr)
{
	int mini,maxi;
        static char *usage="/questlevel <min:int> <max:int>";

	if (!(ptr=get_int(ptr,&mini))) { log_sys(cn,"°c3Missing mini. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&maxi))) { log_sys(cn,"°c3Missing maxi. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	if (mini<1 || mini>200) {
		log_sys(cn,"°c3Min Level is out of bounds (1 to 200).");
		return;
	}
	if (maxi<1 || maxi>200) {
		log_sys(cn,"°c3Max Level is out of bounds (1 to 200).");
		return;
	}

	if (mini>maxi) {
		log_sys(cn,"°c3Min Level cannot be greater than Max Level.");
		return;
	}
	
	lq_data.min_level=mini;
	lq_data.max_level=maxi;

	log_sys(cn,"Set min level to %d and max level to %d.",mini,maxi);
}

void cmd_questshow(int cn,char *ptr)
{
	int n;

	log_sys(cn,"Min level: %d",lq_data.min_level);
	log_sys(cn,"Max level: %d",lq_data.max_level);

	for (n=1; n<MAXLQMARK; n++) {
		if (lq_data.reward[n])
			log_sys(cn,"Reward for mark %s (%d) is %d exp.",lq_data.reward_desc[n],n,lq_data.reward[n]);
	}	
}


void cmd_doorlist(int cn,char *ptr)
{
	int n;

	for (n=0; n<MAXLQDOOR; n++) {
		if (lq_door[n].nick[0]) {
			log_sys(cn,"Door %d, Nick: %s, Pos: %d,%d, Key: %d.",n,lq_door[n].nick,it[lq_door[n].in].x,it[lq_door[n].in].y,lq_door[n].keyID);
		}
	}
}

void update_lqdoor(int nr)
{
	int in;

	if (!(in=lq_door[nr].in)) return;
	
	*(unsigned int*)(it[in].drdata+1)=MAKE_ITEMID(DEV_ID_LQ,lq_door[nr].keyID);
}

void cmd_doorlock(int cn,char *ptr)
{
	int n,cnt=0,keyID;
	char nick[40];
	static char *usage="/doorlock <doornick> <keyID:int> (keyID=0 for unlocked)";

	if (!(ptr=get_str(ptr,nick,sizeof(nick)))) { log_sys(cn,"°c3Missing doornick. Usage is: %s.",usage); return; }
	if (!(ptr=get_int(ptr,&keyID))) { log_sys(cn,"°c3Missing keyID. Usage is: %s.",usage); return; }
	if (ptr && check_anything(ptr)) { log_sys(cn,"°c3Trailing garbage. Usage is: %s.",usage); return; }

	n=atoi(nick);
	if (n>0 && n<MAXLQDOOR) {
		if (lq_door[n].nick[0]) { lq_door[n].keyID=keyID; update_lqdoor(n); cnt++; }
	} else {
		for (n=1; n<MAXLQDOOR; n++) {
			if (lq_door[n].nick[0] && !strcasecmp(lq_door[n].nick,nick)) {
				lq_door[n].keyID=keyID; update_lqdoor(n);
				cnt++;				
			}
		}
	}
	if (!cnt) log_sys(cn,"°c3Door not found.");
	else log_sys(cn,"Set key for %d doors.",cnt);
}


int special_driver(int nr,int cn,char *ptr)
{
	struct lq_plr_data *pdat;
	struct lq_npc_data *dat;
	int len,n,co;

	if (nr!=CDR_LQPARSER) return 0;

        if (*ptr=='#' || *ptr=='/') {
		ptr++;

		if (cmdcmp(ptr,"wimp",4)) { cmd_wimp(cn); return 2; }

		if (ch[cn].flags&(CF_GOD|CF_LQMASTER)) {
			if (!(pdat=set_data(cn,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data)))) return 0;

			if ((len=cmdcmp(ptr,"npc",3))) { cmd_npc(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"thrall",3))) { cmd_thrall(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"killthrall",3))) { cmd_killthrall(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"usurp",3))) { cmd_usurp(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"follow",3))) { cmd_follow(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"stop",3))) { cmd_stop(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"exit",3))) { cmd_exit(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcname",5))) { cmd_npcname(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcgold",5))) { cmd_npcgold(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcsprite",5))) { cmd_npcsprite(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcpos",5))) { cmd_npcpos(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcdescription",5))) { cmd_npcdesc(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcgreeting",5))) { cmd_npcgreet(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcreply",5))) { cmd_npcreply(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npclist",5))) { cmd_npclist(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcdelete",5))) { cmd_npcdel(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcwantitem",5))) { cmd_npcwantitem(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcitem",5))) { cmd_npcitem(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcshow",5))) { cmd_npcshow(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npckillmark",5))) { cmd_npckillmark(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npchurtmark",5))) { cmd_npchurtmark(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcrewarditem",5))) { cmd_npcrewarditem(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcmodlevel",5))) { cmd_npcmodlevel(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"npcrespawn",5))) { cmd_npcrespawn(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"doorlist",6))) { cmd_doorlist(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"doorlock",6))) { cmd_doorlock(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questsave",8))) { cmd_questsave(cn,ptr+len); return 2; }	
			if ((len=cmdcmp(ptr,"questdelete",8))) { cmd_questdel(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questend",8))) { cmd_questend(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questload",8))) { cmd_questload(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questshow",8))) { cmd_questshow(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questreward",8))) { cmd_questreward(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questlevel",8))) { cmd_questlevel(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questreset",10))) { cmd_questreset(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"questentrance",8))) { cmd_questentrance(cn,ptr+len); return 2; }	
			if ((len=cmdcmp(ptr,"queststart",8))) { cmd_queststart(cn,ptr+len); return 2; }	
			if ((len=cmdcmp(ptr,"nspawn",5))) { cmd_nspawn(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"nremove",5))) { cmd_nremove(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"nsay",4))) { cmd_nsay(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"nimmortal",4))) { cmd_nimmortal(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"nemote",4))) { cmd_nemote(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"nattack",4))) { cmd_nattack(cn,ptr+len); return 2; }
			if ((len=cmdcmp(ptr,"xinfo",2))) {
				for (n=1; n<MAXLQMARK; n++)
					if (pdat->mark[n]) log_sys(cn,"I have mark %d",n);
				
				return 2;
			}
			if ((co=pdat->usurp) && ch[co].flags && ch[co].driver==CDR_LQNPC &&
			    (dat=set_data(co,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data))) && dat->usurp==cn) {
				if ((len=cmdcmp(ptr,"me",2)) || (len=cmdcmp(ptr,"emote",2))) {
					emote(co,"%s",ptr+len);
					return 2;
				}
				if ((len=cmdcmp(ptr,"c9",2)) || (len=cmdcmp(ptr,"mirror",2))) {
					char buf[512];
					
					ptr+=len;
					while (isspace(*ptr)) ptr++;
					
					sprintf(buf,"%010u:%02u:%02u:°c%d%s: %s says: \"%s\"",0,areaID,areaM,11,"LQ",ch[co].name,ptr);
					server_chat(9,buf);
	
					return 2;
				}			
			}
		}
		return 1;
	}
	
	if (ch[cn].flags&(CF_GOD|CF_LQMASTER)) {
		if (!(pdat=set_data(cn,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data)))) return 0;

		if ((co=pdat->usurp) && ch[co].flags && ch[co].driver==CDR_LQNPC &&
		(dat=set_data(co,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data))) && dat->usurp==cn) {
			say(co,"%s",ptr);
			return 2;
		}
	}

	return 1;
}

void lqnpc(int cn,int ret,int lastact)
{
        struct lq_npc_data *dat;
	struct lq_plr_data *pdat;
	struct msg *msg,*next;
        int co,n,in,in2,domirror=0;
	char *ptr;

	dat=set_data(cn,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,30,0,60);			
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			if (dat->greeting[0] && cn!=co && (ch[co].flags&CF_PLAYER) && char_dist(cn,co)<12 && char_see_char(cn,co) && !mem_check_driver(cn,co,7)) {
				say(cn,"%s",dat->greeting);
				mem_add_driver(cn,co,7);
			}
		}

		if (msg->type==NT_GOTHIT) {
			co=msg->dat1;			
			if (co && (ch[co].flags&CF_PLAYER) &&
			    dat->hurt_markID>0 &&
			    dat->hurt_markID<MAXLQMARK &&
			    (pdat=set_data(co,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data))))
				pdat->mark[dat->hurt_markID]=1;
		}

		if (msg->type==NT_TEXT) {
			ptr=(char*)msg->dat2;
			co=msg->dat3;

			if (strcasestr(ptr,"followme") && (ch[co].flags&CF_LQMASTER) && char_dist(cn,co)<20) {
				dat->follow=co;
			}
			if (strcasestr(ptr,"stopfollow") && (ch[co].flags&CF_LQMASTER) && char_dist(cn,co)<20) {
				dat->follow=0;
			}

			// don't talk to NPCs
			if (!(ch[co].flags&CF_PLAYER)) continue;
			
			// dont talk to someone we cant see, and dont talk to ourself
			if (!co || !char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }


			while (*ptr && *ptr!=':') ptr++;
			while (*ptr && *ptr!='\"') ptr++;

			for (n=0; n<5; n++) {
				if (dat->trigger[n][0] && strcasestr(ptr,dat->trigger[n])) {
					say(cn,"%s",dat->reply[n]);
				}
			}

			tabunga(cn,co,(char*)msg->dat2);
		}

		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==MAKE_ITEMID(DEV_ID_LQ,dat->want_keyID)) {
					if (dat->reward_item.base[0]) {
						in2=create_lq_item(&dat->reward_item);
						if (in2 && !give_char_item(co,in2)) {
							destroy_item(in2);
						}
					}
					say(cn,"Thanks, that's what I wanted.");
				}
				// let it vanish
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

                standard_message_driver(cn,msg,dat->mode=='a',0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if ((co=dat->usurp) && (ch[co].flags&(CF_GOD|CF_LQMASTER)) && (pdat=set_data(co,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data))) && pdat->usurp==cn) {
		if (ch[co].x-ch[cn].x!=dat->udx || ch[co].y-ch[cn].y!=dat->udy) {
			if (move_driver(cn,ch[co].x-dat->udx,ch[co].y-dat->udy,0)) return;
		}
		if (ch[cn].dir!=ch[co].dir) {
			turn(cn,ch[co].dir);
		}
		domirror=1;
		ch[cn].tmpx=ch[cn].x;
		ch[cn].tmpy=ch[cn].y;
	}

	if (!domirror && (co=dat->follow) && (ch[co].flags&(CF_GOD|CF_LQMASTER))) {
                if (move_driver(cn,ch[co].x,ch[co].y,2)) return;
		ch[cn].tmpx=ch[cn].x;
		ch[cn].tmpy=ch[cn].y;
	}

        if (fight_driver_attack_visible(cn,0)) return;
        if (!domirror && fight_driver_follow_invisible(cn)) return;
	
        if (!domirror && secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;

	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

        do_idle(cn,TICKS/2);
}

void lq_ticker(int in,int cn)
{
	int n,in2,m;

	if (cn) return;

	if (!init_done) {
		init_done=1;
		for (m=in2=1; in2<MAXITEM && m<MAXLQDOOR; in2++) {
			if (it[in2].flags && it[in2].driver==2 && it[in2].drdata[10]) {
				strcpy(lq_door[m].nick,it[in2].name);
				lq_door[m].in=in2;
				update_lqdoor(m);
                                xlog("found door %d %s at %d,%d",m,lq_door[m].nick,it[in2].x,it[in2].y);
				m++;
			}
		}
	}

	for (n=1; n<MAXLQNPC; n++)
		if (lq_respawn[n] && ticker>=lq_respawn[n] && spawn_npc(n,0,0,0,NULL)) lq_respawn[n]=0;
	
	call_item(it[in].driver,in,0,ticker+TICKS);
}

void lqnpc_died(int cn,int co)
{
	struct lq_npc_data *dat;
	struct lq_plr_data *pdat;

	dat=set_data(cn,DRD_LQ_NPC_DATA,sizeof(struct lq_npc_data));
	if (!dat) return;	// oops...

	if (lq_npc[dat->n].cn==cn && lq_npc[dat->n].cserial==ch[cn].serial) {
		if (lq_npc[dat->n].respawn) {
			lq_respawn[dat->n]=ticker+lq_npc[dat->n].respawn*TICKS;
			//xlog("set respawn for NPC %d",dat->n);
		}
		lq_npc[dat->n].cn=lq_npc[dat->n].cserial=0;
	}
	if (co && (ch[co].flags&CF_PLAYER) && (dat->kill_markID || dat->hurt_markID)) {
		pdat=set_data(co,DRD_LQ_PLR_DATA,sizeof(struct lq_plr_data));
		if (!pdat) return;	// oops...

		if (dat->kill_markID>0 && dat->kill_markID<MAXLQMARK) pdat->mark[dat->kill_markID]=1;
		if (dat->hurt_markID>0 && dat->hurt_markID<MAXLQMARK) pdat->mark[dat->hurt_markID]=1;
	}
}

void lq_entrance(int in,int cn)
{
	struct misc_ppd *ppd;
	if (!cn) return;
	
	if (!lq_data.open) {
		log_char(cn,LOG_SYSTEM,0,"No quest is in progress, you may not enter.");
		return;
	}
	if (lq_data.min_level>ch[cn].level || lq_data.max_level<ch[cn].level) {
		log_char(cn,LOG_SYSTEM,0,"This quest is for levels %d to %d, you may not enter.",lq_data.min_level,lq_data.max_level);
		return;
	}
	if (!lq_data.entrance_x) {
		log_char(cn,LOG_SYSTEM,0,"No entrance defined, bad quest.");
		return;
	}
	if ((ppd=set_data(cn,DRD_MISC_PPD,sizeof(struct misc_ppd))) && realtime-ppd->last_lq_death<60*5) {
		log_char(cn,LOG_SYSTEM,0,"You may not enter again yet. Your remaining penalty is: %.2f minutes.",5.0-(realtime-ppd->last_lq_death)/60.0);
		return;
	}
	if (!teleport_char_driver(cn,lq_data.entrance_x,lq_data.entrance_y)) {
		log_char(cn,LOG_SYSTEM,0,"Target is busy, please try again later.");
	}
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_LQNPC:		lqnpc(cn,ret,lastact); return 1;
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {		
		case IDR_LQ_TICKER:	lq_ticker(in,cn); return 1;
		case IDR_LQ_ENTRANCE:	lq_entrance(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_LQNPC:			lqnpc_died(cn,co); return 1;
                default:			return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_LQNPC:			return 1;
                default:			return 0;
	}
}





