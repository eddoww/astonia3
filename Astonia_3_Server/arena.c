/*
 * $Id: arena.c,v 1.5 2007/08/13 18:50:38 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: arena.c,v $
 * Revision 1.5  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.4  2005/12/10 21:01:35  ssim
 * fixed another loophole and a bug in arena manager
 *
 * Revision 1.3  2005/12/10 14:59:30  ssim
 * changed arena policy to not re-rent while anybody is in it
 * made arena "owner" lose his status when he leaves the arena
 *
 * Revision 1.2  2005/12/01 14:43:30  ssim
 * made small arena manager kick out all currently inside his arena when executing 'rent' command
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
	{{"register",NULL},NULL,3},
	{{"enter",NULL},NULL,4},
	{{"leave",NULL},NULL,5},
	{{"rent",NULL},NULL,6}
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

struct arena_ppd
{
	int score;

	int fights;
	int wins;
	int losses;
	int lastfight;
};

#define MAXCONTENDER	50

struct contender
{
	int ID;
	int cn;
	int score;
	int reg_time;
};

#define MS_PAIR		0	// find fighter pair
#define MS_IN		1	// get them inside the arena
#define MS_FIGHT	2	// watch the fight

struct entry
{
	char name[40];
	int score;
	int updated;
};

struct toplist
{
	struct entry entry[100];
};

struct master_data
{
        int last_talk;
        int amgivingback;

	int state;
	unsigned int fight1_ID,fight2_ID;
	int fight1_cn,fight2_cn;
	int timeout;
	int lastsave;

	struct contender ct[MAXCONTENDER];

	int storage_state;
	int storage_version;
	int storage_ID;

	struct toplist storage_data;
};

static struct toplist *tops=NULL;

void add_contender(int cn,int co,struct master_data *dat,struct arena_ppd *ppd)
{
	int n,ID;

	/*if (realtime-ppd->lastfight<60*5) {
		say(cn,"Sorry, you have fought within the last five minutes already, you cannot fight again now, %s.",ch[co].name);
		return;
	}*/

	ID=charID(co);

	for (n=0; n<MAXCONTENDER; n++) {
		if (dat->ct[n].ID==ID) {
			say(cn,"You're already registered for this tournament, %s.",ch[co].name);
			notify_char(co,NT_NPC,NTID_ARENA,3,0);
			return;
		}
	}
	for (n=0; n<MAXCONTENDER; n++) {
		if (!dat->ct[n].ID) {
			dat->ct[n].ID=ID;
			dat->ct[n].cn=co;
			dat->ct[n].score=ppd->score;
			dat->ct[n].reg_time=ticker;
                        say(cn,"Good luck, %s. I will call you when your fight starts.",ch[co].name);
			notify_char(co,NT_NPC,NTID_ARENA,3,0);
			return;
		}
	}
	say(cn,"I'm sorry, %s, but there are no free slots at the moment. Please try again after the next fight.",ch[co].name);
}

void find_contender(int cn,struct master_data *dat)
{
	int n,m,bestdiff=999999999,diff,fight1=0,fight2=0;

	for (n=0; n<MAXCONTENDER; n++) {
		if (!dat->ct[n].ID) continue;
		if (charID(dat->ct[n].cn)!=dat->ct[n].ID) { dat->ct[n].ID=0; continue; }
		if (!ch[dat->ct[n].cn].flags) continue;		

		for (m=n+1; m<MAXCONTENDER; m++) {
			if (!dat->ct[m].ID) continue;
			if (charID(dat->ct[m].cn)!=dat->ct[m].ID) { dat->ct[m].ID=0; continue; }
			if (!ch[dat->ct[m].cn].flags) continue;

			diff=abs(dat->ct[n].score-dat->ct[m].score)*100;
			diff-=(ticker-dat->ct[n].reg_time)+(ticker-dat->ct[m].reg_time);
			if (diff<bestdiff) {
				bestdiff=diff;
				fight1=n; fight2=m;
			}
		}
	}
	if (fight2) {
		dat->state=MS_IN;
		dat->fight1_ID=dat->ct[fight1].ID;
		dat->fight2_ID=dat->ct[fight2].ID;
		dat->fight1_cn=dat->ct[fight1].cn;
		dat->fight2_cn=dat->ct[fight2].cn;
		dat->timeout=ticker+TICKS*30;

		say(cn,"Next fight is: °c6%s versus %s.°c0 Both participants please step forward and say: 'enter'. You have 30 seconds to enter the arena, otherwise you lose by default.",
		    ch[dat->fight1_cn].name,
		    ch[dat->fight2_cn].name);

		notify_char(dat->fight1_cn,NT_NPC,NTID_ARENA,0,0);
		notify_char(dat->fight2_cn,NT_NPC,NTID_ARENA,0,0);
	}
}

