/*
 * $Id: drvlib.c,v 1.15 2008/04/14 14:38:47 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: drvlib.c,v $
 * Revision 1.15  2008/04/14 14:38:47  devel
 * support for /nowarcry
 *
 * Revision 1.14  2008/03/27 09:08:42  devel
 * flee from high level improvements
 *
 * Revision 1.13  2007/12/30 12:49:56  devel
 * some fixes to fleeing demons from high level chars
 *
 * Revision 1.12  2007/09/21 11:00:41  devel
 * gatekeeper should not stop and regen
 *
 * Revision 1.11  2007/09/11 17:07:33  devel
 * no FR during LF fix
 *
 * Revision 1.10  2007/08/15 09:14:07  devel
 * NPC will no longer repeatedly cast heal
 *
 * Revision 1.9  2007/07/24 19:17:10  devel
 * added time delay to fire and lightning ball
 *
 * Revision 1.8  2007/07/09 11:21:53  devel
 * added check for cannot escape in walk_use
 *
 * Revision 1.7  2007/07/04 09:21:32  devel
 * added prio of lightning ball in fight driver
 *
 * Revision 1.6  2007/07/01 12:09:39  devel
 * fight logic no longer warcries if it would have no effect
 *
 * Revision 1.5  2007/06/22 13:02:39  devel
 * made fight_driver() use magic even if the result is minimal
 *
 * Revision 1.4  2007/05/07 09:49:12  devel
 * fixed bug in teleport_char_driver_extended
 *
 * Revision 1.3  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.2  2005/12/14 10:51:25  ssim
 * made fight driver not regenerate in tunnels
 *
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "server.h"
#include "drdata.h"
#include "direction.h"
#include "error.h"
#include "notify.h"
#include "path.h"
#include "do.h"
#include "see.h"
#include "talk.h"
#include "map.h"
#include "container.h"
#include "timer.h"
#include "libload.h"
#include "spell.h"
#include "tool.h"
#include "effect.h"
#include "create.h"
#include "sector.h"
#include "act.h"
#include "date.h"
#include "player_driver.h"
#include "lostcon.h"
#include "balance.h"
#include "drvlib.h"
#include "fight.h"
#include "consistency.h"
#include "log.h"

int walk_or_use_driver(int cn,int dir)
{
	if (do_walk(cn,dir)) return 1;
	if (error==ERR_CANNOT_ESCAPE) return 0;

        if (do_use(cn,dir,0)) return 1;

        return 0;
}

int walk_swap_or_use_driver(int cn,int dir)
{
	if (do_walk(cn,dir)) return 1;

	turn(cn,dir);
	if (char_swap(cn)) return 1;
	
	if (do_use(cn,dir,0)) return 1;

        return 0;
}

int move_driver(int cn,int tx,int ty,int mindist)
{
	int dir;

        dir=pathfinder(ch[cn].x,ch[cn].y,tx,ty,mindist,NULL,0);
	
	if (dir==-1) return 0;

        return walk_or_use_driver(cn,dir);
}

static int swap_check_target(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
	if (map[m].flags&MF_DOOR) return 1;
	if (map[m].ch && (ch[map[m].ch].flags&(CF_PLAYER|CF_PLAYERLIKE)) && ch[map[m].ch].action==AC_IDLE) return 1;
        if (map[m].flags&MF_TMOVEBLOCK) return 0;

	return 1;
}

int swap_move_driver(int cn,int tx,int ty,int mindist)
{
	int dir;

        dir=pathfinder(ch[cn].x,ch[cn].y,tx,ty,mindist,swap_check_target,0);
	
	if (dir==-1) return 0;

        return walk_swap_or_use_driver(cn,dir);
}

static int tmove_path(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
	if (map[m].flags&MF_DOOR) return 1;
        //if (map[m].flags&MF_TMOVEBLOCK) return 0;

	return 1;
}

int tmove_driver(int cn,int tx,int ty,int mindist)
{
	int dir;

        dir=pathfinder(ch[cn].x,ch[cn].y,tx,ty,mindist,tmove_path,0);
	if (dir==-1) return 0;

        return do_walk(cn,dir);
}

// preliminary version - hunts down co and hits him
// future versions could include prediction of target
// movements, intercept course calculation etc.
int attack_driver(int cn,int co)
{
	int dx,dy,dir,dir1,dir2=-1,cost1,cost2=999999;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (cn==co) { error=ERR_SELF; return 0; }

	if (!char_see_char(cn,co)) { error=ERR_NOT_VISIBLE; return 0; }
	if (!can_attack(cn,co)) { error=ERR_ILLEGAL_ATTACK; return 0; }

        dx=ch[co].x-ch[cn].x;
	dy=ch[co].y-ch[cn].y;

        // if we're close enough, hit him
	if (dx== 0 && dy== 1) return do_attack(cn,DX_DOWN,co);
	if (dx== 0 && dy==-1) return do_attack(cn,DX_UP,co);
	if (dx== 1 && dy== 0) return do_attack(cn,DX_RIGHT,co);
	if (dx==-1 && dy== 0) return do_attack(cn,DX_LEFT,co);

	dx=ch[co].tox-ch[cn].x;
	dy=ch[co].toy-ch[cn].y;

        // if he's moving towards us, hit him
	if (dx== 0 && dy== 1) return do_attack(cn,DX_DOWN,co);
	if (dx== 0 && dy==-1) return do_attack(cn,DX_UP,co);
	if (dx== 1 && dy== 0) return do_attack(cn,DX_RIGHT,co);
	if (dx==-1 && dy== 0) return do_attack(cn,DX_LEFT,co);

	dir1=pathfinder(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y,1,NULL,0);
	cost1=pathcost();

	if (ch[co].tox) {
		dir2=pathfinder(ch[cn].x,ch[cn].y,ch[co].tox,ch[co].toy,1,NULL,0);
		cost2=pathcost();
	}
	if (cost1<cost2 || dir2==-1) dir=dir1;
	else dir=dir2;

        if (dir==-1) {
		dir=pathbestdir();
		if (dir!=-1) {
			int dist;

			dist=abs(ch[cn].x-ch[co].x)+abs(ch[cn].y-ch[co].y);
		
			if (pathbestdist()<dist) return walk_or_use_driver(cn,dir);
			else if (!(ch[cn].flags&CF_PLAYER)) return do_idle(cn,TICKS/4);
			else return 0;
		}
	} else return walk_or_use_driver(cn,dir);
	
        return 0;
}

int attack_driver_nomove(int cn,int co)
{
	int dx,dy;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (cn==co) { error=ERR_SELF; return 0; }

	if (!char_see_char(cn,co)) { error=ERR_NOT_VISIBLE; return 0; }
	if (!can_attack(cn,co)) { error=ERR_ILLEGAL_ATTACK; return 0; }

        dx=ch[co].x-ch[cn].x;
	dy=ch[co].y-ch[cn].y;

        // if we're close enough, hit him
	if (dx== 0 && dy== 1) return do_attack(cn,DX_DOWN,co);
	if (dx== 0 && dy==-1) return do_attack(cn,DX_UP,co);
	if (dx== 1 && dy== 0) return do_attack(cn,DX_RIGHT,co);
	if (dx==-1 && dy== 0) return do_attack(cn,DX_LEFT,co);

	dx=ch[co].tox-ch[cn].x;
	dy=ch[co].toy-ch[cn].y;

        // if he's moving towards us, hit him
	if (dx== 0 && dy== 1) return do_attack(cn,DX_DOWN,co);
	if (dx== 0 && dy==-1) return do_attack(cn,DX_UP,co);
	if (dx== 1 && dy== 0) return do_attack(cn,DX_RIGHT,co);
	if (dx==-1 && dy== 0) return do_attack(cn,DX_LEFT,co);

        return 0;
}

int distance_driver(int cn,int co,int distance)
{
	int dir=-1;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (cn==co) { error=ERR_SELF; return 0; }

	if (!char_see_char(cn,co)) { error=ERR_NOT_VISIBLE; return 0; }

	if (step_char_dist(cn,co)==distance) { error=ERR_ALREADY_THERE; return 0; }

        if (ch[co].tox) dir=pathfinder(ch[cn].x,ch[cn].y,ch[co].tox,ch[co].toy,distance,NULL,0);
        if (dir==-1) dir=pathfinder(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y,distance,NULL,0);

	if (dir==-1) dir=pathbestdir();
        if (dir!=-1) return walk_or_use_driver(cn,dir);
	
        return 0;
}

int give_driver(int cn,int co)
{
	int dx,dy,dir,in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (cn==co) { error=ERR_SELF; return 0; }

	if (!char_see_char(cn,co)) { error=ERR_NOT_VISIBLE; return 0; }

	if (!(in=ch[cn].citem)) { error=ERR_NO_CITEM; return 0; }

	if ((it[in].flags&IF_QUEST) && !(ch[co].flags&(CF_QUESTITEM|CF_GOD)) && !(ch[cn].flags&(CF_QUESTITEM|CF_GOD))) { error=ERR_QUESTITEM; return 0; }

        dx=ch[co].x-ch[cn].x;
	dy=ch[co].y-ch[cn].y;

        // if we're close enough, hit him
	if (dx== 0 && dy== 1) return do_give(cn,DX_DOWN);
	if (dx== 0 && dy==-1) return do_give(cn,DX_UP);
	if (dx== 1 && dy== 0) return do_give(cn,DX_RIGHT);
	if (dx==-1 && dy== 0) return do_give(cn,DX_LEFT);

	dir=pathfinder(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y,1,NULL,0);
	if (dir!=-1) return walk_or_use_driver(cn,dir);	

        return 0;
}

// goto item and pick it up
int take_driver(int cn,int in)
{
	int dx,dy,dir;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (in<1 || in>=MAXITEM) { error=ERR_ILLEGAL_ITEMNO; return 0; }

	if (!char_see_item(cn,in)) { error=ERR_NOT_VISIBLE; return 0; }

	dx=it[in].x-ch[cn].x;
	dy=it[in].y-ch[cn].y;

        // if we're close enough, get it
	if (dx== 0 && dy== 1) return do_take(cn,DX_DOWN);
	if (dx== 0 && dy==-1) return do_take(cn,DX_UP);
	if (dx== 1 && dy== 0) return do_take(cn,DX_RIGHT);
	if (dx==-1 && dy== 0) return do_take(cn,DX_LEFT);

	dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x,it[in].y,1,NULL,0);
	if (dir!=-1) return walk_or_use_driver(cn,dir);
	
        return 0;
}

// goto item and use it
int use_driver(int cn,int in,int spec)
{
	int dx,dy,dir;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (in<1 || in>=MAXITEM) { error=ERR_ILLEGAL_ITEMNO; return 0; }
	if (!(it[in].flags&IF_USE)) { error=ERR_NOT_USEABLE; return 0; }

	//if (!char_see_item(cn,in)) { error=ERR_NOT_VISIBLE; return 0; }

	dx=it[in].x-ch[cn].x;
	dy=it[in].y-ch[cn].y;

        // if we're close enough, use it
	if (dx== 0 && dy== 1 && !(it[in].flags&IF_FRONTWALL)) return do_use(cn,DX_DOWN,spec);
	if (dx== 0 && dy==-1) return do_use(cn,DX_UP,spec);
	if (dx== 1 && dy== 0 && !(it[in].flags&IF_FRONTWALL)) return do_use(cn,DX_RIGHT,spec);
	if (dx==-1 && dy== 0) return do_use(cn,DX_LEFT,spec);

	if (it[in].flags&IF_FRONTWALL) {
		dir=-1;
		if (!(map[it[in].x+it[in].y*MAXMAP+1].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x+1,it[in].y,0,NULL,0);
		if (dir==-1 && !(map[it[in].x+it[in].y*MAXMAP+MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x,it[in].y+1,0,NULL,0);
	} else dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x,it[in].y,1,NULL,0);
	
	if (dir!=-1) return walk_or_use_driver(cn,dir);
	
        return 0;
}

// goto place and drop item there
int drop_driver(int cn,int x,int y)
{
	int dx,dy,dir,in,m;

        if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) { error=ERR_ILLEGAL_COORDS; return 0; }

        m=x+y*MAXMAP;
	if ((map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m].it) { error=ERR_BLOCKED; return 0; }

        if (!(in=ch[cn].citem)) { error=ERR_NO_CITEM; return 0; }
	if (it[in].flags&IF_QUEST) { error=ERR_QUESTITEM; return 0; }

        dx=x-ch[cn].x;
	dy=y-ch[cn].y;

        // if we're close enough, get it
	if (dx== 0 && dy== 1) return do_drop(cn,DX_DOWN);
	if (dx== 0 && dy==-1) return do_drop(cn,DX_UP);
	if (dx== 1 && dy== 0) return do_drop(cn,DX_RIGHT);
	if (dx==-1 && dy== 0) return do_drop(cn,DX_LEFT);

	dir=pathfinder(ch[cn].x,ch[cn].y,x,y,1,NULL,0);
	if (dir!=-1) return walk_or_use_driver(cn,dir);
	
        return 0;
}

// scan the surroundings for take-able items
#define SCANDIST	20
struct scan_item_data
{
	int x,y;
};

int scan_item_driver(int cn)
{
	struct scan_item_data *dat;
	int x,y,xs,xe,ys,ye,m,diff,in;

        dat=set_data(cn,DRD_SCANITEM,sizeof(struct scan_item_data));
	if (!dat) return 0;	// oops...

	diff=max(abs(ch[cn].x-dat->x),abs(ch[cn].y-dat->y));

	if (!dat->x || diff>(SCANDIST/2+SCANDIST/4)) {
		xs=max(1,ch[cn].x-SCANDIST);
		ys=max(1,ch[cn].y-SCANDIST);
		xe=min(MAXMAP-2,ch[cn].x+SCANDIST);
		ye=min(MAXMAP-2,ch[cn].y+SCANDIST);
	
                for (y=ys; y<ye; y++) {
			for (x=xs; x<xe; x++) {
				m=x+y*MAXMAP;
	
				if ((in=map[m].it)) notify_char(cn,NT_ITEM,in,0,0);
			}
		}
		dat->x=ch[cn].x;
		dat->y=ch[cn].y;

		return 0;
	}

        if (ch[cn].x!=dat->x) {	// scan left/right		
		xs=max(1,ch[cn].x-SCANDIST);
		ys=max(1,ch[cn].y-SCANDIST);
		xe=min(MAXMAP-2,ch[cn].x+SCANDIST);
		ye=min(MAXMAP-2,ch[cn].y+SCANDIST);

                if (ch[cn].x>dat->x) xs=min(MAXMAP-2,dat->x+SCANDIST);
		else xe=max(1,dat->x-SCANDIST);

                for (y=ys; y<ye; y++) {
			for (x=xs; x<xe; x++) {
				m=x+y*MAXMAP;
	
				if ((in=map[m].it)) notify_char(cn,NT_ITEM,in,0,0);
			}
		}
	}

	if (ch[cn].y!=dat->y) {	// scan up/down		
		xs=max(1,ch[cn].x-SCANDIST);
		ys=max(1,ch[cn].y-SCANDIST);
		xe=min(MAXMAP-2,ch[cn].x+SCANDIST);
		ye=min(MAXMAP-2,ch[cn].y+SCANDIST);

                if (ch[cn].y>dat->y) ys=min(MAXMAP-2,dat->y+SCANDIST);
		else ye=max(1,dat->y-SCANDIST);

                for (y=ys; y<ye; y++) {
			for (x=xs; x<xe; x++) {
				m=x+y*MAXMAP;
	
				if ((in=map[m].it)) notify_char(cn,NT_ITEM,in,0,0);
			}
		}
	}

        dat->x=ch[cn].x;
	dat->y=ch[cn].y;

	return 0;
}

struct char_mem_data
{
	int cnt,max;
        unsigned int xID[0];
};

// add co to memory number nr (nr=0...7)
int mem_add_driver(int cn,int co,int nr)
{
	struct char_mem_data *dat;
	unsigned int xID;
        int n;

	if (nr<0 || nr>7) return 0;
	
        dat=set_data(cn,DRD_CHARMEM+nr,sizeof(struct char_mem_data));
	if (!dat) return 0;	// oops...

	if (ch[co].ID) xID=ch[co].ID|0x80000000;
	else xID=ch[co].serial&0x7fffffff;

	// already there?
	for (n=0; n<dat->cnt; n++)
		if (dat->xID[n]==xID) return 1;			

	// need more memory?
	if (dat->cnt==dat->max) {
		dat->max+=8;
		dat=set_data(cn,DRD_CHARMEM+nr,sizeof(struct char_mem_data)+sizeof(unsigned int)*dat->max);
		if (!dat) return 0;
	}

	n=dat->cnt++;
	dat->xID[n]=xID;

	return 1;
}

// check if co is in memory nr (nr=0...7)
// returns TRUE if yes, FALSE otherwise
int mem_check_driver(int cn,int co,int nr)
{
	struct char_mem_data *dat;
	unsigned int xID;
	int n;

	if (nr<0 || nr>7) return 0;
	
        dat=set_data(cn,DRD_CHARMEM+nr,sizeof(struct char_mem_data));
	if (!dat) return 0;	// oops...

	if (ch[co].ID) xID=ch[co].ID|0x80000000;
	else xID=ch[co].serial&0x7fffffff;

	// already there?
	for (n=0; n<dat->cnt; n++)
		if (dat->xID[n]==xID) return 1;

	return 0;
}

void mem_erase_driver(int cn,int nr)
{
	struct char_mem_data *dat;

	if (nr<0 || nr>7) return;
	
        dat=set_data(cn,DRD_CHARMEM+nr,sizeof(struct char_mem_data));
	if (!dat) return;	// oops...

	dat->cnt=0;
}

// simple parser for name value pairs:
// x=5;y=12;z=15; etc.
// name and value must be at least 64 bytes.
// returns a ptr behind the semicolon of the current nv
// pair or NULL on error/end of string
char *nextnv(char *ptr,char *name,char *value)
{
	int nlen=0,vlen=0;

	while (isspace(*ptr)) ptr++;

	while (isalpha(*ptr) && nlen++<60) *name++=*ptr++;
	*name=0;

	while (isspace(*ptr)) ptr++;

	if (*ptr=='=') ptr++;
	else return NULL;

	while (isspace(*ptr)) ptr++;

	while ((isalnum(*ptr) || *ptr=='-') && vlen++<60) *value++=*ptr++;
	*value=0;

	if (*ptr==';') ptr++;
	else return NULL;

	while (isspace(*ptr)) ptr++;

	return ptr;
}

// guesstimate of the distance between cn
// and co. does not take walls etc. into account.
// one straight tile is 2, one diagonal tile is 3.
int char_dist(int cn,int co)
{
	int dx,dy;

	dx=abs(ch[cn].x-ch[co].x);
	dy=abs(ch[cn].y-ch[co].y);
	
	if (dx>dy) return (dx<<1)+dy;
	else return (dy<<1)+dx;
}

// guesstimate of the distance between f
// and t. does not take walls etc. into account.
// one straight tile is 2, one diagonal tile is 3.
int map_dist(int fx,int fy,int tx,int ty)
{
	int dx,dy;

	dx=abs(fx-tx);
	dy=abs(fy-ty);
	
	if (dx>dy) return (dx<<1)+dy;
	else return (dy<<1)+dx;
}

// exact number of tile between cn (max of dx,dy)
// and co. does not take walls etc. into account.
int tile_char_dist(int cn,int co)
{
	int dx,dy;

	dx=abs(ch[cn].x-ch[co].x);
	dy=abs(ch[cn].y-ch[co].y);
	
        return max(dx,dy);
}

// exact number of tile between cn (sum of dx+dy)
// and co. does not take walls etc. into account.
// does use to-position if available
int step_char_dist(int cn,int co)
{
	int dx,dy;

	if (ch[co].tox) {
		dx=abs(ch[cn].x-ch[co].tox);
		dy=abs(ch[cn].y-ch[co].toy);
	} else {
		dx=abs(ch[cn].x-ch[co].x);
		dy=abs(ch[cn].y-ch[co].y);
	}
	
        return dx+dy;
}

// free up citem by either storing the item
// in the backpack (if there is room) or
// putting it away (on ground)
// returning TRUE means that we make an action
int free_citem_driver(int cn)
{
	int n;

	// store citem in backpack
	if (ch[cn].citem) {
		for (n=30; n<INVENTORYSIZE; n++) {
			if (!ch[cn].item[n]) {
				swap(cn,n);
				break;
			}
		}		
	}
	// if that fails, try to drop it
	if (ch[cn].citem) {
		if (do_drop(cn,DX_UP)) return 1;
		if (do_drop(cn,DX_DOWN)) return 1;
		if (do_drop(cn,DX_LEFT)) return 1;
		if (do_drop(cn,DX_RIGHT)) return 1;
	}
	
	if (ch[cn].citem) return 0;

	return 0;
}

// check if co is a valid enemy, that is:
// - alive
// - visible
// - not a member of our group
// - and in our memory <mem>. mem=-1 means dont check
int is_valid_enemy(int cn,int co,int mem)
{
	if (!co) return 0;
	
	if (cn==co) return 0;
	
        if (ch[cn].group==ch[co].group) return 0;

	if (!can_attack(cn,co)) return 0;

	if (!char_see_char(cn,co)) return 0;

	if (mem==-1) return 1;

	if (!mem_check_driver(cn,co,mem)) return 0;

	return 1;
}

int remove_item(int in)
{
	if (it[in].x && it[in].carried) {
		elog("remove_item: item %s (%d) pos and carried set",it[in].name,in);
	}
	if (it[in].x && it[in].contained) {
		elog("remove_item: item %s (%d) pos and contained set",it[in].name,in);
	}
	if (it[in].carried && it[in].contained) {
		elog("remove_item: item %s (%d) carried and contained set",it[in].name,in);
	}
	if (it[in].x) {
		return remove_item_map(in);
	}
	if (it[in].carried) {
		return remove_item_char(in);
	}
	if (it[in].contained) {
		return remove_item_container(in);
	}
	return 0;
}

static void call_item_callback(int driver,int in,int cn,int serial,int dum2)
{
	if (!it[in].flags) return;		// item was deleted, dont call driver
        if (it[in].serial!=serial) return;	// different item by now, dont call driver
	
	item_driver(driver,in,cn);
}

// call item driver at tick due with the parameters specified.
// for timeouts, like closing doors etc.
int call_item(int driver,int in,int cn,int due)
{
	return set_timer(due,call_item_callback,driver,in,cn,it[in].serial,0);
}

//-------------- fight driver and helpers ------------------------

/*struct person
{
	unsigned int cn;
	unsigned int ID;

	unsigned short lastx,lasty;
	unsigned char visible;
	unsigned char hurtme;
};

struct fight_driver_data
{
	struct person enemy[10];

	int start_dist;		// distance from respawn point at which to start attacking
	int stop_dist;		// distance from respawn point at which to stop attacking
	int char_dist;		// distance from character we start attacking

	int home_x,home_y;	// position to compare start_dist and start_dist with

	int lasthit;
};*/

