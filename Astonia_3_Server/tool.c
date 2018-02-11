/*
 * $Id: tool.c,v 1.10 2007/12/10 10:12:41 devel Exp $
 *
 * $Log: tool.c,v $
 * Revision 1.10  2007/12/10 10:12:41  devel
 * removed learning
 *
 * Revision 1.9  2006/10/06 17:27:12  devel
 * changed some clogs to dlogs
 *
 * Revision 1.8  2006/06/23 16:21:07  ssim
 * some fixes to teufel PK hooks
 *
 * Revision 1.7  2006/06/19 09:59:03  ssim
 * added display of demon suit type to look_item
 *
 * Revision 1.6  2006/04/24 17:22:42  ssim
 * added hooks and basics for teufelheim PK
 *
 * Revision 1.5  2006/04/07 11:00:03  ssim
 * added min level for +22 and +23 items
 *
 * Revision 1.4  2005/12/08 18:42:33  ssim
 * added exception to sanitize_item for demon reward items
 *
 * Revision 1.3  2005/11/27 19:16:08  ssim
 * removed serial display from look at item
 *
 * Revision 1.2  2005/09/24 10:10:04  ssim
 * fixed warning in buggy_items()
 *
 * Revision 1.1  2005/09/24 09:48:53  ssim
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "server.h"
#include "log.h"
#include "talk.h"
#include "direction.h"
#include "create.h"
#include "skill.h"
#include "player.h"
#include "drdata.h"
#include "drvlib.h"
#include "item_id.h"
#include "effect.h"
#include "timer.h"
#include "date.h"
#include "lookup.h"
#include "clan.h"
#include "do.h"
#include "chat.h"
#include "path.h"
#include "map.h"
#include "notify.h"
#include "tool.h"
#include "shrine.h"
#include "military.h"
#include "prof.h"
#include "act.h"
#include "poison.h"
#include "statistics.h"
#include "balance.h"
#include "bank.h"
#include "area.h"
#include "depot.h"
#include "lab.h"
#include "database.h"
#include "consistency.h"
#include "sector.h"
#include "lostcon.h"
#include "death.h"
#include "club.h"
#include "teufel_pk.h"

#if __GNUC_MINOR__==8
unsigned long long atoll(char *string)
{
        unsigned long long val=0;

        while (isspace(*string)) string++;

        while (isdigit(*string)) {
                val*=10;
                val+=*string-'0';
                string++;
        }
        return val;
}
#endif

// compare the ending of two strings
int endcmp(char *a,char *b)
{
	char *starta=a,*startb=b;
	int lena,lenb;

	while (*a) a++;
	while (*b) b++;

	lena=a-starta;
	lenb=b-startb;
	
	if (lenb>lena) return 0;

	a-=lenb;
	b-=lenb;

	return !strcmp(a,b);
}

// translate character speed into ticks
int speed(int speedy,int mode,int ticks)
{
	double f;

	if (speedy>0) speedy/=2;
	else speedy=speedy*0.75;

	if (mode==SM_FAST) speedy+=40;
	if (mode==SM_STEALTH) speedy-=40;

        f=0.75+speedy/288.0;

	if (f<0.2) f=0.2;
	if (f>2.0) f=2.0;

	ticks/=f;

	if (ticks<2) return 2;
	if (ticks>255) return 255;

	return ticks;
}

int speed2(int speedy,int mode,int ticks)
{
	double f;

	if (speedy>0) speedy/=2;
	else speedy=speedy*0.75;

	if (mode==SM_FAST) speedy+=40;
	if (mode==SM_STEALTH) speedy-=40;

        f=0.75+speedy/288.0;

	if (f<0.2) f=0.2;
	if (f>2.0) f=2.0;

	ticks=ceil(ticks*f);
	
	if (ticks<2) return 2;
	if (ticks>255) return 255;

	return ticks;
}

// translate direction into map offset. sets diag if the
// direction is diagonal.
int dx2offset(int dir,int *x,int *y,int *diag)
{
	switch(dir) {
		case DX_UP:		if (x) *x= 0; if (y) *y=-1; if (diag) *diag=0; break;
		case DX_DOWN:		if (x) *x= 0; if (y) *y= 1; if (diag) *diag=0; break;
		case DX_LEFT:		if (x) *x=-1; if (y) *y= 0; if (diag) *diag=0; break;
		case DX_RIGHT:		if (x) *x= 1; if (y) *y= 0; if (diag) *diag=0; break;
		case DX_LEFTUP:		if (x) *x=-1; if (y) *y=-1; if (diag) *diag=1; break;
		case DX_RIGHTUP:	if (x) *x= 1; if (y) *y=-1; if (diag) *diag=1; break;
		case DX_LEFTDOWN:	if (x) *x=-1; if (y) *y= 1; if (diag) *diag=1; break;
		case DX_RIGHTDOWN:	if (x) *x= 1; if (y) *y= 1; if (diag) *diag=1; break;
		default:                return 0;
	}
	return 1;
}

// returns the direction pointing from frx,fry to tox,toy
int offset2dx(int frx,int fry,int tox,int toy)
{
	int dx,dy;

	dx=tox-frx;
	dy=toy-fry;

	if (abs(dx)/2>abs(dy)) dy=0;
	if (abs(dy)/2>abs(dx)) dx=0;
	
	if (dx>0) {
		if (dy>0) return DX_RIGHTDOWN;
		if (dy<0) return DX_RIGHTUP;
		return DX_RIGHT;
	}

	if (dx<0) {
		if (dy>0) return DX_LEFTDOWN;
		if (dy<0) return DX_LEFTUP;
		return DX_LEFT;
	}
	if (dy>0) return DX_DOWN;
	if (dy<0) return DX_UP;

	return 0;
}

#define MAXHATE	50

struct pk_ppd
{
	int kills;		// nr of kills
	int deaths;		// nr of deaths
	
	int last_kill;		// realtime of last kill
	int last_death;		// realtime of last death

	int hate[MAXHATE];	// IDs of enemies
};

static int arena_check_target(int m)
{
	if (!(map[m].flags&MF_ARENA)) return 0;

	return 1;
}

int can_help(int cn,int co)
{
	int m;
	unsigned long long mf1,mf2;

	if (cn<0 || cn>=MAXCHARS) {
                elog("can_help(): called with illegal character cn=%d",cn);
                return 0;
	}

	if (co<1 || co>=MAXCHARS) {
		elog("can_help(): called with illegal character co=%d (cn=%s,%d)",co,ch[cn].name,cn);
                return 0;
	}

        if (ch[co].flags&CF_DEAD) return 0;

	if (cn==co) return 1;
	if (!cn) return 1;

	if ((ch[co].flags&CF_PLAYER) && (ch[co].flags&CF_PK) && !is_hate_empty(co)) return 0;

	m=ch[cn].x+ch[cn].y*MAXMAP;
	mf1=map[m].flags;

	m=ch[co].x+ch[co].y*MAXMAP;
	mf2=map[m].flags;

	// no help unless neither is in an arena or both are in the same arena
	if ((mf1&MF_ARENA) && (mf2&MF_ARENA) && pathfinder(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y,0,arena_check_target,0)!=-1) return 1;	// both in same
        if ((mf1&MF_ARENA) || (mf2&MF_ARENA)) return 0;	// one in arena or both in different arenas
	
	// everything else is fair game
	return 1;
}

// check if cn may attack co. cn may be zero, in that case, almost all
// attacks are legal.
int can_attack(int cn,int co)
{
	int c1,c2,m;
	unsigned long long mf1,mf2;

        if (cn<0 || cn>=MAXCHARS) {
                elog("can_attack(): called with illegal character cn=%d",cn);
                return 0;
	}

	if (co<1 || co>=MAXCHARS) {
		elog("can_attack(): called with illegal character co=%d (cn=%s,%d)",co,ch[cn].name,cn);
		{ char *ptr=NULL; *ptr=0; }
		return 0;
	}

	if (!(ch[co].flags)) return 0;

        if (ch[co].flags&(CF_DEAD|CF_NOATTACK)) return 0;

	if (cn==co) return 0;

	if (!cn) return 1;

	// players may not attack characters with CF_NOPLRATT set
	if ((ch[cn].flags&(CF_PLAYER|CF_PLAYERLIKE)) && (ch[co].flags&CF_NOPLRATT)) return 0;
	if ((ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE)) && (ch[cn].flags&CF_NOPLRATT)) return 0;

	m=ch[cn].x+ch[cn].y*MAXMAP;
	mf1=map[m].flags;

	m=ch[co].x+ch[co].y*MAXMAP;
	mf2=map[m].flags;

	// check if one of them is in a peace-zone
	if ((mf1&MF_PEACE) || (mf2&MF_PEACE)) return 0;

	// everything's fair in the arena
	if ((mf1&MF_ARENA) && (mf2&MF_ARENA) && pathfinder(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y,0,arena_check_target,0)!=-1) {
		// unless its a clan-area too, then no attacking of same clan or allied clan
		if ((mf1&MF_CLAN) && (mf2&MF_CLAN) && (c1=get_char_clan(cn)) && (c2=get_char_clan(co)) && (c1==c2 || clan_alliance(c1,c2))) return 0;
		return 1;
	}

	// clans may attack under special circumstances
	if ((c1=get_char_clan(cn)) && (c2=get_char_clan(co)) && areaID!=1) {
		if ((mf1&MF_CLAN) && (mf2&MF_CLAN) && clan_can_attack_inside(c1,c2)) return 1;
                if (clan_can_attack_outside(c1,c2) && abs(ch[cn].level-ch[co].level)<=3) return 1;
	}

	// one of them is 'player-like', dont allow attacks between him and players/player-likes
	if ((ch[cn].flags&(CF_PLAYER|CF_PLAYERLIKE)) && (ch[co].flags&CF_PLAYERLIKE)) return 0;
	if ((ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE)) && (ch[cn].flags&CF_PLAYERLIKE)) return 0;

        // player against player. check for PK and deny anything else
	if ((ch[cn].flags&CF_PLAYER) && (ch[co].flags&CF_PLAYER)) {
		struct pk_ppd *ppd;
		int n;

		if (areaID==1) return 0;

		if (!check_hate(cn,co)) {
                        del_hate(cn,co);
			return 0;
		}

		if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS

		for (n=0; n<MAXHATE; n++)
			if (ppd->hate[n]==ch[co].ID)
                                return 1;			
			
		return 0;
	}

	// at least one NPC is fighting, unless they're of the same group...
	if (ch[cn].group==ch[co].group) return 0;

	// ... clans members may not attack each other if they're of the same clan or of an alliance...
	if ((c1=get_char_clan(cn)) && (c2=get_char_clan(co))) {
                if (c1==c2) return 0;
		if (clan_alliance(c1,c2)) return 0;
	}

	// ... everything else is fair game
        return 1;
}

int check_hate(int cn,int co)
{
	if (cn==co) return 0;

	if (!(ch[cn].flags&CF_PLAYER) || !(ch[cn].flags&CF_PK) ||
	    !(ch[co].flags&CF_PLAYER) || !(ch[co].flags&CF_PK)) return 0;

	if (abs(ch[cn].level-ch[co].level)>3) return 0;
	
	return 1;
}

int is_hate_empty(int cn)
{
	struct pk_ppd *ppd;
	int n;

        if (!(ch[cn].flags&CF_PLAYER) || !(ch[cn].flags&CF_PK)) return 1;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 1;
	for (n=0; n<MAXHATE; n++)
		if (ppd->hate[n]) return 0;
	
        return 1;
}

int on_hate_list(int cn,int co)
{
	struct pk_ppd *ppd;
	int n;

	if (!(ch[cn].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS
	for (n=0; n<MAXHATE; n++)
		if (ppd->hate[n]==ch[co].ID) return 1;
	
	return 0;
}

int get_pk_relation(int cn,int co)
{
        int me,him;

        if (ch[co].flags&CF_PK) {
		
		him=on_hate_list(co,cn);
		me=on_hate_list(cn,co);

		if (him && me) return 5;
		if (him) return 4;
		if (me) return 3;

		if (check_hate(cn,co)) return 2;
		return 1;
	}

	return 0;
}

void add_hate(int cn,int co)
{
	struct pk_ppd *ppd;
	int n;

	if (!check_hate(cn,co)) return;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return;	// OOPS

	for (n=0; n<MAXHATE-1; n++)
		if (ppd->hate[n]==ch[co].ID) break;			
	
	if (n) memmove(ppd->hate+1,ppd->hate,sizeof(int)*(n));
	ppd->hate[0]=ch[co].ID;	

	ch[cn].flags&=~CF_LAG;

	if (n==MAXHATE) {
		log_char(cn,LOG_SYSTEM,0,"Added %s to hate list",ch[co].name);	
		
		reset_name(cn);
		reset_name(co);
	}
}

int del_hate(int cn,int co)
{
	struct pk_ppd *ppd;
	int n;

	if (!(ch[cn].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS
	for (n=0; n<MAXHATE; n++) {
		if (ppd->hate[n]==ch[co].ID) {
			ppd->hate[n]=0;
			log_char(cn,LOG_SYSTEM,0,"Removed %s from hate list",ch[co].name);

			reset_name(cn);
			reset_name(co);

			return 1;
		}
	}
	return 0;
}

int del_hate_ID(int cn,int uID)
{
	struct pk_ppd *ppd;
	int n;

	if (!(ch[cn].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS
	for (n=0; n<MAXHATE; n++) {
		if (ppd->hate[n]==uID) {
			ppd->hate[n]=0;
			log_char(cn,LOG_SYSTEM,0,"Removed from hate list");

			reset_name(cn);
			return 1;
		}
	}
	return 0;
}

int del_all_hate(int cn)
{
	struct pk_ppd *ppd;
	int n;

	if (!(ch[cn].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS
	for (n=0; n<MAXHATE; n++) {
                ppd->hate[n]=0;		
	}
	return 1;
}

void list_hate(int cn)
{
	struct pk_ppd *ppd;
	int n,res,flag=0;
	char name[80];

	if (!(ch[cn].flags&CF_PLAYER) || !(ch[cn].flags&CF_PK)) return;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return;	// OOPS

	for (n=0; n<MAXHATE; n++) {
		if (ppd->hate[n]) {
			res=lookup_ID(name,ppd->hate[n]);
			if (res>=0) { log_char(cn,LOG_SYSTEM,0,"Hate: %s",name); flag=1; }
		}
	}
	if (!flag) log_char(cn,LOG_SYSTEM,0,"List is empty.");	
}

int leave_pk(int cn)
{
	struct pk_ppd *ppd;

	if (!(ch[cn].flags&CF_PK)) {
		log_char(cn,LOG_SYSTEM,0,"But you are not a playerkiller anyway.");
		return 0;
	}

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) {
		log_char(cn,LOG_SYSTEM,0,"Could not attach data block, please contact game admin and tell them you found bug #1774");
		return 0;	// OOPS
	}

	if (ppd->last_kill+60*60*24*28>realtime) {
		log_char(cn,LOG_SYSTEM,0,"You have killed %.2f days ago, you need to wait %.2f more days.",
			 (realtime-ppd->last_kill)/(60*60*24.0),(ppd->last_kill+60*60*24*28-realtime)/(60*60*24.0));

		return 0;
	}
	
	ch[cn].flags&=~CF_PK;
	del_data(cn,DRD_PK_PPD);	// remove unneeded data block

	dlog(cn,0,"Left PK");

	reset_name(cn);

        return 1;
}

int join_pk(int cn)
{
	if (ch[cn].flags&CF_PK) return 0;
	
	del_data(cn,DRD_PK_PPD);	// make sure its empty
	
	if (!set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd))) return 0;	// OOPS

	ch[cn].flags|=CF_PK;

	reset_name(cn);

	dlog(cn,0,"Turned PK");

	return 1;
}

int add_pk_kill(int cn,int co)
{
	struct pk_ppd *ppd;

	if (!(ch[cn].flags&CF_PLAYER) || !(ch[cn].flags&CF_PK) ||
	    !(ch[co].flags&CF_PLAYER) || !(ch[co].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS

	ppd->kills++;
	ppd->last_kill=realtime;

	return 1;
}

int add_pk_steal(int cn)
{
	struct pk_ppd *ppd;

	if (!(ch[cn].flags&CF_PLAYER) || !(ch[cn].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS

        ppd->last_kill=realtime;

	return 1;
}

int add_pk_death(int cn)
{
	struct pk_ppd *ppd;

	if (!(ch[cn].flags&CF_PLAYER) || !(ch[cn].flags&CF_PK)) return 0;

	if (!(ppd=set_data(cn,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS

	ppd->deaths++;
	ppd->last_death=realtime;

	return 1;
}

int show_pk_info(int cn,int co,char *buf)
{
	struct pk_ppd *ppd;

	if (!(ch[co].flags&CF_PLAYER) || !(ch[co].flags&CF_PK)) return 0;

	if (!(ppd=set_data(co,DRD_PK_PPD,sizeof(struct pk_ppd)))) return 0;	// OOPS

	return sprintf(buf,"%s is a player killer. %s killed %d players and died %d times through the hands of other players. ",
		 ch[co].name,Hename(co),
		 ppd->kills,ppd->deaths);	
}

int check_requirements(int cn,int in)
{
	int v1,v2,m;

	// check requirements
	for (m=0; m<MAXMOD; m++) {
		if (!(v2=it[in].mod_value[m])) continue;	// ignore empty ones
		 v1=it[in].mod_index[m];
		 if (v1<=-V_MAX || v1>=V_MAX) {
			 elog("can_wear(): item %s (%d) has illegal modifier (index %d out of bounds). Removing modifier.",it[in].name,in,v1);
			 it[in].mod_value[m]=0;
			 it[in].mod_index[m]=0;
			 continue;
		 }
		 if (v1<0) {	// negative indizes are requirements			
			 if (ch[cn].value[1][-v1]<v2) return 0;
		 }
	}

	if (it[in].min_level && ch[cn].level<it[in].min_level) return 0;
	if (it[in].max_level && ch[cn].level>it[in].max_level) return 0;
	
        if ((it[in].needs_class&1) && (ch[cn].flags&CF_MAGE)) return 0;
	if ((it[in].needs_class&2) && (ch[cn].flags&CF_WARRIOR)) return 0;
	if ((it[in].needs_class&4) && (ch[cn].flags&(CF_MAGE|CF_WARRIOR))!=(CF_MAGE|CF_WARRIOR)) return 0;
	if ((it[in].needs_class&8) && !(ch[cn].flags&CF_ARCH)) return 0;

	if ((it[in].flags&IF_BONDWEAR) && it[in].ownerID!=ch[cn].ID) return 0;

	return 1;
}

// check if the character can wear the item at the given position
int can_wear(int cn,int in,int pos)
{
	int inr,inl;

	if (cn<1 || cn>=MAXCHARS) {
		elog("can_wear(): called with illegal character cn=%d",cn);
		return 0;
	}

	if (in<1 || in>=MAXITEM) {
		elog("can_wear(): called with illegal item in=%d",in);
		return 0;
	}
	
	if (pos<0 || pos>11) {
		elog("can_wear(): called with illegal pos pos=%d",pos);
		return 0;
	}

	// check position
	inl=ch[cn].item[WN_LHAND];
	if (inl<0 || inl>=MAXITEM) {
		elog("can_wear(): illegal item %d in WN_LHAND of character %s (%d) found. Removing.",in,ch[cn].name,cn);
		inl=ch[cn].item[WN_LHAND]=0;
	}

	inr=ch[cn].item[WN_RHAND];
	if (inr<0 || inr>=MAXITEM) {
		elog("can_wear(): illegal item %d in WN_RHAND of character %s (%d) found. Removing.",in,ch[cn].name,cn);
		inl=ch[cn].item[WN_RHAND]=0;
	}

	switch(pos) {
		case WN_HEAD:	if (!(it[in].flags&IF_WNHEAD)) return 0; break;
		case WN_NECK:	if (!(it[in].flags&IF_WNNECK)) return 0; break;
		case WN_BODY:	if (!(it[in].flags&IF_WNBODY)) return 0; break;
		case WN_ARMS:	if (!(it[in].flags&IF_WNARMS)) return 0; break;
		case WN_BELT:	if (!(it[in].flags&IF_WNBELT)) return 0; break;
		case WN_LEGS:	if (!(it[in].flags&IF_WNLEGS)) return 0; break;
		case WN_FEET:	if (!(it[in].flags&IF_WNFEET)) return 0; break;
                case WN_CLOAK:	if (!(it[in].flags&IF_WNCLOAK)) return 0; break;
		case WN_LRING:	if (!(it[in].flags&IF_WNLRING)) return 0; break;
		case WN_RRING:	if (!(it[in].flags&IF_WNRRING)) return 0; break;

		case WN_LHAND:	if (inr && (it[inr].flags&IF_WNTWOHANDED)) return 0;	// left hand is blocked by two-handed weapon
				if (!(it[in].flags&IF_WNLHAND)) return 0;
				break;

		case WN_RHAND:	if (it[in].flags&IF_WNTWOHANDED) {			// is it a two-handed weapon?
					if (inl) return 0;				// yes, but if left hand is full, its still illegal
				} else if (!(it[in].flags&IF_WNRHAND)) return 0;	// no, but can it be worn on right hand?
				break;
	}

	return check_requirements(cn,in);	
}

// check if the character can carry the item
int can_carry(int cn,int in,int quiet)
{
	int n,in2;

	if (cn<1 || cn>=MAXCHARS) {
		elog("can_carry(): called with illegal character cn=%d",cn);
		return 0;
	}

	if (in<1 || in>=MAXITEM) {
		elog("can_carry(): called with illegal item in=%d",in);
		return 0;
	}

	if (IDR_ONECARRY(it[in].driver)) {
		for (n=0; n<INVENTORYSIZE; n++) {
			if (n>=12 && n<30) continue;			
			if ((in2=ch[cn].item[n]) && it[in2].driver==it[in].driver) break;
		}
		if (n<INVENTORYSIZE || ((in2=ch[cn].citem) && it[in2].driver==it[in].driver)) {
			if (!quiet) log_char(cn,LOG_SYSTEM,0,"You can only carry one %s at a time!",it[in].name);
			return 0;
		}
	}

	if ((it[in].flags&IF_BONDTAKE) && it[in].ownerID!=ch[cn].ID) return 0;
	
	return 1;
}

int get_fight_skill(int cn)
{
	int in,m=0;

	if (cn<1 || cn>=MAXCHARS) {
		elog("get_fight_skill(): called with illegal character %d",cn);
		return 0;
	}

	in=ch[cn].item[WN_RHAND];

	if (in<0 || in>=MAXITEM) {
		elog("get_fight_skill(): character %s (%d) is wielding illegal item %d. Removing.",ch[cn].name,cn,in);
		ch[cn].item[WN_RHAND]=0;
		in=0;
	}

	if (!in || !(it[in].flags&IF_WEAPON)) {	// not wielding anything, or not wielding a weapon
		return ch[cn].value[0][V_HAND];
	}

	if (it[in].flags&IF_HAND) m=max(m,ch[cn].value[0][V_HAND]);
	if (it[in].flags&IF_DAGGER) m=max(m,ch[cn].value[0][V_DAGGER]);
	if (it[in].flags&IF_STAFF) m=max(m,ch[cn].value[0][V_STAFF]);
	if (it[in].flags&IF_SWORD) m=max(m,ch[cn].value[0][V_SWORD]);
	if (it[in].flags&IF_TWOHAND) m=max(m,ch[cn].value[0][V_TWOHAND]);

	return m;
}

double get_spell_average(int cn)
{
	return ( ch[cn].value[0][V_BLESS]+
		 ch[cn].value[0][V_HEAL]+
		 ch[cn].value[0][V_FREEZE]+
		 ch[cn].value[0][V_MAGICSHIELD]+
		 ch[cn].value[0][V_FLASH]+
		 ch[cn].value[0][V_FIREBALL]+		
		 ch[cn].value[0][V_PULSE] )/8.0;
}

int tactics2melee(int val)
{
	//int str,str1,str2;
	//extern float new_system,old_system;

	//str1=val/2;
	//str2=val*0.375;

	//str=str1*old_system+str2*new_system;
	
        return val*0.375;
}

int tactics2immune(int val)
{
        return val*0.125;
}

int tactics2spell(int val)
{
        return val*0.125;
}

int spellpower(int cn,int v)
{
	return ch[cn].value[0][v]+tactics2spell(ch[cn].value[0][V_TACTICS]);
}

int get_parry_skill(int cn)
{
	int val;
	
        if (ch[cn].value[1][V_PARRY]) val=get_fight_skill(cn)+ch[cn].value[0][V_PARRY]*2+tactics2melee(ch[cn].value[0][V_TACTICS])+(ch[cn].rage/5/POWERSCALE);
	else if (ch[cn].flags&CF_EDEMON) val=get_fight_skill(cn)*3.5;
	else if (ch[cn].value[1][V_MAGICSHIELD]) val=get_fight_skill(cn)+ch[cn].value[0][V_MAGICSHIELD]*2;
	else val=get_fight_skill(cn)+get_spell_average(cn)*2;

        return val;
}

int get_attack_skill(int cn)
{
	int val; //,val1,val2;
	//extern float new_system,old_system;
	
	if (ch[cn].value[1][V_ATTACK]) val=get_fight_skill(cn)+ch[cn].value[0][V_ATTACK]*2+tactics2melee(ch[cn].value[0][V_TACTICS])+(ch[cn].rage/5/POWERSCALE);
	else if (ch[cn].flags&CF_EDEMON) val=get_fight_skill(cn)*3.5;
	else {
		//val1=get_fight_skill(cn)+get_spell_average(cn)*2-6;
		//val2=get_fight_skill(cn)+get_spell_average(cn)*2-(ch[cn].level);
		val=get_fight_skill(cn)+get_spell_average(cn)*2-(ch[cn].level);
		//say(cn,"attack_skill values str=%d old=%d new=%d weight=%.2f",val,val1,val2,new_system);
	}

        return val;
}

int get_surround_attack_skill(int cn)
{
	int val;

	
	if (ch[cn].value[1][V_SURROUND]) val=get_fight_skill(cn)+
						ch[cn].value[0][V_SURROUND]*2+
						tactics2melee(ch[cn].value[0][V_TACTICS])+
						(ch[cn].rage/5/POWERSCALE) - 12;
	else return 0;

	return val;
}

int die(int cnt,int sides)
{
	int val=0,n;

	if (sides<1) return 0;

	for (n=0; n<cnt; n++) {
		val+=RANDOM(sides)+1;
	}

	return val;
}

// X exp yield level Y
int exp2level(int val)
{
	return max(1,(int)(sqrt(sqrt(val))));
}

// to reach level X you need Y exp
int level2exp(int level)
{
	return pow(level,4);
}

// to reach level X you need Y exp
int level_value(int level)
{
	return pow(level+1,4)-pow(level,4);
}

char *save_number(int nr)
{
	static char buf[80];

	switch(nr) {
		case 0:		return "no";
		case 1:		return "one";
		case 2:		return "two";
		case 3: 	return "three";
		case 4: 	return "four";
		case 5: 	return "five";
		case 6: 	return "six";
		case 7:		return "seven";
		case 8:		return "eight";
		case 9:		return "nine";
		case 10:	return "ten";
		default:	sprintf(buf,"%d",nr);
				return buf;
	}
}

int check_levelup(int cn)
{
	char buf[256];
	int experience,flag=0;

	experience=max(ch[cn].exp,ch[cn].exp_used);

	while (exp2level(experience)>ch[cn].level) {
		ch[cn].level++;
		set_sector(ch[cn].x,ch[cn].y);
		log_char(cn,LOG_SYSTEM,0,"Thou gained a level! Thou art level %d now.",ch[cn].level);
		
		if (ch[cn].flags&CF_HARDCORE) ch[cn].saves=0;
		else {
			ch[cn].saves++;
		
			if (ch[cn].saves>10) ch[cn].saves=10;
		
			log_char(cn,LOG_SYSTEM,0,"Thy persistence has pleased Ishtar. He will save thee when thou art in need.");
			log_char(cn,LOG_SYSTEM,0,"Thou hast %s saves now.",save_number(ch[cn].saves));
		}

		if (ch[cn].level>=20 && !ch[cn].value[1][V_PROFESSION]) {
			ch[cn].value[1][V_PROFESSION]=1;
			log_char(cn,LOG_SYSTEM,0,"Thou mayest now choose to learn a profession.");
		}

		if (ch[cn].level%10==0) {
			sprintf(buf,"0000000000°c10Grats: %s is level %d now!",ch[cn].name,ch[cn].level);
			server_chat(6,buf);
		}

		reset_name(cn);

		dlog(cn,0,"gained a level");
		flag=1;
	}
	return flag;
}

// low level routine to give exp to a character
// MUST USE
void give_exp(int cn,int val)
{
        if (areaID==21) return;	// !!!!!!!!!!!!!!!!! hack !!!!!!!!!!!!!!!

	if (ch[cn].exp<200*200*200*200) ch[cn].exp+=val;

	check_levelup(cn);
}

void give_exp_bonus(int cn,int val)
{
        give_exp(cn,val);
}

void give_money(int cn,int val,char *reason)
{
	ch[cn].gold+=val;
	ch[cn].flags|=CF_ITEMS;
	if (val%100) log_char(cn,LOG_SYSTEM,0,"You received %dG %dS.",val/100,val%100);
	else log_char(cn,LOG_SYSTEM,0,"You received %dG.",val/100);
	dlog(cn,0,"received %dG, %dS for %s.",val/100,val%100,reason);
}

const char *hisname(int cn)
{
	if (ch[cn].flags&CF_MALE) return "his";
	if (ch[cn].flags&CF_FEMALE) return "her";
	return "its";
}

const char *Hisname(int cn)
{
	if (ch[cn].flags&CF_MALE) return "His";
	if (ch[cn].flags&CF_FEMALE) return "Her";
	return "Its";
}

const char *hename(int cn)
{
	if (ch[cn].flags&CF_MALE) return "he";
	if (ch[cn].flags&CF_FEMALE) return "she";
	return "it";
}

const char *Hename(int cn)
{
	if (ch[cn].flags&CF_MALE) return "He";
	if (ch[cn].flags&CF_FEMALE) return "She";
	return "It";
}

const char *himname(int cn)
{
	if (ch[cn].flags&CF_MALE) return "him";
	if (ch[cn].flags&CF_FEMALE) return "her";
	return "it";
}

const char *Sirname(int cn)
{
	if (ch[cn].flags&CF_MALE) return "Sir";
	if (ch[cn].flags&CF_FEMALE) return "Lady";
	return "Neuter";
}

void remove_spell(int cn,int in,int pos,int cserial,int iserial)
{
	int old_speed;
	//xlog("remove spell: %d %d %d %d %d",cn,in,pos,cserial,iserial);
	
	// char alive and the right one?
	if (!(ch[cn].flags) || ch[cn].serial!=cserial) {
		//xlog("char gone or wrong serial");
		return;
	}

	// item existant and the right one?
	if (!(it[in].flags) || it[in].serial!=iserial) {
		//xlog("item gone or wrong serial");
		return;
	}
	
	// item where we expect it to be?
	if (ch[cn].item[pos]!=in) {
		//xlog("item not found");
		return;
	}

        old_speed=ch[cn].value[0][V_SPEED];

	ch[cn].item[pos]=0;
        free_item(in);

	update_char(cn);

        if (it[in].driver==IDR_FREEZE && ch[cn].duration) {	// adjust duration and step to remove effect of freeze
		int old_dur,new_dur,real_dur;

		old_dur=ch[cn].duration;
		real_dur=speed2(old_speed,ch[cn].speed_mode,ch[cn].duration);
		new_dur=speed(ch[cn].value[0][V_SPEED],ch[cn].speed_mode,real_dur);

                //say(cn,"old_dur=%d, real_dur=%d, new_dur=%d",old_dur,real_dur,new_dur);
		//say(cn,"dur=%d, step=%d",ch[cn].duration,ch[cn].step);

		ch[cn].duration=new_dur;
		ch[cn].step=ch[cn].step*new_dur/old_dur;

		//say(cn,"dur=%d, step=%d",ch[cn].duration,ch[cn].step);
	}

	//xlog("removed");
}

// checks if a spell with the same driver is already working
// returns 0 if that is the case, or if the character does not
// have a free spell slot
// returns the number of the spell slot otherwise
int may_add_spell(int cn,int driver)
{
	int n,fre=0,in;

	for (n=12; n<30; n++) {
                if ((in=ch[cn].item[n])) {
			if (it[in].driver==driver) { // another spell with the same driver is already active
				if (driver==IDR_BLESS && *(unsigned*)(it[in].drdata)-ticker<TICKS*30) return n;				
                                return 0;
			}
		} else fre=n;
	}	
	if (fre==0) return 0;

	return fre;
}

int add_same_spell(int cn,int driver)
{
	int n,fre=0,in;

	for (n=12; n<30; n++) {
                if ((in=ch[cn].item[n])) {
			if (it[in].driver==driver) return n; // such a spell is already active, return its number
		} else fre=n;
	}	
	if (fre==0) return 0;

	return fre;
}

int add_spell(int cn,int driver,int duration,char *name)
{
	int in,fre=-1;

        if (!(fre=may_add_spell(cn,driver))) return 0;

        in=create_item(name);
	if (!in) return 0;

        it[in].driver=driver;

	*(signed long*)(it[in].drdata)=ticker+duration;
	*(signed long*)(it[in].drdata+4)=ticker;

	it[in].carried=cn;

        ch[cn].item[fre]=in;

	create_spell_timer(cn,in,fre);

	update_char(cn);

	return in;
}

int look_item(int cn,struct item *in)
{
	int n,v,m=0,r=0,s;

	if (!in->name[0]) return 1;	// no name means we dont display anything	

	log_char(cn,LOG_SYSTEM,0,"°c5%s:",in->name);
	if (in->description[0]) log_char(cn,LOG_SYSTEM,0,"%s",in->description);
	if (in->ID==IID_HARDKILL) log_char(cn,LOG_SYSTEM,0,"This is a level %d holy weapon.",in->drdata[37]);

	for (n=0; n<MAXMOD; n++) {
		if ((v=in->mod_value[n]) && (s=in->mod_index[n])>-1) {
			if (!m) {
				log_char(cn,LOG_SYSTEM,0,"°c5Modifiers:");
				m=1;
			}
			if (in->driver==IDR_DECAYITEM) {
                                log_char(cn,LOG_SYSTEM,0,"%s +%d (active: %+d)",skill[s].name,v,in->drdata[2]);
			} else {
				if (ch[cn].flags&CF_GOD) {
					if (s==V_ARMOR) log_char(cn,LOG_SYSTEM,0,"%s %+.2f °c1(%d: %d+%d)",skill[s].name,v/20.0,n,s,v);
					else log_char(cn,LOG_SYSTEM,0,"%s %+d °c1(%d: %d+%d)",skill[s].name,v,n,s,v);
				} else {
					if (s==V_ARMOR) log_char(cn,LOG_SYSTEM,0,"%s %+.2f",skill[s].name,v/20.0);
					else log_char(cn,LOG_SYSTEM,0,"%s %+d",skill[s].name,v);
				}
			}
		}
	}

	for (n=0; n<MAXMOD; n++) {
		if ((v=in->mod_value[n]) && (s=in->mod_index[n])<0) {
			if (!r) {
				log_char(cn,LOG_SYSTEM,0,"°c5Requirements:");
				r=1;
			}
			log_char(cn,LOG_SYSTEM,0,"%s %d (you have %d)",skill[-s].name,v,ch[cn].value[1][-s]);			
		}
	}
        if (!r && (in->min_level || in->max_level || in->needs_class)) {
		log_char(cn,LOG_SYSTEM,0,"°c5Requirements:");
		r=1;
	}
	if (in->min_level) log_char(cn,LOG_SYSTEM,0,"Minimum Level: %d",in->min_level);
	if (in->max_level) log_char(cn,LOG_SYSTEM,0,"Maximum Level: %d",in->max_level);

	if (in->needs_class&1) log_char(cn,LOG_SYSTEM,0,"Only usable by a Warrior.");
	if (in->needs_class&2) log_char(cn,LOG_SYSTEM,0,"Only usable by a Mage.");
	if (in->needs_class&4) log_char(cn,LOG_SYSTEM,0,"Only usable by a Seyan'Du.");
	if (in->needs_class&8) log_char(cn,LOG_SYSTEM,0,"Only usable by an Arch.");

	if (in->flags&IF_BONDTAKE) log_char(cn,LOG_SYSTEM,0,"This item is bonded to %s. Only %s can take it.",in->ownerID==ch[cn].ID ? "you" : "somebody else",in->ownerID==ch[cn].ID ? "you" : "he");
	if (in->flags&IF_BONDWEAR) log_char(cn,LOG_SYSTEM,0,"This item is bonded to %s. Only %s can wear it.",in->ownerID==ch[cn].ID ? "you" : "somebody else",in->ownerID==ch[cn].ID ? "you" : "he");

	if (in->flags&IF_QUEST) log_char(cn,LOG_SYSTEM,0,"This is a quest item. You cannot drop it or give it away.");
	if (in->flags&IF_NOENHANCE) log_char(cn,LOG_SYSTEM,0,"This item resists magic, so you cannot enhance it using orbs, metals or shrines.");
	if (in->flags&IF_BEYONDMAXMOD) log_char(cn,LOG_SYSTEM,0,"This item goes beyond maximum modifier limits.");

	if (in->driver==IDR_FLASK && in->drdata[2]) {
		log_char(cn,LOG_SYSTEM,0,"Duration: %d minutes.",in->drdata[3]);
	}
	if (in->driver==IDR_BEYONDPOTION) {
		log_char(cn,LOG_SYSTEM,0,"Duration: %d minutes.",in->drdata[0]);
	}
	if (in->driver==IDR_DECAYITEM) {
		log_char(cn,LOG_SYSTEM,0,"Duration: %d:%02d:%02d of %d:%02d:%02d active time used up.",
			((*(unsigned short*)(in->drdata+3))*2)/60/60,(((*(unsigned short*)(in->drdata+3))*2)/60)%60,((*(unsigned short*)(in->drdata+3))*2)%60,
			((*(unsigned short*)(in->drdata+5))*2)/60/60,(((*(unsigned short*)(in->drdata+5))*2)/60)%60,((*(unsigned short*)(in->drdata+5))*2)%60);
	}
	/*if (IDR_ISSPELL(in->driver)) {
		log_char(cn,LOG_SYSTEM,0,"Duration: %.2f minutes.",(*(unsigned long*)(in->drdata)-ticker)/TICKS/60.0);
	}*/

	if ((in->sprite>=59200 && in->sprite<59299) || in->sprite==59474) {	// gold item
		log_char(cn,LOG_SYSTEM,0,"The item has been gilded.");
	}
	if ((in->sprite>=59299 && in->sprite<=59390) || in->sprite==59473) {	// silver item
		log_char(cn,LOG_SYSTEM,0,"The item has been silvered.");
	}

	if ((in->sprite>=53000 && in->sprite<=53006)) {	// edemon suit
		log_char(cn,LOG_SYSTEM,0,"This is part of an earth demon suit.");
	}

	if ((in->sprite>=53025 && in->sprite<=53030)) {	// ice suit
		log_char(cn,LOG_SYSTEM,0,"This is part of an ice demon suit.");
	}

	if ((in->sprite>=53031 && in->sprite<=53036)) {	// fire suit
		log_char(cn,LOG_SYSTEM,0,"This is part of an fire demon suit.");
	}

        return 1;
}

