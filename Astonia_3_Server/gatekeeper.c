/*
 * $Id: gatekeeper.c,v 1.5 2008/03/24 11:22:15 devel Exp $ (c) 2006 D.Brockhaus
 *
 * $Log: gatekeeper.c,v $
 * Revision 1.5  2008/03/24 11:22:15  devel
 * arkhata
 *
 * Revision 1.4  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.3  2006/10/06 18:14:38  devel
 * fixed seyaning to delete the questlog
 *
 * Revision 1.2  2006/01/12 12:14:33  ssim
 * now removes tunnel and IAC PPD when seyaning
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
#include "tool.h"
#include "store.h"
#include "date.h"
#include "map.h"
#include "create.h"
#include "light.h"
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "area3.h"
#include "depot.h"
#include "lab.h"
#include "chat.h"
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
		default: 	return 0;
	}
}

//-----------------------

struct qa
{
	char *word[20];
	char *answer;
	int answer_code;
};

struct qa qa[]={
	{{"how","are","you",NULL},"I'm fine!",0},
	{{"hello",NULL},"Hello, %s!",0},
	{{"hi",NULL},"Hi, %s!",0},
	{{"greetings",NULL},"Greetings, %s!",0},
	{{"hail",NULL},"And hail to you, %s!",0},
        {{"what's","up",NULL},"Everything that isn't nailed down.",0},
	{{"what","is","up",NULL},"Everything that isn't nailed down.",0},
	{{"repeat",NULL},NULL,2},
	{{"restart",NULL},NULL,2},
	{{"please","repeat",NULL},NULL,2},
	{{"please","restart",NULL},NULL,2},
	{{"aye",NULL},NULL,3},
	{{"nay",NULL},NULL,4},

	{{"arch","warrior",NULL},NULL,5},
	{{"arch-warrior",NULL},NULL,5},
	{{"arch","mage",NULL},NULL,6},
	{{"arch-mage",NULL},NULL,6},
	{{"arch-seyan","du",NULL},NULL,7},
	{{"arch","seyan","du",NULL},NULL,7},
	{{"arch-seyan'du",NULL},NULL,7},
	{{"arch","seyan'du",NULL},NULL,7},
	{{"arch","seyan",NULL},NULL,7},
	{{"arch-seyan",NULL},NULL,7},
	{{"seyan","du",NULL},NULL,8},
	{{"seyan'du",NULL},NULL,8},
	{{"seyan",NULL},NULL,8},
	{{"reset",NULL},NULL,9}

};

void lowerstrcpy(char *dst,char *src)
{
	while (*src) *dst++=tolower(*src++);
	*dst=0;
}

int analyse_text_driver(int cn,int type,char *text,int co)
{
	char word[256];
	char wordlist[20][256];
	int n,w,q,name=0;

	// ignore game messages
	if (type==LOG_SYSTEM || type==LOG_INFO) return 0;

	// ignore our own talk
	if (cn==co) return 0;

	if (!(ch[co].flags&CF_PLAYER)) return 0;

	if (char_dist(cn,co)>12) return 0;

	if (!char_see_char(cn,co)) return 0;

	while (isalpha(*text)) text++;
	while (isspace(*text)) text++;
	while (isalpha(*text)) text++;
	if (*text==':') text++;
	while (isspace(*text)) text++;
	if (*text=='"') text++;
	
	n=w=0;
	while (*text) {
		switch (*text) {
			case ' ':
			case ',':
			case ':':
			case '?':
			case '!':
			case '"':
			case '.':       if (n) {
						word[n]=0;
						lowerstrcpy(wordlist[w],word);
						if (strcasecmp(wordlist[w],ch[cn].name)) { if (w<20) w++; }
						else name=1;
					}					
					n=0; text++;
					break;
			default: 	word[n++]=*text++;
					if (n>250) return 0;
					break;
		}		
	}

	if (w) {
		for (q=0; q<sizeof(qa)/sizeof(struct qa); q++) {
			for (n=0; n<w && qa[q].word[n]; n++) {
				//say(cn,"word = '%s'",wordlist[n]);
				if (strcmp(wordlist[n],qa[q].word[n])) break;			
			}
			if (n==w && !qa[q].word[n]) {
				if (qa[q].answer) say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}

struct gate_ppd
{
	int welcome_state;
	int target_class;
	int step;
};

static int enter_room(int cn,int class,int xs,int ys)
{
	int x,y,m,in,n,co;
	struct gate_ppd *ppd;
	
	ppd=set_data(cn,DRD_GATE_PPD,sizeof(struct gate_ppd));
	if (!ppd) return 0;

	for (x=xs; x<xs+9; x++) {
		for (y=ys; y<ys+17; y++) {
			m=x+y*MAXMAP;
			if (map[m].ch) return 0;
			if ((in=map[m].it) && (it[in].flags&IF_TAKE)) return 0;			
		}
	}

	switch(class) {
		case 5:		co=create_char("gatekeeper_w",0); break;
		case 6:		co=create_char("gatekeeper_m",0); break;
		case 7:		co=create_char("gatekeeper_s",0); break;
		case 8:		co=create_char("gatekeeper_s",0); break;
		default:	return 0;
	}
	
	if (!co) return 0;
	
	update_char(co);
        ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
	ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
	ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
        ch[co].dir=DX_RIGHTDOWN;
	ch[co].tmpx=xs+4;
	ch[co].tmpy=ys+13;
	
	notify_char(co,NT_NPC,NTID_GATEKEEPER,cn,0);

	if (!drop_char(co,xs+4,ys+13,0)) {
		destroy_char(co);
		return 0;
	}
	
	if (!teleport_char_driver(cn,xs+4,ys+4)) {
		remove_destroy_char(co);
		return 0;
	}

	destroy_chareffects(cn);
	for (n=12; n<30; n++) {
		if ((in=ch[cn].item[n])) {
			destroy_item(in);
			ch[cn].item[n]=0;
		}
	}

	log_char(cn,LOG_SYSTEM,0,"All your spells have been removed.");

	log_char(cn,LOG_SYSTEM,0,"Once you are ready for the test, use the door to the south-west to enter the room containing your opponent. You have ten minutes from now on.");

	ch[cn].hp=POWERSCALE*1;
	if (ch[cn].mana) ch[cn].mana=POWERSCALE*1;
	ch[cn].endurance=POWERSCALE*1;
	ch[cn].regen_ticker=ticker;

	ppd->target_class=class;
	ppd->step=1;

        return 1;
}

static int enter_test(int cn,int class)
{
	static int room_start[]={186,196,194,196,202,196,
		         178,212,186,212,194,212,202,212};
	int n,cnt;

	if (!(ch[cn].flags&CF_PAID)) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, only paying players may take the test.");
		return 1;
	}

	if (teleport_next_lab(cn,0) && !(ch[cn].flags&CF_GOD)) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, you may not enter before you have solved the labyrinth.");
		return 1;
	}

	if (!(ch[cn].flags&CF_GOD)) {
		// make sure the choice is valid
		switch(class) {
			case 5:		// arch warrior
					if (ch[cn].flags&(CF_MAGE|CF_ARCH)) return 0;
					break;				
			case 6:		// arch mage
					if (ch[cn].flags&(CF_WARRIOR|CF_ARCH)) return 0;
					break;
			case 7:		// arch seyan'dr
					if (ch[cn].flags&CF_ARCH) return 0;
					if (!(ch[cn].flags&CF_WARRIOR) || ! (ch[cn].flags&CF_MAGE)) return 0;
					break;
			case 8:		// seyan'du
					if (ch[cn].flags&CF_ARCH) return 0;
					if ((ch[cn].flags&CF_WARRIOR) && (ch[cn].flags&CF_MAGE)) return 0;
					break;
			default:	return 0;
		}
	
		for (cnt=0,n=30; n<INVENTORYSIZE; n++) {
			if (ch[cn].item[n]) cnt++;
		}
		if (ch[cn].citem) cnt++;
	
		if (cnt && class!=8) {
			log_char(cn,LOG_SYSTEM,0,"Sorry, you may not enter while you are carrying items. You currently have %d items.",cnt);
			return 1;
		}
	
		if (cnt>3) {
			log_char(cn,LOG_SYSTEM,0,"Sorry, you may not enter while you are carrying more than three items. You currently have %d items.",cnt);
			return 1;
		}
	}

	if (!take_money(cn,100*100)) {
		log_char(cn,LOG_SYSTEM,0,"Thou canst pay the price of 100G.");
		return 1;
	}

	for (n=0; n<ARRAYSIZE(room_start); n+=2) {
		if (enter_room(cn,class,room_start[n+0],room_start[n+1])) break;
	}
	if (n==ARRAYSIZE(room_start)) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, the gatekeeper is busy at the moment. Please come back later.");
		ch[cn].gold+=100*100;
		return 1;
	}

	return 1;
}

struct gate_welcome_driver_data
{
        int last_talk;
	int current_victim;
	int amgivingback;
};

void gate_welcome_driver(int cn,int ret,int lastact)
{
	struct gate_welcome_driver_data *dat;
	struct gate_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_GATE_WELCOME,sizeof(struct gate_welcome_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }

			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*5) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_GATE_PPD,sizeof(struct gate_ppd));

                        if (ppd) {
				switch(ppd->welcome_state) {
					case 0:		say(cn,"Be greeted, %s. These are the halls of Ishtar. Only the greatest fighters and magic users come here, to take the final test and fight the Gatekeeper.",ch[co].name);
							ppd->welcome_state++; didsay=1;
							break;
					case 1:		say(cn,"Those who succeed in this test will be able to enhance their abilities further. They may either choose to learn more about their profession than any other mortal being, or to start again as one who can learn all arts.");
							ppd->welcome_state++; didsay=1;
							break;
					case 2:		if (teleport_next_lab(co,0)) {
								say(cn,"Before thou mayest engage the Gatekeeper, thou must solve the Labyrinth built by Ishtar. Thou canst enter the labyrinth through the door to the east.");
								ppd->welcome_state++; didsay=1;
							} else {
								ppd->welcome_state=4; didsay=1;
							}
					case 3:		if (!teleport_next_lab(co,0)) ppd->welcome_state++; // fall through intended
							else break;

					case 4:		if (teleport_next_lab(co,0)) {
								ppd->welcome_state=2; didsay=1;
								break;
							}
								
							if (ch[co].flags&CF_ARCH) {	// arch-XXX
								say(cn,"There is nothing I can do for thee, %s, though, since thou art already an Arch-%s.",
								    ch[co].name,
								    (ch[co].flags&CF_WARRIOR) ? (ch[co].flags&CF_MAGE) ? "Seyan'Du" : "Warrior" : "Mage");
								ppd->welcome_state=6; didsay=1;
							} else if ((ch[co].flags&CF_MAGE) && (ch[co].flags&CF_WARRIOR)) { // seyan, non-arch
								say(cn,"Since thou art already a Seyan'Du, thy only choice is to become Arch-Seyan'Du.");
								ppd->welcome_state++; didsay=1;
							} else {
								say(cn,"The choice is hard, and so is the test. If thou wishest to take the test, decide which path to follow. That of the Arch-%s, or that of the Seyan'Du.",(ch[co].flags&CF_WARRIOR) ? "Warrior" : "Mage");
								ppd->welcome_state++; didsay=1;
							}							
							break;
					case 5:		say(cn,"Name the class thou wishest to become to begin the test. Each try will cost thee 100 gold coins.");
							ppd->welcome_state++; didsay=1;
							break;
					case 6:		// waiting for answer
							break;

				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ticker>dat->last_talk+TICKS*10 && dat->current_victim) dat->current_victim=0;

			if (dat->current_victim && dat->current_victim!=co) { remove_message(cn,msg); continue; }

			switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 2:		ppd=set_data(co,DRD_GATE_PPD,sizeof(struct gate_ppd));
						if (ppd && ppd->welcome_state<=6) ppd->welcome_state=0;
						break;
				case 5:	
				case 6:
				case 7:
				case 8:         if (!enter_test(co,didsay)) {
							say(cn,"That is not a possible choice.");
						}				
						break;
				case 9:		if (ch[co].flags&CF_GOD) del_data(co,DRD_LAB_PPD);
						break;
			}
			if (didsay) {
				dat->last_talk=ticker;
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				
                                if (!dat->amgivingback) {					
					say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					dat->amgivingback=1;
				} else dat->amgivingback++;
					
				if (dat->amgivingback<20 && give_driver(cn,co)) return;
					
                                // let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	dat->amgivingback=0;

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;		
	}
	
	do_idle(cn,TICKS);
}

struct gate_fight_driver_data
{
	int creation_time;
	int victim;
};

void gate_fight_driver(int cn,int ret,int lastact)
{
        struct gate_fight_driver_data *dat;
	struct msg *msg,*next;

	dat=set_data(cn,DRD_GATE_FIGHT,sizeof(struct gate_fight_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        dat->creation_time=ticker;
                        fight_driver_set_dist(cn,10,0,20);
                }

		if (msg->type==NT_NPC && msg->dat1==NTID_GATEKEEPER) {
			dat->victim=msg->dat2;
		}

                standard_message_driver(cn,msg,1,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	// self destruct eventually
	if (ticker-dat->creation_time>TICKS*60*10) {
		say(cn,"Thats all folks!");
		remove_destroy_char(cn);
		return;
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;
			
	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

        do_idle(cn,TICKS);
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

void turn_seyan(int cn)
{
	int co,n,m,in;
	struct depot_ppd *ppd;

	// set skill values
	co=create_char("seyan_m",0);
	if (!co) return;

	for (n=0; n<V_MAX; n++) {
		ch[cn].value[1][n]=ch[co].value[1][n];
	}
	destroy_char(co);

	// set experience
	ch[cn].exp=0;
	ch[cn].exp_used=0;
	ch[cn].level=1;
	ch[cn].lifeshield=0;

	// remove professions
	for (n=0; n<P_MAX; n++) {
		ch[cn].prof[n]=0;
	}

	// un-equip items
	for (n=0,m=30; n<12; n++) {
		if (!(in=ch[cn].item[n])) continue;
		for (; m<INVENTORYSIZE; m++) {
			if (!ch[cn].item[m]) break;
		}
		if (m==INVENTORYSIZE) {
			free_item(in);
		} else ch[cn].item[m]=in;
                ch[cn].item[n]=0;		
	}

	// remove spells
	for (n=12; n<30; n++) {
		if (!(in=ch[cn].item[n])) continue;
		free_item(in);
		ch[cn].item[n]=0;		
	}
	destroy_chareffects(cn);

	// set flags
	ch[cn].flags|=CF_MAGE|CF_WARRIOR|CF_ITEMS;

	// update powers
	ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
	ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
	
	update_char(cn);

	del_data(cn,DRD_TREASURE_CHEST_PPD);
	del_data(cn,DRD_FIRSTKILL_PPD);
	del_data(cn,DRD_AREA1_PPD);
	del_data(cn,DRD_AREA3_PPD);
	del_data(cn,DRD_RANK_PPD);
	del_data(cn,DRD_FLOWER_PPD);
	del_data(cn,DRD_RANDCHEST_PPD);	
        del_data(cn,DRD_DEMONSHRINE_PPD);
	del_data(cn,DRD_MILITARY_PPD);
	del_data(cn,DRD_FARMY_PPD);
	del_data(cn,DRD_ARENA_PPD);
	del_data(cn,DRD_RANDOMSHRINE_PPD);
	del_data(cn,DRD_TWOCITY_PPD);
	del_data(cn,DRD_ORBSPAWN_PPD);
	del_data(cn,DRD_RUNE_PPD);
	del_data(cn,DRD_NOMAD_PPD);
	del_data(cn,DRD_LAB_PPD);
	del_data(cn,DRD_SIDESTORY_PPD);
	del_data(cn,DRD_RATCHEST_PPD);
	del_data(cn,DRD_STAFFER_PPD);
	del_data(cn,DRD_TUNNEL_PPD);
	del_data(cn,DRD_STRATEGY_PPD);
	del_data(cn,DRD_QUESTLOG_PPD);
	del_data(cn,DRD_ARKHATA_PPD);


	// remove quest items from inventory
        for (n=0; n<INVENTORYSIZE; n++) {
		if (!(in=ch[cn].item[n])) continue;
		if (!(it[in].flags&IF_QUEST)) continue;
		
		free_item(in);
		ch[cn].item[n]=0;		
	}

	// remove quest items from depot
	ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd));
	if (!ppd) return;
	for (n=0; n<MAXDEPOT; n++) {
		if (ppd->itm[n].flags&IF_QUEST) {
			ppd->itm[n].flags=0;
		}
	}
}

void gate_fight_dead(int cn,int co)
{
	struct gate_ppd *ppd;
	char buf[256];
	
	ppd=set_data(co,DRD_GATE_PPD,sizeof(struct gate_ppd));
	if (!ppd) return;

        log_char(co,LOG_SYSTEM,0,"Well done.");

	switch(ppd->target_class) {
		case 5:		// arch warrior
				if (ch[co].flags&(CF_MAGE|CF_ARCH)) return;
				ch[co].flags|=CF_ARCH;
				ch[co].value[1][V_RAGE]=1;
				log_char(co,LOG_SYSTEM,0,"You are an Arch-Warrior now.");
				
				sprintf(buf,"0000000000°c10Grats: %s is an Arch-Warrior now!",ch[co].name);
				server_chat(6,buf);
				break;				
		case 6:		// arch mage
				if (ch[co].flags&(CF_WARRIOR|CF_ARCH)) return;
				ch[co].flags|=CF_ARCH;
				ch[co].value[1][V_DURATION]=1;
				log_char(co,LOG_SYSTEM,0,"You are an Arch-Mage now.");

				sprintf(buf,"0000000000°c10Grats: %s is an Arch-Mage now!",ch[co].name);
				server_chat(6,buf);
				break;
		case 7:		// arch seyan'dr
				if (ch[co].flags&CF_ARCH) return;
				if (!(ch[co].flags&CF_WARRIOR) || ! (ch[co].flags&CF_MAGE)) return;
				ch[co].flags|=CF_ARCH;
				log_char(co,LOG_SYSTEM,0,"You are an Arch-Seyan'Du now.");

				sprintf(buf,"0000000000°c10Grats: %s is an Arch-Seyan'Du now!",ch[co].name);
				server_chat(6,buf);
				break;
		case 8:		// seyan'du
				//if (ch[co].flags&CF_ARCH) return;
				//if ((ch[co].flags&CF_WARRIOR) && (ch[co].flags&CF_MAGE)) return;
				turn_seyan(co);
				log_char(co,LOG_SYSTEM,0,"You are a Seyan'Du now.");

				sprintf(buf,"0000000000°c10Grats: %s is a Seyan'Du now!",ch[co].name);
				server_chat(6,buf);
				break;
	}
	teleport_char_driver(co,181,198);
}

void labentrance(int in,int cn)
{
	int ret;

	if (!cn) return;

	ret=teleport_next_lab(cn,1);
	if (ret==0) log_char(cn,LOG_SYSTEM,0,"You have solved all existing labyrinths already. You can now fight the gatekeeper.");
	else if (ret==-1) log_char(cn,LOG_SYSTEM,0,"°c3The area containing the next labyrinth part is down. Please try again soon.");
	else if (ret<-1) log_char(cn,LOG_SYSTEM,0,"°c3You may not enter before reaching level %d.",-ret);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_GATE_WELCOME:	gate_welcome_driver(cn,ret,lastact); return 1;
		case CDR_GATE_FIGHT:	gate_fight_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_LABENTRANCE:	labentrance(in,cn); return 1;
                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_GATE_WELCOME:	immortal_dead(cn,co); return 1;
		case CDR_GATE_FIGHT:	gate_fight_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_GATE_WELCOME:	return 1;

		default:		return 0;
	}
}





