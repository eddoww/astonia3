/*
 *
 * $Id: base.c,v 1.10 2008/03/25 09:53:28 devel Exp devel $
 *
 * $Log: base.c,v $
 * Revision 1.10  2008/03/25 09:53:28  devel
 * small bugfix in macro
 *
 * Revision 1.9  2008/03/24 11:19:10  devel
 * meaner macro daemon
 *
 * Revision 1.8  2007/12/10 10:08:00  devel
 * no more bigspawn
 *
 * Revision 1.7  2006/12/19 15:19:35  devel
 * fixed lost item bug in xmastree
 *
 * Revision 1.6  2006/12/14 13:58:05  devel
 * added xmass special
 *
 * Revision 1.5  2006/04/26 16:05:44  ssim
 * added hack for teufelheim arena: no potions allowed
 *
 * Revision 1.4  2005/11/22 12:16:46  ssim
 * added stackable (demon) chips
 *
 * Revision 1.3  2005/10/17 11:17:53  ssim
 * added logging to macro daemon
 *
 * Revision 1.2  2005/09/25 15:29:25  ssim
 * added check to trader's "accept trade" to prevent some scams
 *
 * Revision 1.1  2005/09/24 09:48:53  ssim
 * Initial revision
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
#include "sector.h"
#include "lab.h"
#include "player.h"
#include "area.h"
#include "misc_ppd.h"
#include "consistency.h"
#include "act.h"
#include "staffer_ppd.h"
#include "transport.h"

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
        {{"what's","your","name",NULL},NULL,1},
	{{"what","is","your","name",NULL},NULL,1},
        {{"who","are","you",NULL},NULL,1},
	{{"trade",NULL},"I am not a normal merchant. Talk to Fred in Cameron or Jeremy in Aston instead.",0},
	{{"buy",NULL},"I am not a normal merchant. Talk to Fred in Cameron or Jeremy in Aston instead.",0},
	{{"sell",NULL},"I am not a normal merchant. Talk to Fred in Cameron or Jeremy in Aston instead.",0},
	{{"help",NULL},"To start trading with someone, say: 'trade with <name>'. Then you hand me the items you wish to exchange. You can stop the deal at any time by saying: 'stop trade'. To check what items I am holding, say: 'show trade'. When you are satisfied with the deal, say 'accept trade'. Both parties must accept the deal to make it take place.",1},
	{{"repeat",NULL},"To start trading with someone, say: 'trade with <name>'. Then you hand me the items you wish to exchange. You can stop the deal at any time by saying: 'stop trade'. To check what items I am holding, say: 'show trade'. When you are satisfied with the deal, say 'accept trade'. Both parties must accept the deal to make it take place.",1}
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
				else switch(qa[q].answer_code) {
					     case 1:	quiet_say(cn,"I'm %s.",ch[cn].name); break;
				}
				break;
			}
		}
	}
	

        return 42;
}

struct macro_data
{
        int victim;
	int v_ID;
	int state;
	int start;
	int last;
	int val1,val2;
};

struct macro_ppd
{
	int nextcheck;	// realtime
	int karma;	
};

int macro_set_char(int cn,int x,int y,int nosteptrap)
{
	if (map[x+y*MAXMAP].flags&(MF_SOUNDBLOCK|MF_SHOUTBLOCK)) return 0;
	return set_char(cn,x,y,nosteptrap);	
}

int macro_drop_char(int cn,int x,int y,int nosteptrap)
{
	if (macro_set_char(cn,x,y,nosteptrap)) return 1;

	// direct neighbors
	if (x<MAXMAP-1 && macro_set_char(cn,x+1,y,nosteptrap)) return 1;
	if (y<MAXMAP-1 && macro_set_char(cn,x,y+1,nosteptrap)) return 1;
	if (x>1 && macro_set_char(cn,x-1,y,nosteptrap)) return 1;
	if (y>1 && macro_set_char(cn,x,y-1,nosteptrap)) return 1;

	// diagonal neighbors
	if (x<MAXMAP-1 && y<MAXMAP-1 && macro_set_char(cn,x+1,y+1,nosteptrap)) return 1;
	if (x>1 && y<MAXMAP-1 && macro_set_char(cn,x-1,y+1,nosteptrap)) return 1;
	if (x<MAXMAP-1 && y>1 && macro_set_char(cn,x+1,y-1,nosteptrap)) return 1;
	if (x>1 && y>1 && macro_set_char(cn,x-1,y-1,nosteptrap)) return 1;

	return 0;
}

int macro_teleport_char_driver(int cn,int x,int y)
{
	int oldx,oldy;

	if (abs(ch[cn].x-x)+abs(ch[cn].y-y)<2) return 0;

	oldx=ch[cn].x;
	oldy=ch[cn].y;

	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;

	if (macro_drop_char(cn,x,y,0)) return 1;

	drop_char(cn,oldx,oldy,0);

	return 0;
}

void macro_driver(int cn,int ret,int lastact)
{
	struct macro_data *dat;
	struct macro_ppd *ppd;
        int co,talkdir=0,val;
	struct msg *msg,*next;
	char *text;

        dat=set_data(cn,DRD_MACRODRIVER,sizeof(struct macro_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (co==dat->victim && ch[co].ID==dat->v_ID) {
				
				text=(char*)(msg->dat2);
				
				while (isalpha(*text)) text++;
				while (isspace(*text)) text++;
				while (isalpha(*text)) text++;
				if (*text==':') text++;
				while (isspace(*text)) text++;
				if (*text=='"') text++;
				
				val=atoi(text);
				
				if (val==dat->val1+dat->val2) {
					say(cn,"Very well, %s.",ch[co].name);
					ppd=set_data(co,DRD_MACRO_PPD,sizeof(struct macro_ppd));
					dlog(co,0,"answered macro correctly");
					if (ppd) {
						ppd->karma=ppd->karma*0.9+10;
						ppd->nextcheck=realtime+60*5+RANDOM(60*60);
						if (ppd->karma>12) ppd->nextcheck+=RANDOM(60*60*2);
						if (ppd->karma>25) ppd->nextcheck+=RANDOM(60*60*3);
						if (ppd->karma>50) ppd->nextcheck+=RANDOM(60*60*4);
						if (ppd->karma>65) ppd->nextcheck+=RANDOM(60*60*5);
						if (ppd->karma>75) ppd->nextcheck+=RANDOM(60*60*6);
					}
					if (isxmas) {
						int in;

						in=create_item("xmaspop");
						if (in) {
							if (!give_char_item(co,in)) destroy_item(in);
							else say(cn,"Merry Christmas, %s!",ch[co].name);
						}
					} else if (RANDOM(20)==0) {
						say(cn,"Experience is a nice thing, isn't it?");
						give_exp(co,level_value(ch[co].level)/20+1);
					}
					dat->victim++;
					dat->state=0;
				} else if (val) {
					dlog(co,0,"answered macro wrongly: '%s' (%d)",text,dat->val1+dat->val2);
					say(cn,"That's wrong, %s. I'll reduce your time by 30 seconds.",ch[co].name);
					dat->start-=TICKS*30;
				}
				tabunga(cn,co,(char*)(msg->dat2));
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
                        if (ch[cn].citem) {
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
	if (isxmas) {
		strcpy(ch[cn].name,"Saint Nick");
		strcpy(ch[cn].description,"A fat man in red with a ridiculous white beard.");
		ch[cn].sprite=13;
	} else {
		strcpy(ch[cn].name,"Macro Daemon");
		strcpy(ch[cn].description,"Your friendly Macro Daemon.");
		ch[cn].sprite=161;
	}

        if (dat->state==0) {		// no victim yet
		for (co=dat->victim; co<MAXCHARS; co++) {
			if (!(ch[co].flags&CF_PLAYER)) continue;
                        if (ch[co].level<5) continue;									// dont bug newbies
			if (realtime-ch[co].login_time<60*5) continue;							// dont ask directly after login
			// removed if (ch[co].driver==CDR_LOSTCON && !(ch[co].flags&CF_LAG)) continue;				// dont try disconnected players, but try those who're just testing lag control
			if ((ch[co].flags&CF_INVISIBLE)) continue;							// dont try invisible players
			if (areaID==3 && get_section(ch[co].x,ch[co].y)!=20) continue;					// no asking in aston, unless in palace
			if (get_section(ch[co].x,ch[co].y)==114) continue;						// no asking in UW lab
                        if (areaID==22 && ch[co].x>=76 && ch[co].y>=19 && ch[co].x<=94 && ch[co].y<=37) continue;	// no asking in regen lab ritual room

			ppd=set_data(co,DRD_MACRO_PPD,sizeof(struct macro_ppd));
			if (!ppd) continue;

                        if (realtime>=ppd->nextcheck) break;
		}
		if (co==MAXCHARS) {	// none found, sleep
                        dat->victim=1;
			teleport_char_driver(cn,ch[cn].tmpx,ch[cn].tmpy);
			do_idle(cn,TICKS);
			return;
		}
		dat->victim=co;
		dat->v_ID=ch[co].ID;
		dat->state=1;
	}
	if (dat->state==1) {		// teleport to victim
		co=dat->victim;
		if (ch[co].ID==dat->v_ID && macro_teleport_char_driver(cn,ch[co].x,ch[co].y)) {
			dat->state=2;	// we're close to him now, go on
		} else {
			dat->victim++;	// teleport didnt work, sleep and cycle to next victim
			dat->state=0;
			do_idle(cn,TICKS);
			return;
		}
	}
	if (dat->state==2) {
		co=dat->victim;
		if (ch[co].ID!=dat->v_ID) {
			dat->victim++;
			dat->state=0;
			do_idle(cn,TICKS);
			return;
		}
		dat->start=ticker;
		dat->last=dat->start-TICKS*31;
                dat->val1=RANDOM(5000)+1;
		dat->val2=RANDOM(6)+1;
		dat->state=3;
		
		say(cn,"Hello, %s. I'm %s. I'll ask you a question. If you do not answer correctly within 5 minutes, I'll punish you.",ch[co].name,ch[cn].name);
		talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
	}
	if (dat->state==3) {
		co=dat->victim;
		if (ch[co].ID!=dat->v_ID || !char_see_char(cn,co)) {
			dat->state=0;
			dat->victim++;
			do_idle(cn,TICKS);
			return;
		}
		if (ticker-dat->start>TICKS*60*7) {
			dat->state=4;
		} else if (ticker-dat->last>TICKS*30) {
			say(cn,"Now, %s, answer me this: What is %d plus %d?",ch[co].name,dat->val1,dat->val2);
			talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
			dat->last=ticker;
			dlog(co,0,"asked by macro: %d + %d",dat->val1,dat->val2);
		}
	}
	if (dat->state==4) {
		co=dat->victim;
		if (ch[co].ID!=dat->v_ID) {
			dat->victim++;
			dat->state=0;
			do_idle(cn,TICKS);
			return;
		}
		say(cn,"Your time is up. Sorry, pal.");
		talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);

		ppd=set_data(co,DRD_MACRO_PPD,sizeof(struct macro_ppd));
		if (ppd) {
			ppd->karma-=50;
			ppd->nextcheck=realtime+5*60+RANDOM(5*60);
		}
		task_punish_player(ch[co].ID,0,1,"Using macros");
		dat->state=0;
		dat->victim++;
		dlog(co,0,"punished by macro");
	}

        if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}


void potion_driver(int in,int cn)
{
	int in2=0,empty;
	char buf[80];

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it
	if (!it[in].carried) return;

	if (areaID==33) {	// long tunnel: no potions!
		log_char(cn,LOG_SYSTEM,0,"You sense that the potion would not work.");
		return;
	}

	// no potions in teufelheim arena
	if (areaID==34 && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) {
		log_char(cn,LOG_SYSTEM,0,"You sense that the potion would not work.");
		return;
	}

	if ((empty=it[in].drdata[0])) {
		sprintf(buf,"empty_potion%d",empty);
		in2=create_item(buf);
		if (!in2) return;
	}

	log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s drinks a potion.",ch[cn].name);

	ch[cn].hp=min(ch[cn].hp+it[in].drdata[1]*POWERSCALE,ch[cn].value[0][V_HP]*POWERSCALE);
	ch[cn].mana=min(ch[cn].mana+it[in].drdata[2]*POWERSCALE,ch[cn].value[0][V_MANA]*POWERSCALE);
	ch[cn].endurance=min(ch[cn].endurance+it[in].drdata[3]*POWERSCALE,ch[cn].value[0][V_ENDURANCE]*POWERSCALE);

	if (empty) replace_item_char(in,in2);
	else remove_item_char(in);
	
	free_item(in);
}

int door_driver(int in,int cn)
{
	int m,in2,n;

	if (!it[in].x) return 2;

	if (!cn) {	// called by timer
		if (it[in].drdata[39]) it[in].drdata[39]--;	// timer counter
                if (!it[in].drdata[0]) return 2;		// if the door is closed already, don't open it again
		if (it[in].drdata[39]) return 2;		// we have more outstanding timer calls, dont close now
	}

	if (!cn && it[in].drdata[5]) return 2;	// no auto-close for this door

        m=it[in].x+it[in].y*MAXMAP;

	if (it[in].drdata[0] && (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) {	// doorway is blocked
		if (!cn) {	// timer callback - restart
			it[in].drdata[39]++;	// timer counter
			if (!it[in].drdata[5]) call_item(2,in,0,ticker+TICKS*5);			
		}
		return 2;
	}

	// if the door needs a key, check if the character has it
	if (cn && (it[in].drdata[1] || it[in].drdata[2])) {
		for (n=30; n<INVENTORYSIZE; n++)
			if ((in2=ch[cn].item[n]))
				if (*(unsigned int*)(it[in].drdata+1)==it[in2].ID) break;
		if (n==INVENTORYSIZE) {
			if (!(in2=ch[cn].citem) || *(unsigned int*)(it[in].drdata+1)!=it[in2].ID) {
				log_char(cn,LOG_SYSTEM,0,"You need a key to use this door.");
				return 2;
			}
		}
		log_char(cn,LOG_SYSTEM,0,"You use %s to %slock the door.",it[in2].name,it[in].drdata[0] ? "" :  "un");
	}

	remove_lights(it[in].x,it[in].y);

	if (cn) sound_area(it[in].x,it[in].y,3);
	else sound_area(it[in].x,it[in].y,2);

        if (it[in].drdata[0]) {	// it is open, close
		it[in].flags|=*(unsigned long long*)(it[in].drdata+30);
		if (it[in].flags&IF_MOVEBLOCK) map[m].flags|=MF_TMOVEBLOCK;
		if (it[in].flags&IF_SIGHTBLOCK) map[m].flags|=MF_TSIGHTBLOCK;
		if (it[in].flags&IF_SOUNDBLOCK) map[m].flags|=MF_TSOUNDBLOCK;
		if (it[in].flags&IF_DOOR) map[m].flags|=MF_DOOR;
		it[in].drdata[0]=0;
		it[in].sprite--;

		if (it[in].drdata[7]) {		// extended door covering three tiles?
			m=it[in].x+it[in].y*MAXMAP;
			if (map[m+1].fsprite) map[m+1].fsprite--;
			if (map[m-1].fsprite) map[m-1].fsprite--;
			if (map[m+MAXMAP].fsprite) map[m+MAXMAP].fsprite--;
			if (map[m-MAXMAP].fsprite) map[m-MAXMAP].fsprite--;		
		}
	} else { // it is closed, open
		*(unsigned long long*)(it[in].drdata+30)=it[in].flags&(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		it[in].flags&=~(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		map[m].flags&=~(MF_TMOVEBLOCK|MF_TSIGHTBLOCK|MF_DOOR|MF_TSOUNDBLOCK);
		it[in].drdata[0]=1;
		it[in].sprite++;

		if (it[in].drdata[7]) {		// extended door covering three tiles?
			m=it[in].x+it[in].y*MAXMAP;
			if (map[m+1].fsprite) map[m+1].fsprite++;
			if (map[m-1].fsprite) map[m-1].fsprite++;
			if (map[m+MAXMAP].fsprite) map[m+MAXMAP].fsprite++;
			if (map[m-MAXMAP].fsprite) map[m-MAXMAP].fsprite++;		
		}

		it[in].drdata[39]++;	// timer counter
		if (!it[in].drdata[5]) call_item(2,in,0,ticker+TICKS*10);		
	}

        reset_los(it[in].x,it[in].y);
        if (!it[in].drdata[38] && !reset_dlight(it[in].x,it[in].y)) it[in].drdata[38]=1;
	add_lights(it[in].x,it[in].y);

	return 1;
}

void double_door_driver(int in,int cn)
{
        int in2;

        door_driver(in,cn);
        if ((in2=map[(it[in].x)+(it[in].y+1)*MAXMAP].it) && it[in].drdata[0]!=it[in2].drdata[0]) door_driver(in2,cn);
        if ((in2=map[(it[in].x)+(it[in].y-1)*MAXMAP].it) && it[in].drdata[0]!=it[in2].drdata[0]) door_driver(in2,cn);
        if ((in2=map[(it[in].x+1)+(it[in].y)*MAXMAP].it) && it[in].drdata[0]!=it[in2].drdata[0]) door_driver(in2,cn);
        if ((in2=map[(it[in].x-1)+(it[in].y)*MAXMAP].it) && it[in].drdata[0]!=it[in2].drdata[0]) door_driver(in2,cn);
}

void balltrap(int in,int cn)
{
	int dx,dy,power,fn,dxs,dys;

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	if (ch[cn].flags&CF_PLAYER) return;	// dont allow players to fire the cannon

	dx=it[in].drdata[0]-128;
	dy=it[in].drdata[1]-128;
	power=it[in].drdata[2];

	if (dx>0) dxs=1;
	else if (dx<0) dxs=-1;
	else dxs=0;

	if (dy>0) dys=1;
	else if (dy<0) dys=-1;
	else dys=0;

	fn=create_ball(0,it[in].x+dxs,it[in].y+dys,it[in].x+dx,it[in].y+dy,power);	
}

struct treasure_chest_ppd
{
	int last_access[200];
};

void chest_driver(int in,int cn)
{
	struct treasure_chest_ppd *ppd;
	int nr,in2,n,timeout;
	char name[80];

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	/*if (it[in].min_level>ch[cn].level) {
		log_char(cn,LOG_SYSTEM,0,"You're not strong enough to use this.");
		return;
	}*/

	// get chest number
        nr=it[in].drdata[0];
	if (nr>=sizeof(ppd->last_access)/sizeof(int)) {
		elog("treasure_chest item driver: chest number (drdata[0]) must be below %d!",sizeof(ppd->last_access)/sizeof(int));
		return;	
	}

	// if the chest needs a key, check if the character has it
	if (it[in].drdata[1] || it[in].drdata[2]) {
		for (n=30; n<INVENTORYSIZE; n++)
			if ((in2=ch[cn].item[n]))
				if (*(unsigned int*)(it[in].drdata+1)==it[in2].ID) break;
		if (n==INVENTORYSIZE) {
			if (!(in2=ch[cn].citem) || *(unsigned int*)(it[in].drdata+1)!=it[in2].ID) {
				log_char(cn,LOG_SYSTEM,0,"You need a key to open this chest.");
				return;
			}
		}
		log_char(cn,LOG_SYSTEM,0,"You use %s to unlock the chest.",it[in2].name);
	}

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your 'hand' (mouse cursor) first.");
		return;
	}

	ppd=set_data(cn,DRD_TREASURE_CHEST_PPD,sizeof(struct treasure_chest_ppd));

	timeout=(*(unsigned short*)(it[in].drdata+5))*60*60;

        if (ppd->last_access[nr] && ppd->last_access[nr]+timeout>realtime) {
		log_char(cn,LOG_SYSTEM,0,"The chest is empty.");
		return;
	}

	// after-first-death chests in area 1
	if (it[in].drdata[7] && ch[cn].deaths<it[in].drdata[7]) {
		log_char(cn,LOG_SYSTEM,0,"The chest is empty.");
		return;
	}

	sprintf(name,"treasure_%d",nr);
	in2=create_item(name);
	if (!in2) {
		elog("treasure_chest item driver: could not create treasure '%s'",name);
		log_char(cn,LOG_SYSTEM,0,"The chest is empty.");
		return;			
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from treasure chest");

	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;

	ppd->last_access[nr]=realtime;

	log_char(cn,LOG_SYSTEM,0,"You got a %s.",it[in2].name);
}

