/*

$Id: prof.c,v 1.2 2007/05/04 08:52:53 devel Exp $

$Log: prof.c,v $
Revision 1.2  2007/05/04 08:52:53  devel
no stealing in live quests

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:36  sam
Added RCS tags


*/

#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "tool.h"
#include "talk.h"
#include "map.h"
#include "drvlib.h"
#include "log.h"
#include "act.h"
#include "notify.h"
#include "consistency.h"
#include "database.h"
#include "prof.h"

struct prof prof[P_MAX]={
	{"Athlete",		6,	30,	3},		// 0
	{"Alchemist",		10,	50,	10},		// 1
	{"Miner",		4,	20,	2},		// 2
	{"Assassin",		10,	50,	5},		// 3
	{"Thief",		6,	30,	3},		// 4
	{"Light Warrior",	6,	30,	3},		// 5
	{"Dark Warrior",	6,	30,	3},		// 6
        {"Trader",		4,	20,	2},		// 7
	{"Mercenary",		4,	20,	2},		// 8
	{"Clan Warrior",	6,	30,	3},		// 9
	{"Herbalist",		10,	30,	10},		// 10
	{"Demon",		1,	30,	1},		// 11
	{"empty",		3,	30,	1},		// 12
	{"empty",		3,	30,	1},		// 13
	{"empty",		3,	30,	1},		// 14
	{"empty",		3,	30,	1},		// 15
	{"empty",		3,	30,	1},		// 16
	{"empty",		3,	30,	1},		// 17
	{"empty",		3,	30,	1},		// 18
	{"empty",		3,	30,	1}		// 19
};

static char *prof_title(int p,int v)
{
        int d;

	d=100*v/prof[p].max;

	if (d<15) return "a newbie ";
	if (d<30) return "an apprentice ";
	if (d<45) return "an intermediate ";
	if (d<60) return "a fairly skilled ";
	if (d<75) return "a skilled ";
	if (d<90) return "a very skilled ";

	return "a master ";
}


int show_prof_info(int cn,int co,char *buf)
{
        int n,len=0;

	for (n=0; n<P_MAX; n++) {
		if (ch[co].prof[n]) len+=sprintf(buf+len,"%s is %s%s. ",Hename(co),prof_title(n,ch[co].prof[n]),prof[n].name);
	}

	return len;
}

int free_prof_points(int co)
{
	int n,cnt;

	for (n=cnt=0; n<P_MAX; n++) {
		cnt+=ch[co].prof[n];
	}
	return ch[co].value[1][V_PROFESSION]-cnt;
}

void cmd_steal(int cn)
{
	int co,x,y,n,cnt,in=0,chance,dice,diff,m;

	if (!ch[cn].prof[P_THIEF]) {
		log_char(cn,LOG_SYSTEM,0,"You are not a thief, you cannot steal.");
		return;
	}
	if (ch[cn].action!=AC_IDLE) {
		log_char(cn,LOG_SYSTEM,0,"You can only steal when standing still.");
		return;
	}

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please free your hand (mouse cursor) first.");
		return;
	}

	dx2offset(ch[cn].dir,&x,&y,NULL);
	
	x+=ch[cn].x;
	y+=ch[cn].y;

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) {
		log_char(cn,LOG_SYSTEM,0,"Out of map.");
		return;
	}
	
	m=x+y*MAXMAP;
	co=map[m].ch;
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"There's no one to steal from.");
		return;
	}

	if (!can_attack(cn,co)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot steal from someone you are not allowed to attack.");
		return;
	}
	if (map[m].flags&(MF_ARENA|MF_CLAN)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot steal inside an arena.");
		return;
	}
	if (!(ch[co].flags&CF_PLAYER)) {
		log_char(cn,LOG_SYSTEM,0,"You can only steal from players.");
		return;
	}
	if (ch[co].driver==CDR_LOSTCON) {
		log_char(cn,LOG_SYSTEM,0,"You cannot steal from lagging players.");
		return;
	}
	if (areaID==20) {
		log_char(cn,LOG_SYSTEM,0,"You cannot steal in Live Quests.");
		return;
	}

	if (ch[co].action!=AC_IDLE || ticker-ch[co].regen_ticker<TICKS) {
		log_char(cn,LOG_SYSTEM,0,"You cannot steal from someone if your victim is not standing still.");
		return;
	}
	
	for (n=cnt=0; n<INVENTORYSIZE; n++) {
		if (n>=12 && n<30) continue;
		if ((in=ch[co].item[n]) && !(it[in].flags&IF_QUEST) && can_carry(cn,in,1)) cnt++;
	}
	if (!cnt) {
		log_char(cn,LOG_SYSTEM,0,"You could not find anything to steal.");
		return;
	}
	cnt=RANDOM(cnt);

	for (n=cnt=0; n<INVENTORYSIZE; n++) {
		if (n>=12 && n<30) continue;
		if ((in=ch[co].item[n]) && !(it[in].flags&IF_QUEST) && can_carry(cn,in,1)) {
			if (cnt<1) break;
			cnt--;
		}		
	}
	if (n==INVENTORYSIZE) {
		log_char(cn,LOG_SYSTEM,0,"You could not find anything to steal (2).");
		return;
	}

	diff=(ch[cn].value[0][V_STEALTH]-ch[co].value[0][V_PERCEPT])/2;
	chance=40+diff;
	if (chance<10) {
		log_char(cn,LOG_SYSTEM,0,"You'd get caught for sure. You decide not to try.");
		return;
	}
	chance=min(chance,ch[cn].prof[P_THIEF]*3);

        dice=RANDOM(100);
	diff=chance-dice;

        if (diff<-20) {
		log_char(cn,LOG_SYSTEM,0,"%s noticed your attempt and stopped you from stealing.",ch[co].name);
		ch[cn].endurance=1;
		if (ch[co].flags&CF_PLAYER) {
			log_char(co,LOG_SYSTEM,0,"°c3%s tried to steal from you!",ch[cn].name);
		} else notify_char(co,NT_GOTHIT,cn,0,0);
		return;
	}

	dlog(co,in,"dropped because %s stole it",ch[cn].name);
	remove_item_char(in);
	if (!give_char_item(cn,in)) {
		destroy_item(in);
		elog("had to destroy item in cmd_steal()!");
		return;
	}

	add_pk_steal(cn);

	if (diff<0) {
		log_char(cn,LOG_SYSTEM,0,"%s noticed your theft, but you managed to steal a %s anyway.",ch[co].name,it[in].name);
		ch[cn].endurance=1;
		if (ch[co].flags&CF_PLAYER) {
			log_char(co,LOG_SYSTEM,0,"°c3%s stole your %s!",ch[cn].name,it[in].name);
		} else notify_char(co,NT_GOTHIT,cn,0,0);
	} else log_char(cn,LOG_SYSTEM,0,"You stole a %s without %s noticing.",it[in].name,ch[co].name);
}

















