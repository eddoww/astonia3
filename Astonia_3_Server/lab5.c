/*

$Id: lab5.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: lab5.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:17  sam
Added RCS tags


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
#include "sector.h"
#include "lab.h"
#include "book.h"

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

// -- general lab5 stuff --------------------------------------------------------------------------------------------

struct lab5_player_data
{
        unsigned char seyanstate;               // the talk state of the seyan
        unsigned char seyangot;                 // bit1=head1 bit2=head2 bit3=head3

        unsigned char magegot;                  // (the door will set this values, so the mage can say something - not implemented, talking is complex and i was tired)
        unsigned char magestate;                // the talk state of the mage

        unsigned char ritualdaemon;             // the daemon the ritual is for
        unsigned char ritualstate;              // the ritualstate (0=none at all, 1=touched name, 2=touched realname)
};

struct lab5_talk_data
{
        int cv_co;      // current victim
        int cv_serial;  // current victim
        int lasttalk;
};

struct lab5_daemon_data
{
        int attackstart;
        char aggressive,type,dir,dummy;  // 1=master 2=gunned

};

char *daemonname[4]={"xxnamexx","Asfaloth","Beronath","Cyradeth"};
char *daemonreal[4]={"xxrealxx","Fao Thals","Breth Ona","Ch Dae Tyr"};
int namecoordx[4]={ 85,90,85,80 };   // need initialisation for dll reload (it took me a while till i found that "bug")
int namecoordy[4]={ 33,28,23,28 };   // need initialisation for dll reload

#define MAXDOOR 4
int daemondoorx[MAXDOOR]={  119, 119, 119, 119 };    // need initialisation for dll reload
int daemondoory[MAXDOOR]={  108,  95,  82,  69 };    // need initialisation for dll reload

// warning: might kill (and thus delete) cn
void ritual_hurt(int cn, struct lab5_player_data *pd, int x, int y)
{
        int fn;

        fn=create_show_effect(EF_PULSEBACK,cn,ticker,ticker+17,20,42);
        if (fn) {
                ef[fn].x=x;
                ef[fn].y=y;
        }

        log_char(cn,LOG_SYSTEM,0,"°c3The Ritual Of %s ended.°c0",daemonname[pd->ritualdaemon]);
        pd->ritualdaemon=0;
        pd->ritualstate=0;

	// always do hurt last as it might kill the character
	hurt(cn,5*POWERSCALE,0,1,0,0);
}

void ritual_create_char(char *name, int x, int y, int dir, int attackstart)
{
        int cn;
        struct lab5_daemon_data *dat;

        // create
        cn=create_char(name,0);
        if (!cn) return;

        ch[cn].dir=dir;
        ch[cn].flags&=~CF_RESPAWN;
        update_char(cn);

        ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
        ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
        ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

        // drop him to map
        if (!drop_char(cn,x,y,0)) {
		destroy_char(cn);
                /*destroy_items(cn);
                free_char(cn);*/
                xlog("drop_char failed (%s,%d)",__FILE__,__LINE__);
                return;
        }

        // set values of the secure move driver
        ch[cn].tmpx=ch[cn].x;
        ch[cn].tmpy=ch[cn].y;

        // set direction of deamon driver
        dat=set_data(cn,DRD_LAB5_DAEMON,sizeof(struct lab5_daemon_data));
        if (dat) {
                dat->dir=dir;
                dat->attackstart=attackstart*TICKS;
        }

}