void usetrap(int in,int cn)
{
	int in2,m,x,y;

	if (!cn) return;

	x=it[in].drdata[0];
	y=it[in].drdata[1];

        if (x<0 || x>=MAXMAP || y<0 || y>=MAXMAP) return;

	m=x+y*MAXMAP;

        if (!(in2=map[m].it)) return;

        call_item(it[in2].driver,in2,cn,ticker+TICKS/2);
}

void steptrap(int in,int cn)
{
	int in2,m,x,y,dir;

	if (!cn) {	// use auto-call to set target
		if (!it[in].drdata[0]) {
			for (dir=1; dir<9; dir+=2) {
				dx2offset(dir,&x,&y,NULL);
				x+=it[in].x; y+=it[in].y;
				if (x>0 && x<MAXMAP && y>0 && y<MAXMAP) {
					m=x+y*MAXMAP;
					if ((in2=map[m].it) && it[in2].driver && it[in2].driver!=IDR_STEPTRAP) break;
				}
				dx2offset(dir,&x,&y,NULL);
				x=it[in].x+x*2; y=it[in].y+y*2;
				if (x>0 && x<MAXMAP && y>0 && y<MAXMAP) {
					m=x+y*MAXMAP;
					if ((in2=map[m].it) && it[in2].driver && it[in2].driver!=IDR_STEPTRAP) break;
				}
			}
			if (dir<9) {
				it[in].drdata[0]=x;
				it[in].drdata[1]=y;				
			} else {
				elog("steptrap at %d,%d: couldnt find target!",it[in].x,it[in].y);
			}
		}
		return;
	}

	x=it[in].drdata[0];
	y=it[in].drdata[1];

        if (x<0 || x>=MAXMAP || y<0 || y>=MAXMAP) return;

	m=x+y*MAXMAP;

        if (!(in2=map[m].it)) return;

        call_item(it[in2].driver,in2,0,ticker+1);
}

void toylight_driver(int in,int cn)
{
	int light;

        if (!cn) return;	// we dont handle timer calls

	if (it[in].drdata[0]) {
		
		light=it[in].drdata[1];

		remove_item_light(in);

                it[in].drdata[0]=0;
                it[in].mod_value[0]=0;
		it[in].sprite--;
	} else {

		light=it[in].drdata[1];

		it[in].drdata[0]=1;
		it[in].mod_value[0]=light;
		it[in].sprite++;

		add_item_light(in);		
	}
}

void nightlight_driver(int in,int cn)
{
	int light;

        if (cn) return;		// we handle ONLY timer calls

	if (it[in].drdata[0] && dlight>80) {
		
		light=it[in].drdata[1];

		remove_item_light(in);

                it[in].drdata[0]=0;
                it[in].mod_value[0]=0;
		it[in].sprite--;
		
                //add_light(it[in].x,it[in].y,-light,0);
	}

	if (!it[in].drdata[0] && dlight<80) {

		light=it[in].drdata[1];

		it[in].drdata[0]=1;
		it[in].mod_value[0]=light;
		it[in].sprite++;

		add_item_light(in);
		//add_light(it[in].x,it[in].y,light,0);
	}

	call_item(IDR_NIGHTLIGHT,in,0,ticker+TICKS*30);
}

void torch_driver(int in,int cn)
{
	int n;

        if (!cn) {	// timer call
		for (n=0; n<MAXMOD; n++) {	// is the torch orbed?
			if (it[in].mod_index[n]!=V_LIGHT && it[in].mod_value[n]>0 && it[in].mod_index[n]>=0 && it[in].min_level!=200) {
				it[in].min_level=200;
				if (it[in].carried) dlog(it[in].carried,in,"set min level to 200");
			}
		}
                if (it[in].drdata[0]) {	// torch burning?
			
			if ((cn=it[in].carried)) {
                                if (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_UNDERWATER) {
					log_char(cn,LOG_SYSTEM,0,"Your hear your torch hiss.");
					it[in].drdata[0]=0;
					it[in].mod_value[0]=0;
					it[in].sprite++;
					it[in].flags&=(~IF_NODECAY);
					update_char(cn);
					ch[cn].flags|=CF_ITEMS;
					call_item(IDR_TORCH,in,0,ticker+TICKS*30);
					return;
				}
			}

			it[in].drdata[1]++;
			if (it[in].drdata[1]>it[in].drdata[2]) {				
				if (it[in].carried) log_char(it[in].carried,LOG_SYSTEM,0,"Your %s expired.",it[in].name);
				if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it expired");
                                remove_item(in);
				destroy_item(in);
                                return;
			}
			if (it[in].x) remove_item_light(in);

			it[in].mod_index[0]=V_LIGHT;
			it[in].mod_value[0]=min(it[in].drdata[3],it[in].drdata[3]*it[in].drdata[2]/(it[in].drdata[1]+1)/2);
			
			if (it[in].carried) {
				update_char(it[in].carried);				
			} else if (it[in].x) add_item_light(in);

                        call_item(IDR_TORCH,in,0,ticker+TICKS*30);
		}		
	} else {
		if (it[in].x) return;

		for (n=0; n<MAXMOD; n++) {	// remove any non-light modifiers and turn them into orbs
			int in2;

			if (it[in].mod_index[n]!=V_LIGHT && it[in].mod_value[n]>0 && it[in].mod_index[n]>=0 && (in2=create_orb2(it[in].mod_index[n]))) {
				if (give_char_item(cn,in2)) {
					it[in].mod_value[n]--;
					if (cn) dlog(cn,in,"created %s from torch",it[in2].name);
				} else destroy_item(in2);
				return;
			}			
		}

		if (it[in].drdata[0]) {	// torch burning?
			it[in].drdata[0]=0;
			it[in].mod_value[0]=0;
			it[in].sprite++;
			it[in].flags&=(~IF_NODECAY);
			update_char(cn);
			ch[cn].flags|=CF_ITEMS;
		} else {
                        if (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_UNDERWATER) {
                                log_char(cn,LOG_SYSTEM,0,"Obviously, thou canst not light thy torch under water.");
                                return;
                        }
			
                        it[in].drdata[0]=1;
			it[in].mod_index[0]=V_LIGHT;
			it[in].mod_value[0]=min(it[in].drdata[3],it[in].drdata[3]*it[in].drdata[2]/(it[in].drdata[1]+1)/2);
			it[in].sprite--;
			it[in].flags|=IF_NODECAY;
			update_char(cn);
			ch[cn].flags|=CF_ITEMS;
			call_item(IDR_TORCH,in,0,ticker+TICKS*30);
		}
	}
}

