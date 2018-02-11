/*
 *
 * $Id: clan.c,v 1.6 2007/12/10 10:08:12 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: clan.c,v $
 * Revision 1.6  2007/12/10 10:08:12  devel
 * clan changes
 *
 * Revision 1.5  2007/08/21 22:07:33  devel
 * /relations and empty clans fixed
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

#define CLANHALLRENT	5

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
static int update_training(void);

struct clan clan[MAXCLAN];
static int update_time,update_state=0,version=0;

static int update_done=0,clan_changed=0;

static char *bonus_name[MAXBONUS]={
		"Pentagram Quest",	//0
		"Military Advisor",	//1
		"Merchant",		//2
		"unassigned",		//3
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned",
		"unassigned"		
};
int clan_get_training_score(int cnr)
{
	return clan[cnr].dungeon.training_score;
}
int score_to_level(int score)
{
	return score/100;
}

static void clan_standards(struct clan *c)
{
	int n;

	strcpy(c->rankname[0],"Member");
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
	int n,cnr,rank,cost=0;

	log_char(cn,LOG_SYSTEM,0,"Clan List");

        for (n=1; n<MAXCLAN; n++) {
		if (!clan[n].name[0]) continue;
		log_char(cn,LOG_SYSTEM,0,"\001%d \004%d jewels \010%s \030Raiding %s (+%d)",
			 n,
			 clan[n].treasure.jewels-(clan[n].treasure.debt/1000),
			 clan[n].name,
			 clan[n].dungeon.doraid ? "ON" : (clan[n].dungeon.raidonstart ? "PENDING" : "OFF"),
			score_to_level(clan[n].dungeon.training_score));
	}

        if ((cnr=get_char_clan(cn))) {
		rank=ch[cn].clan_rank;
		if (rank<0 || rank>4) { elog("showclan(): got illegal clan rank (%s,%d,%d)",ch[cn].name,cn,rank); rank=0; }
		
		log_char(cn,LOG_SYSTEM,0,"You are a member of the clan '%s' (%d), your rank is %s.",clan[cnr].name,cnr,clan[cnr].rankname[rank]);
		if (rank>0) {
			log_char(cn,LOG_SYSTEM,0,"Your clan has %d jewels, %.3f expenses per week, and a debt of %.3f jewels. Your clan's treasure holds %dG.",
				 clan[cnr].treasure.jewels,
				 clan[cnr].treasure.cost_per_week/1000.0,
                                 clan[cnr].treasure.debt/1000.0,
				 clan[cnr].depot.money);
	
			log_char(cn,LOG_SYSTEM,0,"%d +0 Warriors, %d +2 Warriors, %d +4 Warriors, %d +6 Warriors, %d +8 Warriors, %d +10 Warriors",
				 clan[cnr].dungeon.warrior[1][0],
				 clan[cnr].dungeon.warrior[1][1],
				 clan[cnr].dungeon.warrior[1][2],
				 clan[cnr].dungeon.warrior[1][3],
				 clan[cnr].dungeon.warrior[1][4],
				 clan[cnr].dungeon.warrior[1][5]);
			log_char(cn,LOG_SYSTEM,0,"%d +0 Mages, %d +2 Mages, %d +4 Mages, %d +6 Mages, %d +8 Mages, %d +10 Mages",
				 clan[cnr].dungeon.mage[1][0],
				 clan[cnr].dungeon.mage[1][1],
				 clan[cnr].dungeon.mage[1][2],
				 clan[cnr].dungeon.mage[1][3],
				 clan[cnr].dungeon.mage[1][4],
				 clan[cnr].dungeon.mage[1][5]);
	
			log_char(cn,LOG_SYSTEM,0,"%d +0 Seyans, %d +2 Seyans, %d +4 Seyans, %d +6 Seyans, %d +8 Seyans, %d +10 Seyans",
				 clan[cnr].dungeon.seyan[1][0],
				 clan[cnr].dungeon.seyan[1][1],
				 clan[cnr].dungeon.seyan[1][2],
				 clan[cnr].dungeon.seyan[1][3],
				 clan[cnr].dungeon.seyan[1][4],
				 clan[cnr].dungeon.seyan[1][5]);
	
			log_char(cn,LOG_SYSTEM,0,"%d teleports, %d fake walls, %d keys",
				 clan[cnr].dungeon.teleport[1],
				 clan[cnr].dungeon.fake[1],
				 clan[cnr].dungeon.key[1]);

			for (n=1; n<22; n++) {
                                if (n<7) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.warrior[1][n-1]);
				else if (n<13) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.mage[1][n-7]);
				else if (n<19) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.seyan[1][n-13]);
				else if (n==19) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.teleport[1]);
				else if (n==20) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.fake[1]);
				else if (n==21) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.key[1]);
			}

			log_char(cn,LOG_SYSTEM,0,"Used %d of 400 points.",cost);

			log_char(cn,LOG_SYSTEM,0,"Training score is at %d, guard levels are +%d, next update in %dm",
				clan[cnr].dungeon.training_score,
				score_to_level(clan[cnr].dungeon.training_score),
				60-(realtime-clan[cnr].dungeon.last_training_update)/60);
		}

		log_char(cn,LOG_SYSTEM,0,"Clan Website is at: %s",clan[cnr].website);
		log_char(cn,LOG_SYSTEM,0,"Clan message: %s",clan[cnr].message);

		for (n=0; n<MAXBONUS; n++) {
			if (clan[cnr].bonus[n].level) {
				log_char(cn,LOG_SYSTEM,0,"Bonus '%s' (%d) is at level %d.",bonus_name[n],n,clan[cnr].bonus[n].level);
			}
		}

		if (clan[cnr].dungeon.doraid) {
			log_char(cn,LOG_SYSTEM,0,"Raiding is ON.");
		} else {
			if (clan[cnr].dungeon.raidonstart) {
				log_char(cn,LOG_SYSTEM,0,"Raiding will be on in %.2f minutes.",((60*60*48)-(realtime-clan[cnr].dungeon.raidonstart))/60.0);
			} else {
				log_char(cn,LOG_SYSTEM,0,"Raiding is OFF.");
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
                return 0;
	}

        // dont erase clans because the serial numbers havent been read yet
	if (!update_done) return cnr;

        if (ch[cn].clan_serial!=clan[cnr].status.serial) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
		return 0;
	}
	if (!clan[cnr].name[0]) {
		ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;
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
	if (cnr<1 || cnr>31) return NULL;

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
	int res,size;
	void *tmp;

        switch(update_state) {
		case 0:         if (read_storage(1,version)) update_state++;
				break;
		case 1:		res=check_read_storage(&version,&tmp,&size);
				if (res==1) {
					if (size) {
                                                if (size!=sizeof(clan)) {
							elog("Incompatible Clan data found! (%d vs %d)",size,sizeof(clan));
							exit(1);
						} else memcpy(clan,tmp,size);
                                                xfree(tmp);
					}
					update_time=ticker;
					update_state++;
				} else if (res==-1) {
					elog("Could not read Clan data");
					update_state=2;
				}				
				break;
		case 2:		update_done=1;
				if (areaID!=3) {	// only area 3 updates clan info
					if (ticker>update_time+TICKS*10) update_state=0;
				} else update_state++;
				break;
		
		case 3:		if (!update_relations()) update_state++;
				break;
		case 4:		if (!update_treasure()) update_state++;
				update_training();
				break;
                case 5:        	if (ticker>update_time+TICKS*10) {
					update_state++;
					update_time=ticker;
				}
				break;
		case 6:		if (!clan_changed) {
					update_state=3;
					break;
				}
				if (update_storage(1,version,clan,sizeof(clan))) update_state++;
				break;				
		case 7:		res=check_update_storage();
				if (res==1) {
					version++;
					update_state=3;
                                        clan_changed=0;
				} else if (res==-1) {
					update_state=0;
					elog("clan update storage failed, data lost!");
				}
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

int found_clan(char *name,int cn,int *pclan)
{
	int n,serial;

        if (strlen(name)>78) return -1;
	if (!pclan) return -1;
	
	for (n=1; n<MAXCLAN; n++)
		if (!clan[n].name[0]) break;
	if (n==MAXCLAN) return -1;
		
	*pclan=n;
	serial=clan[n].status.serial;
	bzero(clan+n,sizeof(struct clan));
	strcpy(clan[n].name,name);
	clan[n].status.serial=serial;
	clan_standards(clan+n);
	zero_relation(n);
	clan_changed=1;

	add_clanlog(*pclan,clan_serial(*pclan),ch[cn].ID,1,"Clan was founded by %s",ch[cn].name);
	
	return 0;
}

int add_jewel(int nr,int cn)
{
        clan[nr].treasure.jewels++;
	clan_changed=1;

	add_clanlog(nr,clan_serial(nr),ch[cn].ID,25,"%s added a jewel",ch[cn].name);	
	
	return 0;
}

void swap_jewels(int nr1,int nr2,int cnt)
{
        if (cnt_jewels(nr1)<1) return;
	if (cnt_jewels(nr1)<cnt) cnt=cnt_jewels(nr1);
	
	clan[nr1].treasure.debt+=cnt*1000;
	clan[nr2].treasure.jewels+=cnt;
	clan_changed=1;
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
	if (nr==3 && !clan[cnr].dungeon.doraid) return -1;
	
        clan[cnr].bonus[nr].level=level;
	clan_changed=1;
	
	return 0;
}

int set_clan_raid(int cnr,int cn,int onoff)
{
	if (onoff && !clan[cnr].dungeon.doraid && !clan[cnr].dungeon.raidonstart) {
		clan[cnr].dungeon.raidonstart=realtime;
		add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,1,"%s set raiding to ON",ch[cn].name);
		clan_changed=1;
		return 0;
	}

	if (!onoff && !clan[cnr].dungeon.doraid && clan[cnr].dungeon.raidonstart) {
		clan[cnr].dungeon.raidonstart=0;
		add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,1,"%s canceled raiding",ch[cn].name);
		clan_changed=1;
		return 0;
	}

        return 1;
}

int set_clan_raid_god(int cnr,int cn,int onoff)
{
	if (onoff && !clan[cnr].dungeon.doraid) {
		clan[cnr].dungeon.raidonstart=0;
		clan[cnr].dungeon.doraid=1;
		add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,1,"%s set raiding to ON",ch[cn].name);
		clan_changed=1;
		return 0;
	}

	if (!onoff && clan[cnr].dungeon.doraid) {
		clan[cnr].dungeon.raidonstart=clan[cnr].dungeon.doraid=0;
		add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,1,"%s canceled raiding",ch[cn].name);
		clan_changed=1;
		return 0;
	}

        return 1;
}

int set_clan_website(int cnr,char *site,int cn)
{
        strncpy(clan[cnr].website,site,79); clan[cnr].website[79]=0;
	clan[cnr].website[strlen(clan[cnr].website)-1]=0;
	clan_changed=1;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,35,"%s set website %s",ch[cn].name,site);

	return 0;
}

int set_clan_message(int cnr,char *site,int cn)
{
        strncpy(clan[cnr].message,site,79); clan[cnr].message[79]=0;
	clan[cnr].message[strlen(clan[cnr].message)-1]=0;
	clan_changed=1;

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

int set_clan_dungeon_use(int cnr,int type,int number,int cn)
{
	int n,cost=0;

	if (cnr<1 || cnr>=MAXCLAN) return -1;
        if (type<1 || type>21) return -1;

        switch(type) {
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
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:	if (number<0 || number>10) return -1; break;	// warrior, mage, seyan +0...+5 each
		case 19:	if (number<0 || number>25) return -1; break; 	// teleport traps
		case 20:	if (number<0 || number>1) return -1; break; 	// fake walls
		case 21:	if (number<0 || number>2) return -1; break;	// keys
		default:	return -1;
	}

	for (n=1; n<22; n++) {
		if (n==type) cost+=get_clan_dungeon_cost(n,number);
                else if (n<7) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.warrior[1][n-1]);
		else if (n<13) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.mage[1][n-7]);
		else if (n<19) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.seyan[1][n-13]);
		else if (n==19) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.teleport[1]);
		else if (n==20) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.fake[1]);
		else if (n==21) cost+=get_clan_dungeon_cost(n,clan[cnr].dungeon.key[1]);
	}
	if (cost>400 && number>0) {
		return cost;
	}
	
	switch(type) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:		clan[cnr].dungeon.warrior[1][type-1]=number; break;
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:        clan[cnr].dungeon.mage[1][type-7]=number; break;
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:	clan[cnr].dungeon.seyan[1][type-13]=number; break;
		case 19:	clan[cnr].dungeon.teleport[1]=number; break;
		case 20:	clan[cnr].dungeon.fake[1]=number; break;
		case 21:	clan[cnr].dungeon.key[1]=number; break;
	}
	clan_changed=1;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,35,"%s set dungeon use of type %d to %d",ch[cn].name,type,number);
	
	return 0;
}

int get_clan_dungeon_cost(int type,int number)
{
        switch(type) {
		case 1:		return number*1;
		case 2:		return number*2;
		case 3:		return number*4;
		case 4:		return number*8;
		case 5:		return number*12;
		case 6:		return number*16;
		case 7:		return number*1;
		case 8:		return number*2;
		case 9:		return number*4;
		case 10:	return number*8;
		case 11:	return number*12;
		case 12:	return number*16;
		case 13:	return number*1;
		case 14:	return number*2;
		case 15:	return number*4;
		case 16:	return number*8;
		case 17:	return number*12;
		case 18:	return number*16;
		
		case 19:	return number*8;
		case 20:	return number*16;
		case 21:	return number*12;
	}

	elog("get_clan_dungeon_cost: unknown type %d",type);
	return 0;
}

int get_clan_dungeon(int cnr,int type)
{
	if (cnr<1 || cnr>=MAXCLAN) return 0;
	
	switch(type) {
		case 1:		return clan[cnr].dungeon.warrior[1][0];
		case 2:		return clan[cnr].dungeon.warrior[1][1];
		case 3:		return clan[cnr].dungeon.warrior[1][2];
		case 4:		return clan[cnr].dungeon.warrior[1][3];
		case 5:		return clan[cnr].dungeon.warrior[1][4];
		case 6:		return clan[cnr].dungeon.warrior[1][5];

		case 7:		return clan[cnr].dungeon.mage[1][0];
		case 8:		return clan[cnr].dungeon.mage[1][1];
		case 9:		return clan[cnr].dungeon.mage[1][2];
		case 10:	return clan[cnr].dungeon.mage[1][3];
		case 11:	return clan[cnr].dungeon.mage[1][4];
		case 12:	return clan[cnr].dungeon.mage[1][5];

		case 13:	return clan[cnr].dungeon.seyan[1][0];
		case 14:	return clan[cnr].dungeon.seyan[1][1];
		case 15:	return clan[cnr].dungeon.seyan[1][2];
		case 16:	return clan[cnr].dungeon.seyan[1][3];
		case 17:	return clan[cnr].dungeon.seyan[1][4];
		case 18:	return clan[cnr].dungeon.seyan[1][5];

		case 19:	return clan[cnr].dungeon.teleport[1];
		case 20:	return clan[cnr].dungeon.fake[1];
		case 21:	return clan[cnr].dungeon.key[1];
		default:	return 0;
	}
}

int set_clan_relation(int cnr,int onr,int rel,int cn)
{
        if (cnr<1 || cnr>=MAXCLAN) return -1;
	if (onr<1 || onr>=MAXCLAN) return -1;
	if (rel<1 || rel>5) return -1;
		
        if (clan[cnr].status.want_relation[onr]!=rel) clan[cnr].status.want_date[onr]=realtime;		
	clan[cnr].status.want_relation[onr]=rel;
	clan_changed=1;
	
        add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,22,"%s set relation to clan %s (%d) to %s (%d)",ch[cn].name,get_clan_name(onr),onr,rel_name[rel],rel);

	return 0;
}

int set_clan_rankname(int cnr,int rank,char *name,int cn)
{
	if (cnr<1 || cnr>=MAXCLAN) return -1;
	if (rank<0 || rank>=5) return -1;
	if (strlen(name)>37) return -1;

	strcpy(clan[cnr].rankname[rank],name);
	clan_changed=1;
	
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

	switch(clan[nr].status.current_relation[cnr]) {		// rest depends on relations
		case CS_ALLIANCE:	return 1;
		//case CS_WAR:		return 1;
		//case CS_FEUD:		return 1;
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

static int update_relations(void)
{
	int n,m;
	int want1,want2,cur,diff1,diff2;

	for (n=1; n<MAXCLAN; n++) {
		if (!clan[n].name[0]) continue;

		if (!clan[n].dungeon.doraid) {
                        clan[n].dungeon.doraid=1;
			clan_changed=1;
			xlog("turned raiding on for clan %d (%s)",n,clan[n].name);
			add_clanlog(n,clan_serial(n),0,1,"Raiding enabled");
		}

                for (m=1; m<MAXCLAN; m++) {
			if (n==m) continue;
			if (!clan[m].name[0]) continue;

			if (!clan[n].dungeon.doraid) {
				clan[n].status.current_relation[m]=min(CS_NEUTRAL,clan[n].status.current_relation[m]);
				clan[n].status.want_relation[m]=min(CS_NEUTRAL,clan[n].status.want_relation[m]);
			}

			if (!clan[m].dungeon.doraid) {
				clan[n].status.current_relation[m]=min(CS_NEUTRAL,clan[n].status.current_relation[m]);
				clan[n].status.want_relation[m]=min(CS_NEUTRAL,clan[n].status.want_relation[m]);
			}

			want1=clan[n].status.want_relation[m];
			diff1=realtime-clan[n].status.want_date[m];
			want2=clan[m].status.want_relation[n];
			diff2=realtime-clan[m].status.want_date[n];
			cur=clan[n].status.current_relation[m];

			if (want1==want2 && cur==want1) continue;	// both want what they have, no work

			if (want1==want2) {	// both want the same, set it
				clan[n].status.current_relation[m]=clan[m].status.current_relation[n]=want1;
				clan_changed=1;
				add_clanlog(n,clan_serial(n),0,10,"%s with %s (%d) started",rel_name[want1],get_clan_name(m),m);
				add_clanlog(m,clan_serial(m),0,10,"%s with %s (%d) started",rel_name[want1],get_clan_name(n),n);
				continue;
			}
			
			switch(cur) {
				case CS_ALLIANCE:	// we are at alliance, but one of them wants out
							if (want1>CS_ALLIANCE && diff1>60*60*24) {
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Alliance with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Alliance with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want2>CS_ALLIANCE && diff2>60*60*24) {
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Alliance with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Alliance with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
				case CS_PEACETREATY:	// we're at treaty, going up to alliance is only possible if both want
							// it, and thats dealt with above. therefore, one of them wants out
							if (want1>CS_PEACETREATY && diff1>60*60*24) {
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want2>CS_PEACETREATY && diff2>60*60*24) {
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Peace Treaty with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
				case CS_NEUTRAL:	// we're at neutral, lets see
							if (want1>CS_NEUTRAL && want2>CS_NEUTRAL) {	// both want at least war, ok without timecheck
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) started",get_clan_name(n),n);
								break;
							}
							if (want1>CS_NEUTRAL && diff1>60*60) {	// nr1 wants war, needs 1 hour
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) started",get_clan_name(n),n);
								break;
							}
							if (want2>CS_NEUTRAL && diff2>60*60) {	// nr2 wants war, needs 1 hour
								cur++; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) started",get_clan_name(n),n);
								break;
							}
							if (want1<CS_NEUTRAL && want2<CS_NEUTRAL) {	// both want a better status, ok at once
								cur--; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Peace Treaty with %s (%d) started",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Peace Treaty with %s (%d) started",get_clan_name(n),n);
								break;
							}
							break;
				case CS_WAR:		// we're at war. changing to feud can only happen if both want it
							if (want1<CS_WAR && want2<CS_WAR) {	// both want a better status, ok at once
								cur--; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"War with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"War with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							break;
				case CS_FEUD:		// we have a feud, but at least one of them wants out
							if (want1<CS_FEUD && want2<CS_FEUD) {	// both want a better status, ok at once
								cur--; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Feud with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Feud with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want1<CS_FEUD && diff1>60*60*24) {	// nr1 wants out, needs 24 hours
								cur--; clan_changed=1;
								add_clanlog(n,clan_serial(n),0,10,"Feud with %s (%d) ended",get_clan_name(m),m);
								add_clanlog(m,clan_serial(m),0,10,"Feud with %s (%d) ended",get_clan_name(n),n);
								break;
							}
							if (want2<CS_FEUD && diff2>60*60*24) {	// nr2 wants out, needs 24 hours
								cur--; clan_changed=1;
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

static void reduce_clan_bonus(int cnr)
{
	int bestl=0,bestn=0,n;

	for (n=0; n<MAXBONUS; n++) {
		if (clan[cnr].bonus[n].level>bestl) { bestl=clan[cnr].bonus[n].level; bestn=n; }		
	}
	if (bestl) {
		clan[cnr].bonus[bestn].level--;
	}
}

static int update_treasure(void)
{
	int n,cost,cnr,step,diff;

        for (cnr=1; cnr<MAXCLAN; cnr++) {
		if (!clan[cnr].name[0]) continue;
			
		do {
			cost=0;
			for (n=0; n<MAXBONUS; n++) {
				cost+=clan[cnr].bonus[n].level*1000;
			}

			if (cost/250>clan[cnr].treasure.jewels) {
				reduce_clan_bonus(cnr);
			}

		} while (cost>0 && cost/250>clan[cnr].treasure.jewels);

		cost+=CLANHALLRENT*1000;

                if (clan[cnr].treasure.cost_per_week!=cost) {
			clan[cnr].treasure.cost_per_week=cost;
			clan_changed=1;
		}

		diff=realtime-clan[cnr].treasure.payed_till;
		if (diff>60*5) {	// update 5 minutes late to reduce load
			step=(60*60*24*7)/cost;
			n=diff/step+1;
			clan[cnr].treasure.debt+=n;
			clan[cnr].treasure.payed_till+=step*n;
			clan_changed=1;
		}
                if (clan[cnr].treasure.debt>=1000 && clan[cnr].treasure.jewels>0) {
			n=clan[cnr].treasure.debt/1000;
			if (n>clan[cnr].treasure.jewels) { n=clan[cnr].treasure.jewels; clan[cnr].treasure.jewels=0; }
			else clan[cnr].treasure.jewels-=n;

			clan[cnr].treasure.debt-=n*1000;
			clan_changed=1;
			xlog("clan %s, paid %d jewels",clan[cnr].name,n);
		}
		
		if (clan[cnr].treasure.debt>=2000) {
			xlog("clan %s is broke, removing",clan[cnr].name);
			add_clanlog(cnr,clan_serial(cnr),0,1,"Clan %s went broke and was deleted",get_clan_name(cnr));
			clan[cnr].name[0]=0;
			clan[cnr].status.serial++;
			clan_changed=1;
		}
	}
	
        return 0;
}

static int update_training(void)
{
	int cnr;

        for (cnr=1; cnr<MAXCLAN; cnr++) {
		if (!clan[cnr].name[0]) continue;
		if (realtime-clan[cnr].dungeon.last_training_update<60*60) continue;
		
		clan[cnr].dungeon.last_training_update=realtime;
		clan[cnr].dungeon.training_score=(int)(clan[cnr].dungeon.training_score*0.95f);
		xlog("clan %d, new training score %d",cnr,clan[cnr].dungeon.training_score);
		clan_changed=1;		
	}
	
        return 0;
}

void add_member(int cn,int cnr,char *master)
{
	int n;

	ch[cn].clan=cnr;
	ch[cn].clan_serial=clan[cnr].status.serial;

	add_clanlog(cnr,clan_serial(cnr),ch[cn].ID,15,"%s was added to clan by %s",ch[cn].name,master);

	for (n=1; n<MAXPLAYER; n++)
		set_player_knows_name(n,cn,0);
}

void remove_member(int cn,int master_cn)
{
	int n;

	if (ch[cn].clan<CLUBOFFSET) add_clanlog(ch[cn].clan,clan_serial(ch[cn].clan),ch[cn].ID,15,"%s was fired from clan by %s",ch[cn].name,ch[master_cn].name);

	ch[cn].clan=ch[cn].clan_rank=ch[cn].clan_serial=0;

	for (n=1; n<MAXPLAYER; n++)
		set_player_knows_name(n,cn,0);
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
	clan_changed=1;
}

/*int clan_stolen_jewel_nr(int cnr,int onr,int level)
{
	int cnt;

	cnt=10;					// 10 is absolute max
	cnt=min(cnt,cnt_jewels(cnr)/10);	// not more than defender jewels/10
	cnt=min(cnt,cnt_jewels(onr)/10);	// not more than attacker jewels/10
	cnt=min(cnt,level/5);			// not more then attacker level/5
	cnt=max(cnt,2);				// but at least 2
        cnt=min(cnt,cnt_jewels(cnr)-2);		// but leave 2 jewels for the defender
	cnt=max(cnt,0);				// no negative results
	
	if (cnt_jewels(onr)<3) cnt=0;		// dont allow clans with 2 jewels to steal any

	return cnt;
}*/

