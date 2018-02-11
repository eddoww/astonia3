/*

$Id: tell.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: tell.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:57  sam
Added RCS tags


*/

#include <string.h>

#include "server.h"
#include "drdata.h"
#include "lookup.h"
#include "talk.h"
#include "tell.h"

#define MAXTELL	10

struct tell_data
{
	int target[MAXTELL];
	int time[MAXTELL];
};

void register_sent_tell(int cn,int coID)
{
	struct tell_data *dat;
	int n,empty=-1;

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(dat=set_data(cn,DRD_TELL_DATA,sizeof(struct tell_data)))) return;	// OOPS

	for (n=0; n<10; n++) {
		if (dat->target[n]==coID) return;		
		if (dat->target[n]==0 && empty==-1) empty=n;		
	}
	if (empty!=-1) {	
		dat->target[empty]=coID;
		dat->time[empty]=ticker;
	}
}

void register_rec_tell(int cn,int coID)
{
	int n;
	struct tell_data *dat;

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(dat=set_data(cn,DRD_TELL_DATA,sizeof(struct tell_data)))) return;	// OOPS

	//log_char(cn,LOG_SYSTEM,0,"should remove tell %d",coID);

	for (n=0; n<MAXTELL; n++) {
		if (dat->target[n]==coID) {
			dat->target[n]=0;
			dat->time[n]=0;
			//log_char(cn,LOG_SYSTEM,0,"removed tell %d %d",n,coID);
		}
	}
}

void check_tells(int cn)
{
	int n;
	struct tell_data *dat;
	char name[80];

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(dat=set_data(cn,DRD_TELL_DATA,sizeof(struct tell_data)))) return;	// OOPS
	
        for (n=0; n<MAXTELL; n++) {
		if (!dat->target[n]) continue;
		if (ticker-dat->time[n]>TICKS) {
			lookup_ID(name,dat->target[n]);
			log_char(cn,LOG_SYSTEM,0,"%s is not listening.",name);
			dat->target[n]=0;
			dat->time[n]=0;
		}
	}
}
