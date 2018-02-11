/*

$Id: consistency.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: consistency.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:01  sam
Added RCS tags


*/

#include <stdio.h>
#include <string.h>

#include "server.h"
#include "log.h"
#include "mem.h"
#include "container.h"
#include "create.h"
#include "talk.h"
#include "consistency.h"

static unsigned char *item_used=NULL;

int consistency_check_items(void)
{
	int in,n,cn,m,ct,err=0;

	for (in=1; in<MAXITEM; in++) {
		if (!it[in].flags) continue;
                if (it[in].flags&IF_VOID) continue;

		// check if the item is valid (just one check so far:)
		// check containers:
                if ((ct=it[in].content)) {
			if (ct<1 || ct>=MAXCONTAINER) {
				elog("item %s (%d) thinks its content is container %d but that is out of bounds. fixing.",it[in].name,in,it[in].content);
				elog_item(in);

				it[in].content=0;
				continue;
			}
			if (con[ct].in!=in) {
				elog("item %s (%d) thinks its content is container %d but that container has no link back. fixing.",it[in].name,in,it[in].content);
				elog_item(in);

				it[in].content=0;
				continue;
			}
		}
		
		// check if the item is linked (ie. carried by a char, on the map, in a container)
		if ((cn=it[in].carried)) {
			if (cn<1 || cn>=MAXCHARS) {
				elog("item %s (%d) thinks it is carried by character %d, but that number is out of bounds. fixing.",it[in].name,in,cn);
				elog_item(in);

				it[in].carried=0;
				err++;
                                continue;
			}
			if (!ch[cn].flags) {
				elog("item %s (%d) thinks it is carried by character %s (%d), but that character is unused. fixing.",it[in].name,in,ch[cn].name,cn);
				elog_item(in);

				it[in].carried=0;
				err++;
				continue;
			}
			for (n=0; n<INVENTORYSIZE; n++) {
				if (ch[cn].item[n]==in) break;				
			}
			if (n==INVENTORYSIZE && ch[cn].citem!=in) {
				elog("item %s (%d) thinks it is carried by character %s (%d), but there is no link back. fixing.",it[in].name,in,ch[cn].name,cn);
				elog_item(in);

				it[in].carried=0;
				err++;
				continue;
			}
		} else if (it[in].x) {
			if (it[in].x<1 || it[in].x>=MAXMAP || it[in].y<1 || it[in].y>=MAXMAP) {
				elog("item %s (%d) thinks it is on position %d,%d, but that position is out of bounds. fixing.",it[in].name,in,it[in].x,it[in].y);
				elog_item(in);

				it[in].x=it[in].y=0;
				err++;
				continue;
			}
			m=it[in].x+it[in].y*MAXMAP;
			if (map[m].it!=in) {
				elog("item %s (%d) thinks it is on position %d,%d, but there is no link back. fixing.",it[in].name,in,it[in].x,it[in].y);
				elog_item(in);

				it[in].x=it[in].y=0;
				err++;
				continue;
			}
		} else if ((ct=it[in].contained)) {
			if (ct<1 || ct>=MAXCONTAINER) {
				elog("item %s (%d) thinks it is in container %d, but that number is out of bounds. fixing.",it[in].name,in,ct);
				elog_item(in);

				it[in].contained=0;
				err++;
				continue;
			}
			if (!con[ct].cn && !con[ct].in) {
				elog("item %s (%d) thinks it is in container (%d), but that container is unused. fixing.",it[in].name,in,ct);
				elog_item(in);

				it[in].contained=0;
				err++;
				continue;
			}
			for (n=0; n<CONTAINERSIZE; n++) {
				if (con[ct].item[n]==in) break;				
			}
			if (n==CONTAINERSIZE) {
				elog("item %s (%d) thinks it is in container (%d), but there is no link back. fixing.",it[in].name,in,ct);
				elog_item(in);

				it[in].contained=0;
				err++;
				continue;
			}
		} else {
			elog("item %s (%d): not linked. fixing.",it[in].name,in);
			elog_item(in);
			free_item(in);
			err++;
		}
	}
	return err;
}

