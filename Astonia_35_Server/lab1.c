/*
 * $Id: lab1.c,v 1.2 2007/10/29 13:58:10 devel Exp $
 *
 * $Log: lab1.c,v $
 * Revision 1.2  2007/10/29 13:58:10  devel
 * deathfibrin no longer usable when on the ground
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "server.h"
#include "log.h"
#include "notify.h"
#include "direction.h"
#include "do.h"
#include "path.h"
#include "error.h"
#include "drdata.h"
#include "see.h"
#include "death.h"
#include "talk.h"
#include "effect.h"
#include "database.h"
#include "map.h"
#include "create.h"
#include "container.h"
#include "drvlib.h"
#include "tool.h"
#include "spell.h"
#include "effect.h"
#include "light.h"
#include "date.h"
#include "los.h"
#include "skill.h"
#include "item_id.h"
#include "libload.h"
#include "player_driver.h"
#include "task.h"
#include "poison.h"
#include "misc_ppd.h"
#include "act.h"
#include "lab.h"
#include "sector.h"
#include "consistency.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);			// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies
int ch_respawn_driver(int nr,int cn);					// called when an NPC is about to respawn

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case CDT_DRIVER:	return ch_driver(nr,obj,ret,lastact);
		case CDT_ITEM: 		return it_driver(nr,obj,ret);
		case CDT_DEAD:		return ch_died_driver(nr,obj,ret);
		case CDT_RESPAWN:	return ch_respawn_driver(nr,obj);
		default: 		return 0;
	}
}

//----------------------- drvlib extension

static int attack_check_target(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
	if (map[m].flags&MF_DOOR) return 1;
	if (map[m].ch && (ch[map[m].ch].flags&(CF_PLAYER|CF_PLAYERLIKE)) && ch[map[m].ch].action==AC_IDLE) return 1;
        if (map[m].flags&MF_TMOVEBLOCK) return 0;

	return 1;
}

// goto item and use it, returns 0 if not possible. check co if there is an enemy
int use_and_attack_driver(int cn,int in,int spec,int *co)
{
	int dx,dy,dir,mn;

        if (co) *co=0;

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
		if (!(map[it[in].x+it[in].y*MAXMAP+1].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x+1,it[in].y,0,attack_check_target,0);
		if (dir==-1 && !(map[it[in].x+it[in].y*MAXMAP+MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x,it[in].y+1,0,attack_check_target,0);
	} else dir=pathfinder(ch[cn].x,ch[cn].y,it[in].x,it[in].y,1,attack_check_target,0);
	
	if (dir!=-1) {
                if (do_walk(cn,dir)) return 1;
                if (do_use(cn,dir,0)) return 1;

                if (co) {
                        dx2offset(dir,&dx,&dy,NULL);
                        mn=ch[cn].x+dx+(ch[cn].y+dy)*MAXMAP;
                        if (attack_check_target(mn)) *co=map[mn].ch;
                }
        }

        return 0;
}

// ------------------------------

#define MAX_GNOMETORCH  10

struct labgnome_driver_data
{
        int usetarget; // short2int
        unsigned char numtorch;
        int torch[MAX_GNOMETORCH]; // short2int
        unsigned char outch;
        unsigned char fighter;
        unsigned char master;
        unsigned char text;

        unsigned char aggressive;
        unsigned char helper;
        unsigned char dir;
};

static int __gnomex;
static int __gnomey;

int qsortproc_gnometorch(const void *a, const void *b)
{
        int da,db;

        da=(it[*((int *)(a))].x-__gnomex)*(it[*((int *)(a))].x-__gnomex)+(it[*((int *)(a))].y-__gnomey)*(it[*((int *)(a))].y-__gnomey); // short2int
        db=(it[*((int *)(b))].x-__gnomex)*(it[*((int *)(b))].x-__gnomex)+(it[*((int *)(b))].y-__gnomey)*(it[*((int *)(b))].y-__gnomey); // short2int
        return db-da;
}

static int scan_gnometorch_check_target(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
        if (map[m].it && it[map[m].it].driver==2) return 0;
	return 1;
}

void scan_gnometorches(int cn, struct labgnome_driver_data *dat)
{
        int x,y,sx,sy,ex,ey,m;

        sx=max(ch[cn].x-15,0);
        sy=max(ch[cn].y-15,0);
        ex=min(ch[cn].x+15,MAXMAP);
        ey=min(ch[cn].y+15,MAXMAP);

        for (y=sy; y<ey && dat->numtorch<MAX_GNOMETORCH; y++) {
                for (x=sx; x<ex && dat->numtorch<MAX_GNOMETORCH; x++) {
                        m=x+y*MAXMAP;
                        if (map[m].it && it[map[m].it].driver==IDR_LABTORCH && los_can_see(cn,ch[cn].x,ch[cn].y,x,y,15)) { //&& char_see_item(cn,map[m].it)*/) {
                                if (pathfinder(ch[cn].x,ch[cn].y,x,y,1,scan_gnometorch_check_target,0)==-1) continue;
                                dat->torch[dat->numtorch++]=map[m].it;
                        }
                }
        }

        // sort by distance
        __gnomex=ch[cn].x;
        __gnomey=ch[cn].y;
        qsort(dat->torch,dat->numtorch,sizeof(int),qsortproc_gnometorch); // short2int

}

