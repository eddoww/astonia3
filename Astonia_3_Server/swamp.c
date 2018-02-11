/*
 * $Id: swamp.c,v 1.3 2006/09/25 14:08:51 devel Exp $
 *
 * $Log: swamp.c,v $
 * Revision 1.3  2006/09/25 14:08:51  devel
 * replaced give_driver calls with more secure give_char_item
 *
 * Revision 1.2  2006/09/23 10:59:49  devel
 * added swamp quests to questlog
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "database.h"
#include "mem.h"
#include "player.h"
#include "skill.h"
#include "expire.h"
#include "clan.h"
#include "chat.h"
#include "los.h"
#include "shrine.h"
#include "area3.h"
#include "sector.h"
#include "consistency.h"
#include "questlog.h"

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
	{{"repeat",NULL},NULL,2}
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

	if (!(ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE))) return 0;

	//if (char_dist(cn,co)>16) return 0;

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

void swamparm(int in,int cn)
{
	int flag=0,co;

	if (cn) return;

	if (it[in].drdata[0]) flag=1;
	else if (map[it[in].x+1+it[in].y*MAXMAP].ch) flag=2;
	else if (map[it[in].x+2+it[in].y*MAXMAP].ch) flag=2;
	else if (map[it[in].x-1+it[in].y*MAXMAP].ch) flag=2;
	else if (map[it[in].x-2+it[in].y*MAXMAP].ch) flag=2;
	else if (map[it[in].x+1+it[in].y*MAXMAP+MAXMAP].ch) flag=3;
	else if (map[it[in].x+2+it[in].y*MAXMAP+MAXMAP].ch) flag=3;
	else if (map[it[in].x-1+it[in].y*MAXMAP+MAXMAP].ch) flag=3;
	else if (map[it[in].x-2+it[in].y*MAXMAP+MAXMAP].ch) flag=3;
	else if (map[it[in].x+1+it[in].y*MAXMAP-MAXMAP].ch) flag=3;
	else if (map[it[in].x+2+it[in].y*MAXMAP-MAXMAP].ch) flag=3;
	else if (map[it[in].x-1+it[in].y*MAXMAP-MAXMAP].ch) flag=3;
	else if (map[it[in].x-2+it[in].y*MAXMAP-MAXMAP].ch) flag=3;

	if (flag==2 && RANDOM(5)) flag=0;
	if (flag==3 && RANDOM(3)) flag=0;

	if (flag) {
		it[in].drdata[0]++;
		it[in].sprite++;
		if (it[in].drdata[0]==12) {
			if ((co=map[it[in].x+1+it[in].y*MAXMAP].ch)) hurt(co,10*POWERSCALE,0,1,50,90);
			if ((co=map[it[in].x+2+it[in].y*MAXMAP].ch)) hurt(co,10*POWERSCALE,0,1,50,90);
			if ((co=map[it[in].x-1+it[in].y*MAXMAP].ch)) hurt(co,10*POWERSCALE,0,1,50,90);
			if ((co=map[it[in].x-2+it[in].y*MAXMAP].ch)) hurt(co,10*POWERSCALE,0,1,50,90);
		}
		if (it[in].drdata[0]>15) { it[in].drdata[0]=0; it[in].sprite-=16; }
		set_sector(it[in].x,it[in].y);		
	}
	call_item(it[in].driver,in,0,ticker+1);
}

#define DXX_CIRCLE_LEFT		10
#define DXX_CIRCLE_RIGHT	11
#define DXX_DARK		12

void swampwhisp(int in,int cn)
{
	int x,y,dx,dy;

	if (cn) return;	

	x=it[in].x; y=it[in].y;

	if (!it[in].drdata[1]) {
		it[in].drdata[1]=x;
		it[in].drdata[2]=y;
		it[in].drdata[3]=DX_DOWN;		
	}

	dx=it[in].x-it[in].drdata[1];
	dy=it[in].y-it[in].drdata[2];

	if (dlight>50) it[in].drdata[3]=DXX_DARK;

	switch(it[in].drdata[3]) {
		case DX_DOWN:	it[in].drdata[0]++;
                                if (it[in].drdata[0]>15) it[in].drdata[0]=0;
				if (it[in].drdata[0]==12) {
					remove_item_map(in);
					if (set_item_map(in,x,y+1)) {
						it[in].drdata[0]=1;
						it[in].drdata[3]=DXX_CIRCLE_LEFT;
					} else {
						set_item_map(in,x,y);
						it[in].drdata[3]=DXX_CIRCLE_RIGHT;
					}
				}
				break;
		
		case DX_UP:	it[in].drdata[0]--;
                                if (it[in].drdata[0]>15) it[in].drdata[0]=15;
				if (it[in].drdata[0]==2) {
					remove_item_map(in);
					if (set_item_map(in,x,y-1)) {
						it[in].drdata[0]=14;
						it[in].drdata[3]=DXX_CIRCLE_RIGHT;
					} else {
						set_item_map(in,x,y);
						it[in].drdata[3]=DXX_CIRCLE_LEFT;
					}
				}
				break;


		case DX_LEFT:	it[in].drdata[0]++;
                                if (it[in].drdata[0]>15) it[in].drdata[0]=0;
				if (it[in].drdata[0]==0) {
					remove_item_map(in);
					if (set_item_map(in,x+1,y)) {
						it[in].drdata[0]=7;
						it[in].drdata[3]=DXX_CIRCLE_LEFT;
					} else {
						set_item_map(in,x,y);
						it[in].drdata[3]=DXX_CIRCLE_RIGHT;
					}
				}
				break;
		
		case DX_RIGHT:	it[in].drdata[0]--;
                                if (it[in].drdata[0]>15) it[in].drdata[0]=15;
				if (it[in].drdata[0]==6) {
					remove_item_map(in);
					if (set_item_map(in,x-1,y)) {
						it[in].drdata[0]=2;
						it[in].drdata[3]=DXX_CIRCLE_RIGHT;
					} else {
						set_item_map(in,x,y);
						it[in].drdata[3]=DXX_CIRCLE_LEFT;
					}
				}
				break;
		case DXX_CIRCLE_LEFT:
				it[in].drdata[0]--;
                                if (it[in].drdata[0]>15) it[in].drdata[0]=15;
				if (dx<2 && !RANDOM(10)) it[in].drdata[3]=DX_RIGHT;
				if (dy<2 && !RANDOM(10)) it[in].drdata[3]=DX_DOWN;				
				break;
		case DXX_CIRCLE_RIGHT:
				it[in].drdata[0]++;
                                if (it[in].drdata[0]>15) it[in].drdata[0]=0;
				if (dx>-2 && !RANDOM(10)) it[in].drdata[3]=DX_LEFT;
				if (dy>-2 && !RANDOM(10)) it[in].drdata[3]=DX_UP;
                                break;

		case DXX_DARK:	if (dlight<50) {
					it[in].drdata[3]=DXX_CIRCLE_LEFT;
					it[in].mod_value[0]=100;
					add_item_light(in);
					call_item(it[in].driver,in,0,ticker+1);
				} else if (it[in].sprite) {
                                        it[in].sprite=0;
					remove_item_light(in);
					it[in].mod_value[0]=0;
					call_item(it[in].driver,in,0,ticker+TICKS);
				} else call_item(it[in].driver,in,0,ticker+TICKS);
				return;
	}
	it[in].sprite=20934+it[in].drdata[0];
        set_sector(it[in].x,it[in].y);
	call_item(it[in].driver,in,0,ticker+2);

        //xlog("item %d state %d, pos %d,%d",in,it[in].drdata[0],it[in].x,it[in].y);
}

int player_close(int xc,int yc,int dist)
{
	int xf,yf,xt,yt,x,y,co;

	xf=max(0,xc-dist);
	yf=max(0,yc-dist);
	xt=min(MAXMAP-1,xc+dist);
	yt=min(MAXMAP-1,yc+dist);

	for (y=yf; y<=yt; y+=8) {
		for (x=xf; x<=xt; x+=8) {
                        for (co=getfirst_char_sector(x,y); co; co=ch[co].sec_next) {
				if (abs(ch[co].x-xc)<=dist && abs(ch[co].y-yc)<=dist && (ch[co].flags&CF_PLAYER)) return 1;				
			}
		}
	}
	return 0;
}

void swampspawn(int in,int cn)
{
	int co,ser,last,m,stop;
	static char *name[]={
		"swamp25n",
		"swamp27n",
		"swamp29n",
		"swamp31n"
	};

        if (cn) return;

	if (!it[in].drdata[1]) {	// replace editor sprite with game sprite
		it[in].drdata[1]=1;
		*(unsigned int*)(it[in].drdata+16)=it[in].sprite-8;
		it[in].sprite=0;
	}

	co=*(unsigned int*)(it[in].drdata+4);
	ser=*(unsigned int*)(it[in].drdata+8);
	last=*(unsigned int*)(it[in].drdata+12);

	if (!it[in].drdata[2] && co && (ch[co].flags) && ch[co].serial==ser) { call_item(it[in].driver,in,0,ticker+TICKS); return; }
	if (!it[in].drdata[2] && last && ticker-last<TICKS*60*2) { call_item(it[in].driver,in,0,ticker+TICKS); return; }

        if (it[in].drdata[2] || player_close(it[in].x,it[in].y,4)) {
		it[in].drdata[2]++;
		it[in].sprite=(*(unsigned int*)(it[in].drdata+16))+it[in].drdata[2];

		m=it[in].x+it[in].y*MAXMAP;
		if (map[m].gsprite>=59405 && map[m].gsprite<=59413) stop=6;
		else if (map[m].gsprite>=59414 && map[m].gsprite<=59422) stop=5;
		else if (map[m].gsprite>=59423 && map[m].gsprite<=59431) stop=3;
		else stop=7;

		if (it[in].drdata[2]>stop) {
                        it[in].drdata[2]=0;
			it[in].sprite=0;
			
			if ((co=create_char(name[it[in].drdata[0]],0))) {
				if (set_char(co,it[in].x,it[in].y,0)) {
					ch[co].tmpx=ch[co].x;
					ch[co].tmpy=ch[co].y;
	
					update_char(co);
				
					ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
					ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
					ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
				
					ch[co].dir=DX_RIGHTDOWN;
	
					*(unsigned int*)(it[in].drdata+4)=co;
					*(unsigned int*)(it[in].drdata+8)=ch[co].serial;
					*(unsigned int*)(it[in].drdata+12)=ticker;
				} else {
					destroy_char(co);
				}
			}
		}
		set_sector(it[in].x,it[in].y);
                call_item(it[in].driver,in,0,ticker+3);
	} else call_item(it[in].driver,in,0,ticker+TICKS);	
}

struct clara_driver_data
{
        int last_talk;
	int current_victim;
};

// note: the ppd is borrowed from area3 - the missions interact...
void clara_driver(int cn,int ret,int lastact)
{
	struct clara_driver_data *dat;
	struct area3_ppd *ppd;
        int co,in,talkdir=0,didsay=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_CLARADRIVER,sizeof(struct clara_driver_data));
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
                        ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));

                        if (ppd) {
				switch(ppd->clara_state) {
					case 0:		say(cn,"Greetings, %s! I am %s, First Sergeant of the Seyan'Du and commander of this outpost.",ch[co].name,ch[cn].name);
							ppd->clara_state++; didsay=1;
							break;
					case 1:		if (ppd->kelly_state>=15) ppd->clara_state++;	// fall through...
							else break;
					case 2:		say(cn,"I assume thou hast been sent from Aston, %s, to report on our status. The road through the swamp is no longer secure and we have been under attack from beasts emerging from the swamp.",get_army_rank_string(co));
							ppd->clara_state++; didsay=1;
							break;
					case 3:		say(cn,"Under the current circumstances, I do not recommend sending reinforcements to secure the road. We cannot afford to bind our forces here. Now go back to Aston and deliver this report.");
							ppd->clara_state++; didsay=1;
							break;
					case 4:		say(cn,"Afterwards come back here, I have more work for thee. That will be all, %s. Dismissed!",get_army_rank_string(co));
							ppd->clara_state++; didsay=1;
							break;
					case 5:		if (ppd->kelly_state>=18) ppd->clara_state++;	// fall through...
							else break;	// waiting for player to reach kelly and come back
					case 6:		say(cn,"I have a difficult mission for thee, %s. The main reason we had to retreat to this camp was one huge swamp beast. It seemed to be immune to our attacks.",ch[co].name);
							questlog_open(co,21);
							ppd->clara_state++; didsay=1;
							break;
					case 7:		say(cn,"I want thee to find a way to slay it. I have heard rumors about a man who used to live with the swamp beasts north-east of this camp. Mayhap he knows a way to injure this beast.");
							ppd->clara_state++; didsay=1;
							break;
					case 8:		say(cn,"Dismissed, %s. And good luck. Thou wilt need it.",get_army_rank_string(co));
							ppd->clara_state++; didsay=1;
							break;
					case 9:		if ((in=ch[co].item[WN_RHAND]) && it[in].ID==IID_HARDKILL) {
								if (questlog_count(co,21)==0) give_military_pts(cn,co,4,EXP_AREA15_HARDKILL);
								ppd->clara_state++;
							} else break;
					case 10:	if ((in=ch[co].item[WN_RHAND]) && it[in].ID==IID_HARDKILL && it[in].drdata[37]<36)
								say(cn,"So that is how one can kill them. Thou wilt need to find all three stone circles and perform the ritual in each one, then, %s.",ch[co].name);
							else say(cn,"So that is how one can kill them.");
							ppd->clara_state++; didsay=1;
							break;
					case 11:	if ((in=ch[co].item[WN_RHAND]) && it[in].ID==IID_HARDKILL && it[in].drdata[37]>=36) ppd->clara_state++;
							else break;
					case 12:	say(cn,"Now that thou knowest how to kill that beast, please go and do it.");
							ppd->clara_state++; didsay=1;
							break;
					case 13:        break;	// waiting for kill
					case 14:        say(cn,"Well done indeed, %s!",ch[co].name);
							questlog_done(co,21);
							if (questlog_count(co,21)==1) give_military_pts(cn,co,8,1);
							ppd->clara_state++; didsay=1;
							break;
					case 15:	say(cn,"The swamp will be safer now, but more dangers await thee on thy travels. May Ishtar be with thee, %s.",ch[co].name);
							ppd->clara_state++; didsay=1;
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
				case 2:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
                                                if (ppd && ppd->clara_state<=5) { dat->last_talk=0; ppd->clara_state=0; }
						if (ppd && ppd->clara_state>=6 && ppd->clara_state<=9) { dat->last_talk=0; ppd->clara_state=6; }
						if (ppd && ppd->clara_state>=10 && ppd->clara_state<=11) { dat->last_talk=0; ppd->clara_state=10; }
						if (ppd && ppd->clara_state>=12 && ppd->clara_state<=13) { dat->last_talk=0; ppd->clara_state=12; }
						if (ppd && ppd->clara_state>=15 && ppd->clara_state<=16) { dat->last_talk=0; ppd->clara_state=15; }
						break;
			}
			if (didsay) {
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				dat->current_victim=co;
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if (ch[cn].citem) {	// we still have it
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

void monster_dead(int cn,int co)
{
	struct area3_ppd *ppd;
	int bit=0,in;

	if (!co) return;
	if (!(ch[co].flags&CF_PLAYER)) return;

	if ((ch[cn].flags&CF_HARDKILL) && (ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd))) && ppd->clara_state>=12 && ppd->clara_state<=13) {
		ppd->clara_state=14;
		log_char(co,LOG_SYSTEM,0,"Well done. Clara will be proud of thee!");
	}
	
	if (ch[co].x>=142 && ch[co].y>=83 && ch[co].x<=153 && ch[co].y<=92) bit=1;
	if (ch[co].x>=34 && ch[co].y>=150 && ch[co].x<=44 && ch[co].y<=160) bit=2;
	if (ch[co].x>=183 && ch[co].y>=154 && ch[co].x<=192 && ch[co].y<=162) bit=4;

        if (hour==0 && bit && (in=ch[co].item[WN_RHAND]) && it[in].driver==0 && !(it[in].drdata[36]&bit)) {
		it[in].ID=IID_HARDKILL;
		it[in].drdata[37]+=12;
		it[in].drdata[36]|=bit;
		it[in].flags|=IF_QUEST;
		log_char(co,LOG_SYSTEM,0,"Your %s starts to glow.",it[in].name);
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_SWAMPCLARA:	clara_driver(cn,ret,lastact); return 1;
		case CDR_SWAMPMONSTER:	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact); return 1;
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_SWAMPARM:	swamparm(in,cn); return 1;
		case IDR_SWAMPWHISP:	swampwhisp(in,cn); return 1;
		case IDR_SWAMPSPAWN:	swampspawn(in,cn); return 1;


                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_SWAMPCLARA:	return 1;
		case CDR_SWAMPMONSTER:	monster_dead(cn,co); return 1;
                default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_SWAMPCLARA:	return 1;
		case CDR_SWAMPMONSTER:	return 1;
                default:		return 0;
	}
}