int ritual_start(int cn, int daemon)
{
        int i,x=0,y,sx,sy,ex,ey,break2,mn,doorx,doory,co; // x=0 to avoid compiler warning!
        static int statue1[4]={11165,11123,11157,11161};
        static int statue2[4]={11167,11125,11159,11163};

        // check for free room
        for (i=0; i<MAXDOOR; i++) {

                sx=daemondoorx[i]+0;
                sy=daemondoory[i]-6;
                ex=daemondoorx[i]+14;
                ey=daemondoory[i]+6;

                for (break2=0,y=sy; !break2 && y<=ey; y++) {
                        for (x=sx; x<=ex; x++) {
                                if ((co=map[x+y*MAXMAP].ch) && (ch[co].flags&CF_PLAYER)) { break2=1; break; }
                        }
                }

                if (y==ey+1 && x==ex+1) break;
        }

        if (i==MAXDOOR) return 0;

        // cleanup
        for (y=sy; y<=ey; y++) {
                for (x=sx; x<=ex; x++) {
                        mn=x+y*MAXMAP;
                        if ((co=map[mn].ch) && !(ch[co].flags&CF_PLAYER)) remove_destroy_char(co);
                }
        }

        // create new room
        doorx=daemondoorx[i];
        doory=daemondoory[i];

        map[(doorx+2)+(doory-2)*MAXMAP].fsprite=statue1[daemon]; set_sector(doorx+2,doory-2); // set sector ?
        map[(doorx+2)+(doory+2)*MAXMAP].fsprite=statue1[daemon]; set_sector(doorx+2,doory+2);
        map[(doorx+12)+(doory-2)*MAXMAP].fsprite=statue2[daemon]; set_sector(doorx+12,doory-2);
        map[(doorx+12)+(doory+2)*MAXMAP].fsprite=statue2[daemon]; set_sector(doorx+12,doory+2);

        if (daemon==1) {
                ritual_create_char("lab5_one_servant",doorx+10,doory-2,DX_LEFT,7);
                ritual_create_char("lab5_one_servant",doorx+10,doory+2,DX_LEFT,7);
                ritual_create_char("lab5_one_master",doorx+12,doory+0,DX_LEFT,8);
        }
        else if (daemon==2) {
                ritual_create_char("lab5_two_servant",doorx+10,doory-2,DX_LEFT,7);
                ritual_create_char("lab5_two_servant",doorx+10,doory+2,DX_LEFT,7);
                ritual_create_char("lab5_two_master",doorx+12,doory+0,DX_LEFT,8);
        }
        else if (daemon==3) {
                ritual_create_char("lab5_three_servant_mage",doorx+6,doory-4,DX_DOWN,7);
                ritual_create_char("lab5_three_servant_mage",doorx+6,doory+4,DX_UP,7);
                ritual_create_char("lab5_three_servant",doorx+12,doory+1,DX_LEFT,8);
                ritual_create_char("lab5_three_master",doorx+12,doory-1,DX_LEFT,8);
        }

        // insert character
        if (teleport_char_driver(cn,doorx+1,doory)) return 1;
        return 0;
}

int has_potion(int cn)
{
        int i,in;

        for (i=30; i<INVENTORYSIZE; i++) {
                if ((in=ch[cn].item[i]) && it[in].driver==IDR_POTION) return 1;
        }

        if ((in=ch[cn].citem) && it[in].driver==IDR_POTION) return 1;

        return 0;
}

// -- seyan driver --------------------------------------------------------------------------------------------------

void set_seyan_state(struct lab5_player_data *pd)
{
        if ((pd->seyangot&(1<<0)) && (pd->seyangot&(1<<1)) && (pd->seyangot&(1<<2))) pd->seyanstate=20;     // done
        else if (pd->seyangot) pd->seyanstate=10; // some
        else pd->seyanstate=0; // start
}