void recall_driver(int in,int cn)
{
	int oldx,oldy;

	if (!cn) return;		//  no timer calls please
	if (!it[in].carried) return;	// can only use if item is carried

	if (ch[cn].action==AC_DIE) return;	// already dying, cannot use scroll...

	if (ch[cn].level>it[in].drdata[0]) {	// above rank restriction
		log_char(cn,LOG_SYSTEM,0,"This scroll is too weak to transport someone of your power.");
		return;
	}

	// no recalls in teufelheim arena
	if (areaID==34 && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) {
		log_char(cn,LOG_SYSTEM,0,"You sense that the scroll would not work.");
		return;
	}
	
        log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s uses a scroll of recall and vanishes.",ch[cn].name);

        if (ch[cn].resta!=areaID) {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
		remove_item(in);
                if (!change_area(cn,ch[cn].resta,ch[cn].restx,ch[cn].resty)) {
			log_char(cn,LOG_SYSTEM,0,"Nothing happens - target area server is down.");
			// give item back to char!!
		}
		destroy_item(in);
                return;
	}

	oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;
	player_driver_stop(ch[cn].player,0);

	if (!drop_char(cn,ch[cn].restx,ch[cn].resty,0)) {
		log_char(cn,LOG_SYSTEM,0,"Please try again soon. Target is busy");
		drop_char(cn,oldx,oldy,0);
	} else {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
		remove_item(in);
		destroy_item(in);
	}
}

void teleport_driver(int in,int cn)
{
	int x,y,a,oldx,oldy,aflag,branflag,stopflag;

        if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	x=*(unsigned short*)(it[in].drdata+0);
	y=*(unsigned short*)(it[in].drdata+2);
	a=*(unsigned short*)(it[in].drdata+4);
	aflag=*(unsigned char*)(it[in].drdata+10);
	branflag=*(unsigned char*)(it[in].drdata+11);
	stopflag=*(unsigned char*)(it[in].drdata+12);

	if (branflag) {
		struct transport_ppd *dat;
		dat=set_data(cn,DRD_TRANSPORT_PPD,sizeof(struct transport_ppd));
		if (!dat) return;	// oops...

		if (!(dat->seen&(1ull<<22)) || !(ch[cn].flags&CF_ARCH)) {
			log_char(cn,LOG_SYSTEM,0,"You've never been to the Brannington Transport. You may not pass.");
			return;
		}
	}

	if (aflag && !(ch[cn].flags&CF_ARCH)) {
		log_char(cn,LOG_SYSTEM,0,"This door will only allow Arches to pass.");
		return;
	}
	if (it[in].max_level && ch[cn].level>it[in].max_level) {
		log_char(cn,LOG_SYSTEM,0,"This door will only allow level %d or lower to pass.",it[in].max_level);
		return;
	}
	if (ch[cn].level<it[in].min_level) {
		log_char(cn,LOG_SYSTEM,0,"This door will only allow level %d or higher to pass.",it[in].min_level);
		return;
	}

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d,%d).",ch[cn].name,x,y,a);
		return;
	}

        if (a) {
		if (!(ch[cn].flags&CF_PLAYER)) return;
		
		if (!change_area(cn,a,x,y)) {
			log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - target area server is down.",ch[cn].name);
		}
                return;
	}

        if (!it[in].drdata[6]) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a magic object and vanishes.",ch[cn].name);

	oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);
	
	if (!drop_char_extended(cn,x,y,6)) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,1);
	}

	if (!it[in].drdata[6]) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s pops in.",ch[cn].name);
	if (stopflag) { if (ch[cn].player) player_driver_stop(ch[cn].player,0); }
}

void teleport_door_driver(int in,int cn)
{
	int x,y,oldx,oldy,dx,dy,co;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
		it[in].max_level=it[in].drdata[1];
		return;
	}

	dx=(ch[cn].x-it[in].x);
	dy=(ch[cn].y-it[in].y);

	if (dx && dy) return;

	if (it[in].drdata[0]==1 && dx==1) return;
	if (it[in].drdata[0]==2 && dx==-1) return;
	if (it[in].drdata[0]==3 && dy==1) return;
	if (it[in].drdata[0]==4 && dy==-1) return;

	if (it[in].drdata[1] && ch[cn].level>it[in].drdata[1]) {
		log_char(cn,LOG_SYSTEM,0,"This door can only be used by characters of level %d or below.",it[in].drdata[1]);
		return;
	}

	if (it[in].drdata[1] && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_CLAN)) {
		log_char(cn,LOG_SYSTEM,0,"This door can only be used to enter clan areas.");
		return;
	}

	x=it[in].x-dx;
	y=it[in].y-dy;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

	if (it[in].drdata[1] && (map[x+y*MAXMAP].flags&MF_CLAN)) {
                log_char(cn,LOG_SYSTEM,0,"Entering Clan Spawner.");		
	}

        if (!it[in].drdata[6]) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a magic object and vanishes.",ch[cn].name);

	oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char(cn,x,y,0)) {
		// door is blocked and its a door with level restriction --> clan spawner door
		if (it[in].drdata[1] && drop_char_extended(cn,x,y,7)) {
			;
		} else if (it[in].drdata[1] && (co=map[x+y*MAXMAP].ch) && (ch[co].flags&CF_PLAYER) && ch[co].level<=it[in].drdata[1]) {
			remove_char(co); ch[co].action=ch[co].step=ch[co].duration=0;
			if (!set_char(co,oldx,oldy,0)) exit_char_player(co);
			if (!set_char(cn,x,y,0)) exit_char_player(cn);
		} else {
			log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
			drop_char(cn,oldx,oldy,0);
		}
	}

	switch(ch[cn].dir) {
		case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
		case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
		case DX_UP:	ch[cn].dir=DX_DOWN; break;
		case DX_DOWN:	ch[cn].dir=DX_UP; break;
	}
	if (it[in].drdata[1]) update_char(cn);	// clan spawner door, update char since we've entered/left a clan area

	if (!it[in].drdata[6]) log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s pops in.",ch[cn].name);
}

void stat_scroll_driver(int in,int cn)
{
	int v,cnt,n;

        if (!cn) return;	// always make sure its not an automatic call if you don't handle it
	if (!it[in].carried) return;

	/*if (ch[cn].exp_used>ch[cn].exp) {
		log_char(cn,LOG_SYSTEM,0,"This scroll won't work while you have negative experience.");
		return;
	}*/

	v=it[in].drdata[0];
	cnt=it[in].drdata[1];

	if (!ch[cn].value[1][v]) {
		log_char(cn,LOG_SYSTEM,0,"Nothing happens.");
		return;
	}

        for (n=0; n<cnt; n++) {
		if (!raise_value_exp(cn,v)) break;
	}

	if (n==0) {	// raise didnt work at all?
		log_char(cn,LOG_SYSTEM,0,"Nothing happens.");
		return;
        }

	log_char(cn,LOG_SYSTEM,0,"Raised %s by %d.",skill[v].name,cnt);
	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"Used stat scroll of %s",skill[v].name);
	
	remove_item_char(in);
        destroy_item(in);
}

int assemble_sun1(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_AREA2_SUN2:	return create_item("sun_amulet12");
		case IID_AREA2_SUN3:	return create_item("sun_amulet13");
		case IID_AREA2_SUN23:	return create_item("sun_amulet123");
		default:		return 0;
	}
}

int assemble_sun2(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_AREA2_SUN1:	return create_item("sun_amulet12");
		case IID_AREA2_SUN3:	return create_item("sun_amulet23");
		case IID_AREA2_SUN13:	return create_item("sun_amulet123");
		default:		return 0;
	}
}

int assemble_sun3(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_AREA2_SUN1:	return create_item("sun_amulet13");
		case IID_AREA2_SUN2:	return create_item("sun_amulet23");
		case IID_AREA2_SUN12:	return create_item("sun_amulet123");
		default:		return 0;
	}
}

int assemble_sun12(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_AREA2_SUN3:	return create_item("sun_amulet123");
		default:		return 0;
	}
}

int assemble_sun13(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_AREA2_SUN2:	return create_item("sun_amulet123");		
		default:		return 0;
	}
}

int assemble_sun23(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_AREA2_SUN1:	return create_item("sun_amulet123");		
		default:		return 0;
	}
}

int assemble_bluekey1(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_BLUEKEY2:	return create_item("warr_bluekey12");
		case IID_STAFF_BLUEKEY3:	return create_item("warr_bluekey13");
		case IID_STAFF_BLUEKEY23:	return create_item("warr_bluekey123");
		default:		return 0;
	}
}

int assemble_bluekey2(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_BLUEKEY1:	return create_item("warr_bluekey12");
		case IID_STAFF_BLUEKEY3:	return create_item("warr_bluekey23");
		case IID_STAFF_BLUEKEY13:	return create_item("warr_bluekey123");
		default:		return 0;
	}
}

int assemble_bluekey3(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_BLUEKEY1:	return create_item("warr_bluekey13");
		case IID_STAFF_BLUEKEY2:	return create_item("warr_bluekey23");
		case IID_STAFF_BLUEKEY12:	return create_item("warr_bluekey123");
		default:		return 0;
	}
}

int assemble_bluekey12(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_BLUEKEY3:	return create_item("warr_bluekey123");
		default:		return 0;
	}
}

int assemble_bluekey13(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_BLUEKEY2:	return create_item("warr_bluekey123");		
		default:		return 0;
	}
}

int assemble_bluekey23(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_BLUEKEY1:	return create_item("warr_bluekey123");		
		default:		return 0;
	}
}

int assemble_greenkey1(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_GREENKEY2:	return create_item("warr_greenkey12");
		case IID_STAFF_GREENKEY3:	return create_item("warr_greenkey13");
		case IID_STAFF_GREENKEY23:	return create_item("warr_greenkey123");
		default:		return 0;
	}
}

int assemble_greenkey2(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_GREENKEY1:	return create_item("warr_greenkey12");
		case IID_STAFF_GREENKEY3:	return create_item("warr_greenkey23");
		case IID_STAFF_GREENKEY13:	return create_item("warr_greenkey123");
		default:		return 0;
	}
}

int assemble_greenkey3(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_GREENKEY1:	return create_item("warr_greenkey13");
		case IID_STAFF_GREENKEY2:	return create_item("warr_greenkey23");
		case IID_STAFF_GREENKEY12:	return create_item("warr_greenkey123");
		default:		return 0;
	}
}

int assemble_greenkey12(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_GREENKEY3:	return create_item("warr_greenkey123");
		default:		return 0;
	}
}

int assemble_greenkey13(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_GREENKEY2:	return create_item("warr_greenkey123");		
		default:		return 0;
	}
}

int assemble_greenkey23(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_GREENKEY1:	return create_item("warr_greenkey123");		
		default:		return 0;
	}
}

int assemble_redkey1(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_REDKEY2:		return create_item("warr_redkey12");
		case IID_STAFF_REDKEY3:		return create_item("warr_redkey13");
		case IID_STAFF_REDKEY23:	return create_item("warr_redkey123");
		default:			return 0;
	}
}

int assemble_redkey2(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_REDKEY1:		return create_item("warr_redkey12");
		case IID_STAFF_REDKEY3:		return create_item("warr_redkey23");
		case IID_STAFF_REDKEY13:	return create_item("warr_redkey123");
		default:			return 0;
	}
}

int assemble_redkey3(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_REDKEY1:		return create_item("warr_redkey13");
		case IID_STAFF_REDKEY2:		return create_item("warr_redkey23");
		case IID_STAFF_REDKEY12:	return create_item("warr_redkey123");
		default:			return 0;
	}
}

int assemble_redkey12(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_REDKEY3:		return create_item("warr_redkey123");
		default:			return 0;
	}
}

int assemble_redkey13(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_REDKEY2:		return create_item("warr_redkey123");		
		default:			return 0;
	}
}

int assemble_redkey23(int in1,int in2)
{
	switch(it[in2].ID) {
		case IID_STAFF_REDKEY1:		return create_item("warr_redkey123");		
		default:			return 0;
	}
}

