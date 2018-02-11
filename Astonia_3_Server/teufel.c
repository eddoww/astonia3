/*
 * $Id: teufel.c,v 1.20 2008/03/24 11:23:17 devel Exp $
 *
 * $Log: teufel.c,v $
 * Revision 1.20  2008/03/24 11:23:17  devel
 * less points while lagging
 *
 * Revision 1.19  2006/07/16 19:43:53  ssim
 * changed points per rat kill formula from x^4 to x^2
 *
 * Revision 1.18  2006/07/16 19:40:44  ssim
 * added fire and hell logic to rat spawner
 *
 * Revision 1.17  2006/07/16 19:02:09  ssim
 * reduced rat area money reward by half
 * quest items can no longer be worn in PK area
 *
 * Revision 1.16  2006/07/01 10:31:46  ssim
 * *** empty log message ***
 *
 * Revision 1.15  2006/06/28 23:06:35  ssim
 * added PK arena exit
 *
 * Revision 1.14  2006/06/26 23:25:09  ssim
 * minor improvements to rat nest logic
 *
 * Revision 1.13  2006/06/26 22:56:31  ssim
 * lots of small fixes and improvements
 *
 * Revision 1.12  2006/06/26 18:14:11  ssim
 * added logic for rat nest quest
 *
 * Revision 1.11  2006/06/25 12:59:32  ssim
 * added entry locations for teufelheim PK arena 1
 *
 * Revision 1.10  2006/06/23 16:20:13  ssim
 * more work on PK area door logic
 *
 * Revision 1.9  2006/06/20 13:27:40  ssim
 * added basics for arena door driver
 *
 * Revision 1.8  2006/04/07 13:01:33  ssim
 * fixed bug in reward choice
 *
 * Revision 1.7  2006/04/07 11:59:03  ssim
 * added two gamblers
 *
 * Revision 1.6  2005/12/10 14:36:55  ssim
 * finished gambler
 *
 * Revision 1.5  2005/12/07 13:35:47  ssim
 * added basics for demon gambler
 *
 * Revision 1.4  2005/11/27 18:06:06  ssim
 * added section door driver
 *
 * Revision 1.3  2005/11/22 23:42:29  ssim
 * added support for fire and ice demon player sprites
 *
 * Revision 1.2  2005/11/21 15:19:25  ssim
 * added teufeldemon driver
 *
 * Revision 1.1  2005/11/21 12:14:20  ssim
 * Initial revision
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
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "skill.h"
#include "area1.h"
#include "consistency.h"
#include "database.h"
#include "sector.h"
#include "sleep.h"
#include "mission_ppd.h"
#include "btrace.h"
#include "act.h"
#include "spell.h"
#include "player_driver.h"

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
        {{"what's","your","name",NULL},NULL,1},
	{{"what","is","your","name",NULL},NULL,1},
        {{"who","are","you",NULL},NULL,1},
        {{"play",NULL},"I only play for bronze chips. You can °c4bet one°c0 or °c4bet two°c0 or °c4bet five°c0 of them. Then you'll roll three dice and depending on the °c4results°c0 you'll win the most fantastic stuff possible!",0},
	{{"results",NULL},"The dice are twenty-sided and the numbers are added up. If you roll 3 to 20 you win and if you roll 43 to 60 you win, too. Want to hear about the °c4prizes°c0?",0},
	{{"prizes",NULL},"A 3 gets you a Cape of the Warrior. A 60 a Cape of the Mage. +7 if you bet one chip, +14 if you bet two, +21 if you bet all five. Want to hear °c4 more prizes°c0?",0},
	{{"more","prizes",NULL},"With a 4 or a 59 you'll win 100,000 gold (when betting 5 chips, 20,000 for 1 chip, 40,000 for 2 chips). And there are many, many more prizes...",0},

	{{"play2",NULL},"I only play for silver chips. You can °c4bet one°c0 or °c4bet two°c0 or °c4bet five°c0 of them. Then you'll roll three dice and depending on the °c4results2°c0 you'll win the most fantastic stuff possible!",0},
	{{"results2",NULL},"The dice are twenty-sided and the numbers are added up. If you roll 3 to 20 you win and if you roll 43 to 60 you win, too. Want to hear about the °c4prizes2°c0?",0},
	{{"prizes2",NULL},"A 3 gets you boots of the Warrior. A 60 boots of the Mage. +8 if you bet one chip, +15 if you bet two, +22 if you bet all five. Want to hear °c4 more prizes2°c0?",0},
	{{"more","prizes2",NULL},"With a 4 or a 59 you'll win 150,000 gold (when betting 5 chips, 30,000 for 1 chip, 60,000 for 2 chips). And there are many, many more prizes...",0},

	{{"play3",NULL},"I only play for gold chips. You can °c4bet one°c0 or °c4bet two°c0 or °c4bet five°c0 of them. Then you'll roll three dice and depending on the °c4results3°c0 you'll win the most fantastic stuff possible!",0},
	{{"results3",NULL},"The dice are twenty-sided and the numbers are added up. If you roll 3 to 20 you win and if you roll 43 to 60 you win, too. Want to hear about the °c4prizes3°c0?",0},
	{{"prizes3",NULL},"A 3 gets you a helmet of the Warrior. A 60 a hat of the Mage. +9 if you bet one chip, +16 if you bet two, +23 if you bet all five. Want to hear °c4 more prizes3°c0?",0},
	{{"more","prizes3",NULL},"With a 4 or a 59 you'll win 200,000 gold (when betting 5 chips, 40,000 for 1 chip, 80,000 for 2 chips). And there are many, many more prizes...",0},

	{{"bet","one",NULL},NULL,2},
	{{"bet","two",NULL},NULL,3},
	{{"bet","five",NULL},NULL,4},

	{{"repeat",NULL},"Hello, %s! We have a slight rat problem in the caverns to the north. There's a nice °c4reward°c0 for killing some rats.",0},
        {{"reward",NULL},"Yeah. You go kill some rats. The more and bigger the rats you kill, the more points you get in my book. The more points you have, the better the rewards you get. You know, °c4experience°c0, °c4military°c0 knowledge or just plain °c4money°c0 if that's what you want.",0},
	{{"experience",NULL},"Exactly. Experience. The fire-is-hot-so-don't-touch-it kind of experience. °c4Give experience°c0 will exchange your points for experience.",0},
	{{"military",NULL},"That's right. Everything your drill sergeant told you and you can't remember. °c4Give military°c0 will exchange your points for military knowledge.",0},
        {{"money",NULL},"You know, them greenbacks. Oh, wait. Wrong dimension. Money... Ah, right. Round, flat and shiny... Coins! That's it. °c4Give money°c0 will exchange your points for greenbacks. Err, gold coins.",0},

	{{"give","experience",NULL},NULL,5},
	{{"give","military",NULL},NULL,6},
	{{"give","money",NULL},NULL,7},
	{{"give","godly",NULL},NULL,8}
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
					     case 2:	return 2;
					     case 3:	return 3;
					     case 4:	return 4;
					     case 5:	return 5;
					     case 6:	return 6;
					     case 7:	return 7;
					     case 8:	return 8;
				}
				break;
			}
		}
	}
	

        return 1;
}
// ----------

int is_demon(int cn)
{
	if (ch[cn].sprite==27 || ch[cn].sprite==157 || ch[cn].sprite==39) return 1;
	return 0;
}

void teufeldemon_driver(int cn,int ret,int lastact)
{
	struct msg *msg,*next;
        int co;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch(msg->type) {
			case NT_CHAR:
				co=msg->dat1;
				if ((ch[co].flags&CF_PLAYER) && !is_demon(co)) {
					if (is_valid_enemy(cn,co,-1)) fight_driver_add_enemy(cn,co,0,1);
				}
				break;
		}
	}

	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void teufeldoor(int in,int cn)
{
	int x,y,oldx,oldy,dx,dy;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
                return;
	}

	if (ch[cn].sprite!=27 && ch[cn].sprite!=157 && ch[cn].sprite!=39) {
		log_char(cn,LOG_SYSTEM,0,"A demon looks through the view-hole in the door and shouts: \"No humans allowed!\"");
		return;
	}

	if (it[in].drdata[0]==2 && ch[cn].sprite==27) {
		log_char(cn,LOG_SYSTEM,0,"A demon looks through the view-hole in the door and shouts: \"No beggars allowed!\"");
		return;
	}

	if (it[in].drdata[0]==3 && ch[cn].sprite!=39) {
		log_char(cn,LOG_SYSTEM,0,"A demon looks through the view-hole in the door and shouts: \"Only nobles allowed!\"");
		return;
	}

	dx=(ch[cn].x-it[in].x);
	dy=(ch[cn].y-it[in].y);

	if (dx && dy) return;

        x=it[in].x-dx;
	y=it[in].y-dy;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char_extended(cn,x,y,7)) {
                log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,0);		
	}

        switch(ch[cn].dir) {
		case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
		case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
		case DX_UP:	ch[cn].dir=DX_DOWN; break;
		case DX_DOWN:	ch[cn].dir=DX_UP; break;
	}
}

void teufelarenaexit(int in,int cn)
{
	int x,y,oldx,oldy;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
                return;
	}

	if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE) {
		log_char(cn,LOG_SYSTEM,0,"You cannot leave with less than full health.");
		return;
	}

        x=206;
	y=231;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char_extended(cn,x,y,7)) {
                log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,0);		
	}
}

void teufelarena(int in,int cn)
{
	int x,y,oldx,oldy,nr,r,n,in2;

        if (!cn) {	// always make sure its not an automatic call if you don't handle it
                return;
	}

	nr=it[in].drdata[0];
	if (nr<1 || nr>1) return;
	r=RANDOM(8);
	switch(nr*10+r) {
		case 10:	x=154; y=215; break;
		case 11:	x=134; y=220; break;
		case 12:	x=167; y=196; break;
		case 13:	x=186; y=221; break;
		case 14:	x=212; y=223; break;
		case 15:	x=228; y=224; break;
		case 16:	x=247; y=220; break;
		case 17:	x=237; y=198; break;
		default:	elog("error in teufelarena.c #455"); x=250; y=250; break;
	}

	if (nr==1 && ch[cn].sprite!=27) {
		log_char(cn,LOG_SYSTEM,0,"You need to wear an earth demon suit.");
		return;
	}
	if (nr==1 && ch[cn].level>38) {
		log_char(cn,LOG_SYSTEM,0,"Max Level 38, sorry.");
		return;
	}

	for (n=0; n<12; n++) {
                if (!(in2=ch[cn].item[n])) continue;
                if (it[in2].sprite>=53001 && it[in2].sprite<=53006) continue; // edemon suit
		if (nr>1 && it[in2].sprite>=53031 && it[in2].sprite<=53036) continue; // fdemon suit
		if (nr>2 && it[in2].sprite>=53025 && it[in2].sprite<=53030) continue; // udemon suit
		if (count_enhancements(in2)>1) {
			log_char(cn,LOG_SYSTEM,0,"You cannot enter while wearing your %s. It has more than one enhancement.",it[in2].name);
			return;
		}
		if (it[in2].flags&(IF_QUEST|IF_BONDTAKE|IF_BONDWEAR)) {
			log_char(cn,LOG_SYSTEM,0,"You cannot enter while wearing your %s. It is a quest or a bound item.",it[in2].name);
			return;
		}
	}


	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);

        if (!drop_char_extended(cn,x,y,7)) {
                log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,0);		
	}

	destroy_chareffects(cn);
	for (n=12; n<30; n++) {
		if ((in2=ch[cn].item[n])) {
			destroy_item(in2);
			ch[cn].item[n]=0;
		}
	}

	log_char(cn,LOG_SYSTEM,0,"All your spells have been removed.");

	if (ch[cn].player) player_driver_stop(ch[cn].player,0);
}

/*

3		60		0.0125%		demon_warrior, demon_mage (cape)
4		59		0.0375%		20,000, 40,000, 100,000 gold
5		58		0.0750%		tactical offense, magical offense (boots)
6		57		0.1250%		tactical defense, magical defense (armor, vest)
7		56		0.1875%		einsatz x gold chips
8		55		0.2625%		5,000, 10,000, 25,000 gold
9		54		0.3500%		sword, flash (leggings, trousers)
10		53		0.4500%		two-handed, fire (leggings, trousers)
11		52		0.5625%		einsatz x silver chips
12		51		0.6875%		1,000, 2,000, 5,000 gold
13		50		0.8250%         500, 1,000, 2,500 gold
14		49		0.9750%		2 x einsatz x huge combo pot
15		48		1.1375%         einsatz x huge combo pot
16		47		1.3125%		einsatz x big combo pot
17		46		1.5000%		einsatz x medium combo pot
18		45		1.7000%		einsatz x small combo pot
19		44		1.9125%		einsatz x green torch
20		43		2.1375%		einsatz x torch

*/

