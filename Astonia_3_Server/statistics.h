/*

$Id: statistics.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: statistics.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:51  sam
Added RCS tags


*/

#define MAXSTAT	365
#define RESOLUTION (60*60*24)

struct stats
{
	int exp;
	int gold;
	int online;
};

struct stats_ppd
{
	int last_update;
	struct stats stats[MAXSTAT];
};

void stats_update(int cn,int onl,int gold);
int stats_online_time(int cn);