static char *rankname[]={
	"nobody",		//0
	"Private",		//1
	"Private First Class",  //2
	"Lance Corporal",       //3
	"Corporal",             //4
	"Sergeant",             //5	lvl 30
	"Staff Sergeant",       //6
	"Master Sergeant",      //7
	"First Sergeant",       //8	lvl 45
	"Sergeant Major",       //9
	"Second Lieutenant",    //10	lvl 55
	"First Lieutenant",     //11
	"Captain",              //12
	"Major",                //13
	"Lieutenant Colonel",   //14
	"Colonel",              //15
	"Brigadier General",    //16
	"Major General",        //17
	"Lieutenant General",   //18
	"General",              //19
	"Field Marshal",        //20 	lvl 105
	"Knight of Astonia",    //21
	"Baron of Astonia",     //22
	"Earl of Astonia",      //23
	"Warlord of Astonia"    //24	lvl 125
};

struct rank_ppd
{
	int army_rank;
};

char *get_title(int co)
{
	if (!(ch[co].flags&CF_WON)) return "";
	
	if (ch[co].flags&CF_FEMALE) return "Lady ";
	else return "Sir ";
}

int look_char(int cn,int co)
{
	struct rank_ppd *ppd;
	struct shrine_ppd *shrine;
	char buf[1024];
	int len,cnt;

        if (ch[cn].flags&CF_PLAYER) {
		shrine=set_data(co,DRD_RANDOMSHRINE_PPD,sizeof(struct shrine_ppd));
		if (shrine && shrine->used[DEATH_SHRINE_INDEX]&DEATH_SHRINE_BIT) {
			log_char(cn,LOG_SYSTEM,0,"#1%s%s the Brave (%d):",get_title(co),ch[co].name,ch[co].level);
                } else {
			log_char(cn,LOG_SYSTEM,0,"#1%s%s (%d):",get_title(co),ch[co].name,ch[co].level);
		}

		len=0;
		len+=sprintf(buf+len,"#2%s ",ch[co].description);
		if (ch[co].flags&CF_PLAYER) {
			if (ch[co].flags&CF_HARDCORE) {
				len+=sprintf(buf+len,"%s is a hardcore character and died %d times. ",Hename(co),ch[co].deaths);
				
			} else {
				len+=sprintf(buf+len,"%s has %d saves, was saved %d times and died %d times. ",Hename(co),ch[co].saves,ch[co].got_saved,ch[co].deaths);				
			}
			if ((cnt=count_solved_labs(co))) len+=sprintf(buf+len,"%s solved %d parts of the labyrinth. ",ch[co].name,cnt);

			if (check_first_kill(co,403)) len+=sprintf(buf+len,"%s has mastered Hell. ",Hename(co));
			else if (check_first_kill(co,395) || check_first_kill(co,396) ||
				 check_first_kill(co,397) || check_first_kill(co,398) ||
				 check_first_kill(co,399) || check_first_kill(co,400) ||
				 check_first_kill(co,401) || check_first_kill(co,402)) len+=sprintf(buf+len,"%s has ventured into Hell. ",Hename(co));
			else if (check_first_kill(co,388) || check_first_kill(co,389) ||
				 check_first_kill(co,390) || check_first_kill(co,391) ||
				 check_first_kill(co,392) || check_first_kill(co,393) ||
				 check_first_kill(co,394)) len+=sprintf(buf+len,"%s got %s nose burned in Hell. ",Hename(co),hisname(co));
		}

		plr_send_inv(cn,co);

		len+=show_prof_info(cn,co,buf+len);

                if (ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE)) {
			if (!(ppd=set_data(co,DRD_RANK_PPD,sizeof(struct rank_ppd)))) return 0;	// OOPS

                        if (ppd->army_rank) {
                                len+=sprintf(buf+len,"%s is a %s in the Imperial Army. ",ch[co].name,rankname[ppd->army_rank]);
			}
                        len+=show_pk_info(cn,co,buf+len);
			len+=show_clan_info(cn,co,buf+len);
			len+=show_club_info(cn,co,buf+len);
		}

                if (ch[co].flags&CF_PLAYER) {
			len+=sprintf(buf+len,"Mirror=%d. ",ch[co].mirror);
			len+=sprintf(buf+len,"Karma: %d",ch[co].karma);
		}
		log_char(cn,LOG_SYSTEM,0,"%s",buf);
	}
	if (ch[cn].flags&CF_GOD) {
		int n,in,fn;

		for (n=12; n<30; n++) {
			if ((in=ch[co].item[n])) look_item(cn,it+in);			
		}

		for (n=0; n<4; n++) {
			if (!(fn=ch[co].ef[n])) continue;
			log_char(cn,LOG_SYSTEM,0,"slot %d, effect type %d",n,ef[fn].type);
		}
	}

	return 1;
}

