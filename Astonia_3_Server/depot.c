/*

$Id: depot.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: depot.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:09  sam
Added RCS tags


*/

#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "log.h"
#include "error.h"
#include "create.h"
#include "tool.h"
#include "talk.h"
#include "drdata.h"
#include "consistency.h"
#include "depot.h"
#include "database.h"


int swap_depot(int cn,int nr)
{
	int in,in2;
	struct depot_ppd *ppd;
	
	ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd));
	if (!ppd) return 0;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
        if (nr<0 || nr>=MAXDEPOT) { error=ERR_ACCESS_DENIED; return 0; }

	// remember citem
	in2=ch[cn].citem;

	if (it[in2].flags&IF_NODEPOT) {
		error=ERR_ACCESS_DENIED;
		return 0;
	}

        // remember item from depot
	if (ppd->itm[nr].flags) {
		in=alloc_item();
		if (!in) { error=ERR_CONFUSED; return 0; }	
		it[in]=ppd->itm[nr];
		dlog(cn,in,"Took %s from depot slot %d",it[in].name,nr);
	} else in=0;

        // set new citem
	ch[cn].citem=in;
	ch[cn].flags|=CF_ITEMS;
	if (in) it[in].carried=cn;

	if (in2) {
		ppd->itm[nr]=it[in2];
		dlog(cn,in2,"Put %s into depot slot %d",it[in2].name,nr);
		free_item(in2);
	} else ppd->itm[nr].flags=0;	

	return 1;
}

// character (usually a player) cn is using store NR
// flag=1 take/drop, flag=0 look
void player_depot(int cn,int nr,int flag,int fast)
{
        struct depot_ppd *ppd;
	
	ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd));
	if (!ppd) return;
	
        if (flag) {
		if (fast && ch[cn].citem) {
			for (nr=0; nr<MAXDEPOT; nr++) {
				if (!(ppd->itm[nr].flags)) break;
			}
			if (nr==MAXDEPOT) return;
			swap_depot(cn,nr);
		} else {
			swap_depot(cn,nr);
			if (fast) store_citem(cn);
		}
	} else {
		if (!ppd->itm[nr].flags) return;
		look_item(cn,&ppd->itm[nr]);
	}
}

int depot_cmp(const void *a,const void *b)
{
	if (!((struct item*)(a))->flags && !((struct item*)(b))->flags) return 0;
	if (!((struct item*)(a))->flags) return 1;
	if (!((struct item*)(b))->flags) return -1;

        if (((struct item*)(b))->sprite<((struct item*)(a))->sprite) return -1;
	if (((struct item*)(b))->sprite>((struct item*)(a))->sprite) return 1;

	if (((struct item*)(b))->value<((struct item*)(a))->value) return -1;
	if (((struct item*)(b))->value>((struct item*)(a))->value) return 1;

	return strncmp(((struct item*)(a))->name,((struct item*)(b))->name,35);	
}

void depot_sort(int cn)
{
	struct depot_ppd *ppd;
	
	ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd));
	if (!ppd) return;

	qsort(ppd->itm,MAXDEPOT,sizeof(ppd->itm[0]),depot_cmp);

}
