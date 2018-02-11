/*

$Id: clanlog.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: clanlog.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:55  sam
Added RCS tags


*/

#include <stdlib.h>
#include <ctype.h>

#include "server.h"
#include "talk.h"
#include "lookup.h"
#include "database.h"
#include "tool.h"
#include "clan.h"
#include "clanlog.h"

void clanlog_help(int cn)
{
	log_char(cn,LOG_SYSTEM,0,"Usage: /clanlog [-p player] [-c clan] [-x prio] [-s starttime] [-e endtime]");
	log_char(cn,LOG_SYSTEM,0,"-p <player name>: restrict output to entries about that player");
	log_char(cn,LOG_SYSTEM,0,"-c <clan number>: restrict output to entries about that clan");
	log_char(cn,LOG_SYSTEM,0,"-x <prio number>: restrict output to entries with prio or higher (1=highest prio, 100=lowest prio)");
	log_char(cn,LOG_SYSTEM,0,"-s <hours>: restrict output to entries not more then hours old");
	log_char(cn,LOG_SYSTEM,0,"-e <hours>: restrict output to entries at least hours old");
	log_char(cn,LOG_SYSTEM,0,"-i: show clan internal log (same as -x 50 -c <own clan>)");
	log_char(cn,LOG_SYSTEM,0,"Example: /clanlog -p Ishtar -c 4 -x 5 -s 48 -e 24");
	log_char(cn,LOG_SYSTEM,0,"If no times are specified, /clanlog will use the last 24 hours. If no priority is given, it will be set to 20.");
	log_char(cn,LOG_SYSTEM,0,"If you specify a priority higher than 20, output will be restricted to your clan.");
}

static char *clanlog_player(int cn,char *ptr,int *pID,int *prepeat)
{
	char name[80];
	int len,ID;

	while (isspace(*ptr)) ptr++;

	for (len=0; len<75; len++) {
		if (!*ptr || isspace(*ptr)) break;
		name[len]=*ptr++;
	}
	if (len<1 || len>70) {
		log_char(cn,LOG_SYSTEM,0,"Invalid name");
		return NULL;
	}
	name[len]=0;

	ID=lookup_name(name,NULL);
	if (!ID) { 	
		if (prepeat) *prepeat=1;
	} else {
		if (prepeat) *prepeat=0;
		if (pID) *pID=ID;		
	}

	return ptr;
}

char *clanlog_clan(int cn,char *ptr,int *pclan,int *pserial)
{
	int clan;

	while (isspace(*ptr)) ptr++;
	clan=atoi(ptr);
	while (isdigit(*ptr)) ptr++;
	
	if (clan<1 || clan>=MAXCLAN) {
		log_char(cn,LOG_SYSTEM,0,"Clan number out of bounds");
		return NULL;
	}
	if (pclan) *pclan=clan;
	if (pserial) *pserial=clan_serial(clan);

	return ptr;
}

char *clanlog_prio(int cn,char *ptr,int *pprio)
{
	int prio;

	while (isspace(*ptr)) ptr++;
	prio=atoi(ptr);
	while (isdigit(*ptr)) ptr++;
	
	if (prio<1 || prio>100) {
		log_char(cn,LOG_SYSTEM,0,"Priority out of bounds");
		return NULL;
	}
	if (pprio) *pprio=prio;

	return ptr;
}

char *clanlog_hours(int cn,char *ptr,int *phours)
{
	int hours;

	while (isspace(*ptr)) ptr++;
	hours=atoi(ptr);
	while (isdigit(*ptr)) ptr++;

	hours=time_now-hours*60*60;

        if (hours<0 || hours>time_now) {
		log_char(cn,LOG_SYSTEM,0,"Hours out of bounds");
		return NULL;
	}
	if (phours) *phours=hours;

	return ptr;
}

int cmd_clanlog(int cn,char *ptr)
{
	int coID=0,clan=0,serial=0,prio=20,start=0,end=0,repeat=0;

	while (*ptr) {
		if (isspace(*ptr)) { ptr++; continue; }
		if (*ptr!='-') {
			clanlog_help(cn);
			return 1;
		}
		ptr++;
		switch(*ptr) {
			case 'p':	ptr=clanlog_player(cn,ptr+1,&coID,&repeat);
                                        if (!ptr) return 1;
					if (repeat) return 0;
					break;
			case 'c':	ptr=clanlog_clan(cn,ptr+1,&clan,&serial);
					if (!ptr) return 1;
					break;
			case 'x':	ptr=clanlog_prio(cn,ptr+1,&prio);
					if (!ptr) return 1;
					break;
			case 's':	ptr=clanlog_hours(cn,ptr+1,&start);
					if (!ptr) return 1;
					break;
			case 'e':	ptr=clanlog_hours(cn,ptr+1,&end);
					if (!ptr) return 1;
					break;
			case 'i':	ptr++;
					prio=50;
					clan=ch[cn].clan;
					break;
			default:	
			case 'h':	clanlog_help(cn);
					return 1;
		}
	}
	if (!start) start=time_now-60*60*24;
	if (!end) end=time_now;
	
	if (start>end) {
		log_char(cn,LOG_SYSTEM,0,"Start time may not be greater than end time.");
		return 1;
	}

	if (prio>20) {
		if (!ch[cn].clan) {
			log_char(cn,LOG_SYSTEM,0,"Only clan members may set a priority greater than 20.");
			return 1;
		}
		if (clan!=ch[cn].clan) {
			clan=ch[cn].clan;
			log_char(cn,LOG_SYSTEM,0,"Changed clan to %d.",clan);			
		}
	}

	lookup_clanlog(ch[cn].ID,clan,serial,coID,prio,start,end);
	return 1;
}
