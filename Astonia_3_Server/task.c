/*

$Id: task.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: task.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:56  sam
Added RCS tags


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

#define CHARINFO

int mysql_query_con(MYSQL *my,const char *query);
void mysql_free_result_cnt(MYSQL_RES *result);
MYSQL_RES *mysql_store_result_cnt(MYSQL *my);

#include "server.h"
#include "log.h"
#include "database.h"
#include "create.h"
#include "punish.h"
#include "chat.h"
#include "player.h"
#include "clan.h"
#include "task.h"
#include "club.h"

extern MYSQL mysql;

struct task_data
{
	int tasknr;
	int uID;
};

struct set_clan_rank_data
{
	int tasknr;
	int target_ID;
	int master_ID;
	int clan;
	int rank;
	char master_name[80];
};

struct fire_from_clan_data
{
	int tasknr;
	int target_ID;
	int master_ID;
	int clan;
	char master_name[80];
};

struct punish_player_data
{
	int tasknr;
	int target_ID;
	int master_ID;
	int level;
	char reason[80];
};

struct unpunish_player_data
{
	int tasknr;
	int target_ID;
	int master_ID;
	int ID;
};

struct set_flags
{
	int tasknr;
	int target_ID;
	int master_ID;
	unsigned long long flags;	// will use xor
};


// --------- called by database thread --------------------
// player table is locked already and must not be unlocked
// no server functions may be called unless they are known
// to be threadsafe or marked as doing locking

int set_clan_rank(struct set_clan_rank_data *set,struct character *tmp,int *plock,int *pkick)
{
        if (plock) *plock=0;
	if (pkick) *pkick=0;

	if (set->clan<CLUBOFFSET) {
		if (tmp->clan!=set->clan) {
			tell_chat(0,set->master_ID,1,"%s is not a member of your clan, you cannot set the rank.",tmp->name);
			return 0;
		}
		if (!(tmp->flags&CF_PAID) && set->rank>1) {
			tell_chat(0,set->master_ID,1,"%s is not a paying player, you cannot set the rank higher than 1.",tmp->name);
			return 0;
		}
		tmp->clan_rank=set->rank;
	
		add_clanlog(set->clan,clan_serial(set->clan),tmp->ID,30,"%s rank was set to %d by %s",tmp->name,set->rank,set->master_name);
	
		tell_chat(0,set->master_ID,1,"Set %s's rank to %d.",tmp->name,set->rank);
	} else {
		if (tmp->clan!=set->clan) {
			tell_chat(0,set->master_ID,1,"%s is not a member of your club, you cannot set the rank.",tmp->name);
			return 0;
		}
		if (!(tmp->flags&CF_PAID) && set->rank>0) {
			tell_chat(0,set->master_ID,1,"%s is not a paying player, you cannot set the rank higher than 0.",tmp->name);
			return 0;
		}
		if (tmp->clan_rank==2) {
			tell_chat(0,set->master_ID,1,"%s is the club's founder, can't change rank.",tmp->name);
			return 0;
		}
		tmp->clan_rank=set->rank;
	
		tell_chat(0,set->master_ID,1,"Set %s's rank to %d.",tmp->name,set->rank);
	}

        return 1;
}

int fire_from_clan(struct fire_from_clan_data *set,struct character *tmp,int *plock,int *pkick)
{
	if (plock) *plock=0;
	if (pkick) *pkick=0;

	if (set->clan<CLUBOFFSET) {
		if (tmp->clan!=set->clan) {
			tell_chat(0,set->master_ID,1,"%s is not a member of your clan, you cannot fire him/her.",tmp->name);
			return 0;
		}
		tmp->clan=0;
		tmp->clan_rank=0;
	
		add_clanlog(set->clan,clan_serial(set->clan),set->target_ID,15,"%s was fired by %s",tmp->name,set->master_name);
	
		tell_chat(0,set->master_ID,1,"Fired %s.",tmp->name);	
	} else {
		if (tmp->clan!=set->clan) {
			tell_chat(0,set->master_ID,1,"%s is not a member of your club, you cannot fire him/her.",tmp->name);
			return 0;
		}
		if (tmp->clan_rank>1) {
			tell_chat(0,set->master_ID,1,"You cannot fire %s, he is the founder of the club.",tmp->name);
			return 0;
		}
		tmp->clan=0;
		tmp->clan_rank=0;
	
		tell_chat(0,set->master_ID,1,"Fired %s.",tmp->name);	
	}

        return 1;
}

void punish_player(struct punish_player_data *set,struct character *tmp,int *plock,int *pkick)
{
	if (punish(set->master_ID,tmp,set->level,set->reason,plock,pkick)) {
		tell_chat(0,set->master_ID,1,"Punished %s with a level %d punishment for %s",tmp->name,set->level,set->reason);
		if (set->level) tell_chat(0,set->target_ID,1,"°c03You have just been punished for %s. You have lost experience and karma. Your karma is now down to %d. If your karma reaches %d, you will be banned from this game.",set->reason,tmp->karma,(tmp->flags&CF_PAID) ? -12 : -5);
		else tell_chat(0,set->target_ID,1,"°c03You have been warned for %s. You will not be warned again. Next time you will lose experience and karma.",set->reason);
	}
}

// unpunish needs a rewrite: it is using the database while the server is locked!!
void unpunish_player(struct unpunish_player_data *set,struct character *tmp,int *plock,int *pkick)
{
	if (unpunish(set->master_ID,tmp,set->ID,plock,pkick)) {
		tell_chat(0,set->master_ID,1,"UnPunished %s ID %d.",tmp->name,set->ID);		
	}
}

int set_flags(struct set_flags *set,struct character *tmp,int *plock,int *pkick)
{
        if (plock) *plock=0;
	if (pkick) *pkick=0;

        tmp->flags^=set->flags;

        tell_chat(0,set->master_ID,1,"Set flag on %s to %s.",tmp->name,(tmp->flags&set->flags) ? "on" : "off");

        return 1;
}


int set_task(struct task_data *set,int (*proc)(void*,struct character*,int *,int *))
{
	MYSQL_RES *result;
	MYSQL_ROW row;	
	unsigned long *len;
	int cn,lock,kick;
	char data[sizeof(struct character)*2];
	char buf[sizeof(data)+256];
	struct character tmp;

	xlog("set task: start for ID=%d, task=%d",set->uID,set->tasknr);

	// character online here?
	lock_server();
	for (cn=getfirst_char(); cn; cn=getnext_char(cn)) {
		if (ch[cn].ID==set->uID) break;
	}
	if (cn) {	// yes: set data and delete task
		xlog("set task: found online");
		
                proc(set,ch+cn,&lock,&kick);
		if (lock>0 && ch[cn].player) {
			kick_player(ch[cn].player,"You have been locked as a result of your punishment.");
			xlog("kicked %s as part of punishment",ch[cn].name);
		}
		if (kick>0 && ch[cn].player) {
			kick_player(ch[cn].player,"You have been locked as a result of your punishment.");
			xlog("kicked %s as part of punishment",ch[cn].name);
		}
                unlock_server();

		if (lock>0) {
			xlog("locked character with ID=%d",set->uID);
			sprintf(buf,"update chars set locked='Y' where ID=%d",set->uID);
			if (mysql_query_con(&mysql,buf)) {
				elog("Failed to update account ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
			}
#ifdef CHARINFO
			sprintf(buf,"update charinfo set locked='Y' where ID=%d",set->uID);
			if (mysql_query_con(&mysql,buf)) {
				elog("Failed to update charinfo ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
			}
#endif
		}
		if (lock<0) {
			xlog("unlocked character with ID=%d",set->uID);
			sprintf(buf,"update chars set locked='N' where ID=%d",set->uID);
			if (mysql_query_con(&mysql,buf)) {
				elog("Failed to update account ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
			}
#ifdef CHARINGFO
			sprintf(buf,"update charinfo set locked='N' where ID=%d",set->uID);
			if (mysql_query_con(&mysql,buf)) {
				elog("Failed to update charinfo ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
			}
#endif
		}

		return 1;
	} else unlock_server();

	xlog("set task: checking DB for ID=%d, task=%d",set->uID,set->tasknr);
	// no? check if not online anywhere else and adjust data in database
	
	// read the data we need
        sprintf(buf,"select chr,current_area from chars where ID=%d",set->uID);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to select account ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
	}
        if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
	}
	if (mysql_num_rows(result)==0) {
		elog("set_task: num_rows returned 0");
		mysql_free_result_cnt(result);
                return 0;
	}
        if (!(row=mysql_fetch_row(result))) {
		elog("set_task: fetch_row returned NULL");
		mysql_free_result_cnt(result);
                return 0;
	}
	if (!row[0] || !row[1]) {
		elog("set_task: one of the values NULL");
		mysql_free_result_cnt(result);
                return 0;
	}

	if (atoi(row[1])) {	// player is online somewhere, leave
		mysql_free_result_cnt(result);
                xlog("set task: is online somewhere else");
                return 0;
	}
	xlog("set task: updating for ID=%d, task=%d",set->uID,set->tasknr);

	len=mysql_fetch_lengths(result);

	bzero(&tmp,sizeof(tmp));
	memcpy(&tmp,row[0],min(len[0],sizeof(struct character)));

	mysql_free_result_cnt(result);

	if (proc(set,&tmp,&lock,&kick)) {

		xlog("set task: lock=%d, kick=%d for ID=%d, task=%d",lock,kick,set->uID,set->tasknr);

		mysql_real_escape_string(&mysql,data,(void*)&tmp,sizeof(struct character));
	
		if (lock>0) sprintf(buf,"update chars set chr='%s', locked='Y',karma=%d,clan=%d,clan_rank=%d,experience=%d where ID=%d",
				    data,
				    tmp.karma,
				    tmp.clan,
				    tmp.clan_rank,
				    tmp.exp,
				    set->uID);
                else if (lock<0) sprintf(buf,"update chars set chr='%s',karma=%d,clan=%d,clan_rank=%d,experience=%d,locked='N' where ID=%d",
                                         data,
                                         tmp.karma,
					 tmp.clan,
					 tmp.clan_rank,
					 tmp.exp,
                                         set->uID);
		else sprintf(buf,"update chars set chr='%s',karma=%d,clan=%d,clan_rank=%d,experience=%d where ID=%d",
                             data,
			     tmp.karma,
			     tmp.clan,
			     tmp.clan_rank,
			     tmp.exp,
			     set->uID);
	
		if (mysql_query_con(&mysql,buf)) {
			elog("Failed to update account ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
			return 0;
		}
#ifdef CHARINFO
		if (lock>0) sprintf(buf,"update charinfo set locked='Y',karma=%d,clan=%d,clan_rank=%d,experience=%d where ID=%d",
                                    tmp.karma,
                                    tmp.clan,
				    tmp.clan_rank,
                                    tmp.exp,
                                    set->uID);
                else if (lock<0) sprintf(buf,"update charinfo set karma=%d,clan=%d,clan_rank=%d,experience=%d,locked='N' where ID=%d",
                                         tmp.karma,
                                         tmp.clan,
					 tmp.clan_rank,
                                         tmp.exp,
                                         set->uID);
		else sprintf(buf,"update charinfo set karma=%d,clan=%d,clan_rank=%d,experience=%d where ID=%d",
                             tmp.karma,
			     tmp.clan,
			     tmp.clan_rank,
			     tmp.exp,
			     set->uID);
	
		if (mysql_query_con(&mysql,buf)) {
			elog("Failed to update charinfo ID=%d: Error: %s (%d)",set->uID,mysql_error(&mysql),mysql_errno(&mysql));
		}
#endif
		xlog("set task: db update done for ID=%d, task=%d",set->uID,set->tasknr);
	}

        xlog("set task: done for ID=%d, task=%d",set->uID,set->tasknr);

	return 1;
}

int process_task(unsigned char *taskdata)
{
	int task,del;

	task=*(unsigned int*)(taskdata);

	switch(task) {
		case 0:		xlog("task test"); del=1; break;
		case 1:		del=set_task((void*)(taskdata),(int (*)(void*,struct character *,int*,int*))set_clan_rank); break;
		case 2:		del=set_task((void*)(taskdata),(int (*)(void*,struct character *,int*,int*))fire_from_clan); break;
		case 3:		del=set_task((void*)(taskdata),(int (*)(void*,struct character *,int*,int*))punish_player); break;
		case 4:		del=set_task((void*)(taskdata),(int (*)(void*,struct character *,int*,int*))unpunish_player); break;
		case 5:		del=set_task((void*)(taskdata),(int (*)(void*,struct character *,int*,int*))set_flags); break;

		default:	elog("deleting unknown task %d",task); del=1; break;
	}

	return del;
}

// --------- called by server thread ---------------
int task_set_clan_rank(int target_ID,int master_ID,int clan,int rank,char *master_name)
{
	struct set_clan_rank_data set;

	set.tasknr=1;
	set.target_ID=target_ID;
	set.master_ID=master_ID;
	set.clan=clan;
	set.rank=rank;
	strcpy(set.master_name,master_name);

	return add_task(&set,sizeof(set));
}

int task_fire_from_clan(int target_ID,int master_ID,int clan,char *master_name)
{
	struct fire_from_clan_data set;

	set.tasknr=2;
	set.target_ID=target_ID;
	set.master_ID=master_ID;
	set.clan=clan;
	strcpy(set.master_name,master_name);

	return add_task(&set,sizeof(set));
}

int task_punish_player(int target_ID,int master_ID,int level,char *reason)
{
	struct punish_player_data set;
	int ret;

        set.tasknr=3;
	set.target_ID=target_ID;
	set.master_ID=master_ID;
	set.level=level;
	strncpy(set.reason,reason,79); set.reason[79]=0;

	ret=add_task(&set,sizeof(set));

        return ret;
}

int task_unpunish_player(int target_ID,int master_ID,int ID)
{
	struct unpunish_player_data set;

	set.tasknr=4;
	set.target_ID=target_ID;
	set.master_ID=master_ID;
	set.ID=ID;

	return add_task(&set,sizeof(set));
}

int task_set_flags(int target_ID,int master_ID,unsigned long long flags)
{
	struct set_flags set;

	set.tasknr=5;
	set.target_ID=target_ID;
	set.master_ID=master_ID;
	set.flags=flags;

	return add_task(&set,sizeof(set));
}