void labgnome_driver_parse(int cn, struct labgnome_driver_data *dat)
{
	char *ptr,name[64],value[64];

        dat->fighter=0;
        dat->text=0;
        dat->master=0;

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {
                if (!strcmp(name,"fighter")) dat->fighter=atoi(value);
                else if (!strcmp(name,"text")) dat->text=atoi(value);
                else if (!strcmp(name,"master")) dat->master=atoi(value);
                else if (!strcmp(name,"aggressive")) dat->aggressive=atoi(value);
                else if (!strcmp(name,"helper")) dat->helper=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void labgnome_driver(int cn, int ret, int lastact)
{
        struct labgnome_driver_data *dat;
	struct msg *msg,*next;
        int co,in,i,mn;

        dat=set_data(cn,DRD_LABGNOMEDRIVER,sizeof(*dat));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {

                        labgnome_driver_parse(cn,dat);
                        if (dat->fighter) fight_driver_set_dist(cn,30,0,60);
                        else if (dat->master) {
                                fight_driver_set_dist(cn,14,0,14);
                                ch[cn].flags|=CF_IMMORTAL;
                        }
                        else { fight_driver_set_dist(cn,15,0,15); scan_gnometorches(cn,dat); }

                        // turn inside room
                        mn=ch[cn].x+ch[cn].y*MAXMAP;
                        dat->dir=DX_DOWN;

                        if (map[mn-1].flags&MF_SIGHTBLOCK) dat->dir=DX_RIGHT;
                        if (map[mn+1].flags&MF_SIGHTBLOCK) dat->dir=DX_LEFT;

                }

                // we got a torch calling
                if (msg->type==NT_NPC && msg->dat1==NTID_LABGNOMETORCH) {
                        in=msg->dat2;
                        co=msg->dat3;

                        // is it one of our torches?
                        for (i=0; i<dat->numtorch; i++) {
                                if (in==dat->torch[i] && fight_driver_add_enemy(cn,co,1,1)) { shout(cn,"Hurgha. Master me told protecting torch. Prepare to die %s!",ch[co].name); break; }
                        }
                }

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                standard_message_driver(cn,msg,dat->aggressive,dat->helper);
                remove_message(cn,msg);
	}

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        // checking torches
        if (dat->usetarget==0 || it[dat->usetarget].drdata[0]==1) {

                dat->usetarget=0;

                for (i=0; i<dat->numtorch; i++) {
                        if (it[dat->torch[i]].drdata[0]==0) {   // torch is off
                                dat->usetarget=dat->torch[i];
                                say(cn,"Master me told keeping torch burning.");
                        }
                }
        }

        // use torch, or attack all (possible) chars around it
        if (dat->usetarget) {
                if (use_and_attack_driver(cn,dat->usetarget,0,&co)) return;
                if (co) if (fight_driver_add_enemy(cn,co,1,1)) shout(cn,"You're in my way %s! Die!",ch[co].name);
        }

        // rest of standard action
	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;
        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;

        // nonsense
        if (!dat->master) {
                switch(RANDOM(500)) {
                        case 0: say(cn,"Grmadasd."); break;
                        case 1: say(cn,"Huas. Grkasd Wod."); break;
                        case 2: if (dat->numtorch) say(cn,"Me have to protect %d torch. Hungrfa.",dat->numtorch); break;
                        case 3: say(cn,"Me have dark here feeling."); break;
                }
        }

        // nothing left to do
        do_idle(cn,TICKS/2);
}

void labgnome_died_driver(int cn, int co)
{
        struct labgnome_driver_data *dat;

        dat=set_data(cn,DRD_LABGNOMEDRIVER,sizeof(*dat));
	if (!dat) return;	// oops...

        if (dat->text) say(cn,"Arrrggh. %s me killed, but %s never kills master behind door. Master can be killed only by Deathfibrin.",ch[co].name,ch[co].name);
	if (dat->master) {
		if (co && (ch[co].flags&CF_PLAYER)) create_lab_exit(co,20);
	}
}

void labtorch(int in, int cn)
{
        if (!cn) {
                it[in].drdata[1]=it[in].mod_value[0];
                return;
        }

        if (it[in].drdata[0]==0) {
		if (ch[cn].flags&CF_PLAYER) return;	// players may not light torches
		
                it[in].sprite++;
                it[in].drdata[0]=1;
                it[in].mod_value[0]=it[in].drdata[1];
                add_item_light(in);
        } else {
                remove_item_light(in);
                it[in].sprite--;
                it[in].drdata[0]=0;
                it[in].mod_value[0]=0;
                notify_area(it[in].x,it[in].y,NT_NPC,NTID_LABGNOMETORCH,in,cn);
        }
}

struct deathfibrin_data
{
        int amount;
        char init;
        char used;
        int tickerused;         // used to prevent staff from vanishing during use
        int tickervanish;       // used to remove the staff from the map if it's to long alone
};

int deathfibrin_scan(int cn)
{
        int x,y,sx,sy,ex,ey,m,co;

        sx=max(ch[cn].x-8,0);
        sy=max(ch[cn].y-8,0);
        ex=min(ch[cn].x+8,MAXMAP);
        ey=min(ch[cn].y+8,MAXMAP);

        for (y=sy; y<ey; y++) {
                for (x=sx; x<ex; x++) {
                        m=x+y*MAXMAP;
                        if ((co=map[m].ch) && ch[co].driver==CDR_LABGNOMEDRIVER && !strcmp(ch[co].name,"Immortal Master") && char_see_char(cn,co)) return co;
                }
        }
        return 0;
}

int deathfibrin_check(int in, struct deathfibrin_data *dat)
{
        int oldsprite;

        // check if it should vanisch
        if (dat->amount==0) {
                remove_item(in);
                destroy_item(in);
                return 1;
        }

        // determin the sprite number
        oldsprite=it[in].sprite;
        it[in].sprite=min(10427,10428-10*(dat->amount+500)/10000);
        if (oldsprite!=it[in].sprite) update_item(in);

        sprintf(it[in].description,"Staff containing %d%% Deathfibrin",dat->amount/100);

        return 0;
}

void deathfibrin(int in, int cn)
{
        int co,x,y,fn,mn,in2;
        struct deathfibrin_data *dat;

        // shrine
        if (it[in].sprite==10428) {
                if (!cn) return;

                if (ch[cn].citem) { log_char(cn,LOG_SYSTEM,0,"The Shrine of Deathfibrin seems to ignore everything. It may want to give you something."); return; }

                in2=create_item("deathfibrin");
                log_char(cn,LOG_SYSTEM,0,"You received a %s.",it[in2].name);
                bondtake_item(in2,cn);

		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took deathfibrin");
                ch[cn].citem=in2;
                ch[cn].flags|=CF_ITEMS;
                it[in2].carried=cn;

                return;
        }

        // staff
        dat=(struct deathfibrin_data *)it[in].drdata;

        if (cn) {
		if (!it[in].carried) {
			log_char(cn,LOG_SYSTEM,0,"You need to carry this to use it.");
			return;
		}
                // scan around to find the master
                co=deathfibrin_scan(cn);
                if (!co) { log_char(cn,LOG_SYSTEM,0,"Nothing happens. There is no immortal close enough. %d",map[ch[cn].x+ch[cn].y*MAXMAP].light); return; }

                //  add the show effect
                fn=create_show_effect(EF_PULSEBACK,cn,ticker,ticker+7,20,42);
                if (fn) {
                        ef[fn].x=ch[co].x;
                        ef[fn].y=ch[co].y;
                }
                dat->tickerused=ticker+7;

                // hurt the master
                ch[co].flags&=~CF_IMMORTAL;
                hurt(co,10*POWERSCALE,cn,1,0,0);
                ch[co].flags|=CF_IMMORTAL;
                say(co,"Oh no! Deathfibrin hurts.");

                // remove amount
                dat->amount=max(0,dat->amount-1000);
                if (deathfibrin_check(in,dat)) { log_char(cn,LOG_SYSTEM,0,"°c3Your %s vanished.",it[in].name); return; }
        } else {
                // init values
                if (dat->init==0) {
                        dat->init=1;
                        dat->amount=10000;
                        deathfibrin_check(in,dat);
                }

                // get position
                if (it[in].carried) {
                        co=it[in].carried;
                        x=ch[co].x;
                        y=ch[co].y;
                        dat->tickervanish=0;
                }
                else {
                        co=0;
                        x=it[in].x;
                        y=it[in].y;
                        if (dat->tickervanish==0) dat->tickervanish=ticker+10*60*TICKS;
                }

                // remove amount (do not if it's in use)
                if (map[mn=x+y*MAXMAP].light>4 && ticker>dat->tickerused) {
                        dat->amount=max(0,dat->amount-(map[mn].light-3)*(map[mn].light*map[mn].light-3));
                        if (deathfibrin_check(in,dat) && co) { log_char(co,LOG_SYSTEM,0,"°c3Your %s vanished.",it[in].name); return; }
                }

                // remove staff, if it's left alone to long
                if (dat->tickervanish && ticker>dat->tickervanish) { dat->amount=0; deathfibrin_check(in,dat); return; }

                // set timer
                call_item(IDR_DEATHFIBRIN,in,0,ticker+TICKS/4);
        }
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_LABGNOMEDRIVER:         labgnome_driver(cn,ret,lastact); return 1;
                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_LABTORCH: labtorch(in,cn); return 1;
                case IDR_DEATHFIBRIN: deathfibrin(in,cn); return 1;
		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_LABGNOMEDRIVER:        labgnome_died_driver(cn,co); return 1;
                default:	                return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_LABGNOMEDRIVER:        return 1;
		default:		        return 0;
	}
}




