void check_inside(int cn,struct master_data *dat)
{
	int n;

	if (ticker<dat->timeout) {
		if (ch[dat->fight1_cn].x<=233 || ch[dat->fight1_cn].x>=243 || ch[dat->fight1_cn].y<=132 || ch[dat->fight1_cn].y>=142) return;
		if (ch[dat->fight2_cn].x<=233 || ch[dat->fight2_cn].x>=243 || ch[dat->fight2_cn].y<=132 || ch[dat->fight2_cn].y>=142) return;

		notify_char(dat->fight1_cn,NT_NPC,NTID_ARENA,1,dat->fight2_cn);
		notify_char(dat->fight2_cn,NT_NPC,NTID_ARENA,1,dat->fight1_cn);
	}

	for (n=0; n<MAXCONTENDER; n++) {
		if (dat->ct[n].ID==dat->fight1_ID) dat->ct[n].ID=0;
		if (dat->ct[n].ID==dat->fight2_ID) dat->ct[n].ID=0;
	}

	say(cn,"Let the fight begin! You have two minutes to kill your opponent.");
        dat->state=MS_FIGHT;
	dat->timeout=ticker+TICKS*60*2;
}

int toplist_cmp(const void *a,const void *b)
{
	if (!((struct entry *)(a))->name[0]) return 1;
	if (!((struct entry *)(b))->name[0]) return -1;

	if (!((struct entry *)(a))->name[0] && !((struct entry *)(b))->name[0]) return 0;

	return ((struct entry *)(b))->score - ((struct entry *)(a))->score;
}

void update_toplist(struct master_data *dat,int cn,int co,int cn_score,int co_score)
{
	int n;
	int found_cn=0,found_co=0;

	for (n=0; n<100; n++) {
		if (!strcmp(dat->storage_data.entry[n].name,ch[cn].name)) {
			if (found_cn) {
				dat->storage_data.entry[n].name[0]=0;
			} else {
				dat->storage_data.entry[n].score=cn_score;
				dat->storage_data.entry[n].updated=realtime;
				found_cn=1;
			}
			continue;
		}
		if (!strcmp(dat->storage_data.entry[n].name,ch[co].name)) {
			if (found_co) {
				dat->storage_data.entry[n].name[0]=0;
			} else {
				dat->storage_data.entry[n].score=co_score;
				dat->storage_data.entry[n].updated=realtime;
				found_co=1;
			}
			continue;
		}
		if (realtime-dat->storage_data.entry[n].updated>60*60*24*7) {
			dat->storage_data.entry[n].name[0]=0;
		}
	}
	if (!found_cn) {
		strcpy(dat->storage_data.entry[98].name,ch[cn].name);
		dat->storage_data.entry[98].score=cn_score;
		dat->storage_data.entry[98].updated=realtime;
	}
	if (!found_co) {
		strcpy(dat->storage_data.entry[99].name,ch[co].name);
		dat->storage_data.entry[99].score=co_score;
		dat->storage_data.entry[99].updated=realtime;
	}
	qsort(dat->storage_data.entry,100,sizeof(dat->storage_data.entry[0]),toplist_cmp);	
}