int fight_driver_flee_eval_path(int x,int y,int dir)
{
	int n,score=0,dx,dy,m,diag;

	dx2offset(dir,&dx,&dy,&diag);
	
	for (n=0; n<10; n++) {
		
		x+=dx; y+=dy;
		
		if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return score;
		
		m=x+y*MAXMAP;		
		if (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) return score;

		score+=300;
		score-=max(map[m].light,map[m].dlight*dlight/256);
	}
	
	return score;
}

int fight_driver_flee(int cn)
{
	int dir_score[9]={0,0,0,0,0,0,0,0,0};
	int dir,n,dist,co,bdir=0,bscore=-99999,dx,dy,dt,score,mindist=99;
	struct fight_driver_data *dat;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

        for (n=0; n<10; n++) {
		if (!dat->enemy[n].cn) continue;
		if (!dat->enemy[n].visible) continue;

		co=dat->enemy[n].cn;
		
		dist=char_dist(cn,co);
		if (dist>30) continue;

                mindist=min(mindist,dist);

		score=5000-dist*50;
		
		dx=ch[co].x-ch[cn].x;
		dy=ch[co].y-ch[cn].y;
		dt=abs(dx)+abs(dy);
		if (!dt) continue;	// sanity check, should never happen

		if (dx>0) {
			dir_score[DX_RIGHT]-=score*abs(dx)/dt;
                        dir_score[DX_RIGHTUP]-=score*abs(dx)/dt/2;
			dir_score[DX_RIGHTDOWN]-=score*abs(dx)/dt/2;

			dir_score[DX_LEFT]+=score*abs(dx)/dt/4;
                        dir_score[DX_LEFTUP]+=score*abs(dx)/dt/8;
			dir_score[DX_LEFTDOWN]+=score*abs(dx)/dt/8;
		}
		if (dx<0) {
			dir_score[DX_LEFT]-=score*abs(dx)/dt;
			dir_score[DX_LEFTUP]-=score*abs(dx)/dt/2;
			dir_score[DX_LEFTDOWN]-=score*abs(dx)/dt/2;

			dir_score[DX_RIGHT]-=score*abs(dx)/dt/4;
                        dir_score[DX_RIGHTUP]-=score*abs(dx)/dt/8;
			dir_score[DX_RIGHTDOWN]-=score*abs(dx)/dt/8;
		}
		if (dy>0) {
			dir_score[DX_DOWN]-=score*abs(dy)/dt;
			dir_score[DX_LEFTDOWN]-=score*abs(dy)/dt/2;
			dir_score[DX_RIGHTDOWN]-=score*abs(dy)/dt/2;

			dir_score[DX_UP]-=score*abs(dy)/dt/4;
			dir_score[DX_LEFTUP]-=score*abs(dy)/dt/8;
			dir_score[DX_RIGHTUP]-=score*abs(dy)/dt/8;
		}
		if (dy<0) {
			dir_score[DX_UP]-=score*abs(dy)/dt;
			dir_score[DX_LEFTUP]-=score*abs(dy)/dt/2;
			dir_score[DX_RIGHTUP]-=score*abs(dy)/dt/2;

			dir_score[DX_DOWN]-=score*abs(dy)/dt/4;
			dir_score[DX_LEFTDOWN]-=score*abs(dy)/dt/8;
			dir_score[DX_RIGHTDOWN]-=score*abs(dy)/dt/8;
		}
	}
	if (mindist>30) return 0;

        if (mindist<10 && (ch[cn].endurance>4*POWERSCALE || ch[cn].speed_mode==SM_FAST)) {
		ch[cn].speed_mode=SM_FAST;
	} else if (mindist<10) {
		ch[cn].speed_mode=SM_NORMAL;
	} else ch[cn].speed_mode=SM_STEALTH;

	//say(cn,"mode=%d",ch[cn].speed_mode);

	for (dir=1; dir<9; dir++) {
		dir_score[dir]+=fight_driver_flee_eval_path(ch[cn].x,ch[cn].y,dir);
                if (dir_score[dir]>bscore) {
			bdir=dir;
			bscore=dir_score[dir];
		}
	}
	if (bdir && do_walk(cn,bdir)) return 1;

	return 0;
}