void assemble_driver(int in,int cn)
{
	int in2,in3;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried
	
	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You can only use this item with another item.");
		return;
	}
	switch(it[in].ID) {
		case IID_AREA2_SUN1:	in3=assemble_sun1(in,in2); break;
		case IID_AREA2_SUN2:	in3=assemble_sun2(in,in2); break;
		case IID_AREA2_SUN3:	in3=assemble_sun3(in,in2); break;
		case IID_AREA2_SUN12:	in3=assemble_sun12(in,in2); break;
		case IID_AREA2_SUN13:	in3=assemble_sun13(in,in2); break;
		case IID_AREA2_SUN23:	in3=assemble_sun23(in,in2); break;

		case IID_STAFF_BLUEKEY1:	in3=assemble_bluekey1(in,in2); break;
		case IID_STAFF_BLUEKEY2:	in3=assemble_bluekey2(in,in2); break;
		case IID_STAFF_BLUEKEY3:	in3=assemble_bluekey3(in,in2); break;
		case IID_STAFF_BLUEKEY12:	in3=assemble_bluekey12(in,in2); break;
		case IID_STAFF_BLUEKEY13:	in3=assemble_bluekey13(in,in2); break;
		case IID_STAFF_BLUEKEY23:	in3=assemble_bluekey23(in,in2); break;

		case IID_STAFF_GREENKEY1:	in3=assemble_greenkey1(in,in2); break;
		case IID_STAFF_GREENKEY2:	in3=assemble_greenkey2(in,in2); break;
		case IID_STAFF_GREENKEY3:	in3=assemble_greenkey3(in,in2); break;
		case IID_STAFF_GREENKEY12:	in3=assemble_greenkey12(in,in2); break;
		case IID_STAFF_GREENKEY13:	in3=assemble_greenkey13(in,in2); break;
		case IID_STAFF_GREENKEY23:	in3=assemble_greenkey23(in,in2); break;

		case IID_STAFF_REDKEY1:		in3=assemble_redkey1(in,in2); break;
		case IID_STAFF_REDKEY2:		in3=assemble_redkey2(in,in2); break;
		case IID_STAFF_REDKEY3:		in3=assemble_redkey3(in,in2); break;
		case IID_STAFF_REDKEY12:	in3=assemble_redkey12(in,in2); break;
		case IID_STAFF_REDKEY13:	in3=assemble_redkey13(in,in2); break;
		case IID_STAFF_REDKEY23:	in3=assemble_redkey23(in,in2); break;

		default:		log_char(cn,LOG_SYSTEM,0,"Bug # 42556"); return;
	}
	if (in3) {
		if (ch[cn].flags&CF_PLAYER) {
			dlog(cn,in2,"dropped because it was used in assembly");
			dlog(cn,in,"dropped because it was used in assembly");
			dlog(cn,in3,"took because it was created by assembly");
		}
		remove_item(in2); free_item(in2);
		replace_item_char(in,in3); free_item(in);
	} else log_char(cn,LOG_SYSTEM,0,"That doesn't seem to fit.");
}

#define MAXRANDCHEST	100

struct randchest_ppd
{
	int ID[MAXRANDCHEST];
	int last_used[MAXRANDCHEST];
};

void randchest_driver(int in,int cn)
{
	int ID,n,old_n=0,old_val=0,in2=0,amount;
	struct randchest_ppd *ppd;

	if (!cn) return;
	
	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	ppd=set_data(cn,DRD_RANDCHEST_PPD,sizeof(struct randchest_ppd));
	if (!ppd) return;	// oops...

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXRANDCHEST; n++) {
		if (ppd->ID[n]==ID) break;
                if (realtime-ppd->last_used[n]>old_val) {
			old_val=realtime-ppd->last_used[n];
			old_n=n;
		}
	}

        if (n==MAXRANDCHEST) n=old_n;
	else if (realtime-ppd->last_used[n]<60*60*24) {
		log_char(cn,LOG_SYSTEM,0,"You didn't find anything.");
		return;
	}

	ppd->ID[n]=ID;
	ppd->last_used[n]=realtime;

        if (it[in].drdata[1]) {
                if (it[in].drdata[1]==1) {	// lvl 1-10
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;
	
				case 21:	in2=create_item("healing_potion1"); break;
				case 22:	in2=create_item("mana_potion1"); break;
				case 23:	in2=create_item("combo_potion1"); break;
				
				case 24:	in2=create_item("sword4_potion"); break;
				case 25:	in2=create_item("twohand4_potion"); break;
				case 26:	in2=create_item("flash4_potion"); break;
				case 27:	in2=create_item("immunity4_potion"); break;
			}
		} else if (it[in].drdata[1]==2) {	// lvl 10-20
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;
				case 21:	; in2=create_item("healing_potion2"); break;
				case 22:	in2=create_item("mana_potion2"); break;
				case 23:	in2=create_item("combo_potion2"); break;
	
				case 24:	in2=create_item("sword8_potion"); break;
				case 25:	in2=create_item("twohand8_potion");break;
				case 26:	in2=create_item("flash8_potion"); break;
				case 27:	in2=create_item("immunity8_potion"); break;
			}
		} else if (it[in].drdata[1]==3) {	// lvl 20-30
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion2"); break;
				case 22:	in2=create_item("mana_potion2"); break;
				case 23:	in2=create_item("combo_potion2"); break;
	
				case 24:	in2=create_item("sword12_potion"); break;
				case 25:	in2=create_item("twohand12_potion"); break;
				case 26:	in2=create_item("flash12_potion"); break;
				case 27:	in2=create_item("immunity12_potion"); break;
			}
		} else if (it[in].drdata[1]==4) {	// lvl 30-40
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword16_potion"); break;
				case 25:	in2=create_item("twohand16_potion"); break;
				case 26:	in2=create_item("flash16_potion"); break;
				case 27:	in2=create_item("immunity16_potion"); break;
			}
		} else if (it[in].drdata[1]==5) {	// lvl 40-50
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword20_potion"); break;
				case 25:	in2=create_item("twohand20_potion"); break;
				case 26:	in2=create_item("flash20_potion"); break;
				case 27:	in2=create_item("immunity20_potion"); break;
			}
		} else if (it[in].drdata[1]==6) {	// lvl 50-60
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword24_potion"); break;
				case 25:	in2=create_item("twohand24_potion"); break;
				case 26:	in2=create_item("flash24_potion"); break;
				case 27:	in2=create_item("immunity24_potion"); break;
			}
		} else if (it[in].drdata[1]==7) {	// lvl 60-70
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword28_potion"); break;
				case 25:	in2=create_item("twohand28_potion"); break;
				case 26:	in2=create_item("flash28_potion"); break;
				case 27:	in2=create_item("immunity28_potion"); break;
			}
		} else if (it[in].drdata[1]==8) {	// lvl 70-80
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword32_potion"); break;
				case 25:	in2=create_item("twohand32_potion"); break;
				case 26:	in2=create_item("flash32_potion"); break;
				case 27:	in2=create_item("immunity32_potion"); break;
			}
		} else if (it[in].drdata[1]==9) {	// lvl 80-90
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword36_potion"); break;
				case 25:	in2=create_item("twohand36_potion"); break;
				case 26:	in2=create_item("flash36_potion"); break;
				case 27:	in2=create_item("immunity36_potion"); break;
			}
		} else if (it[in].drdata[1]==10) {	// lvl 90-100
			switch(RANDOM(28)) {
				case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
				case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: break;

				case 21:	in2=create_item("healing_potion3"); break;
				case 22:	in2=create_item("mana_potion3"); break;
				case 23:	in2=create_item("combo_potion3"); break;
	
				case 24:	in2=create_item("sword40_potion"); break;
				case 25:	in2=create_item("twohand40_potion"); break;
				case 26:	in2=create_item("flash40_potion"); break;
				case 27:	in2=create_item("immunity40_potion"); break;
			}
		}		
	} else {
		if (RANDOM(4)) {
			log_char(cn,LOG_SYSTEM,0,"You didn't find anything.");
			return;
		}
	}

	if (!in2) {
		amount=(RANDOM(it[in].drdata[0])+1)*(RANDOM(it[in].drdata[0])+1);
		in2=create_money_item(amount);
		if (!in2) {	// should never happen
			log_char(cn,LOG_SYSTEM,0,"You didn't find anything.");
			return;
		}
		log_char(cn,LOG_SYSTEM,0,"You found some money (%.2fG)!",amount/100.0);
	} else {
		log_char(cn,LOG_SYSTEM,0,"You found a %s.",it[in2].name);
	}

	if (in2) {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from randchest");

		ch[cn].citem=in2;
		it[in2].carried=cn;
	}
	ch[cn].flags|=CF_ITEMS;
}

#define MAXDEMONSHRINE	100

struct demonshrine_ppd
{
	int ID[MAXDEMONSHRINE];	
};

void demonshrine_driver(int in,int cn)
{
	int ID,n,old_n=-1;
	struct demonshrine_ppd *ppd;

	if (!cn) return;

	if (ch[cn].level<it[in].min_level) {
		log_char(cn,LOG_SYSTEM,0,"You're not powerful enough to read this book.");
		return;
	}
	
	ppd=set_data(cn,DRD_DEMONSHRINE_PPD,sizeof(struct demonshrine_ppd));
	if (!ppd) return;	// oops...

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXDEMONSHRINE; n++) {
		if (ppd->ID[n]==ID) break;
                if (ppd->ID[n]==0 && old_n==-1) {			
			old_n=n;
		}
	}

        if (n==MAXDEMONSHRINE && old_n==-1) {
		log_char(cn,LOG_SYSTEM,0,"Bug 771");
		return;		
	}

	if (n==MAXDEMONSHRINE) n=old_n;
	else {
		log_char(cn,LOG_SYSTEM,0,"You've been here before. You cannot learn more from this book.");
		return;
	}

	ppd->ID[n]=ID;

	log_char(cn,LOG_SYSTEM,0,"You study the old book and learn something about the ancient tribes. Your Ancient Knowledge went up by one and you gained experience.");

        ch[cn].value[1][V_DEMON]++;
	update_char(cn);

	give_exp(cn,min(250+ch[cn].value[1][V_DEMON]*100,ch[cn].exp/25));
}

void lollipop(int in,int cn)
{
	if (it[in].drdata[1]==8) {
		log_char(cn,LOG_SYSTEM,0,"Ahh memories, sweet memories.");
		return;
	}

	it[in].sprite++;
	it[in].drdata[1]++;

	give_exp(cn,max(5,level_value(ch[cn].level)/750));

	if (it[in].drdata[1]==1) sprintf(it[in].description,"A sweet lollipop. Well, it's already used.");
	else if (it[in].drdata[1]==8) sprintf(it[in].description,"A lollipop stick.");

	log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s licks a lollipop.",ch[cn].name);

	update_item(in);
}

void xmaspop(int in,int cn)
{
	if (cn) {
		log_char(cn,LOG_SYSTEM,0,"You notice a tiny inscription on the lollipop. It reads:");
		log_char(cn,LOG_SYSTEM,0,"\"Place me under a christmas tree and you shall be rewarded.\"");
		log_char(cn,LOG_SYSTEM,0,"In even smaller print you see:");
		log_char(cn,LOG_SYSTEM,0,"\"Offer usable once per tree.\"");
	}
}

void xmastree(int in,int cn)
{
	int in2,in3,idx,bit;
	char *god;
	struct misc_ppd *ppd;

	if (!cn) return;
	if (!isxmas) {
		log_char(cn,LOG_SYSTEM,0,"There doesn't appear to be anything you can do with the tree at this time of year.");
		return;
	}
	
	if (!(ppd=set_data(cn,DRD_MISC_PPD,sizeof(struct misc_ppd)))) return;
	
	idx=areaID/8;
	bit=1<<(areaID%8);

	if (idx<0 || idx>7 || ppd->treedone[idx]&bit) {
		log_char(cn,LOG_SYSTEM,0,"Offer usable only once per tree.");
		return;
	}

	in3=ch[cn].citem;

	if (!in3 || it[in3].driver!=IDR_FOOD || it[in3].drdata[0]!=3) {
		log_char(cn,LOG_SYSTEM,0,"It seems you have to put a special candy under the tree before you can take your gift.");
		return;
	}

	in2=0;
        switch(RANDOM(17)) {
		case 0:		in2=create_item("ad_bracelet1"); break;
		case 1:		in2=create_item("ad_bracelet2"); break;
		case 2:		in2=create_item("ad_ring1"); break;
		case 3:		in2=create_item("ad_ring2"); break;
		case 4:		in2=create_item("ad_ring3"); break;
		case 5:		in2=create_item("ad_ring4"); break;
		case 6:		in2=create_item("ad_ring5"); break;
		case 7:		in2=create_item("ad_necklace1"); break;
		case 8:		in2=create_item("ad_necklace2"); break;
		case 9:		in2=create_item("ad_cape1"); break;
		case 10:	in2=create_item("ad_cape2"); break;
		case 11:	in2=create_item("ad_cape3"); break;
		case 12:	in2=create_item("ad_boots1"); break;
		case 13:	in2=create_item("ad_boots2"); break;
		case 14:	in2=create_item("ad_boots3"); break;
		case 15:	in2=create_item("ad_belt1"); break;
		case 16:	in2=create_item("ad_belt2"); break;
	}
	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"Oops. That was a bug. please try again!");
		return;
	}

	god="Islena";
        switch(RANDOM(5)) {
		case 0:	god="Ishtar"; break;
		case 1:	god="Zoetje"; break;
		case 2:	god="Coloman"; break;
		case 3:	god="Ankh"; break;
		case 4:	god="Goitia"; break;
	}

        sprintf(it[in2].description,"For %s, from %s. Merry Christmas.",ch[cn].name,god);

	if (give_char_item(cn,in2)) {
		destroy_item(in3);
		ch[cn].citem=0;
		ppd->treedone[idx]|=bit;
		log_char(cn,LOG_SYSTEM,0,"You got a %s from under the tree.",it[in2].name);
	} else {
		log_char(cn,LOG_SYSTEM,0,"No space in inventory!");
		destroy_item(in2);
	}
}

void xmasmaker(int in,int cn)
{
	int in2;
	
	if (!cn || !(ch[cn].flags&(CF_STAFF|CF_GOD))) return;

	in2=create_item("xmaspop");
	if (!give_char_item(cn,in2)) destroy_item(in2);	
}

void food_driver(int in,int cn)
{
        if (!cn) return;	// always make sure its not an automatic call if you don't handle it
	if (!it[in].carried) return;

	switch(it[in].drdata[0]) {
		case 0:		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s eats a %s. You see juice dripping down %s chin.",ch[cn].name,it[in].name,hisname(cn)); break;
		case 1:		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s eats a bowl of %s. A moment later, flames burst from %s ears.",ch[cn].name,it[in].name,hisname(cn)); break;
		case 2:		lollipop(in,cn); return;
		case 3:		xmaspop(in,cn); return;
	}
	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was eaten");
	remove_item_char(in);
        destroy_item(in);
}