void lab5_seyan_driver(int cn, int ret, int lastact)
{
	struct lab5_player_data *pd;
        static struct lab5_talk_data datbuf;   // we only have one, so there is no need to use the memory system (?)
        struct lab5_talk_data *dat=&datbuf;
        struct msg *msg,*next;
        int co;
        int talkdir=0,didsay=0;
        char *str;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_GIVE) {
                        if (!ch[cn].citem) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source
                        co=msg->dat1;

                        // players only
                        if (ch[co].flags&CF_PLAYER) {

                                // get lab ppd
                                pd=set_data(co,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                                if (!pd) { remove_message(cn,msg); continue; }

                                // check item
                                if (it[ch[cn].citem].ID==IID_LAB5_HEAD1) {
                                        pd->seyangot|=(1<<0);
                                        set_seyan_state(pd);
                                        if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while I'm talking to others.",ch[co].name); }
                                }
                                if (it[ch[cn].citem].ID==IID_LAB5_HEAD2) {
                                        pd->seyangot|=(1<<1);
                                        set_seyan_state(pd);
                                        if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while I'm talking to others.",ch[co].name); }
                                }
                                if (it[ch[cn].citem].ID==IID_LAB5_HEAD3) {
                                        pd->seyangot|=(1<<2);
                                        set_seyan_state(pd);
                                        if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while I'm talking to others.",ch[co].name); }
                                }
                        }

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_CHAR) {

                        co=msg->dat1;

                        // dont talk
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }            // dont talk to other NPCs
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }           // dont talk to players without connection
			if (ticker<dat->lasttalk+5*TICKS) { remove_message(cn,msg); continue; }              // only talk when the old sentence is read
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }      // dont talk to someone we cant see, and dont talk to ourself
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }                  // dont talk to someone far away

                        // remove cv
                        if (dat->cv_co) {
                                if (!ch[dat->cv_co].flags || ch[dat->cv_co].serial!=dat->cv_serial || char_dist(cn,dat->cv_co)>10 || !char_see_char(cn,dat->cv_co)) {
                                        dat->cv_co=0;
                                }
                        }

                        // only talk to cv
                        if (dat->cv_co && dat->cv_co!=co) { remove_message(cn,msg); continue; }

                        // set new cv
                        if (!dat->cv_co) {
                                dat->cv_co=co;
                                dat->cv_serial=ch[co].serial;
                        }

                        // get lab ppd
                        pd=set_data(co,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) { remove_message(cn,msg); continue; }

			switch(pd->seyanstate) {
                                // INTRO
                                case 0: say(cn,"Hello %s. I am here to introduce thee to the quest that has to be done here.",ch[co].name); didsay=1; pd->seyanstate++; break;
                                case 1: say(cn,"There are three Demons controlling this Labyrinth. Your mission is extremely simple: Destroy them. To prove their death, bring me their heads. Then thou art worthy to enter the next Gate."); didsay=1; pd->seyanstate++; break;
                                case 2: say(cn,"But I have to tell thee, that thou shouldst not carry any healing or mana potions, nor a combo potion with thee when entering here. If thou hast some, please deposit them in thine depot at the Gatekeeper's."); didsay=1; pd->seyanstate++; break;
                                case 3: if (has_potion(co)) { dat->cv_co=0; break; }
                                        say(cn,"Go ahead now, %s, and fulfil thine destiny.",ch[co].name); didsay=1; pd->seyanstate++; break;
                                case 4: say(cn,"Ah, and %s, thou mightst find a friend of mine here. Listen carefully to his advice.",ch[co].name); didsay=1; pd->seyanstate++; break;
                                case 5: dat->cv_co=0; break;

                                // received something
                                case 10: if (pd->seyangot==1 || pd->seyangot==2 || pd->seyangot==4) say(cn,"Very well done, %s.",ch[co].name);
                                         if (pd->seyangot==3 || pd->seyangot==5 || pd->seyangot==6) say(cn,"I'm impressed, %s.",ch[co].name);
                                         didsay=1; pd->seyanstate++; break;
                                case 11: dat->cv_co=0; break;
			
                                // received all, open gate
                                case 20: say(cn,"%s, thou broughtst me the three Demon's heads and proved thine worth.",ch[co].name); didsay=1; pd->seyanstate++; break;
                                case 21: say(cn,"Now I will open a magic gate for thee. Use it, and thou wilt be able to travel to the next part of the Labyrinth."); didsay=1; pd->seyanstate++; break;
                                case 22: create_lab_exit(co,15); say(cn,"Mayest thou pass the last gate, %s",ch[co].name); didsay=1; pd->seyanstate++; break;
                                case 23: dat->cv_co=0; break;
                        }

                        if (didsay) {
                                dat->lasttalk=ticker;
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

                        // get lab ppd
                        pd=set_data(co,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) { remove_message(cn,msg); continue; }

                        if (strcasestr(str,"REPEAT")) { say(cn,"I will repeat, %s",ch[co].name); set_seyan_state(pd); }
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        if (talkdir) turn(cn,talkdir);

        if (dat->lasttalk+TICKS*30<ticker) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHTDOWN,ret,lastact)) return;		
        }

        do_idle(cn,TICKS);
}

// -- mage driver ---------------------------------------------------------------------------------------------------

