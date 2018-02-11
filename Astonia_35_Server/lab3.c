/*
 * $Id: lab3.c,v 1.2 2007/05/02 12:33:27 devel Exp $
 *
 * $Log: lab3.c,v $
 * Revision 1.2  2007/05/02 12:33:27  devel
 * fixed bug with repeat in character names
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

// -- prisoner ------------------------------------------------------------------------------------------------------

struct lab3_passguard_driver_data
{
        int talk;
        int current_victim;
        int current_victim_serial;
        int last_talk;
};

void lab3_passguard_driver(int cn, int ret, int lastact)
{
        static int talk=0;
	struct lab_ppd *ppd;
        struct lab3_passguard_driver_data *dat;
        struct msg *msg,*next;
        int co;
        int talkdir=0,didsay=0;
        char *str;

        // get data
        dat=set_data(cn,DRD_LAB3_PASSGUARD,sizeof(struct lab3_passguard_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,10,0,12);
                        if (!talk) { talk=1; dat->talk=1; }
                }

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                if (msg->type==NT_GIVE && ch[cn].citem) {
                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_CHAR) {

                        co=msg->dat1;

                        // is he the talking one
                        if (!dat->talk) { remove_message(cn,msg); continue; }

                        // is he at home
                        if (ch[cn].x!=ch[cn].tmpx || ch[cn].y!=ch[cn].tmpy) { remove_message(cn,msg); continue; }

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			
                        // dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

                        // get lab ppd
                        if (!(ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd)))) { remove_message(cn,msg); continue; }

			// dont talk to someone far away (if he is, reset the talkstep)
			if (char_dist(cn,co)>10) {
                                if (ppd->guard_talkstep==20) say(cn,"Do not forget thine password, %s. Next time I will ask you again.",ch[co].name);
                                else if (ppd->guard_talkstep) say(cn,"Thou art wise, %s, very wise.",ch[co].name);
                                ppd->guard_talkstep=0;
                                remove_message(cn,msg);
                                continue;
                        }

                        // only talk every five seconds
			if (ticker<dat->last_talk+5*TICKS) { remove_message(cn,msg); continue; }

                        // talk now
                        switch (ppd->guard_talkstep) {
                                case 0: say(cn,"Halt, %s, say the password, or leave this place immediately!",ch[co].name); didsay=1; ppd->guard_talkstep++; break;
                                case 1: didsay=1; ppd->guard_talkstep++; break;
                                case 2: didsay=1; ppd->guard_talkstep++; break;
                                case 3: say(cn,"I'll count up to three, then I will kill thee, %s. So move, or say the password!",ch[co].name); didsay=1; ppd->guard_talkstep++; break;
                                case 4: say(cn,"One."); didsay=1; ppd->guard_talkstep++; break;
                                case 5: say(cn,"Two."); didsay=1; ppd->guard_talkstep++; break;
                                case 6: say(cn,"Three! %s, I'm coming!",ch[co].name);
                                        ppd->guard_talkstep=0;
                                        fight_driver_add_enemy(cn,co,1,1);
                                        break;

                                case 20: break; // password mode
                        }


                        if (didsay) {
                                dat->last_talk=ticker;
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
                        }

		}

                if (msg->type==NT_TEXT) {
                        char password[16];

                        co=msg->dat3;
                        str=(char *)msg->dat2;
                        tabunga(cn,co,str);

			// no emotes
			if (msg->dat1==LOG_INFO) { remove_message(cn,msg); continue; }

                        // is he the talking one
                        if (!dat->talk) { remove_message(cn,msg); continue; }

                        // dont react on other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to someone far away (if he is, reset the talkstep)
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get lab ppd
                        if (!(ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd)))) { remove_message(cn,msg); continue; }

                        sprintf(password,"%s%s",ppd->password1,ppd->password2);

                        if (strcasestr(str,password)) { say(cn,"Fine, %s, I will open the door for thee. Mayest thou pass the last gate.",ch[co].name); ppd->guard_talkstep=20; }
			else if (strcasestr(str,"BLUB")) { say(cn,"What?"); dat->last_talk=ticker+10*TICKS;}
			else if (strcasestr(str,"REPEAT")) { say(cn,"I'll repeat."); ppd->guard_talkstep=0; }

                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        // rest of standard action
	if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        // turn to the one he's talking with
        if (talkdir) turn(cn,talkdir);
        if (dat->last_talk+TICKS*5<ticker) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;		
        }

        do_idle(cn,TICKS);
}

// -- prisoner ------------------------------------------------------------------------------------------------------

struct lab3_prisoner_driver_data
{
        int last_talk;
        int next_talk;
        int give_target,give_serial;
};

void lab3_prisoner_driver(int cn, int ret, int lastact)
{
	struct lab_ppd *ppd;
        struct lab3_prisoner_driver_data *dat;
        struct msg *msg,*next;
        int co,in;
        int talkdir=0,didsay=0;
        char *str;

        // get data
        dat=set_data(cn,DRD_LAB3_PRISONER,sizeof(struct lab3_prisoner_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_GIVE) {

                        co=msg->dat1;
                        if (!(in=ch[cn].citem)) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source

                        // get lab ppd
                        ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd));

                        // check for the key
                        if (ppd && ch[co].flags&CF_PLAYER && it[in].ID==IID_LAB3_PRISONKEY) {
                                ppd->prisoner_talkstep=20;
                        }

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_CHAR) {

                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			
                        // dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
                        // only talk when the old sentence is read
			if (ticker<dat->next_talk) { remove_message(cn,msg); continue; }
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get lab ppd
                        ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd));
                        if (!ppd) { remove_message(cn,msg); continue; }

			switch(ppd->prisoner_talkstep) {
                                case 0: // INTRO
                                        say(cn,"Blub.");
                                        log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,DIST*2,"The Prisoner looks glad to see thee, %s.",ch[co].name);
                                        didsay=1;
                                        ppd->prisoner_talkstep++;
                                        dat->next_talk=ticker+3*TICKS;
                                        break;

                                case 1: log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,DIST*2,"The Prisoner points to thee, %s, and moves his Hands like unlocking a door.",ch[co].name);
                                        didsay=1;
                                        ppd->prisoner_talkstep++;
                                        dat->next_talk=ticker+3*TICKS;
                                        break;

                                case 2: log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,DIST*2,"He points to the imaginary key, then he points to himself.");
                                        didsay=1;
                                        ppd->prisoner_talkstep++;
                                        dat->next_talk=ticker+3*TICKS;
                                        break;

                                case 3: // say(cn,"I wrote it down, so i won't forget it. Wait a moment and I'll give it to thee, %s.",ch[co].name);
                                        log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,DIST*2,"Now he makes signs like giving something to thee.");
                                        didsay=1;
                                        ppd->prisoner_talkstep=255;
                                        dat->next_talk=ticker+3*TICKS;
                                        break;

                                case 20:// GIVE NOTE
                                        if (dat->give_target || ch[cn].citem) break;
					in=create_item("lab3_note_generic");
                                        if (!in) break;
                                        it[in].sprite=11076;
                                        it[in].drdata[1]=21;
                                        it[in].carried=cn;
                                        ch[cn].citem=in;
                                        dat->give_target=co;
                                        dat->give_serial=ch[co].serial;
                                        break;

                                case 21:say(cn,"Blub.");
                                        didsay=1;
                                        ppd->prisoner_talkstep=255;
                                        dat->next_talk=ticker+2*TICKS;
                                        break;
			}

                        if (didsay) {
                                dat->last_talk=ticker;
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
                        }

		}

                if (msg->type==NT_TEXT) {

                        co=msg->dat3;
                        str=(char *)msg->dat2;

                        // tabunga
                        tabunga(cn,co,str);

                        // check
                        if (co==cn) { remove_message(cn,msg); continue; }
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co)) { remove_message(cn,msg); continue; }

                        // get lab ppd
                        ppd=set_data(co,DRD_LAB_PPD,sizeof(*ppd));
                        if (!ppd) { remove_message(cn,msg); continue; }

                        if (strcasestr(str,"BLUB")) { say(cn,"Blub!"); ppd->prisoner_talkstep=0; dat->next_talk=ticker+TICKS*5; }
			else if (strcasestr(str,"REPEAT")) { say(cn,"Blub."); ppd->prisoner_talkstep=0; dat->next_talk=ticker+TICKS*1; }
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        if (dat->give_target) {
                if (!(ch[dat->give_target].flags) || ch[dat->give_target].serial!=dat->give_serial || dist_from_home(cn,dat->give_target)>8) {
                        dat->give_target=0;
			in=ch[cn].citem;
                        remove_item(in);
                        destroy_item(in);
                } else {
                        if (give_driver(cn,dat->give_target)) return;
                        if (ch[cn].citem==0) {
                                if((ppd=set_data(dat->give_target,DRD_LAB_PPD,sizeof(struct lab_ppd)))) {
                                        ppd->prisoner_talkstep++;
                                        dat->next_talk=ticker;
                                }
                                dat->give_target=0;
                                dat->last_talk=ticker-TICKS*28;
                        }
                }
        }

        if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
        }

        do_idle(cn,TICKS);
}

// -- plant ---------------------------------------------------------------------------------------------------------

// drdata[0]=type               -  1 = yellowplant
//                              -  2 = whiteplant
//                              -  3 = brownplant
//                              -  5 = yellowberry
//                              -  6 = whiteberry
//                              - 10 = whiteberry light
//                              - 11 = brownberry

// plant
// drdata[1]=numberries*2
// drdata[2]=maxberries*2

// berries (except brown one)
// drdata[1]=numberries
// drdate[2]=freshness          - 4=fresh to 0=bad

// whiteberry light
// drdata[2]=step
// drdata[3]=value

void lab3_plant(int in, int cn)
{
        static char *freshtext[5]={" awful", "", " tasty", " delicious", " very delicious"};
        static int oxygenseconds[5]={  3,  8, 10, 12, 15};  // seconds of oxygen at freshness level min=(9s) (max=45s)
        static int rotseconds[5]=   { 35, 22, 18, 12,  8};  // seconds of rot timer (yellow) (1m35s together)
        static int lightpower[5]=   { 10, 30, 40, 45, 50};  // power of the light modifier
        static int growseconds=5;  //     65  38  20   8
        int in2,val;
        unsigned char *drdata;
        int immeat=0,startemit=0;

        drdata=it[in].drdata;

        if (!cn) {
                // yellow/white/brown plant
                if (drdata[0]==1 || drdata[0]==2 || drdata[0]==3) {

                        // check if we are maxed out
                        if (drdata[1]==drdata[2]) return;

                        // let it grow
                        it[in].sprite++;
                        drdata[1]++;
                        set_sector(it[in].x,it[in].y);

                        // check if we are maxed out now
                        if (drdata[1]==drdata[2]) return;

                        // set the grow timer
                        call_item(it[in].driver,in,0,ticker+TICKS*growseconds);

                        // done with yellow/white/brown plant
                        return;
                }

                // yellow/white berry
                if (drdata[0]==5 || drdata[0]==6) {
                        static char *numtext[4]={"Oops","One", "Two", "Three"};

                        // check if we are already rotten
                        if (drdata[2]==0) {
				if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it rotten");
                                remove_item(in);
                                destroy_item(in);
                                return;
                        }

                        // let it rot
                        it[in].sprite++;
                        drdata[2]--;
                        if (drdata[0]==5) sprintf(it[in].description,"%s%s yellowberr%s.",numtext[drdata[1]],freshtext[drdata[2]],drdata[1]==1?"y":"ies");
                        if (drdata[0]==6) sprintf(it[in].description,"%s%s whiteberr%s.",numtext[drdata[1]],freshtext[drdata[2]],drdata[1]==1?"y":"ies");
                        update_item(in);

                        // set the rot timer again
                        call_item(it[in].driver,in,0,ticker+TICKS*rotseconds[drdata[2]]*(drdata[0]==6?3:1));

                        // check if he should eat that thing immediately (cn is set in if clause)
                        if (drdata[0]==5 && drdata[2]==4 && (cn=it[in].carried) && !(ch[cn].flags&CF_OXYGEN) && ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/3) { immeat=1; }

                        // done with yellow/white berry
                        if (!immeat) return;
                }

                // whiteberry light
                if (drdata[0]==10) {

                        // paranoia
                        if (!it[in].carried) return;

                        // reduce light value
                        it[in].mod_value[0]=3*it[in].mod_value[0]/4;

                        // if to low, destroy item (or if it's not carried, what never happens :)
                        if (it[in].mod_value[0]<8 || !it[in].carried) {
				if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was rotten");
                                remove_item(in);
                                destroy_item(in);
                                return;
                        }

                        // update character values
                        update_char(it[in].carried);

                        // set the light reduction timer
                        call_item(it[in].driver,in,0,ticker+20*TICKS);

                        // done with whiteberry light
                        return;
                }

                // done with no char
                if (!immeat) return;
        }

        // yellow/white/brown plant
        if (drdata[0]==1 || drdata[0]==2 || drdata[0]==3) {

                // he already has something in his hand
                if (ch[cn].citem) { log_char(cn,LOG_SYSTEM,0,"Nothing happens"); return; }

                // check amount of berries and create the berry item
                if (drdata[0]==1) {
                        if (drdata[1]/2==3) in2=create_item("lab3_yellowberry_three");
                        else if (drdata[1]/2==2) in2=create_item("lab3_yellowberry_two");
                        else if (drdata[1]/2==1) in2=create_item("lab3_yellowberry_one");
                        else { log_char(cn,LOG_SYSTEM,0,"At the moment this plant has no berries to pick!"); return; }
			
                        // the server calls the item to init it, but then it will start to rot immediately, so we increase the freshness by one
                        it[in2].drdata[2]++;
                        it[in2].sprite--;
                }
                else if (drdata[0]==2) {
                        if (drdata[1]/2==3) in2=create_item("lab3_whiteberry_three");
                        else if (drdata[1]/2==2) in2=create_item("lab3_whiteberry_two");
                        else if (drdata[1]/2==1) in2=create_item("lab3_whiteberry_one");
                        else { log_char(cn,LOG_SYSTEM,0,"At the moment this plant has no berries to pick!"); return; }
			
                        // the server calls the item to init it, but then it will start to rot immediately, so we increase the freshness by one
                        it[in2].drdata[2]++;
                        it[in2].sprite--;
                }
                else {
                        if (drdata[1]/2==1) in2=create_item("lab3_brownberry");
                        else { log_char(cn,LOG_SYSTEM,0,"At the moment this plant has no berries to pick!"); return; }
			
                }
                if (!in2) return;

                // give it to him
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"got from lab3_plant");
                it[in2].carried=cn;
                ch[cn].citem=in2;
                ch[cn].flags|=CF_ITEMS;

                // remove it from the plant
                it[in].sprite-=drdata[1];
                drdata[1]=0;
                set_sector(it[in].x,it[in].y);

                // set the grow timer
                call_item(it[in].driver,in,0,ticker+TICKS*growseconds);

                // done with yellow/white/brown plant
                return;
        }

        // yellow/white berry
        if (drdata[0]==5 || drdata[0]==6) {
                char *numtext;

                if (drdata[0]==5) { // eating yellow berries
                        int slot;

                        // remove any existing oxygene spell
                        for (slot=12; slot<30; slot++) {
                                if (it[ch[cn].item[slot]].driver==IDR_OXYGEN) {
                                        destroy_item(ch[cn].item[slot]);
                                        ch[cn].item[slot]=0;
                                }
                        }

                        // create oxygene spell (time depends on freshness*count)
                        val=oxygenseconds[drdata[2]]*drdata[1];
                        if (!add_spell(cn,IDR_OXYGEN,TICKS*val,"nonomagic_spell")) { log_char(cn,LOG_SYSTEM,0,"Due to some strange reasons thou canst not eat %s now.",drdata[1]==1?"that berry":"those berries"); return; }
                }
                else {  // eating white berries
                        int slot,emptyslot;

                        val=lightpower[drdata[2]]*drdata[1];

                        // look if there is already a light spell working (or find an empty one)
                        for (slot=12,emptyslot=0; slot<30; slot++ ) {
                                if (!ch[cn].item[slot]) emptyslot=slot;
                                if (it[ch[cn].item[slot]].driver!=IDR_LAB3_PLANT) continue;
                                if (it[ch[cn].item[slot]].drdata[0]!=10) continue;

                                // found one, increase value
                                it[ch[cn].item[slot]].mod_value[0]=min(255,val+it[ch[cn].item[slot]].mod_value[0]);
                                update_char(cn);
                                emptyslot=0;
                                break;
                        }

                        // otherwise create a new one
                        if (emptyslot && (in2=create_item("lab3_whiteberry_light"))) {
                                it[in2].mod_value[0]=min(255,4*val/3);       // we need a little increment cause we will reduce it immediately
                                it[in2].carried=cn;
                                ch[cn].item[emptyslot]=in2;
                                ch[cn].flags|=CF_ITEMS;
                                startemit=1;
                        }
                }

                // destroy berries
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
                remove_item(in);
                destroy_item(in);

                // tell everyone
                if (drdata[1]==1 && drdata[2]==0) numtext="an";
                else if (drdata[1]==1) numtext="a";
                else if (drdata[1]==2) numtext="two";
                else if (drdata[1]==3) numtext="three";
                else numtext="a strange amount of";

                if (drdata[0]==5) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s %seats %s%s yellowberr%s. (%ds)",ch[cn].name,immeat?"picks and immediately ":"",numtext,freshtext[drdata[2]],drdata[1]==1?"y":"ies",val);
                if (drdata[0]==6) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s %seats %s%s whiteberr%s%s.",ch[cn].name,immeat?"picks and immediately ":"",numtext,freshtext[drdata[2]],drdata[1]==1?"y":"ies",startemit?" and begins to emit light":"");

                // done with yellow/white berry
                return;
        }

        // brownberry
        if (drdata[0]==11) {
                // create effect
                if (!add_spell(cn,IDR_UWTALK,10*TICKS,"nonomagic_spell")) { log_char(cn,LOG_SYSTEM,0,"Thou art still chewing a brown berry."); return; }

                // destroy berry
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
                remove_item(in);
                destroy_item(in);

                // tell everyone
                log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s eats a brown, pearlike berry.",ch[cn].name);
        }

}

// -- special -------------------------------------------------------------------------------------------------------

// drdata[0]=type               - 1 = teleport door
// drdata[0]=type               - 2 = note giving skeleton

// teleport door
// drdata[1]=relative target x  (signed)
// drdata[2]=relative target y  (signed)
// drdata[3]=door is password protected

// note giving skeleton
// drdata[1]=note

void lab3_init_password(struct lab_ppd *ppd)
{
        int ho;

        static char password[][8] = {
                "Gero","nimo",          "Ban","zai",            "Yum","my",             "Jun","ker",            "Jun","ction",
                "Jun","gle",            "Ea","gle",             "Ban","gle",            "An","gle",             "An","gel",
                "E","el",               "He","el",              "Re","el",              "Lab","el",             "Ba","nd",
                "Ba","nns",             "Seal","skin",          "Bu","skin",            "Sheep","skin",         "Sheep","ish",
                "Era","sure",           "Era","sing",           "Era","ser",            "Sen","sing",           "Rai","sing",
                "Rai","der",            "Rai","son",            "Per","son",            "Pri","son",            "Per","mit",
                "Per","iod",            "Per","ch",             "Sw","itch",            "Fet","ch",             "Wre","nch",
                "Bra","nch",            "Be","nch",             "Was","te",             "Da","te",              "Te","st",
                "Sum","moner",          "Sum","pter",           "Sta","ck",             "Sta","ff",             "Sta","te",
                "Gru","nt",             "Gru","dge",            "Ti","bet",             "Gob","bet",            "Gib","bet",
                "Sor","bet",            "Sor","b",              "Sor","ghum",           "Sc","um",              "Al","um",
                "Atr","ium",            "Atr","ophy",           "Tal","on",             "Ta","le",              "Tal","ker",
                "Wa","sh",              "Tal","ent",            "In","tent",            "Stu","dy",             "Stu","ff",
                "Ti","me",              "Na","me",              "Du","st",              "Al","to",              "Fra","me"
        };

        if (*ppd->password1) return;

        ho=RANDOM(sizeof(password)/(8*2))*2;
        sprintf(ppd->password1,password[ho]);
        sprintf(ppd->password2,password[ho+1]);
}

void lab3_special(int in, int cn)
{
        unsigned char *drdata;
        int in2,slot,extinguished;
        struct lab_ppd *ppd;

        drdata=it[in].drdata;

        if (!cn) return;

        // teleport door
        if (drdata[0]==1) {

                // check for password protection
                if (drdata[3] && (ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd))) && ppd->guard_talkstep<20) {
                        log_char(cn,LOG_SYSTEM,0,"The Guard has not opened the door for thee yet, %s.",ch[cn].name);
                        return;
                }

                // teleport him
                if (!teleport_char_driver(cn,ch[cn].x+(signed char)drdata[1],ch[cn].y+(signed char)drdata[2])) {
                        log_char(cn,LOG_SYSTEM,0,"Hm. It seems there is a crowd behind the door. Please try again later.");
                        return;
                }

                // comes into in the water
                if (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_UNDERWATER) {

                        // extinguish torches
                        extinguished=0;
                        for (slot=30; slot<INVENTORYSIZE; slot++) if ((in2=ch[cn].item[slot]) && it[in2].driver==IDR_TORCH && it[in2].drdata[0]) { item_driver(IDR_TORCH,in2,cn); extinguished++; }
                        if ((in2=ch[cn].citem) && it[in2].driver==IDR_TORCH && it[in2].drdata[0]) { item_driver(IDR_TORCH,in2,cn); extinguished++; }
                        if ((in2=ch[cn].item[WN_LHAND]) && it[in2].driver==IDR_TORCH && it[in2].drdata[0]) { item_driver(IDR_TORCH,in2,cn); extinguished++; }
                        if (extinguished) log_char(cn,LOG_SYSTEM,0,"Thine torch%s extinguished due to the water.",extinguished==1?"":"es");

                        // generate some bubbles
                        if (!(ch[cn].flags&CF_OXYGEN)) {
                                create_map_effect(EF_BUBBLE,ch[cn].x,ch[cn].y,ticker+0,ticker+1,0,45);
                                create_map_effect(EF_BUBBLE,ch[cn].x,ch[cn].y,ticker+1,ticker+2,0,45);
                                create_map_effect(EF_BUBBLE,ch[cn].x,ch[cn].y,ticker+2,ticker+3,0,45);
                                create_map_effect(EF_BUBBLE,ch[cn].x,ch[cn].y,ticker+3,ticker+4,0,45);
                                create_map_effect(EF_BUBBLE,ch[cn].x,ch[cn].y,ticker+4,ticker+5,0,45);
                                sound_area(ch[cn].x,ch[cn].y,44+RANDOM(3));
                                sound_area(ch[cn].x,ch[cn].y,44+RANDOM(3));
                                sound_area(ch[cn].x,ch[cn].y,44+RANDOM(3));
                                say(cn,"Hrgblub.");
                        }
                }

                // if the door was password protected, we create the lab exit
                if (drdata[3]) create_lab_exit(cn,25);

                // done with teleport door
                return;
        }

        // note giving skeleton
        if (drdata[0]==2) {

                // char had something in his hand
                if (ch[cn].citem) { log_char(cn,LOG_SYSTEM,0,"Nothing happens."); return; }

                // create note (and set the note value)
		in2=create_item("lab3_note_generic");
                if(!in2) return;
                it[in2].drdata[1]=drdata[1];

                // change sprite of password note
                if (drdata[1]==20) it[in2].sprite=11076;

                // give it to him
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from lab3_special");
                it[in2].carried=cn;
                ch[cn].citem=in2;
                ch[cn].flags|=CF_ITEMS;

                // done with note giving skeleton
                return;
        }

        // note
        if (drdata[0]==3) {
                switch(drdata[1]) {
                        case 1: log_char(cn,LOG_SYSTEM,0,"I have to find a way of holding my breath for longer.  Too bad you can’t breathe under water."); break; // "Ich muss einen Weg finden, meine Luft laenger anzuhalten. Zu dumm, dass man unter Wasser nicht atmen kann."
                        case 2: log_char(cn,LOG_SYSTEM,0,"The yellow berries seem to release oxygen. I have finally figured out how to stay underwater for longer. Now I simply need to manage fighting these hordes of crustaceans in order to find the exit to this part of the labyrinth. The exit is supposed to be somewhere in the south."); break; // "Die gelben Beeren scheinen Sauerstoff freizusetzen. Endlich habe ich einen Weg gefunden, laenger unter Wasser bleiben zu koennen. Jetzt muss ich es nur noch schaffen, gegen diese Horden von crustacean zu bestehen, um den Ausgang aus diesem Labyrinthteil zu finden. Angeblich soll er irgendwo im Sueden liegen."
                        case 3: log_char(cn,LOG_SYSTEM,0,"Behind the southern caves I discovered a rare brown berry. Encouraged by my experience with the yellow berries I ate it. Nothing much happened, but when I expressed my disappointment, I could understand my own words. Very interesting, might even come in handy."); break; // "Hinter den Höhlen im Süden habe ich eine seltene braune Beere entdeckt. Aufgrund meiner Erfahrung mit den gelben Beeren habe ich sie gegessen. Viel ist nicht passiert, doch als ich meiner Entaeuschung Ausdruck verliehen habe, konnte ich meine Worte verstehen. Sehr interresant, vielleicht sogar nuetzlich."
                        case 4: log_char(cn,LOG_SYSTEM,0,"In the south I only discovered the entrance to some caves. I will explore them later on, for the time being I just want to find the exit to this part of the labyrinth. It must be further to the east."); break; // "Im Sueden konnte Ich leider nur einen Zugang zu irgendwelchen Hoehlen finden. Ich werde sie später erforschen, jetzt will ich erst einmal den Ausgang aus diesem Labyrinth Teil finden. Er muss wohl etwas weiter im Osten liegen."
                        case 5: log_char(cn,LOG_SYSTEM,0,"These large crustaceans are too strong, but fortunately very slow."); break; // "Diese large crustacean sind viel zu stark, doch zum glück sind sie auch sehr langsam."
                        case 6: log_char(cn,LOG_SYSTEM,0,"These berries are incredible. When you eat the white ones, you start glowing. Thus I can finally explore the darker regions."); break; // "Diese Beeren sind erstaunlich hier. Isst man die weissen, beginnt man zu leuchten. So kann ich nun endlich auch die dunkleren Regionen absuchen."

                        case 20:if (!(ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd)))) break;
                                lab3_init_password(ppd);
                                log_char(cn,LOG_SYSTEM,0,"Thou can read the incomplete word \"%s...\".",ppd->password1);
                                break;

                        case 21:if (!(ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd)))) break;
                                lab3_init_password(ppd);
                                log_char(cn,LOG_SYSTEM,0,"Thou can read the incomplete word \"...%s\".",ppd->password2);
                                break;

                        default: xlog("Invalid note at %s %d",__FILE__,__LINE__); break;
                }

                // done with note
                return;
        }

}

// -- general -------------------------------------------------------------------------------------------------------

int ch_died_driver(int nr, int cn, int co)
{
	switch(nr) {
                case CDR_LAB3PASSGUARD:         return 1;
                case CDR_LAB3PRISONER:          return 1;
                default: return 0;
	}
}

int ch_respawn_driver(int nr, int cn)
{
	switch(nr) {
                case CDR_LAB3PASSGUARD:         return 1;
                case CDR_LAB3PRISONER:          return 1;
		default: return 0;
	}
}

int ch_driver(int nr, int cn, int ret, int lastact)
{
	switch(nr) {
                case CDR_LAB3PASSGUARD: lab3_passguard_driver(cn,ret,lastact); return 1;
                case CDR_LAB3PRISONER:  lab3_prisoner_driver(cn,ret,lastact); return 1;
                default: return 0;
	}
}

int it_driver(int nr, int in, int cn)
{
	switch(nr) {
                case IDR_LAB3_PLANT:    lab3_plant(in,cn); return 1;
                case IDR_LAB3_SPECIAL:  lab3_special(in,cn); return 1;
		default: return 0;
	}
}









