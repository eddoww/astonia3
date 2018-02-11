/*
 * $Id: staffer.c,v 1.5 2007/12/10 10:12:29 devel Exp $
 *
 * $Log: staffer.c,v $
 * Revision 1.5  2007/12/10 10:12:29  devel
 * added new carlos quest
 *
 * Revision 1.4  2007/08/21 22:08:54  devel
 * smuggler book gets deleted after quest
 *
 * Revision 1.3  2006/12/08 10:34:46  devel
 * fixed bug in logain driver
 *
 * Revision 1.2  2006/09/26 10:59:25  devel
 * added questlog to nomad plains and brannington forest
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "item_id.h"
#include "los.h"
#include "libload.h"
#include "quest_exp.h"
#include "sector.h"
#include "consistency.h"
#include "staffer_ppd.h"
#include "player_driver.h"
#include "questlog.h"

#define SMUGGLEBIT_PEARLS		1
#define SMUGGLEBIT_RING			2
#define SMUGGLEBIT_CAPE			4
#define SMUGGLEBIT_NECKLACE		8

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
	{{"reset","me",NULL},NULL,3}
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
				if (qa[q].answer) quiet_say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}


//-----------------------


void staffer_spiketrap(int in,int cn)
{
	if (cn && !it[in].drdata[1]) {
		it[in].sprite++;
		it[in].drdata[1]=1;

		hurt(cn,it[in].drdata[2]*POWERSCALE,0,1,75,75);

		call_item(it[in].driver,in,0,ticker+TICKS);
		return;
	}
	if (!cn && it[in].drdata[1]) {
		it[in].sprite--;
		it[in].drdata[1]=0;
		set_sector(it[in].x,it[in].y);
		return;
	}
}

void staffer_fireball_machine(int in,int cn)
{
	int dx,dy,power,fn,dxs,dys,frequency;

	if (cn) return;

        dx=it[in].drdata[1]-128;
	dy=it[in].drdata[2]-128;
	power=it[in].drdata[3];
	frequency=it[in].drdata[4];

        if (frequency) call_item(it[in].driver,in,0,ticker+frequency);

	if (dx>0) dxs=1;
	else if (dx<0) dxs=-1;
	else dxs=0;

	if (dy>0) dys=1;
	else if (dy<0) dys=-1;
	else dys=0;

	fn=create_fireball(0,it[in].x+dxs,it[in].y+dys,it[in].x+dx,it[in].y+dy,power);
	notify_area(it[in].x,it[in].y,NT_SPELL,0,V_FIREBALL,fn);
}

void staffer_block_move(int in,int cn)
{
	int m,m2,dx,dy,dir,nr;

	if (cn) {	// player using item
		m=it[in].x+it[in].y*MAXMAP;
		dir=ch[cn].dir;
		dx2offset(dir,&dx,&dy,NULL);
		m2=(it[in].x+dx)+(it[in].y+dy)*MAXMAP;

		if ((map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m2].it || map[m2].gsprite<12150 || map[m2].gsprite>12158) {
			log_char(cn,LOG_SYSTEM,0,"It won't move.");
			return;
		}
		map[m].flags&=~MF_TMOVEBLOCK;
		map[m].it=0;
		set_sector(it[in].x,it[in].y);

		map[m2].flags|=MF_TMOVEBLOCK;
		map[m2].it=in;
		it[in].x+=dx; it[in].y+=dy;
		set_sector(it[in].x,it[in].y);

		// hack to avoid using the chest over and over
		if ((nr=ch[cn].player)) {
			player_driver_halt(nr);
		}

		*(unsigned int*)(it[in].drdata+4)=ticker;

		return;
	} else {	// timer call
		if (!(*(unsigned int*)(it[in].drdata+8))) {	// no coords set, so its first call. remember coords
			*(unsigned short*)(it[in].drdata+8)=it[in].x;
			*(unsigned short*)(it[in].drdata+10)=it[in].y;
		}

		// if 15 minutes have passed without anyone touching the chest, beam it back.
		if (ticker-*(unsigned int*)(it[in].drdata+4)>TICKS*60*15 &&
		    (*(unsigned short*)(it[in].drdata+8)!=it[in].x ||
		     *(unsigned short*)(it[in].drdata+10)!=it[in].y)) {
			
			m=it[in].x+it[in].y*MAXMAP;
			m2=(*(unsigned short*)(it[in].drdata+8))+(*(unsigned short*)(it[in].drdata+10))*MAXMAP;

			if (!(map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) && !map[m2].it) {
				map[m].flags&=~MF_TMOVEBLOCK;
				map[m].it=0;
				set_sector(it[in].x,it[in].y);
	
				map[m2].flags|=MF_TMOVEBLOCK;
				map[m2].it=in;
				it[in].x=*(unsigned short*)(it[in].drdata+8);
				it[in].y=*(unsigned short*)(it[in].drdata+10);
				set_sector(it[in].x,it[in].y);
			}
                }
		call_item(it[in].driver,in,0,ticker+TICKS*5);
	}
}

void vault_skull(int in,int cn)
{
	struct staffer_ppd *ppd;

	if (!cn) return;

	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	if (!ppd) return;

	if (ppd->rouven_state>=0 && ppd->rouven_state<=5) {
		ppd->rouven_state=6;
		log_char(cn,LOG_SYSTEM,0,"You feel an evil aura coming from the skulls. A magical barrier prevents anyone from getting close and you decide to report back to Rouven.");
		questlog_done(cn,62);
	}
}

void vault_shelf(int in,int cn)
{
	int in2;
	
	if (!cn) return;
	
	if (it[in].drdata[1]==2 && (in2=create_item("vault_ritual"))) {
		if (!give_char_item(cn,in2)) {
			destroy_item(in2);
			log_char(cn,LOG_SYSTEM,0,"Please make room in your inventory!");
			return;
		}
		log_char(cn,LOG_SYSTEM,0,"After a lengthy search you find %s.",it[in2].name);
	} else if (it[in].drdata[1]==1 && (in2=create_item("vault_journal"))) {
		if (!give_char_item(cn,in2)) {
			destroy_item(in2);
			log_char(cn,LOG_SYSTEM,0,"Please make room in your inventory!");
			return;
		}
		log_char(cn,LOG_SYSTEM,0,"After a lengthy search you find %s.",it[in2].name);
	} else log_char(cn,LOG_SYSTEM,0,"After a lengthy search you find nothing of interest");
}

void staffer_item(int in,int cn)
{
	switch(it[in].drdata[0]) {
		case 1:		staffer_spiketrap(in,cn); break;
		case 2:		staffer_fireball_machine(in,cn); break;
		case 3:		staffer_block_move(in,cn); break;
		case 4:		vault_skull(in,cn); break;
		case 5:		vault_shelf(in,cn); break;
		default:	elog("unknown staffer item type %d",it[in].drdata[0]);
	}
}


struct smugglecom_data
{
        int last_talk;
	int last_walk;
	int pos;
	int current_victim;
};	

void smugglecom_driver(int cn,int ret,int lastact)
{
	struct smugglecom_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SMUGGLECOMDRIVER,sizeof(struct smugglecom_data));
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
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->smugglecom_state) {
					case 0:         quiet_say(cn,"Greetings, %s!",ch[co].name);
							questlog_open(co,35);
							ppd->smugglecom_state++; didsay=1;
                                                        break;
					case 1:		quiet_say(cn,"I want you to find a book for me called 'the contraband book', which contains the names of four of the smuggler's most precious items.");
							ppd->smugglecom_state++; didsay=1;
                                                        break;
					case 2:		//quiet_say(cn,"Also, I will reward you for every piece of contraband you bring me. ");
                                                        //ppd->smugglecom_state++; didsay=1;
                                                        //break;
							// fall through intended for now
					case 3:		quiet_say(cn,"Go now, and may Ishtar be with you.");
							ppd->smugglecom_state=4; didsay=1;
                                                        break;
					case 4:		break;
					case 5:		if (questlog_isdone(co,36)) { ppd->smugglecom_state=7; break; }
							quiet_say(cn,"It lists four important items which I want you to retrieve: the Rainbow Pearls, the Crimson Ring, the Leopard Cape, and the Emerald Necklace. Find them, and bring them to me.");
							questlog_open(co,36);
							ppd->smugglecom_state++; didsay=1;
                                                        break;
					case 6:		if (ppd->smugglecom_bits==15) {
								ppd->smugglecom_state++;
								questlog_done(co,36);
							}
							break;
					case 7:		quiet_say(cn,"Thank you, you are of great help in hurting the smuggler's operations.");
							if (questlog_isdone(co,37)) { ppd->smugglecom_state=10; break; }
                                                        quiet_say(cn,"Now, as a final task, I want you to kill the smuggler's leader. Good luck!");
							questlog_open(co,37);
							ppd->smugglecom_state++; didsay=1;
							break;
					case 8:		break;
					case 9:		quiet_say(cn,"Thank you for helping us, %s, you have been of great value.",ch[co].name);
							ppd->smugglecom_state++; didsay=1;
							questlog_done(co,37);
                                                        break;
					case 10:	break;

	

				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DIDSAY,cn,0);
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->smugglecom_state<=4) { dat->last_talk=0; ppd->smugglecom_state=0; }
							if (ppd && ppd->smugglecom_state>=5 && ppd->smugglecom_state<=6) { dat->last_talk=0; ppd->smugglecom_state=5; }
							if (ppd && ppd->smugglecom_state>=7 && ppd->smugglecom_state<=8) { dat->last_talk=0; ppd->smugglecom_state=7; }
							break;				
					case 3:		if (ch[co].flags&CF_GOD) ppd->smugglecom_bits=ppd->smugglecom_state=0;
							break;
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				int val;

				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

				if (it[in].ID==IID_STAFF_SMUGGLEBOOK && ppd && ppd->smugglecom_state<=4 && (ch[co].flags&CF_PLAYER)) {
					quiet_say(cn,"Thank you for the book, %s.",ch[co].name);
                                        questlog_done(co,35);
					destroy_item_byID(co,IID_STAFF_SMUGGLEBOOK);
                                        ppd->smugglecom_state=5;
				} else if (it[in].ID==IID_STAFF_SMUGGLEPEARLS && ppd && !(ppd->smugglecom_bits&SMUGGLEBIT_PEARLS) && (ch[co].flags&CF_PLAYER)) {
					quiet_say(cn,"Thank you for bringing back the %s, %s.",it[in].name,ch[co].name);
					
					val=questlog_scale(questlog_count(co,36),1000);
                                        dlog(cn,0,"Received %d exp for doing quest Contraband I for the %d. time (nominal value %d exp)",val,questlog_count(co,36)+1,1000);
					give_exp(co,min(val,level_value(ch[co].level)/4));
					
					ppd->smugglecom_bits|=SMUGGLEBIT_PEARLS;
				} else if (it[in].ID==IID_STAFF_SMUGGLERING && ppd && !(ppd->smugglecom_bits&SMUGGLEBIT_RING) && (ch[co].flags&CF_PLAYER)) {
					quiet_say(cn,"Thank you for bringing back the %s, %s.",it[in].name,ch[co].name);
					
					val=questlog_scale(questlog_count(co,36),1000);
                                        dlog(cn,0,"Received %d exp for doing quest Contraband II for the %d. time (nominal value %d exp)",val,questlog_count(co,36)+1,1000);
					give_exp(co,min(val,level_value(ch[co].level)/4));
					
					ppd->smugglecom_bits|=SMUGGLEBIT_RING;
				} else if (it[in].ID==IID_STAFF_SMUGGLECAPE && ppd && !(ppd->smugglecom_bits&SMUGGLEBIT_CAPE) && (ch[co].flags&CF_PLAYER)) {
					quiet_say(cn,"Thank you for bringing back the %s, %s.",it[in].name,ch[co].name);
					
					val=questlog_scale(questlog_count(co,36),1000);
                                        dlog(cn,0,"Received %d exp for doing quest Contraband III for the %d. time (nominal value %d exp)",val,questlog_count(co,36)+1,1000);
					give_exp(co,min(val,level_value(ch[co].level)/4));
					
					ppd->smugglecom_bits|=SMUGGLEBIT_CAPE;
				} else if (it[in].ID==IID_STAFF_SMUGGLENECKLACE && ppd && !(ppd->smugglecom_bits&SMUGGLEBIT_NECKLACE) && (ch[co].flags&CF_PLAYER)) {
					quiet_say(cn,"Thank you for bringing back the %s, %s.",it[in].name,ch[co].name);
					
					val=questlog_scale(questlog_count(co,36),1000);
                                        dlog(cn,0,"Received %d exp for doing quest Contraband IV for the %d. time (nominal value %d exp)",val,questlog_count(co,36)+1,1000);
					give_exp(co,min(val,level_value(ch[co].level)/4));
					
					ppd->smugglecom_bits|=SMUGGLEBIT_NECKLACE;
				} else {
                                        quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (give_char_item(co,ch[cn].citem)) ch[cn].citem=0;					
				}
				
				// let it vanish, then
				if (ch[cn].citem) {
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

void smugglelead_died(int cn,int co)
{
	struct staffer_ppd *ppd;

	if (!(ch[co].flags&CF_PLAYER)) return;
	
	if (!(ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) return;
	
	if (ppd->smugglecom_state!=8) return;
	
	ppd->smugglecom_state=9;
}

struct rouven_data
{
	int last_talk;
	int current_victim;
};	

void rouven_driver(int cn,int ret,int lastact)
{
	struct rouven_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0,in2;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_ROUVENDRIVER,sizeof(struct rouven_data));
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
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->rouven_state) {
					case 0:         if (ppd->carlos2_state==0) {
								if (!mem_check_driver(cn,co,0)) {
									quiet_say(cn,"Hullo %s. Please talk to Carlos first.",ch[co].name);
									mem_add_driver(cn,co,0);
								}
								break;
							}
							quiet_say(cn,"Hail, %s. Carlos sent you for a ritual? Did he mention the place is cursed? Well, I have two quests of my own for you.",ch[co].name);
                                                        ppd->rouven_state++; didsay=1;
							questlog_open(co,62);
                                                        break;
					case 1:         quiet_say(cn,"First, I beg you to try to locate the source of the curse that has befallen the Imperial Vault.");
                                                        ppd->rouven_state++; didsay=1;
                                                        break;
					case 2:         quiet_say(cn,"It was in the armory where the guards first started acting strangely and attacking everyone.");
                                                        ppd->rouven_state++; didsay=1;
                                                        break;
					case 3:         quiet_say(cn,"The way there is through the left door.");
                                                        ppd->rouven_state++; didsay=1;
                                                        break;
					case 4:         quiet_say(cn,"Good luck.");
                                                        ppd->rouven_state++; didsay=1;
                                                        break;
					case 5:		break;	// waiting for player to find skull
					
					case 6:         quiet_say(cn,"You say there's demons and a pile of strange skulls? They must have burrowed in from the underground. We'll look into this immediately.");
                                                        ppd->rouven_state++; didsay=1;
							questlog_open(co,63);
                                                        break;
					case 7:         quiet_say(cn,"Now I ask you to retrieve the chronicles of Seyan I. He kept a journal detailing many of his plans. Including those for the Aston Empire." );
                                                        ppd->rouven_state++; didsay=1;
                                                        break;
					case 8:         quiet_say(cn,"It was stored in the archives to the right.");
                                                        ppd->rouven_state++; didsay=1;
                                                        break;
					case 9:		break;	// waiting for player to find chronicles
					
					case 10:	quiet_say(cn,"Thank you %s. It will be most useful in our rebuilding efforts.",ch[co].name);
                                                        ppd->rouven_state++; didsay=1;
							break;
					case 11:	quiet_say(cn,"Now we get to the magical ritual. It was kept within the treasury behind me in the emperor's personal vault.");
                                                        ppd->rouven_state++; didsay=1;
							break;
					case 12:	quiet_say(cn,"Take this key and when you find the scroll return to Carlos.");
							if (!has_item(co,IID_MAX_VAULTKEY) && (in2=create_item("vault_key1"))) {
								if (!give_char_item(co,in2)) destroy_item(in2);
							}
                                                        ppd->rouven_state++; didsay=1;
							break;
					case 13:	break;	// all done

				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
					notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DIDSAY,cn,0);
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->rouven_state<=5) { dat->last_talk=0; ppd->rouven_state=0; }
							if (ppd && ppd->rouven_state>=6 && ppd->rouven_state<=9) { dat->last_talk=0; ppd->rouven_state=6; }
							if (ppd && ppd->rouven_state>=10 && ppd->rouven_state<=13) { dat->last_talk=0; ppd->rouven_state=10; }
                                                        break;				
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                                if (it[in].ID==IID_MAX_CHRONICLES && ppd && ppd->rouven_state>=6 && ppd->rouven_state<=9 && (ch[co].flags&CF_PLAYER)) {
					quiet_say(cn,"Thank you for the book, %s.",ch[co].name);
                                        questlog_done(co,63);
					destroy_item_byID(co,IID_MAX_CHRONICLES);
                                        ppd->rouven_state=10;
				} else {
                                        if (it[in].ID==IID_MAX_RITUAL) quiet_say(cn,"Please take the ritual to Carlos.");
					else quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (give_char_item(co,ch[cn].citem)) ch[cn].citem=0;					
				}
				
				// let it vanish, then
				if (ch[cn].citem) {
					destroy_item(ch[cn].citem);
					ch[cn].citem=0;
				}
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_LEFT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}



int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_STAFFER:	staffer_item(in,cn); return 1;

                default:	return 0;
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_SMUGGLECOM:	smugglecom_driver(cn,ret,lastact); return 1;
		case CDR_SMUGGLELEAD:	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;
		case CDR_ROUVEN:	rouven_driver(cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_SMUGGLECOM:	return 1;
		case CDR_SMUGGLELEAD:	smugglelead_died(cn,co); return 1;

                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_SMUGGLECOM:	return 1;
		case CDR_SMUGGLELEAD:	return 1;

                default:	return 0;
	}
}