int set_army_rank(int cn,int rank)
{
	struct rank_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_RANK_PPD,sizeof(struct rank_ppd)))) return 0;	// OOPS

	ppd->army_rank=min(24,rank);

        return 1;
}

int get_army_rank_int(int cn)
{
	struct rank_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_RANK_PPD,sizeof(struct rank_ppd)))) return 0;	// OOPS

	if (ppd->army_rank>24) ppd->army_rank=24;

	return ppd->army_rank;
}

char *get_army_rank_string(int cn)
{
	struct rank_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_RANK_PPD,sizeof(struct rank_ppd)))) return 0;	// OOPS

	return rankname[min(24,ppd->army_rank)];
}

int bigdir(int dir)
{
	switch(dir) {
		case DX_RIGHTUP:	return DX_RIGHT;
		case DX_RIGHTDOWN:	return DX_RIGHT;
		case DX_LEFTUP:		return DX_LEFT;
		case DX_LEFTDOWN:	return DX_LEFT;

		default:		return dir;
	}
}

int is_facing(int cn,int co)
{
        int ok=0;

        switch(ch[cn].dir) {
                case    DX_RIGHT:       if (ch[cn].x+1==ch[co].x && ch[cn].y==ch[co].y) ok=1; break;
                case    DX_LEFT:        if (ch[cn].x-1==ch[co].x && ch[cn].y==ch[co].y) ok=1; break;
                case    DX_UP:          if (ch[cn].x==ch[co].x && ch[cn].y-1==ch[co].y) ok=1; break;
                case    DX_DOWN:        if (ch[cn].x==ch[co].x && ch[cn].y+1==ch[co].y) ok=1; break;
                default: break;
        }

        return ok;
}

