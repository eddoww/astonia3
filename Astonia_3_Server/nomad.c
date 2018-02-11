/*
 * $Id: nomad.c,v 1.2 2006/09/26 10:59:25 devel Exp $
 *
 * $Log: nomad.c,v $
 * Revision 1.2  2006/09/26 10:59:25  devel
 * added questlog to nomad plains and brannington forest
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
#include "bank.h"
#include "player_driver.h"
#include "act.h"
#include "consistency.h"
#include "nomad_ppd.h"
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
	{{"hello",NULL},"Sul vana ley, %s.",0},
	{{"hi",NULL},"Sul vana ley, %s.",0},
	{{"greetings",NULL},"Sul vana ley, %s.",0},
	{{"hail",NULL},"Sul vana ley, %s.",0},
	{{"whats","up",NULL},"Everything that isn't nailed down.",0},
        {{"what's","up",NULL},"Everything that isn't nailed down.",0},
	{{"what","is","up",NULL},"Everything that isn't nailed down.",0},
	{{"llakal","sla",NULL},"Llakal Sla is a game played with three dice. The opponents agree on a bet, and the one throwing the higher number wins. If thou wishest to play, say: 'bet <amount>', where amount is in ounces of salt.",0},
	{{"repeat",NULL},NULL,2},
	{{"cheap","dice",NULL},NULL,3},
	{{"mediocre","dice",NULL},NULL,4},
	{{"good","dice",NULL},NULL,5},
	{{"golden","statue",NULL},NULL,6}
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

	if (char_dist(cn,co)>11) return 0;

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

//----------

struct nomad_data
{
	int nr;
	int dice_skill,min_bet,max_bet;
	int lasttalk;

	int play_with;
	int play_timer;
	int bet;
	int my_throw;
	int max_loss;
};

void set_salt_data(int in)
{
	if (*(unsigned int*)(it[in].drdata+0)>=10000) it[in].sprite=13212;
	else if (*(unsigned int*)(it[in].drdata+0)>=1000) it[in].sprite=13211;
	else if (*(unsigned int*)(it[in].drdata+0)>=100) it[in].sprite=13210;
	else if (*(unsigned int*)(it[in].drdata+0)>=10) it[in].sprite=13209;
	else it[in].sprite=13208;
	
	sprintf(it[in].description,"%d ounces of %s.",*(unsigned int*)(it[in].drdata),it[in].name);
}

void remove_salt(int cn,int val)
{
	int in,n,price;

	for (n=30; n<INVENTORYSIZE; n++) {
		if ((in=ch[cn].item[n]) && it[in].ID==IID_AREA19_SALT) {
			if (*(unsigned int*)(it[in].drdata)<=val) {
				val-=*(unsigned int*)(it[in].drdata);
				if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because player lost nomad game");
				remove_item_char(in);
				destroy_item(in);
			} else {
				price=it[in].value/(*(unsigned int*)(it[in].drdata));
				*(unsigned int*)(it[in].drdata)-=val;
				it[in].value=price**(unsigned int*)(it[in].drdata);
				set_salt_data(in);

				val=0;
			}
		}
		if (val<=0) break;		
	}
}

int count_salt(int cn)
{
	int cnt,n,in;

	for (cnt=0,n=30; n<INVENTORYSIZE; n++) {
		if ((in=ch[cn].item[n]) && it[in].ID==IID_AREA19_SALT) cnt+=*(unsigned int*)(it[in].drdata);
	}

	return cnt;
}

void nomad_parse(int cn,struct nomad_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {
                if (!strcmp(name,"nr")) { dat->nr=atoi(value); }
		else if (!strcmp(name,"diceskill")) { dat->dice_skill=atoi(value); }
		else if (!strcmp(name,"minbet")) { dat->min_bet=atoi(value); }
		else if (!strcmp(name,"maxbet")) { dat->max_bet=atoi(value); }
		else if (!strcmp(name,"maxloss")) { dat->max_loss=atoi(value); }
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

int nomad_1(int cn,int co,struct nomad_ppd *ppd,int nr)
{
        if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}
	
	switch(ppd->nomad_state[nr]) {
		case 0:		say(cn,"Sul vana ley, %s. I am %s.",ch[co].name,ch[cn].name);
				questlog_open(co,32);
				ppd->nomad_state[nr]++;
				return 1;
		case 1:		say(cn,"Welcome to the plains of the Vana Laka. Thou wouldst best learn about our customs, before thou venturest further north, %s.",ch[co].name);
				ppd->nomad_state[nr]++;
				return 1;
		case 2:		say(cn,"We value not what the city-folks call money. We have no use for it. Most of our trades work in skins or salt for both are important for our survival.");
				ppd->nomad_state[nr]++;
				return 1;
		case 3:		say(cn,"We are fierce warriors, and our shaman's magic is deadly. Each tribe of the Vana Laka is loyal only to its members, and, to a certain degree, to the members of the other tribes.");
				ppd->nomad_state[nr]++;
				return 1;
		case 4:		say(cn,"Those who are tribe-less will have a hard time finding trade partners, or opponents for °c4Llakal Sla°c0.");
				ppd->nomad_state[nr]++;
				return 1;
		case 5:		say(cn,"If thou wishest to earn membership in my tribe, the Vana Kiru, thou must prove thy worth to us. Collect 100 ounces of salt and hand them to me, and I will welcome thee to the Vana Kiru.");
				ppd->nomad_state[nr]++;
				return 1;
		case 6:		say(cn,"I will also trade any wolf skins thou might find for salt, in spite of thee being tribe-less. Thou canst find wolves to the north-east, or thou canst go north-west, to my tribe.");
				ppd->nomad_state[nr]++;
				return 1;
		case 7:		say(cn,"Kan vana ley, %s - go in peace.",ch[co].name);
				ppd->nomad_state[nr]++;
				return 1;
		case 8:		return 0;	// waiting for salt
		case 9:		say(cn,"Mayest thy step be light and thy pockets filled with salt.");
				ppd->nomad_state[nr]++;
				return 1;
		case 10:	return 0;	// we're done.
	}
	return 0;
}

int nomad_2(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}
	
	switch(ppd->nomad_state[nr]) {
		case 0:		if (!(ppd->tribe_member&TM_TRIBE1)) break;
				say(cn,"Sul vana ley, %s. I am %s.",ch[co].name,ch[cn].name);
				ppd->nomad_state[nr]++;
				return 1;
		case 1:		say(cn,"I have a nice collection of dice. I'd sell thee a set of °c4cheap dice°c0 for 200 ounces of salt, or a set of °c4mediocre dice°c0 for 500 ounces, or a set of °c4 good dice°c0 for 1200 ounces.");
				ppd->nomad_state[nr]++;
				return 1;
		case 2:         return 0; // finished
	}
	return 0;
}

int nomad_3(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}
	
	switch(ppd->nomad_state[nr]) {
		case 0:		if (!(ppd->tribe_member&TM_TRIBE1)) break;
				say(cn,"Sul vana ley, %s. I am %s.",ch[co].name,ch[cn].name);
				ppd->nomad_state[nr]++;
				return 1;
		case 1:		say(cn,"Would you like a game of °c4Llakal Sla°c0?");
				ppd->nomad_state[nr]++;
				return 1;
		case 2:         return 0;	// finished
	}
	return 0;
}

int nomad_4(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}
	
	switch(ppd->nomad_state[nr]) {
		case 0:		say(cn,"Welcome to the monastery of Kir Laka, %s. I am %s.",ch[co].name,ch[cn].name);
				questlog_open(co,33);
				ppd->nomad_state[nr]++;
				return 1;
		case 1:		say(cn,"It is seldom indeed that we have visitors in these sad times. The mountains have never been friendly, but only a short while ago all one had to fear was the cold, or losing one's way. Now one has to fear being eaten alive by Harpies. Welcome again, %s. It is good to see that there are %s brave enough to visit us.",ch[co].name,(ch[co].flags&CF_MALE) ? "men" : "women");
				ppd->nomad_state[nr]++;
				return 1;
		case 2:		say(cn,"Unfortunately, the Harpies are not our only problem. Brother Sarkilar has left the monastery with a few of the younger brothers. We haven't heard from him since he left, and I am worried. Neither the nomads nor the valkyries have seen him, so he must still be in this mountain. If thou couldst find out what happened to him?");
				ppd->nomad_state[nr]++;
				return 1;
		case 3:		return 0;	// waiting for news about sarkilar
		case 4:		say(cn,"Oh, Sarkilar! What hast thou done? How couldst thou fall for the silver tongue of evil? I thank thee, %s, even though thine news is sad indeed.",ch[co].name);
				ppd->nomad_state[nr]++;
				return 1;
                case 5:		return 0;	// done
	}
	return 0;
}

int nomad_5(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}
	
	switch(ppd->nomad_state[nr]) {
		case 0:		say(cn,"Welcome to the monastery of Kir Laka, %s. I am %s, the teacher.",ch[co].name,ch[cn].name);
				questlog_open(co,34);
				ppd->nomad_state[nr]++;
				return 1;
		case 1:		say(cn,"We of the Kir believe in life, and in the mastery of life. The concept of god-hood is alien to us; and while we value Ishtar's fight against the demonic hordes, we do not believe he is a god. But enough of Ishtar.");
				ppd->nomad_state[nr]++;
				return 1;
		case 2:		say(cn,"I can teach thee about life, in exchange for a golden statue of Kir. One of the nomads sells them.");
				ppd->nomad_state[nr]++;
				return 1;
		case 3:		return 0;	// waiting for statue
		case 4:		say(cn,"If thou ever needst to regain lost experiences, I can help thee in the process. I won't be able to bring it all back, but most of it. Do not forget to bring a golden Kir statue...");
				ppd->nomad_state[nr]++;
				return 1;
		case 5:		return 0;	// waiting for statue
	}
	return 0;
}

int nomad_6(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}
	
	switch(ppd->nomad_state[nr]) {
		case 0:		if (!(ppd->tribe_member&TM_TRIBE1)) break;
				say(cn,"Sul vana ley, %s. I am %s.",ch[co].name,ch[cn].name);
				ppd->nomad_state[nr]++;
				return 1;
		case 1:		say(cn,"Wouldst thou like to buy a °c4golden statue°c0? I shall give it to thee for only 10000 ounces of salt!");
				ppd->nomad_state[nr]++;
				return 1;
		case 2:         return 0;	// finished
	}
	return 0;
}


void nomad_1_repeat(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ppd->nomad_state[nr]>=0 && ppd->nomad_state[nr]<=8) { ppd->nomad_state[nr]=0; return; }
	if (ppd->nomad_state[nr]>=9 && ppd->nomad_state[nr]<=10) { ppd->nomad_state[nr]=9; return; }
}

void nomad_2_repeat(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ppd->nomad_state[nr]>=0 && ppd->nomad_state[nr]<=2) { ppd->nomad_state[nr]=0; return; }
}

void nomad_3_repeat(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ppd->nomad_state[nr]>=0 && ppd->nomad_state[nr]<=2) { ppd->nomad_state[nr]=0; return; }
}

void nomad_4_repeat(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ppd->nomad_state[nr]>=0 && ppd->nomad_state[nr]<=3) { ppd->nomad_state[nr]=0; return; }
	if (ppd->nomad_state[nr]>=4 && ppd->nomad_state[nr]<=5) { ppd->nomad_state[nr]=4; return; }
}

void nomad_5_repeat(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ppd->nomad_state[nr]>=0 && ppd->nomad_state[nr]<=3) { ppd->nomad_state[nr]=0; return; }
	if (ppd->nomad_state[nr]>=4 && ppd->nomad_state[nr]<=5) { ppd->nomad_state[nr]=4; return; }
}

void nomad_6_repeat(int cn,int co,struct nomad_ppd *ppd,int nr)
{
	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ppd->nomad_state[nr]>=0 && ppd->nomad_state[nr]<=3) { ppd->nomad_state[nr]=0; return; }
}

void nomad_2_text(int cn,int co,struct nomad_ppd *ppd,int nr,int res)
{
	int cost=0,in;
	char *name=NULL;

	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (!(ppd->tribe_member&TM_TRIBE1)) return;
	
	if (res==3) { cost=200; name="dice0"; }
	if (res==4) { cost=500; name="dice1"; }
	if (res==5) { cost=1200; name="dice2"; }

	if (!cost) return;
	
	if (count_salt(co)<cost) {
		say(cn,"But thou dost not have enough salt to pay.");
		return;
	}
	in=create_item(name);
	if (in) {
		if (!give_char_item(co,in)) {
			destroy_item(in);
		} else {
			remove_salt(co,cost);
			say(cn,"It's a pleasure doing business with thee, %s.",ch[co].name);
		}
	}
}

void nomad_6_text(int cn,int co,struct nomad_ppd *ppd,int nr,int res)
{
	int cost=0,in;
	char *name=NULL;

	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (!(ppd->tribe_member&TM_TRIBE1)) return;
	if (res!=6) return;

	cost=10000; name="kir_statue";
	
	if (count_salt(co)<cost) {
		say(cn,"But thou dost not have enough salt to pay.");
		return;
	}
	in=create_item(name);
	if (in) {
		if (!give_char_item(co,in)) {
			destroy_item(in);
		} else {
			remove_salt(co,cost);
			say(cn,"It's a pleasure doing business with thee, %s.",ch[co].name);
		}
	}
}

int nomad_1_give(int cn,int co,int in,struct nomad_ppd *ppd,int nr)
{
	int cnt,val,in2;

	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}

	if (it[in].ID==IID_AREA19_SALT) {
		if (ppd->nomad_state[nr]>8) {
			say(cn,"Thou art already a member of my tribe, %s.",ch[co].name);
			return 0;
		}
		if (*(unsigned int*)(it[in].drdata)<100) {
			say(cn,"This is not enough. Thou needst give me 100 ounces of salt for thy membership.");
			return 0;
		}
		if (*(unsigned int*)(it[in].drdata)>100) {
			say(cn,"This is most generous, but I wilt not accept more than 100 ounces for thy membership.");
			return 0;
		}
		say(cn,"Welcome to the tribe of the Vana Kiru, %s.",ch[co].name);
		questlog_done(co,32);
		ppd->nomad_state[nr]=9;
		ppd->tribe_member|=TM_TRIBE1;
		destroy_item(in);
		return 1;
	}
	
	if (it[in].ID==IID_AREA19_WOLFSSKIN || it[in].ID==IID_AREA19_WOLFSSKIN2) {
		cnt=*(unsigned int*)(it[in].drdata);
		if (it[in].ID==IID_AREA19_WOLFSSKIN) val=cnt*5;
		else val=cnt*20;
		
		in2=create_item("salt");
		if (!in2) {
			say(cn,"Oopsy");
			return 0;
		}
		it[in2].value*=val;
		*(unsigned int*)(it[in2].drdata)=val;
		set_salt_data(in2);
		if (give_char_item(co,in2)) {
			say(cn,"Here, %d ounces of salt for these skins.",val);
			destroy_item(in);
			return 1;
		}
		say(cn,"Oops.");
		destroy_item(in2);
		return 0;
	}

	return 0;
}

int nomad_4_give(int cn,int co,int in,struct nomad_ppd *ppd,int nr)
{
        if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}

	if (it[in].ID==IID_AREA19_KIRLETTER && ppd->nomad_state[nr]<=3) {
		ppd->nomad_state[nr]=4;
		questlog_done(co,33);
                destroy_item(in);
		destroy_item_byID(co,IID_AREA19_KIRLETTER);
		return 1;
	}

	return 0;
}

int nomad_5_give(int cn,int co,int in,struct nomad_ppd *ppd,int nr)
{
	int diff;

	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return 0;
	}

	if (it[in].ID==IID_AREA19_KIR) {
		if (ppd->nomad_state[nr]>3) {
			if (ch[co].exp>ch[co].exp_used) {
				say(cn,"But thou dost not have lost any experience.");
				return 0;
			}
			say(cn,"There, some of the memories are back. I can work with thee further, if thou bringst me another of these statues.");
			diff=ch[co].exp_used-ch[co].exp;
			if (ch[co].flags&CF_HARDCORE) give_exp(co,diff/10);
			else give_exp(co,diff/2);

		} else {
			say(cn,"Isn't it beautiful? Thank thee, %s. Now, let me teach thee...",ch[co].name);
			questlog_done(co,34);
                        ppd->nomad_state[nr]=4;
		}
		destroy_item(in);
		return 1;
	}

	return 0;
}

int lucky_die(int sides,int luck)
{
	int n,val,tmp;

	for (n=val=0; n<=luck; n++) {
		tmp=die(1,sides);
		if (tmp>val) val=tmp;		
	}
	return val;
}

void nomad_bet(int cn,struct nomad_data *dat,int co,int val,struct nomad_ppd *ppd,int nr)
{
	int d1,d2,d3;

	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	if (ticker-dat->play_timer>TICKS*60) dat->play_with=0;
        if (dat->play_with && !char_see_char(cn,dat->play_with)) dat->play_with=0;
	
	if (dat->play_with) {
		say(cn,"Sorry, I'm playing with %s right now.",ch[dat->play_with].name);
		return;
	}
	if (ppd->nomad_win[nr]<-dat->max_loss) {
		say(cn,"I won't play with thee anymore, %s. Thou art too lucky for my taste.",ch[co].name);
		return;
	}
	
	if (val<dat->min_bet) {
		say(cn,"%d ounces? That's too cheap.",val);
		return;
	}
	if (val>dat->max_bet) {
		say(cn,"%d ounces is too much for my taste.",val);
		return;
	}

	if (val>count_salt(co)) {
		say(cn,"Thou dost not have %d ounces of salt to play with, %s.",val,ch[co].name);
		return;
	}

	dat->play_with=co;
	dat->play_timer=ticker;
	dat->bet=val;

        if (ppd->open_bet) {
		d1=ppd->open_roll1; d2=ppd->open_roll1; d3=ppd->open_roll1;
	} else {
		d1=lucky_die(6,dat->dice_skill); d2=lucky_die(6,dat->dice_skill); d3=lucky_die(6,dat->dice_skill);
	}
	log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,8,"%s rolled %d, %d and %d for a total of %d.",ch[cn].name,d1,d2,d3,d1+d2+d3);
	
	dat->my_throw=d1+d2+d3;
	if (dat->my_throw<11) {
		say(cn,"Ack. Well, roll your dice now, %s. (USE the dice)",ch[co].name);
	} else if (dat->my_throw<14) {
		say(cn,"Your turn, %s. (USE the dice)",ch[co].name);
	} else {
		say(cn,"Ha! Now it's your turn, %s. (USE the dice)",ch[co].name);
	}

	// remember high dice rolls in case player walks away from them
	if (d1+d2+d3>13) {
		ppd->open_bet=val;
		ppd->open_roll1=d1;
		ppd->open_roll2=d2;
		ppd->open_roll3=d3;
	}
}

void nomad_roll(int cn,struct nomad_data *dat,int co,int val,struct nomad_ppd *ppd,int nr)
{
	int in2;

	if (nr<0 || nr>=MAXNOMAD) {
		elog("nomad_sub: illegal nr %d",nr);
		return;
	}

	dat->play_with=0;
	
	if (dat->bet>=ppd->open_bet) ppd->open_bet=0;
	else ppd->open_bet-=dat->bet;

	if (dat->bet>count_salt(co)) {
		say(cn,"Thou dost not have %d ounces of salt to play with, %s. The game's cancelled.",val,ch[co].name);
		return;
	}

	if (dat->my_throw>val) {
		say(cn,"It's a pleasure playing with thee, %s.",ch[co].name);
		remove_salt(co,dat->bet);
		ch[co].flags|=CF_ITEMS;

		ppd->nomad_win[nr]+=dat->bet;
		return;
	}
	if (dat->my_throw<val) {
		say(cn,"Dang. Lost again.");
		in2=create_item("salt");
		if (!in2) {
			say(cn,"Oopsy");
			return;
		}
		it[in2].value*=dat->bet;
		*(unsigned int*)(it[in2].drdata)=dat->bet;
		set_salt_data(in2);
		
		if (give_char_item(co,in2)) {
			say(cn,"Here, %d ounces of salt.",dat->bet);
			ppd->nomad_win[nr]-=dat->bet;
			return;
		}
		say(cn,"Oops.");
		destroy_item(in2);
                return;
	}
	say(cn,"Oh, a draw. No winner.");
}

void nomad(int cn,int ret,int lastact)
{
	struct nomad_data *dat;
	struct nomad_ppd *ppd;
	struct msg *msg,*next;
	int co,cc,val,in,res,talkdir=0,didsay=0;
	char *ptr;

        dat=set_data(cn,DRD_NOMADDRIVER,sizeof(struct nomad_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
                        if (ch[cn].arg) { nomad_parse(cn,dat); ch[cn].arg=NULL; }
                }

		if (msg->type==NT_CHAR) {
			co=msg->dat1;
                        if ((ch[co].flags&CF_PLAYER) && ticker-dat->lasttalk>TICKS*5 && char_dist(cn,co)<12 && char_see_char(cn,co) && (ppd=set_data(co,DRD_NOMAD_PPD,sizeof(struct nomad_ppd)))) {
				//say(cn,"nr=%d, state=%d",dat->nr,ppd->nomad_state[dat->nr]);
				switch(dat->nr) {
					case 1:		didsay=nomad_1(cn,co,ppd,dat->nr); break;
					case 2:		didsay=nomad_2(cn,co,ppd,dat->nr); break;
					case 3:		didsay=nomad_3(cn,co,ppd,dat->nr); break;
					case 4:		didsay=nomad_4(cn,co,ppd,dat->nr); break;
					case 5:		didsay=nomad_5(cn,co,ppd,dat->nr); break;
					case 6:		didsay=nomad_6(cn,co,ppd,dat->nr); break;


					default:	break;
				}
				dat->lasttalk=ticker;
				if (didsay) talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);				
			}
		}

                if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if ((ppd=set_data(co,DRD_NOMAD_PPD,sizeof(struct nomad_ppd)))) {
				switch((res=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:	
						switch(dat->nr) {
							case 1:		nomad_1_repeat(cn,co,ppd,dat->nr); dat->lasttalk=0; break;
							case 2:		nomad_2_repeat(cn,co,ppd,dat->nr); dat->lasttalk=0; break;
							case 3:		nomad_3_repeat(cn,co,ppd,dat->nr); dat->lasttalk=0; break;
							case 4:		nomad_4_repeat(cn,co,ppd,dat->nr); dat->lasttalk=0; break;
							case 5:		nomad_5_repeat(cn,co,ppd,dat->nr); dat->lasttalk=0; break;
							case 6:		nomad_6_repeat(cn,co,ppd,dat->nr); dat->lasttalk=0; break;


						}
						break;
					case 3:
					case 4:
					case 5:	
					case 6:
						switch(dat->nr) {
							case 2:		nomad_2_text(cn,co,ppd,dat->nr,res); dat->lasttalk=0; break;
							case 6:		nomad_6_text(cn,co,ppd,dat->nr,res); dat->lasttalk=0; break;

						}
					default:	break;
				}
			}
			if ((ptr=strstr((char*)msg->dat2,"bet ")) && char_dist(cn,co)<12 && char_see_char(cn,co)) {
				val=atoi(ptr+4);
				if (val) {
					switch(dat->nr) {
						case 1:		if (ppd->nomad_state[dat->nr]>=9) nomad_bet(cn,dat,co,val,ppd,dat->nr);
								else say(cn,"Sorry, I do not play with strangers.");
								break;
						case 2:		if (ppd->tribe_member&TM_TRIBE1)  nomad_bet(cn,dat,co,val,ppd,dat->nr);
								else say(cn,"Sorry, I do not play with strangers.");
								break;
						case 3:		if (ppd->tribe_member&TM_TRIBE1)  nomad_bet(cn,dat,co,val,ppd,dat->nr);
								else say(cn,"Sorry, I do not play with strangers.");
								break;
						case 6:		if (ppd->tribe_member&TM_TRIBE1)  nomad_bet(cn,dat,co,val,ppd,dat->nr);
								else say(cn,"Sorry, I do not play with strangers.");
								break;
						default:	say(cn,"I do not play.");
								break;
					}
				}
			}
			
			tabunga(cn,co,(char*)msg->dat2);
		}
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				if ((ppd=set_data(co,DRD_NOMAD_PPD,sizeof(struct nomad_ppd)))) {
					switch(dat->nr) {
						case 1:		if (!nomad_1_give(cn,co,in,ppd,dat->nr) && !give_char_item(co,in)) {
									destroy_item(in);
								}
								ch[cn].citem=0;
								break;
						case 4:		if (!nomad_4_give(cn,co,in,ppd,dat->nr) && !give_char_item(co,in)) {
									destroy_item(in);
								}
								ch[cn].citem=0;
								break;
						case 5:		if (!nomad_5_give(cn,co,in,ppd,dat->nr) && !give_char_item(co,in)) {
									destroy_item(in);
								}
								ch[cn].citem=0;
								break;
						
						default:	if (!give_char_item(co,in)) {
									destroy_item(in);
								}
								ch[cn].citem=0;
								break;
					}
				} else {
					destroy_item(in);
					ch[cn].citem=0;
				}
			}
		}

		if (msg->type==NT_GOTHIT) {
                        co=msg->dat1;
		}
		if (msg->type==NT_SEEHIT) {
			cc=msg->dat1;
			co=msg->dat2;			
		}

		if (msg->type==NT_NPC) {
			if (msg->dat1==NTID_DICE) {
				co=msg->dat2;
				val=msg->dat3;
				if (co==dat->play_with && (ppd=set_data(co,DRD_NOMAD_PPD,sizeof(struct nomad_ppd)))) nomad_roll(cn,dat,co,val,ppd,dat->nr);
			}
		}

                standard_message_driver(cn,msg,0,0);
		remove_message(cn,msg);
	}

        if (spell_self_driver(cn)) { return; }
	if (regenerate_driver(cn)) { return; }

	if (talkdir) turn(cn,talkdir);
	if (ticker-dat->lasttalk>TICKS*10 && secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	do_idle(cn,TICKS);
}


void nomad_dice(int in,int cn)
{
	int d1,d2,d3,luck;

	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried

	luck=it[in].drdata[0];

	d1=lucky_die(6,luck); d2=lucky_die(6,luck); d3=lucky_die(6,luck);
	log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,8,"%s rolled %d, %d and %d for a total of %d.",ch[cn].name,d1,d2,d3,d1+d2+d3);
	notify_area(ch[cn].x,ch[cn].y,NT_NPC,NTID_DICE,cn,d1+d2+d3);
}

void madhermit_driver(int cn,int ret,int lastact)
{
        //struct simple_baddy_driver_data *dat;
	struct msg *msg,*next;
        int co,in;

	//dat=set_data(cn,DRD_SIMPLEBADDYDRIVER,sizeof(struct simple_baddy_driver_data));
	//if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,30,0,60);			
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			if (ch[co].action==AC_USE && (in=ch[co].act1) && in>0 && in<MAXITEM && it[in].driver==IDR_FLOWER) {
				if (fight_driver_add_enemy(cn,co,1,1)) shout(cn,"Hey! %s! Those flowers are mine!",ch[co].name);
			}
		}

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

	
        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	if (regenerate_driver(cn)) return;

	if (spell_self_driver(cn)) return;

        do_idle(cn,TICKS/2);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_NOMAD:		nomad(cn,ret,lastact); return 1;
		case CDR_MADHERMIT:	madhermit_driver(cn,ret,lastact); return 1;
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {		
		case IDR_NOMADDICE:	nomad_dice(in,cn); return 1;
                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_NOMAD:			return 1;
		case CDR_MADHERMIT:		return 1;

                default:			return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_NOMAD:			return 1;
		case CDR_MADHERMIT:		return 1;
                default:			return 0;
	}
}


