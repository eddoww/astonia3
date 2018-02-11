/*

$Id: task.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: task.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:57  sam
Added RCS tags


*/

int process_task(unsigned char *taskdata);
int task_set_clan_rank(int target_ID,int master_ID,int clan,int rank,char *master_name);
int task_fire_from_clan(int target_ID,int master_ID,int clan,char *master_name);
int task_punish_player(int target_ID,int master_ID,int level,char *reason);
int task_unpunish_player(int target_ID,int master_ID,int ID);
int task_set_flags(int target_ID,int master_ID,unsigned long long flags);