int is_back(int cn,int co)
{
        int ok=0;

        switch(ch[cn].dir) {
                case    DX_LEFT:        if (ch[cn].x+1==ch[co].x && ch[cn].y==ch[co].y) ok=1; break;
                case    DX_RIGHT:       if (ch[cn].x-1==ch[co].x && ch[cn].y==ch[co].y) ok=1; break;
                case    DX_DOWN:        if (ch[cn].x==ch[co].x && ch[cn].y-1==ch[co].y) ok=1; break;
                case    DX_UP:          if (ch[cn].x==ch[co].x && ch[cn].y+1==ch[co].y) ok=1; break;
                default: break;
        }

        return ok;
}

// reduce skill_strength according to subjects immunity
// skill_strength is in the usual bounds (about 7...200)
// subject's immunity will be about the same
int immunity_reduction(int caster,int subject,int skill_strength)
{
	int immun,stren,diff;
	//,skill_bonus,str_bonus;

	
	immun=ch[subject].value[0][V_IMMUNITY];
	if (ch[subject].value[1][V_TACTICS]) immun+=tactics2immune(ch[subject].value[0][V_TACTICS]+14);

	//skill_bonus=ch[caster].value[0][V_ARCANE]/15;
	//str_bonus=ch[caster].value[0][V_ARCANE]/2;

	diff=skill_strength-immun;
        stren=max(0.0,1.0+diff/20.0+skill_strength/100.0)*POWERSCALE;

	//say(caster,"skill=%d, immun=%d, diff=%d, stren=%.2f",skill_strength+skill_bonus,immun,diff,stren*STRIKE_DAMAGE/1000.0);
	
        return stren;
}

int immunity_reduction_no_bonus(int caster,int subject,int skill_strength)
{
	int immun,stren,diff;

	immun=ch[subject].value[0][V_IMMUNITY];
	if (ch[subject].value[1][V_TACTICS]) immun+=tactics2immune(ch[subject].value[0][V_TACTICS]+14);

	diff=skill_strength-immun;

	stren=max(0.1,1.0+diff/25.0)*POWERSCALE;

        //say(caster,"skill=%d, immun=%d, diff=%d, stren=%f",skill_strength,immun,diff,stren/1000.0);
	
        return stren;
}

// returns the resulting speed reduction for cn casting freeze on co
// str>=0 means freeze would fail.
int freeze_value(int cn,int co)
{
	int str;
	//int str1,str2;
	//extern float new_system,old_system;
	
	//str2=-(200+spellpower(cn,V_FREEZE)*11-(ch[co].value[0][V_IMMUNITY]*11));
	//if (ch[co].value[1][V_TACTICS]) str2+=tactics2immune(ch[co].value[0][V_TACTICS]+14)*11;

	//str1=-(200+spellpower(cn,V_FREEZE)*11-ch[co].value[0][V_IMMUNITY]*10);
	//if (ch[co].value[1][V_TACTICS]) str1+=ch[co].value[0][V_TACTICS];

	//str=str1*old_system+str2*new_system;
	//say(cn,"freeze values str=%d old=%d new=%d weight=%.2f",str,str1,str2,new_system);

	str=-(200+spellpower(cn,V_FREEZE)*11-(ch[co].value[0][V_IMMUNITY]*11));
	if (ch[co].value[1][V_TACTICS]) str+=tactics2immune(ch[co].value[0][V_TACTICS]+14)*11;

        if ((ch[cn].flags&CF_IDEMON) && ch[co].value[0][V_COLD]>ch[cn].value[1][V_DEMON]) {
		str+=(ch[co].value[0][V_COLD]-ch[cn].value[1][V_DEMON])*10;		
	}

	return str;
}

