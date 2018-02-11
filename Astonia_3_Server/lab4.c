/*

$Id: lab4.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: lab4.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2003/10/19 10:29:31  sam
fixed typo in seyan driver

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

// -- seyan driver --------------------------------------------------------------------------------------------------

struct lab4_player_data
{
        unsigned char seyan4state;              // the talk state of the seyan
        unsigned char seyan4got;                // bit1=crown bit2=szepter
};

struct lab4_seyan_data
{
        int cv_co;      // current victim
        int cv_serial;  // current victim
        int lasttalk;
};


void set_seyan_state(struct lab4_player_data *pd)
{
        if ((pd->seyan4got&(1<<0)) && (pd->seyan4got&(1<<1))) pd->seyan4state=30;
        else if ((pd->seyan4got&(1<<0))) pd->seyan4state=10;
        else if ((pd->seyan4got&(1<<1))) pd->seyan4state=20;
        else pd->seyan4state=0;
}

void lab4_seyan_driver(int cn, int ret, int lastact)
{
	struct lab4_player_data *pd;
        static struct lab4_seyan_data datbuf;   // we only have one, so there is no need to use the memory system (?)
        struct lab4_seyan_data *dat=&datbuf;
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
                                pd=set_data(co,DRD_LAB4_PLAYER,sizeof(struct lab4_player_data));
                                if (!pd) { remove_message(cn,msg); continue; }

                                // check item
                                if (it[ch[cn].citem].ID==IID_LAB4_CROWN) {
                                        pd->seyan4got|=(1<<0);
                                        set_seyan_state(pd);
                                        if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while i'm talking to others.",ch[co].name); }
                                }
                                if (it[ch[cn].citem].ID==IID_LAB4_SZEPTER) {
                                        pd->seyan4got|=(1<<1);
                                        set_seyan_state(pd);
                                        if (dat->cv_co && (dat->cv_co!=co || ch[dat->cv_co].serial!=dat->cv_serial)) { say(cn,"%s, please be patient while i'm talking to others.",ch[co].name); }
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
                        pd=set_data(co,DRD_LAB4_PLAYER,sizeof(struct lab4_player_data));
                        if (!pd) { remove_message(cn,msg); continue; }

			switch(pd->seyan4state) {
                                // INTRO
                                case 0: say(cn,"Hello %s. This is thy first mission in the Labyrinth.",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 1: say(cn,"Listen, %s. To the east is an entrance to the Gnalbs winter residence.",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 2: say(cn,"The Gnalbs are peaceful creatures, but their King and their Mage and the Guards are not."); didsay=1; pd->seyan4state++; break;
                                case 3: say(cn,"Bring me the King's Crown, and the Mage's Szepter to prove thou art worthy to enter the next Gate."); didsay=1; pd->seyan4state++; break;
                                case 4: say(cn,"Go ahead now, %s, and fulfil thine destiny.",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 5: dat->cv_co=0; break;

                                // received crown (szepter missing)
                                case 10: say(cn,"Thou broughtst me the Kings Crown. Now, %s, seek for the Mage's Szepter.",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 11: dat->cv_co=0; break;
			
                                // received szepter (corwn missing)
                                case 20: say(cn,"Thou broughtst me the Mages Szepter. Now, %s, seek for the King's Crown.",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 21: dat->cv_co=0; break;

                                // received both, opening gate
                                case 30: say(cn,"%s, thou broughtst me the King's Crown and the Mage's Szepter.",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 31: say(cn,"Now I will open a magic gate for thee. Use it, and thou wilt be able to travel to the next part of the Labyrinth."); didsay=1; pd->seyan4state++; break;
                                case 32: create_lab_exit(co,10); say(cn,"Mayest Thou Past The Last Gate, %s",ch[co].name); didsay=1; pd->seyan4state++; break;
                                case 33: dat->cv_co=0; break;
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
                        pd=set_data(co,DRD_LAB4_PLAYER,sizeof(struct lab4_player_data));
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

// -- gnalb driver --------------------------------------------------------------------------------------------------

struct gnalb_path
{
        int x;
        int y;
        int next[4];
};

struct gnalb_path gnalb_path[]=
{
        { 0, 0, { 0, 0, 0, 0 }}, // 0

        { 52, 228, { 45, 2, 0, 0 }}, // 1
        { 56, 231, { 1, 3, 0, 0 }}, // 2
        { 61, 233, { 2, 4, 0, 0 }}, // 3
        { 65, 234, { 3, 5, 0, 0 }}, // 4
        { 68, 236, { 4, 6, 0, 0 }}, // 5
        { 73, 237, { 5, 7, 0, 0 }}, // 6
        { 77, 238, { 6, 8, 0, 0 }}, // 7
        { 81, 239, { 7, 9, 0, 0 }}, // 8
        { 83, 239, { 8, 10, 0, 0 }}, // 9
        { 88, 238, { 9, 11, 0, 0 }}, // 10
        { 89, 236, { 10, 12, 0, 0 }}, // 11
        { 90, 233, { 11, 13, 0, 0 }}, // 12
        { 90, 230, { 12, 14, 0, 0 }}, // 13
        { 89, 227, { 13, 15, 0, 0 }}, // 14
        { 90, 225, { 14, 16, 0, 0 }}, // 15
        { 92, 223, { 15, 17, 47, 0 }}, // 16 -- cross 2 --
        { 92, 220, { 16, 18, 0, 0 }}, // 17
        { 93, 217, { 17, 19, 0, 0 }}, // 18
        { 92, 213, { 18, 20, 0, 0 }}, // 19
        { 89, 209, { 19, 21, 0, 0 }}, // 20
        { 87, 206, { 20, 22, 0, 0 }}, // 21
        { 86, 203, { 21, 23, 0, 0 }}, // 22
        { 86, 201, { 22, 24, 63, 0 }}, // 23 -- cross 1 --
        { 82, 200, { 23, 25, 0, 0 }}, // 24
        { 78, 199, { 24, 26, 0, 0 }}, // 25
        { 74, 197, { 25, 27, 0, 0 }}, // 26
        { 71, 197, { 26, 28, 0, 0 }}, // 27
        { 69, 199, { 27, 29, 0, 0 }}, // 28
        { 67, 201, { 28, 30, 0, 0 }}, // 29
        { 66, 205, { 29, 31, 0, 0 }}, // 30
        { 67, 207, { 30, 32, 0, 0 }}, // 31
        { 67, 210, { 31, 33, 0, 0 }}, // 32
        { 68, 213, { 32, 34, 0, 0 }}, // 33
        { 67, 216, { 33, 35, 0, 0 }}, // 34
        { 67, 218, { 34, 36, 0, 0 }}, // 35
        { 65, 220, { 35, 37, 0, 0 }}, // 36
        { 62, 218, { 36, 38, 0, 0 }}, // 37
        { 58, 215, { 37, 39, 0, 0 }}, // 38
        { 54, 213, { 38, 40, 0, 0 }}, // 39
        { 51, 213, { 39, 41, 0, 0 }}, // 40
        { 49, 214, { 40, 42, 0, 0 }}, // 41
        { 48, 217, { 41, 43, 0, 0 }}, // 42
        { 48, 219, { 42, 44, 0, 0 }}, // 43
        { 49, 222, { 43, 45, 0, 0 }}, // 44
        { 50, 225, { 44,  1, 0, 0 }}, // 45

        { 0, 0, { 0, 0, 0, 0 }}, // 46

        { 95, 226, { 16, 48, 0, 0 }}, // 47 -- cross 2 --
        { 97, 227, { 47, 49, 0, 0 }}, // 48
        { 99, 229, { 48, 50, 0, 0 }}, // 49
        { 102, 229, { 49, 51, 0, 0 }}, // 50
        { 104, 230, { 50, 52, 0, 0 }}, // 51
        { 106, 228, { 51, 53, 0, 0 }}, // 52
        { 108, 225, { 52, 54, 0, 0 }}, // 53
        { 107, 221, { 53, 55, 0, 0 }}, // 54
        { 106, 217, { 54, 56, 0, 0 }}, // 55
        { 105, 213, { 55, 57, 0, 0 }}, // 56
        { 104, 209, { 56, 58, 0, 0 }}, // 57
        { 103, 205, { 57, 59, 0, 0 }}, // 58
        { 100, 201, { 58, 60, 0, 0 }}, // 59
        { 97, 199, { 59, 61, 0, 0 }}, // 60
        { 93, 199, { 60, 62, 0, 0 }}, // 61
        { 90, 200, { 61, 63, 0, 0 }}, // 62
        { 87, 199, { 62, 23, 0, 0 }}, // 63 -- cross 1 --

};

int max_gnalb_path=sizeof(gnalb_path)/sizeof(struct gnalb_path);

struct lab4_gnalb_driver_data
{
        char type;      // 1=guard, 2=gnalb, 3=young, 4=mage, 5=king
        char aggressive,helper;
        char dummyA;

        // gnalb
        int path,lastpath;
};

void lab4_gnalb_driver_parse(int cn, struct lab4_gnalb_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {
                if (!strcmp(name,"type")) dat->type=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void lab4_gnalb_driver_init(int cn, struct lab4_gnalb_driver_data *dat)
{
        int p;

        if (dat->type==1) {
                int dist,mindist=10000;
                for (p=1; p<max_gnalb_path; p++) {
                        if ((dist=map_dist(ch[cn].x,ch[cn].y,gnalb_path[p].x,gnalb_path[p].y))<mindist) {
                                mindist=dist;
                                dat->path=p;
                        }
                }
        }

        if (dat->type==1) {
                // guard
                dat->aggressive=1;
                dat->helper=1;
                fight_driver_set_dist(cn,0,10,20);
        }
        else if (dat->type==2) {
                xlog("CANTBETRUE in %s %d",__FILE__,__LINE__);
        }
        else {
                // crazy
                dat->aggressive=0;
                dat->helper=0;
                fight_driver_set_dist(cn,0,10,0);
        }


}

void lab4_gnalb_driver(int cn, int ret, int lastact)
{
        struct lab4_gnalb_driver_data *dat;
        struct msg *msg,*next;
        int co,cc,in;
        char *str;

        // get data
        dat=set_data(cn,DRD_LAB4_GNALB,sizeof(struct lab4_gnalb_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        lab4_gnalb_driver_parse(cn,dat);
                        lab4_gnalb_driver_init(cn,dat);
                }

                if (msg->type==NT_GIVE) {

                        co=msg->dat1;
                        if (!(in=ch[cn].citem)) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_TEXT) {
                        co=msg->dat3;
                        str=(char *)msg->dat2;
                        tabunga(cn,co,str);
                        if (co==cn) { remove_message(cn,msg); continue; }
                }

                if (msg->type==NT_SEEHIT && dat->type==2) {

                        cc=msg->dat1;
                        co=msg->dat2;
                        if (!cc || !co) { remove_message(cn,msg); continue; }

                        // is the victim our friend? then help
                        if (co!=cn && ch[co].group==ch[cn].group) {
                                if (!is_valid_enemy(cn,cc,-1)) { remove_message(cn,msg); continue; }
                                if (char_dist(cn,cc)>10) { remove_message(cn,msg); continue; }
                                fight_driver_add_enemy(cn,cc,1,1);
                                remove_message(cn,msg);
                                continue;
                        }

                        // is the attacker our friend? then help
                        if (cc!=cn && ch[cc].group==ch[cn].group) {
                                if (!is_valid_enemy(cn,co,-1)) { remove_message(cn,msg); continue; }
                                if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }
                                fight_driver_add_enemy(cn,co,0,1);
                                remove_message(cn,msg);
                                continue;
                        }
                        remove_message(cn,msg);
                        continue;
                }

                standard_message_driver(cn,msg,dat->aggressive,dat->helper);
                remove_message(cn,msg);
	}

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        // rest of standard action
	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        // gnalb guards patroling
        if (dat->type==1 && dat->path) {

                fight_driver_set_home(cn,ch[cn].x,ch[cn].y);

                if (swap_move_driver(cn,gnalb_path[dat->path].x,gnalb_path[dat->path].y,1)) return;
                if (map_dist(ch[cn].x,ch[cn].y,gnalb_path[dat->path].x,gnalb_path[dat->path].y)<4) {
                        int p;

                        do p=RANDOM(4); while(gnalb_path[dat->path].next[p]==0 || (gnalb_path[dat->path].next[1]!=0 && gnalb_path[dat->path].next[p]==dat->lastpath));
                        dat->lastpath=dat->path;
                        dat->path=gnalb_path[dat->path].next[p];
                }
                else do_idle(cn,TICKS/2);

                return;
        }

        // crazy gnalb talking
        if (dat->type==3) {

                switch (RANDOM(50)) {
                        case 0: whisper(cn,"Me saw right in Fire."); break;
                        case 1: whisper(cn,"Me not crazy. In me house me saw in fire."); break;
                        case 2: whisper(cn,"Me will get it out."); break;
                        case 3: whisper(cn,"Fire hot, but me not crazy."); break;
                        case 4: whisper(cn,"Tell mage me saw in fire, me not crazy."); break;
                        case 10: case 11: case 12: case 13: case 14: if (do_use(cn,DX_RIGHT,0)) return;
                }

                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
                // nothing left to do
                do_idle(cn,TICKS/2);
                return;
        }

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

        // nothing left to do
        do_idle(cn,TICKS/2);
}

// -- item driver ---------------------------------------------------------------------------------------------------

// drdata[0]=type               1=fireplace_key

void lab4_item(int in, int cn)
{
        char *drdata;
        int in2;

        drdata=it[in].drdata;

        if (!cn) return;

        if (drdata[0]==1) {
                // fireplace_key
                if (ch[cn].citem) return;
		in2=create_item("lab4_mage_key");
                if (in2) {
                        log_char(cn,LOG_SYSTEM,0,"You took the key out of the fire.");
			if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from lab4_item");
                        ch[cn].citem=in2;
                        ch[cn].flags|=CF_ITEMS;
                        it[in2].carried=cn;
                }
                return;
        }
}

// -- general -------------------------------------------------------------------------------------------------------

int ch_died_driver(int nr, int cn, int co)
{
	switch(nr) {
                case CDR_LAB4SEYAN:             return 1;
                case CDR_LAB4GNALB:             return 1;
                default: return 0;
	}
}

int ch_respawn_driver(int nr, int cn)
{
	switch(nr) {
                case CDR_LAB4SEYAN:             return 1;
                case CDR_LAB4GNALB:             return 1;
		default: return 0;
	}
}

int ch_driver(int nr, int cn, int ret, int lastact)
{
	switch(nr) {
                case CDR_LAB4SEYAN:             lab4_seyan_driver(cn,ret,lastact); return 1;
                case CDR_LAB4GNALB:             lab4_gnalb_driver(cn,ret,lastact); return 1;
                default: return 0;
	}
}

int it_driver(int nr, int in, int cn)
{
	switch(nr) {
                case IDR_LAB4_ITEM:     lab4_item(in,cn); return 1;
		default: return 0;
	}
}

