/*

$Id: statistics.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: statistics.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:51  sam
Added RCS tags


*/

#include <string.h>

#include "server.h"
#include "date.h"
#include "drdata.h"
#include "statistics.h"

void stats_update(int cn,int onl,int gold)
{
	struct stats_ppd *ppd;
	int idx,lidx;

	ppd=set_data(cn,DRD_STATS_PPD,sizeof(struct stats_ppd));
	if (!ppd) return;

	idx=(realtime/RESOLUTION)%MAXSTAT;
	lidx=(ppd->last_update/RESOLUTION)%MAXSTAT;

        // delete current slot if we didnt write to it before (to avoid counting the previous circle)
	while (lidx!=idx) {
		lidx=(lidx+1)%MAXSTAT;
		bzero(ppd->stats+lidx,sizeof(ppd->stats[0]));
	}
	//if (idx!=lidx) bzero(ppd->stats+idx,sizeof(ppd->stats[0]));

	ppd->last_update=realtime;

	ppd->stats[idx].exp=ch[cn].exp;
	ppd->stats[idx].gold+=gold;
	ppd->stats[idx].online+=onl;	
}

int stats_online_time(int cn)
{
	struct stats_ppd *ppd;
	int m,n;

	ppd=set_data(cn,DRD_STATS_PPD,sizeof(struct stats_ppd));
	if (!ppd) return 0;

	for (n=m=0; n<MAXSTAT; n++) {
		m+=ppd->stats[n].online;
	}

	return m;
}