// returns the resulting speed reduction for cn casting warcry on co
// str>=0 means freeze would fail.
int warcry_value(int cn,int co,int pwr)
{
	int str;
	//int str1,str2;
	//extern float new_system,old_system;

        //str1=-(100+pwr*6-ch[co].value[0][V_IMMUNITY]*5);
	//if (ch[co].value[1][V_TACTICS]) str1+=ch[co].value[0][V_TACTICS]/2;

	//str2=-(100+pwr*6-ch[co].value[0][V_IMMUNITY]*6);
	//if (ch[co].value[1][V_TACTICS]) str2+=tactics2immune(ch[co].value[0][V_TACTICS]+14)*6;

	//str=str1*old_system+str2*new_system;
	//say(cn,"warcry values str=%d old=%d new=%d weight=%.2f",str,str1,str2,new_system);

	str=-(100+pwr*6-ch[co].value[0][V_IMMUNITY]*6);
	if (ch[co].value[1][V_TACTICS]) str+=tactics2immune(ch[co].value[0][V_TACTICS]+14)*6;

	return str;
}

int warcry_damage(int cn,int co,int pwr)
{
	return immunity_reduction(cn,co,pwr);
}

// create an item representing amount silver coins
int create_money_item(int amount)
{
	int in;

	in=create_item("money");
	if (!in) return 0;
	
        if (amount>9999999) {
		it[in].sprite=109;
	} else if (amount>999999) {
		it[in].sprite=108;
	} else if (amount>99999) {
		it[in].sprite=107;
	} else if (amount>9999) {
		it[in].sprite=106;
	} else if (amount>999) {
		it[in].sprite=105;
	} else if (amount>99) {
		it[in].sprite=104;
	} else if (amount>9) {
		it[in].sprite=103;
	} else if (amount>2) {
		it[in].sprite=102;
	} else if (amount==2) {
		it[in].sprite=101;
	} else if (amount==1) {
		it[in].sprite=100;
	}
	sprintf(it[in].description,"%.2fG.",amount/100.0);

	it[in].value=amount;

	return in;
}

// destroy money item and return the amount of silver coins it represented
int destroy_money_item(int in)
{
	int value;

	value=it[in].value;

	free_item(in);

	return value;
}

char *strcasestr(const char *haystack,const char *needle)
{
	const char *ptr;

	for (ptr=needle; *haystack; haystack++) {
		if (toupper(*ptr)==toupper(*haystack)) {
			ptr++;
			if (!*ptr) return (char*)(haystack+(needle-ptr+1));
		} else ptr=needle;
	}
	return NULL;
}

struct special_item
{
	char *name;
	int mod_index[3];
	int chance;
	unsigned long long needflag;
	int the;
	int pricemulti;
};

#define SP_LOTS		5000
#define SP_MANY		1000
#define SP_SOME		250
#define SP_FEW		50
#define SP_RARE		10
#define SP_ULTRA	1

struct special_item special_item[]={
        {"Wisdom",			{V_WIS,-1,-1},				SP_MANY,	0,		0,	1},
	{"Intuition",			{V_INT,-1,-1},				SP_MANY,	0,		0,	1},
	{"Agility",			{V_AGI,-1,-1},				SP_MANY,	0,		0,	1},
	{"Strength",			{V_STR,-1,-1},				SP_MANY,	0,		0,	1},
	
	{"Hitpoints",			{V_HP,-1,-1},				SP_MANY,	0,		0,	1},
	{"Mana",			{V_MANA,-1,-1},				SP_MANY,	0,		0,	1},
	{"Endurance",			{V_ENDURANCE,-1,-1},			SP_MANY,	0,		0,	1},

	//{"Axe",      			{V_AXE,-1,-1},				SP_SOME,	IF_AXE,		0,	2},
	{"Dagger",			{V_DAGGER,-1,-1},			SP_SOME,	IF_DAGGER,	0,	2},
	{"Hand to Hand",		{V_HAND,-1,-1},				SP_SOME,	0,		0,	2},
	{"Staff",			{V_STAFF,-1,-1},			SP_SOME,	IF_STAFF,	0,	2},
	{"Sword",			{V_SWORD,-1,-1},			SP_SOME,	IF_SWORD,	0,	2},
	{"Two-Handed",			{V_TWOHAND,-1,-1},			SP_SOME,	IF_TWOHAND,	0,	2},

	{"Attack",			{V_ATTACK,-1,-1},			SP_SOME,	0,		0,	2},
	{"Parry",			{V_PARRY,-1,-1},			SP_SOME,	0,		0,	2},
	{"Warcry",			{V_WARCRY,-1,-1},			SP_MANY,	0,		0,	1},
	{"Tactics",			{V_TACTICS,-1,-1},			SP_SOME,	0,		0,	2},
	{"Surround Hit",		{V_SURROUND,-1,-1},			SP_MANY,	0,		0,	1},
	{"Body Control",		{V_BODYCONTROL,-1,-1},			SP_MANY,	0,		0,	1},
	{"Speed",			{V_SPEEDSKILL,-1,-1},			SP_MANY,	0,		0,	1},

	{"Bartering",			{V_BARTER,-1,-1},			SP_MANY,	0,		0,	1},
	{"Perception",			{V_PERCEPT,-1,-1},			SP_MANY,	0,		0,	1},
	{"Stealth",			{V_STEALTH,-1,-1},			SP_MANY,	0,		0,	1},

	{"Bless",			{V_BLESS,-1,-1},			SP_MANY,	0,		0,	1},
	{"Heal",			{V_HEAL,-1,-1},				SP_MANY,	0,		0,	1},
	{"Freeze",			{V_FREEZE,-1,-1},			SP_SOME,	0,		0,	2},
	{"Magic Shield",		{V_MAGICSHIELD,-1,-1},			SP_SOME,	0,		0,	2},
	{"Lightning",			{V_FLASH,-1,-1},			SP_SOME,	0,		0,	2},
	{"Fireball",			{V_FIREBALL,-1,-1},			SP_MANY,	0,		0,	1},
	//{"Ball Lightning",		{V_BALL,-1,-1},				SP_MANY,	0,		0,	1},
	{"Pulse",			{V_PULSE,-1,-1},			SP_SOME,	0,		0,	2},

	{"Regenerate",			{V_REGENERATE,-1,-1},			SP_MANY,	0,		0,	1},
	{"Meditate",			{V_MEDITATE,-1,-1},			SP_MANY,	0,		0,	1},
	{"Immunity",			{V_IMMUNITY,-1,-1},			SP_SOME,	0,		0,	2},
	{"Duration",			{V_DURATION,-1,-1},			SP_SOME,	0,		0,	2},
	{"Rage",			{V_RAGE,-1,-1},				SP_SOME,	0,		0,	2},

	{"Thief",			{V_STEALTH,V_ENDURANCE,-1},		SP_FEW,		0,		1,	4},
	{"Vision",			{V_PERCEPT,V_INT,-1},			SP_FEW,		0,		0,	4},
	{"Wounded",			{V_HEAL,V_REGENERATE,-1},		SP_FEW,		0,		1,	4},
	{"Eccentric",			{V_BODYCONTROL,V_BLESS,V_SPEEDSKILL},	SP_FEW,		0,		1,	4},
	{"Sorcery",			{V_MANA,V_DURATION,-1},			SP_FEW,		0,		0,	8},

	{"Fighting",			{V_ATTACK,V_PARRY,-1},			SP_RARE,	0,		0,	8},
	{"Magic",			{V_FLASH,V_MAGICSHIELD,-1},		SP_RARE,	0,		0,	8},
	{"Berserk",			{V_ATTACK,V_RAGE,-1},			SP_RARE,	0,		0,	8},
	
        {"One Handed Offense",		{V_ATTACK,V_SWORD,-1},			SP_RARE,	IF_SWORD,	0,	8},
	{"One Handed Defense",		{V_PARRY,V_SWORD,-1},			SP_RARE,	IF_SWORD,	0,	8},
        {"Two Handed Offense",		{V_ATTACK,V_TWOHAND,-1},		SP_RARE,	IF_TWOHAND,	0,	8},
	{"Two Handed Defense",		{V_PARRY,V_TWOHAND,-1},			SP_RARE,	IF_TWOHAND,	0,	8},

	{"Tactical Offense",		{V_ATTACK,V_TACTICS,-1},		SP_RARE,	0,		0,	8},
	{"Tactical Defense",		{V_PARRY,V_TACTICS,-1},			SP_RARE,	0,		0,	8},

	{"Magical Offense",		{V_FLASH,V_FIREBALL,V_PULSE},		SP_RARE,	0,		0,	8},
	{"Magical Defense",		{V_MAGICSHIELD,V_FREEZE,V_HEAL},	SP_RARE,	0,		0,	8},

	{"Warrior",			{V_ATTACK,V_PARRY,V_IMMUNITY},		SP_ULTRA,	0,		1,	16},
	{"Mage",			{V_FLASH,V_MAGICSHIELD,V_IMMUNITY},	SP_ULTRA,	0,		1,	16},
	{"Tactician",			{V_ATTACK,V_PARRY,V_TACTICS},		SP_ULTRA,	0,		1,	16},
	{"Seyan'Du",			{V_ATTACK,V_PARRY,V_BLESS},		SP_ULTRA,	0,		1,	16},
	{"Arch-Warrior",		{V_ATTACK,V_PARRY,V_RAGE},		SP_ULTRA,	0,		1,	16},
	{"Arch-Mage",			{V_FLASH,V_MAGICSHIELD,V_DURATION},	SP_ULTRA,	0,		1,	16}
};

void set_item_requirements_sub(int in,int maxlvl)
{
	int lvl,sum,high,n;

	if (it[in].flags&IF_BEYONDBOUNDS) return;

	for (n=sum=high=0; n<MAXMOD; n++) {
		switch(it[in].mod_index[n]) {
			case V_WEAPON:	break;
			case V_ARMOR:	break;
			case V_SPEED:	break;
			case V_DEMON:	break;
			case V_LIGHT:	break;
			default:	if (it[in].mod_index[n]>=0) {
						high=max(high,it[in].mod_value[n]);
						sum+=it[in].mod_value[n];
			}
					break;
		}
	}
	switch(high) {
		case 0:		lvl= 0; break;
		case 1:		lvl= 2; break;
		case 2:		lvl= 3; break;
		case 3:		lvl= 5; break;
		case 4:		lvl=10; break;
		case 5:		lvl=15; break;
		case 6:		lvl=17; break;
		case 7:		lvl=20; break;
		case 8:		lvl=23; break;
		case 9:		lvl=26; break;
		case 10:	lvl=30; it[in].needs_class|=8; break;
		case 11:	lvl=33; it[in].needs_class|=8; break;
		case 12:	lvl=36; it[in].needs_class|=8; break;
		case 13:	lvl=40; it[in].needs_class|=8; break;
		case 14:	lvl=43; it[in].needs_class|=8; break;
		case 15:	lvl=46; it[in].needs_class|=8; break;
		case 16:	lvl=50; it[in].needs_class|=8; break;
		case 17:	lvl=53; it[in].needs_class|=8; break;
		case 18:	lvl=56; it[in].needs_class|=8; break;
		case 19:	lvl=60; it[in].needs_class|=8; break;
		case 20:	lvl=63; it[in].needs_class|=8; break;
		case 21:	lvl=66; it[in].needs_class|=8; break;
		case 22:	lvl=70; it[in].needs_class|=8; break;
		case 23:	lvl=73; it[in].needs_class|=8; break;
		default:	lvl=76; it[in].needs_class|=8; break;
	}
	
	//lvl+=sum-high;
	it[in].min_level=max(it[in].min_level,min(maxlvl,lvl));
}

int level2maxitem(int level)
{
	
	if (level< 2) return 0;
	if (level< 3) return 1;
	if (level< 5) return 2;
	if (level<10) return 3;
	if (level<15) return 4;
	if (level<17) return 5;
	if (level<20) return 6;
	if (level<23) return 7;
	if (level<26) return 8;
	if (level<30) return 9;
	if (level<33) return 10;
	if (level<36) return 11;
	if (level<40) return 12;
	if (level<43) return 13;
	if (level<46) return 14;
	if (level<50) return 15;
	if (level<53) return 16;
	if (level<56) return 17;
	if (level<60) return 18;
	if (level<63) return 19;
	
	return 20;
}

void set_item_requirements(int in)
{
	set_item_requirements_sub(in,120);
}

void set_legacy_item_lvl(int in,int cn)
{
        if (it[in].flags&(IF_WNHEAD|IF_WNNECK|IF_WNBODY|IF_WNARMS|IF_WNBELT|IF_WNLEGS|IF_WNFEET|IF_WNLHAND|IF_WNRHAND|IF_WNCLOAK|IF_WNLRING|IF_WNRRING|IF_WNTWOHANDED))
		set_item_requirements_sub(in,ch[cn].level);	
}

