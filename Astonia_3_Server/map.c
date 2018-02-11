/*

$Id: map.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: map.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:23  sam
Added RCS tags


*/

#include "server.h"
#include "light.h"
#include "log.h"
#include "create.h"
#include "expire.h"
#include "effect.h"
#include "drdata.h"
#include "notify.h"
#include "libload.h"
#include "container.h"
#include "sector.h"
#include "area.h"
#include "tool.h"
#include "map.h"
#include "path.h"
#include "drvlib.h"
#include "consistency.h"
#include "btrace.h"

// set item to map tile x,y. checks for errors, free space and does light.
int set_item_map(int in,int x,int y)
{
	int m;

	if (in<1 || in>=MAXITEM) {
		elog("set_item(): illegal item number %d",in);
		return 0;
	}

	if (x<1 || x>=MAXMAP || y<1 || y>MAXMAP) {
		elog("set_item(): illegal coordinates (%d,%d)",x,y);
		return 0;
	}

	if (!it[in].flags) {
		elog("set_item(): trying to drop unused item %s (%d) at %d,%d",it[in].name,in,x,y);
		return 0;
	}

        m=x+y*MAXMAP;

	if (map[m].it || (map[m].flags&(MF_TMOVEBLOCK|MF_MOVEBLOCK))) return 0;

	map[m].it=in;

	if (it[in].flags&IF_MOVEBLOCK) map[m].flags|=MF_TMOVEBLOCK;
	if (it[in].flags&IF_SIGHTBLOCK) map[m].flags|=MF_TSIGHTBLOCK;
	
	it[in].x=x;
	it[in].y=y;
	it[in].carried=0;
	it[in].contained=0;

	if (it[in].flags&IF_TAKE) set_expire(in,ITEM_DECAY_TIME);

	add_item_light(in);

	set_sector(it[in].x,it[in].y);
	//notify_area(it[in].x,it[in].y,NT_ITEM,in,0,0);

	return 1;
}

// remove item from map. checks for errors and does light.
// will also update the character values if needed.
int remove_item_map(int in)
{
	int m;

	if (in<1 || in>=MAXITEM) {
		elog("remove_item(): illegal item number %d",in);
		return 0;
	}

        if (it[in].x<1 || it[in].x>=MAXMAP-1 || it[in].y<1 || it[in].y>=MAXMAP-1) {
		elog("remove_item(): Item %s (%d) thinks it is on pos %d,%d, but that pos is out of bounds.",
			it[in].name,in,it[in].x,it[in].y);
		it[in].x=it[in].y=0;
		return 1;	// we return success anyway. we did remove it, sort of...
	}
	m=it[in].x+it[in].y*MAXMAP;

	set_sector(it[in].x,it[in].y);

	if (map[m].it!=in) {
		elog("remove_item(): Item %s (%d) thinks it is on pos %d,%d, but it is not.",
			it[in].name,in,it[in].x,it[in].y);
		it[in].x=it[in].y=0;
		return 1;	// we return success anyway. we did remove it, sort of...
	}

        remove_item_light(in);

        if (it[in].flags&IF_MOVEBLOCK) {
		if (!(map[m].flags&MF_TMOVEBLOCK))
			elog("remove_item(): Item %s (%d) has IF_MOVEBLOCK set but the map on it's position %d,%d has no MF_TMOVEBLOCK",
				it[in].name,in,it[in].x,it[in].y);
		else map[m].flags&=~MF_TMOVEBLOCK;
	}
	
	if (it[in].flags&IF_SIGHTBLOCK) {
		if (!(map[m].flags&MF_TSIGHTBLOCK))
			elog("remove_item(): Item %s (%d) has IF_SIGHTBLOCK set but the map on it's position %d,%d has no MF_TSIGHTBLOCK",
				it[in].name,in,it[in].x,it[in].y);
		else map[m].flags&=~MF_TSIGHTBLOCK;
	}

	map[m].it=0;
	it[in].x=it[in].y=0;
	
	return 1;	
}