int fireball_driver(int cn,int co,int serial)
{
	int dir,dx,dy,dist,left,step,eta,n;

	if (!ch[co].flags || ch[co].serial!=serial) { error=ERR_DEAD; return 0; }

	if (ch[co].action!=AC_WALK) return do_fireball(cn,ch[co].x,ch[co].y);
	
        dir=ch[co].dir;
	dx2offset(dir,&dx,&dy,NULL);
	dist=char_dist(cn,co);

	eta=dist/2+speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,8);
	
	left=ch[co].duration-ch[co].step;
	step=ch[co].duration;

        eta-=left;
	if (eta<=0) return do_fireball(cn,ch[co].x,ch[co].y);

	for (n=1; n<6; n++) {
		eta-=step;
		if (eta<=0) return do_fireball(cn,ch[co].x+dx*n,ch[co].y+dy*n);
	}
	
	// too far away, time-wise to make any prediction. give up.
	return do_fireball(cn,ch[co].x,ch[co].y);
}

int ball_driver(int cn,int co,int serial)
{
	if (!ch[co].flags || ch[co].serial!=serial) { error=ERR_DEAD; return 0; }

        return do_ball(cn,ch[co].x,ch[co].y);
}

int fight_driver_dist_from_home(int cn,int co)
{
	struct fight_driver_data *dat;
	int dx,dy;
	
	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

	if (dat->home_x) {
		dx=abs(ch[co].x-dat->home_x);
		dy=abs(ch[co].y-dat->home_y);
	} else {
		dx=abs(ch[co].x-ch[cn].tmpx);
		dy=abs(ch[co].y-ch[cn].tmpy);
	}

	if (dx>dy) return (dx<<1)+dy;
	else return (dy<<1)+dx;
}

