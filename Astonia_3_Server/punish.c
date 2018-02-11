/*

$Id: punish.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: punish.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:38  sam
Added RCS tags


*/

#include <string.h>
#include <time.h>

#include "server.h"
#include "database.h"
#include "death.h"
#include "punish.h"
#include "lookup.h"
#include "chat.h"

void list_punishment(int rID,struct punishment *pun,int cID,int date,int ID)
{
	char name[80];
	struct tm *tm;

	lookup_ID(name,cID);
	
	tm=localtime((void*)&date);

	tell_chat(0,rID,1,"P%d: Level: %d, Exp: %d, Karma: %d, Creator: %s, Date: %02d/%02d/%04d %02d:%02d:%02d, Reason: %s",
		  ID,
		  pun->level,
		  pun->exp,
		  pun->karma,
		  name,
		  tm->tm_mon+1,
		  tm->tm_mday,
		  tm->tm_year+1900,
		  tm->tm_hour,
		  tm->tm_min,
		  tm->tm_sec,
		  pun->reason);	
}

// pID is punishing co
int punish(int pID,struct character *co,int level,char *reason,int *plock,int *pkick)
{
	struct punishment pm;
	int exp,karma;

	if (plock) *plock=0;
	if (pkick) *pkick=0;

	pm.level=level;
	strncpy(pm.reason,reason,79); pm.reason[79]=0;
	
	switch(level) {
		case 0:		exp=0; karma=0; break;
		case 1:		exp=(death_loss(co->exp)+3)/4; karma=1; break;
		case 2:		exp=(death_loss(co->exp)+1)/2; karma=2; break;
		case 3:		exp=death_loss(co->exp); karma=4; break;
		case 4:		exp=death_loss(co->exp)*2; karma=6; break;
		case 5:		exp=death_loss(co->exp)*4; karma=8; break;
		case 6:		exp=0; karma=12; break;
		default:	exp=0; karma=0; break;
	}

	pm.exp=exp;
	pm.karma=karma;

	co->exp-=exp;
	co->karma-=karma;

	co->flags|=CF_UPDATE;

        if (co->karma<=-12 && plock) *plock=1;
        if (!(co->flags&CF_PAID) && co->karma<=-5 && pkick) *pkick=1;

	return add_note(co->ID,1,pID,(void*)&pm,sizeof(pm));
}

int unpunish(int pID,struct character *co,int ID,int *plock,int *pkick)
{
        struct punishment pm;

	if (plock) *plock=0;
	if (pkick) *pkick=0;

	if (!db_unpunish(ID,&pm,sizeof(pm))) return 0;

        co->exp+=pm.exp;
	co->karma+=pm.karma;
	co->flags|=CF_UPDATE;

        if (plock) *plock=-1;
	
	return 1;
}
