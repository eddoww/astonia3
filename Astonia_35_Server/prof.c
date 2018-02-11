/*
 * $Id: prof.c,v 1.4 2007/07/09 11:24:10 devel Exp $
 *
 * $Log: prof.c,v $
 * Revision 1.4  2007/07/09 11:24:10  devel
 * reduced P_MAX to 10
 *
 * Revision 1.3  2007/07/04 09:23:06  devel
 * added new and removed old professions
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
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
	{"Smith",		5,	25,	5},		// 0
	{"Alchemist",		5,	25,	5},		// 1
	{"Miner",		5,	25,	5},		// 2
	{"Enhancer",		5,	25,	5},		// 3
	{"Mercenary",		5,	25,	5},		// 4
        {"Trader",		5,	25,	5},		// 5
	{"empty",		4,	20,	2},		// 6
	{"empty",		1,	30,	1},		// 7
	{"empty",		3,	30,	1},		// 8
	{"empty",		3,	30,	1},		// 9
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

int get_support_prof(int cn,int p)
{
	int co;

	if (p<0 || p>=P_MAX) return 0;

	if (!(co=ch[cn].supported)) return ch[cn].prof[p];
	if (!(ch[co].flags&CF_PLAYER)) return ch[cn].prof[p];
	if (ch[co].supporting!=cn) return ch[cn].prof[p];
	
	return max(ch[cn].prof[p],ch[co].prof[p]);
}










