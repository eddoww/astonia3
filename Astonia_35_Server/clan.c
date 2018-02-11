/*
 *
 * $Id: clan.c,v 1.12 2008/03/27 09:07:59 devel Exp devel $ (c) 2005 D.Brockhaus
 *
 * $Log: clan.c,v $
 * Revision 1.12  2008/03/27 09:07:59  devel
 * bonuses get turned off when jewel count reaches 0
 *
 * Revision 1.11  2007/12/03 10:06:09  devel
 * added one-time cost to bonus increase
 *
 * Revision 1.10  2007/11/04 12:59:18  devel
 * new clan bonuses
 *
 * Revision 1.9  2007/08/21 22:03:05  devel
 * *** empty log message ***
 *
 * Revision 1.8  2007/07/01 12:08:42  devel
 * fixed bug in del_jewels
 *
 * Revision 1.7  2007/04/12 11:51:32  devel
 * made characters who were in expired clans able to join a new clan right away
 *
 * Revision 1.6  2007/03/03 12:14:45  devel
 * increased costs by factor 2
 *
 * Revision 1.5  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.4  2005/12/14 15:33:15  ssim
 * added clan message display to /clan
 *
 * Revision 1.3  2005/12/01 16:31:09  ssim
 * added clan message
 *
 * Revision 1.2  2005/11/06 13:41:04  ssim
 * fixed bug in clan_dungeon_chat(): missing break would remove simple potion after successful raid
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

#define CLANHALLRENT	100

static char *rel_name[]={
	"none",
	"Alliance",
	"Peace-Treaty",
	"Neutral",
	"War",
	"Feud"
};


static int update_relations(void);
static int update_treasure(void);

struct clan clan[MAXCLAN];
static int update_time,update_state=0;

int this_clan_changed[MAXCLAN];
int clan_update_done=0;

static char *bonus_name[MAXBONUS]={
		"Pentagram Quest",	//0
		"Merchant",		//1
		"Gear Preservation",	//2
		"Smith",		//3
		"Enhancer",		//4
		"unassigned",		//5
		"unassigned",		//6
		"unassigned",		//7
		"unassigned",		//8
		"unassigned"		//9
};
static void clan_standards(struct clan *c)
{
	int n;

	strcpy(c->rankname[0],"Trial Member");
	strcpy(c->rankname[1],"Member");
	strcpy(c->rankname[2],"Recruiter");
	strcpy(c->rankname[3],"Treasurer");
	strcpy(c->rankname[4],"Leader");

	for (n=1; n<MAXCLAN; n++) {
		c->status.current_relation[n]=CS_NEUTRAL;
		c->status.want_relation[n]=CS_NEUTRAL;
		c->status.want_date[n]=realtime;
	}

	c->treasure.payed_till=realtime;
	c->treasure.debt=0;
	c->status.members=1;
	c->depot.money=0;
}

void showclan(int cn)
{
	int n,cnr,rank;

	log_char(cn,LOG_SYSTEM,0,"Clan List");

        for (n=1; n<MAXCLAN; n++) {
		if (!clan[n].name[0]) continue;
		log_char(cn,LOG_SYSTEM,0,"\001%d \004%d jewels \010%d members \014%s",
			 n,
			 clan[n].treasure.jewels-(clan[n].treasure.debt/1000),
			 clan[n].status.members,
			 clan[n].name);
	}

        if ((cnr=get_char_clan(cn))) {
		rank=ch[cn].clan_rank;
		if (rank<0 || rank>4) { elog("showclan(): got illegal clan rank (%s,%d,%d)",ch[cn].name,cn,rank); rank=0; }
		
		log_char(cn,LOG_SYSTEM,0,"You are a member of the clan '%s' (%d), your rank is %s.",clan[cnr].name,cnr,clan[cnr].rankname[rank]);
		if (rank>0) {
			log_char(cn,LOG_SYSTEM,0,"Your clan has %d jewels, %.3f expenses per week, and a debt of %.3f jewels. Your clan's treasury holds %dG.",
				 clan[cnr].treasure.jewels,
				 clan[cnr].treasure.cost_per_week/1000.0,
                                 clan[cnr].treasure.debt/1000.0,
				 clan[cnr].depot.money);			
		}

		log_char(cn,LOG_SYSTEM,0,"Clan Website is at: %s",clan[cnr].website);
		log_char(cn,LOG_SYSTEM,0,"Clan message: %s",clan[cnr].message);

		for (n=0; n<MAXBONUS; n++) {
			if (clan[cnr].bonus[n].level) {
				log_char(cn,LOG_SYSTEM,0,"Bonus '%s' (%d) is at level %d.",bonus_name[n],n,clan[cnr].bonus[n].level);
			}
		}
	}
}

int get_char_clan(int cn)
{
	int cnr;

        if (!(cnr=ch[cn].clan)) return 0;

	if (cnr>=CLUBOFFSET) return 0;

	if (cnr<1 || cnr>=MAXCLAN) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
		ch[cn].clan_leave_date=realtime-60*60*24;
                return 0;
	}

        // dont erase clans because the serial numbers havent been read yet
	if (!clan_update_done) return cnr;

	if (!(ch[cn].flags&CF_PLAYER)) return cnr;

        if (ch[cn].clan_serial!=clan[cnr].status.serial) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
		ch[cn].clan_leave_date=realtime-60*60*24;
		return 0;
	}
	if (!clan[cnr].name[0]) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
		ch[cn].clan_leave_date=realtime-60*60*24;
		return 0;	
	}
	return cnr;
}

char *get_char_clan_name(int cn)
{
	int cnr;

	cnr=get_char_clan(cn);

	if (!cnr) return NULL;	

	return clan[cnr].name;
}

char *get_clan_name(int cnr)
{
	if (cnr<1 || cnr>=MAXCLAN) return NULL;

	return clan[cnr].name;
}

int show_clan_info(int cn,int co,char *buf)
{
	int cnr,rank;

	if (!(cnr=get_char_clan(co))) return 0;

        rank=ch[co].clan_rank;
	if (rank<0 || rank>4) { elog("showclan(): got illegal clan rank (%s,%d,%d)",ch[cn].name,cn,rank); rank=0; }

	return sprintf(buf,"%s is a member of the clan '%s', %s rank is %s. ",Hename(co),clan[cnr].name,hisname(co),clan[cnr].rankname[rank]);
}

void show_clan_relation(int cn,int cnr)
{
	int n,crel,wrel,orel,wdiff,odiff;

	if (cnr<1 || cnr>=MAXCLAN) return;

	if (!clan[cnr].name[0]) {
		log_char(cn,LOG_SYSTEM,0,"No clan by that number (%d).",cnr);
		return;
	}

	log_char(cn,LOG_SYSTEM,0,"%s relations:",clan[cnr].name);

	for (n=1; n<MAXCLAN; n++) {
		if (!clan[n].name[0]) continue;
		if (n==cnr) continue;
		
		crel=clan[cnr].status.current_relation[n];
		wrel=clan[cnr].status.want_relation[n];
		orel=clan[n].status.want_relation[cnr];
		wdiff=realtime-clan[cnr].status.want_date[n];
		odiff=realtime-clan[n].status.want_date[cnr];

		if (crel<1 || crel>5) { elog("show_clan_relation(): got illegal current_relation %d in clan %d for clan %d",crel,cnr,n); crel=0; }
		if (wrel<1 || wrel>5) { elog("show_clan_relation(): got illegal want_relation %d in clan %d for clan %d",wrel,cnr,n); crel=0; }
		if (orel<1 || orel>5) { elog("show_clan_relation(): got illegal want_relation %d in clan %d for clan %d",wrel,n,cnr); orel=0; }

		log_char(cn,LOG_SYSTEM,0,"%d: %s: %s (%s [%02d:%02d] - %s [%02d:%02d])",
			 n,
			 clan[n].name,
			 rel_name[crel],
			 rel_name[wrel],
			 (wdiff/60)/60,(wdiff/60)%60,
			 rel_name[orel],
			 (odiff/60)/60,(odiff/60)%60);
	}
}

void tick_clan(void)
{
	int n;
	static int cnr=1;

	switch(update_state) {
		case 0:         schedule_clans();
				update_time=ticker;
                                update_state++;
				break;
		case 1:         if (areaID!=13) {	// only area 13 updates clan info
					if (ticker>update_time+TICKS*10) // re-read data every 10 seconds
						update_state=0;
				} else update_state++;
				break;
		
		case 2:		if (!update_relations()) update_state++;
				break;
		case 3:		if (!update_treasure()) update_state++;
				break;
		case 4:		if (clan[cnr].name[0]) schedule_clan_membercount(cnr);
				cnr++;
				if (cnr>=MAXCLAN) cnr=1;
				update_state++;
				break;
                case 5:        	if (ticker>update_time+TICKS*10) {
					update_state++;
					update_time=ticker;
				}
				break;
		case 6:         for (n=1; n<MAXCLAN; n++) {
					if (this_clan_changed[n]) {
						this_clan_changed[n]=0;
						clan[n].hall.tricky++;
						db_update_clan(n);
						break;
					}
				}
				if (n==MAXCLAN) update_state=2;
				break;				
		default:	update_state=0; // sanity check, should never happen
				break;
	}	
}

void zero_relation(int nr)
{
	int n;

	for (n=1; n<MAXCLAN; n++) {
		clan[n].status.current_relation[nr]=CS_NEUTRAL;
		clan[n].status.want_relation[nr]=CS_NEUTRAL;
		clan[n].status.want_date[nr]=CS_NEUTRAL;
	}
}

int get_clanalive(int cnr)
{
	if (cnr<1 || cnr>=MAXCLAN) return 0;
	return clan[cnr].name[0];
}

int found_clan(char *name,int cn,int cnr)
{
	int serial;

        if (strlen(name)>78) return -1;
        if (clan[cnr].name[0]) return -1;
		
        serial=clan[cnr].status.serial;
	bzero(clan+cnr,sizeof(struct clan));
	strcpy(clan[cnr].name,name);
	clan[cnr].status.serial=serial;
	clan_standards(clan+cnr);
	zero_relation(cnr);
        this_clan_changed[cnr]=1;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,1,"Clan was founded by %s",ch[cn].name);
	
	return 0;
}

int add_jewel(int nr,int cn,int cnt)
{
        clan[nr].treasure.jewels+=cnt;
	this_clan_changed[nr]=1;

	add_clanlog(nr,clan_serial(nr),ch[cn].ID,25,"%s added a jewel worth %d points",ch[cn].name,cnt);	
	
	return 0;
}

int del_jewel(int nr,int cn,int cnt)
{
	int v;

	v=min(cnt,clan[nr].treasure.jewels);
        if (v>0) {
		clan[nr].treasure.jewels-=v;
		this_clan_changed[nr]=1;

		add_clanlog(nr,clan_serial(nr),ch[cn].ID,25,"%s stole a jewel worth %d points",ch[cn].name,v);
                return v;
	}
	
	return 0;
}

int cnt_jewels(int nr)
{
	return clan[nr].treasure.jewels;
}

int get_clan_bonus(int cnr,int nr)
{
	return clan[cnr].bonus[nr].level;
}

int set_clan_bonus(int cnr,int nr,int level,int cn)
{
	int diff,cost;

	diff=level-clan[cnr].bonus[nr].level;
	if (diff>0) {
                if (cnt_jewels(cnr)<10) return -1;

		cost=diff*250;
		clan[cnr].treasure.debt+=cost;
		add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,35,"%s set bonus %d to %d and paid %.2f jewels",ch[cn].name,nr,level,cost/1000.0);
	} else add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,35,"%s set bonus %d to %d",ch[cn].name,nr,level);
        clan[cnr].bonus[nr].level=level;
	this_clan_changed[cnr]=1;
	
	return 0;
}

int set_clan_website(int cnr,char *site,int cn)
{
        strncpy(clan[cnr].website,site,79); clan[cnr].website[79]=0;
	clan[cnr].website[strlen(clan[cnr].website)-1]=0;
	this_clan_changed[cnr]=1;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,35,"%s set website %s",ch[cn].name,site);

	return 0;
}

int set_clan_message(int cnr,char *site,int cn)
{
        strncpy(clan[cnr].message,site,79); clan[cnr].message[79]=0;
	clan[cnr].message[strlen(clan[cnr].message)-1]=0;
	this_clan_changed[cnr]=1;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,35,"%s set message %s",ch[cn].name,site);

	return 0;
}

void show_clan_message(int cn)
{
	int cnr;

	cnr=get_char_clan(cn);
	if (cnr) {
		if (clan[cnr].message[0]) {
			log_char(cn,LOG_SYSTEM,0,"°c16Clan Message: %s",clan[cnr].message);
		}
	}
}

int set_clan_relation(int cnr,int onr,int rel,int cn)
{
        if (cnr<1 || cnr>=MAXCLAN) return -1;
	if (onr<1 || onr>=MAXCLAN) return -1;
	if (rel<1 || rel>5) return -1;
		
        if (clan[cnr].status.want_relation[onr]!=rel) clan[cnr].status.want_date[onr]=realtime;		
	clan[cnr].status.want_relation[onr]=rel;
	this_clan_changed[cnr]=1;
	
        add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,22,"%s set relation to clan %s (%d) to %s (%d)",ch[cn].name,get_clan_name(onr),onr,rel_name[rel],rel);

	return 0;
}

int set_clan_rankname(int cnr,int rank,char *name,int cn)
{
	if (cnr<1 || cnr>=MAXCLAN) return -1;
	if (rank<0 || rank>=5) return -1;
	if (strlen(name)>37) return -1;

	strcpy(clan[cnr].rankname[rank],name);
	this_clan_changed[cnr]=1;
	
	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,33,"%s set rank name %d to %s",ch[cn].name,rank,name);

	return 0;
}

int may_enter_clan(int cn,int nr)
{
	int cnr;

	cnr=get_char_clan(cn);
	if (cnr==nr) return 1;	// everybody may enter his own clan
	if (!cnr) return 0;	// non clan members may never enter

	// you may not enter deleted clans
	if (!clan[nr].name[0]) return 0;

	// rest depends on relations
	switch(clan[nr].status.current_relation[cnr]) {		
		case CS_ALLIANCE:	return 1;
		default:		return 0;
	}
}

int clan_can_attack_outside(int c1,int c2)
{
	switch(clan[c1].status.current_relation[c2]) {
                case CS_FEUD:		return 1;
		default:		return 0;
	}
}

int clan_can_attack_inside(int c1,int c2)
{
	switch(clan[c1].status.current_relation[c2]) {
		case CS_WAR:		return 1;
                case CS_FEUD:		return 1;
		default:		return 0;
	}
}

int clan_alliance(int c1,int c2)
{
	switch(clan[c1].status.current_relation[c2]) {
		case CS_ALLIANCE:	return 1;
                default:		return 0;
	}
}

int clan_alliance_self(int c1,int c2)
{
	if (c1==c2) return 1;
        switch(clan[c1].status.current_relation[c2]) {
		case CS_ALLIANCE:	return 1;
                default:		return 0;
	}
}

static int update_relations(void)
{
	int n,m;
	int want1,want2,cur,diff1,diff2;

	for (n=1; n<MAXCLAN; n++) {
		if (!clan[n].name[0]) continue;

                for (m=1; m<MAXCLAN; m++) {
			if (n==m) continue;
			if (!clan[m].name[0]) continue;

                        want1=clan[n].status.want_relation[m];
			diff1=realtime-clan[n].status.want_date[m];
			want2=clan[m].status.want_relation[n];
			diff2=realtime-clan[m].status.want_date[n];
			cur=clan[n].status.current_relation[m];

			if (want1==want2 && cur==want1) continue;	// both want what they have, no work

			if (want1==want2) {	// both want the same, set it
				clan[n].status.current_relation[m]=clan[m].status.current_relation[n]=want1;
				this_clan_changed[n]=1;
				this_clan_changed[m]=1;
				add_clanlog(n,clan_serial(n),0,10,"%s with %s (%d) started",rel_name[want1],get_clan_name(m),m);
				add_clanlog(m,clan_serial(m),0,10,"%s with %s (%d) started",rel_name[want1],get_clan_name(n),n);
				continue;
			}
			
			switch(cur) {
				case CS_ALLIANCE:	// we are at alliance, but one of them wants out
							if (want1>CS_ALLIANCE && diff1>60*60*24) {
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Alliance with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Alliance with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want2>CS_ALLIANCE && diff2>60*60*24) {
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Alliance with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Alliance with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
				case CS_PEACETREATY:	// we're at treaty, going up to alliance is only possible if both want
							// it, and thats dealt with above. therefore, one of them wants out
							if (want1>CS_PEACETREATY && diff1>60*60*24) {
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want2>CS_PEACETREATY && diff2>60*60*24) {
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
				case CS_NEUTRAL:	// we're at neutral, lets see
							if (want1>CS_NEUTRAL && want2>CS_NEUTRAL) {	// both want at least war, ok without timecheck
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) started",get_clan_name(n),n);
								break;
							}
							if (want1>CS_NEUTRAL && diff1>60*60) {	// nr1 wants war, needs 1 hour
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) started",get_clan_name(n),n);
								break;
							}
							if (want2>CS_NEUTRAL && diff2>60*60) {	// nr2 wants war, needs 1 hour
								cur++;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) started",get_clan_name(n),n);
								break;
							}
							if (want1<CS_NEUTRAL && want2<CS_NEUTRAL) {	// both want a better status, ok at once
								cur--;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Peace Treaty with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Peace Treaty with %s (%d) started",get_clan_name(n),n);
								break;
							}
							break;
				case CS_WAR:		// we're at war. changing to feud can only happen if both want it
							if (want1<CS_WAR && want2<CS_WAR) {	// both want a better status, ok at once
								cur--;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
				case CS_FEUD:		// we have a feud, but at least one of them wants out
							if (want1<CS_FEUD && want2<CS_FEUD) {	// both want a better status, ok at once
								cur--;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Feud with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Feud with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want1<CS_FEUD && diff1>60*60*24) {	// nr1 wants out, needs 24 hours
								cur--;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Feud with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Feud with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want2<CS_FEUD && diff2>60*60*24) {	// nr2 wants out, needs 24 hours
								cur--;
								this_clan_changed[n]=1;
								this_clan_changed[m]=1;
								add_clanlog(n,clan_serial(n),0,10,"Feud with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Feud with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
			}
			clan[n].status.current_relation[m]=clan[m].status.current_relation[n]=cur;
		}
	}
	return 0;
}

static int update_treasure(void)
{
	int n,cost,cnr,step,diff;

        for (cnr=1; cnr<MAXCLAN; cnr++) {
		if (!clan[cnr].name[0]) continue;

		if (clan[cnr].treasure.jewels<=0) {
			for (n=0; n<MAXBONUS; n++) {
				if (clan[cnr].bonus[n].level) {
					add_clanlog(cnr,clan_serial(cnr),0,35,"Bonus %d reset to 0. No clan jewels left.",n);
					clan[cnr].bonus[n].level=0;
				}
			}		
		}
			
		for (n=cost=0; n<MAXBONUS; n++) {
			cost+=clan[cnr].bonus[n].level*2000;
		}

		cost+=CLANHALLRENT*1000;

                if (clan[cnr].treasure.cost_per_week!=cost) {
			clan[cnr].treasure.cost_per_week=cost;
			this_clan_changed[cnr]=1;
			//xlog("clan %d, cost per week changed",cnr);
		}

		diff=realtime-clan[cnr].treasure.payed_till;
		if (diff>60*15) {	// update 15 minutes late to reduce load
			step=(60*60*24*7)/cost;
			n=diff/step+1;
			clan[cnr].treasure.debt+=n;
			clan[cnr].treasure.payed_till+=step*n;
			this_clan_changed[cnr]=1;
			//xlog("clan %d, deducted rent",cnr);
		}
                if (clan[cnr].treasure.debt>=1000 && clan[cnr].treasure.jewels>0) {
			n=clan[cnr].treasure.debt/1000;
			if (n>clan[cnr].treasure.jewels) { n=clan[cnr].treasure.jewels; clan[cnr].treasure.jewels=0; }
			else clan[cnr].treasure.jewels-=n;

			clan[cnr].treasure.debt-=n*1000;
			this_clan_changed[cnr]=1;
			xlog("clan %s, paid %d jewels",clan[cnr].name,n);
		}
		
		if (clan[cnr].treasure.debt>=10000) {
			xlog("clan %s is broke, removing",clan[cnr].name);
			add_clanlog(cnr,clan_serial(cnr),0,1,"Clan %s went broke and was deleted",get_clan_name(cnr));
			clan[cnr].name[0]=0;
			clan[cnr].status.serial++;
			this_clan_changed[cnr]=1;
		}
	}
	
        return 0;
}

void add_member(int cn,int cnr,char *master)
{
	int n;

	ch[cn].clan=cnr;
	ch[cn].clan_serial=clan[cnr].status.serial;
	ch[cn].clan_join_date=realtime;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,15,"%s was added to clan by %s",ch[cn].name,master);

	for (n=1; n<MAXPLAYER; n++) set_player_knows_name(n,cn,0);
}

void remove_member(int cn,int master_cn)
{
	int n;

	if (ch[cn].clan<CLUBOFFSET) add_clanlog(ch[cn].clan,clan_serial(ch[cn].clan),ch[cn].ID,15,"%s was fired from clan by %s",ch[cn].name,ch[master_cn].name);

	ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
	ch[cn].clan_leave_date=realtime;

	for (n=1; n<MAXPLAYER; n++) set_player_knows_name(n,cn,0);
}

int clan_serial(int cnr)
{
	if (cnr<1 || cnr>=MAXCLAN) return 0;	
	return clan[cnr].status.serial;
}

int get_clan_money(int cnr)
{
	if (cnr<1 || cnr>=MAXCLAN) return 0;	

	return clan[cnr].depot.money;
}

void clan_money_change(int cnr,int diff,int cn)
{
	if (cnr<1 || cnr>=MAXCLAN) return;

	if (cn && (diff>=100 || diff<0)) {
                if (diff>0) add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,28,"%s deposited %dG",ch[cn].name,diff);
		else add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,28,"%s withdrew %dG",ch[cn].name,-diff);
	}

	clan[cnr].depot.money+=diff;
	this_clan_changed[cnr]=1;
}

void kill_clan(int cnr)
{
	if (cnr>0 && cnr<MAXCLAN) clan[cnr].treasure.debt=9999*1000;
}

void clan_setname(int cnr,char *name)
{
	if (cnr>0 && cnr<MAXCLAN) {
		strncpy(clan[cnr].name,name,78);
		clan[cnr].name[78]=0;
	}
}

int get_clan_ready()
{
	return clan_update_done;
}

int clan_trade_bonus(int cn)
{
	int cnr;

	if (!(cnr=get_char_clan(cn))) return 0;

	return (int)(get_clan_bonus(cnr,1)*7.5);
}

int clan_preservation_bonus(int cn)
{
	int cnr;

	if (!(cnr=get_char_clan(cn))) return 0;

	return get_clan_bonus(cnr,2)/2;
}

int clan_smith_bonus(int cn)
{
	int cnr;

	if (!(cnr=get_char_clan(cn))) return 0;

	return get_clan_bonus(cnr,3)/2;
}

int clan_enhance_bonus(int cn)
{
	int cnr;

	if (!(cnr=get_char_clan(cn))) return 0;

	return get_clan_bonus(cnr,4)/2;
}

int clanslot_free(int cnr)
{
	if (!clan[cnr].name[0]) return 1;
	else return 0;
}

int may_join_clan(int cn,char *errbuff)
{
	int diff;

	diff=realtime-ch[cn].clan_leave_date;
	if (diff<60*60*12) {
		sprintf(errbuff,"You were part of another clan during the last 12 hours (%.2fh to go).",12.0-diff/(60.0*60.0));
		return 0;
	}
	diff=realtime-ch[cn].clan_join_date;
	if (diff<60*60*24*3) {
		sprintf(errbuff,"You joined another clan less than 3 days ago (%.2fh to go).",24.0*3.0-diff/(60.0*60.0));
		return 0;
	}
	return 1;
}




/*

spawners:

area	cnt
1	1 * 4
2	2
3	1
4	4 * 2
5	0
6	2
7	4
8	4
9	4
10	0
11	0
12	0
13	0
14	0
15	2
16	2
17	1
      ----
       34


*/