static int fight_driver_fireball_enemy_check(int cn,int co)
{
        struct fight_driver_data *dat;
	int n;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

	for (n=0; n<10; n++)
		if (dat->enemy[n].cn==co) return 1;
	
	return 0;
}

// --- helpers to determine effectiveness of different kinds of attacks ---

#define LOW_PRIO	  1
#define MED_PRIO	500
#define HIGH_PRIO	750

static int earthmud_driver(int cn,int co,int str)
{
	int x,y;

	if (ch[co].action==AC_WALK) {
		x=ch[co].tox+ch[co].tox-ch[co].x;
		y=ch[co].toy+ch[co].toy-ch[co].y;
	} else {
		x=ch[co].x;
		y=ch[co].y;
	}
	if (!str) return x+y*MAXMAP;
	
	return do_earthmud(cn,x,y,str);
}

static int earthrain_driver(int cn,int co,int str)
{
	int x,y;

	if (ch[co].action==AC_WALK) {
		x=ch[co].tox+ch[co].tox-ch[co].x;
		y=ch[co].toy+ch[co].toy-ch[co].y;
	} else {
		x=ch[co].x;
		y=ch[co].y;
	}
	if (!str) return x+y*MAXMAP;
	
	return do_earthrain(cn,x,y,str);
}

// earthrain
/*static int check_earthrain_field(int x,int y)
{
	int m,n,fn;

	m=x+y*MAXMAP;

	if (map[m].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) return 0;

	for (n=0; n<4; n++) {
		if ((fn=map[m].ef[n]) && ef[fn].type==EF_EARTHRAIN) return 0;		
	}

	return 1;
	
}*/

// earthmud
static int check_earthmud_field(int x,int y)
{
	int m,n,fn;

	m=x+y*MAXMAP;

	if (map[m].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) return 0;

	for (n=0; n<4; n++) {
		if ((fn=map[m].ef[n]) && ef[fn].type==EF_EARTHMUD) return 0;
	}

	return 1;
	
}

/*static int fight_driver_earthrain_value(int cn,int co)
{
	int m,x,y,val,good=0;


	m=earthrain_driver(cn,co,0);
	x=m%MAXMAP;
	y=m/MAXMAP;

	if (check_earthrain_field(x,y)) good++;
        if (check_earthrain_field(x+1,y)) good++;
	if (check_earthrain_field(x-1,y)) good++;
	if (check_earthrain_field(x,y+1)) good++;
	if (check_earthrain_field(x,y-1)) good++;

	if (good<1) return 0;
	if (good<2) val=LOW_PRIO;
	else if (good<4) val=MED_PRIO;
	else val=HIGH_PRIO+(good*20);

	return val;
}*/

static int fight_driver_earthmud_value(int cn,int co)
{
	int m,x,y,val,good=0;


	m=earthmud_driver(cn,co,0);
	x=m%MAXMAP;
	y=m/MAXMAP;

	if (check_earthmud_field(x,y)) good++;
        if (check_earthmud_field(x+1,y)) good++;
	if (check_earthmud_field(x-1,y)) good++;
	if (check_earthmud_field(x,y+1)) good++;
	if (check_earthmud_field(x,y-1)) good++;

	if (good<1) return 0;
	if (good<2) val=LOW_PRIO;
	else if (good<4) val=MED_PRIO;
	else val=HIGH_PRIO+(good*20);

	return val;
}


// freeze
static int fight_driver_freeze_value(int cn)
{
	int val;

	val=HIGH_PRIO+ch[cn].value[0][V_FREEZE];

	return val;
}

// heal
static int fight_driver_heal_value(int cn)
{
	int val;

	val=HIGH_PRIO+ch[cn].value[0][V_HEAL];

	return val;
}

// magicshield
static int fight_driver_ms_value(int cn)
{
	int val;

	val=HIGH_PRIO+ch[cn].value[0][V_MAGICSHIELD];

	return val;
}

// bless
static int fight_driver_bless_value(int cn)
{
	int val;

	val=HIGH_PRIO+ch[cn].value[0][V_BLESS];

	return val;
}

// fireball value: moving target? distance to target?
static int fight_driver_fireball_value(int cn,int co)
{
	int val;

	val=MED_PRIO+ch[cn].value[0][V_FIRE]+ch[cn].value[0][V_FIRE]/2;

	return val;
}

static int fight_driver_firering_value(int cn,int co)
{
	int val;

	val=MED_PRIO+ch[cn].value[0][V_FIRE]+ch[cn].value[0][V_FIRE]/2;

	return val;
}

// ball value: distance to target -? fighting target +?
// direct ball or offset? (offset when fighting)
static int fight_driver_directball_value(int cn,int co)
{
	int val;

        val=MED_PRIO+ch[cn].value[0][V_FLASH]+ch[cn].value[0][V_FLASH]/2;

	return val;
}

static int fight_driver_flash_value(int cn,int co)
{
	int val;

	val=MED_PRIO+ch[cn].value[0][V_FLASH]+ch[cn].value[0][V_FLASH]/2;

	return val;
}

static int fight_driver_warcry_value(int cn,int co)
{
	int val;

	val=HIGH_PRIO+ch[cn].value[0][V_WARCRY]/2;

	return val;
}

static int fight_driver_regen_value(int cn,int co,struct fight_driver_data *dat)
{
	int val,diff;

	if (areaID==33) return 0;

	val=max(ch[cn].value[0][V_FIRE],max(ch[cn].value[0][V_FLASH],max(ch[cn].value[0][V_FREEZE],ch[cn].value[0][V_ATTACK])))*2;

	diff=ch[cn].regen_ticker+REGEN_TIME-ticker;
	if (diff<=0) return val+HIGH_PRIO;

	if (areaID==3) return 0;

        diff=dat->lasthit+REGEN_TIME*2-ticker;
        if (diff<=0) return val+LOW_PRIO;

	return (val*REGEN_TIME*2-val*diff)/(REGEN_TIME*2)+LOW_PRIO;
}

static int fight_driver_attack_value(int cn,int co)
{
	int val;

	if (ch[cn].value[1][V_ATTACK]) val=MED_PRIO+ch[cn].value[0][V_OFFENSE]/3.5+10;
	else val=LOW_PRIO+ch[cn].value[0][V_OFFENSE]/3;
	
	return val;
}