// check if all item references on the map are valid
int consistency_check_map(void)
{
	int m,in,err=0;

	if (!item_used) item_used=xcalloc(MAXITEM,IM_TEMP);
	else bzero(item_used,MAXITEM);

	for (m=0; m<MAXMAP*MAXMAP; m++) {
		if ((in=map[m].it)) {
			if (in<1 || in>=MAXITEM) {
				elog("map pos %d,%d contains link to illegal item number %d. fixing.",m%MAXMAP,m/MAXMAP,in);
				map[m].it=0;
				err++;
				continue;
			}
			if (!it[in].flags) {
				elog("map pos %d,%d contains link to item %s (%d) but that item is unused. fixing.",m%MAXMAP,m/MAXMAP,it[in].name,in);
				elog_item(in);

				map[m].it=0;
				err++;
				continue;
			}
			if (it[in].flags&IF_VOID) {
				elog("map pos %d,%d contains link to item %s (%d) but that item is in the void. fixing.",m%MAXMAP,m/MAXMAP,it[in].name,in);
				elog_item(in);

				map[m].it=0;
                                err++;
				continue;
			}
			if (it[in].carried) {
				elog("map pos %d,%d contains link to item %s (%d) but that item has carried set to %d. fixing.",m%MAXMAP,m/MAXMAP,it[in].name,in,it[in].carried);
				elog_item(in);

				map[m].it=0;
				err++;
				continue;
			}
			if (it[in].contained) {
				elog("map pos %d,%d contains link to item %s (%d) but that item has contained set to %d. fixing.",m%MAXMAP,m/MAXMAP,it[in].name,in,it[in].contained);
				elog_item(in);

				map[m].it=0;
				err++;
				continue;
			}
			if (it[in].x+it[in].y*MAXMAP!=m) {
				elog("map pos %d,%d contains link to item %s (%d) but that item has no link back (%d,%d). fixing.",m%MAXMAP,m/MAXMAP,it[in].name,in,it[in].x,it[in].y);
				elog_item(in);

				map[m].it=0;
				err++;
				continue;
			}
			item_used[in]++;
			if (item_used[in]>1) {
				elog("item %s (%d) has %d links, one from map at %d,%d. fixing.",it[in].name,in,item_used[in],m%MAXMAP,m/MAXMAP);
				elog_item(in);

				map[m].it=0;
				err++;
				continue;
			}
		}
	}
	return err;
}