// remove item from char. checks for errors and does light.
// will also update the character values if needed.
int remove_item_char(int in)
{
	int cn,n;

	if (in<1 || in>=MAXITEM) {
		elog("remove_item_char(): illegal item number %d",in);
		return 0;
	}

	if (!(cn=it[in].carried)) {
		elog("remove_item_char(): item %s (%d) is not carried",it[in].name,in);
		btrace("remove_item_char");
		return 0;
	}

	if (cn<1 || cn>=MAXCHARS) {
		elog("remove_item_char(): Item %s (%d) thinks it is carried by character %d, but that number is out of bounds.",it[in].name,in,cn);
		it[in].carried=0;

		return 1;	// we return success anyway. we did remove it, sort of...
	}
	
	if (ch[cn].citem==in) {
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		it[in].carried=0;
		return 1;
	} else {
		for (n=0; n<INVENTORYSIZE; n++)
			if (ch[cn].item[n]==in)
				break;
		if (n==INVENTORYSIZE) {
			elog("remove_item(): Item %s (%d) thinks it is carried by character %s (%d), but it is not.",it[in].name,in,ch[cn].name,cn);
			// we're going to return success anyway: the character does NOT carry the item anymore
		} else ch[cn].item[n]=0;
		ch[cn].flags|=CF_ITEMS;
		it[in].carried=0;
		
		if (n<30) update_char(cn);	// worn item or spell

		return 1;
	}
}

// replace item old by item new.
// old must be carried by a character,
// new must not be on map, char or container
int replace_item_char(int old,int new)
{
	int cn,n;

	if (old<1 || old>=MAXITEM) {
		elog("replace_item(): illegal old item number %d",old);
		return 0;
	}

	if (new<1 || new>=MAXITEM) {
		elog("replace_item(): illegal new item number %d",new);
		return 0;
	}

	if (!(cn=it[old].carried)) return 0;

	if (cn<1 || cn>=MAXCHARS) {
		elog("replace_item(): Item %s (%d) thinks it is carried by character %d, but that number is out of bounds.",it[old].name,old,cn);
		btrace("replace_item_char");
                return 0;
	}
	
	if (ch[cn].citem==old) {
		ch[cn].citem=new;
		ch[cn].flags|=CF_ITEMS;
		it[old].carried=0;
		it[new].carried=cn;
		return 1;
	} else {
		for (n=0; n<INVENTORYSIZE; n++)
			if (ch[cn].item[n]==old)
				break;
		if (n==INVENTORYSIZE) {
			elog("replace_item(): Item %s (%d) thinks it is carried by character %s (%d), but it is not.",it[old].name,old,ch[cn].name,cn);
			btrace("replace_item_char");
			return 0;
		}
		
		ch[cn].item[n]=new;
		ch[cn].flags|=CF_ITEMS;
		it[old].carried=0;
		it[new].carried=cn;
		
		if (n<30) update_char(cn);	// worn item or spell

		return 1;
	}
}

// sets char on map, does light.
int set_char(int cn,int x,int y,int nosteptrap)
{
	int m,in;

	if (cn<1 || cn>=MAXCHARS) {
		elog("set_char(): illegal character number %d",cn);
		return 0;
	}

	if (x<1 || x>=MAXMAP || y<1 || y>MAXMAP) {
		elog("set_char(): illegal coordinates (%d,%d (%s))",x,y,ch[cn].name);
		return 0;
	}
	
	m=x+y*MAXMAP;

	if (map[m].ch || (map[m].flags&(MF_TMOVEBLOCK|MF_MOVEBLOCK))) return 0;

	map[m].ch=cn;
	map[m].flags|=MF_TMOVEBLOCK;

        ch[cn].x=x;
	ch[cn].y=y;

	ch[cn].tox=0;
	ch[cn].toy=0;

	set_sector(x,y);
	add_char_light(cn);
        add_char_sector(cn);

	walk_section_msg(cn);

	if ((map[m].flags&MF_NOMAGIC) && !(ch[cn].flags&CF_NOMAGIC)) {
		ch[cn].flags|=CF_NOMAGIC;
		update_char(cn);
	}
	if (!(map[m].flags&MF_NOMAGIC) && (ch[cn].flags&CF_NOMAGIC)) {
		ch[cn].flags&=~CF_NOMAGIC;
		update_char(cn);
	}

	if ((in=map[m].it)) {
		if (in<1 || in>=MAXITEM) {
			elog("set_char(): found illegal item %d at %d,%d. removing.",in,x,y);
			map[m].it=0;
			return 1;
		}		
		if (!nosteptrap && (it[in].flags&IF_STEPACTION)) {
			item_driver(it[in].driver,in,cn);
		}
	}

	return 1;
}

