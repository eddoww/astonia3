#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "server.h"
#include "log.h"
#include "mem.h"
#include "tool.h"
#include "direction.h"
#include "talk.h"
#include "date.h"
#include "clan.h"
#include "database.h"
#include "map.h"
#include "effect.h"
#include "create.h"
#include "drdata.h"
#include "notify.h"
#include "sector.h"
#include "player.h"
#include "drvlib.h"
#include "club.h"

int club_update_done=0;

int get_char_club(int cn)
{
	int cnr;

        if (!(cnr=ch[cn].clan)) return 0;

        if (cnr<CLUBOFFSET) return 0;
	
	if (cnr>=CLUBOFFSET+MAXCLUB) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
                return 0;
	}

	// dont erase clans because the serial numbers havent been read yet
	if (!club_update_done) return cnr;

	cnr-=CLUBOFFSET;
	
	if (ch[cn].clan_serial!=club[cnr].serial) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
		return 0;
	}
	if (!club[cnr].name[0]) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
		return 0;	
	}
	return cnr;
}

static char *rankname[3]={
		"Member",
		"Leader",
		"Founder"
	};

int show_club_info(int cn,int co,char *buf)
{
	int cnr,rank;

	if (!(cnr=get_char_club(co))) return 0;

        rank=ch[co].clan_rank;
	if (rank<0 || rank>2) { elog("showclub(): got illegal club rank (%s,%d,%d)",ch[cn].name,cn,rank); rank=0; }

	return sprintf(buf,"%s is a member of the club '%s' (%d), %s rank is %s. ",Hename(co),club[cnr].name,cnr,hisname(co),rankname[rank]);
}

void tick_club(void)
{
	static int last_update=0;
	int n;
	
	if (areaID!=3 && realtime-last_update>30) {
		schedule_clubs();
		last_update=realtime;
	}
	if (areaID==3 && realtime-last_update>30) {
		for (n=0; n<MAXCLUB; n++) {
			if (!club[n].name[0]) continue;
			if (club[n].paid>realtime) continue;
			
			if (club[n].money<10000*100) {
				xlog("club %d %s deleted",n,club[n].name);
				club[n].name[0]=0;				
			} else {
				club[n].money-=10000*100;
				club[n].paid+=60*60*24*7;
				xlog("club %d %s paid 10000g, new money %d, new paid %d",n,club[n].name,club[n].money,club[n].paid);
			}
			db_update_club(n);
			break;
		}
		last_update=realtime;
	}
}

void showclub(int cn)
{
	int cnr,rank;

        if ((cnr=get_char_club(cn))) {
		rank=ch[cn].clan_rank;
		if (rank<0 || rank>2) { elog("showclub(): got illegal clan rank (%s,%d,%d)",ch[cn].name,cn,rank); rank=0; }
		
		log_char(cn,LOG_SYSTEM,0,"You are a member of the club '%s' (%d), your rank is %s.",club[cnr].name,cnr,rankname[rank]);
		
		log_char(cn,LOG_SYSTEM,0,"Your club has %d gold, the next payment of 10000 gold is due in %d hours.",
			 club[cnr].money/100,
			 (club[cnr].paid-realtime)/60/60);
	}
}

void kill_club(int cnr)
{
	if (cnr>0 && cnr<MAXCLUB) {
		club[cnr].paid=1;
		club[cnr].money=0;
		db_update_club(cnr);
	}
}

int create_club(char *name)
{
	int n;

	for (n=0; name[n]; n++) {
		if (name[n]!=' ' && !isalpha(name[n])) return 0;
	}
	if (n>75) return 0;

	for (n=1; n<MAXCLUB; n++) {
		if (!strcmp(club[n].name,name)) break;
	}
	if (n!=MAXCLUB) return 0;

	for (n=1; n<MAXCLUB; n++) {
		if (!club[n].name[0]) break;
	}
	if (n==MAXCLUB) return 0;

	strcpy(club[n].name,name);
	club[n].serial++;
	club[n].paid=realtime+60*60*24*7;
	club[n].money=0;
	if (club[n].serial==1) db_create_club(n);
	else db_update_club(n);

	xlog("created club %d %s",n,club[n].name);
	
	return n;
}

int rename_club(int nr,char *name)
{
	int n;
	
	for (n=0; name[n]; n++) {
		if (name[n]!=' ' && !isalpha(name[n])) return 0;
	}
	if (n>75) return 0;
	
	for (n=1; n<MAXCLUB; n++) {
		if (!strcmp(club[n].name,name)) break;
	}
	if (n!=MAXCLUB) return 0;
	if (nr<1 || nr>=MAXCLUB) return 0;

	strcpy(club[nr].name,name);
	db_update_club(nr);
	return 1;
}