// check if all item references on characters are valid
int consistency_check_chars(void)
{
	int cn,in,n,err=0;

	if (!item_used) return -1;

	for (cn=1; cn<MAXCHARS; cn++) {
		if (!ch[cn].flags) continue;
		for (n=0; n<=INVENTORYSIZE; n++) {
			if ((n!=INVENTORYSIZE && (in=ch[cn].item[n])) || (n==INVENTORYSIZE && (in=ch[cn].citem))) {
				if (in<1 || in>=MAXITEM) {
					elog("character %s (%d) contains link to illegal item number %d. fixing.",ch[cn].name,cn,in);
                                        if (n==INVENTORYSIZE) ch[cn].citem=0; else ch[cn].item[n]=0;
					ch[cn].flags|=CF_ITEMS;
					if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"°c3You encountered a bug (consist1). Your item %s has been removed. Please email game@astonia.com. We apologize for the bug.",it[in].name);
					err++;
					continue;
				}
				if (!it[in].flags) {
					elog("character %s (%d) contains link to item %s (%d) but that item is unused. fixing.",ch[cn].name,cn,it[in].name,in);
					elog_item(in);

					if (n==INVENTORYSIZE) ch[cn].citem=0; else ch[cn].item[n]=0;
					ch[cn].flags|=CF_ITEMS;
					if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"°c3You encountered a bug (consist2). Your item %s has been removed. Please email game@astonia.com. We apologize for the bug.",it[in].name);
					err++;
					continue;
				}
				if (it[in].flags&IF_VOID) {
					elog("character %s (%d) contains link to item %s (%d) but that item is in the void. fixing.",ch[cn].name,cn,it[in].name,in);
					elog_item(in);

					if (n==INVENTORYSIZE) ch[cn].citem=0; else ch[cn].item[n]=0;
					ch[cn].flags|=CF_ITEMS;
					if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"°c3You encountered a bug (consist3). Your item %s has been removed. Please email game@astonia.com. We apologize for the bug.",it[in].name);
					err++;
					continue;
				}
				if (it[in].carried!=cn) {
					elog("character %s (%d) contains link to item %s (%d) but that item has no link back (%d). fixing.",ch[cn].name,cn,it[in].name,in,it[in].carried);
					elog_item(in);

					if (n==INVENTORYSIZE) ch[cn].citem=0; else ch[cn].item[n]=0;
					ch[cn].flags|=CF_ITEMS;
					if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"°c3You encountered a bug (consist4). Your item %s has been removed. Please email game@astonia.com. We apologize for the bug.",it[in].name);
					err++;
					continue;
				}
				if (it[in].x) {
					elog("character %s (%d) contains link to item %s (%d) but that item has pos set to %d,%d. fixing.",ch[cn].name,cn,it[in].name,in,it[in].x,it[in].y);
					elog_item(in);

                                        it[in].x=it[in].y=0;
                                        err++;
					continue;
				}
				if (it[in].contained) {
					elog("character %s (%d) contains link to item %s (%d) but that item has contained set to %d. fixing.",ch[cn].name,cn,it[in].name,in,it[in].contained);
					elog_item(in);

					it[in].contained=0;
					err++;
					continue;
				}
                                item_used[in]++;
				if (item_used[in]>1) {
					elog("item %s (%d) has %d links, one from character %s (%d). fixing.",it[in].name,in,item_used[in],ch[cn].name,cn);
					elog_item(in);

					if (n==INVENTORYSIZE) ch[cn].citem=0; else ch[cn].item[n]=0;
					ch[cn].flags|=CF_ITEMS;
					if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"°c3You encountered a bug (consist4). Your item %s has been removed. Please email game@astonia.com. We apologize for the bug.",it[in].name);
					err++;
					continue;
				}
			}
		}
	}
	return err;
}

int consistency_check_containers(void)
{
	int ct,n,in,err=0;

	for (ct=1; ct<MAXCONTAINER; ct++) {
		if (!con[ct].cn && !con[ct].in) continue;
		for (n=0; n<CONTAINERSIZE; n++) {
			if ((in=con[ct].item[n])) {
				if (in<1 || in>=MAXITEM) {
					elog("container (%d) contains link to illegal item number %d. fixing.",ct,in);
					con[ct].item[n]=0;
					err++;
					continue;
				}
				if (!it[in].flags) {
					elog("container (%d) contains link to item %s (%d) but that item is unused. fixing.",ct,it[in].name,in);
					elog_item(in);

					con[ct].item[n]=0;
					err++;
					continue;
				}
				if (it[in].flags&IF_VOID) {
					elog("container (%d) contains link to item %s (%d) but that item is in the void. fixing.",ct,it[in].name,in);
					elog_item(in);

					con[ct].item[n]=0;
					err++;
					continue;
				}
				if (it[in].contained!=ct) {
					elog("container (%d) contains link to item %s (%d) but that item has no link back (%d). fixing.",ct,it[in].name,in,it[in].contained);
					elog_item(in);

					con[ct].item[n]=0;
					err++;
					continue;
				}
				if (it[in].x) {
					elog("container (%d) contains link to item %s (%d) but that item has pos set to %d,%d. fixing.",ct,it[in].name,in,it[in].x,it[in].y);
					elog_item(in);

					it[in].x=it[in].y=0;
					err++;
					continue;
				}
				if (it[in].carried) {
					elog("container (%d) contains link to item %s (%d) but that item has carried set to %d. fixing.",ct,it[in].name,in,it[in].carried);
					elog_item(in);

					it[in].carried=0;
					err++;
					continue;
				}
                                item_used[in]++;
				if (item_used[in]>1) {
					elog("item %s (%d) has %d links, one from container (%d). fixing.",it[in].name,in,item_used[in],ct);
					elog_item(in);

					con[ct].item[n]=0;
					err++;
					continue;
				}
			}
		}
	}
	return err;
}