static int fight_driver_distance3_value(int cn,int co)
{
	int val;

	if (ch[cn].value[1][V_ATTACK]) val=LOW_PRIO+ch[cn].value[0][V_FREEZE];
	else val=MED_PRIO+ch[cn].value[0][V_FREEZE];

	return val;
}

static int fight_driver_distance3_flash_value(int cn,int co)
{
	int val;

	if (ch[cn].value[1][V_ATTACK]) val=LOW_PRIO+ch[cn].value[0][V_FLASH];
	else val=MED_PRIO+ch[cn].value[0][V_FLASH];

	return val;
}

static int fight_driver_distance7_value(int cn,int co)
{
	int val;

	if (ch[cn].value[1][V_ATTACK]) val=LOW_PRIO+ch[cn].value[0][V_FIRE];
	else val=MED_PRIO+ch[cn].value[0][V_FIRE];

	return val;
}

static int fight_driver_attackback_value(int cn,int co)
{
	int x,y,diag,tx,ty,cc;

	dx2offset(ch[co].dir,&tx,&ty,&diag);
	if (diag) return 0;

	x=ch[co].x-tx;
	y=ch[co].y-ty;
	if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) return 0;

        if (map[x+y*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) return 0;	// spot behind him is blocked, no sense in trying

	x=ch[co].x+tx;
	y=ch[co].y+ty;
	if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) return 0;

	if (ch[cn].x==x && ch[cn].y==y) return 0;	// dont try if i'm in front of him
	
	if (ch[co].action==AC_IDLE && ticker-ch[co].regen_ticker>TICKS/2) return HIGH_PRIO;	// target back if he's just standing around
	
	if (!map[x+y*MAXMAP].ch) return 0;		// dont try if no one is in front of him

	x=ch[co].x+ty;
	y=ch[co].y+tx;
	if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) return 0;

        if ((cc=map[x+y*MAXMAP].ch) && cc!=cn && ch[cc].group==ch[cn].group) return 0;	// dont try if someone is on his side already (he will go)

        return HIGH_PRIO;
}

int attack_back_driver(int cn,int co)
{
	int x,y,diag,dir;

	dx2offset(ch[co].dir,&x,&y,&diag);
	if (diag) return 0;
	
	x=ch[co].x-x;
	y=ch[co].y-y;

	if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) return 0;

	dir=pathfinder(ch[cn].x,ch[cn].y,x,y,0,NULL,10);
	if (dir==-1) return 0;

	return do_walk(cn,dir);
}

void drv_stealth(int cn)
{
	ch[cn].speed_mode=SM_STEALTH;
}

void drv_fast(int cn)
{
	ch[cn].speed_mode=SM_FAST;
}

enum tasktype {freeze,fireball,ball,flash,warcry,attack,moveright,moveleft,moveup,movedown,regenerate,distance3,distance7,bless,earthrain,earthmud,heal,ms,attackback,flee,firering,maxtasktype};

struct task
{
	enum tasktype task;
	int value;
};

static int task_cmp(const void *a,const void *b)
{
	return ((struct task *)b)->value-((struct task *)a)->value;
}