// removes char from map. does light.
int remove_char(int cn)
{
	int m;

	if (cn<1 || cn>=MAXCHARS) {
		elog("set_char(): illegal character number %d",cn);
		return 0;
	}

        if (ch[cn].x<1 || ch[cn].x>=MAXMAP-1 || ch[cn].y<1 || ch[cn].y>=MAXMAP-1) {
		elog("remove_char(): Char %s (%d) thinks it is on pos %d,%d, but that pos is out of bounds.",
			ch[cn].name,cn,ch[cn].x,ch[cn].y);
		ch[cn].x=ch[cn].y=0;		
	} else { // character coords seem to be correct
		m=ch[cn].x+ch[cn].y*MAXMAP;

		set_sector(ch[cn].x,ch[cn].y);
		del_char_sector(cn);

		if (map[m].ch!=cn) {
			elog("remove_char(): Char %s (%d) thinks it is on pos %d,%d, but it is not.",ch[cn].name,cn,ch[cn].x,ch[cn].y);
			ch[cn].x=ch[cn].y=0;
		} else {	// character coords are correct
			remove_char_light(cn);
	
                        if (!(map[m].flags&MF_TMOVEBLOCK))
				elog("remove_char(): Char %s (%d) was on the map but the map on it's position %d,%d has no MF_TMOVEBLOCK",ch[cn].name,cn,ch[cn].x,ch[cn].y);
			else map[m].flags&=~MF_TMOVEBLOCK;

			map[m].ch=0;
			ch[cn].x=ch[cn].y=0;
		}		
	}

	if (ch[cn].tox<1 || ch[cn].tox>=MAXMAP-1 || ch[cn].toy<1 || ch[cn].toy>=MAXMAP-1) {
		if (ch[cn].tox) elog("remove_char(): Char %s (%d) thinks it is going to pos %d,%d, but that pos is out of bounds.",ch[cn].name,cn,ch[cn].tox,ch[cn].toy);
		ch[cn].tox=ch[cn].toy=0;		
	} else {
		m=ch[cn].tox+ch[cn].toy*MAXMAP;

		if (ch[cn].tox) {
			if (!(map[m].flags&MF_TMOVEBLOCK))
				elog("remove_char(): Char %s (%d) was moving to position %d,%d that has no MF_TMOVEBLOCK",
					ch[cn].name,cn,ch[cn].tox,ch[cn].toy);
			else map[m].flags&=~MF_TMOVEBLOCK;
		}

		ch[cn].tox=ch[cn].toy=0;
	}

        return 1;		

}

int drop_item(int in,int x,int y)
{
	if (set_item_map(in,x,y)) return 1;

	// direct neighbors
	if (set_item_map(in,x+1,y)) return 1;
	if (set_item_map(in,x,y+1)) return 1;
	if (set_item_map(in,x-1,y)) return 1;
	if (set_item_map(in,x,y-1)) return 1;

	// diagonal neighbors
	if (set_item_map(in,x+1,y+1)) return 1;
	if (set_item_map(in,x-1,y+1)) return 1;
	if (set_item_map(in,x+1,y-1)) return 1;
	if (set_item_map(in,x-1,y-1)) return 1;

	return 0;
}