void give_reward(int cn,int nr,int bet)
{
	char *ptr=NULL;
	int cnt=0,n,in;

	switch(nr) {
		case 3:	
			if (bet==5) ptr="demon_warrior3";
			else if (bet==2) ptr="demon_warrior2";
			else ptr="demon_warrior1";
			cnt=1;
			break;
		case 60:
			if (bet==5) ptr="demon_mage3";
			else if (bet==2) ptr="demon_mage2";
			else ptr="demon_mage1";
			cnt=1;
			break;
		case 4:	
		case 59:
			give_money(cn,bet*2000000,"Won at Demon Gambler");
			return;
		case 5:	
			if (bet==5) ptr="demon_tacoff3";
			else if (bet==2) ptr="demon_tacoff2";
			else ptr="demon_tacoff1";
			cnt=1;
			break;
		case 58:
			if (bet==5) ptr="demon_magoff3";
			else if (bet==2) ptr="demon_magoff2";
			else ptr="demon_magoff1";
			cnt=1;
			break;
		case 6:	
			if (bet==5) ptr="demon_tacdef3";
			else if (bet==2) ptr="demon_tacdef2";
			else ptr="demon_tacdef1";
			cnt=1;
			break;
		case 57:
			if (bet==5) ptr="demon_magdef3";
			else if (bet==2) ptr="demon_magdef2";
			else ptr="demon_magdef1";
			cnt=1;
			break;
		case 7:
		case 56:
			ptr="goldchip";
			cnt=bet;
			break;
		case 8:	
		case 55:
			give_money(cn,bet*500000,"Won at Demon Gambler");
			return;
		case 9:	
			if (bet==5) ptr="demon_sword3";
			else if (bet==2) ptr="demon_sword2";
			else ptr="demon_sword1";
			cnt=1;
			break;
		case 54:
			if (bet==5) ptr="demon_flash3";
			else if (bet==2) ptr="demon_flash2";
			else ptr="demon_flash1";
			cnt=1;
			break;
		case 10:	
			if (bet==5) ptr="demon_twohanded3";
			else if (bet==2) ptr="demon_twohanded2";
			else ptr="demon_twohanded1";
			cnt=1;
			break;
		case 53:
			if (bet==5) ptr="demon_fire3";
			else if (bet==2) ptr="demon_fire2";
			else ptr="demon_fire1";
			cnt=1;
			break;
		case 11:
		case 52:
			ptr="silverchip";
			cnt=bet;
			break;
		case 12:	
		case 51:
			give_money(cn,bet*100000,"Won at Demon Gambler");
			return;
		case 13:	
		case 50:
			give_money(cn,bet*50000,"Won at Demon Gambler");
			return;
		case 14:
		case 49:
			ptr="mis_combopot";
			cnt=bet*2;
			break;
		case 15:
		case 48:
			ptr="mis_combopot";
			cnt=bet;
			break;
		case 16:
		case 47:
			ptr="combo_potion3";
			cnt=bet;
			break;
		case 17:
		case 46:
			ptr="combo_potion2";
			cnt=bet;
			break;
		case 18:
		case 45:
			ptr="combo_potion1";
			cnt=bet;
			break;
		case 19:
		case 44:
			ptr="green_torch";
			cnt=bet;
			break;
		case 20:
		case 43:
			ptr="torch";
			cnt=bet;
			break;
	}
        if (!ptr || !cnt) {
		elog("no reward for nr=%d, bet=%d (%p %d)",nr,bet,ptr,cnt);
		log_char(cn,LOG_SYSTEM,0,"Bug #1778");
		return;
	}
	for (n=0; n<cnt; n++) {
		in=create_item(ptr);
		if (!in) {
			elog("no reward for nr=%d, bet=%d (%p %d)",nr,bet,ptr,cnt);
			log_char(cn,LOG_SYSTEM,0,"Bug #1779");
			return;
		}
		set_item_requirements(in);
		if (!give_char_item(cn,in)) {
			log_char(cn,LOG_SYSTEM,0,"Seeing that you have no room to store the %s, the Gamber quickly pockets it again. Too bad...",it[in].name);
			destroy_item(in);
		} else {
			log_char(cn,LOG_SYSTEM,0,"The Gambler gives you a %s.",it[in].name);
		}
	}
}