void enchant_item(int in,int cn)
{
	int in2,n,what,plus,cnt=0;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You have to use another item on this one.");
		return;
	}
	if (!(it[in2].flags&IF_WEAR)) {
		log_char(cn,LOG_SYSTEM,0,"You can only enhance equipment (items you can wear).");
		return;
	}
	if (it[in2].flags&IF_NOENHANCE) {
		log_char(cn,LOG_SYSTEM,0,"This item resists the magic of the orb.");
		return;
	}
	if (it[in2].flags&IF_WNLHAND) {
		log_char(cn,LOG_SYSTEM,0,"You cannot enhance this.");
		return;
	}

	what=it[in].drdata[0];
	plus=it[in].drdata[1];

	for (n=0; n<MAXMOD; n++) {
		if (it[in2].mod_value[n] && it[in2].mod_index[n]==what) {
			if (it[in2].mod_value[n]+plus>20) {
				log_char(cn,LOG_SYSTEM,0,"You cannot enhance the modifier %s on this item any more.",skill[what].name);
				return;
			}
			break;
		}
                switch(it[in2].mod_index[n]) {
			case V_WEAPON:	break;
			case V_ARMOR:	break;
			case V_SPEED:	break;
			case V_DEMON:	break;
			case V_LIGHT:	break;
			default:        if (it[in2].mod_value[n]>0 && it[in2].mod_index[n]>=0) cnt++;
		}
	}
	if (cnt>2) {
		log_char(cn,LOG_SYSTEM,0,"This item cannot be enhanced with any additional abilities.");
		return;	
	}
	if (n==MAXMOD) {
		for (n=0; n<MAXMOD; n++) {
			if (!it[in2].mod_value[n]) break;			
		}
	}
	if (n==MAXMOD) {
		log_char(cn,LOG_SYSTEM,0,"This item cannot be enhanced with any additional abilities.");
		return;
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used to enhance %s",it[in2].name);
        remove_item_char(in);
	destroy_item(in);
	
	it[in2].mod_index[n]=what;
	it[in2].mod_value[n]+=plus;

	set_item_requirements(in2);

        look_item(cn,it+in2);
}

#define MAXORBSPAWN	100

struct orbspawn_ppd
{
	int ID[MAXORBSPAWN];
	int last_used[MAXORBSPAWN];
};

void orbspawn_driver(int in,int cn)
{
	int ID,n,old_n=0,old_val=0,in2=0;
	struct orbspawn_ppd *ppd;

	if (!cn) return;
	
	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	if (ch[cn].level<it[in].min_level) {
		log_char(cn,LOG_SYSTEM,0,"You feel overwhelmed by the energy of the orb and decide not to touch it before you've grown stronger.");
		return;
	}

	if (!(ch[cn].flags&CF_PAID)) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, only paying players may get orbs.");
		return;
	}

	ppd=set_data(cn,DRD_ORBSPAWN_PPD,sizeof(struct orbspawn_ppd));
	if (!ppd) return;	// oops...

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXORBSPAWN; n++) {
		//log_char(cn,LOG_SYSTEM,0,"%u,%u %u: %.2f Days).",(ppd->ID[n]&255),((ppd->ID[n]>>8)&255),((ppd->ID[n]>>16)&255),(realtime-ppd->last_used[n])/60.0/60/24);
		if (ppd->ID[n]==ID) break;
                if (realtime-ppd->last_used[n]>old_val) {
			old_val=realtime-ppd->last_used[n];
			old_n=n;			
		}
	}

        if (n==MAXORBSPAWN) n=old_n;
	else if (realtime-ppd->last_used[n]<60*60*24*60) {	// 60 days respawn time
		log_char(cn,LOG_SYSTEM,0,"Nothing happens");
		return;
	}

	ppd->ID[n]=ID;
	ppd->last_used[n]=realtime;

	in2=create_orb();

        if (!in2) {
                log_char(cn,LOG_SYSTEM,0,"Nothing happens.");
		return;		
	} else {
		log_char(cn,LOG_SYSTEM,0,"An %s was created.",it[in2].name);
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from orbspawner");

        ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;
}

void spade(int in,int cn)
{
	int in2=0;
	struct staffer_ppd *ppd;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	if (areaID==16 && ch[cn].x==205 && ch[cn].y==234) {
		in2=create_item("forest_note1");
	}

	if (areaID==16 && ch[cn].x==130 && ch[cn].y==219) {
		teleport_char_driver(cn,44,231);
		log_char(cn,LOG_SYSTEM,0,"The floor collapses below your feet and you fall...");
		return;
	}
	if (areaID==29 && (ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd)))) {
                switch(ppd->forestbran_done) {
			case 0:		if (ch[cn].x==83 && ch[cn].y==127) {
						in2=create_money_item(1000*100+RANDOM(1000*100));
						ppd->forestbran_done++;
					}
					break;
			case 1:		if (ch[cn].x==94 && ch[cn].y==222) {
						in2=create_money_item(1000*100+RANDOM(1000*100));
						ppd->forestbran_done++;
					}
					break;
			case 2:		if (ch[cn].x==214 && ch[cn].y==136) {
						in2=create_money_item(1000*100+RANDOM(1000*100));
						ppd->forestbran_done++;
					}
					break;
			case 3:		if (ch[cn].x==185 && ch[cn].y==22) {
						in2=create_money_item(1000*100+RANDOM(1000*100));
						ppd->forestbran_done++;
					}
					break;
			case 4:		if (ch[cn].x==165 && ch[cn].y==79) {
						in2=create_money_item(1000*100+RANDOM(1000*100));
						ppd->forestbran_done++;
					}
					break;
		}
	}
	
	if (in2) {
		log_char(cn,LOG_SYSTEM,0,"You found a %s.",it[in2].name);
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from dug hole");
		it[in2].carried=cn;
		ch[cn].citem=in2;
		ch[cn].flags|=CF_ITEMS;
	} else log_char(cn,LOG_SYSTEM,0,"You dug a nice deep hole but you didn't find anything. Embarrassed you stop digging and fill the hole again.");
}

struct combine
{
	int part1,part2,result;
};

static struct combine combine[]={
	{51015,51016,51021},
	{51015,51017,51027},
	{51015,51022,51023},
	{51015,51024,51026},
	{51015,51025,51027},
	{51015,51029,51032},
	{51015,51030,51033},
	{51015,51034,51031},
	{51015,51036,51038},
	{51015,51039,51014},
	{51015,51040,51037},

	{51016,51018,51022},
	{51016,51025,51024},
	{51016,51027,51041},
	{51016,51028,51026},
	{51016,51030,51034},
	{51016,51032,51042},
	{51016,51033,51031},
        {51016,51037,51014},
	{51016,51038,51043},
	{51016,51040,51039},

	{51017,51018,51025},
	{51017,51019,51029},
	{51017,51021,51041},
	{51017,51022,51024},
	{51017,51023,51026},
	{51017,51035,51036},
	{51017,51022,51024},

	{51018,51021,51023},
	{51018,51027,51028},
	{51018,51029,51030},
	{51018,51032,51033},
	{51018,51036,51040},
	{51018,51038,51037},
	{51018,51041,51026},
	{51018,51042,51031},
	{51018,51043,51014},

	{51019,51020,51035},
	{51019,51024,51034},
	{51019,51025,51030},
	{51019,51026,51031},
	{51019,51027,51032},
	{51019,51028,51033},
	{51019,51041,51042},

	{51020,51029,51036},
	{51020,51030,51040},
	{51020,51031,51014},
	{51020,51032,51038},
	{51020,51033,51037},
	{51020,51034,51039},

	{51021,51025,51026},
	{51021,51030,51031},
	{51021,51036,51043},
	{51021,51040,51014},

	{51022,51027,51026},
	{51022,51029,51034},
	{51022,51032,51031},
	{51022,51036,51039},
	{51022,51038,51014},

	{51023,51029,51031},
	{51023,51036,51014},

	{51024,51035,51039},
	
	{51025,51035,51040},

	{51026,51035,51014},

	{51027,51035,51038},

	{51028,51035,51037},

	{51035,51041,51037}
};

void palace_key(int in,int cn)
{
	int in2,n;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (!(in2=ch[cn].citem)) {
		for (n=0; n<ARRAYSIZE(combine); n++) {
			if (it[in].sprite==combine[n].result) {
				log_char(cn,LOG_SYSTEM,0,"%d %d %d",combine[n].part1,combine[n].part2,combine[n].result);

				it[in].sprite=combine[n].part2;
				
				in2=create_item("palace_key_part1");
				it[in2].sprite=combine[n].part1;
				it[in2].carried=cn;
				ch[cn].citem=in2;
				ch[cn].flags|=CF_ITEMS;				
				return;
			}
		}
		
		log_char(cn,LOG_SYSTEM,0,"The only thing you can think of to do with this key part is to add another key part to it.");
		return;
	}
	if (it[in2].ID!=IID_AREA11_PALACEKEYPART) {
		log_char(cn,LOG_SYSTEM,0,"That doesn't fit.");
		return;
	}

	for (n=0; n<ARRAYSIZE(combine); n++) {
		if ((it[in].sprite==combine[n].part1 && it[in2].sprite==combine[n].part2) ||
		    (it[in2].sprite==combine[n].part1 && it[in].sprite==combine[n].part2)) {
			log_char(cn,LOG_SYSTEM,0,"%d %d %d",combine[n].part1,combine[n].part2,combine[n].result);
			
			it[in].sprite=combine[n].result;
			if (it[in].sprite==51014) {
				it[in].ID=IID_AREA11_PALACEKEY;
				it[in].driver=0;
				it[in].flags&=~(IF_USE);
				strcpy(it[in].name,"Palace Key");
				strcpy(it[in].description,"The key to the ice palace.");
			}

			ch[cn].citem=0;
			destroy_item(in2);
			ch[cn].flags|=CF_ITEMS;
			return;
		}
	}
	log_char(cn,LOG_SYSTEM,0,"That doesn't fit (%d,%d).",it[in].sprite,it[in2].sprite);
}

void special_potion(int in,int cn)
{
	int type,flag,n,cnt,exp_old;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (it[in].min_level>ch[cn].level) {
		log_char(cn,LOG_SYSTEM,0,"You're not powerful enough to use this.");
		return;
	}
	if (it[in].max_level && ch[cn].level>it[in].max_level) {
		log_char(cn,LOG_SYSTEM,0,"You're too powerful to use this.");
		return;
	}

	if (areaID==33) {	// long tunnel: no potions!
		log_char(cn,LOG_SYSTEM,0,"You sense that the potion would not work.");
		return;
	}

	// no potions in teufelheim arena
	if (areaID==34 && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) {
		log_char(cn,LOG_SYSTEM,0,"You sense that the potion would not work.");
		return;
	}

	type=it[in].drdata[0];
	switch(type) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:		if (type>=0 && type<=3) flag=remove_poison(cn,type);
				else flag=remove_all_poison(cn);

				if (flag) {
					log_char(cn,LOG_SYSTEM,0,"You feel better.");
				} else log_char(cn,LOG_SYSTEM,0,"It didn't have any effect.");
				if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
                                remove_item_char(in);
				free_item(in);
				return;
		case 5:		if (ch[cn].saves<10 && !(ch[cn].flags&CF_HARDCORE)) {
					ch[cn].saves++;
					log_char(cn,LOG_SYSTEM,0,"You feel secure.");
					if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
					remove_item_char(in);
					free_item(in);
				} else log_char(cn,LOG_SYSTEM,0,"You don't feel like drinking this potion now.");
				return;
		case 6:		if (add_spell(cn,IDR_INFRARED,TICKS*60*10,"infravision_spell")) {
					if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
					remove_item_char(in);
					free_item(in);
				}
				log_char(cn,LOG_SYSTEM,0,"Your eyes start to itch.");
				return;

		case 7:         if (ch[cn].exp<ch[cn].exp_used) {
                                        log_char(cn,LOG_SYSTEM,0,"You don't feel like drinking this potion now.");
                                        return;
                                }
                                for (n=cnt=0; n<P_MAX; n++) {
					cnt+=ch[cn].prof[n];
					ch[cn].prof[n]=0;
				}
				if (!cnt) {
					log_char(cn,LOG_SYSTEM,0,"You don't feel like drinking this potion now.");
					return;
				}
				exp_old=ch[cn].exp_used;
                                for (cnt=cnt/3; cnt>0; cnt--) lower_value(cn,V_PROFESSION);
				ch[cn].exp-=(exp_old-ch[cn].exp_used);
				ch[cn].flags|=CF_PROF;
				update_char(cn);

				remove_item_char(in);
				free_item(in);
				
				return;

		case 8:		ch[cn].hp=max(1,ch[cn].hp-10*POWERSCALE);
				ch[cn].endurance=max(0,ch[cn].endurance-10*POWERSCALE);
				ch[cn].mana=max(0,ch[cn].mana-10*POWERSCALE);
				ch[cn].regen_ticker=ticker;
				log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"You see %s hit %sself on the head with a mug.",ch[cn].name,himname(cn));
				remove_item_char(in);
				free_item(in);
                                return;
		
		case 9:		ch[cn].hp=max(1,ch[cn].hp-10*POWERSCALE);
				ch[cn].regen_ticker=ticker;
                                log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s suddenly starts singing in a slurred tongue... Dogs start howling...",ch[cn].name);
				remove_item_char(in);
				free_item(in);
                                return;
		
		case 10:	ch[cn].mana=max(0,ch[cn].mana-10*POWERSCALE);
				ch[cn].regen_ticker=ticker;
				log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s's hair suddenly shoots up as if hit by electricity.",ch[cn].name);
				remove_item_char(in);
				free_item(in);
                                return;

		case 11:	ch[cn].hp=max(1,ch[cn].hp-10*POWERSCALE);
				ch[cn].endurance=max(0,ch[cn].endurance-10*POWERSCALE);
				ch[cn].mana=max(0,ch[cn].mana-10*POWERSCALE);
				ch[cn].regen_ticker=ticker;
				log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s seems to be enjoying a fine ale.",ch[cn].name);
				remove_item_char(in);
				free_item(in);
                                return;

		case 12:	if (areaID!=33) ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+3*POWERSCALE);
                                log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s drinks a delicious apple juice.",ch[cn].name);
				remove_item_char(in);
				free_item(in);
                                return;

		case 13:	if (areaID!=33) ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+4*POWERSCALE);
                                log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s feels refreshed.",ch[cn].name);
				remove_item_char(in);
				free_item(in);
                                return;

		case 14:	if (areaID!=33) ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+5*POWERSCALE);
                                log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s cracks %s strong knuckles.",ch[cn].name,hisname(cn));
				remove_item_char(in);
				free_item(in);
                                return;

		case 15:        ch[cn].endurance=max(0,ch[cn].endurance-10*POWERSCALE);
                                ch[cn].regen_ticker=ticker;
				log_area(ch[cn].x,ch[cn].y,LOG_SYSTEM,0,16,"%s starts frothing at the mouth.",ch[cn].name);
				remove_item_char(in);
				free_item(in);
                                return;

				
		default:	log_char(cn,LOG_SYSTEM,0,"Please report bug #1734.");
				return;
	}
}