// strength: +X to stats, actually gives +X-0..7
// base: 1,10,20,...90
// potionprob: 1 (0%), 2(50%), 10(90%), ...
// maxchance: 10000 (normal), 1000 (no junk), 250 (better), 50 (good), 10 (real good), 1 (absolutely cool)
int create_special_item(int strength,int base,int potionprob,int maxchance)
{
	int in,n,tot,cnt,m,nr;
	char buf[256];

	int type,len,priceadd=0;
	char *str_desc;

	if (RANDOM(potionprob)) {
		strength+=RANDOM(4);
		if (strength<6) n=1;
		else if (strength<9) n=2;
		else n=3;
		
		switch(RANDOM(3)) {
			case 0:		sprintf(buf,"healing_potion%d",n); break;
			case 1:		sprintf(buf,"mana_potion%d",n); break;
			case 2:		sprintf(buf,"combo_potion%d",n); break;
		}
		in=create_item(buf);
		return in;
	}
	
        switch(base) {
		case 1:		break;
		case 10:	base=2; break;
		case 20:	base=3; break;
		case 30:	base=4; break;
		case 40:	base=5; break;
		case 50:	base=6; break;
		case 60:	base=7; break;
		case 70:	base=8; break;
		case 80:	base=9; break;
		case 90:	base=10; break;
	}

	type=RANDOM(21);

	switch(type) {
		case 0:		sprintf(buf,"armor%dq3",base); break;
		case 1:		sprintf(buf,"helmet%dq3",base); break;
		case 2:		sprintf(buf,"sleeves%dq3",base); break;
		case 3:		sprintf(buf,"leggings%dq3",base); break;
                case 4:		sprintf(buf,"sword%dq3",base); break;
		case 5:		sprintf(buf,"twohanded%dq3",base); break;
		case 6:		sprintf(buf,"dagger%dq3",base); break;
		case 7:		sprintf(buf,"staff%dq3",base); break;
		case 8:		sprintf(buf,"plain_gold_ring"); break;
		case 9:		sprintf(buf,"plain_gold_ring"); break;
		case 10:	sprintf(buf,"green_hat"); break;
		case 11:	sprintf(buf,"brown_hat"); break;
		case 12:	sprintf(buf,"blue_cape"); break;
		case 13:	sprintf(buf,"brown_cape"); break;
		case 14:	sprintf(buf,"belt"); break;
		case 15:	sprintf(buf,"amulet"); break;
		case 16:	sprintf(buf,"boots"); break;
		case 17:	sprintf(buf,"vest"); break;
		case 18:	sprintf(buf,"trousers"); break;
		case 19:	sprintf(buf,"bracelet"); break;
		case 20:	sprintf(buf,"gloves"); break;

                default:	return 0;
	}

	in=create_item(buf);
	if (!in) return 0;

	// strength
        if (strength==1) ;
        else if (strength==2) strength=lowhi_random(2);
	else if (strength==3) strength=lowhi_random(3);
	else if (strength==4) strength=lowhi_random(4);
	else if (strength==5) strength=lowhi_random(5);
	else if (strength==6) strength=lowhi_random(6);
	else if (strength==7) strength=lowhi_random(7);
        else strength=strength-7+lowhi_random(7);

        switch(strength) {
		case 1:		str_desc="Extremely Weak "; priceadd+=200; break;
		case 2:		str_desc="Very Weak "; priceadd+=300; break;
		case 3:		str_desc="Weak "; priceadd+=400; break;
		case 4:		str_desc="Fairly Weak "; priceadd+=500; break;
		case 5:		str_desc="Somewhat Weak "; priceadd+=600; break;
		case 6:		str_desc=""; priceadd+=700; break;
		case 7:		str_desc="Somewhat Strong "; priceadd+=800; break;
		case 8:		str_desc="Fairly Strong "; priceadd+=1000; break;
		case 9:		str_desc="Strong "; priceadd+=1200; break;
		case 10:	str_desc="Very Strong "; priceadd+=1400; break;
		case 11:	str_desc="Extremely Strong "; priceadd+=1600; break;
		case 12:	str_desc="Somewhat Powerful "; priceadd+=2000; break;
		case 13:	str_desc="Fairly Powerful "; priceadd+=2400; break;
		case 14:	str_desc="Powerful "; priceadd+=2800; break;
		case 15:	str_desc="Very Powerful "; priceadd+=3200; break;
		case 16:	str_desc="Extremely Powerful "; priceadd+=4000; break;
		case 17:	str_desc="Superhuman "; priceadd+=4800; break;
		case 18:	str_desc="Demi-Godly "; priceadd+=5600; break;
		case 19:	str_desc="Godly "; priceadd+=10000; break;
                case 20:	str_desc="Ultimate "; priceadd+=20000; break;
		default:	return 0;
	}

        // find all fitting items
	for (nr=tot=0; nr<ARRAYSIZE(special_item); nr++) {
		if (special_item[nr].needflag && (it[in].flags&IF_WEAPON) && !(it[in].flags&special_item[nr].needflag)) continue;
		if (maxchance<special_item[nr].chance) continue;
                tot+=special_item[nr].chance;
	}
	if (!tot) return 0;
	
	// roll dice
	cnt=RANDOM(tot);

	// find correct item
	for (nr=tot=0; nr<ARRAYSIZE(special_item); nr++) {
		if (special_item[nr].needflag && (it[in].flags&IF_WEAPON) && !(it[in].flags&special_item[nr].needflag)) continue;
		if (maxchance<special_item[nr].chance) continue;
                tot+=special_item[nr].chance;
		if (tot>cnt) break;		
	}
	if (nr==ARRAYSIZE(special_item)) { elog("create_special_item(): dice wrong"); return 0; }

        len=snprintf(buf,sizeof(it[0].description)-1,"%s of %s%s%s.",
		     it[in].name,
		     special_item[nr].the ? "the " : "",
		     str_desc,
		     special_item[nr].name);
	
	
	if (len<3 || len==sizeof(it[0].description)-1) return 0;

        for (n=m=0; n<MAXMOD && m<3; n++) {
		if (special_item[nr].mod_index[m]==-1) break;
		
		if (!it[in].mod_index[n]) {	// add modifier
			it[in].mod_index[n]=special_item[nr].mod_index[m];
			it[in].mod_value[n]=strength;
			m++;
		}
	}

	// change description
	strcpy(it[in].description,buf);

	// change price
	it[in].value+=priceadd*special_item[nr].pricemulti;

	// add min_level and make arch-only if str>10
	set_item_requirements(in);

	// note change
	it[in].ID=IID_GENERIC_SPECIAL;

	return in;
}

// gives non-gauss random values from 1 to val (inclusive)
// low numbers are a lot more probable. max value is 215
int lowhi_random(int val)
{
        int p;

	p=sqrt(sqrt(RANDOM((val+1)*(val+1)*(val+1)*(val+1)-1)+1));

	return val-p+1;
}

int create_spell_timer(int cn,int in,int pos)
{
	switch(it[in].driver) {
		case IDR_BLESS:		create_show_effect(EF_BLESS,cn,*(signed long*)(it[in].drdata+4),*(signed long*)(it[in].drdata),0,it[in].mod_value[0]);
					set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
		case IDR_WARCRY:	create_show_effect(EF_WARCRY,cn,*(signed long*)(it[in].drdata+4),*(signed long*)(it[in].drdata),0,0);
					set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
		case IDR_FREEZE:	create_show_effect(EF_FREEZE,cn,*(signed long*)(it[in].drdata+4),*(signed long*)(it[in].drdata),0,0);
					set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
		case IDR_FLASH:		set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
		case IDR_ARMOR:
		case IDR_WEAPON:
		case IDR_HP:
		case IDR_MANA:		set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
		case IDR_POTION_SP:	create_show_effect(EF_POTION,cn,*(signed long*)(it[in].drdata+4),*(signed long*)(it[in].drdata),0,it[in].mod_value[0]);
					set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;

		case IDR_CURSE:		create_show_effect(EF_CURSE,cn,*(signed long*)(it[in].drdata+4),*(signed long*)(it[in].drdata),0,-it[in].mod_value[0]);
					set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;

		case IDR_POISON0:
		case IDR_POISON1:
		case IDR_POISON2:
		case IDR_POISON3:	set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					set_timer(ticker+TICKS,poison_callback,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
		case IDR_NONOMAGIC:
		case IDR_FIRERING:	
		case IDR_INFRARED:	
		case IDR_OXYGEN:
		case IDR_UWTALK:	set_timer(*(unsigned long*)(it[in].drdata),remove_spell,cn,in,pos,ch[cn].serial,it[in].serial);
					break;
	}
	return 1;
}

int store_citem(int cn)
{
	int n;

	for (n=30; n<INVENTORYSIZE; n++) {
		if (ch[cn].item[n]) continue;
		return swap(cn,n);		
	}
	return 0;
}

void look_values_bg(int cnID,int coID)
{
	int co,n;
	char buf[512];
	struct bank_ppd *ppd;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].ID==coID) break;		
	}
	if (!co) return;

	sprintf(buf,"%s, level %d, class %s%s%s",
		ch[co].name,
		ch[co].level,
		(ch[co].flags&CF_ARCH) ? "A" : "",
		(ch[co].flags&CF_WARRIOR) ? "W" : "",
		(ch[co].flags&CF_MAGE) ? "M" : "");
	tell_chat(0,cnID,1,"%s",buf);
	
	sprintf(buf,"Desc: %s",ch[co].description);
	tell_chat(0,cnID,1,"%s",buf);

        if (ch[co].paid_till&1) sprintf(buf,"Paying player: %s (%02d:%02d:%02d hours left)",(ch[co].flags&CF_PAID) ? "yes" : "no",
		(ch[co].paid_till-time_now)/(60*60),
		((ch[co].paid_till-time_now)/60)%60,
		(ch[co].paid_till-time_now)%60);
	else sprintf(buf,"Paying player: %s (%d days left)",(ch[co].flags&CF_PAID) ? "yes" : "no",(ch[co].paid_till-time_now)/(60*60*24));
	tell_chat(0,cnID,1,"%s",buf);

	sprintf(buf,"PK: %s, Hardcore: %s",(ch[co].flags&CF_PK) ? "yes" : "no",(ch[co].flags&CF_HARDCORE) ? "yes" : "no");
	tell_chat(0,cnID,1,"%s",buf);

	sprintf(buf,"Playing for %d hours.",stats_online_time(co)/60);
	tell_chat(0,cnID,1,"%s",buf);

	ppd=set_data(co,DRD_BANK_PPD,sizeof(struct bank_ppd));
	if (ppd) {
		sprintf(buf,"Gold in hand: %.2fG, gold in bank: %.2fG",ch[co].gold/100.0,ppd->imperial_gold/100.0);
		tell_chat(0,cnID,1,"%s",buf);
	}
	sprintf(buf,"Mirror: %d, actual mirror: %d. Area %d, %s",ch[co].mirror,areaM,areaID,get_section_name(ch[co].x,ch[co].y));
		tell_chat(0,cnID,1,"%s",buf);
	
	for (n=0; n<V_MAX; n+=3) {
		sprintf(buf,"%s: %d/%d \010%s: %d/%d \020%s: %d/%d",
			skill[n].name,ch[co].value[1][n],ch[co].value[0][n],
			n+1<V_MAX ? skill[n+1].name : "none",
			n+1<V_MAX ? ch[co].value[1][n+1] : 0,
			n+1<V_MAX ? ch[co].value[0][n+1] : 0,
			n+2<V_MAX ? skill[n+2].name : "none",
			n+2<V_MAX ? ch[co].value[1][n+2] : 0,
			n+2<V_MAX ? ch[co].value[0][n+2] : 0);
                tell_chat(0,cnID,1,"%s",buf);
	}

	tell_chat(0,cnID,1,"Offensive Value: %d, Defensive Value: %d, WV: %d, AV: %d",
		  get_attack_skill(co),
		  get_parry_skill(co),
		  ch[co].value[0][V_WEAPON],
		  ch[co].value[0][V_ARMOR]/20);

}

void lollipop_bg(int cnID,int coID)
{
	int co,in;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].ID==coID) break;		
	}
	if (!co) return;

	in=create_item("lollipop");
	if (give_char_item(co,in)) {
		log_char(co,LOG_SYSTEM,0,"A lollipop dropped out of thin air into your hand.");
		tell_chat(0,cnID,1,"Gave lollipop to %s.",ch[co].name);
	} else tell_chat(0,cnID,1,"Could not give lollipop to %s.",ch[co].name);
}