void give_reward2(int cn,int nr,int bet)
{
	char *ptr=NULL;
	int cnt=0,n,in;

	switch(nr) {
		case 3:	
			if (bet==5) ptr="demon_warrior3b";
			else if (bet==2) ptr="demon_warrior2b";
			else ptr="demon_warrior1b";
			cnt=1;
			break;
		case 60:
			if (bet==5) ptr="demon_mage3b";
			else if (bet==2) ptr="demon_mage2b";
			else ptr="demon_mage1b";
			cnt=1;
			break;
		case 4:	
		case 59:
			give_money(cn,bet*3000000,"Won at Demon Gambler");
			return;
		case 5:	
			if (bet==5) ptr="demon_tacoff3b";
			else if (bet==2) ptr="demon_tacoff2b";
			else ptr="demon_tacoff1b";
			cnt=1;
			break;
		case 58:
			if (bet==5) ptr="demon_magoff3b";
			else if (bet==2) ptr="demon_magoff2b";
			else ptr="demon_magoff1b";
			cnt=1;
			break;
		case 6:	
			if (bet==5) ptr="demon_tacdef3b";
			else if (bet==2) ptr="demon_tacdef2b";
			else ptr="demon_tacdef1b";
			cnt=1;
			break;
		case 57:
			if (bet==5) ptr="demon_magdef3b";
			else if (bet==2) ptr="demon_magdef2b";
			else ptr="demon_magdef1b";
			cnt=1;
			break;
		case 7:
		case 56:
			ptr="goldchip";
			cnt=bet*2;
			break;
		case 8:	
		case 55:
			give_money(cn,bet*750000,"Won at Demon Gambler");
			return;
		case 9:	
			if (bet==5) ptr="demon_sword3b";
			else if (bet==2) ptr="demon_sword2b";
			else ptr="demon_sword1b";
			cnt=1;
			break;
		case 54:
			if (bet==5) ptr="demon_flash3b";
			else if (bet==2) ptr="demon_flash2b";
			else ptr="demon_flash1b";
			cnt=1;
			break;
		case 10:
			if (bet==5) ptr="demon_twohanded3b";
			else if (bet==2) ptr="demon_twohanded2b";
			else ptr="demon_twohanded1b";
			cnt=1;
			break;
		case 53:
			if (bet==5) ptr="demon_fire3b";
			else if (bet==2) ptr="demon_fire2b";
			else ptr="demon_fire1b";
			cnt=1;
			break;
		case 11:
		case 52:
			ptr="goldchip";
			cnt=bet;
			break;
		case 12:	
		case 51:
			give_money(cn,bet*150000,"Won at Demon Gambler");
			return;
		case 13:	
		case 50:
			give_money(cn,bet*75000,"Won at Demon Gambler");
			return;
		case 14:
		case 49:
			ptr="mis_combopot";
			cnt=bet*3;
			break;
		case 15:
		case 48:
			ptr="mis_combopot";
			cnt=bet*2;
			break;
		case 16:
		case 47:
			ptr="combo_potion3";
			cnt=bet*2;
			break;
		case 17:
		case 46:
			ptr="combo_potion2";
			cnt=bet*2;
			break;
		case 18:
		case 45:
			ptr="combo_potion1";
			cnt=bet*2;
			break;
		case 19:
		case 44:
			ptr="green_torch";
			cnt=bet*2;
			break;
		case 20:
		case 43:
			ptr="torch";
			cnt=bet*2;
			break;
	}
        if (!ptr || !cnt) {
		elog("no reward for nr=%d, bet=%d (%p %d)",nr,bet,ptr,cnt);
		log_char(cn,LOG_SYSTEM,0,"Bug #1778");
		return;
	}
	for (n=0; n<cnt; n++) {
		in=create_item(ptr);
		if (!in) {
			elog("no reward for nr=%d, bet=%d (%p %d)",nr,bet,ptr,cnt);
			log_char(cn,LOG_SYSTEM,0,"Bug #1779");
			return;
		}
		set_item_requirements(in);
		if (!give_char_item(cn,in)) {
			log_char(cn,LOG_SYSTEM,0,"Seeing that you have no room to store the %s, the Gamber quickly pockets it again. Too bad...",it[in].name);
			destroy_item(in);
		} else {
			log_char(cn,LOG_SYSTEM,0,"The Gambler gives you a %s.",it[in].name);
		}
	}
}