void clan_dungeon_chat(char *ptr)
{
	int cnr,level,onr,cnt,cID,nr,str;
	char type;

	if (areaID!=3) return;

	xlog("dungeon_chat: %s",ptr);
	
	cnr=atoi(ptr);
	if (cnr<1 || cnr>=MAXCLAN) return;
	if (strlen(ptr)<4) return;

	type=ptr[3];

	switch(type) {
		case 'T':	if (clan[cnr].dungeon.teleport[0]>0) {
					clan[cnr].dungeon.teleport[0]--;
					clan_changed=1;
				}
				break;
		case 'F':	if (clan[cnr].dungeon.fake[0]>0) {
					clan[cnr].dungeon.fake[0]--;
					clan_changed=1;
				}
				break;
		case 'K':	if (clan[cnr].dungeon.key[0]>0) {
					clan[cnr].dungeon.key[0]--;
					clan_changed=1;
				}
				break;

                case 'W':	if (strlen(ptr)<6) return;
				level=atoi(ptr+5);
                                if (level>=0 && level<=5 && clan[cnr].dungeon.warrior[0][level]>0) {
					clan[cnr].dungeon.warrior[0][level]--;
					clan_changed=1;
					//xlog("clan %d lost warrior %d",cnr,level);
				}
				break;
		case 'M':	if (strlen(ptr)<6) return;
				level=atoi(ptr+5);
                                if (level>=0 && level<=5 && clan[cnr].dungeon.mage[0][level]>0) {
					clan[cnr].dungeon.mage[0][level]--;
					clan_changed=1;
				}
				break;
		case 'S':	if (strlen(ptr)<6) return;
				level=atoi(ptr+5);
                                if (level>=0 && level<=5 && clan[cnr].dungeon.seyan[0][level]>0) {
					clan[cnr].dungeon.seyan[0][level]--;
					clan_changed=1;
				}
				break;
		case 'J':	if (strlen(ptr)<19) return;
				// sprintf(buf,"%02d:J:%02d:%03d:%010u:%s",cnr,onr,ch[cn].ID,ch[cn].name);
				// 00:J:00:0000000000:Name
				// 03:J:04:0000000001:Ishtar
				// 012345678901234567890
                                onr=atoi(ptr+5);
				level=atoi(ptr+8);
				cID=atoi(ptr+12);

                                cnt=min(cnt_jewels(cnr)-11,3);
				if (cnt_jewels(onr)<10) { return; }

				if (cnt>0) {
					xlog("clan %d: clan %d stole %d jewels (level=%d)",cnr,onr,cnt,level);
					//swap_jewels(cnr,onr,cnt);
					clan[cnr].dungeon.training_score+=150;
					clan[cnr].treasure.debt+=cnt*1000+1000;
					clan[onr].treasure.jewels+=cnt;
					clan_changed=1;
	
					add_clanlog(cnr,clan_serial(cnr),cID,5,"Clan was raided by %s of %s (%d) for %d jewels",ptr+19,get_clan_name(onr),onr,cnt);
					add_clanlog(onr,clan_serial(onr),cID,5,"%s raided clan %s (%d) for %d jewels",ptr+19,get_clan_name(cnr),cnr,cnt);
				}
				break;
		case 's':	// %02d:s:%01d:%01d
				if (strlen(ptr)<8) return;
				nr=atoi(ptr+5); str=atoi(ptr+7);
                                if (clan[cnr].dungeon.simple_pot[nr][str]>0) {
					clan[cnr].dungeon.simple_pot[nr][str]--;
					clan_changed=1;
					xlog("clan %d lost simple potion: %d %d (%s)",cnr,nr,str,ptr);
				}				
				break;
		case 'a':	// %02d:s:%01d:%01d
				if (strlen(ptr)<8) return;
				nr=atoi(ptr+5); str=atoi(ptr+7);
                                if (clan[cnr].dungeon.alc_pot[nr][str]>0) {
					clan[cnr].dungeon.alc_pot[nr][str]--;
					clan_changed=1;
					xlog("clan %d lost alc potion: %d %d (%s)",cnr,nr,str,ptr);
				}				
				break;
		case 'X':	// %02d:X:%02d:%10u:%s
				// 01:X:01:0123456789:Ishtar
				clan[cnr].treasure.jewels+=1;
				clan_changed=1;
                                level=atoi(ptr+5);
				cID=atoi(ptr+8);
				xlog("clan %d: %s won a jewel from level % d spawn",cnr,ptr+19,level);

				add_clanlog(cnr,clan_serial(cnr),cID,5,"%s won a jewel from level %d spawn",ptr+19,level);
	
	}
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

void show_clan_pots(int cn)
{
	int cnr,n;
	static char *psize[]={"Small","Medium","Big"};

	if (!(cnr=get_char_clan(cn))) { log_char(cn,LOG_SYSTEM,0,"Only for clan members."); return; }
	if (ch[cn].clan_rank<1) { log_char(cn,LOG_SYSTEM,0,"Not of sufficient rank."); return; }
	
        for (n=0; n<6; n++) {
		log_char(cn,LOG_SYSTEM,0,"Attack, Parry, Immunity+%d: \016%d",n*4+4,clan[cnr].dungeon.alc_pot[0][n]);		
	}
	for (n=0; n<6; n++) {
		log_char(cn,LOG_SYSTEM,0,"Flash, Magic Shield, Immunity+%d: \016%d",n*4+4,clan[cnr].dungeon.alc_pot[1][n]);
	}
	for (n=0; n<3; n++) {
		log_char(cn,LOG_SYSTEM,0,"%s healing potions: \016%d",psize[n],clan[cnr].dungeon.simple_pot[0][n]);
	}
	for (n=0; n<3; n++) {
		log_char(cn,LOG_SYSTEM,0,"%s mana potions: \016%d",psize[n],clan[cnr].dungeon.simple_pot[1][n]);
	}
	for (n=0; n<3; n++) {
		log_char(cn,LOG_SYSTEM,0,"%s combo potions: \016%d",psize[n],clan[cnr].dungeon.simple_pot[2][n]);
	}
}

int add_alc_potion(int nr,int in)
{
	int str;

        if (it[in].driver!=IDR_FLASK) return -1;
	
        if (it[in].mod_index[0]==V_ATTACK && it[in].mod_index[1]==V_PARRY && it[in].mod_index[2]==V_IMMUNITY) {
		str=min(5,(it[in].mod_value[0]/4)-1);
		clan[nr].dungeon.alc_pot[0][str]++;
		clan_changed=1;	
		return 0;
	}
	if (it[in].mod_index[0]==V_FLASH && it[in].mod_index[1]==V_MAGICSHIELD && it[in].mod_index[2]==V_IMMUNITY) {
		str=min(5,(it[in].mod_value[0]/4)-1);
		clan[nr].dungeon.alc_pot[1][str]++;
		clan_changed=1;	
		return 0;
	}
	
	return -1;
}

int add_simple_potion(int nr,int co)
{
	int n,in,flag,cnt=0;

	for (n=30; n<INVENTORYSIZE; n++) {
       		if ((in=ch[co].item[n]) && it[in].driver==IDR_POTION) {
			flag=0;
			//xlog("%d %d %d",it[in].drdata[1],it[in].drdata[2],it[in].drdata[3]);

			if (it[in].drdata[1]==8 && !it[in].drdata[2] && !it[in].drdata[3]) {
				flag=1;
				clan[nr].dungeon.simple_pot[0][0]++;
			}
			if (it[in].drdata[1]==16 && !it[in].drdata[2] && !it[in].drdata[3]) {
				flag=1;
				clan[nr].dungeon.simple_pot[0][1]++;
			}
			if (it[in].drdata[1]==32 && !it[in].drdata[2] && !it[in].drdata[3]) {
				flag=1;
				clan[nr].dungeon.simple_pot[0][2]++;
			}
			if (it[in].drdata[2]==8 && !it[in].drdata[1] && !it[in].drdata[3]) {
				flag=1;
				clan[nr].dungeon.simple_pot[1][0]++;
			}
			if (it[in].drdata[2]==16 && !it[in].drdata[1] && !it[in].drdata[3]) {
				flag=1;
				clan[nr].dungeon.simple_pot[1][1]++;
			}
			if (it[in].drdata[2]==32 && !it[in].drdata[1] && !it[in].drdata[3]) {
				flag=1;
				clan[nr].dungeon.simple_pot[1][2]++;
			}

			if (it[in].drdata[1]==8 && it[in].drdata[2]==8 && it[in].drdata[3]==8) {
				flag=1;
				clan[nr].dungeon.simple_pot[2][0]++;
			}
			if (it[in].drdata[1]==16 && it[in].drdata[2]==16 && it[in].drdata[3]==16) {
				flag=1;
				clan[nr].dungeon.simple_pot[2][1]++;
			}
			if (it[in].drdata[1]==32 && it[in].drdata[2]==32 && it[in].drdata[3]==32) {
				flag=1;
				clan[nr].dungeon.simple_pot[2][2]++;
			}
			if (flag) {
				ch[co].item[n]=0;
				destroy_item(in);
				ch[co].flags|=CF_ITEMS;
				clan_changed=1;
				cnt++;
			}
		}
	}
	return cnt;
}

int get_clan_ready()
{
	return update_done;
}

int get_clan_raid(int cnr)
{
	return clan[cnr].dungeon.doraid;
}

int clan_trade_bonus(int cn)
{
	int cnr;

	if (!(cnr=get_char_clan(cn))) return 0;

	return (int)(get_clan_bonus(cnr,2)*7.5);
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