void score_fight(int cn,int co,struct master_data *dat)
{
	struct arena_ppd *p1,*p2;
	int diff,worth;

        p1=set_data(cn,DRD_ARENA_PPD,sizeof(struct arena_ppd)); if (!p1->fights) p1->score=-2000;
        p2=set_data(co,DRD_ARENA_PPD,sizeof(struct arena_ppd)); if (!p2->fights) p2->score=-2000;

	p1->fights++; p1->wins++;
	p2->fights++; p2->losses++;

	diff=p1->score-p2->score;

	if (diff>10000) worth=0;	// no pts
	else if (diff>8000) worth=1;
	else if (diff>6000) worth=2;
	else if (diff>5000) worth=3;
	else if (diff>4000) worth=4;
	else if (diff>3000) worth=5;
	else if (diff>2500) worth=6;
	else if (diff>2000) worth=7;
	else if (diff>1500) worth=8;
	else if (diff>1250) worth=9;
	else if (diff>1000) worth=10;
	else if (diff>800) worth=20;
	else if (diff>600) worth=30;
	else if (diff>500) worth=40;
	else if (diff>400) worth=50;
	else if (diff>300) worth=60;
	else if (diff>200) worth=70;
	else if (diff>100) worth=85;
	else if (diff>0) worth=100;
	else if (diff>-100) worth=150;
	else if (diff>-200) worth=200;
	else if (diff>-300) worth=250;
	else if (diff>-400) worth=300;
	else if (diff>-500) worth=350;
	else if (diff>-600) worth=400;
	else if (diff>-800) worth=450;
	else if (diff>-1000) worth=500;
	else if (diff>-1250) worth=550;
	else if (diff>-1500) worth=600;
	else if (diff>-2000) worth=650;
	else if (diff>-2500) worth=700;
	else if (diff>-3000) worth=750;
	else if (diff>-4000) worth=800;
	else if (diff>-5000) worth=850;
	else if (diff>-6000) worth=900;
	else if (diff>-8000) worth=950;
	else worth=1000;

	p1->score+=worth; p2->score-=worth;
	p1->lastfight=realtime; p2->lastfight=realtime;

        update_toplist(dat,cn,co,p1->score,p2->score);
}

void empty_arena(int cn)
{
	int x,y,co;

	for (y=133; y<142; y++) {
		for (x=234; x<243; x++) {
			if ((co=map[x+y*MAXMAP].ch)) {
				teleport_char_driver(co,ch[cn].x,ch[cn].y);
			}
		}
	}
}

void check_fight(int cn,struct master_data *dat)
{
	int end=0,win1=1,win2=1;
	
	if (ch[dat->fight1_cn].x<=233 || ch[dat->fight1_cn].x>=243 || ch[dat->fight1_cn].y<=132 || ch[dat->fight1_cn].y>=142) { end++; win1=0; }
        if (ch[dat->fight2_cn].x<=233 || ch[dat->fight2_cn].x>=243 || ch[dat->fight2_cn].y<=132 || ch[dat->fight2_cn].y>=142) { end++; win2=0; }

	if (ticker>dat->timeout) {
		win1=win2=0;
		end=1;
	}

	if (!end) return;
	
	if (win1) {
		say(cn,"And the winner is %s.",ch[dat->fight1_cn].name);
		if (charID(dat->fight1_cn)==dat->fight1_ID && charID(dat->fight2_cn)==dat->fight2_ID) score_fight(dat->fight1_cn,dat->fight2_cn,dat);
	} else if (win2) {
		say(cn,"And the winner is %s.",ch[dat->fight2_cn].name);
		if (charID(dat->fight1_cn)==dat->fight1_ID && charID(dat->fight2_cn)==dat->fight2_ID) score_fight(dat->fight2_cn,dat->fight1_cn,dat);
	} else {
		say(cn,"Hu? No one won? Oh well...");
	}

	if (ch[dat->fight1_cn].flags && charID(dat->fight1_cn)==dat->fight1_ID) notify_char(dat->fight1_cn,NT_NPC,NTID_ARENA,2,0);
	if (ch[dat->fight2_cn].flags && charID(dat->fight2_cn)==dat->fight2_ID) notify_char(dat->fight2_cn,NT_NPC,NTID_ARENA,2,0);

	empty_arena(cn);
        dat->state=MS_PAIR;
	if (ticker-dat->lastsave>TICKS*60*15) {
		dat->storage_state=5;
		dat->lastsave=ticker;
	}
}