void give_reward3(int cn,int nr,int bet)
{
	char *ptr=NULL;
	int cnt=0,n,in;

	switch(nr) {
		case 3:	
			if (bet==5) ptr="demon_warrior3c";
			else if (bet==2) ptr="demon_warrior2c";
			else ptr="demon_warrior1c";
			cnt=1;
			break;
		case 60:
			if (bet==5) ptr="demon_mage3c";
			else if (bet==2) ptr="demon_mage2c";
			else ptr="demon_mage1c";
			cnt=1;
			break;
		case 4:	
		case 59:
			give_money(cn,bet*4000000,"Won at Demon Gambler");
			return;
		case 5:	
			if (bet==5) ptr="demon_tacoff3c";
			else if (bet==2) ptr="demon_tacoff2c";
			else ptr="demon_tacoff1c";
			cnt=1;
			break;
		case 58:
			if (bet==5) ptr="demon_magoff3c";
			else if (bet==2) ptr="demon_magoff2c";
			else ptr="demon_magoff1c";
			cnt=1;
			break;
		case 6:	
			if (bet==5) ptr="demon_tacdef3c";
			else if (bet==2) ptr="demon_tacdef2c";
			else ptr="demon_tacdef1c";
			cnt=1;
			break;
		case 57:
			if (bet==5) ptr="demon_magdef3c";
			else if (bet==2) ptr="demon_magdef2c";
			else ptr="demon_magdef1c";
			cnt=1;
			break;
		case 7:
		case 56:
			give_money(cn,bet*1250000,"Won at Demon Gambler");
			return;
		case 8:	
		case 55:
			give_money(cn,bet*1000000,"Won at Demon Gambler");
			return;
		case 9:	
			if (bet==5) ptr="demon_sword3c";
			else if (bet==2) ptr="demon_sword2c";
			else ptr="demon_sword1c";
			cnt=1;
			break;
		case 54:
			if (bet==5) ptr="demon_flash3c";
			else if (bet==2) ptr="demon_flash2c";
			else ptr="demon_flash1c";
			cnt=1;
			break;
		case 10:
			if (bet==5) ptr="demon_twohanded3c";
			else if (bet==2) ptr="demon_twohanded2c";
			else ptr="demon_twohanded1c";
			cnt=1;
			break;
		case 53:
			if (bet==5) ptr="demon_fire3c";
			else if (bet==2) ptr="demon_fire2c";
			else ptr="demon_fire1c";
			cnt=1;
			break;
		case 11:
		case 52:
			give_money(cn,bet*250000,"Won at Demon Gambler");
			return;
		case 12:	
		case 51:
			give_money(cn,bet*200000,"Won at Demon Gambler");
			return;
		case 13:	
		case 50:
			give_money(cn,bet*100000,"Won at Demon Gambler");
			return;
		case 14:
		case 49:
			ptr="mis_combopot";
			cnt=bet*4;
			break;
		case 15:
		case 48:
			ptr="mis_combopot";
			cnt=bet*2;
			break;
		case 16:
		case 47:
			ptr="combo_potion3";
			cnt=bet*2;
			break;
		case 17:
		case 46:
			ptr="combo_potion2";
			cnt=bet*2;
			break;
		case 18:
		case 45:
			ptr="combo_potion1";
			cnt=bet*2;
			break;
		case 19:
		case 44:
			ptr="green_torch";
			cnt=bet*2;
			break;
		case 20:
		case 43:
			ptr="torch";
			cnt=bet*2;
			break;
	}
        if (!ptr || !cnt) {
		elog("no reward for nr=%d, bet=%d (%p %d)",nr,bet,ptr,cnt);
		log_char(cn,LOG_SYSTEM,0,"Bug #1778");
		return;
	}
	for (n=0; n<cnt; n++) {
		in=create_item(ptr);
		if (!in) {
			elog("no reward for nr=%d, bet=%d (%p %d)",nr,bet,ptr,cnt);
			log_char(cn,LOG_SYSTEM,0,"Bug #1779");
			return;
		}
		set_item_requirements(in);
		if (!give_char_item(cn,in)) {
			log_char(cn,LOG_SYSTEM,0,"Seeing that you have no room to store the %s, the Gamber quickly pockets it again. Too bad...",it[in].name);
			destroy_item(in);
		} else {
			log_char(cn,LOG_SYSTEM,0,"The Gambler gives you a %s.",it[in].name);
		}
	}
}