int drop_char(int cn,int x,int y,int nosteptrap)
{
	if (set_char(cn,x,y,nosteptrap)) return 1;

	// direct neighbors
	if (x<MAXMAP-1 && set_char(cn,x+1,y,nosteptrap)) return 1;
	if (y<MAXMAP-1 && set_char(cn,x,y+1,nosteptrap)) return 1;
	if (x>1 && set_char(cn,x-1,y,nosteptrap)) return 1;
	if (y>1 && set_char(cn,x,y-1,nosteptrap)) return 1;

	// diagonal neighbors
	if (x<MAXMAP-1 && y<MAXMAP-1 && set_char(cn,x+1,y+1,nosteptrap)) return 1;
	if (x>1 && y<MAXMAP-1 && set_char(cn,x-1,y+1,nosteptrap)) return 1;
	if (x<MAXMAP-1 && y>1 && set_char(cn,x+1,y-1,nosteptrap)) return 1;
	if (x>1 && y>1 && set_char(cn,x-1,y-1,nosteptrap)) return 1;

	return 0;
}


static int path_ignore_char(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
        if (map[m].ch) return 1;
        if (map[m].flags&MF_TMOVEBLOCK) return 0;

	return 1;
}

int drop_char_extended(int cn,int x,int y,int maxdist)
{
	int dx,dy;

	if (set_char(cn,x,y,0)) return 1;

	for (dx=1; dx<maxdist; dx++) {
		// direct neighbors
		if (x+dx<MAXMAP-1 && pathfinder(x,y,x+dx,y,0,path_ignore_char,20)!=-1 && set_char(cn,x+dx,y,0)) return 1;
		if (y+dx<MAXMAP-1 && pathfinder(x,y,x,y+dx,0,path_ignore_char,20)!=-1 && set_char(cn,x,y+dx,0)) return 1;
		if (x-dx>1 && pathfinder(x,y,x-dx,y,0,path_ignore_char,20)!=-1 && set_char(cn,x-dx,y,0)) return 1;
		if (y-dx>1 && pathfinder(x,y,x,y-dx,0,path_ignore_char,20)!=-1 && set_char(cn,x,y-dx,0)) return 1;
		for (dy=1; dy<maxdist; dy++) {
			// diagonal neighbors
			if (x+dx<MAXMAP-1 && y+dy<MAXMAP-1 && pathfinder(x,y,x+dx,y+dy,0,path_ignore_char,20)!=-1 && set_char(cn,x+dx,y+dy,0)) return 1;
			if (x-dx>1 && y+dy<MAXMAP-1 && pathfinder(x,y,x-dx,y+dy,0,path_ignore_char,20)!=-1 && set_char(cn,x-dx,y+dy,0)) return 1;
			if (x+dx<MAXMAP-1 && y-dy>1 && pathfinder(x,y,x+dx,y-dy,0,path_ignore_char,20)!=-1 && set_char(cn,x+dx,y-dy,0)) return 1;
			if (x-dx>1 && y-dy>1 && pathfinder(x,y,x-dx,y-dy,0,path_ignore_char,20)!=-1 && set_char(cn,x-dx,y-dy,0)) return 1;
		}
	}

	return 0;
}

