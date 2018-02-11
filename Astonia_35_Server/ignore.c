/*

$Id: ignore.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: ignore.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:14  sam
Added RCS tags


*/

#include <string.h>

#include "server.h"
#include "talk.h"
#include "drdata.h"
#include "lookup.h"
#include "tool.h"
#include "ignore.h"

#define MAXIGNORE	100

struct ignore_ppd
{
	int ignore[MAXIGNORE];	// IDs of enemies
};

int ignoring(int cn,int ID)
{
	struct ignore_ppd *ppd;
	int n;

	if (!ID) return 0;

        if (!(ppd=set_data(cn,DRD_IGNORE_PPD,sizeof(struct ignore_ppd)))) return 0;	// OOPS
	for (n=0; n<MAXIGNORE; n++)
		if (ppd->ignore[n]==ID) return 1;
	
	return 0;
}

void ignore(int cn,int ID)
{
	struct ignore_ppd *ppd;
	int n,empty=-1;

        if (!(ppd=set_data(cn,DRD_IGNORE_PPD,sizeof(struct ignore_ppd)))) return;	// OOPS

	for (n=0; n<MAXIGNORE; n++) {	
		if (ppd->ignore[n]==ID) {
			ppd->ignore[n]=0;
			log_char(cn,LOG_SYSTEM,0,"Deleted from ignore list.");
			return;
		}
		if (ppd->ignore[n]==0 && empty==-1) empty=n;
	}
	
	if (empty!=-1) {
		ppd->ignore[empty]=ID;
		log_char(cn,LOG_SYSTEM,0,"Added to ignore list.");
	} else log_char(cn,LOG_SYSTEM,0,"Ignore list is full, cannot add.");
}

void list_ignore(int cn)
{
	struct ignore_ppd *ppd;
	int n,res,flag=0;
	char name[80];

        if (!(ppd=set_data(cn,DRD_IGNORE_PPD,sizeof(struct ignore_ppd)))) return;	// OOPS

	for (n=0; n<MAXIGNORE; n++) {
		if (ppd->ignore[n]) {
			res=lookup_ID(name,ppd->ignore[n]);
			if (res>=0) {
				log_char(cn,LOG_SYSTEM,0,"Ignoring: %s",name);
				flag=1;
			} else {
				log_char(cn,LOG_SYSTEM,0,"Removed deleted char from list.");
				ppd->ignore[n]=0;
			}
		}
	}
	if (!flag) log_char(cn,LOG_SYSTEM,0,"Ignore list is empty.");	
}


int ignore_cmd(int cn,char *name)
{
	int coID;

	if (!name || !*name) {
		list_ignore(cn);
		return 1;
	}

	coID=lookup_name(name,NULL);
	if (coID==0) return 0;
	if (coID==-1) { log_char(cn,LOG_SYSTEM,0,"No player by that name."); return 1; }

	ignore(cn,coID);

        return 1;
}

void clearignore_cmd(int cn)
{
	struct ignore_ppd *ppd;

	if (!(ppd=set_data(cn,DRD_IGNORE_PPD,sizeof(struct ignore_ppd)))) return;	// OOPS

	bzero(ppd,sizeof(struct ignore_ppd));

	log_char(cn,LOG_SYSTEM,0,"Ignore list is now empty.");
}