void lab5_mage_driver(int cn, int ret, int lastact)
{
	struct lab5_player_data *pd;
        static struct lab5_talk_data datbuf;   // we only have one, so there is no need to use the memory system (?)
        struct lab5_talk_data *dat=&datbuf;
        struct msg *msg,*next;
        int co;
        int talkdir=0,didsay=0;
        char *str;
        int called;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        namecoordx[0]=ch[cn].x;
                        namecoordy[0]=ch[cn].y;
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
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }            // dont talk to other NPCs
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }           // dont talk to players without connection
			if (ticker<dat->lasttalk+5*TICKS) { remove_message(cn,msg); continue; }              // only talk when the old sentence is read
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }      // dont talk to someone we cant see, and dont talk to ourself
			if (char_dist(cn,co)>7) { remove_message(cn,msg); continue; }                  // dont talk to someone far away

                        // remove cv
                        if (dat->cv_co) {
                                if (!ch[dat->cv_co].flags || ch[dat->cv_co].serial!=dat->cv_serial || char_dist(cn,dat->cv_co)>7 || !char_see_char(cn,dat->cv_co)) {
                                        dat->cv_co=0;
                                }
                        }

                        // only talk to cv
                        if (dat->cv_co && dat->cv_co!=co) { remove_message(cn,msg); continue; }

                        // set new cv
                        if (!dat->cv_co) {
                                dat->cv_co=co;
                                dat->cv_serial=ch[co].serial;
                        }

                        // get lab ppd
                        pd=set_data(co,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) { remove_message(cn,msg); continue; }

			switch(pd->magestate) {
                                // INTRO
                                case 0: say(cn,"Hello %s. My name is Mathor, and I am the friend Laros surely mentioned.",ch[co].name); didsay=1; pd->magestate++; break;
                                case 1: say(cn,"It is the entrance room to the Master °c4Demons°c0 what thou see here. Those stone plates show their names. But thou have to find their real names written on similar plates somewhere behind those doors here."); didsay=1; pd->magestate++; break;
                                case 2: say(cn,"Once thou foundst the real name of a Master Demon, Thou can °c4force°c0 him to summon thee into his place, and fight him there. Thou might ask me for more details, if thou art interested."); didsay=1; pd->magestate++; break;
                                case 3: say(cn,"And thou shouldst be!"); didsay=1; pd->magestate++; break;
                                case 4: dat->cv_co=0; break;
                                // FORCE
                                case 10: say(cn,"Well %s. To force a Master °c4Demon°c0 to summon thee into his place thou have to perform a certain °c4ritual°c0 first. But be very careful, %s. If thou makest only one mistake it might kill thee. The powers that are working here are strong.",ch[co].name,ch[co].name); didsay=1; pd->magestate++; break;
                                case 11: say(cn,"Very strong indeed!"); didsay=1; pd->magestate++; break;
                                case 12: dat->cv_co=0; break;
                                // DAEMONs
                                case 20: say(cn,"Well %s, unfortunetaly those Master Demons can't be hurt by normal weapons. So make sure thou art properly equipped with a sacred stone weapon when fighting the Masters.",ch[co].name); didsay=1; pd->magestate++; break;
                                case 21: say(cn,"I have heard that those weapon might be found somewhere in the section behind the south western door of this room."); didsay=1; pd->magestate++; break;
                                case 22: dat->cv_co=0; break;
                                // RITUAL
                                case 30: say(cn,"Oh %s, it's a ritual of mighty powers thou art asking for. So listen carefully.",ch[co].name); didsay=1; pd->magestate++; break;
                                case 31: say(cn,"First, thou hast to touch the stone plate of the Demons name."); didsay=1; pd->magestate++; break;
                                case 32: say(cn,"Second, thou hast to touch the correct stone plate of the Demons real name. Those thou hast to find."); didsay=1; pd->magestate++; break;
                                case 33: say(cn,"Third, thou hast to enter the inner square from the opposite entrance."); didsay=1; pd->magestate++; break;
                                case 34: say(cn,"Then place thineself in the center, and shout the real name of the Master."); didsay=1; pd->magestate++; break;
                                case 35: say(cn,"Well, thats it. Prepare to fight him then."); didsay=1; pd->magestate++; break;
                                case 36: say(cn,"Ah, and thou couldst do it in any order, but may I suggest doing them from the east to the west."); didsay=1; pd->magestate++; break;
                                case 37: dat->cv_co=0; break;
                        }

                        if (didsay) {
                                dat->lasttalk=ticker;
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

                        // get lab ppd
                        pd=set_data(co,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) { remove_message(cn,msg); continue; }

                        if (strcasestr(str,"REPEAT")) { pd->magestate=0; say(cn,"I will repeat, %s",ch[co].name); }
                        else if (strcasestr(str,"FORCE")) { pd->magestate=10; if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while i'm talking to others.",ch[co].name); } }
                        else if (strcasestr(str,"DEMON")) { pd->magestate=20; if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while i'm talking to others.",ch[co].name); } }
                        else if (strcasestr(str,"DEMONS")) { pd->magestate=20; if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while i'm talking to others.",ch[co].name); } }
                        else if (strcasestr(str,"RITUAL")) { pd->magestate=30; if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while i'm talking to others.",ch[co].name); } }
                        else if (ch[co].flags&CF_GOD) {
                                if (strcasestr(str,"SET 1")) { pd->ritualdaemon=1; pd->ritualstate=3;  say(cn,"set %d %d (%s)",pd->ritualdaemon,pd->ritualstate,daemonreal[pd->ritualdaemon]); }
                                else if (strcasestr(str,"SET 2")) { pd->ritualdaemon=2; pd->ritualstate=3;  say(cn,"set %d %d (%s)",pd->ritualdaemon,pd->ritualstate,daemonreal[pd->ritualdaemon]); }
                                else if (strcasestr(str,"SET 3")) { pd->ritualdaemon=3; pd->ritualstate=3;  say(cn,"set %d %d (%s)",pd->ritualdaemon,pd->ritualstate,daemonreal[pd->ritualdaemon]); }
                        }

                        // inside square
                        if (pd->ritualstate && ch[co].x>namecoordx[3]+2 && ch[co].x<namecoordx[1]-2 && ch[co].y>namecoordy[2]+2 && ch[co].y<namecoordy[0]-2 && strcasestr(str,":")) {

                                called=0;

                                if (strcasestr(str,"shouts:")) {
                                        if (strcasestr(str,daemonreal[1])) called=1;
                                        else if (strcasestr(str,daemonreal[2])) called=2;
                                        else if (strcasestr(str,daemonreal[3])) called=3;
                                }
                                else if ((ch[co].flags&CF_GOD) && strcasestr(str,"SET")) called=pd->ritualdaemon;

                                say(cn,"%d %d %d",pd->ritualdaemon,called,pd->ritualstate);

                                if (pd->ritualstate==3 && pd->ritualdaemon==called && ch[co].x==namecoordx[2] && ch[co].y==namecoordy[1]) {

                                        if (ritual_start(co,pd->ritualdaemon)) {
                                                sound_area(ch[cn].x,ch[cn].y,41);
                                                log_char(co,LOG_SYSTEM,0,"°c3The Ritual of %s is fulfilled.°c0",daemonreal[pd->ritualdaemon]);
                                                pd->ritualstate=0;
                                        }
                                        else {
                                                log_char(co,LOG_SYSTEM,0,"°c3Thou have to call again, but wait a while to do so!°c0");
                                                ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
                                        }
                                }
                                else ritual_hurt(co,pd,namecoordx[pd->ritualdaemon],namecoordy[pd->ritualdaemon]);
                        }
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        if (talkdir) turn(cn,talkdir);

        if (dat->lasttalk+TICKS*30<ticker) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;		
        }

        do_idle(cn,TICKS);
}

// -- daemon driver -------------------------------------------------------------------------------------------------

void lab5_daemon_driver_parse(int cn, struct lab5_daemon_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {
                if (!strcmp(name,"type")) dat->type=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}

        if (dat->type==1) { // master
                dat->attackstart+=ticker;
        }
        else if (dat->type==2) { // gunned
                dat->dir=DX_LEFT;
                dat->attackstart=2147483647;
        }
        else {
                dat->attackstart+=ticker;
        }
}

void lab5_daemon_driver(int cn, int ret, int lastact)
{
	struct msg *msg,*next;
        int in,co,imm=-1;
        struct lab5_daemon_data *dat;

        dat=set_data(cn,DRD_LAB5_DAEMON,sizeof(struct lab5_daemon_data));
        if (!dat) return;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        lab5_daemon_driver_parse(cn,dat);
                }

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                if (msg->type==NT_CHAR) {
                        co=msg->dat1;
                        if (dat->type==1) { // master
                                if (ch[co].flags&CF_PLAYER && char_see_char(cn,co)) {
                                        if ((in=ch[co].item[WN_RHAND])==0 || it[in].ID!=IID_LAB5_WEAPON) imm=1; else if (imm==-1) imm=0;
                                }
                        }
                        else imm=0;

                        if (dat->type==2) { // gunned
                                if (ch[co].flags&CF_PLAYER && ch[co].y<namecoordy[0]+25 && char_see_char(cn,co)) fight_driver_add_enemy(cn,co,1,1); // dat->aggressive=1;
                        }
                }

                standard_message_driver(cn,msg,dat->aggressive,1);
                remove_message(cn,msg);
	}

        // switch to attack
        if (dat->aggressive==0 && ticker>dat->attackstart) dat->aggressive=1;

        // immortal switch
        if (imm==1) ch[cn].flags|=CF_IMMORTAL; else if (imm==0) ch[cn].flags&=~CF_IMMORTAL;

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        // rest of standard action
	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;
        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;

        // nothing left to do
        do_idle(cn,TICKS/2);
}



// -- item driver ---------------------------------------------------------------------------------------------------

// drdata[0]=type               1=obelisk
//                              2=fireface
//                              3=chestbox
//                              4=combopotion
//                              5=nameplate
//                              6=realnameplate
//                              7=entrance
//                              8=backdoor
//                              9=gun
//                              10=pike
//                              11=no potion door
//                              12=manapotion
//                              13=lightface

// fireface                     drdata[1]=time init
// chestbox                     drdata[1]=content
//                              drdata[2]=chestbox number
//                              drdata[3]=open/closed
// nameplate                    drdata[1]=daemon
// realnameplate                drdata[1]=daemon
// entrance                     drdata[1]=daemon
// gun                          drdata[1]=state
// pike                         drdata[1]=state
// lightface                    drdata[1]=time init
// lightface                    drdata[2]=interval


int numchestboxes=0;
int sizeofchestboxes=0;

void count_chestboxes(void)
{
        int x,y,in;

        for (y=0; y<MAXMAP; y++) {
                for (x=0; x<MAXMAP; x++) {
                        if (!(in=map[x+y*MAXMAP].it)) continue;
                        if (it[in].driver!=IDR_LAB5_ITEM || it[in].drdata[0]!=3) continue;
                        it[in].drdata[2]=numchestboxes++;
                }
        }

        if (numchestboxes>255) { xlog("too many chestboxes in lab5."); }

        sizeofchestboxes=(numchestboxes+7)/8;

        xlog("found %d chests in lab5 (%d bytes)",numchestboxes,sizeofchestboxes);
}

int check_chestbox(int cn, int in)
{
        unsigned char *dat;
        int nr,byte,bit,ret;

        dat=set_data(cn,DRD_LAB5_CHESTBOX,sizeofchestboxes);
        if (!dat) return 1;

        nr=it[in].drdata[2];

        if (nr>=numchestboxes) { xlog("take more care of chestboxes in %s %d!!!",__FILE__,__LINE__); return 1; }

        byte=nr/8;
        bit=nr%8;

        ret=dat[byte]&(1<<bit);
        dat[byte]|=(1<<bit);

        return ret;
}

#define GUNRELOAD       (2*TICKS/3)

void lab5_item(int in, int cn)
{
        struct lab5_player_data *pd;
        char *drdata,dx,dy;
        int in2;

        drdata=it[in].drdata;

        if (!numchestboxes) count_chestboxes();

        if (!cn) {

                // fireface
                if (drdata[0]==2) {
                        if (it[in].sprite==11135) { dx=1; dy=0; }
                        else if (it[in].sprite==11136) { dx=0; dy=-1; }
                        else if (it[in].sprite==11137) { dx=-1; dy=0; }
                        else /*if (it[in].sprite==11138)*/ { dx=0; dy=1; }
                        create_fireball(0,it[in].x+dx,it[in].y+dy,it[in].x+dx+dx,it[in].y+dy+dy,50);

                        if (drdata[1]==0) {
                                drdata[1]=1;
                                call_item(it[in].driver,in,0,ticker+(((it[in].x+it[in].y)%17)+1)*TICKS);
                        }
                        else call_item(it[in].driver,in,0,ticker+5*TICKS);
                }

                // lightface
                if (drdata[0]==13) {
                        if (it[in].sprite==11135) { dx=1; dy=0; }
                        else if (it[in].sprite==11136) { dx=0; dy=-1; }
                        else if (it[in].sprite==11137) { dx=-1; dy=0; }
                        else /*if (it[in].sprite==11138)*/ { dx=0; dy=1; }
                        create_ball(0,it[in].x+dx,it[in].y+dy,it[in].x+dx+dx,it[in].y+dy+dy,40);

                        if (drdata[1]==0) {
                                drdata[1]=1;
                                call_item(it[in].driver,in,0,ticker+(((it[in].x+it[in].y)%10)+1)*TICKS);
                        }
                        else if (drdata[2]==4) {
                                call_item(it[in].driver,in,0,ticker+9*TICKS);
                                drdata[2]=0;
                        }
                        else {
                                call_item(it[in].driver,in,0,ticker+7*TICKS/4);
                                drdata[2]++;
                        }
                }

                // chestbox
                if (drdata[0]==3) {
                        if (!drdata[3]) return;
                        drdata[3]=0;
                        it[in].sprite--;
                        set_sector(it[in].x,it[in].y);
                }

                // nameplate
                if (drdata[0]==5) {
                        namecoordx[(int)drdata[1]]=it[in].x;
                        namecoordy[(int)drdata[1]]=it[in].y;
                }

                // entrance
                if (drdata[0]==7) {
                        it[in].sprite=0;
                        set_sector(it[in].x,it[in].y);
                }

                // backdoor
                if (drdata[0]==8) {
                        static int bcnt=0;
                        daemondoorx[bcnt]=it[in].x;
                        daemondoory[bcnt]=it[in].y;
                        bcnt++;
                }

                // gun
                if (drdata[0]==9) {
                        if (!drdata[1]) return;
                        drdata[1]--;
                        it[in].sprite--;
                        set_sector(it[in].x,it[in].y);
                        if (drdata[1]) call_item(it[in].driver,in,0,ticker+GUNRELOAD);
                }

                // pike
                if (drdata[0]==10) {
                        if (!drdata[1]) return;
                        drdata[1]=0;
                        it[in].sprite--;
                        set_sector(it[in].x,it[in].y);
                }

        }
        else {

                // obelisk
                if (drdata[0]==1) {
                        ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
                        ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
                        ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
                        ch[cn].lifeshield=get_lifeshield_max(cn)*POWERSCALE;
                        sound_area(ch[cn].x,ch[cn].y,41);
                }

                // chestbox
                if (drdata[0]==3) {

                        char *str;

                        if (drdata[3]) return;
                        if (ch[cn].citem) return;

                        // check if it was already opened
                        if (check_chestbox(cn,in)) { log_char(cn,LOG_SYSTEM,0,"Thou canst not open the chest again."); return; }

                        // open box (and call close timer)
                        drdata[3]=1;
                        it[in].sprite++;
                        set_sector(it[in].x,it[in].y);
                        call_item(it[in].driver,in,0,ticker+2*TICKS);

                        // which item
                        switch(drdata[1]) {
                                case 1: str="lab5_combopotion"; break;
                                case 2: str="lab5_staff"; break;
                                case 3: str="lab5_dagger"; break;
                                case 4: str="lab5_sword"; break;
                                case 5: str="lab5_twohanded"; break;
                                case 6: str="lab5_manapotion"; break;
                                case 7: str="lab5_manslayer"; break;
                                default: str="oops"; break;
                        }

                        // create and give potion to player
                        in2=create_item(str);
                        log_char(cn,LOG_SYSTEM,0,"You received a %s.",it[in2].name);

                        if (!in2) { xlog("failed to create '%s' item in %s %d",str,__FILE__,__LINE__); return; }

                        ch[cn].citem=in2;
                        ch[cn].flags|=CF_ITEMS;
                        it[in2].carried=cn;
                }

                // combopotion
                if (drdata[0]==4) {
                        ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
                        ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
                        ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
                        if (ch[cn].value[1][V_MAGICSHIELD]) ch[cn].lifeshield=get_lifeshield_max(cn)*POWERSCALE;
                        log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s drinks a potion.",ch[cn].name);
                        remove_item(in);
                        free_item(in);
                }

                // manapotion
                if (drdata[0]==12) {
                        // ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
                        ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
                        // ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
                        if (ch[cn].value[1][V_MAGICSHIELD]) ch[cn].lifeshield=get_lifeshield_max(cn)*POWERSCALE;
                        log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s drinks a potion.",ch[cn].name);
                        remove_item(in);
                        free_item(in);
                }

                // nameplate
                if (drdata[0]==5) {

                        // get pd
                        pd=set_data(cn,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) return;

                        if (pd->ritualstate==0) {
                                sound_area(ch[cn].x,ch[cn].y,41);
                                pd->ritualdaemon=drdata[1];
                                pd->ritualstate=1;
                                log_char(cn,LOG_SYSTEM,0,"Thou canst read the symbols now. They form the words:");
                                log_char(cn,LOG_SYSTEM,0,"°c3The Ritual of %s started.°c0",daemonname[pd->ritualdaemon]);
                        }
                        else ritual_hurt(cn,pd,it[in].x,it[in].y);

                }

                // realnameplate
                if (drdata[0]==6) {

                        // get pd
                        pd=set_data(cn,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) return;

                        if (pd->ritualstate==0) { log_char(cn,LOG_SYSTEM,0,"Nothing happens."); return; }

                        if (pd->ritualstate==1 && pd->ritualdaemon==drdata[1]) {
                                sound_area(ch[cn].x,ch[cn].y,41);
                                pd->ritualstate=2;
                                log_char(cn,LOG_SYSTEM,0,"Thou canst read the symbols now. They form the words:");
                                log_char(cn,LOG_SYSTEM,0,"°c3The ritual of %s is the Ritual of %s.°c0",daemonname[pd->ritualdaemon],daemonreal[pd->ritualdaemon]);
                        }
                        else ritual_hurt(cn,pd,it[in].x,it[in].y);

                }

                // entrance
                if (drdata[0]==7) {

                        static int hurttrans[4]={2,3,0,1};

                        // get pd
                        pd=set_data(cn,DRD_LAB5_PLAYER,sizeof(struct lab5_player_data));
                        if (!pd) return;

                        if (pd->ritualstate==0) return;

                        if (pd->ritualstate==2 && pd->ritualdaemon==drdata[1]){
                                sound_area(ch[cn].x,ch[cn].y,41);
                                pd->ritualdaemon=drdata[1];
                                pd->ritualstate=3;
                                log_char(cn,LOG_SYSTEM,0,"Mathor tells you: \"The ritual continues. Well done so far, %s.\"",ch[cn].name);
                                // log_char(cn,LOG_SYSTEM,0,"°c3The Ritual of %s continues.°c0",daemonname[pd->ritualdaemon]);
                        }
                        else {
				if (drdata[1]==2) log_char(cn,LOG_SYSTEM,0,"Mathor tells you: \"Sorry. But a strange power forced me.\"");
                                ritual_hurt(cn,pd,namecoordx[hurttrans[(int)drdata[1]]],namecoordy[hurttrans[(int)drdata[1]]]);
                        }

                }

                // backdoor
                if (drdata[0]==8) {
                        // watch the syntax!
                        if (!teleport_char_driver(cn,namecoordx[2],namecoordy[1]))
                                if (!teleport_char_driver(cn,namecoordx[0],namecoordy[0]))
                                if (!teleport_char_driver(cn,namecoordx[1],namecoordy[1]))
                                if (!teleport_char_driver(cn,namecoordx[2],namecoordy[2]))
                                teleport_char_driver(cn,namecoordx[3],namecoordy[3]);
                }

                // gun
                if (drdata[0]==9) {
                        if (drdata[1]) { log_char(cn,LOG_SYSTEM,0,"Thou canst not push the lever."); return; }
                        drdata[1]=7;
                        it[in].sprite+=7;
                        set_sector(it[in].x,it[in].y);
                        call_item(it[in].driver,in,0,ticker+GUNRELOAD);

                        create_fireball(cn,it[in].x+2,it[in].y,it[in].x+60,it[in].y,100);
                }

                // pike
                if (drdata[0]==10) {
                        hurt(cn,5*POWERSCALE,0,1,0,0);
                        if (drdata[1]) return;
                        drdata[1]++;
                        it[in].sprite++;
                        set_sector(it[in].x,it[in].y);
                        call_item(it[in].driver,in,0,ticker+5*TICKS);
                }

                // no potion door
                if (drdata[0]==11) {
                        if (ch[cn].x<it[in].x && has_potion(cn) ) {
                                log_char(cn,LOG_SYSTEM,0,"°c3Thou canst not enter carrying a mana, healing or combo potion!°c0");
                                return;
                        }

                        if (ch[cn].x<it[in].x) teleport_char_driver(cn,it[in].x-9,it[in].y-7);
                        else teleport_char_driver(cn,it[in].x+9,it[in].y+7);
                }
        }

}

// -- general -------------------------------------------------------------------------------------------------------

int ch_died_driver(int nr, int cn, int co)
{
	switch(nr) {
                case CDR_LAB5DAEMON:            return 1;
                case CDR_LAB5SEYAN:             return 1;
                case CDR_LAB5MAGE:              return 1;
                default: return 0;
	}
}

int ch_respawn_driver(int nr, int cn)
{
	switch(nr) {
                case CDR_LAB5DAEMON:            return 1;
                case CDR_LAB5SEYAN:             return 1;
                case CDR_LAB5MAGE:              return 1;
		default: return 0;
	}
}

int ch_driver(int nr, int cn, int ret, int lastact)
{
	switch(nr) {
                case CDR_LAB5DAEMON:            lab5_daemon_driver(cn,ret,lastact); return 1;
                case CDR_LAB5SEYAN:             lab5_seyan_driver(cn,ret,lastact); return 1;
                case CDR_LAB5MAGE:              lab5_mage_driver(cn,ret,lastact); return 1;
                default: return 0;
	}
}

int it_driver(int nr, int in, int cn)
{
	switch(nr) {
                case IDR_LAB5_ITEM:     lab5_item(in,cn); return 1;
		default: return 0;
	}
}

