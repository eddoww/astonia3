/*

$Id: saltmine.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: saltmine.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2003/11/03 12:43:51  sam
fixed typo

Revision 1.2  2003/10/13 14:12:41  sam
Added RCS tags


*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

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
#include "sector.h"

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

// -- talk helpers --------------------------------------------------------------------------------------------------

struct talk_data
{
        int cv_co;      // current victim
        int cv_serial;  // current victim
        int lasttalk;
};

// -- general stuff -------------------------------------------------------------------------------------------------

#define SALTMINE_PPD_VERSION            1
#define MAXLADDER                       20   // absolute max is 256
#define LADDER_REUSE_TIME               (60*60*24) // in seconds - uses time(), not the ticker

struct saltmine_ppd
{
        char version;                   // increase version number to reset ppd
        char useitemflag;               // will be deleted by first monk that is willing to take the ladder (temporary, but i placed it in the ppd)
        char quitflag;                  // temporary. the monks will quit if

        unsigned char gatamastate;

        int laddertime[MAXLADDER];      // remembers the time you used this ladder
        int salt;                       // number of succesful salt deliveries not received a reward for yet
};

struct saltmine_worker_data
{
        int follow_cn,follow_serial;    // cn and serial of "owner"
        int useitem;
        int waituntil;
        int turnvisible;
        char hassalt,leftmonastery,d3,d4;
};

// -- gatama --------------------------------------------------------------------------------------------------------

struct saltmine_ppd *get_saltmine_ppd(int cn)
{
        struct saltmine_ppd *dat;

        dat=set_data(cn,DRD_SALTMINE_PPD,sizeof(struct saltmine_ppd));
        if (!dat) return NULL;

        if (dat->version!=SALTMINE_PPD_VERSION) {
                bzero(dat,sizeof(struct saltmine_ppd));
                dat->version=SALTMINE_PPD_VERSION;
        }

        return dat;
}

int saltbag_itemx=189,saltbag_itemy=8;
int door_itemx=177,door_itemy=19;

void create_worker(int follow_cn, struct saltmine_ppd *ppd)
{
        int cn;
        struct saltmine_worker_data *workerdat;

        // create
        cn=create_char("monk_worker",0);
        if (!cn) { xlog("create_char() failed in %s %d",__FILE__,__LINE__); return; }

        snprintf(ch[cn].name,sizeof(ch[cn].name)-1,"%s's Monk",ch[follow_cn].name); ch[cn].name[sizeof(ch[cn].name)-1]=0;
        ch[cn].dir=DX_DOWN;
        update_char(cn);

        ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
        ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
        ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

        // set initital values
        if ((workerdat=set_data(cn,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) {
                workerdat->follow_cn=follow_cn;
                workerdat->follow_serial=ch[follow_cn].serial;
        }
        else {
                destroy_char(cn);
                xlog("set_data() failed in %s %d",__FILE__,__LINE__);
                return;
        }

        // drop him to map
        if (!drop_char(cn,179,19,0) && !drop_char(cn,182,19,0)) { destroy_char(cn); xlog("drop_char() failed (%s,%d)",__FILE__,__LINE__); return; }

        // set first coords
        ch[cn].tmpx=ch[follow_cn].x;
        ch[cn].tmpy=ch[follow_cn].y;
}

void remove_worker(int cn)
{
        struct saltmine_worker_data *workerdat;
        struct saltmine_ppd *ppd;

        // get data
        if (!(workerdat=set_data(cn,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) return;
        if (!(ppd=get_saltmine_ppd(workerdat->follow_cn))) return;

        remove_destroy_char(cn);
}

int in_monastery(int x, int y)
{
        if (x>=183 && y>=5 && x<=210 && y<=24) return 1;
        if (x>=177 && y>=17 && x<=185 && y<=21) return 1; // grrr
        return 0;
}

int monksout(int cn, struct saltmine_ppd *ppd)
{
        int co;
        struct saltmine_worker_data *workerdat;

        if ((co=getfirst_char())) do {
                if (ch[co].driver!=CDR_MONK_WORKER) continue;
                if (!(workerdat=set_data(co,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) continue;
                if (workerdat->follow_cn!=cn) continue;
                return 1;
        } while((co=getnext_char(co)));

        return 0;
}

void monk_gatama_driver(int cn, int ret, int lastact)
{
        static struct talk_data talkbuf;   // we only have one, so there is no need to use the memory system (?)
        struct talk_data *talkdat=&talkbuf;
        struct saltmine_ppd *ppd;
        struct msg *msg,*next;
        int co;
        int talkdir=0,didsay=0,noturn=0;
        char *str;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,20,0,30);
                }

                if (msg->type==NT_GIVE) {
                        if (!ch[cn].citem) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source
                        co=msg->dat1;

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_CHAR) {

                        co=msg->dat1;

                        // dont talk
                        if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }             // dont talk to other NPCs
                        if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }            // dont talk to players without connection
                        if (ticker<talkdat->lasttalk+5*TICKS) { remove_message(cn,msg); continue; }      // only talk when the old sentence is read
                        if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }       // dont talk to someone we cant see, and dont talk to ourself

                        // get ppd
                        if (!(ppd=get_saltmine_ppd(co))) { remove_message(cn,msg); continue; }

                        if (char_dist(cn,co)>7) { ppd->gatamastate=0; remove_message(cn,msg); continue; } // dont talk to someone far away

                        // remove cv
                        if (talkdat->cv_co) {
                                if (!ch[talkdat->cv_co].flags || ch[talkdat->cv_co].serial!=talkdat->cv_serial || char_dist(cn,talkdat->cv_co)>7 || !char_see_char(cn,talkdat->cv_co)) {
                                        talkdat->cv_co=0;
                                }
                        }

                        // only talk to cv
                        if (talkdat->cv_co && talkdat->cv_co!=co) { remove_message(cn,msg); continue; }

                        // set new cv
                        if (!talkdat->cv_co) {
                                talkdat->cv_co=co;
                                talkdat->cv_serial=ch[co].serial;
                        }

                        switch(ppd->gatamastate) {
                                case 0: if (ppd->salt) ppd->gatamastate=50; else ppd->gatamastate=10;
                                        break;

                                // Intro
                                case 10: say(cn,"Welcome %s. The Monastery of Kir Laka needs thine help. We live from the salt we get from the saltmine thou can find to the west. But now many golems appeared in the mine and we had to run for our lives, and all the salt we had already mined is lost there.",ch[co].name); didsay=1; ppd->gatamastate++; break;
                                case 11: say(cn,"Thou might want to know more °c4details°c0 of what thee can do for the monastery. After this, thou might °c4begin°c0 helping us, or not."); didsay=1; ppd->gatamastate++; break;
                                case 12: talkdat->cv_co=0; break;

                                // Details
                                case 20: say(cn,"Thou will have to lead group of monks safely to certain ladders in the saltmine. Once reached, thou \"use\" the ladder, and one of the monks will get the salt out of that place. Every ladder can be used only once every 12 astonian days, and every monk can only carry one bag of salt."); didsay=1; ppd->gatamastate++; break;
                                case 21: say(cn,"The monks will follow thee, and help thee against the golems. Once left, thou canst return to the monastery any time you like, the monks will then deposit their salt, if they have any, and will rest then. Thou can give some commands to the monks by simply speaking to them. Those commands are:");
                                         say(cn,"wait - and they will wait a short while.");
                                         say(cn,"come - and they will come to thee.");
                                         say(cn,"salt - and they will show thee if they carry salt, or not.");
                                         didsay=1; ppd->gatamastate++; break;
                                case 22: say(cn,"Hopefully thou now decide to °c4begin°c0 helping us."); didsay=1; ppd->gatamastate++; break;
                                case 23: talkdat->cv_co=0; break;

                                // Begin (check)
                                case 30: if (monksout(co,ppd)) ppd->gatamastate=40; else ppd->gatamastate++; break;
                                case 31: say(cn,"Come here, Monks. The mighty %s offers %s help. I want to see and hear quick feet now!",ch[co].name,hisname(co)); didsay=1; ppd->gatamastate++;
                                         create_worker(co,ppd);
                                         create_worker(co,ppd);
                                         create_worker(co,ppd);
                                         break;
                                case 32: say(cn,"Lead them wise and carfully, %s. Mayest thou all return safe.",ch[co].name); didsay=1; ppd->gatamastate++;
                                case 33: talkdat->cv_co=0; break;

                                // ...
                                case 40: say(cn,"Let us wait until all thine Monks are resting. Either in the rest room, or in peace."); didsay=1; ppd->gatamastate++;
                                case 41: talkdat->cv_co=0; break;

                                // Reward
                                case 50: say(cn,"Thanks thee %s for thine help. Thou canst use the saltbag in the store room, to take thee thine reward. I trust thee, that thou wilt take the right amount.",ch[co].name); ppd->gatamastate++;
                                case 51: talkdat->cv_co=0; break;
                        }

                        if (didsay) {
                                talkdat->lasttalk=ticker;
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
                        }

		}

                if (msg->type==NT_TEXT) {

                        co=msg->dat3;
                        str=(char *)msg->dat2;

                        tabunga(cn,co,(char*)msg->dat2);

                        if (co==cn) { remove_message(cn,msg); continue; }
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co)) { remove_message(cn,msg); continue; }
                        if (char_dist(cn,co)>7) { remove_message(cn,msg); continue; }

                        // get ppd
                        if (!(ppd=get_saltmine_ppd(co))) { remove_message(cn,msg); continue; }

                        if (strcasestr(str,"REPEAT")) { say(cn,"I will repeat, %s",ch[co].name); ppd->gatamastate=0; }
                        else if (strcasestr(str,"DETAILS")) { ppd->gatamastate=20; }
                        else if (strcasestr(str,"BEGIN")) { ppd->gatamastate=30; }
                }

                standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

        if (talkdir) turn(cn,talkdir);

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        if (talkdat->lasttalk+TICKS*30<ticker || noturn) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;
                if (!noturn && (ticker/TICKS)%5==0) whisper(cn,"Auaaauuuuuuummmmmmmmmmm");
        }

        do_idle(cn,TICKS);
}

// -- govida --------------------------------------------------------------------------------------------------------

void monk_govida_driver(int cn, int ret, int lastact)
{
        do_idle(cn,TICKS/2);
}

// -- worker --------------------------------------------------------------------------------------------------------

void monk_worker_driver(int cn, int ret, int lastact)
{
        struct saltmine_worker_data *workerdat;
        struct saltmine_ppd *ppd;
        struct msg *msg,*next;
        int co,dir;
        char *str;

        // get data
        if (!(workerdat=set_data(cn,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) return;
        if (!(ppd=get_saltmine_ppd(workerdat->follow_cn))) return;

        // kill monk
        if ((!ch[workerdat->follow_cn].flags) || (ch[workerdat->follow_cn].serial!=workerdat->follow_serial)) {
                remove_destroy_char(cn);
                return;
        }

        if (char_dist(cn,workerdat->follow_cn)>90) {
                log_char(workerdat->follow_cn,LOG_SYSTEM,0,"°c3One of thine monks vanished!°c0");
                remove_destroy_char(cn);
                return;
        }

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,0,20,0);
                }

                if (msg->type==NT_GIVE) {
                        if (!ch[cn].citem) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source
                        co=msg->dat1;

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_TEXT) {

                        co=msg->dat3;
                        str=(char *)msg->dat2;

                        tabunga(cn,co,(char*)msg->dat2);

                        if (co==cn) { remove_message(cn,msg); continue; }
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			// if (!char_see_char(cn,co)) { remove_message(cn,msg); continue; }

                        if (co==workerdat->follow_cn) {
                                if (strcasestr(str,"COME")) { workerdat->waituntil=0; }
                                else if (strcasestr(str,"WAIT")) { workerdat->waituntil=ticker+20*TICKS; }
                                else if (strcasestr(str,"SALT")) {  log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,10,"%s %s his head.",ch[cn].name,workerdat->hassalt?"nods":"shakes");  }
                        }
                }

                if (msg->type==NT_CHAR) {
                        co=msg->dat1;

                        if (co==workerdat->follow_cn && !(ch[cn].flags&CF_INVISIBLE)) {
                                if (ch[co].x<216)  {
                                        ch[cn].tmpx=ch[co].x;
                                        ch[cn].tmpy=ch[co].y;
                                } else workerdat->useitem=map[door_itemx+door_itemy*MAXMAP].it;
                        }
                }

                if (msg->type==NT_NPC && msg->dat1==NTID_SALTMINE_USEITEM) {
                        if (msg->dat2==workerdat->follow_cn && !workerdat->hassalt && !workerdat->useitem && ppd->useitemflag) {
                                workerdat->useitem=msg->dat3;
                                ppd->useitemflag=0;
                        }
                }

                standard_message_driver(cn,msg,1,1);
                remove_message(cn,msg);
	}

        // monastery action of worker
        if (workerdat->leftmonastery && workerdat->leftmonastery+10*TICKS<ticker) {
                if (in_monastery(ch[cn].x,ch[cn].y)) {
                        if (workerdat->hassalt && !workerdat->useitem) workerdat->useitem=map[saltbag_itemx+saltbag_itemy*MAXMAP].it;
                        if (!workerdat->hassalt && !workerdat->useitem) workerdat->useitem=map[door_itemx+door_itemy*MAXMAP].it;
                }
        }
        else {
                if (!workerdat->leftmonastery && !in_monastery(ch[cn].x,ch[cn].y)) workerdat->leftmonastery=ticker;
        }

        if (workerdat->hassalt && ch[cn].speed_mode!=SM_STEALTH) ch[cn].speed_mode=SM_STEALTH;
        if (!workerdat->hassalt && ch[cn].speed_mode!=SM_NORMAL) ch[cn].speed_mode=SM_NORMAL;
        if (workerdat->hassalt && ch[cn].hp<ch[cn].value[1][V_HP]*POWERSCALE && ch[cn].speed_mode!=SM_NORMAL) ch[cn].speed_mode=SM_NORMAL;

        // fighting
        fight_driver_update(cn);
        if (ch[cn].hp>20*POWERSCALE) { if (fight_driver_attack_visible(cn,0)) return; }
        else {  if (fight_driver_flee(cn)) return; }
	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        if ((ch[cn].flags&CF_INVISIBLE)) {
                if (ticker>workerdat->turnvisible) {
                        ch[cn].flags&=~CF_INVISIBLE;
                }
                else if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;
        }
        else if (workerdat->useitem) {
                if (use_driver(cn,workerdat->useitem,0)) return;
        }
        else if (ticker>workerdat->waituntil) {
                if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,3)) return;
        }

        // turn to owner
        dir=offset2dx(ch[cn].x,ch[cn].y,ch[workerdat->follow_cn].x,ch[workerdat->follow_cn].y);
        if (ch[cn].dir!=dir) turn(cn,dir);

        do_idle(cn,TICKS/8); // they need to be fast, otherwise they act a bit stupid (easy solution. swap driver cause some deadlocks)
}

void monk_worker_died_driver(int cn, int co)
{
        struct saltmine_worker_data *workerdat;
        if (!(workerdat=set_data(cn,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) return;
        log_char(workerdat->follow_cn,LOG_SYSTEM,0,"°c3One of thine monks died!°c0");
}


// -- item driver ---------------------------------------------------------------------------------------------------

// drdata[0]=type               1=ladder
//                              2=saltbag
//                              3=door

// ladder                       drdata[1]=ladder number

void set_salt_data(int in)
{
	if (*(unsigned int*)(it[in].drdata+0)>=10000) it[in].sprite=13212;
	else if (*(unsigned int*)(it[in].drdata+0)>=1000) it[in].sprite=13211;
	else if (*(unsigned int*)(it[in].drdata+0)>=100) it[in].sprite=13210;
	else if (*(unsigned int*)(it[in].drdata+0)>=10) it[in].sprite=13209;
	else it[in].sprite=13208;
	
	sprintf(it[in].description,"%d ounces of %s.",*(unsigned int*)(it[in].drdata),it[in].name);
}

void saltmine_item(int in, int cn)
{
        struct saltmine_worker_data *workerdat;
        struct saltmine_ppd *ppd;
        char *drdata;
        static int laddernumber=0;
        int in2;

        drdata=it[in].drdata;

        if (!cn) {
                // ladder
                if (drdata[0]==1) {
			//xlog("item %d, at %d,%d, nr=%d",in,it[in].x,it[in].y,laddernumber);
                        if (laddernumber<MAXLADDER) drdata[1]=laddernumber++;
                        else xlog("Too many ladders in %s %d!!!",__FILE__,__LINE__);
                }

                // saltbag
                if (drdata[0]==2) {
                        if (saltbag_itemx!=it[in].x || saltbag_itemy!=it[in].y) { xlog("wrong coordinates of saltbag item in %s %d!!!",__FILE__,__LINE__); }
                        saltbag_itemx=it[in].x;
                        saltbag_itemy=it[in].y;
                }

                // door
                if (drdata[0]==3) {
                        if (door_itemx!=it[in].x || door_itemy!=it[in].y) { xlog("wrong coordinates of door item in %s %d!!!",__FILE__,__LINE__); }
                        door_itemx=it[in].x;
                        door_itemy=it[in].y;
                }
        }
        else {
                // ladder
                if (drdata[0]==1) {

                        // player using it
                        if (ch[cn].flags&CF_PLAYER) {

                                // get data
                                if (!(ppd=get_saltmine_ppd(cn))) return;

                                // check if this ladder was already used
                                if (ppd->laddertime[it[in].drdata[1]]+LADDER_REUSE_TIME>time(NULL)) {
                                        log_char(cn,LOG_SYSTEM,0,"Thou already got all the Salt out of this, so thou have to wait until it is refilled again.");
                                        return;
                                }

                                notify_area(it[in].x,it[in].y,NT_NPC,NTID_SALTMINE_USEITEM,cn,in);
                                ppd->useitemflag=1;
                                return;
                        }

                        // worker using it
                        if (ch[cn].driver==CDR_MONK_WORKER) {

                                // get data
                                if (!(workerdat=set_data(cn,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) return;
                                if (!(ppd=get_saltmine_ppd(workerdat->follow_cn))) return;

                                // check if this ladder was already used
                                if (ppd->laddertime[it[in].drdata[1]]+LADDER_REUSE_TIME>time(NULL)) {
                                        log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,10,"%s shakes his head.",ch[cn].name);
                                        workerdat->useitem=0;
                                        return;
                                }

                                // set the time of the ladder
                                ppd->laddertime[it[in].drdata[1]]=time(NULL);

                                // make him invisible, give him the salt
                                ch[cn].flags|=CF_INVISIBLE;

                                workerdat->hassalt=1;
                                workerdat->useitem=0;
                                workerdat->turnvisible=ticker+3*TICKS;
                                ch[cn].tmpx=it[in].x;
                                ch[cn].tmpy=it[in].y;

                                return;
                        }
                }

                // saltbag
                if (drdata[0]==2) {

                        if (ch[cn].driver==CDR_MONK_WORKER) {
                                // get data
                                if (!(workerdat=set_data(cn,DRD_SALTMINE_WORKER,sizeof(struct saltmine_worker_data)))) return;
                                if (!(ppd=get_saltmine_ppd(workerdat->follow_cn))) return;

                                if (workerdat->hassalt) {
                                        ppd->salt++;
                                        ppd->gatamastate=50;
                                        workerdat->hassalt=0;
                                        workerdat->useitem=0;
                                        log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,10,"%s drops his bag of salt.",ch[cn].name);
                                }

                                // set the time of the ladder
                                //ppd->laddertime[it[in].drdata[1]]=time(NULL); !!!!!!!!!!!
                        }

                        if (ch[cn].flags&CF_PLAYER) {
                                if (ch[cn].citem) return;
                                if (!(ppd=get_saltmine_ppd(cn))) return;

                                // if (!ppd->trusttime) { log_char(cn,LOG_SYSTEM,0,"Nothing happens."); return; }
                                if (!ppd->salt) { log_char(cn,LOG_SYSTEM,0,"Thou feelst thou should bring salt to the monastery, before rewarding thinself."); return; }

                                in2=create_item("salt");
                                if (!in2) return;

                                it[in2].value*=ppd->salt*1000;
                                *(unsigned int*)(it[in2].drdata)=ppd->salt*1000;
                                set_salt_data(in2);

                                ppd->salt=0;
                                ppd->gatamastate=0;

                                ch[cn].citem=in2;
                                ch[cn].flags|=CF_ITEMS;
                                it[in2].carried=cn;

                                log_char(cn,LOG_SYSTEM,0,"Thou took %d units of salt, feeling thou have earned it.",*(unsigned int*)(it[in2].drdata));
                        }
                }

                // door
                if (drdata[0]==3) {
                        if (ch[cn].driver==CDR_MONK_WORKER) remove_worker(cn);
                        else log_char(cn,LOG_SYSTEM,0,"Thou canst not enter there.");
                }
        }
}

// -- general -------------------------------------------------------------------------------------------------------

int ch_died_driver(int nr, int cn, int co)
{
	switch(nr) {
                case CDR_MONK_GATAMA:           return 1;
                case CDR_MONK_GOVIDA:           return 1;
                case CDR_MONK_WORKER:           monk_worker_died_driver(cn,co); return 1;
                default: return 0;
	}
}

int ch_respawn_driver(int nr, int cn)
{
	switch(nr) {
                case CDR_MONK_GATAMA:           return 1;
                case CDR_MONK_GOVIDA:           return 1;
                case CDR_MONK_WORKER:           return 1;
		default: return 0;
	}
}

int ch_driver(int nr, int cn, int ret, int lastact)
{
	switch(nr) {
                case CDR_MONK_GATAMA:           monk_gatama_driver(cn,ret,lastact); return 1;
                case CDR_MONK_GOVIDA:           monk_govida_driver(cn,ret,lastact); return 1;
                case CDR_MONK_WORKER:           monk_worker_driver(cn,ret,lastact); return 1;
                default: return 0;
	}
}

int it_driver(int nr, int in, int cn)
{
	switch(nr) {
                case IDR_SALTMINE_ITEM:     saltmine_item(in,cn); return 1;
		default: return 0;
	}
}