void infinite_chest(int in,int cn)
{
        int in2,n;

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your 'hand' (mouse cursor) first.");
		return;
	}

	if (it[in].drdata[1] || it[in].drdata[2]) {
		for (n=30; n<INVENTORYSIZE; n++)
			if ((in2=ch[cn].item[n]))
				if (*(unsigned int*)(it[in].drdata+1)==it[in2].ID) break;
		if (n==INVENTORYSIZE) {
			if (!(in2=ch[cn].citem) || *(unsigned int*)(it[in].drdata+1)!=it[in2].ID) {
				log_char(cn,LOG_SYSTEM,0,"You need a key to open this chest.");
				return;
			}
		}
		log_char(cn,LOG_SYSTEM,0,"You use %s to open the chest.",it[in2].name);
	}

	// get item to spawn
        switch(it[in].drdata[0]) {
		case 1:		in2=create_item("rune1"); break;
		case 2:		in2=create_item("rune2"); break;
		case 3:		in2=create_item("rune3"); break;
		case 4:		in2=create_item("rune4"); break;
		case 5:		in2=create_item("rune5"); break;
		case 6:		in2=create_item("rune6"); break;
		case 7:		in2=create_item("rune7"); break;
		case 8:		in2=create_item("rune8"); break;
		case 9:		in2=create_item("rune9"); break;

		default:	
				log_char(cn,LOG_SYSTEM,0,"Congratulations, %s, you have just discovered bug #4744B-%d-%d, please report it to the authorities!",ch[cn].name,it[in].x,it[in].y);
				return;
	}

        if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"Congratulations, %s, you have just discovered bug #4744C-%d-%d, please report it to the authorities!",ch[cn].name,it[in].x,it[in].y);
		return;			
	}

	if (!can_carry(cn,in2,0)) {
		destroy_item(in2);
		return;
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from infinite chest");

	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;

        log_char(cn,LOG_SYSTEM,0,"You got a %s.",it[in2].name);
}

struct trader_data
{
	int state;

	int c1ID,c2ID;

	int c1itm[10],c1cnt;
	int c2itm[10],c2cnt;

	int c1ok,c2ok;

	int timeout;

	int memcleartimer;
};

int find_char_byname(char *name)
{
	int co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!(ch[co].flags&CF_PLAYER)) continue;
		if (!strcasecmp(name,ch[co].name)) break;
	}
	return co;
}

int find_char_byID(int ID)
{
	int co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].ID==ID) break;
	}
	return co;
}

// hack to determine if char is in gatekeeper room in aston
int is_gk_room(int cn)
{
	if (areaID!=3) return 0;
	if (ch[cn].x>=178 && ch[cn].y>=196 && ch[cn].x<=210 && ch[cn].y<=228) return 1;
	
	return 0;
}

void return_items(struct trader_data *dat,int switched)
{
	int c2,n;

        if (switched) c2=find_char_byID(dat->c2ID);
	else c2=find_char_byID(dat->c1ID);

	for (n=0; n<dat->c1cnt; n++) {
		it[dat->c1itm[n]].flags&=~IF_VOID;
		if (!c2 || is_gk_room(c2) || !give_char_item(c2,dat->c1itm[n])) {
			destroy_item(dat->c1itm[n]);
		}
		dat->c1itm[n]=0;
	}
	dat->c1cnt=0;				
	
        if (switched) c2=find_char_byID(dat->c1ID);
	else c2=find_char_byID(dat->c2ID);

	for (n=0; n<dat->c2cnt; n++) {
		it[dat->c2itm[n]].flags&=~IF_VOID;
		if (!c2 || is_gk_room(c2) || !give_char_item(c2,dat->c2itm[n])) {
			destroy_item(dat->c2itm[n]);
		}
		dat->c2itm[n]=0;
	}
	dat->c2cnt=0;
}