static int shutdown_last=0;

void shutdown_warn(void)
{
	char buf[256];
	int min,n;

	if (!shutdown_at) return;

	min=(shutdown_at-realtime+50)/60;
	if (min!=shutdown_last) {
		if (min>0) sprintf(buf,"°c3The server will go down in %d minute%s. Expected downtime: %d minutes.",min,min>1?"s":"",shutdown_down);
		else sprintf(buf,"°c3The server will go down NOW. Expected downtime: %d minutes.",shutdown_down);
		for (n=1; n<MAXCHARS; n++) {
			if (!(ch[n].flags&CF_PLAYER)) continue;
                        log_char(n,LOG_SYSTEM,0,buf);
		}
		shutdown_last=min;
		if (min<3) nologin=1;		
	}
	if (realtime>=shutdown_at) {
		quit=1;
	}
}

void shutdown_bg(int t,int down)
{
	if (t) {
		shutdown_at=t;
		shutdown_down=down;
		shutdown_last=999;
		shutdown_warn();
	} else if (shutdown_at || nologin) {
		int n;
		shutdown_at=nologin=0;
		
		for (n=1; n<MAXCHARS; n++) {
			if (!(ch[n].flags&CF_PLAYER)) continue;
                        log_char(n,LOG_SYSTEM,0,"°c3Shutdown has been cancelled.");
		}
	}
}

int edemon_reduction(int cn,int str)
{
	return max(0,str-ch[cn].value[0][V_DEMON]);
}

// can cn see co based on CF_INVIS etc.?
int is_visible(int cn,int co)
{
	if (ch[co].flags&CF_INVISIBLE) return 0;
	
	return 1;
}

// market driven pricing: calculate current cost
// hint = price to start at
// wanted = number missions per day we want in ten days
int calc_cost(int hint,struct cost_data *dat,int wanted)
{
	int avg_amount=0,num=0,n,avg_date=0;
	float days;
	
	for (n=0; n<20; n++) {
		if (dat->amount[n]) {
			avg_amount+=dat->amount[n];
			avg_date+=dat->date[n];
			num++;
		}
	}

        avg_amount=(avg_amount+hint*(20-num))/20;
	avg_date=(avg_date+(dat->created-24*60*60*wanted)*(20-num))/20;	// theoretical creation time is 10 days ago

	days=(realtime-avg_date)/(60*60*24.0*10/wanted);		// we want to have wanted missions in 10 days
        if (days<0.01) days=0.01;
	if (days>100) days=100;	

	//xlog("hint=%d, amount=%d, days=%.2f, num=%d, wanted=%d, diff=%.2fd",hint,avg_amount,days,num,wanted,(realtime-avg_date)/(60*60*24.0));

	return avg_amount/days;
}

// market driven pricing: add sale to data structure
// cost = sales price
void add_cost(int cost,struct cost_data *dat)
{
	memmove(dat->amount+1,dat->amount,sizeof(dat->amount)-sizeof(dat->amount[0]));
	memmove(dat->date+1,dat->date,sizeof(dat->date)-sizeof(dat->date[0]));
	dat->amount[0]=cost;
	dat->date[0]=realtime;
	dat->earned+=cost;
	dat->sold++;
}

void destroy_char(int cn)
{
        del_all_data(cn);
	purge_messages(cn);
	destroy_chareffects(cn);
	destroy_items(cn);
	free_char(cn);
}

void remove_destroy_char(int cn)
{
	remove_char(cn);
	destroy_char(cn);
}


#define END_COST	(POWERSCALE/4)
int end_cost(int cn)
{
	if (ch[cn].prof[P_ATHLETE]) return END_COST-(ch[cn].prof[P_ATHLETE]*END_COST/45);
	else return END_COST;
}

void give_military_pts(int cn,int co,int pts,int exps)
{
	struct military_ppd *ppd;
	int rank;
	char buf[256];
	
	if (!(ppd=set_data(co,DRD_MILITARY_PPD,sizeof(struct military_ppd)))) return;
			
	give_exp(co,exps); ppd->normal_exp+=exps;
	
	ppd->military_pts+=pts;	
	rank=cbrt(ppd->military_pts);
	while (rank<25 && rank>get_army_rank_int(co)) {
		set_army_rank(co,rank);
		say(cn,"You've been promoted to %s. Congratulations, %s!",get_army_rank_string(co),ch[co].name);
		if (get_army_rank_int(co)>9) {
			sprintf(buf,"0000000000°c10Grats: %s is a %s now!",ch[co].name,get_army_rank_string(co));
			server_chat(6,buf);
		}

	}
	dlog(co,0,"got %d military pts, %d exp",pts,exps);
}

void give_military_pts_no_npc(int co,int pts,int exps)
{
	struct military_ppd *ppd;
	int rank;
	char buf[256];
	
	if (!(ppd=set_data(co,DRD_MILITARY_PPD,sizeof(struct military_ppd)))) return;
			
	give_exp(co,exps); ppd->normal_exp+=exps;
	
	ppd->military_pts+=pts;	
	rank=cbrt(ppd->military_pts);
	if (rank<25 && rank>get_army_rank_int(co)) {
		set_army_rank(co,rank);
		log_char(co,LOG_SYSTEM,0,"You've been promoted to %s!",get_army_rank_string(co));
		if (get_army_rank_int(co)>9) {
			sprintf(buf,"0000000000°c10Grats: %s is a %s now!",ch[co].name,get_army_rank_string(co));
			server_chat(6,buf);
		}
	}
	dlog(co,0,"got %d military pts, %d exp",pts,exps);
}

int create_drop_char(char *name,int x,int y)
{
	int cn;

        cn=create_char(name,0);
	if (!cn) return 0;

        if (drop_char(cn,x,y,0)) {

		ch[cn].tmpx=ch[cn].x;
		ch[cn].tmpy=ch[cn].y;
		
                update_char(cn);
		
		ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
		ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
		ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
		
		ch[cn].dir=DX_RIGHTDOWN;

                return cn;
	}

	destroy_char(cn);

	return 0;
}

int check_dlight(int x,int y)
{
	int m;
	if (x<0 || y<0 || x>=MAXMAP || y>=MAXMAP) return 0;

	m=x+y*MAXMAP;
	return (dlight*map[m].dlight)/256;
}

int check_light(int x,int y)
{
	int m;
	if (x<0 || y<0 || x>=MAXMAP || y>=MAXMAP) return 0;

	m=x+y*MAXMAP;
	return map[m].light;
}

char *lower_case(char *src)
{
	static char buf[256],*dst;
	int n;

	for (n=0,dst=buf; n<255 && *src; n++) *dst++=tolower(*src++);
	*dst=0;

	return buf;
}

int give_char_item(int cn,int in)
{
	int n;

	if (!ch[cn].citem) ch[cn].citem=in;
	else {
		for (n=30; n<INVENTORYSIZE; n++) {
			if (!ch[cn].item[n]) { ch[cn].item[n]=in; break; }
		}
		if (n==INVENTORYSIZE) return 0;		
	}
	it[in].carried=cn;
	ch[cn].flags|=CF_ITEMS;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"was given %s from NPC",it[in].name);
	return 1;
}

int pulse_damage(int cn,int co,int val)
{
	int str;

        str=immunity_reduction_no_bonus(cn,co,ch[cn].value[0][V_PULSE]);
	if (str<1) return 0;
	
	str=val*str/10;

	return str;
}

int create_orb(void)
{
	int in;
	int r,what;

	in=create_item("empty_orb");

	r=RANDOM(32);

	switch(r) {
		case 0:		what=V_ENDURANCE; break;
                case 1:		what=V_HP; break;
                case 2:		what=V_MANA; break;
                case 3:		what=V_WIS; break;
                case 4:		what=V_INT; break;
                case 5:		what=V_AGI; break;
                case 6:		what=V_STR; break;
                case 7:		what=V_BARTER; break;
                case 8:		what=V_PERCEPT; break;
                case 9:		what=V_STEALTH; break;
                case 10:	what=V_HAND; break;
                case 11:	what=V_WARCRY; break;
                case 12:	what=V_SURROUND; break;
                case 13:	what=V_BODYCONTROL; break;
                case 14:	what=V_SPEEDSKILL; break;
                case 15:	what=V_HEAL; break;
                case 16:	what=V_FIREBALL; break;
                case 17:	what=V_TACTICS; break;
		case 18:	what=V_DURATION; break;
		case 19:	what=V_RAGE; break;
		case 20:	what=V_BLESS; break;
		case 21:	what=V_FREEZE; break;
		case 22:	what=V_MAGICSHIELD; break;
		case 23:	what=V_FLASH; break;
		case 24:	what=V_PULSE; break;
                case 25:	what=V_DAGGER; break;
                case 26:	what=V_STAFF; break;
		case 27:	what=V_SWORD; break;
		case 28:	what=V_TWOHAND; break;
                case 29:	what=V_ATTACK; break;
		case 30:	what=V_PARRY; break;
		case 31:	what=V_IMMUNITY; break;
		default:	elog("unknown %d in create_orb()",r); what=V_HP; break;
	}
	sprintf(it[in].name,"Orb of %s",skill[what].name);
	it[in].drdata[0]=what;
	it[in].drdata[1]=1;

	return in;
}

int create_orb2(int what)
{
	int in;

	in=create_item("empty_orb");

        sprintf(it[in].name,"Orb of %s",skill[what].name);
	it[in].drdata[0]=what;
	it[in].drdata[1]=1;

	return in;
}


int take_money(int cn,int val)
{
	if (ch[cn].gold<val) return 0;
	ch[cn].gold-=val;
	ch[cn].flags|=CF_ITEMS;
	return 1;
}


int fireball_damage(int cn,int co,int str)
{
        return immunity_reduction(cn,co,str)*FIREBALL_DAMAGE;
}

int strike_damage(int cn,int co,int str)
{
        return immunity_reduction(cn,co,str)*STRIKE_DAMAGE;
}