struct gamble_data
{
	int memcleartimer;
	int nr;
};

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

void teufelgambler_driver(int cn,int ret,int lastact)
{
	struct gamble_data *dat;
        int co,talkdir=0,n,cnt,in,have,a,b,c,t;
	char *ptr;
        struct msg *msg,*next;

        dat=set_data(cn,DRD_TEUFELGAMBLE,sizeof(struct gamble_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE && ch[cn].arg) {
			dat->nr=atoi(ch[cn].arg);
			ch[cn].arg=NULL;
		}

		// did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			if (dat->nr==1) {
				if (!is_demon(co)) {
					say(cn,"Oh. A human. Well, no matter I guess. Wanna °c4play°c0 with me, kid?");
				} else {
					say(cn,"Hello there, %s! Make your bet! Win big! Come on, °c4play°c0 with me!",ch[co].name);
				}
			} else if (dat->nr==2) {
				if (!is_demon(co)) {
					say(cn,"Oh. A human. Well, no matter I guess. Wanna °c4play2°c0 with me, kid?");
				} else {
					say(cn,"Hello there, %s! Make your bet! Win big! Come on, °c4play2°c0 with me!",ch[co].name);
				}
			}else if (dat->nr==3) {
				if (!is_demon(co)) {
					say(cn,"Oh. A human. Well, no matter I guess. Wanna °c4play3°c0 with me, kid?");
				} else {
					say(cn,"Hello there, %s! Make your bet! Win big! Come on, °c4play3°c0 with me!",ch[co].name);
				}
			}
			
			talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			if ((n=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3))) {
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				if (n>1) {
					if (n==2) cnt=1;
					else if (n==3) cnt=2;
					else cnt=5;
					
					for (n=30; n<INVENTORYSIZE; n++) {
						if (dat->nr==1 && (in=ch[co].item[n]) && it[in].ID==IID_BRONZECHIP && (have=*(unsigned int*)(it[in].drdata+0))>=cnt) {
							if (cnt==have) {
								destroy_item(in);
								ch[co].item[n]=0;
							} else {
								*(unsigned int*)(it[in].drdata+0)-=cnt;
								set_chip_data(in,0);
							}
							ch[co].flags|=CF_ITEMS;
							break;
						}

						if (dat->nr==2 && (in=ch[co].item[n]) && it[in].ID==IID_SILVERCHIP && (have=*(unsigned int*)(it[in].drdata+0))>=cnt) {
							if (cnt==have) {
								destroy_item(in);
								ch[co].item[n]=0;
							} else {
								*(unsigned int*)(it[in].drdata+0)-=cnt;
								set_chip_data(in,12);
							}
							ch[co].flags|=CF_ITEMS;
							break;
						}

						if (dat->nr==3 && (in=ch[co].item[n]) && it[in].ID==IID_GOLDCHIP && (have=*(unsigned int*)(it[in].drdata+0))>=cnt) {
							if (cnt==have) {
								destroy_item(in);
								ch[co].item[n]=0;
							} else {
								*(unsigned int*)(it[in].drdata+0)-=cnt;
								set_chip_data(in,6);
							}
							ch[co].flags|=CF_ITEMS;
							break;
						}
					}
					
					if (n<INVENTORYSIZE) {
						a=RANDOM(20)+1;
						b=RANDOM(20)+1;
						c=RANDOM(20)+1;
						
						t=a+b+c;
						if (t>20 && t<43) say(cn,"Ha! You rolled %d, %d and %d. You lost!",a,b,c);
						else {
							say(cn,"Oh. You rolled %d, %d and %d. You win!",a,b,c);
							if (dat->nr==1) give_reward(co,t,cnt);
							else if (dat->nr==2) give_reward2(co,t,cnt);
							else if (dat->nr==3) give_reward3(co,t,cnt);
							else say(cn,"Bug #4227v");
						}
					} else say(cn,"No chips, no game (make sure you only have one stack of chips).");
				}
			}

			if ((ch[co].flags&CF_GOD) && (ptr=strstr((char*)msg->dat2,"reward: ")) && (n=atoi(ptr+8))) {
				if (dat->nr==1) give_reward(co,n,5);
				else if (dat->nr==2) give_reward2(co,n,5);
				else if (dat->nr==3) give_reward3(co,n,5);
				else say(cn,"Bug #4227v");
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;			
		}

		remove_message(cn,msg);
	}
        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
	if (ticker>dat->memcleartimer) {
		mem_erase_driver(cn,7);
		dat->memcleartimer=ticker+TICKS*60*60*12;
	}

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