int drop_item_extended(int in,int x,int y,int maxdist)
{
	int dx,dy;

	if (set_item_map(in,x,y)) return 1;

	for (dx=1; dx<maxdist; dx++) {
		//xlog("trying %d,%d",dx,0);
		if (x+dx<MAXMAP-1 && pathfinder(x,y,x+dx,y,0,path_ignore_char,20)!=-1 && set_item_map(in,x+dx,y)) return 1;

		//xlog("trying %d,%d",0,dx);
		if (y+dx<MAXMAP-1 && pathfinder(x,y,x,y+dx,0,path_ignore_char,20)!=-1 && set_item_map(in,x,y+dx)) return 1;
		
		//xlog("trying %d,%d",-dx,0);
		if (x-dx>1 && pathfinder(x,y,x-dx,y,0,path_ignore_char,20)!=-1 && set_item_map(in,x-dx,y)) return 1;

		//xlog("trying %d,%d",0,-dx);
		if (y-dx>1 && pathfinder(x,y,x,y-dx,0,path_ignore_char,20)!=-1 && set_item_map(in,x,y-dx)) return 1;
		
		for (dy=1; dy<=dx; dy++) {
			//xlog("trying %d,%d",dx,dy);
			if (x+dx<MAXMAP-1 && y+dy<MAXMAP-1 && pathfinder(x,y,x+dx,y+dy,0,path_ignore_char,20)!=-1 && set_item_map(in,x+dx,y+dy)) return 1;
			//xlog("trying %d,%d",-dx,dy);
			if (x-dx>1 && y+dy<MAXMAP-1 && pathfinder(x,y,x-dx,y+dy,0,path_ignore_char,20)!=-1 && set_item_map(in,x-dx,y+dy)) return 1;
			//xlog("trying %d,%d",dx,-dy);
			if (x+dx<MAXMAP-1 && y-dy>1 && pathfinder(x,y,x+dx,y-dy,0,path_ignore_char,20)!=-1 && set_item_map(in,x+dx,y-dy)) return 1;
			//xlog("trying %d,%d",-dx,-dy);
			if (x-dx>1 && y-dy>1 && pathfinder(x,y,x-dx,y-dy,0,path_ignore_char,20)!=-1 && set_item_map(in,x-dx,y-dy)) return 1;

			//xlog("trying %d,%d",dy,dx);
			if (x+dy<MAXMAP-1 && y+dx<MAXMAP-1 && pathfinder(x,y,x+dy,y+dx,0,path_ignore_char,20)!=-1 && set_item_map(in,x+dy,y+dx)) return 1;
			//xlog("trying %d,%d",-dy,dx);
			if (x-dy>1 && y+dx<MAXMAP-1 && pathfinder(x,y,x-dy,y+dx,0,path_ignore_char,20)!=-1 && set_item_map(in,x-dy,y+dx)) return 1;
			//xlog("trying %d,%d",dy,-dx);
			if (x+dy<MAXMAP-1 && y-dx>1 && pathfinder(x,y,x+dy,y-dx,0,path_ignore_char,20)!=-1 && set_item_map(in,x+dy,y-dx)) return 1;
			//xlog("trying %d,%d",-dy,-dx);
			if (x-dy>1 && y-dx>1 && pathfinder(x,y,x-dy,y-dx,0,path_ignore_char,20)!=-1 && set_item_map(in,x-dy,y-dx)) return 1;
		}
	}
	
	return 0;
}