void tabunga(int cn,int co,char *ptr)
{
	if ((ch[co].flags&CF_GOD) && strcasestr(ptr,"tabunga") && char_dist(cn,co)<3) {
		say(cn,"%s (%d):",ch[cn].name,ch[cn].level);
		say(cn,"HP:        %3d/%3d (%d)",ch[cn].value[1][V_HP],ch[cn].value[0][V_HP],ch[cn].hp/POWERSCALE);
		say(cn,"Endurance: %3d/%3d (%d)",ch[cn].value[1][V_ENDURANCE],ch[cn].value[0][V_ENDURANCE],ch[cn].endurance/POWERSCALE);
		say(cn,"Mana:      %3d/%3d (%d)",ch[cn].value[1][V_MANA],ch[cn].value[0][V_MANA],ch[cn].mana/POWERSCALE);
		say(cn,"Wisdom:    %3d/%3d",ch[cn].value[1][V_WIS],ch[cn].value[0][V_WIS]);
		say(cn,"Intuition: %3d/%3d",ch[cn].value[1][V_INT],ch[cn].value[0][V_INT]);
		say(cn,"Agility:   %3d/%3d",ch[cn].value[1][V_AGI],ch[cn].value[0][V_AGI]);
		say(cn,"Strength:  %3d/%3d",ch[cn].value[1][V_STR],ch[cn].value[0][V_STR]);
		say(cn,"Hand2Hand: %3d/%3d",ch[cn].value[1][V_HAND],ch[cn].value[0][V_HAND]);
		say(cn,"Sword:     %3d/%3d",ch[cn].value[1][V_SWORD],ch[cn].value[0][V_SWORD]);
		say(cn,"Twohanded: %3d/%3d",ch[cn].value[1][V_TWOHAND],ch[cn].value[0][V_TWOHAND]);
		say(cn,"Attack:    %3d/%3d",ch[cn].value[1][V_ATTACK],ch[cn].value[0][V_ATTACK]);
		say(cn,"Parry:     %3d/%3d",ch[cn].value[1][V_PARRY],ch[cn].value[0][V_PARRY]);
		say(cn,"Tactics:   %3d/%3d",ch[cn].value[1][V_TACTICS],ch[cn].value[0][V_TACTICS]);
		say(cn,"Immunity:  %3d/%3d",ch[cn].value[1][V_IMMUNITY],ch[cn].value[0][V_IMMUNITY]);
		say(cn,"Bless:     %3d/%3d",ch[cn].value[1][V_BLESS],ch[cn].value[0][V_BLESS]);
		say(cn,"M-Shield:  %3d/%3d  (%d)",ch[cn].value[1][V_MAGICSHIELD],ch[cn].value[0][V_MAGICSHIELD],ch[cn].lifeshield/POWERSCALE);
		say(cn,"Flash:     %3d/%3d",ch[cn].value[1][V_FLASH],ch[cn].value[0][V_FLASH]);
		say(cn,"Freeze:    %3d/%3d",ch[cn].value[1][V_FREEZE],ch[cn].value[0][V_FREEZE]);
		say(cn,"Speed:     %3d/%3d",ch[cn].value[1][V_SPEED],ch[cn].value[0][V_SPEED]);
                say(cn,"F-Ball:    %3d/%3d",ch[cn].value[1][V_FIREBALL],ch[cn].value[0][V_FIREBALL]);
		say(cn,"Percept:   %3d/%3d",ch[cn].value[1][V_PERCEPT],ch[cn].value[0][V_PERCEPT]);
		say(cn,"Stealth:   %3d/%3d",ch[cn].value[1][V_STEALTH],ch[cn].value[0][V_STEALTH]);
		say(cn,"Warcry:    %3d/%3d",ch[cn].value[1][V_WARCRY],ch[cn].value[0][V_WARCRY]);
		say(cn,"P_DEMON:   %3d",ch[cn].prof[P_DEMON]);
		say(cn,"P_CLAN:    %3d",ch[cn].prof[P_CLAN]);
		say(cn,"P_LIGHT:   %3d",ch[cn].prof[P_LIGHT]);
		say(cn,"P_DARK:    %3d",ch[cn].prof[P_DARK]);
		say(cn,"Offensive Value: %d, WV: %d",get_attack_skill(cn),ch[cn].value[0][V_WEAPON]);
		say(cn,"Defensive Value: %d, AV: %d",get_parry_skill(cn),ch[cn].value[0][V_ARMOR]/20);
                say(cn,"Mod Immunity: %d",ch[cn].value[0][V_IMMUNITY]+tactics2immune(ch[cn].value[0][V_TACTICS]+14));
		say(cn,"War Immunity: %d",ch[cn].value[0][V_IMMUNITY]+ch[cn].value[0][V_TACTICS]/10);
		say(cn,"Flash Power: %d",ch[cn].value[0][V_FLASH]+tactics2spell(ch[cn].value[0][V_TACTICS]));
                say(cn,"x=%d, y=%d, speedmode=%d",ch[cn].restx,ch[cn].resty,ch[cn].speed_mode);
		say(cn,"undead=%s, alive=%s",(ch[cn].flags&CF_UNDEAD) ? "yes" : "no",(ch[cn].flags&CF_ALIVE) ? "yes" : "no");
	}
}

int get_lifeshield_max(int cn)
{
	if (ch[cn].value[0][V_MAGICSHIELD]) return ch[cn].value[0][V_MAGICSHIELD];
	return ch[cn].value[0][V_WARCRY];
}


void buggy_items(int cn)
{
	struct depot_ppd *ppd;
	int n,in;
	
	ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd));

	for (n=0; n<INVENTORYSIZE; n++) {
		if (n>=12 && n<30) continue;
		
		if ((in=ch[cn].item[n])) {
			sanitize_item(it+in,cn);
		}
	}

	if (ppd) {
		for (n=0; n<MAXDEPOT; n++) {
			if (ppd->itm[n].flags) {
                                sanitize_item(ppd->itm+n,cn);
			}
		}
	}
}

void bondtake_item(int in,int cn)
{
	it[in].flags|=IF_BONDTAKE;
	it[in].ownerID=ch[cn].ID;
}

void bondwear_item(int in,int cn)
{
	it[in].flags|=IF_BONDWEAR;
	it[in].ownerID=ch[cn].ID;
}

int cnt_free_inv(int cn)
{
	int n,cnt;

	for (n=30,cnt=0; n<INVENTORYSIZE; n++) {
		if (!ch[cn].item[n]) cnt++;		
	}
	return cnt;
}

void do_whostaff(int cnID)
{
	int co;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].flags&CF_INVISIBLE) continue;
		if (!(ch[co].flags&CF_PLAYER)) continue;
		if (!(ch[co].flags&(CF_STAFF|CF_GOD))) continue;

		tell_chat(0,cnID,1,"%s [%s]%s",
			  ch[co].name,
			  ch[co].staff_code,
			  ch[co].driver==0 ? "" : " (lagging)");
	}
}

void update_item(int in)
{
	int cn;

	if (in<1 || in>=MAXITEM) {
		elog("update item called with illegal in=%d",in);
		return;
	}
	if (!it[in].flags) {
		elog("update item called with freed in=%d (%s)",in,it[in].name);
		return;
	}

	if ((cn=it[in].carried)) {
		if (cn<1 || cn>=MAXCHARS) {
			elog("update item wants to use illegal cn=%d",cn);
			return;
		}
		if (!ch[cn].flags) {
			elog("update item wants to use freed cn=%d (%s)",cn,ch[cn].name);
			return;
		}
                ch[it[in].carried].flags|=CF_ITEMS;
                return;
        }
	if (it[in].contained) return;
	
	if (it[in].x<1 || it[in].x>=MAXMAP || it[in].y<1 || it[in].y>=MAXMAP) {
		elog("update item wants to use illegal map pos %d,%d",it[in].x,it[in].y);
		return;
	}
        set_sector(it[in].x,it[in].y);
}

void player_use_potion(int cn)
{
	int n,in;
	struct lostcon_ppd *ppd;

	ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd));
	if (!ppd) return;	// oops...

	if (ch[cn].hp<ch[cn].value[1][V_HP]*(int)(POWERSCALE*0.50)) {
                if (!ppd->nolife || !ppd->nocombo) {
			for (n=30; n<INVENTORYSIZE; n++) {
				if ((in=ch[cn].item[n]) && it[in].driver==IDR_POTION && it[in].drdata[1]) {
					if (it[in].drdata[2] && !ppd->nocombo) { use_item(cn,in); break; }
					if (!it[in].drdata[2] && !ppd->nolife) { use_item(cn,in); break; }
				}
			}
		}
	}	
}

void player_use_recall(int cn)
{
	int n,in;
	struct lostcon_ppd *ppd;

	// dont use recall *after* you died
	if (ch[cn].hp<(POWERSCALE/2)) return;

	ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd));
	if (!ppd) return;	// oops...

	if (ch[cn].hp<ch[cn].value[1][V_HP]*(int)(POWERSCALE*0.40) && !ppd->norecall) {
		for (n=30; n<INVENTORYSIZE; n++) {
			if ((in=ch[cn].item[n]) && it[in].driver==IDR_RECALL && it[in].drdata[0]>=ch[cn].level) {
				use_item(cn,in); return;
			}
		}
	}
}

int check_can_wear_item(int cn)
{
	int in,n;

        for (n=0; n<12; n++) {
		if ((in=ch[cn].item[n]) && !can_wear(cn,in,n) && give_char_item(cn,in)) {
			dlog(cn,in,"non-wearable item removed");
			ch[cn].item[n]=0;
		}
	}
	return 0;
}

void sanitize_item(struct item *in,int cn)
{
	int n,cnt=0;
	int armor=0,weapon=0;
	int armor_req=0,weapon_req=0,req,req_idx=-1,req_type;
	double ex;

	if (in->min_level>199) return;
	if (!strcmp(in->name,"Ring of Islena")) return;
	if (!strncmp(in->description,"A sacred ",9)) return;
	if (!strncmp(in->name,"Demon ",6)) return;
        if (in->driver==IDR_FLASK) return;
	if (in->flags&IF_BEYONDBOUNDS) return;
	if (in->flags&IF_BEYONDMAXMOD) return;

	for (n=0; n<MAXMOD; n++) {
		if (!in->mod_value[n]) continue;

		switch(in->mod_index[n]) {
			case V_ARMOR: 		armor=in->mod_value[n]; continue;
			case V_WEAPON:		weapon=in->mod_value[n]; continue;
			case V_LIGHT: 		continue;
			case V_SPEED:		dlog(cn,0,"%s: change raw speed to speed skill",in->name);
						in->mod_index[n]=V_SPEEDSKILL;
						break;
			case V_EMPTY:		dlog(cn,0,"%s: remove +empty",in->name);
						in->mod_index[n]=in->mod_value[n]=0;
						continue;
			case -V_ARMORSKILL:	armor_req=in->mod_value[n]; req_idx=n; continue;
			case -V_DAGGER:
			case -V_SWORD:		
			case -V_STAFF:		
			case -V_TWOHAND:	weapon_req=in->mod_value[n]; req_idx=n; continue;
		}
		if (in->mod_index[n]<0) continue;

		if (in->mod_value[n]>20) in->mod_value[n]=20;

		cnt++;
	}
	// check for 4/5 stat
	if (cnt>3) {
		in->min_level=250;
		strcpy(in->description,"This item is locked (4/5stat). Please contact game@astonia.com.");
		dlog(cn,0,"%s: item locked, 4/5stat",in->name);
		return;
	}
	// check for missing or wrong armor skill requirement
	if (armor) {
		if (in->flags&IF_WNARMS) {
			ex=(armor)/(20.0/100.0*15.0)/10.0;
     			req=(int)(ex+0.5);
			if (req>1) req=req*10-10;
		} else if (in->flags&IF_WNBODY) {
			ex=(armor)/(20.0/100.0*50.0)/10.0;
			req=(int)(ex+0.5);
			if (req>1) req=req*10-10;
		} else if (in->flags&IF_WNHEAD) {
			ex=(armor)/(20.0/100.0*20.0)/10.0;
			req=(int)(ex+0.5);
			if (req>1) req=req*10-10;
		} else if (in->flags&IF_WNLEGS) {
			ex=(armor)/(20.0/100.0*15.0)/10.0;
			req=(int)(ex+0.5);
			if (req>1) req=req*10-10;
		} else {
			elog("sanitize_item: cannot deal with item %s of character %s",in->name,ch[cn].name);
			return;
		}
		if (req!=armor_req) {
			dlog(cn,0,"%s: armor: ex=%f %d, req=%d, req change %d to %d",in->name,ex,(int)(ex+0.5),req,armor_req,req);
			if (req_idx!=-1) {
				in->mod_value[req_idx]=req;
			} else {
				for (n=0; n<MAXMOD; n++) {
					if (!in->mod_value[n]) {
						in->mod_index[n]=-V_ARMORSKILL;
						in->mod_value[n]=req;
						break;
					}
				}
				if (n==MAXMOD) {
					elog("sanitize_item: cannot deal with item %s of character %s",in->name,ch[cn].name);
					return;
				}
			}
		}
	}
	// check for missing or wrong weapon skill requirement
	if (weapon) {
		if (in->flags&IF_TWOHAND) req=(int)((weapon-3)/10.0+0.5);
                else req=(int)(weapon/10.0+0.5);
		if (req>1) req=req*10-10;

		if (in->flags&IF_DAGGER) req_type=V_DAGGER;
		else if (in->flags&IF_SWORD) req_type=V_SWORD;
		else if (in->flags&IF_STAFF) req_type=V_STAFF;
		else if (in->flags&IF_TWOHAND) req_type=V_TWOHAND;
		else {
			elog("sanitize_item: cannot deal with item %s of character %s",in->name,ch[cn].name);
			return;
		}

		if (req!=weapon_req) {
			dlog(cn,0,"%s: weapon: req=%d, req change %d to %d",in->name,req,weapon_req,req);
			if (req_idx!=-1) {
				in->mod_value[req_idx]=req;
			} else {
				for (n=0; n<MAXMOD; n++) {
					if (!in->mod_value[n]) {
						in->mod_index[n]=-req_type;
						in->mod_value[n]=req;
						break;
					}
				}
				if (n==MAXMOD) {
					elog("sanitize_item: cannot deal with item %s of character %s",in->name,ch[cn].name);
					return;
				}
			}
		}
	}
}

int count_enhancements(int in)
{
	int m,cnt=0;

	// check requirements
	for (m=0; m<MAXMOD; m++) {
                if (!(it[in].mod_value[m])) continue;	// ignore empty ones
		if (it[in].mod_index[m]<0) continue;				// ignore requirements

		switch(it[in].mod_index[m]) {
			 case V_WEAPON:	break;
			 case V_ARMOR:	break;
			 case V_SPEED:	break;
			 case V_DEMON:	break;
			 case V_LIGHT:	break;
			 default:	cnt++;
					break;
		}
	}
	return cnt;
}