// attack specific enemy. enemy should be in the fight driver enemy array for fireball to work
static int fight_driver_attack_enemy(int cn,int co,int nomove)
{
        //static char *typename[]={"freeze","fireball","ball","flash","warcry","atttack","moveright","moveleft","moveup","movedown","regenerate","distance3","distance7","bless","earthrain","earthmud","heal","ms","attackback","flee","firering","max"};
	struct task task[maxtasktype];
        int maxvalue=0,maxtask=0,n,ret,cdist,tdist,sdist,tmp;
	int sillyness=ch[cn].level/2+5,val;
	struct fight_driver_data *dat;
	int nowarcry=0;

	if (ch[cn].flags&CF_PLAYER) {
		struct lostcon_ppd *lc;

		lc=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd));

		if (lc) {
			nowarcry=lc->nowarcry;
		}
	}

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;	

	cdist=char_dist(cn,co);
	tdist=tile_char_dist(cn,co);
	sdist=step_char_dist(cn,co);

	if (ch[cn].value[0][V_FREEZE]>1 &&
	    freeze_value(cn,co)<=-1 &&
	    ch[cn].mana>=FREEZECOST &&
	    tdist<4 &&
            may_add_spell(co,IDR_FREEZE)) {
		task[maxtask].task=freeze;
		maxvalue+=(task[maxtask].value=fight_driver_freeze_value(cn));
		maxtask++;
	}

	if (ch[cn].value[0][V_HEAL]>1 &&
	    ch[cn].mana>=POWERSCALE*2 &&
	    ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 &&
	    !has_spell(cn,IDR_HEAL)) {
		task[maxtask].task=heal;
		maxvalue+=(task[maxtask].value=fight_driver_heal_value(cn));
		maxtask++;
	}

	if (ch[cn].value[0][V_MAGICSHIELD]>1 &&
	    ch[cn].mana>=POWERSCALE*2 &&
	    ch[cn].lifeshield<ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE/MAGICSHIELDMOD/2) {
		task[maxtask].task=ms;
		maxvalue+=(task[maxtask].value=fight_driver_ms_value(cn));
		maxtask++;
	}

        if ((ch[cn].flags&CF_EDEMON) &&
	    ch[cn].value[1][V_DEMON]==30 &&
	    ch[cn].hp>=ch[cn].value[0][V_HP]*POWERSCALE/2 &&
	    (val=fight_driver_earthmud_value(cn,co))) {
		task[maxtask].task=earthmud;
		maxvalue+=(task[maxtask].value=val);
		maxtask++;
	}

	if (ch[cn].value[0][V_BLESS]>1 &&
	    ch[cn].mana>=BLESSCOST &&
            may_add_spell(cn,IDR_BLESS)) {
		task[maxtask].task=bless;
		maxvalue+=(task[maxtask].value=fight_driver_bless_value(cn));
		maxtask++;
	}

	if (ch[cn].value[0][V_FIRE]>1 &&
	    fireball_damage(cn,co,ch[cn].value[0][V_FIRE])>=POWERSCALE &&
	    ch[cn].mana>=FIREBALLCOST &&
	    may_add_spell(cn,IDR_FIRE)) {
		if (ishit_fireball(cn,ch[cn].x,ch[cn].y,ch[co].x,ch[co].y,fight_driver_fireball_enemy_check)) {
			task[maxtask].task=fireball;
			maxvalue+=(task[maxtask].value=fight_driver_fireball_value(cn,co));
			maxtask++;
		} else if (!nomove) {
			int dirdead[9]={0,0,0,0,0,0,0,0,0};

			for (n=1; n<5; n++) {
				if (!dirdead[DX_RIGHT] && ch[cn].x+n>=MAXMAP) dirdead[DX_RIGHT]=1;
                                if (!dirdead[DX_RIGHT] && (map[(ch[cn].x+n)+(ch[cn].y)].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dirdead[DX_RIGHT]=1;

				if (!dirdead[DX_LEFT] && ch[cn].x-n>=MAXMAP) dirdead[DX_LEFT]=1;
                                if (!dirdead[DX_LEFT] && (map[(ch[cn].x-n)+(ch[cn].y)].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dirdead[DX_LEFT]=1;

				if (!dirdead[DX_DOWN] && ch[cn].y+n>=MAXMAP) dirdead[DX_DOWN]=1;
                                if (!dirdead[DX_DOWN] && (map[(ch[cn].x)+(ch[cn].y+n)].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dirdead[DX_DOWN]=1;

				if (!dirdead[DX_UP] && ch[cn].y-n>=MAXMAP) dirdead[DX_UP]=1;
                                if (!dirdead[DX_UP] && (map[(ch[cn].x)+(ch[cn].y-n)].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dirdead[DX_UP]=1;
				
				if (!dirdead[DX_RIGHT] && ishit_fireball(cn,ch[cn].x+n,ch[cn].y,ch[co].x,ch[co].y,fight_driver_fireball_enemy_check)) {
					task[maxtask].task=moveright;
					maxvalue+=(task[maxtask].value=fight_driver_fireball_value(cn,co)/n+1);
					maxtask++;
					break;
				}
				if (!dirdead[DX_LEFT] && ishit_fireball(cn,ch[cn].x-n,ch[cn].y,ch[co].x,ch[co].y,fight_driver_fireball_enemy_check)) {
					task[maxtask].task=moveleft;
					maxvalue+=(task[maxtask].value=fight_driver_fireball_value(cn,co)/n+1);
					maxtask++;
					break;
				}
				if (!dirdead[DX_DOWN] && ishit_fireball(cn,ch[cn].x,ch[cn].y+n,ch[co].x,ch[co].y,fight_driver_fireball_enemy_check)) {
					task[maxtask].task=movedown;
					maxvalue+=(task[maxtask].value=fight_driver_fireball_value(cn,co)/n+1);
					maxtask++;
					break;
				}
				if (!dirdead[DX_UP] && ishit_fireball(cn,ch[cn].x,ch[cn].y-n,ch[co].x,ch[co].y,fight_driver_fireball_enemy_check)) {
					task[maxtask].task=moveup;
					maxvalue+=(task[maxtask].value=fight_driver_fireball_value(cn,co)/n+1);
					maxtask++;
					break;
				}
			}
		}
	}

        if (ch[cn].value[0][V_FLASH]>1 &&
	    strike_damage(cn,co,ch[cn].value[0][V_FLASH])>POWERSCALE/10 &&
	    ch[cn].mana>=FLASHCOST &&
	    may_add_spell(cn,IDR_FLASHY) &&
	    cdist>10 &&
	    cdist<30) {
		if (calc_steps_ball(cn,ch[cn].x,ch[cn].y,ch[co].x,ch[co].y)>tdist*2-5) {
			task[maxtask].task=ball;
			maxvalue+=(task[maxtask].value=fight_driver_directball_value(cn,co));
			maxtask++;
		}
	}

	if (ch[cn].value[0][V_FLASH]>1 &&
	    strike_damage(cn,co,ch[cn].value[0][V_FLASH])>POWERSCALE/10 &&
	    ch[cn].mana>=FLASHCOST &&
	    tdist<4 &&
	    may_add_spell(cn,IDR_FLASH)) {
		task[maxtask].task=flash;
		maxvalue+=(task[maxtask].value=fight_driver_flash_value(cn,co));
		maxtask++;
	}

	if (ch[cn].value[0][V_FIRE]>1 &&
	    fireball_damage(cn,co,ch[cn].value[0][V_FIRE])>=POWERSCALE/10 &&
	    ch[cn].mana>=FIREBALLCOST &&
	    tdist<2 &&
	    may_add_spell(cn,IDR_FIRE) &&
	    !has_spell(cn,IDR_FLASH)) {
		task[maxtask].task=firering;
		maxvalue+=(task[maxtask].value=fight_driver_firering_value(cn,co));
		maxtask++;
	}

	if (!nowarcry &&
	    ch[cn].value[0][V_WARCRY]>1 &&
	    warcry_value(cn,co,ch[cn].value[0][V_WARCRY])<=-1 &&
	    ch[cn].endurance>=WARCRYCOST &&
	    cdist<8 &&
	    may_add_spell(co,IDR_WARCRY)) {
		task[maxtask].task=warcry;
		maxvalue+=(task[maxtask].value=fight_driver_warcry_value(cn,co));
		maxtask++;
	}

	if ((!nomove || cdist==2) && !((int)ch[co].level-(int)ch[cn].level>13 &&
				       (int)ch[co].level-(int)ch[cn].level>(int)ch[cn].level/4 &&
				       (areaID==4 || areaID==7 || areaID==9) &&
				       !(ch[cn].flags&CF_PLAYER))) {
		task[maxtask].task=attack;
		maxvalue+=(task[maxtask].value=fight_driver_attack_value(cn,co));
		maxtask++;
	}

	// we're not full and regenerating
	if (ch[cn].mana<ch[cn].value[0][V_MANA]*POWERSCALE || ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE) {
		task[maxtask].task=regenerate;
		maxvalue+=(task[maxtask].value=fight_driver_regen_value(cn,co,dat));
                maxtask++;
	}

	tmp=0;	
	if (!nomove &&
	    ch[cn].mana>POWERSCALE*3 &&
	    ch[cn].value[1][V_FREEZE] && freeze_value(cn,co)<=-1 &&
	    (may_add_spell(co,IDR_FREEZE) && tdist>3)) {
		tmp+=fight_driver_distance3_value(cn,co);
	}
	if (!nomove && ch[cn].mana>POWERSCALE*3 && ch[cn].value[1][V_FLASH] && !may_add_spell(cn,IDR_FLASH)) {
		tmp+=fight_driver_distance3_flash_value(cn,co);
	}
	if (tmp) {
		task[maxtask].task=distance3;
		maxvalue+=(task[maxtask].value=tmp);
		maxtask++;
	}
	
	if (!nomove &&
	    ch[cn].mana>FIREBALLCOST &&
	    ch[cn].value[1][V_FIRE] &&
	    fireball_damage(cn,co,ch[cn].value[0][V_FIRE])>=POWERSCALE/10 &&
	    may_add_spell(cn,IDR_FLASH) &&
	    ch[cn].value[1][V_FIRE]>ch[cn].value[1][V_FLASH]) {
		task[maxtask].task=distance7;
		maxvalue+=(task[maxtask].value=fight_driver_distance7_value(cn,co));
		maxtask++;
	}

        if (!nomove && (tmp=fight_driver_attackback_value(cn,co))) {
		task[maxtask].task=attackback;
		maxvalue+=(task[maxtask].value=tmp);
		maxtask++;
	}

	if (0 && ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 && (tmp=HIGH_PRIO)) {
		task[maxtask].task=flee;
		maxvalue+=(task[maxtask].value=tmp);
		maxtask++;
	}

	if ((int)ch[co].level-(int)ch[cn].level>13 && (int)ch[co].level-(int)ch[cn].level>(int)ch[cn].level/4 && (areaID==4 || areaID==7 || areaID==9) && !(ch[cn].flags&CF_PLAYER)) {
		tmp=HIGH_PRIO*2;
		task[maxtask].task=flee;
		maxvalue+=(task[maxtask].value=tmp);
		maxtask++;
	}

        if (sillyness>1) {
		for (n=0; n<maxtask; n++) {
			task[n].value+=RANDOM(sillyness);
		}
	}

        qsort(task,maxtask,sizeof(task[0]),task_cmp);
		
	/*//if (ch[cn].flags&CF_PLAYER) {
		say(cn,"---");
		for (n=0; n<maxtask; n++) {
			say(cn,"%d: value %d, type %s",cn,task[n].value,typename[task[n].task]);
		}
	//}*/
        for (n=0; n<maxtask; n++) {
		//if (ch[cn].flags&CF_PLAYER)
			//say(cn,"doing %s",typename[task[n].task]);
		if (task[n].task==attackback && (n==maxtask-1 || task[n+1].task!=attack)) ret=0;
		else switch(task[n].task) {
			     case freeze:		ret=do_freeze(cn); break;
			     case bless:		ret=do_bless(cn,cn); break;
			     case fireball:		ret=fireball_driver(cn,co,ch[co].serial); break;
			     case ball:			ret=do_ball(cn,ch[co].x-1+RANDOM(3),ch[co].y-1+RANDOM(3)); break;
			     case flash:		ret=do_flash(cn); break;
			     case warcry:		ret=do_warcry(cn); break;
			     case attack:		ret=attack_driver(cn,co); break;
			     case moveright:		ret=do_walk(cn,DX_RIGHT); break;
			     case moveleft:		ret=do_walk(cn,DX_LEFT); break;
			     case moveup:		ret=do_walk(cn,DX_UP); break;
			     case movedown:		ret=do_walk(cn,DX_DOWN); break;
			     case regenerate:		ret=do_idle(cn,TICKS/2); break;
			     case distance3:		ret=distance_driver(cn,co,3); if (!ret && error==ERR_ALREADY_THERE) ret=do_idle(cn,TICKS/4); break;
			     case distance7:		ret=distance_driver(cn,co,7); break;
			     case earthrain:		ret=earthrain_driver(cn,co,ch[cn].value[1][V_DEMON]); break;
			     case earthmud:		ret=earthmud_driver(cn,co,ch[cn].value[1][V_DEMON]); break;
			     case heal:			ret=do_heal(cn,cn); break;
			     case ms:			ret=do_magicshield(cn); break;
			     case attackback:		ret=attack_back_driver(cn,co); break;
			     case flee:			if (cdist<12) ret=distance_driver(cn,co,12);
							else { ret=0; error=ERR_ALREADY_THERE; }
							if (!ret && error==ERR_ALREADY_THERE) ret=do_idle(cn,TICKS/4); break;
			     case firering:		ret=do_fireball(cn,ch[cn].x,ch[cn].y); break;


			default:	ret=0; break;
		}
		if (ret) return 1;
		//say(cn,"failed.");
	}

        // emergency solution... ????
	if ((!nomove || cdist==2) && char_dist(cn,co)<4 && attack_driver(cn,co)) { /* say(cn,"all failed, doing attack"); */ return 1; }

        return 0;
}

static int mex,mey,mecn;
int person_cmp(const void *a,const void *b)
{
	int tmpa,tmpb;

	if (((struct person*)b)->cn && !((struct person*)a)->cn) return 1;
	if (!((struct person*)b)->cn && ((struct person*)a)->cn) return -1;

	if (((struct person*)b)->visible && !((struct person*)a)->visible) return 1;
	if (!((struct person*)b)->visible && ((struct person*)a)->visible) return -1;

	if (((struct person*)b)->hurtme && !((struct person*)a)->hurtme) return 1;
	if (!((struct person*)b)->hurtme && ((struct person*)a)->hurtme) return -1;

        tmpa=abs(((struct person *)a)->lastx-mex)+abs(((struct person *)a)->lasty-mey);
	tmpb=abs(((struct person *)b)->lastx-mex)+abs(((struct person *)b)->lasty-mey);
	if (tmpb<tmpa) return 1;
	if (tmpa<tmpb) return -1;
	if (is_facing(mecn,((struct person*)b)->cn)) return 1;
	if (is_facing(mecn,((struct person*)a)->cn)) return -1;
	return 0;
}

// add enemy to fight driver data structures. returns 1 if new enemy, 0 otherwise
// be aware that the driver remembers only up to 10 enemies, so it might still be
// an old enemy.
int fight_driver_add_enemy(int cn,int co,int hurtme,int visible)
{
	struct fight_driver_data *dat;
	int n,ID,m;

        if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

        // dont add enemy if he is further than start_dist away from home
	if (!hurtme && dat->start_dist && fight_driver_dist_from_home(cn,co)>dat->start_dist) {
		//say(cn,"(%d) rejected %s because of home_dist %d",cn,ch[co].name,dat->home_dist);
		return 0;
	}

	// dont add enemy if he is further than char_dist away current position
	if (!hurtme && dat->char_dist && char_dist(cn,co)>dat->char_dist) {
		//say(cn,"(%d) rejected %s because of char_dist %d",cn,ch[co].name,dat->char_dist);
		return 0;
	}

	if (ch[co].x<1 || ch[co].x>=MAXMAP || ch[co].y<1 || ch[co].y>=MAXMAP) return 0;
	
	m=ch[co].x+ch[co].y*MAXMAP;
	if (!hurtme && (map[m].flags&MF_NEUTRAL)) return 0;	// dont attack if enemy is in neutral zone and didnt hurt us
	
	ID=charID(co);

	/*for (n=0; n<10; n++)
		if (dat->enemy[n].ID==ID) break;
	if (n==10) {
                memmove(dat->enemy+1,dat->enemy,sizeof(dat->enemy)-sizeof(dat->enemy[0]));
	} else memmove(dat->enemy+1,dat->enemy,sizeof(dat->enemy)-sizeof(dat->enemy[0])*(10-n));
	
	dat->enemy[0].cn=co;
	dat->enemy[0].ID=ID;
	dat->enemy[0].lastx=ch[co].x;
	dat->enemy[0].lasty=ch[co].y;
	dat->enemy[0].visible=visible;
	dat->enemy[0].hurtme=hurtme;
	if (n==10) return 1;*/

	for (n=0; n<9; n++)
		if (dat->enemy[n].ID==ID) break;

	dat->enemy[n].cn=co;
	dat->enemy[n].ID=ID;
	dat->enemy[n].lastx=ch[co].x;
	dat->enemy[n].lasty=ch[co].y;
	dat->enemy[n].visible=visible;
	dat->enemy[n].hurtme=hurtme;

	mecn=cn;
	mex=ch[cn].x;
	mey=ch[cn].y;
	qsort(dat->enemy,10,sizeof(struct person),person_cmp);

	/*if (1 || RANDOM(100)==0) {
		for (n=0; n<10; n++) {
			if (!dat->enemy[n].cn) continue;
			
			say(cn,"%02d: v=%d, h=%d, d=%d, f=%d",
			    n,
			    dat->enemy[n].visible,
			    dat->enemy[n].hurtme,
			    abs(mex-dat->enemy[n].lastx)+abs(mey-dat->enemy[n].lasty),
			    is_facing(cn,dat->enemy[n].cn));
		}
	}*/

        if (n==9) return 1;

	return 0;
}

void fight_driver_note_hit(int cn)
{
	struct fight_driver_data *dat;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return;

	dat->lasthit=ticker;
}

int fight_driver_remove_enemy(int cn,int co)
{
	struct fight_driver_data *dat;
	int n,ID;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

	ID=charID(co);

	for (n=0; n<10; n++) {	
		if (dat->enemy[n].ID==ID) {
			dat->enemy[n].cn=dat->enemy[n].ID=0;
			return 1;
		}
	}

	return 0;
}

// takes a look around, updates	friend and enemy data structure
int fight_driver_update(int cn)
{
	struct fight_driver_data *dat;
	int n,co;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

        for (n=0; n<10; n++) {
		if (!(co=dat->enemy[n].cn)) continue;
		if (!ch[co].flags) { dat->enemy[n].cn=dat->enemy[n].ID=0; continue; }			// deleted (dead)? trash

                if (dat->enemy[n].ID!=charID(co)) { dat->enemy[n].cn=dat->enemy[n].ID=0; continue; }	// different ID? trash

		if (!can_attack(cn,co)) { dat->enemy[n].cn=dat->enemy[n].ID=0; continue; } 		// not attackable anymore? trash

		//say(cn,"enemy %s at %d %d",ch[co].name,dat->enemy[n].lastx,dat->enemy[n].lasty);

                if (char_see_char(cn,co)) {
			// if enemy is too far away from respawn point, remove him from the list
			if (dat->stop_dist && fight_driver_dist_from_home(cn,co)>dat->stop_dist) {
                                dat->enemy[n].cn=dat->enemy[n].ID=0;
				//say(cn,"removed enemy %s (stop dist)",ch[co].name);
                                continue;
			}

			dat->enemy[n].visible=1;
			dat->enemy[n].lastx=ch[co].x;
			dat->enemy[n].lasty=ch[co].y;
			//say(cn,"set %s coords to %d,%d",ch[co].name,ch[co].x,ch[co].y);
		} else dat->enemy[n].visible=0;
	}

	return 1;
}

// will attack the closest visible enemy
// assumes that the data is up to date
// returns 1 if it is acting (called do_xxx), 0 if no action was needed
int fight_driver_attack_visible(int cn,int nomove)
{
	struct fight_driver_data *dat;
	int n,bscore,bn,dist,score,co;
	int bad[10]={0,0,0,0,0,0,0,0,0,0};

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

        while (42) {
		bscore=0; bn=-1;
		for (n=0; n<10; n++) {
			if (bad[n]) continue;
			if (!(co=dat->enemy[n].cn)) continue;
			if (!dat->enemy[n].visible) continue;
	
			dist=char_dist(cn,dat->enemy[n].cn);
	
			score=(999-dist)*10;			// distance
			//if (dat->enemy[n].hurtme) score+=5;	// prefer those who hit me before
			if (is_facing(cn,co)) score+=5;		// prefer the one i'm facing
			//score+=(200-ch[co].level)*5;		// prefer low levels :P
			//if (!RANDOM(20)) score+=10;		// plus some randomness

                        //say(cn,"%s - score %d",ch[co].name,score);
	
			if (score>bscore) { bscore=score; bn=n; }
		}
		if (bn==-1) return 0;

		//say(cn,"target is %s",ch[dat->enemy[bn].cn].name);
		if (fight_driver_attack_enemy(cn,dat->enemy[bn].cn,nomove)) return 1;
		bad[bn]=1;
	}
}

// will hunt down one invisible enemy
// assumes that the data is up to date
// returns 1 if it is acting (called do_xxx), 0 if no action was needed
int fight_driver_follow_invisible(int cn)
{
	struct fight_driver_data *dat;
	int n,dir,co;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

        for (n=0; n<10; n++) {
		if (!(co=dat->enemy[n].cn)) continue;
		if (dat->enemy[n].visible) continue;
		if ((int)ch[co].level-(int)ch[cn].level>13 &&
		    (int)ch[co].level-(int)ch[cn].level>(int)ch[cn].level/4 &&
		    (areaID==4 || areaID==7 || areaID==9) &&
		    !(ch[cn].flags&CF_PLAYER)) continue;
		break;
	}
	if (n==10) return 0;	

        // we're at his last position, but didnt find him there. give up
	if (abs(ch[cn].x-dat->enemy[n].lastx)<2 && abs(ch[cn].y-dat->enemy[n].lasty)<2) {
		dat->enemy[n].cn=0;
		return 0;
	}

	dir=pathfinder(ch[cn].x,ch[cn].y,dat->enemy[n].lastx,dat->enemy[n].lasty,0,NULL,0);
	if (dir==-1) dir=pathfinder(ch[cn].x,ch[cn].y,dat->enemy[n].lastx,dat->enemy[n].lasty,1,NULL,0);
	if (dir==-1) {	// we cannot go to his place, nor close to it. give up.
		dat->enemy[n].cn=0;
		return 0;
	}

	return walk_or_use_driver(cn,dir);
}

// home_dist is the maximum distance from the respawn point
// we add new enemies, char_dist is the maximum distance from
// the character we add new enemies, stop_dist is the distance
// from the respawn point at which we stop pursuing them.
int fight_driver_set_dist(int cn,int start_dist,int char__dist,int stop_dist)
{
	struct fight_driver_data *dat;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

	dat->start_dist=start_dist;
	dat->char_dist=char__dist;
	dat->stop_dist=stop_dist;

	return 1;
}

// set home position. if none is set, respawn point is used
int fight_driver_set_home(int cn,int x,int y)
{
	struct fight_driver_data *dat;

	if (!(dat=set_data(cn,DRD_FIGHTDRIVER,sizeof(struct fight_driver_data)))) return 0;

	dat->home_x=x;
	dat->home_y=y;

	return 1;
}

int fight_driver_reset(int cn)
{
	del_data(cn,DRD_FIGHTDRIVER);

	return 1;
}

// calculates the distance co is away from cn's respawn point
int dist_from_home(int cn,int co)
{
	int dx,dy;

	dx=abs(ch[co].x-ch[cn].tmpx);
	dy=abs(ch[co].y-ch[cn].tmpy);
	
	if (dx>dy) return (dx<<1)+dy;
	else return (dy<<1)+dx;
}

// checks if cn is wearing an item with the signature dr0...dr4
// in drdata[0]...[4]
int is_wearing(int cn,int dr0,int dr1,int dr2,int dr3,int dr4)
{
	int in,n;

	for (n=0; n<12; n++) {
		if (!(in=ch[cn].item[n])) continue;
		if (dr0!=-1 && it[in].drdata[0]!=dr0) continue;
		if (dr1!=-1 && it[in].drdata[1]!=dr1) continue;
		if (dr2!=-1 && it[in].drdata[2]!=dr2) continue;
		if (dr3!=-1 && it[in].drdata[3]!=dr3) continue;
		if (dr4!=-1 && it[in].drdata[4]!=dr4) continue;
		return 1;
	}

	return 0;
}

// checks if character cn has the item with ID
// returns the item number on success, 0 otherwise
int has_item(int cn,int ID)
{
	int n,in;

	for (n=0; n<INVENTORYSIZE; n++)
		if ((in=ch[cn].item[n]) && it[in].ID==ID) return in;
	
	if ((in=ch[cn].citem) && it[in].ID==ID) return in;

	return 0;
}

void destroy_item(int in)
{
	if (it[in].content) destroy_item_container(in);
        free_item(in);
}

// use the spells magicshield, heal and bless when apropriate
int spell_self_driver(int cn)
{
        if (ch[cn].value[0][V_BLESS] && ch[cn].mana>=BLESSCOST && may_add_spell(cn,IDR_BLESS) && do_bless(cn,cn)) { return 1; }
        if (ch[cn].value[0][V_MAGICSHIELD]*POWERSCALE/MAGICSHIELDMOD>ch[cn].lifeshield && ch[cn].mana>=POWERSCALE*3 && do_magicshield(cn)) { return 1; }
	if (ch[cn].value[0][V_HEAL] && ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 && ch[cn].mana>=POWERSCALE*3 && !has_spell(cn,IDR_HEAL) && do_heal(cn,cn)) { return 1; }

        return 0;
}

// stand still when regeneration of hp/mana is needed
int regenerate_driver(int cn)
{	
	if (ch[cn].mana<ch[cn].value[0][V_MANA]*POWERSCALE) return do_idle(cn,TICKS);
        if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE) return do_idle(cn,TICKS);

	return 0;
}

// does standard reactions to see, seehit and gothit messages
// by adding enemies to fight_driver
// messages are not removed
void standard_message_driver(int cn,struct msg *msg,int agressive,int helper)
{
	int co,cc;

	switch(msg->type) {
		case NT_CHAR:		// did we see someone?
                        // valid enemy?
			if (!agressive || !is_valid_enemy(cn,(co=msg->dat1),-1)) break;
                        fight_driver_add_enemy(cn,co,0,1);
			break;
	
		case NT_SEEHIT:		// did we see someone get hit?
			if (!helper) break;

			cc=msg->dat1;
			co=msg->dat2;
			if (!cc || !co) break;
	
			// is the victim our friend? then help
			if (co!=cn && ch[co].group==ch[cn].group) {
				if (!is_valid_enemy(cn,cc,-1)) break;
	
				//say(cn,"help victim %s against %s",ch[co].name,ch[cc].name);
				fight_driver_add_enemy(cn,cc,1,1);
				break;
			}
	
			// is the attacker our friend? then help
			if (cc!=cn && ch[cc].group==ch[cn].group) {
				if (!is_valid_enemy(cn,co,-1)) break;
	
				//say(cn,"help attacker %s against %s",ch[cc].name,ch[co].name);
				fight_driver_add_enemy(cn,co,0,1);
				break;
			}
			break;
	
		case NT_GOTHIT:		// did we get hit?

			fight_driver_note_hit(cn);

			co=msg->dat1;
			if (!co) break;

			//say(cn,"defend against %s (%d)?",ch[co].name,msg->dat2);

                        if (ch[cn].group==ch[co].group) break;
			if (!can_attack(cn,co)) break;
			
			//say(cn,"defend against %s (%d)!",ch[co].name,msg->dat2);

			if (!char_see_char(cn,co)) fight_driver_add_enemy(cn,co,1,0);
			else fight_driver_add_enemy(cn,co,1,1);

			
			break;		
	}
}

// check if the standard door at x,y is closed
int is_closed(int x,int y)
{
	int in;


	if (x<0 || x>=MAXMAP || y<0 || y>=MAXMAP) return 0;
        if (!(in=map[x+y*MAXMAP].it)) return 0;
        if (it[in].driver!=IDR_DOOR) return 0;
	
	return !it[in].drdata[0];
}

// check if the room given by xs,ys -> xe,ye is free of players
int is_room_empty(int xs,int ys,int xe,int ye)
{
	int x,y,cn;

	if (xs<0 || xs>=MAXMAP || ys<0 || ys>=MAXMAP || xe<0 || xe>=MAXMAP || ye<0 || ye>=MAXMAP) return 0;

	for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (cn=getfirst_char_sector(x,y); cn; cn=ch[cn].sec_next) {
				if ((ch[cn].flags&CF_PLAYER) && ch[cn].x>=xs && ch[cn].x<=xe && ch[cn].y>=ys && ch[cn].y<=ye) {
					return 0;
				}
			}
		}
	}
	return 1;
}

// use the item located at x,y
int use_item_at(int cn,int x,int y,int spec)
{
	int in,dir;

	if (x<0 || x>=MAXMAP || y<0 || y>=MAXMAP) return 0;
        if (!(in=map[x+y*MAXMAP].it)) return 0;

        if (use_driver(cn,in,spec)) return 1;

	dir=pathfinder(ch[cn].x,ch[cn].y,x,y,1,NULL,0);
	if (dir!=-1) return walk_or_use_driver(cn,dir);
	
	return 0;	
}

int add_bonus_spell(int cn,int driver,int strength,int duration)
{
	int fre,in,endtime;

	if (!(fre=may_add_spell(cn,driver))) return 0;

        switch(driver) {
		case IDR_ARMOR:		in=create_item("armor_spell"); break;
		case IDR_WEAPON:	in=create_item("weapon_spell"); break;
		case IDR_MANA:		in=create_item("mana_spell"); break;
		case IDR_HP:		in=create_item("hp_spell"); break;

		default:		in=0; break;
	}
        if (!in) return 0;

        it[in].mod_value[0]=strength;
	
	it[in].driver=driver;

	endtime=ticker+duration;

	*(unsigned long*)(it[in].drdata)=endtime;

	it[in].carried=cn;

        ch[cn].item[fre]=in;

	create_spell_timer(cn,in,fre);

	update_char(cn);

	return 1;
}

int teleport_char_driver(int cn,int x,int y)
{
	int oldx,oldy;

	if (abs(ch[cn].x-x)+abs(ch[cn].y-y)<2) return 0;

	oldx=ch[cn].x;
	oldy=ch[cn].y;

	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;
	if (ch[cn].player) player_driver_stop(ch[cn].player,0);

	if (drop_char(cn,x,y,0)) return 1;

	drop_char(cn,oldx,oldy,0);

	return 0;
}

int teleport_char_driver_extended(int cn,int x,int y,int maxdist)
{
	int oldx,oldy;

	if (abs(ch[cn].x-x)+abs(ch[cn].y-y)<2) return 0;

	oldx=ch[cn].x;
	oldy=ch[cn].y;

	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;
	if (ch[cn].player) player_driver_stop(ch[cn].player,0);

	if (drop_char_extended(cn,x,y,maxdist)) return 1;

	drop_char(cn,oldx,oldy,0);

	return 0;
}


int secure_move_driver(int cn,int tx,int ty,int dir,int ret,int lastact)
{
	if (ch[cn].x!=tx || ch[cn].y!=ty) {
                if ((lastact!=AC_USE || ret!=2) && move_driver(cn,tx,ty,0)) return 1;
		if (teleport_char_driver(cn,tx,ty)) return 1;
		return 0;
	} else {
		if (ch[cn].dir!=dir) turn(cn,dir);
		return 0;
	}
}




























