/*
 * $Id: staffer2.c,v 1.7 2008/03/29 15:47:12 devel Exp $
 *
 * $Log: staffer2.c,v $
 * Revision 1.7  2008/03/29 15:47:12  devel
 * added arkhata quest
 *
 * Revision 1.6  2007/07/01 12:10:33  devel
 * reduced money rewards
 *
 * Revision 1.5  2007/06/12 11:34:43  devel
 * changed silver to gold trade ratio
 *
 * Revision 1.4  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.3  2006/10/04 17:30:17  devel
 * added a lot of items to be destroyed on quest solve
 *
 * Revision 1.2  2006/09/27 11:40:43  devel
 * added questlog to brannington
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
#include "player_driver.h"
#include "staffer_ppd.h"
#include "questlog.h"
#include "arkhata.h"

#define BRANFO_EXP_BASE	10000	
#define BRAN_EXP_BASE	15000	//(gives out base*19 = 285,000 exp)

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
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
	{{"reset","me",NULL},NULL,3},
	{{"thousand","gold",NULL},NULL,4},
	{{"five","thousand","silver",NULL},NULL,5}
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

void staffer_book(int in,int cn)
{
	if (!cn) return;
	
	if (*(unsigned int*)(it[in].drdata+4)!=ch[cn].ID) {
		it[in].drdata[1]=0;
		*(unsigned int*)(it[in].drdata+4)=ch[cn].ID;
	}
	
	switch(it[in].drdata[1]) {
		case 0:		it[in].drdata[1]++;
				log_char(cn,LOG_SYSTEM,0,
"The training of these thieves into skilled mages has been succesful. They can "
"now create Golems, and summon the old enemies of Aston, the Grolms. I will not teach "
"them how to create and control Undead though, lest they use them against me... Also, to "
"this end, I have enlisted the help of an assassin by the name of Brenneth. I hope he will "
"not disappoint me...");
				log_char(cn,LOG_SYSTEM,0,"USE again to continue.");
				break;
		case 1:		it[in].drdata[1]++;
				log_char(cn,LOG_SYSTEM,0,
"My golems have dug their way into the Brannington Crypt. I have taken their "
"Holy Relic, and turned it into my weapon to make undead of the Brannington Ancestors. "
"They shall serve as my army and take over Brannington town. All serve as zombies and "
"skeletons, however, there is one spirit who managed to escape my grasp. I will have to "
"find ways to control it... Also, Brenneth was attacked by a grolm and is suffering from "
"loss of memory... He is in one of the thief mage houses right now... Fortunately, they don't "
"know who he is...");
				log_char(cn,LOG_SYSTEM,0,"USE again to continue.");
				break;

		case 2:		it[in].drdata[1]++;
				log_char(cn,LOG_SYSTEM,0,
"Brenneth got rescued by a group of traveling adventurers while the thief mage "
"who had him captured was creating more golems... Luckily, Brenneth doesn't recall "
"anything of what he is supposed to do, and it doesn't look like he'll get his memory back... "
"ever...");
				log_char(cn,LOG_SYSTEM,0,"USE again to continue.");
				break;

		case 3:		it[in].drdata[1]++;
				log_char(cn,LOG_SYSTEM,0,
"The spirit seems uncontrollable... I will have to become stronger to control it, "
"which means I have to train... And that takes time, time which I'd rather not waste... I have "
"also seen the face of a new enemy... This enemy has killed my thief mages, and surely "
"must be coming for me next... He ruined my plans to open the crypt doors with the jewelry "
"the thief mages had managed to steal... They should have been faster in returning it to "
"me... fools...");
				log_char(cn,LOG_SYSTEM,0,"USE again to continue.");
				break;

		case 4:		it[in].drdata[1]=0;
				log_char(cn,LOG_SYSTEM,0,
"I can hear my enemy coming for me... I shall kill and make of my enemy a "
"commander in my army of undead... Now, I will fight and show my power!");
				log_char(cn,LOG_SYSTEM,0,"USE to start over.");
				break;

	}
}

void staffer_mine(int in,int cn)
{
        if (!cn) {
		if (!it[in].drdata[4]) {
                        it[in].drdata[4]=1;
			switch((it[in].x+it[in].y)%3) {
				case 0:		it[in].sprite=15070; break;
				case 1:		it[in].sprite=15078; break;
				case 2:		it[in].sprite=15086; break;
			}
			
		}

                if (it[in].drdata[3]==8) {
			if ((map[it[in].x+it[in].y*MAXMAP].flags&MF_TMOVEBLOCK) || map[it[in].x+it[in].y*MAXMAP].it) {
				call_item(it[in].driver,in,0,ticker+TICKS);
				return;
			}
			it[in].sprite-=8;
			it[in].drdata[3]=0;
			it[in].flags|=IF_USE;
			it[in].flags&=~IF_VOID;
			
			remove_lights(it[in].x,it[in].y);
			map[it[in].x+it[in].y*MAXMAP].it=in;
			map[it[in].x+it[in].y*MAXMAP].flags|=MF_TSIGHTBLOCK|MF_TMOVEBLOCK;
			it[in].flags|=IF_SIGHTBLOCK;
			reset_los(it[in].x,it[in].y);
			add_lights(it[in].x,it[in].y);
                        set_sector(it[in].x,it[in].y);
		}
		return;
	}

        if (it[in].drdata[3]<9) {
		if (ch[cn].endurance<POWERSCALE) {
			log_char(cn,LOG_SYSTEM,0,"You're too exhausted to continue digging.");
			player_driver_dig_off(cn);
			return;
		}
		ch[cn].endurance-=POWERSCALE/4-(ch[cn].prof[P_MINER]*POWERSCALE/(4*25));

                it[in].drdata[3]++;
		it[in].drdata[5]=0;
		it[in].sprite++;
		
		if (it[in].drdata[3]==8) {
                        map[it[in].x+it[in].y*MAXMAP].it=0;
			map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TMOVEBLOCK;
			it[in].flags&=~IF_USE;
			it[in].flags|=IF_VOID;
			call_item(it[in].driver,in,0,ticker+TICKS*60*5+RANDOM(TICKS*60*5));
		}
		
		set_sector(it[in].x,it[in].y);

		if (it[in].drdata[3]==3) {
			remove_lights(it[in].x,it[in].y);
			map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TSIGHTBLOCK;
			it[in].flags&=~IF_SIGHTBLOCK;
			reset_los(it[in].x,it[in].y);
			add_lights(it[in].x,it[in].y);
		}
	}
	
	if (it[in].drdata[3]<8) player_driver_dig_on(cn);
	else player_driver_dig_off(cn);
}

void staffer_block_move(int in,int cn)
{
	int m,m2,dx,dy,dir,nr,wrongsprite=0;

	if (cn) {	// player using item
		m=it[in].x+it[in].y*MAXMAP;
		dir=ch[cn].dir;
		dx2offset(dir,&dx,&dy,NULL);
		m2=(it[in].x+dx)+(it[in].y+dy)*MAXMAP;

		if ((map[m2].gsprite<20291 || map[m2].gsprite>20299) && map[m2].gsprite!=13154 && map[m2].gsprite>13156) wrongsprite=1;
		
                if ((map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m2].it || wrongsprite) {
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
		if (ticker-*(unsigned int*)(it[in].drdata+4)>TICKS*60*2 &&
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

int staffer_spec_door(int in,int cn)
{
	int m,in2;

	if (!it[in].x) return 2;

	if (!cn) {	// called by timer
		if (it[in].drdata[39]) it[in].drdata[39]--;	// timer counter
                if (!it[in].drdata[1]) return 2;		// if the door is closed already, don't open it again
		if (it[in].drdata[39]) return 2;		// we have more outstanding timer calls, dont close now
	}

        m=it[in].x+it[in].y*MAXMAP;

	if (it[in].drdata[1] && (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) {	// doorway is blocked
		if (!cn) {	// timer callback - restart
			it[in].drdata[39]++;	// timer counter
			call_item(it[in].driver,in,0,ticker+TICKS*5);
		}
		return 2;
	}

	// if the door needs a key, check if the character has it
	if (cn) {
		// check for special conditions...
		if (it[in].drdata[0]==4 && !(in2=map[51+234*MAXMAP].it) && it[in2].sprite!=21203) {
			log_char(cn,LOG_SYSTEM,0,"The door is locked.");
			return 2;
		}
		if (it[in].drdata[0]==5 && !(in2=map[59+240*MAXMAP].it) && it[in2].sprite!=21203) {
			log_char(cn,LOG_SYSTEM,0,"The door is locked.");
			return 2;
		}
	}

	remove_lights(it[in].x,it[in].y);

	if (cn) sound_area(it[in].x,it[in].y,3);
	else sound_area(it[in].x,it[in].y,2);

        if (it[in].drdata[1]) {	// it is open, close
		it[in].flags|=*(unsigned long long*)(it[in].drdata+30);
		if (it[in].flags&IF_MOVEBLOCK) map[m].flags|=MF_TMOVEBLOCK;
		if (it[in].flags&IF_SIGHTBLOCK) map[m].flags|=MF_TSIGHTBLOCK;
		if (it[in].flags&IF_SOUNDBLOCK) map[m].flags|=MF_TSOUNDBLOCK;
		if (it[in].flags&IF_DOOR) map[m].flags|=MF_DOOR;
		it[in].drdata[1]=0;
		it[in].sprite--;
	} else { // it is closed, open
		*(unsigned long long*)(it[in].drdata+30)=it[in].flags&(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		it[in].flags&=~(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		map[m].flags&=~(MF_TMOVEBLOCK|MF_TSIGHTBLOCK|MF_DOOR|MF_TSOUNDBLOCK);
		it[in].drdata[1]=1;
		it[in].sprite++;

		it[in].drdata[39]++;	// timer counter
		if (!it[in].drdata[5]) call_item(it[in].driver,in,0,ticker+TICKS*10);		
	}

        reset_los(it[in].x,it[in].y);
        if (!it[in].drdata[38] && !reset_dlight(it[in].x,it[in].y)) it[in].drdata[38]=1;
	add_lights(it[in].x,it[in].y);

	return 1;
}

void staffer_animation_book(int in,int cn)
{
	struct staffer_ppd *ppd;

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) return;

	if (ppd->shanra_state<3) {
		ppd->shanra_state=3;
                give_exp(cn,min(level_value(60)/5,level_value(ch[cn].level)/4));
	}
	teleport_char_driver(cn,25,114);
}


int staffer2_item(int in,int cn)
{
	switch(it[in].drdata[0]) {
		case 1:		staffer_book(in,cn); break;
		case 2:		staffer_mine(in,cn); break;
		case 3:		staffer_block_move(in,cn); break;
		case 4:		return staffer_spec_door(in,cn);
		case 5:		return staffer_spec_door(in,cn);
		case 6:		staffer_animation_book(in,cn); break;
		default:	elog("unknown staffer item %s type %d",it[in].name,it[in].drdata[0]);
	}
	return 1;
}

void countbran_give_keys(int cn,int co,struct staffer_ppd *ppd)
{
	int in,cnt=0;

	if (!ppd) return;
	
	if ((ppd->countbran_bits&1) && !has_item(co,IID_STAFF_MAUSOLEUMKEY1)) {
		in=create_item("warr_mausoleumkey1");
		if (in) {
			if (!give_char_item(co,in)) destroy_item(in);
		}
		cnt++;
	}
	if ((ppd->countbran_bits&2) && !has_item(co,IID_STAFF_MAUSOLEUMKEY2)) {
		in=create_item("warr_mausoleumkey2");
		if (in) {
			if (!give_char_item(co,in)) destroy_item(in);
		}
		cnt++;
	}
	if ((ppd->countbran_bits&4) && !has_item(co,IID_STAFF_MAUSOLEUMKEY3)) {
		in=create_item("warr_mausoleumkey3");
		if (in) {
			if (!give_char_item(co,in)) destroy_item(in);
		}
		cnt++;
	}
	if (cnt) {
		say(cn,"Here, take %s key%s, %s.",
			cnt>1 ? "these" : "this",
			cnt>1 ? "s" : "",
			ch[co].name);
	}
}


struct count_brannington_data
{
        int last_talk;
        int current_victim;
};	

void count_brannington_driver(int cn,int ret,int lastact)
{
	struct count_brannington_data *dat;
	struct staffer_ppd *ppd;
	struct arkhata_ppd *ppd2;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_COUNTBRANDRIVER,sizeof(struct count_brannington_data));
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
                                switch(ppd->countbran_state) {
					case 0:         quiet_say(cn,"Greetings, %s, welcome to Brannington!",ch[co].name);
							if (!questlog_isdone(co,40)) questlog_open(co,40);
							ppd->countbran_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"My guards told me that you were coming, and they said you might be able to help me with my problems.");
							ppd->countbran_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"You see, my family was recently robbed by three thief mages of our most valuable jewelry, and we would like to ask for your aid.");
                                                        ppd->countbran_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"If you can bring back our jewelry, we would be very thankful indeed.");
							ppd->countbran_state=4; didsay=1;
                                                        break;
					case 4:		break;	// waiting for jewelry
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->countbran_state<=4) { dat->last_talk=0; ppd->countbran_state=0; countbran_give_keys(cn,co,ppd); }
                                                        break;				
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->countbran_bits=ppd->countbran_state=ppd->countessabran_state=ppd->daughterbran_state=0; }
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
				ppd2=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));

				if (it[in].ID==IID_STAFF_COUNTJEWEL && ppd && !(ppd->countbran_bits&1)) {
					quiet_say(cn,"Thank you so much for bringing this back, %s. It has been in the family for generations. Here is your reward.",ch[co].name);
					destroy_item_byID(co,IID_STAFF_COUNTJEWEL);
					destroy_item_byID(co,IID_STAFF_THIEFKEY1);
					destroy_item_byID(co,IID_STAFF_REDKEY1);
					destroy_item_byID(co,IID_STAFF_REDKEY2);
					destroy_item_byID(co,IID_STAFF_REDKEY3);
					destroy_item_byID(co,IID_STAFF_REDKEY12);
					destroy_item_byID(co,IID_STAFF_REDKEY23);
					destroy_item_byID(co,IID_STAFF_REDKEY13);
					destroy_item_byID(co,IID_STAFF_REDKEY123);
					
					val=questlog_scale(questlog_count(co,40),60000);
                                        dlog(cn,0,"Received %d exp for doing quest Three Jewels I for the %d. time (nominal value %d exp)",val,questlog_count(co,40)+1,60000);
					give_exp(co,min(val,level_value(ch[co].level)/4));

                                        if (questlog_count(co,40)==0) give_money(co,250*100,"Count Bran Quest 1");

					ppd->countbran_bits|=1;
					if ((ppd->countbran_bits&(1|2|4))==(1|2|4)) questlog_done(co,40);
					countbran_give_keys(cn,co,ppd);					
				} else if (it[in].ID==IID_STAFF_COUNTESSAJEWEL && ppd && !(ppd->countbran_bits&2)) {
					quiet_say(cn,"Ah, my wife will be most pleased! Here is your reward, %s. If you go to my wife she will give you an additional reward.",ch[co].name);
					destroy_item_byID(co,IID_STAFF_COUNTESSAJEWEL);
					destroy_item_byID(co,IID_STAFF_THIEFKEY2);
					destroy_item_byID(co,IID_STAFF_BLUEKEY1);
					destroy_item_byID(co,IID_STAFF_BLUEKEY2);
					destroy_item_byID(co,IID_STAFF_BLUEKEY3);
					destroy_item_byID(co,IID_STAFF_BLUEKEY12);
					destroy_item_byID(co,IID_STAFF_BLUEKEY23);
					destroy_item_byID(co,IID_STAFF_BLUEKEY13);
					destroy_item_byID(co,IID_STAFF_BLUEKEY123);
					
					val=questlog_scale(questlog_count(co,40),30000);
                                        dlog(cn,0,"Received %d exp for doing quest Three Jewels II for the %d. time (nominal value %d exp)",val,questlog_count(co,40)+1,30000);
					give_exp(co,min(val,level_value(ch[co].level)/4));

                                        if (questlog_count(co,40)==0) give_money(co,125*100,"Count Bran Quest 2");

					ppd->countbran_bits|=2;
					countbran_give_keys(cn,co,ppd);
					if ((ppd->countbran_bits&(1|2|4))==(1|2|4)) questlog_done(co,40);
				} else if (it[in].ID==IID_STAFF_DAUGHTERJEWEL && ppd && !(ppd->countbran_bits&4)) {
					quiet_say(cn,"Returning this will heal my daughter's heart. She has been so upset about losing it. Let me reward you now, %s, and if you go to my daughter, she will further reward you.",ch[co].name);
					destroy_item_byID(co,IID_STAFF_DAUGHTERJEWEL);
					destroy_item_byID(co,IID_STAFF_THIEFKEY3);
					destroy_item_byID(co,IID_STAFF_GREENKEY1);
					destroy_item_byID(co,IID_STAFF_GREENKEY2);
					destroy_item_byID(co,IID_STAFF_GREENKEY3);
					destroy_item_byID(co,IID_STAFF_GREENKEY12);
					destroy_item_byID(co,IID_STAFF_GREENKEY23);
					destroy_item_byID(co,IID_STAFF_GREENKEY13);
					destroy_item_byID(co,IID_STAFF_GREENKEY123);

					val=questlog_scale(questlog_count(co,40),30000);
                                        dlog(cn,0,"Received %d exp for doing quest Three Jewels III for the %d. time (nominal value %d exp)",val,questlog_count(co,40)+1,30000);
					give_exp(co,min(val,level_value(ch[co].level)/4));

                                        if (questlog_count(co,40)==0) give_money(co,125*100,"Count Bran Quest 3");
					ppd->countbran_bits|=4;
					if ((ppd->countbran_bits&(1|2|4))==(1|2|4)) questlog_done(co,40);
					countbran_give_keys(cn,co,ppd);
				} else if (it[in].ID==IID_ARKHATA_LETTER3 && ppd && !(ppd2->letter_bits&4)) {
					quiet_say(cn,"Ahh, this is a most clever solution. Thank you once again, %s.",ch[co].name);
                                        destroy_item_byID(co,IID_ARKHATA_LETTER3);

                                        ppd2->letter_bits|=4;
				} else {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
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

struct brenneth_brannington_data
{
        int last_talk;
        int current_victim;
};	

void brenneth_brannington_driver(int cn,int ret,int lastact)
{
	struct brenneth_brannington_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_BRENNETHBRANDRIVER,sizeof(struct brenneth_brannington_data));
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
                                switch(ppd->brennethbran_state) {
					case 0:         quiet_say(cn,"Greetings stranger... I'm afraid I can't be of much help to you, as I can't recall much, except my name...");
							questlog_open(co,41);
							ppd->brennethbran_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"Perhaps you can help me, though I do not wish to burden you... You see, it appears that I have lost my memory due to being attacked by something...");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"I overheard my captor say something about a grolm before I got rescued by strangers who dropped me off here and went on their way...");
                                                        ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"Perhaps one of these... grolms... has taken something that may help me recall why I'm here... If you find anything, please bring it back to me...");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 4:		break;	// waiting for dagger
                                        case 5:		if (questlog_isdone(co,42)) { ppd->brennethbran_state=9; break; }
							quiet_say(cn,"It does have my name on it, but I don't recall anything of being a fighter...");
							questlog_open(co,42);
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 6:		quiet_say(cn,"Maybe I was foolish enough to think I could defeat grolms, and that got the best of me...");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 7:		quiet_say(cn,"If you will, please continue looking for more items... Maybe my captor managed to hide something...");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 8:		break; // waiting for potion
					case 9:		if (questlog_isdone(co,43)) { ppd->brennethbran_state=12; break; }
							quiet_say(cn,"I found something similar on my sword, and when the bartender looked at it, he said it was a lethal poison... This potion must be just that... ");
							questlog_open(co,43);
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 10:	quiet_say(cn,"I just can't imagine why I would want to poison... anyone or anything...");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 11:	break; // waiting for journal
					case 12:        quiet_say(cn,"I was to kill these thief mages were they to get out of hand... And probably even you and who knows who else...");
                                                        ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 13:	quiet_say(cn,"But I'm going to give that life up now... This loss of memory is perhaps more of a blessing than it is a curse...");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 14:	quiet_say(cn,"Thank you for helping me, I will not forget this, I hope.");
							emote(cn,"smiles");
							ppd->brennethbran_state++; didsay=1;
                                                        break;
					case 15:	break; // all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->brennethbran_state<=4) { dat->last_talk=0; ppd->brennethbran_state=0; }
							if (ppd && ppd->brennethbran_state>=5 && ppd->brennethbran_state<=8) { dat->last_talk=0; ppd->brennethbran_state=5; }
							if (ppd && ppd->brennethbran_state>=9 && ppd->brennethbran_state<=11) { dat->last_talk=0; ppd->brennethbran_state=9; }
							if (ppd && ppd->brennethbran_state>=12 && ppd->brennethbran_state<=15) { dat->last_talk=0; ppd->brennethbran_state=12; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->brennethbran_state=0; }
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

				if (it[in].ID==IID_STAFF_BRENNETHDAGGER && ppd && ppd->brennethbran_state<=4) {
                                        quiet_say(cn,"I see... so this was my dagger?");
					questlog_done(co,41);
					destroy_item_byID(co,IID_STAFF_BRENNETHDAGGER);
                                        ppd->brennethbran_state=5;
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				} else if (it[in].ID==IID_STAFF_BRENNETHPOTION && ppd && ppd->brennethbran_state>=5 && ppd->brennethbran_state<=8) {
					quiet_say(cn,"A potion? And the bottle has the same symbol on it as my sword?");
					questlog_done(co,42);
					destroy_item_byID(co,IID_STAFF_BRENNETHPOTION);
					ppd->brennethbran_state=9;
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				} else if (it[in].ID==IID_STAFF_BRENNETHJOURNAL && ppd && ppd->brennethbran_state>=9 && ppd->brennethbran_state<=11) {
					quiet_say(cn,"Now it all makes sense...	");
					questlog_done(co,43);
					destroy_item_byID(co,IID_STAFF_BRENNETHJOURNAL);
					ppd->brennethbran_state=12;
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				} else {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct spirit_brannington_data
{
        int last_talk;
        int current_victim;
};	

void spirit_brannington_driver(int cn,int ret,int lastact)
{
	struct spirit_brannington_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SPIRITBRANDRIVER,sizeof(struct spirit_brannington_data));
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
                                switch(ppd->spiritbran_state) {					
					case 0:         quiet_say(cn,"Greetings %s, I have watched thee from below here, and have now need for thy strength!",ch[co].name);
							questlog_open(co,44);
							ppd->spiritbran_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"Unbeknownst to everyone in Brannington, a Necromancer has revived their ancestors, and is trying to take over the town with his army of undead.");
							ppd->spiritbran_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"If you can retrieve the Brannington Holy Relic which the necromancer has taken, then his army will no longer obey him, and will be trapped here forever.");
                                                        ppd->spiritbran_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"Also, I wish for you to kill this Necromancer. However, in order to get into the crypt, you will have to ask Count Brannington about his jewelry. You will need his help to open the crypt doors.");
							ppd->spiritbran_state=4; didsay=1;
                                                        break;
					case 4:		break;	// waiting for holy relic
					case 5:		break;	// all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->spiritbran_state<=4) { dat->last_talk=0; ppd->spiritbran_state=0; }
                                                        break;				
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->spiritbran_state=0; }
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

				if (it[in].ID==IID_STAFF_HOLYRELIC && ppd && ppd->spiritbran_state<5) {
					int tmp;
					quiet_say(cn,"The Branningtons owe you much great hero. I give thee Ishtar's blessings... May your journeys be full of adventure and glory, %s!",ch[co].name);
					tmp=questlog_done(co,44);
					destroy_item_byID(co,IID_STAFF_HOLYRELIC);
					if (tmp==1 && ch[co].saves<3) {
						ch[co].saves++;
						log_char(co,LOG_SYSTEM,0,"You received one save.");
					}
                                        ppd->spiritbran_state=5;					
				} else {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_UP,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct forest_brannington_data
{
        int last_talk;
        int current_victim;
};	

void forest_brannington_driver(int cn,int ret,int lastact)
{
	struct forest_brannington_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_FORESTBRANDRIVER,sizeof(struct forest_brannington_data));
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
                                switch(ppd->forestbran_state) {				
					case 0:         quiet_say(cn,"Welcome %s, how are you today?",ch[co].name);
							ppd->forestbran_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"I've heard of your ventures, and thought I might tell you something that might interest you.");
							ppd->forestbran_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"While I was in the forest, I once overheard the thief mages talking about maps they have hidden on monsters.");
                                                        ppd->forestbran_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"These maps supposedly lead to treasures they have hidden in the forest. If you have one, you can give it to me, and I'll tell you where to dig to find the treasure.");
							ppd->forestbran_state=4; didsay=1;
                                                        break;
					case 4:		break;	// waiting for maps
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->forestbran_state<=4) { dat->last_talk=0; ppd->forestbran_state=0; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->forestbran_state=ppd->forestbran_done=0; }
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

				if (it[in].ID==IID_STAFF_FORESTMAP && ppd) {
					switch (ppd->forestbran_done) {
						case 0:	say(cn,"Ah, I see you have brought me a map. Let me see where this one is hidden. hhhmmm... It is beneath a dead tree...");
                                                        break;
						case 1:	say(cn,"Ah, I see you have brought me a map. Let me see where this one is hidden. hhhmmm... It is under the heat of a fire...");
							break;
						case 2:	say(cn,"Ah, I see you have brought me a map. Let me see where this one is hidden. hhhmmm... It is next to an empty bucket...");
							break;
						case 3:	say(cn,"Ah, I see you have brought me a map. Let me see where this one is hidden. hhhmmm... It is inside a circle of stones...");
							break;
						case 4:	say(cn,"Ah, I see you have brought me a map. Let me see where this one is hidden. hhhmmm... It is next to a pair of bags...");
							break;
						case 5: say(cn,"This is the first map again, I'm afraid. I think you've found all the treasures.");
							break;
					}
				} else {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct countessa_brannington_data
{
        int last_talk;
        int current_victim;
};	

void countessa_brannington_driver(int cn,int ret,int lastact)
{
	struct countessa_brannington_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_COUNTESSABRANDRIVER,sizeof(struct countessa_brannington_data));
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
                                switch(ppd->countessabran_state) {
					case 0:         if (ppd->countbran_bits&(2|8)) {
								ppd->countessabran_state=1;
								// fall through intended
							} else {
								quiet_say(cn,"Have you come here to return to us the jewelry that has been handed down from generation to generation? We would be so thankful if you did kind %s!",(ch[co].flags&CF_MALE) ? "Sir" : "Lady");
								ppd->countessabran_state++; didsay=1;
								break;
							}
					case 1:		if (ppd->countbran_bits&(2|8)) {
								ppd->countessabran_state=2;
								// fall through intended
							} else break;
					case 2:		if (ppd->countbran_bits&8) {
								ppd->countessabran_state=3;
								// fall through intended
							} else {
								quiet_say(cn,"Thank you for returning my jewelry! Let me reward you for your kindness, %s!",ch[co].name);
								ppd->countessabran_state++; didsay=1;
								ppd->countbran_bits|=8;
								give_exp(co,min(BRAN_EXP_BASE*2,level_value(ch[co].level)/4));
								give_money(co,125*100,"Count Bran Quest 2B");
								break;
							}
					case 3:         break; // all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd) { dat->last_talk=0; ppd->countessabran_state=0; }
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
                                quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
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

struct daughter_brannington_data
{
        int last_talk;
        int current_victim;
};	

void daughter_brannington_driver(int cn,int ret,int lastact)
{
	struct daughter_brannington_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_DAUGHTERBRANDRIVER,sizeof(struct daughter_brannington_data));
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
                                switch(ppd->daughterbran_state) {
					case 0:         if (ppd->countbran_bits&(4|16)) {
								ppd->daughterbran_state=1;
								// fall through intended
							} else {
								quiet_say(cn,"My jewel! My jewel! %s, please, bring me back my grandmother's jewel!",(ch[co].flags&CF_MALE) ? "Sir" : "Lady");
								ppd->daughterbran_state++; didsay=1;
								break;
							}
					case 1:		if (ppd->countbran_bits&(4|16)) {
								ppd->daughterbran_state=2;
								// fall through intended
							} else break;
					case 2:		if (ppd->countbran_bits&16) {
								ppd->daughterbran_state=3;
								// fall through intended
							} else {
								quiet_say(cn,"Oh thank you great %s, you are my hero! Let me reward you for such heroism, %s!",(ch[co].flags&CF_MALE) ? "Sir" : "Lady",ch[co].name);
								ppd->daughterbran_state++; didsay=1;
								ppd->countbran_bits|=16;
								give_exp(co,min(BRAN_EXP_BASE*2,level_value(ch[co].level)/4));
								in=create_item("lollipop");
								if (in) give_char_item(co,in);
								break;
							}
					case 3:         break; // all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd) { dat->last_talk=0; ppd->daughterbran_state=0; }
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
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
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

struct guard_brannington_data
{
        int last_talk;
        int current_victim;
};	

void guard_brannington_driver(int cn,int ret,int lastact)
{
	struct spirit_brannington_data *dat;
	struct staffer_ppd *ppd;
	struct arkhata_ppd *appd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_GUARDBRANDRIVER,sizeof(struct guard_brannington_data));
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
                                switch(ppd->guardbran_state) {					
					case 0:         if (ppd->countbran_state==0) quiet_say(cn,"Greetings stranger, welcome to the town of Brannington. If you will, the Count would like to ask for your services. We have already informed him of your arrival, and you can find him in the mansion at the end of this street.");
							ppd->guardbran_state++; didsay=1;
                                                        break;
					case 1:         if (ch[co].level>=45 && (ppd->countbran_bits&(1|2|4))==(1|2|4)) ppd->guardbran_state++;
                                                        else break;
					case 2:         quiet_say(cn,"Greetings! Count Brannington has told me thou helped him retrieve his family heirlooms. This time I must ask for thy help.");
							ppd->guardbran_state++; didsay=1;
                                                        break;
					case 3:         quiet_say(cn,"My cousin is a scout for the Count, and last night he came back in a most desperate state. He rambled about having met someone up in the mountains above the mines.");
							ppd->guardbran_state++; didsay=1;
                                                        break;
					case 4:         quiet_say(cn,"However my cousin is not an easily fooled man, and this must be investigated. I have discussed this with the Count and he agrees that I should send you as we are most convinced there actually is someone up there.");
							ppd->guardbran_state++; didsay=1;
                                                        break;
					case 5:         quiet_say(cn,"Your mission, %s is to find out who is up there, and if they are friendly or hostile.",get_army_rank_string(co));
							ppd->guardbran_state++; didsay=1;
							questlog_open(co,64);
                                                        break;
					case 6:		appd=set_data(co,DRD_ARKHATA_PPD,sizeof(struct arkhata_ppd));
							if (appd && appd->rammy_state>0) {
								ppd->guardbran_state++;
								questlog_done(co,64);
							} else break;
					case 7:		quiet_say(cn,"Excellent! The Count will be most pleased to hear this. Thank you, %s!",ch[co].name);
							ppd->guardbran_state++; didsay=1;
							break;
					case 8:		break;
					
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->guardbran_state<=1) { dat->last_talk=0; ppd->guardbran_state=0; }
							if (ppd && ppd->guardbran_state>=2 && ppd->guardbran_state<=6) { dat->last_talk=0; ppd->guardbran_state=2; }
							if (ppd && ppd->guardbran_state>=7 && ppd->guardbran_state<=8) { dat->last_talk=0; ppd->guardbran_state=7; }
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
                                quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct broklin_data
{
        int last_talk;
        int current_victim;
};

void broklin_trade_gold(int cn,int co)
{
	struct staffer_ppd *ppd;
	int in,n;

	if (!(ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) return;

	if (ppd->broklin_state<11) return;

	if (ch[co].citem) {
		say(cn,"Please free your hand (mouse cursor) first.");
		return;
	}

	for (n=30; n<INVENTORYSIZE; n++) {
		if ((in=ch[co].item[n]) && it[in].driver==IDR_ENHANCE && it[in].drdata[0]==2) {
			
			if (*(unsigned int*)(it[in].drdata+1)<1000) continue;

			*(unsigned int*)(it[in].drdata+1)-=1000;

			if (*(unsigned int*)(it[in].drdata+1)) {
                                sprintf(it[in].description,"%d units of %s.",*(unsigned int*)(it[in].drdata+1),it[in].name);
				it[in].value=(*(unsigned int*)(it[in].drdata+1))*25;
			} else {
				remove_item_char(in);
				destroy_item(in);
			}
			in=create_item("silver_2500");
			if (in) {
				if (!give_char_item(co,in)) destroy_item(in);
			}
			say(cn,"Here you go, %s.",ch[co].name);
			return;
		}
	}
	say(cn,"You need to have 1000 gold units in a single spot in your inventory, %s.",ch[co].name);
}
void broklin_trade_silver(int cn,int co)
{
	struct staffer_ppd *ppd;
	int in,n;

	if (!(ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) return;

	if (ppd->broklin_state<11) return;

	if (ch[co].citem) {
		say(cn,"Please free your hand (mouse cursor) first.");
		return;
	}

	for (n=30; n<INVENTORYSIZE; n++) {
		if ((in=ch[co].item[n]) && it[in].driver==IDR_ENHANCE && it[in].drdata[0]==1) {
			
			if (*(unsigned int*)(it[in].drdata+1)<5000) continue;

			*(unsigned int*)(it[in].drdata+1)-=5000;

			if (*(unsigned int*)(it[in].drdata+1)) {
                                sprintf(it[in].description,"%d units of %s.",*(unsigned int*)(it[in].drdata+1),it[in].name);
				it[in].value=(*(unsigned int*)(it[in].drdata+1))*10;
			} else {
				remove_item_char(in);
				destroy_item(in);
			}
			in=create_item("gold_2000");
			if (in) {
				if (!give_char_item(co,in)) destroy_item(in);
			}
			say(cn,"Here you go, %s.",ch[co].name);
			return;
		}
	}
	say(cn,"You need to have 5000 silver units in a single spot in your inventory, %s.",ch[co].name);
}

void broklin_driver(int cn,int ret,int lastact)
{
	struct broklin_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_BROKLINDRIVER,sizeof(struct broklin_data));
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
                                switch(ppd->broklin_state) {
					case 0:         quiet_say(cn,"Greetings stranger! I am Broklin, the Chief Miner. I supply the town with all its plating materials.");
							questlog_open(co,45);
							ppd->broklin_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"Well, I did before those damnable robbers broke into my house and stole my favorite pickaxe.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 2:		quiet_say(cn,"I wonder... you look like a capable adventurer - mayhaps you could take a trip to visit these robbers?");
                                                        ppd->broklin_state++; didsay=1;
                                                        break;
					case 3:		quiet_say(cn,"I suspect that one of the towns shopkeepers is involved with the robbers.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 4:		break;	// waiting for pickaxe

					case 5:		if (questlog_isdone(co,46)) { ppd->broklin_state=11; break; }
							quiet_say(cn,"Could I impose on your services some more? I would reward thee further.");
							questlog_open(co,46);
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 6:		quiet_say(cn,"The robbers infest this city...perhaps you could kill the head robber.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 7:		quiet_say(cn,"I hear his whereabouts is a great secret, but maybe you could find him.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 8:         if (!has_item(co,IID_STAFF_SEWERKEY)) {
								in=create_item("WS_Robber_Key_Area2");
								if (in) {
									if (!give_char_item(co,in)) destroy_item(in);
								}
								quiet_say(cn,"This key lets you enter the sewers under the town.");
							} else quiet_say(cn,"You already have the key for the sewers...");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 9:		quiet_say(cn,"You can begin your search there!");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 10:	break; // waiting player to kill robber's head

					case 11:	quiet_say(cn,"If you ever have need of silver or gold for plating, I can offer reasonable trade rates.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 12:	quiet_say(cn,"I can offer 2,000 gold units for 5,000 silver units.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 13:	quiet_say(cn,"Or, I can offer 2,500 silver units for 1,000 gold units.");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 14:	quiet_say(cn,"Which wouldst thou like to trade? °c4thousand gold°c0 for 2500 silver or °c4five thousand silver°c0 for 2000 gold?");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 15:	break; // waiting for repeat :P
					case 16:	quiet_say(cn,"Hail %s! Nice to see you again.",ch[co].name);
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 17:	quiet_say(cn,"Which wouldst thou like to trade? °c4thousand gold°c0 for 2500 silver or °c4five thousand silver°c0 for 2000 gold?");
							ppd->broklin_state++; didsay=1;
                                                        break;
					case 18:	break; // all done, done, done
					
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->broklin_state>=0 && ppd->broklin_state<=4) { dat->last_talk=0; ppd->broklin_state=0; }
							if (ppd && ppd->broklin_state>=5 && ppd->broklin_state<=10) { dat->last_talk=0; ppd->broklin_state=5; }
							if (ppd && ppd->broklin_state>=11 && ppd->broklin_state<=19) { dat->last_talk=0; ppd->broklin_state=16; }
                                                        break;				
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->broklin_state=0; }
							break;
					case 4:		broklin_trade_gold(cn,co); break;
					case 5:		broklin_trade_silver(cn,co); break;
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

				if (it[in].ID==IID_STAFF_PICKAXE && ppd && ppd->broklin_state<=4) {
					int tmp;
					tmp=questlog_done(co,45);
					destroy_item_byID(co,IID_STAFF_PICKAXE);
					destroy_item_byID(co,IID_STAFF_ROBBERKEYAREA1);

                                        if (tmp==1) {
						quiet_say(cn,"Thank you! Take these 2,000 gu - I am sure it will be useful to you.");
						in=create_item("gold_2000");
						if (in) {
							if (!give_char_item(co,in)) destroy_item(in);						
						}
					} else quiet_say(cn,"Thank you!");
                                        ppd->broklin_state=5;					
				} else {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
					ch[cn].citem=0;
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct grinnich_data
{
        int last_talk;
        int current_victim;
};	

void grinnich_driver(int cn,int ret,int lastact)
{
	struct grinnich_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_GRINNICHDRIVER,sizeof(struct grinnich_data));
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
                                switch(ppd->grinnich_state) {
					case 0:         quiet_say(cn,"Oh my! What brings the likes of you to this hermit's home? Adventure? Treasure? You sure do look like an adventurer... Well, if it's adventure you seek, then adventure you get! You see, this here is no ordinary place... It's a tower!");
							ppd->grinnich_state++; didsay=1;
                                                        break;
                                        case 1:		quiet_say(cn,"Oh no, I'm not crazy dear %s! It is! It's just buried into the ground. Find the entrance that leads to the tower, and venture deep into the earth. I tell you, you will not regret it at all!",Sirname(co));
							ppd->grinnich_state++; didsay=1;
                                                        break;
					case 2:		break; // waiting for completion of quest
					case 3:		quiet_say(cn,"Didn't I tell you it was worth it? Oh, all that knowledge just makes the mind grow! Isn't Shanra wonderful? She can tell so many stories, more even than those books of her can...");
                                                        ppd->grinnich_state++; didsay=1;
                                                        break;
					case 4:         break; // all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->grinnich_state<=2) { dat->last_talk=0; ppd->grinnich_state=0; }
							if (ppd && ppd->grinnich_state>=3 && ppd->grinnich_state<=4) { dat->last_talk=0; ppd->grinnich_state=3; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->grinnich_state=0; }
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
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;				
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct shanra_data
{
        int last_talk;
        int current_victim;
};	

void shanra_driver(int cn,int ret,int lastact)
{
	struct shanra_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_SHANRADRIVER,sizeof(struct shanra_data));
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
                                switch(ppd->shanra_state) {
					case 0:         quiet_say(cn,"Welcome adventurer, Grinnich told me you were coming. You have made it through my tower, and so I will reward you. You will have to work your way through some sentinels to find the Grimoire of Animation");
							ppd->shanra_state++; didsay=1;
                                                        break;
					case 1:		quiet_say(cn,"I will now teleport you to the basement.");
							teleport_char_driver(co,5,106);
							ppd->shanra_state++; didsay=1;
							break;
					case 2:		break;
					case 3:         quiet_say(cn,"Well done! It is good to see others learning about animation. I can't teach you how to use it though, the magic is ancient, and takes a long time to learn and control. I will now send you back to the ruins above.");
                                                        ppd->shanra_state++; didsay=1;
                                                        break;
					case 4:         teleport_char_driver(co,53,129);
							ppd->shanra_state++; didsay=1;
							break;
					case 5:		break; // all done
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

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->shanra_state<=2) { dat->last_talk=0; ppd->shanra_state=0; }
							if (ppd && ppd->shanra_state>=3 && ppd->shanra_state<=5) { dat->last_talk=0; ppd->shanra_state=3; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->shanra_state=0; }
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
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
				
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

void centinel_dead(int cn,int co)
{
	struct staffer_ppd *ppd;

	if (!(ch[co].flags&CF_PLAYER)) return;

	if (!(ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) return;

	ppd->centinel_count++;
	if (ppd->centinel_count>30) ppd->centinel_count=30;
	
	switch(ppd->centinel_count) {
		case 1:		log_char(co,LOG_SYSTEM,0,"You have killed the first sentinel on this floor, kill 29 more!"); break;
		case 10:	log_char(co,LOG_SYSTEM,0,"You have killed 10 sentinels, 20 more to go!"); break;
		case 20:	log_char(co,LOG_SYSTEM,0,"You have killed 20 sentinels, 10 more to go!"); break;
		case 30:	log_char(co,LOG_SYSTEM,0,"Congratulations, you have killed 30 sentinels! Continue your journey.");
				if (teleport_char_driver(co,33,143)) ppd->centinel_count=0;
				break;
	}
}


int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_STAFFER2:	return staffer2_item(in,cn);

                default:	return 0;
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_COUNTBRAN:	count_brannington_driver(cn,ret,lastact); return 1;
		case CDR_COUNTESSABRAN:	countessa_brannington_driver(cn,ret,lastact); return 1;
		case CDR_DAUGHTERBRAN:	daughter_brannington_driver(cn,ret,lastact); return 1;
		case CDR_SPIRITBRAN:	spirit_brannington_driver(cn,ret,lastact); return 1;
		case CDR_GUARDBRAN:	guard_brannington_driver(cn,ret,lastact); return 1;
		case CDR_BRENNETHBRAN:	brenneth_brannington_driver(cn,ret,lastact); return 1;
		case CDR_FORESTBRAN:	forest_brannington_driver(cn,ret,lastact); return 1;
		case CDR_BROKLIN:      	broklin_driver(cn,ret,lastact); return 1;
		case CDR_GRINNICH:	grinnich_driver(cn,ret,lastact); return 1;
		case CDR_SHANRA:	shanra_driver(cn,ret,lastact); return 1;
		case CDR_CENTINEL:	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;


                default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_CENTINEL:	centinel_dead(cn,co); return 1;

                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_CENTINEL:	return 1;

                default:	return 0;
	}
}