void master_driver(int cn,int ret,int lastact)
{
	struct master_data *dat;
	struct arena_ppd *ppd;
        int co,in,talkdir=0,didsay=0,res,size;
	struct msg *msg,*next;
	void *tmp;

        dat=set_data(cn,DRD_ARENAMASTER,sizeof(struct master_data));
	if (!dat) return;	// oops...
	tops=&dat->storage_data;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			dat->storage_ID=49;			
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

                        switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				/*case 2:		ppd=set_data(co,DRD_AREA3_PPD,sizeof(struct area3_ppd));
						if (ppd && ppd->crypt_state<=1) ppd->crypt_state=0;
						break;*/
				case 3:		// register
						ppd=set_data(co,DRD_ARENA_PPD,sizeof(struct arena_ppd)); if (!ppd->fights) ppd->score=-2000;
						if (ppd) add_contender(cn,co,dat,ppd);
						break;
				case 4:		// enter
						if (dat->state!=MS_IN) {
							say(cn,"No fight has been scheduled, %s.",ch[co].name);
							notify_char(co,NT_NPC,NTID_ARENA,5,0);
							break;
						}
						if (charID(co)==dat->fight1_ID) {
							teleport_char_driver(co,235,140);
							notify_char(co,NT_NPC,NTID_ARENA,4,0);
							ch[co].flags&=~CF_LAG;
							dat->fight1_cn=co;
						} else if (charID(co)==dat->fight2_ID) {
							teleport_char_driver(co,241,134);
							notify_char(co,NT_NPC,NTID_ARENA,4,0);
							ch[co].flags&=~CF_LAG;
							dat->fight2_cn=co;
						} else {
							say(cn,"You are not invited to this fight, %s.",ch[co].name);
							notify_char(co,NT_NPC,NTID_ARENA,5,0);
						}
						break;
				case 5:		//leave
						teleport_char_driver(co,238,146);
						break;
	
			}
			if (didsay) {
				dat->last_talk=ticker;
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);				
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

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*10<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}

	if (dat->storage_state>3) {
                switch(dat->state) {
			case MS_PAIR:	find_contender(cn,dat); break;
			case MS_IN:	check_inside(cn,dat); break;
			case MS_FIGHT:	check_fight(cn,dat); break;
		}
	}

	switch(dat->storage_state) {
		case 0:		if (create_storage(dat->storage_ID,"Arena Toplist Data",&dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 1:		if (check_create_storage()) dat->storage_state++;
				break;
		case 2:		if (read_storage(dat->storage_ID,dat->storage_version)) dat->storage_state++;
				break;
		case 3:		res=check_read_storage(&dat->storage_version,&tmp,&size);
				if (res==1 && size==sizeof(dat->storage_data)) {
					memcpy(&dat->storage_data,tmp,sizeof(dat->storage_data));
					xfree(tmp);
					dat->storage_state++;
					//charlog(cn,"read storage: score=%d, wins=%d, losses=%d",dat->storage_data.ppd.score,dat->storage_data.ppd.wins,dat->storage_data.ppd.losses);
					//*ppd=dat->storage_data.ppd;
				} else if (res) {
					elog("check storage said %d (%d)",res,size);
					dat->storage_state++;
				}
				break;
		case 4:		break;	// no op
		case 5:		//dat->storage_data.ppd=*ppd;
				if (update_storage(dat->storage_ID,dat->storage_version,&dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 6:		if (check_update_storage()) {
					dat->storage_state++;
					dat->storage_version++;
					//charlog(cn,"update storage: score=%d, wins=%d, losses=%d",dat->storage_data.ppd.score,dat->storage_data.ppd.wins,dat->storage_data.ppd.losses);
				}
				break;
		case 7:		dat->storage_state=4;
				break;
		default:	dat->storage_state=4;
				break;
	}
	
	do_idle(cn,TICKS);
}

#define FS_LEISURE	0
#define FS_START	1
#define FS_REGISTER	2
#define FS_WAIT		3
#define FS_ENTER	4
#define FS_WAIT2	5
#define FS_FIGHT	6

#define MASTER_POSX	236
#define MASTER_POSY	145

struct fighter_storage
{
	struct arena_ppd ppd;
};

struct fighter_data
{
	int state;
	int enemy;
	int lastact;
	int lastsave;

	int storage_state;
	int storage_version;
	int storage_ID;

	struct fighter_storage storage_data;
};

void fighter_parse(int cn,struct fighter_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"storage")) dat->storage_ID=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}


void fighter_driver(int cn,int ret,int lastact)
{
	struct fighter_data *dat;
	struct arena_ppd *ppd;
        int co,res,size;
	struct msg *msg,*next;
	void *tmp;

        dat=set_data(cn,DRD_ARENAFIGHTER,sizeof(struct master_data));
	if (!dat) return;	// oops...

	ppd=set_data(cn,DRD_ARENA_PPD,sizeof(struct arena_ppd)); if (!ppd->fights) ppd->score=-2000;
	if (!ppd) return;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) {
				fighter_parse(cn,dat);
				ch[cn].arg=NULL;
				ch[cn].restx=247;
				ch[cn].resty=148;
				dat->lastact=-TICKS*60*6;
			}
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
                        co=msg->dat1;			
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;			
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        // let it vanish, then
			destroy_item(ch[cn].citem);
			ch[cn].citem=0;			
		}

		if (msg->type==NT_NPC && msg->dat1==NTID_ARENA && msg->dat2==0 && dat->state==FS_WAIT) {
                        dat->state=FS_ENTER;
			dat->lastact=-TICKS*60*5;
		}
		if (msg->type==NT_NPC && msg->dat1==NTID_ARENA && msg->dat2==1 && dat->state==FS_WAIT2) {
                        dat->state=FS_FIGHT;
			dat->enemy=msg->dat3;
			//say(cn,"You will die, %s",ch[dat->enemy].name);
			fight_driver_reset(cn);
			fight_driver_add_enemy(cn,dat->enemy,1,1);
		}

		if (msg->type==NT_NPC && msg->dat1==NTID_ARENA && msg->dat2==2 && (dat->state==FS_FIGHT || dat->state==FS_WAIT2)) {
                        dat->state=FS_LEISURE;
			dat->lastact=ticker;
			if (ticker-dat->lastsave>TICKS*60*30) {
				dat->storage_state=5;
				dat->lastsave=ticker;
			}			
		}
		if (msg->type==NT_NPC && msg->dat1==NTID_ARENA && msg->dat2==3 && dat->state==FS_REGISTER) {
                        dat->state=FS_WAIT;
			dat->lastact=-TICKS*60*5;
		}
		if (msg->type==NT_NPC && msg->dat1==NTID_ARENA && msg->dat2==4 && dat->state==FS_ENTER) {
                        dat->state=FS_WAIT2;
			dat->lastact=-TICKS*60*5;
		}
		if (msg->type==NT_NPC && msg->dat1==NTID_ARENA && msg->dat2==5) {
                        dat->state=FS_LEISURE;
			dat->lastact=ticker;			
		}
		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

        if (dat->storage_state>3) {
		switch(dat->state) {
			case FS_LEISURE:	if ((abs(ch[cn].x-ch[cn].tmpx)>2 || abs(ch[cn].y-ch[cn].tmpy)>2) && move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,2)) return;
						if (ticker-dat->lastact<TICKS*60*3) break;
                                                dat->state++;
						break;
			case FS_START:		if (abs(ch[cn].x-MASTER_POSX)<5 && abs(ch[cn].y-MASTER_POSY)<5) dat->state++;
						else if (move_driver(cn,MASTER_POSX,MASTER_POSY,4)) return;
						break;
			case FS_REGISTER:	if (ticker-dat->lastact<TICKS*30) break;
						say(cn,"register");
						dat->lastact=ticker;
						//dat->state++;
						break;
			case FS_WAIT:		break;
			case FS_ENTER:		if (ticker-dat->lastact<TICKS*30) break;
						say(cn,"enter");
						dat->lastact=ticker;
						//dat->state++;
						break;
			case FS_WAIT2:		break;
			case FS_FIGHT:		if (fight_driver_attack_visible(cn,0)) return;
						if (fight_driver_follow_invisible(cn)) return;
                                                break;

		}
	}

	if (spell_self_driver(cn)) return;

	// storage management
        switch(dat->storage_state) {
		case 0:		if (create_storage(dat->storage_ID,"Arena Fighter Data",&dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 1:		if (check_create_storage()) dat->storage_state++;
				break;
		case 2:		if (read_storage(dat->storage_ID,dat->storage_version)) dat->storage_state++;
				break;
		case 3:		res=check_read_storage(&dat->storage_version,&tmp,&size);
				if (res==1 && size==sizeof(dat->storage_data)) {
					memcpy(&dat->storage_data,tmp,sizeof(dat->storage_data));
					xfree(tmp);
					dat->storage_state++;
					//charlog(cn,"read storage: score=%d, wins=%d, losses=%d",dat->storage_data.ppd.score,dat->storage_data.ppd.wins,dat->storage_data.ppd.losses);
					*ppd=dat->storage_data.ppd;
				} else if (res) {
					elog("check storage said %d (%d)",res,size);
					dat->storage_state++;
				}
				break;
		case 4:		break;	// no op
		case 5:		dat->storage_data.ppd=*ppd;
				if (update_storage(dat->storage_ID,dat->storage_version,&dat->storage_data,sizeof(dat->storage_data))) dat->storage_state++;
				break;
		case 6:		if (check_update_storage()) {
					dat->storage_state++;
					dat->storage_version++;
					//charlog(cn,"update storage: score=%d, wins=%d, losses=%d",dat->storage_data.ppd.score,dat->storage_data.ppd.wins,dat->storage_data.ppd.losses);
				}
				break;
		case 7:		dat->storage_state=4;
				break;
		default:	dat->storage_state=4;
				break;
	}

        do_idle(cn,TICKS);
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

void toplist_driver(int in,int cn)
{
	struct arena_ppd *ppd;
	int n,m;

	if (!cn) return;
	if (!tops) return;

	ppd=set_data(cn,DRD_ARENA_PPD,sizeof(struct arena_ppd));
	if (!ppd) return;
	if (!ppd->fights) ppd->score=-2000;

	for (n=0; n<10; n++) {
		if (!tops->entry[n].name[0]) break;
		log_char(cn,LOG_SYSTEM,0,"%d: %s %d",n+1,tops->entry[n].name,tops->entry[n].score);
	}
	for (n=10; n<100; n++) {
		if (!tops->entry[n].name[0]) break;
		if (tops->entry[n].score<ppd->score) break;		
	}
	for (m=max(10,n-5); m<min(100,n+5); m++) {
		if (!tops->entry[m].name[0]) break;
		log_char(cn,LOG_SYSTEM,0,"%d: %s %d",m+1,tops->entry[m].name,tops->entry[m].score);
	}

	log_char(cn,LOG_SYSTEM,0,"Your score is %d, you have won %d fights and lost %d fights.",
		 ppd->score,ppd->wins,ppd->losses);
}

struct manager_data
{
	int last_talk;
        int amgivingback;

	int renter;
	int timeout;
	char invite[80];

	int arena_x;
	int arena_y;

	int arena_fx;
	int arena_fy;

	int arena_tx;
	int arena_ty;
};

void manager_parse(int cn,struct manager_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"arenax")) dat->arena_x=atoi(value);
		else if (!strcmp(name,"arenay")) dat->arena_y=atoi(value);
		else if (!strcmp(name,"arenafx")) dat->arena_fx=atoi(value);
		else if (!strcmp(name,"arenafy")) dat->arena_fy=atoi(value);
		else if (!strcmp(name,"arenatx")) dat->arena_tx=atoi(value);
		else if (!strcmp(name,"arenaty")) dat->arena_ty=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

int is_anybody_in(int cn,int fx,int fy,int tx,int ty)
{
	int x,y,co;

	for (y=fy; y<=ty; y++) {
		for (x=fx; x<=tx; x++) {
			if ((co=map[x+y*MAXMAP].ch) && (ch[co].flags&CF_PLAYER)) {
				return 1;
			}
		}
	}
	return 0;
}

void manager_driver(int cn,int ret,int lastact)
{
	struct manager_data *dat;
        int co,in,talkdir=0,didsay=0,n;
	struct msg *msg,*next;	
	char *ptr;

        dat=set_data(cn,DRD_ARENAMANAGER,sizeof(struct manager_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			if (ch[cn].arg) manager_parse(cn,dat);
			ch[cn].arg=NULL;
			dat->timeout=-TICKS*60*5;
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (co==dat->renter && (ch[co].x<232 || ch[co].x>238 || ch[co].y<dat->arena_fy || ch[co].y>dat->arena_ty)) {
				dat->renter=0;
				dat->invite[0]=0;
			}

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (ch[co].x<=dat->arena_fx || ch[co].x>=dat->arena_tx ||
			    ch[co].y<=dat->arena_fy || ch[co].y>=dat->arena_ty)
				{ remove_message(cn,msg); continue; }

                        switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
				case 6:		// rent
                                                if (is_anybody_in(cn,232,dat->arena_fy,238,dat->arena_ty)) {
							say(cn,"Sorry, this arena is already occupied.");
							break;
						}
						dat->renter=co;
						dat->invite[0]=0;
						dat->timeout=ticker+TICKS*60*5;
						
						teleport_char_driver(co,dat->arena_x,dat->arena_y);
						say(cn,"Say 'invite: <name>' to let someone in, or say 'leave' to leave the arena.");
                                                break;
                                case 5:		//leave
						teleport_char_driver(co,ch[cn].x,ch[cn].y);
						if (co==dat->renter) {
							dat->renter=0;
							dat->invite[0]=0;
						}
						break;
				case 4:		// enter
						if (!strcasecmp(ch[co].name,dat->invite)) {
							teleport_char_driver(co,dat->arena_x,dat->arena_y);
							ch[co].flags&=~CF_LAG;
							dat->invite[0]=0;
						} else say(cn,"You have not been invited, %s.",ch[co].name);
						break;
	
			}
			if ((ptr=strcasestr((char*)msg->dat2,"invite:"))) {				
				if (co!=dat->renter) {
					say(cn,"This is not your arena, %s.",ch[co].name);
				} else {
					ptr+=7;
					while (isspace(*ptr)) ptr++;
					for (n=0; n<79; n++) {
						if (!*ptr || *ptr=='"') break;
						dat->invite[n]=*ptr++;
					}
					dat->invite[n]=0;

					say(cn,"%s, say 'enter' if you wish to enter the arena",dat->invite);
				}
			}
                        if (didsay) {
				dat->last_talk=ticker;
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);				
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

	if (ch[cn].citem) {
		charlog(cn,"oops: destroying item %s",it[ch[cn].citem].name);
		destroy_item(ch[cn].citem);
		ch[cn].citem=0;		
	}

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*10<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;		
	}

        do_idle(cn,TICKS);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_ARENAMASTER:	master_driver(cn,ret,lastact); return 1;
		case CDR_ARENAFIGHTER:	fighter_driver(cn,ret,lastact); return 1;
		case CDR_ARENAMANAGER:	manager_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_TOPLIST:	toplist_driver(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_ARENAMASTER:	immortal_dead(cn,co); return 1;
		case CDR_ARENAMANAGER:	immortal_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                case CDR_ARENAFIGHTER:	return 1;

		default:		return 0;
	}
}