void trader_driver(int cn,int ret,int lastact)
{
	struct trader_data *dat;
        int co,c2,n,talkdir=0;
	char *ptr,*text,name[40];
	struct msg *msg,*next;

        dat=set_data(cn,DRD_TRADERDRIVER,sizeof(struct trader_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		// did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			quiet_say(cn,"Hello %s! I will work as middleman in any deal you might wish to make with another player. With my °c4help°c0, no one will cheat you. ",ch[co].name);
			talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			if (analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3)) talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
			
			text=(char*)(msg->dat2);

			// skip past the usual Ishtar says: "
			while (isalpha(*text)) text++;
			while (isspace(*text)) text++;
			while (isalpha(*text)) text++;
			if (*text==':') text++;
			while (isspace(*text)) text++;
			if (*text=='"') text++;

			if ((ptr=strstr(text,"trade with"))) {
				if (dat->state!=0) {
					quiet_say(cn,"Sorry, I am busy.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				ptr+=10;
				while (isspace(*ptr)) ptr++;

				for (n=0; n<39 && isalpha(*ptr); n++) name[n]=*ptr++;
				name[n]=0;
				if (!(c2=find_char_byname(name))) {
					quiet_say(cn,"Sorry, %s does not seem to be around.",name);
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				if (cnt_free_inv(co)<10) {
					quiet_say(cn,"Sorry, your inventory is too filled to trade, %s.",ch[co].name);
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				if (cnt_free_inv(c2)<10) {
					quiet_say(cn,"Sorry, %s's inventory is too filled to trade, %s.",ch[c2].name,ch[co].name);
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				dat->state=1;
				dat->c1ID=ch[co].ID;
                                dat->c2ID=ch[c2].ID;
				dat->timeout=ticker+TICKS*60*3;
				quiet_say(cn,"I will handle a trade between %s and %s. You have three minutes to complete it. When you are satisfied with the deal, say °c4accept trade°c0. If you wish to stop the deal, say °c4stop trade°c0. You can check the deal with °c4show trade°c0.",ch[co].name,ch[c2].name);
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
			}

			if (strstr(text,"stop trade")) {
				if (dat->state!=1 && dat->state!=2) {
					quiet_say(cn,"Sorry, not possible right now.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				if (ch[co].ID!=dat->c1ID && ch[co].ID!=dat->c2ID) {
					quiet_say(cn,"Sorry, I am not trading on your behalf at the moment.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				
				return_items(dat,0);

                                quiet_say(cn,"The trade is cancelled.");
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->state=0;
				dat->c1ok=0;
				dat->c2ok=0;
			}
			if (!strcmp(text,"accept trade\"")) {
				if (dat->state!=1 && dat->state!=2) {
					quiet_say(cn,"Sorry, not possible right now.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				if (ch[co].ID!=dat->c1ID && ch[co].ID!=dat->c2ID) {
					quiet_say(cn,"Sorry, I am not trading at your behalf at the moment.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				if (ch[co].ID==dat->c1ID) dat->c1ok=1;
                                if (ch[co].ID==dat->c2ID) dat->c2ok=1;
				if (dat->c1ok && dat->c2ok) {
					return_items(dat,1);
					quiet_say(cn,"Deal.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->state=0;
					dat->c1ok=0;
					dat->c2ok=0;
				} else {
					quiet_say(cn,"%s is satisfied with the deal.",ch[co].name);
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->state=2;
				}
			} else if (strstr(text,"accept trade")) {
				say(cn,"You have to say \"accept trade\" by itself, not as part of a longer sentence to make it work. Like this:");
				say(cn,"accept trade");
				say(cn,"No leading or trailing spaces, either.");
			}
			if (strstr(text,"show trade")) {
				if (dat->state!=1 && dat->state!=2) {
					quiet_say(cn,"Sorry, not possible right now.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				if (ch[co].ID!=dat->c1ID && ch[co].ID!=dat->c2ID) {
					quiet_say(cn,"Sorry, I am not trading at your behalf at the moment.");
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					remove_message(cn,msg);
					continue;
				}
				
				log_char(co,LOG_SYSTEM,0,"Trading:");
				for (n=0; n<dat->c1cnt; n++) look_item(co,it+dat->c1itm[n]);
				log_char(co,LOG_SYSTEM,0,"For:");
				for (n=0; n<dat->c2cnt; n++) look_item(co,it+dat->c2itm[n]);
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
			if (dat->state!=1) {
				quiet_say(cn,"Sorry, not possible right now.");
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
                                remove_message(cn,msg);
				if (ch[cn].citem && !give_char_item(co,ch[cn].citem)) {
					destroy_item(ch[cn].citem);
				}
				ch[cn].citem=0;
				continue;
			}
			if (!ch[co].ID || (ch[co].ID!=dat->c1ID && ch[co].ID!=dat->c2ID)) {
				quiet_say(cn,"I am not trading at your behalf at the moment, %s.",ch[co].name);
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				if (ch[cn].citem && !give_char_item(co,ch[cn].citem)) {
					destroy_item(ch[cn].citem);
				}
				ch[cn].citem=0;
			} else if (ch[co].ID==dat->c1ID) {
				if (dat->c1cnt>9) {
					quiet_say(cn,"I cannot trade more than ten items at once, %s.",ch[co].name);
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					if (ch[cn].citem && !give_char_item(co,ch[cn].citem)) {
						destroy_item(ch[cn].citem);
					}
					ch[cn].citem=0;
				} else {
					dat->c1itm[dat->c1cnt++]=ch[cn].citem;
					it[ch[cn].citem].flags|=IF_VOID;
                                        c2=find_char_byID(dat->c2ID);
					if (c2) {
						log_char(c2,LOG_SYSTEM,0,"°c2%s gave me:",ch[co].name);
						look_item(c2,it+ch[cn].citem);
					}
					ch[cn].citem=0;
				}
			} else if (ch[co].ID==dat->c2ID) {
				if (dat->c2cnt>9) {
					quiet_say(cn,"I cannot trade more than ten items at once, %s.",ch[co].name);
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					if (ch[cn].citem && !give_char_item(co,ch[cn].citem)) {
						destroy_item(ch[cn].citem);
					}
					ch[cn].citem=0;
				} else {
					dat->c2itm[dat->c2cnt++]=ch[cn].citem;
					it[ch[cn].citem].flags|=IF_VOID;
					c2=find_char_byID(dat->c1ID);
					if (c2) {
						log_char(c2,LOG_SYSTEM,0,"°c2%s gave me:",ch[co].name);
						look_item(c2,it+ch[cn].citem);
					}
					ch[cn].citem=0;
				}
			} else { //uh?
				quiet_say(cn,"I'm confused.");
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
	if (ticker>dat->timeout && dat->state>0) {
		quiet_say(cn,"The trade is cancelled!");
                return_items(dat,0);
		dat->state=0;
		dat->c1ok=0;
		dat->c2ok=0;
	}

	if (ticker>dat->memcleartimer) {
		mem_erase_driver(cn,7);
		dat->memcleartimer=ticker+TICKS*60*60*12;
	}

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void set_salt_data(int in)
{
	if (*(unsigned int*)(it[in].drdata+0)>=10000) it[in].sprite=13212;
	else if (*(unsigned int*)(it[in].drdata+0)>=1000) it[in].sprite=13211;
	else if (*(unsigned int*)(it[in].drdata+0)>=100) it[in].sprite=13210;
	else if (*(unsigned int*)(it[in].drdata+0)>=10) it[in].sprite=13209;
	else it[in].sprite=13208;
	
	sprintf(it[in].description,"%d ounces of %s.",*(unsigned int*)(it[in].drdata),it[in].name);
}

void set_skin1_data(int in)
{
	if (*(unsigned int*)(it[in].drdata+0)>=5) it[in].sprite=59659;
	else if (*(unsigned int*)(it[in].drdata+0)>=4) it[in].sprite=59658;
	else if (*(unsigned int*)(it[in].drdata+0)>=3) it[in].sprite=59657;
	else if (*(unsigned int*)(it[in].drdata+0)>=2) it[in].sprite=59656;
	else it[in].sprite=59655;
	
	sprintf(it[in].description,"%d %ss.",*(unsigned int*)(it[in].drdata),it[in].name);
}

void set_skin2_data(int in)
{
	if (*(unsigned int*)(it[in].drdata+0)>=5) it[in].sprite=59664;
	else if (*(unsigned int*)(it[in].drdata+0)>=4) it[in].sprite=59663;
	else if (*(unsigned int*)(it[in].drdata+0)>=3) it[in].sprite=59662;
	else if (*(unsigned int*)(it[in].drdata+0)>=2) it[in].sprite=59661;
	else it[in].sprite=59660;
	
	sprintf(it[in].description,"%d %ss.",*(unsigned int*)(it[in].drdata),it[in].name);
}

void nomad_stack(int in,int cn)
{
	int in2;
	char *name=NULL,*item=NULL;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (it[in].ID==IID_AREA19_SALT) { name="ounce"; item="salt"; }
	else if (it[in].ID==IID_AREA19_WOLFSSKIN) { name="skin"; item="skin1"; }
	else if (it[in].ID==IID_AREA19_WOLFSSKIN2) { name="skin"; item="skin2"; }
        else {
		log_char(cn,LOG_SYSTEM,0,"Bug #1442y");
		return;
	}


	if (!(in2=ch[cn].citem)) {
		int a,b;

		a=*(unsigned int*)(it[in].drdata+0);
		if (a<2) {
			log_char(cn,LOG_SYSTEM,0,"You cannot split 1 %s.",name);
                        return;
		}
		b=a/2;
		if (b>=10000) b=10000;
		else if (b>=5000) b=5000;
		else if (b>=2500) b=2500;
		else if (b>=1000) b=1000;
		else if (b>=500) b=500;
		else if (b>=250) b=250;
		else if (b>=100) b=100;
		else if (b>=50) b=50;
		else if (b>=25) b=25;
		else if (b>=10) b=10;
		a=a-b;

                in2=create_item(item);
                if (!in2) {
			log_char(cn,LOG_SYSTEM,0,"Bug #3199i");
			return;
		}

		it[in2].value=it[in].value*b/(a+b);
		it[in].value=it[in].value*a/(a+b);

		*(unsigned int*)(it[in].drdata)=a;
		*(unsigned int*)(it[in2].drdata)=b;

		if (it[in].ID==IID_AREA19_SALT) {
			set_salt_data(in);
			set_salt_data(in2);
		} else if (it[in].ID==IID_AREA19_WOLFSSKIN) {
			set_skin1_data(in);
			set_skin1_data(in2);
		} else if (it[in].ID==IID_AREA19_WOLFSSKIN2) {
			set_skin2_data(in);
			set_skin2_data(in2);
		} else {
			log_char(cn,LOG_SYSTEM,0,"Bug #3149x");
			remove_item(in);
			destroy_item(in);
			destroy_item(in2);
			return;
		}

                ch[cn].citem=in2; it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;

		log_char(cn,LOG_SYSTEM,0,"Split into %d %ss and %d %ss.",a,name,b,name);

		return;
	}
	if (it[in2].ID!=it[in].ID) {
		log_char(cn,LOG_SYSTEM,0,"You cannot mix those.");
		return;
	}

	it[in].value+=it[in2].value;

	*(unsigned int*)(it[in].drdata)+=*(unsigned int*)(it[in2].drdata);

	if (it[in].ID==IID_AREA19_SALT) {
		set_salt_data(in);
	} else if (it[in].ID==IID_AREA19_WOLFSSKIN) {
		set_skin1_data(in);
	} else if (it[in].ID==IID_AREA19_WOLFSSKIN2) {
		set_skin2_data(in);
	} else {
		log_char(cn,LOG_SYSTEM,0,"Bug #3149xz");
		remove_item(in);
		remove_item(in2);
		destroy_item(in);
		destroy_item(in2);
		return;
	}

        log_char(cn,LOG_SYSTEM,0,"%d %ss.",*(unsigned int*)(it[in].drdata),name);

	destroy_item(in2);
	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;
}

void labexit(int in,int cn)
{
	if (!cn) {
                if (*(unsigned int*)(it[in].drdata+8)<24) it[in].sprite=1060+*(unsigned int*)(it[in].drdata+8)%24;
		else if (*(unsigned int*)(it[in].drdata+8)<240) it[in].sprite=1060+*(unsigned int*)(it[in].drdata+8)%24+24;
		else if (*(unsigned int*)(it[in].drdata+8)<240+24) it[in].sprite=1060+*(unsigned int*)(it[in].drdata+8)%24+24+24;
		else { remove_item(in); destroy_item(in); return; }

		(*(unsigned int*)(it[in].drdata+8))++;

		set_sector(it[in].x,it[in].y);

		call_item(it[in].driver,in,0,ticker+2);
		return;
	}
	if (ch[cn].ID!=*(unsigned int*)(it[in].drdata)) {
		log_char(cn,LOG_SYSTEM,0,"This gate has not been created for you. You cannot use it.");
		return;
	}
	set_solved_lab(cn,it[in].drdata[4]);

	*(unsigned int*)(it[in].drdata+8)=240-24+*(unsigned int*)(it[in].drdata+8)%24;

	if (!change_area(cn,3,183,199)) log_char(cn,LOG_SYSTEM,0,"Sorry, Aston is down. Please try again soon.");	
}


/*#define INPUT_CNT	8
#define HIDDEN_CNT	8
#define OUTPUT_CNT	8

#define RATE		10

#define MEM_CNT		3

struct janitor_data
{
	int init;
	
	double input_weight[HIDDEN_CNT][INPUT_CNT];
	double hidden_weight[OUTPUT_CNT][HIDDEN_CNT];
	
	double hidden_output[HIDDEN_CNT];
	double output[OUTPUT_CNT];
	
	double inp[MEM_CNT][INPUT_CNT];
	double out[MEM_CNT][OUTPUT_CNT];

	double lastscore;
};

int neu_run(struct janitor_data *dat,double *input,double *output,int dotrain)
{
	int x,y,maxy;
	double maxr,mse,sum;
	double hidden_weight_delta[OUTPUT_CNT];
	double input_weight_delta[HIDDEN_CNT];

	if (!dat->init) {
		dat->init=1;
		
		for (x=0; x<HIDDEN_CNT; x++) {
			for (y=0; y<INPUT_CNT; y++) {
				dat->input_weight[x][y]=(RANDOM(2000)-1000)/2000.0;
			}
			
		}

		for (x=0; x<OUTPUT_CNT; x++) {
			for (y=0; y<HIDDEN_CNT; y++) {
				dat->hidden_weight[x][y]=(RANDOM(2000)-1000)/2000.0;
			}
			
		}
	}

        for (x=0; x<HIDDEN_CNT; x++) {
		dat->hidden_output[x]=0;
		for (y=0; y<INPUT_CNT; y++) {
			dat->hidden_output[x]+=input[y]*dat->input_weight[x][y];
			
		}
		dat->hidden_output[x]=1/(1+exp(-dat->hidden_output[x]));
	}

	for (x=0; x<OUTPUT_CNT; x++) {
		dat->output[x]=0;
		for (y=0; y<HIDDEN_CNT; y++) {
			dat->output[x]+=dat->hidden_output[y]*dat->hidden_weight[x][y];
			
		}
		dat->output[x]=1/(1+exp(-dat->output[x]));
	}
	
        maxr=-1;
	maxy=-1;
        for (y=0; y<OUTPUT_CNT; y++) {
		if (dotrain) xlog("%d: %.2f -> (%.2f) -> %.2f / %.2f ",y,input[y],dat->hidden_output[y],dat->output[y],output[y]);

		if (dat->output[y]>maxr) {
			maxr=dat->output[y];
			maxy=y;
		}
	}

	if (dotrain) {
		for (x=0,mse=0.0; x<OUTPUT_CNT; x++) {
			hidden_weight_delta[x]=output[x]-dat->output[x];
			mse+=hidden_weight_delta[x]*hidden_weight_delta[x];
			hidden_weight_delta[x]*=dat->output[x]*(1-dat->output[x]);
		}

		if (mse<0.02) return maxy;

		for (x=0; x<HIDDEN_CNT; x++) {
			for (y=0, sum=0.0; y<OUTPUT_CNT; y++)
				sum+=hidden_weight_delta[y]*dat->hidden_weight[y][x];

			input_weight_delta[x]=sum*dat->hidden_output[x]*(1-dat->hidden_output[x]);
		}

		for (x=0; x<OUTPUT_CNT; x++)
			for (y=0; y<HIDDEN_CNT; y++)
				dat->hidden_weight[x][y]+=RATE*hidden_weight_delta[x]*dat->hidden_output[y];



		for (x=0; x<HIDDEN_CNT; x++)
			for (y=0; y<INPUT_CNT; y++)
				dat->input_weight[x][y]+=RATE*input_weight_delta[x]*input[y];
		
	}

	return maxy;
}*/

#define MAXLIGHT	150
#define MAXTAKE		10

static int jx,jy,ls;

int lightcmp(const void *a,const void *b)
{
	int in1,in2;

	in1=*(int*)(a); in2=*(int*)(b);

	if (in1 && !in2) return -1;
	if (!in1 && in2) return 1;
	if (!in1 && !in2) return 0;


	if (it[in1].drdata[0]==ls && it[in2].drdata[0]!=ls) return 1;
	if (it[in1].drdata[0]!=ls && it[in2].drdata[0]==ls) return -1;

	return (abs(it[in1].x-jx)+abs(it[in1].y-jy))-(abs(it[in2].x-jx)+abs(it[in2].y-jy));
}

int takecmp(const void *a,const void *b)
{
	int in1,in2;

	in1=*(int*)(a); in2=*(int*)(b);

	if (in1 && !in2) return -1;
	if (!in1 && in2) return 1;
	if (!in1 && !in2) return 0;

	if (it[in1].x && !it[in2].x) return 1;
	if (!it[in1].x && it[in2].x) return -1;

	return (abs(it[in1].x-jx)+abs(it[in1].y-jy))-(abs(it[in2].x-jx)+abs(it[in2].y-jy));
}

struct janitor_data
{
	int cnt;
	int light[MAXLIGHT];
	int take[MAXTAKE];
};

int lightadd(struct janitor_data *dat,int in)
{
	int n;

	for (n=0; n<MAXLIGHT; n++) {
		if (dat->light[n]==in) return 0;
		if (!dat->light[n]) {
			dat->light[n]=in;
			return 1;
		}
	}
	return 0;
}

int takeadd(struct janitor_data *dat,int in)
{
	int n;

	for (n=0; n<MAXTAKE; n++) {
		if (dat->take[n]==in) return 0;
		if (!dat->take[n]) {
			dat->take[n]=in;
			return 1;
		}
	}
	return 0;
}
int janitor_drop(int cn)
{
	if (drop_driver(cn,161,180)) return 1;
	if (drop_driver(cn,161,179)) return 1;
	if (drop_driver(cn,161,178)) return 1;
	if (drop_driver(cn,162,178)) return 1;
	if (drop_driver(cn,162,179)) return 1;
	if (drop_driver(cn,162,180)) return 1;
	if (drop_driver(cn,162,181)) return 1;
	if (drop_driver(cn,162,182)) return 1;
	if (drop_driver(cn,162,183)) return 1;

	return 0;
}

void janitor_driver(int cn,int ret,int lastact)
{
	struct janitor_data *dat;
        int co,in;
        struct msg *msg,*next;

	dat=set_data(cn,DRD_JANITORDRIVER,sizeof(struct janitor_data));
	if (!dat) return;	// oops...

        scan_item_driver(cn);

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		// did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>20) { remove_message(cn,msg); continue; }
		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        //destroy_item(ch[cn].citem);
			//ch[cn].citem=0;			
		}

		if (msg->type==NT_ITEM) {
                        in=msg->dat1;

			if (ch[cn].tmpy<192 && it[in].y>192) { remove_message(cn,msg); continue; }
			if (ch[cn].tmpy>192 && it[in].y<192) { remove_message(cn,msg); continue; }

			if (it[in].driver==IDR_TOYLIGHT) {
				lightadd(dat,in);
			}

			if (it[in].flags&IF_TAKE) {
				if (it[in].x<161 || it[in].x>162 || it[in].y<178 || it[in].y>183) takeadd(dat,in);
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	jx=ch[cn].x; jy=ch[cn].y;
	if (dlight>200) ls=0; else ls=1;

	if (ch[cn].citem && !ch[cn].item[INVENTORYSIZE-1]) {
                memmove(ch[cn].item+31,ch[cn].item+30,sizeof(int)*(INVENTORYSIZE-30-1));
		ch[cn].item[30]=ch[cn].citem;
		ch[cn].citem=0;
	}

	if (!ch[cn].citem) {
		qsort(dat->take,MAXTAKE,sizeof(int),takecmp);
		in=dat->take[0];
		if ((it[in].x<161 || it[in].x>162 || it[in].y<178 || it[in].y>183) && take_driver(cn,in)) return;
                else dat->take[0]=0;
	}

	qsort(dat->light,MAXLIGHT,sizeof(int),lightcmp);

	if (!dat->light[0] || it[dat->light[0]].drdata[0]==ls) {
		if (!ch[cn].citem) {
			int n;
	
			for (n=INVENTORYSIZE; n>=30; n--) {
				if (ch[cn].item[n]) break;			
			}
			
			if (n>=30) {
				ch[cn].citem=ch[cn].item[n]; ch[cn].item[n]=0;
				if (janitor_drop(cn)) return;
				ch[cn].item[n]=ch[cn].citem; ch[cn].citem=0;
			}
		} else {
			if (janitor_drop(cn)) return;
		}

		if (move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,0)) return;
		do_idle(cn,TICKS);
		return;
	}

	if (use_driver(cn,dat->light[0],0)) {
		if (!RANDOM(50)) {
			switch(RANDOM(4)) {
				case 0:		murmur(cn,"I hate my life. I hate my life! I HATE MY LIFE!"); break;
				case 1:		if (!dat->cnt) dat->cnt=17785;
						murmur(cn,"%d lights I turned on in my life, %d lights I turned on in my life...",dat->cnt,dat->cnt);
						dat->cnt++;
						break;
				case 2:		murmur(cn,"Infravision potions. Yes, that's a good way to deal with the dark!");
						break;
				case 3:		murmur(cn,"I need new shoes.");
						break;
			}
		}
		return;
	}

        do_idle(cn,TICKS);
}

void shrike_amulet_driver(int in,int cn)
{
	int in2;

        if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried
	
	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You can only use this item with another item.");
		return;
	}
	if (it[in2].driver!=IDR_SHRIKEAMULET || (it[in].drdata[0]&it[in2].drdata[0])) {
		log_char(cn,LOG_SYSTEM,0,"It doesn't fit.");
		return;
	}
	
	it[in].drdata[0]|=it[in2].drdata[0];
	it[in].sprite=51617+it[in].drdata[0];
	switch (it[in].drdata[0]) {
		case 3:		sprintf(it[in].name,"Crystal on Chain"); sprintf(it[in].description,"A light blue crystal on a silver chain."); break;
		case 5:         sprintf(it[in].name,"Crystal on Charm"); sprintf(it[in].description,"A light blue crystal on a silver crescent charm."); break;
		case 6:		sprintf(it[in].name,"Charm on Chain"); sprintf(it[in].description,"A silver crescent charm on a silver chain."); break;
		case 7:		sprintf(it[in].name,"Talisman"); sprintf(it[in].description,"A silver talisman."); break;
	}
	ch[cn].flags|=CF_ITEMS;
	
	ch[cn].citem=0;
	destroy_item(in2);
}

void minegatewaykey(int in,int cn)
{
	int bit1,bit2,in2;

	if (!cn) return;

	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"Use, yes, but use it with what?");
		return;
	}

	if (it[in2].driver!=IDR_MINEGATEWAYKEY) {
		log_char(cn,LOG_SYSTEM,0,"Interesting idea. Really. Doesn't work, though.");
		return;
	}

	bit1=it[in].drdata[0];
	bit2=it[in2].drdata[0];

        it[in].drdata[0]|=bit2;
	ch[cn].flags|=CF_ITEMS;
	strcpy(it[in].description,"A partially assembled key.");

	ch[cn].citem=0;
	destroy_item(in2);

	switch(it[in].drdata[0]) {
		case 1:		it[in].sprite=52201; break;
		case 2:		it[in].sprite=52202; break;
		case 3:		it[in].sprite=52205; break;
		case 4:		it[in].sprite=52203; break;
		case 5:		it[in].sprite=52206; break;
		case 6:		it[in].sprite=52209; break;
		case 7:		it[in].sprite=52213; break;
		case 8:		it[in].sprite=52204; break;
		case 9:		it[in].sprite=52210; break;
		case 10:	it[in].sprite=52207; break;
		case 11:	it[in].sprite=52212; break;
		case 12:	it[in].sprite=52208; break;
		case 13:	it[in].sprite=52214; break;
		case 14:	it[in].sprite=52211; break;
		case 15:	it[in].sprite=52200;
				it[in].flags&=~IF_USE;
				it[in].ID=IID_MINEGATEWAY;
				strcpy(it[in].name,"Mine gateway key");
				strcpy(it[in].description,"A fully assembled key.");
				break;
	}
}

void decaying_item_driver(int in,int cn)
{
	int n;

        if (!cn) {	// timer call
		if (it[in].drdata[0]) {	// item active?
			
                        *(unsigned short*)(it[in].drdata+3)=(*(unsigned short*)(it[in].drdata+3))+1;
			if (*(unsigned short*)(it[in].drdata+3)>*(unsigned short*)(it[in].drdata+5)) {
				if (it[in].carried) log_char(it[in].carried,LOG_SYSTEM,0,"Your %s expired.",it[in].name);
				if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it expired");
                                remove_item(in);
				destroy_item(in);
                                return;
			}
                        call_item(it[in].driver,in,0,ticker+TICKS*2);
		}		
	} else {
		if (it[in].x) return;

                if (it[in].drdata[0]) {	// item active?
			it[in].drdata[0]=0;
			for (n=0; n<5; n++) if (it[in].mod_value[n]) it[in].mod_value[n]=it[in].drdata[1];
			it[in].sprite--;
                        update_char(cn);
			ch[cn].flags|=CF_ITEMS;
		} else {
                        it[in].drdata[0]=1;
			for (n=0; n<5; n++) if (it[in].mod_value[n]) it[in].mod_value[n]=it[in].drdata[2];
			it[in].sprite++;
                        update_char(cn);
			ch[cn].flags|=CF_ITEMS;
			call_item(it[in].driver,in,0,ticker+TICKS*2);
		}
	}
}

void beyond_potion_driver(int in,int cn)
{
	int fre,in2,endtime,duration;

	if (!cn) return;

	if (!check_requirements(cn,in)) {
		log_char(cn,LOG_SYSTEM,0,"You do not meet the requirements needed to use this potion.");
		return;
	}

        if (!(fre=may_add_spell(cn,IDR_POTION_SP))) {
		log_char(cn,LOG_SYSTEM,0,"Another potion is still active.");
		return;
	}

	// no potions in teufelheim arena
	if (areaID==34 && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) {
		log_char(cn,LOG_SYSTEM,0,"You sense that the potion would not work.");
		return;
	}

	in2=create_item("potion_spell");
	if (!in2) return;

        it[in2].mod_index[0]=it[in].mod_index[0];
	it[in2].mod_value[0]=it[in].mod_value[0];
	it[in2].mod_index[1]=it[in].mod_index[1];
	it[in2].mod_value[1]=it[in].mod_value[1];
	it[in2].mod_index[2]=it[in].mod_index[2];
	it[in2].mod_value[2]=it[in].mod_value[2];	
	it[in2].driver=IDR_POTION_SP;
	it[in2].flags|=(it[in].flags&IF_BEYONDMAXMOD);

	duration=it[in].drdata[0]*60*TICKS;

	endtime=ticker+duration;

	*(signed long*)(it[in2].drdata)=endtime;
	*(signed long*)(it[in2].drdata+4)=ticker;

	it[in2].carried=cn;

        ch[cn].item[fre]=in2;

	create_spell_timer(cn,in2,fre);

	remove_item_char(in);
	destroy_item(in);

	update_char(cn);
}

void set_chip_data(int in,int off)
{
	if (*(unsigned int*)(it[in].drdata+0)>5) it[in].sprite=53012+off;
        else if (*(unsigned int*)(it[in].drdata+0)==5) it[in].sprite=53011+off;
	else if (*(unsigned int*)(it[in].drdata+0)==4) it[in].sprite=53010+off;
	else if (*(unsigned int*)(it[in].drdata+0)==3) it[in].sprite=53009+off;
	else if (*(unsigned int*)(it[in].drdata+0)==2) it[in].sprite=53008+off;
	else it[in].sprite=53007+off;
	
	if (*(unsigned int*)(it[in].drdata+0)>1) sprintf(it[in].description,"%d %ss.",*(unsigned int*)(it[in].drdata),it[in].name);
	else sprintf(it[in].description,"%d %s.",*(unsigned int*)(it[in].drdata),it[in].name);
}

void chip_stack(int in,int cn)
{
	int in2,off;
	char *name=NULL,*item=NULL;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	if (it[in].ID==IID_BRONZECHIP) { name="chip"; item="bronzechip"; off=0; }
	else if (it[in].ID==IID_SILVERCHIP) { name="chip"; item="silverchip"; off=12; }
	else if (it[in].ID==IID_GOLDCHIP) { name="chip"; item="goldchip"; off=6; }
        else {
		log_char(cn,LOG_SYSTEM,0,"Bug #1445y");
		return;
	}


	if (!(in2=ch[cn].citem)) {
		int a,b;

		a=*(unsigned int*)(it[in].drdata+0);
		if (a<2) {
			log_char(cn,LOG_SYSTEM,0,"You cannot split 1 %s.",name);
                        return;
		}
		b=a/2;
		if (b>=10000) b=10000;
		else if (b>=5000) b=5000;
		else if (b>=2500) b=2500;
		else if (b>=1000) b=1000;
		else if (b>=500) b=500;
		else if (b>=250) b=250;
		else if (b>=100) b=100;
		else if (b>=50) b=50;
		else if (b>=25) b=25;
		else if (b>=10) b=10;
		a=a-b;

                in2=create_item(item);
                if (!in2) {
			log_char(cn,LOG_SYSTEM,0,"Bug #3199i");
			return;
		}

		it[in2].value=it[in].value*b/(a+b);
		it[in].value=it[in].value*a/(a+b);

		*(unsigned int*)(it[in].drdata)=a;
		*(unsigned int*)(it[in2].drdata)=b;

		set_chip_data(in,off);
		set_chip_data(in2,off);

                ch[cn].citem=in2; it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;

		log_char(cn,LOG_SYSTEM,0,"Split into %d %ss and %d %ss.",a,name,b,name);

		return;
	}
	if (it[in2].ID!=it[in].ID) {
		log_char(cn,LOG_SYSTEM,0,"You cannot mix those.");
		return;
	}

	it[in].value+=it[in2].value;

	*(unsigned int*)(it[in].drdata)+=*(unsigned int*)(it[in2].drdata);

	set_chip_data(in,off);

        log_char(cn,LOG_SYSTEM,0,"%d %ss.",*(unsigned int*)(it[in].drdata),name);

	destroy_item(in2);
	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_MACRO:		macro_driver(cn,ret,lastact); return 1;
		case CDR_TRADER:	trader_driver(cn,ret,lastact); return 1;
		case CDR_JANITOR:	janitor_driver(cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_POTION:		potion_driver(in,cn); return 1;
		case IDR_DOOR:			return door_driver(in,cn);
		case IDR_BALLTRAP:		balltrap(in,cn); return 1;
                case IDR_CHEST:			chest_driver(in,cn); return 1;
		case IDR_USETRAP:		usetrap(in,cn); return 1;
		case IDR_NIGHTLIGHT:		nightlight_driver(in,cn); return 1;
		case IDR_TORCH:			torch_driver(in,cn); return 1;
		case IDR_RECALL:		recall_driver(in,cn); return 1;
		case IDR_TELEPORT:		teleport_driver(in,cn); return 1;
		case IDR_TELE_DOOR:		teleport_door_driver(in,cn); return 1;
		case IDR_STATSCROLL:		stat_scroll_driver(in,cn); return 1;
		case IDR_STEPTRAP:		steptrap(in,cn); return 1;
		case IDR_ASSEMBLE:		assemble_driver(in,cn); return 1;
		case IDR_RANDCHEST:		randchest_driver(in,cn); return 1;
		case IDR_DEMONSHRINE:		demonshrine_driver(in,cn); return 1;
		case IDR_FOOD:			food_driver(in,cn); return 1;
		case IDR_ENCHANTITEM:		enchant_item(in,cn); return 1;
		case IDR_ORBSPAWN:		orbspawn_driver(in,cn); return 1;
		case IDR_FORESTSPADE:		spade(in,cn); return 1;
		case IDR_PALACEKEY:		palace_key(in,cn); return 1;
		case IDR_SPECIAL_POTION:	special_potion(in,cn); return 1;
		case IDR_INFINITE_CHEST:	infinite_chest(in,cn); return 1;
		case IDR_NOMADSTACK:		nomad_stack(in,cn); return 1;
                case IDR_LABEXIT:		labexit(in,cn); return 1;
		case IDR_DOUBLE_DOOR:           double_door_driver(in,cn); return 1;
		case IDR_TOYLIGHT:		toylight_driver(in,cn); return 1;
		case IDR_SHRIKEAMULET:		shrike_amulet_driver(in,cn); return 1;
		case IDR_MINEGATEWAYKEY:	minegatewaykey(in,cn); return 1;
		case IDR_DECAYITEM:		decaying_item_driver(in,cn); return 1;
		case IDR_BEYONDPOTION:		beyond_potion_driver(in,cn); return 1;
		case IDR_DEMONCHIP:		chip_stack(in,cn); return 1;
		case IDR_XMASTREE:		xmastree(in,cn); return 1;
		case IDR_XMASMAKER:		xmasmaker(in,cn); return 1;

		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_MACRO:		return 1;
		case CDR_TRADER:	return 1;
		case CDR_JANITOR:	return 1;


                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_MACRO:		return 1;
		case CDR_JANITOR:	return 1;
		case CDR_TRADER:	return 1;

		default:		return 0;
	}
}

























