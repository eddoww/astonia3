/*

$Id: expire.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: expire.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:12  sam
Added RCS tags


*/

#include "server.h"
#include "log.h"
#include "timer.h"
#include "map.h"
#include "create.h"
#include "container.h"
#include "consistency.h"
#include "expire.h"

static void expire_timer(int in,int serial,int x,int y,int expire_sn)
{
        if (!(it[in].flags)) return;			// item was destroyed in the meantime
	if (it[in].serial!=serial) return;		// item was destroyed and re-created
	if (it[in].x!=x || it[in].y!=y) return;		// item is not where it was supposed to be	
        if (it[in].expire_sn!=expire_sn) return;	// wrong timer instance

        if (remove_item_map(in)) {
                if (it[in].content) destroy_item_container(in);
		free_item(in);
		return;
	}

	//xlog("%s (%d): max=%d, cur=%d, dam=%d",it[in].name,in,it[in].max_damage,it[in].cur_damage,dam);
	elog("could not expire item %s",it[in].name);
}

// set item expire for items on map
int set_expire(int in,int duration)
{
        if (in<1 || in>=MAXITEM) {
		elog("set_expire: got illegal item number %d",in);
		return 0;
	}

	if (!(it[in].flags)) {
		elog("set_expire: trying to expire unused item %d",in);
		return 0;
	}
	if (it[in].flags&IF_NODECAY) return 1;
	
	it[in].expire_sn++;

        return set_timer(ticker+duration,expire_timer,in,it[in].serial,it[in].x,it[in].y,it[in].expire_sn);
}

static void expire_timer_body(int in,int serial,int x,int y,int dummy)
{
        if (!(it[in].flags)) return;		// item was destroyed in the meantime
	if (it[in].serial!=serial) return;	// item was destroyed and re-created
	if (it[in].x!=x || it[in].y!=y) return;	// item is not where it was supposed to be	

	(*(unsigned int*)(it[in].drdata+8))--;
	
	if (container_itemcnt(in) && *(unsigned int*)(it[in].drdata+8)) {
		set_timer(ticker+TICKS*5,expire_timer_body,in,it[in].serial,it[in].x,it[in].y,0);
		return;
	}

	if (it[in].flags&IF_PLAYERBODY) ilog("expire: destroying item %s (%s) (%d), cnt=%d, pos=%d,%d",it[in].name,it[in].description,in,container_itemcnt(in),it[in].x,it[in].y);
        if (remove_item_map(in)) {		
		if (it[in].content) destroy_item_container(in);
		free_item(in);
		return;
	}

	//xlog("%s (%d): max=%d, cur=%d, dam=%d",it[in].name,in,it[in].max_damage,it[in].cur_damage,dam);
	elog("could not expire item %s",it[in].name);
}

// set item expire for items on map
int set_expire_body(int in,int duration)
{
        if (in<1 || in>=MAXITEM) {
		elog("set_expire: got illegal item number %d",in);
		return 0;
	}

	if (!(it[in].flags)) {
		elog("set_expire: trying to expire unused item %d",in);
		return 0;
	}
	if (it[in].flags&IF_NODECAY) return 1;

	*(unsigned int*)(it[in].drdata+8)=duration/(TICKS*5);

        return set_timer(ticker+TICKS*5,expire_timer_body,in,it[in].serial,it[in].x,it[in].y,0);
}