struct rat_data
{
	int kills;
	int score;
};

void special_rat_reward(int cn,int co,struct rat_data *ppd)
{
	int pts=0,in=0;

	     if (ppd->score>=100000) { pts=100000; in=create_special_item(20,90,1,50); }
	else if (ppd->score>= 50000) { pts= 50000; in=create_special_item(20,90,1,250); }
	else if (ppd->score>= 25000) { pts= 25000; in=create_special_item(20,90,1,1000); }
	else if (ppd->score>= 10000) { pts= 10000; in=create_special_item(20,90,1,10000); }
	else if (ppd->score>=  5000) { pts=  5000; in=create_item("healing_potion3"); }
	else if (ppd->score>=  2500) { pts=  2500; in=create_item("healing_potion2"); }
	else if (ppd->score>=  1000) { pts=  1000; in=create_item("healing_potion1"); }
	
	if (pts && in) {
		say(cn,"Here's a little extra for scoring %d points in one go: %s!",pts,it[in].name);
		if (!give_char_item(co,in)) destroy_item(in);
	}
}

void teufelquest_driver(int cn,int ret,int lastact)
{
	struct gamble_data *dat;
        int co,talkdir=0,n,tmp;
        struct msg *msg,*next;
	struct rat_data *ppd;
	
	dat=set_data(cn,DRD_TEUFELGAMBLE,sizeof(struct gamble_data));
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
			if (char_dist(cn,co)>16) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			
			if (!is_demon(co)) {
				say(cn,"Ah human? AAAAAHHHHHHHHHH! HELP!");
			} else {
				say(cn,"Hello, %s! We have a slight rat problem in the caverns to the north. There's a nice °c4reward°c0 for killing some rats.",ch[co].name);
			}

                        talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			ppd=set_data(co,DRD_TEUFELRAT_PPD,sizeof(struct rat_data));
			if (!ppd) { remove_message(cn,msg); continue; }

			if ((n=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,msg->dat3))) {
				talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
				switch(n) {
					case 5:	

						tmp=ppd->score/20*ch[cn].level; // 12% of a level for 1000 enemies killed (at max level)
						say(cn,"Experience it is. You killed %d rats for a total score of %d.",ppd->kills,ppd->score);
						give_exp(co,tmp);
						special_rat_reward(cn,co,ppd);
						ppd->score=0;
						ppd->kills=0;
						break;
					case 6:
						tmp=ppd->score/1250;	// 50 pts for 625 enemies killed (at max level)
						say(cn,"Military knowledge it is. You killed %d rats for a total score of %d.",ppd->kills,ppd->score);
						give_military_pts_no_npc(co,tmp,1);
						special_rat_reward(cn,co,ppd);
						ppd->score=0;
						ppd->kills=0;
						break;
					case 7:
						tmp=ppd->score*12;	// aprox 12 gold per enemy killed (at max level)
						say(cn,"Money it is. You killed %d rats for a total score of %d.",ppd->kills,ppd->score);
						give_money(co,tmp,"earned from rat hunter");
						special_rat_reward(cn,co,ppd);
						ppd->score=0;
						ppd->kills=0;
						break;
					case 8:
						if (ch[co].flags&CF_GOD) { ppd->kills=500; ppd->score=ppd->kills*50; }
						break;
				}				
				if (!ppd->score) {
					log_char(co,LOG_SYSTEM,0,"#90");
					log_char(co,LOG_SYSTEM,0,"#80");
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;			
		}

		remove_message(cn,msg);
	}
        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.
	if (ticker>dat->memcleartimer) {
		mem_erase_driver(cn,7);
		dat->memcleartimer=ticker+TICKS*60*60*12;
	}

	if (talkdir) turn(cn,talkdir);

        do_idle(cn,TICKS);
}