int item_drop_char(int in,int cn)
{
	int x,y;

	x=it[in].x;
	y=it[in].y;

	if (set_char(cn,x,y,0)) return 1;

	// in front
	if (x<MAXMAP-1 && set_char(cn,x+1,y,0)) return 1;
	if (y<MAXMAP-1 && set_char(cn,x,y+1,0)) return 1;
        if (x<MAXMAP-1 && y<MAXMAP-1 && set_char(cn,x+1,y+1,0)) return 1;

	// behind
        if (!(it[in].flags&IF_FRONTWALL)) {
		if (x>1 && set_char(cn,x-1,y,0)) return 1;
		if (y>1 && set_char(cn,x,y-1,0)) return 1;
		if (x>1 && y>1 && set_char(cn,x-1,y-1,0)) return 1;
		if (x>1 && y<MAXMAP-1 && set_char(cn,x-1,y+1,0)) return 1;
		if (x<MAXMAP-1 && y>1 && set_char(cn,x+1,y-1,0)) return 1;
	}

	// in front
	if (x<MAXMAP-2 && set_char(cn,x+2,y,0)) return 1;
	if (y<MAXMAP-2 && set_char(cn,x,y+2,0)) return 1;
        if (x<MAXMAP-2 && y<MAXMAP-1 && set_char(cn,x+2,y+1,0)) return 1;
	if (x<MAXMAP-1 && y<MAXMAP-2 && set_char(cn,x+1,y+2,0)) return 1;
        if (x<MAXMAP-2 && y<MAXMAP-2 && set_char(cn,x+2,y+2,0)) return 1;
	if (x<MAXMAP-2 && y<MAXMAP-2 && set_char(cn,x+2,y+2,0)) return 1;

	// behind
        if (!(it[in].flags&IF_FRONTWALL)) {
		if (x>1 && set_char(cn,x-1,y,0)) return 1;
		if (y>1 && set_char(cn,x,y-1,0)) return 1;
		if (x>1 && y>1 && set_char(cn,x-1,y-1,0)) return 1;
                if (x>2 && set_char(cn,x-2,y,0)) return 1;
		if (y>2 && set_char(cn,x,y-2,0)) return 1;
		if (x>2 && y>1 && set_char(cn,x-2,y-1,0)) return 1;
		if (x>1 && y>2 && set_char(cn,x-1,y-2,0)) return 1;
		if (x>2 && y<MAXMAP-1 && set_char(cn,x-2,y+1,0)) return 1;
		if (x<MAXMAP-1 && y>2 && set_char(cn,x+1,y-2,0)) return 1;
		if (x>1 && y<MAXMAP-2 && set_char(cn,x-1,y+2,0)) return 1;
		if (x<MAXMAP-2 && y>1 && set_char(cn,x+2,y-1,0)) return 1;
		if (x>2 && y<MAXMAP-2 && set_char(cn,x-2,y+2,0)) return 1;
		if (x>2 && y<MAXMAP-2 && set_char(cn,x-2,y+2,0)) return 1;
		if (x<MAXMAP-2 && y>2 && set_char(cn,x+2,y-2,0)) return 1;
		if (x<MAXMAP-2 && y>2 && set_char(cn,x+2,y-2,0)) return 1;
		if (x>2 && y>2 && set_char(cn,x-2,y-2,0)) return 1;		
	}

        return 0;
}

// remove everything from the map to see if there are any leftover
// characters/items/effects. WARNING: destroys game data!
void check_map(void)
{
	int m,n,cn,in,fn;

	for (m=0; m<MAXMAP*MAXMAP; m++) {
		if ((cn=map[m].ch)) {
			remove_destroy_char(cn);
			map[m].ch=0;
		}
		if ((in=map[m].it) && !(it[in].flags&IF_SIGHTBLOCK)) {
			remove_item_map(in);
                        destroy_item(in);
			map[m].it=0;
		}
		for (n=0; n<4; n++) {
			if ((fn=map[m].ef[n])) {
				remove_effect(fn);
				free_effect(fn);
			}
		}
	}
	for (m=0; m<MAXMAP*MAXMAP; m++) {
		if ((cn=map[m].ch)) {
			remove_destroy_char(cn);
			map[m].ch=0;
		}
		if ((in=map[m].it)) {
			remove_item_map(in);
                        destroy_item(in);
			map[m].it=0;
		}
		for (n=0; n<4; n++) {
			if ((fn=map[m].ef[n])) {
				remove_effect(fn);
				free_effect(fn);
			}
		}
	}

	for (cn=1; cn<MAXCHARS; cn++) {
		if (ch[cn].flags) {
			elog("Unlinked character %d, name=%s, pos=%d,%d (%llX)",cn,ch[cn].name,ch[cn].x,ch[cn].y,ch[cn].flags);
		}
	}
	for (in=1; in<MAXITEM; in++) {
		if (it[in].flags && !(it[in].flags&IF_VOID)) {
			elog("Unlinked item %d, name=%s, pos=%d,%d, carried=%d, container=%d",in,it[in].name,it[in].x,it[in].y,it[in].carried,it[in].contained);
		}
	}
	for (fn=1; fn<MAXEFFECT; fn++) {
		if (ef[fn].type) {
			elog("Unlinked effect %d, type=%d",fn,ef[fn].type);
		}
	}
}
