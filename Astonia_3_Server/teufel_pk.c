/*
 * $Id: teufel_pk.c,v 1.4 2006/06/25 10:38:06 ssim Exp $
 *
 * $Log: teufel_pk.c,v $
 * Revision 1.4  2006/06/25 10:38:06  ssim
 * changed teleport on death to proper destination
 *
 * Revision 1.3  2006/06/23 16:21:07  ssim
 * added item loss logic
 *
 * Revision 1.2  2006/04/26 16:05:44  ssim
 * added hack for teufelheim arena: no bless other allowed
 *
 * Revision 1.1  2006/04/24 17:22:42  ssim
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "server.h"
#include "date.h"
#include "tool.h"
#include "log.h"
#include "talk.h"
#include "drvlib.h"
#include "drdata.h"
#include "database.h"
#include "teufel_pk.h"

#define MAXTEUFEL	10

struct teufel_pk_data
{
	int cc[MAXTEUFEL];
	int dam[MAXTEUFEL];
	char name[MAXTEUFEL][16];
	int last_dam;
};

void teufel_damage(int cn,int cc,int dam)
{
	int n;

	struct teufel_pk_data *dat;

	dat=set_data(cn,DRD_TEUFELPK,sizeof(struct teufel_pk_data));
	if (!dat) return;	// oops...

	if (realtime-dat->last_dam>20) {
		for (n=0; n<MAXTEUFEL; n++) dat->cc[n]=dat->dam[n]=0;
	}
	for (n=0; n<MAXTEUFEL; n++) {
		if (dat->cc[n]==cc) {
			dat->dam[n]+=dam;
			break;
		}
		if (dat->cc[n]==0) {
			dat->cc[n]=cc;
			dat->dam[n]=dam;
			strncpy(dat->name[n],ch[cc].name,15);
			dat->name[n][15]=0;
			break;
		}
	}
	dat->last_dam=realtime;
}

void secure_log(int cc,int co,char *what)
{
	if (!(ch[cc].flags&CF_USED)) return;
	if (!(ch[cc].flags&CF_PLAYER)) return;

	if (!(ch[co].flags&CF_USED)) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

	log_char(cc,LOG_SYSTEM,0,"You got %s on %s.",what,ch[co].name);
}

void winner_gets_item(int cc,int co)
{
	int in,rnd;

        if (!(ch[cc].flags&CF_USED)) return;
	if (!(ch[cc].flags&CF_PLAYER)) return;
	
	if (!(ch[co].flags&CF_USED)) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

        rnd=RANDOM(12);

        if (!(in=ch[co].item[rnd])) return;
	if (!count_enhancements(in)) return;
        if (give_char_item(cc,in)) {
		ch[co].item[rnd]=0;
		ch[co].flags|=CF_ITEMS;
		dlog(cc,in,"won in teufelheim PK from %s",ch[co].name);
		dlog(co,in,"lost in teufelheim PK to %s",ch[cc].name);
		log_char(cc,LOG_SYSTEM,0,"You won a %s from %s!",it[in].name,ch[co].name);
		log_char(co,LOG_SYSTEM,0,"%s won a %s from you!",ch[cc].name,it[in].name);
	}
}

void teufel_death(int cn,int cc)
{
	struct teufel_pk_data *dat;
	int n;
	int kill_n=-1,dam=0,killer=-1;

	dat=set_data(cn,DRD_TEUFELPK,sizeof(struct teufel_pk_data));
	if (!dat) return;	// oops...


	for (n=0; n<MAXTEUFEL; n++) {
		if (dat->cc[n]) {
			xlog("killed by %s, damage %.2f",dat->name[n],dat->dam[n]/1000.0f);
			if (dat->dam[n]>dam) {
				dam=dat->dam[n];
				kill_n=n;
				killer=dat->cc[n];
			}
		}
	}

	if (kill_n==-1 || killer==-1) {
		elog("no one got the kill?");
	} else {
	
		db_new_pvp();
	
		for (n=0; n<MAXTEUFEL; n++) {
			if (dat->cc[n]) {
                                if (n==kill_n) {
					db_add_pvp(dat->name[n],ch[cn].name,"kill",dat->dam[n]);
					secure_log(dat->cc[n],cn,"a kill");
				} else if (dat->cc[n]==cc) {
					db_add_pvp(dat->name[n],ch[cn].name,"final",dat->dam[n]);
					secure_log(dat->cc[n],cn,"a final blow");
				} else {
					db_add_pvp(dat->name[n],ch[cn].name,"assist",dat->dam[n]);
					secure_log(dat->cc[n],cn,"an assist");
				}
			}
		}
		winner_gets_item(killer,cn);
	}
	
	del_data(cn,DRD_TEUFELPK);

	if (ch[cn].x>=120 && ch[cn].x<=254 && ch[cn].y>=139 && ch[cn].y<=228) {
		if (teleport_char_driver(cn,225,249)) ;
		else if (teleport_char_driver(cn,221,248)) ;
		else if (teleport_char_driver(cn,227,245)) ;
		else if (teleport_char_driver(cn,219,241)) ;
		else teleport_char_driver(cn,216,237);
	} else { // error fallback
		if (teleport_char_driver(cn,250,250)) ;
		else if (teleport_char_driver(cn,247,250)) ;
		else if (teleport_char_driver(cn,250,247)) ;
		else if (teleport_char_driver(cn,247,247)) ;
		else teleport_char_driver(cn,245,247);
	}

	ch[cn].hp=10*POWERSCALE;
}