void teufelrat_driver(int cn,int ret,int lastact)
{
        struct msg *msg,*next;
        int co;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch(msg->type) {
			case NT_CHAR:
				co=msg->dat1;
                                break;
		}
	}

	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void teufelrat_dead(int cn,int cc)
{
	struct rat_data *ppd;
	int tmp;
	
	if (!cc || !(ch[cc].flags&CF_PLAYER)) return;
	
	ppd=set_data(cc,DRD_TEUFELRAT_PPD,sizeof(struct rat_data));
	if (!ppd) return;

	ppd->kills++;
	tmp=(ch[cn].level*ch[cn].level)/100;
	//tmp=(tmp*tmp)/100;
	if (ch[cc].flags&CF_LAG) tmp=1;
	if (ch[cc].driver==CDR_LOSTCON) tmp=1;
	
	ppd->score+=tmp;
	log_char(cc,LOG_SYSTEM,0,"#90 %d Rat Kills",ppd->kills);
	log_char(cc,LOG_SYSTEM,0,"#80 %d Rat Points",ppd->score);
}

void teufelratnest(int in,int cn)
{
	int n,co,ser,wave,lvl,nowaveincrease,nr;
	char *desc,*name;

	nr=it[in].drdata[4];

	if (!cn) {	// time call
		call_item(IDR_TEUFELRATNEST,in,0,ticker+TICKS*20);

		if (it[in].drdata[2]>0) {
			it[in].drdata[2]--;
			if (it[in].drdata[2]==0) {
				it[in].sprite=15281;
				set_sector(it[in].x,it[in].y);
			} else return;
		}
                wave=(*(unsigned short*)(it[in].drdata+0));
		if (wave>0) { (*(unsigned short*)(it[in].drdata+0))--; wave--; }

		if (nr==1) {
			if (wave==0) lvl=70; // baby 69
			else if (wave<100) lvl=74; // big baby 73
			else if (wave<200) lvl=78; // kid 77
			else if (wave<400) lvl=83; // big kid 82
			else if (wave<800) lvl=87; // teeny 86
			else if (wave<1600) lvl=92; // big teeny 91
			else if (wave<3200) lvl=96; // big 95
			else if (wave<6400) lvl=100; // very big 99
			else if (wave<12800) lvl=103; // huge 102
			else lvl=108; // enormous 107
		} else if (nr==2) {
			if (wave==0) lvl=109; // baby 108
			else if (wave<100) lvl=113; // big baby 112
			else if (wave<200) lvl=117; // kid 116
			else if (wave<400) lvl=121; // big kid 120
			else if (wave<800) lvl=125; // teeny 124
			else if (wave<1600) lvl=129; // big teeny 128
			else if (wave<3200) lvl=133; // big 132
			else if (wave<6400) lvl=137; // very big 136
			else if (wave<12800) lvl=141; // huge 140
			else lvl=145; // enormous 144
			//xlog("2lvl=%d",lvl);
		} else {
			if (wave==0) lvl=45; // baby 44
			else if (wave<100) lvl=47; // big baby 46
			else if (wave<200) lvl=50; // kid 49
			else if (wave<400) lvl=53; // big kid 52
			else if (wave<800) lvl=56; // teeny 55
			else if (wave<1600) lvl=60; // big teeny 59
			else if (wave<3200) lvl=63; // big 62
			else if (wave<6400) lvl=66; // very big 65
			else if (wave<12800) lvl=70; // huge 69
			else lvl=73; // enormous 72
			//xlog("1lvl=%d",lvl);
		}

		sprintf(it[in].description,"An Ice Rat nest[%d]. You could destroy it...[%d,%d]",nr,wave,lvl);
		
		for (n=0; n<5; n++) {
			co=*(unsigned short*)(it[in].drdata+n*2+10);
			ser=*(unsigned int*)(it[in].drdata+n*4+20);
			if (co && (ch[co].flags&CF_USED) && ch[co].serial==ser) {
				if (ch[co].level>lvl) {
					nowaveincrease=1;
					remove_char(co);
					destroy_char(co);
				} else continue;
			} else nowaveincrease=0;
			
			if (nr==1) {
				if (wave<100) co=create_char("rat80",0); // baby 69
				else if (wave<200) co=create_char("rat80b",0); // big baby 73
				else if (wave<400) co=create_char("rat81",0); // kid 77
				else if (wave<800) co=create_char("rat81b",0); // big kid 82
				else if (wave<1600) co=create_char("rat82",0); // teeny 86
				else if (wave<3200) co=create_char("rat82b",0); // big teeny 91
				else if (wave<6400) co=create_char("rat83",0); // big 95
				else if (wave<12800) co=create_char("rat83b",0); // very big 99
				else if (wave<25600) co=create_char("rat84",0); // huge 102
				else co=create_char("rat84b",0); // enormous 107
			} else if (nr==2) {
				if (wave<100) co=create_char("rat90",0); // baby
				else if (wave<200) co=create_char("rat90b",0); // big baby
				else if (wave<400) co=create_char("rat91",0); // kid
				else if (wave<800) co=create_char("rat91b",0); // big kid
				else if (wave<1600) co=create_char("rat92",0); // teeny
				else if (wave<3200) co=create_char("rat92b",0); // big teeny
				else if (wave<6400) co=create_char("rat93",0); // big
				else if (wave<12800) co=create_char("rat93b",0); // very big
				else if (wave<25600) co=create_char("rat94",0); // huge
				else co=create_char("rat94b",0); // enormous
			} else {
				if (wave<100) co=create_char("rat70",0); // baby
				else if (wave<200) co=create_char("rat70b",0); // big baby
				else if (wave<400) co=create_char("rat71",0); // kid
				else if (wave<800) co=create_char("rat71b",0); // big kid
				else if (wave<1600) co=create_char("rat72",0); // teeny
				else if (wave<3200) co=create_char("rat72b",0); // big teeny
				else if (wave<6400) co=create_char("rat73",0); // big
				else if (wave<12800) co=create_char("rat73b",0); // very big
				else if (wave<25600) co=create_char("rat74",0); // huge
				else co=create_char("rat74b",0); // enormous
			}
			if (!co) break;

			if (item_drop_char(in,co)) {
				ch[co].tmpx=ch[co].x;
				ch[co].tmpy=ch[co].y;

				switch(RANDOM(20)) {
					case 0:		ch[co].value[1][V_ATTACK]+=RANDOM(10)+7; name=" *A"; desc=" Increased Attack."; break;
					case 1:		ch[co].value[1][V_PARRY]+=RANDOM(10)+7; name=" *P"; desc=" Increased Parry."; break;
					case 2:		ch[co].value[1][V_FREEZE]+=RANDOM(10)+7; name=" *R"; desc=" Increased Freeze."; break;
					case 3:		ch[co].value[1][V_FLASH]+=RANDOM(10)+7; name=" *F"; desc=" Increased Flash."; break;
					case 4:		ch[co].value[1][V_IMMUNITY]+=RANDOM(10)+7; name=" *I"; desc=" Increased Immunity."; break;
					default:	name=""; desc=""; break;
				}
				strcat(ch[co].name,name);
				strcat(ch[co].description,desc);

                                update_char(co);

				if (ch[co].value[0][V_BLESS]) bless_someone(co,ch[co].value[0][V_BLESS],BLESSDURATION);
				
				ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
				ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
				ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
				ch[co].lifeshield=ch[co].value[0][V_MAGICSHIELD]*POWERSCALE;
				
				ch[co].dir=DX_RIGHTDOWN;
				ch[co].flags|=CF_NONOTIFY;

				*(unsigned short*)(it[in].drdata+n*2+10)=co;
				*(unsigned int*)(it[in].drdata+n*4+20)=ch[co].serial;
				if (!nowaveincrease && wave<50000) (*(unsigned short*)(it[in].drdata+0))+=10; // increase wave
				break;
			} else {
				destroy_char(co);
                                break;
			}
		}
	} else { // player action
		for (n=0; n<5; n++) {
			co=*(unsigned short*)(it[in].drdata+n*2+10);
			ser=*(unsigned int*)(it[in].drdata+n*4+20);
			if (co && (ch[co].flags&CF_USED) && ch[co].serial==ser) {
				log_char(cn,LOG_SYSTEM,0,"You need a moment of peace to destroy the nest. There is still a guard left, distracting you.");
                                return;
			}			
		}
		log_char(cn,LOG_SYSTEM,0,"You destroy the rat nest.");
		*(unsigned short*)(it[in].drdata+0)=0; // reset wave
		it[in].drdata[2]=5;
		it[in].sprite=0;
		set_sector(it[in].x,it[in].y);
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch (nr) {
		case CDR_TEUFELDEMON:	teufeldemon_driver(cn,ret,lastact); return 1;
		case CDR_TEUFELGAMBLER:	teufelgambler_driver(cn,ret,lastact); return 1;
		case CDR_TEUFELQUEST:	teufelquest_driver(cn,ret,lastact); return 1;
		case CDR_TEUFELRAT:	teufelrat_driver(cn,ret,lastact); return 1;

		default:        	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch (nr) {
		case IDR_TEUFELDOOR:		teufeldoor(in,cn); return 1;
		case IDR_TEUFELARENA:		teufelarena(in,cn); return 1;
		case IDR_TEUFELRATNEST:		teufelratnest(in,cn); return 1;
		case IDR_TEUFELARENAEXIT:	teufelarenaexit(in,cn); return 1;

		default:        return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch (nr) {
		case CDR_TEUFELDEMON:	return 1;
		case CDR_TEUFELGAMBLER:	return 1;
		case CDR_TEUFELQUEST:	return 1;
		case CDR_TEUFELRAT:	teufelrat_dead(cn,co); return 1;
		default:        	return 0;		
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch (nr) {
		case CDR_TEUFELDEMON:	return 1;
		case CDR_TEUFELGAMBLER:	return 1;
		default:        	return 0;
	}
}





