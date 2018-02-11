/*

$Id: professor.c,v 1.2 2007/08/13 18:50:38 devel Exp $

$Log: professor.c,v $
Revision 1.2  2007/08/13 18:50:38  devel
fixed some warnings

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:37  sam
Added RCS tags


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
#include "sector.h"
#include "drvlib.h"
#include "prof.h"
#include "create.h"
#include "consistency.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case 0:		return ch_driver(nr,obj,ret,lastact);
		case 1: 	return it_driver(nr,obj,ret);
		case 2:		return ch_died_driver(nr,obj,ret);
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
	{{"help",NULL},"Sorry, I'm just a merchant, %s!",0},
	{{"what's","up",NULL},"Everything that isn't nailed down.",0},
	{{"what","is","up",NULL},"Everything that isn't nailed down.",0},
        {{"buy",NULL},"Hey %s, use 'buy %s'!",0},
	{{"sell",NULL},"Hey %s, use 'sell %s'!",0},
        {{"what's","your","name",NULL},NULL,1},
	{{"what","is","your","name",NULL},NULL,1},
        {{"who","are","you",NULL},NULL,1},
	{{"teach",NULL},NULL,2},
	{{"repeat",NULL},NULL,2},
	{{"learn",NULL},NULL,4},
	{{"improve",NULL},NULL,5},
        {{"athlete",NULL},NULL,3},
	{{"alchemist",NULL},NULL,3},
	{{"miner",NULL},NULL,3},
	{{"assassin",NULL},NULL,3},
	{{"thief",NULL},NULL,3},
	{{"light",NULL},NULL,3},
	{{"light","warrior",NULL},NULL,3},
	{{"dark",NULL},NULL,3},
	{{"dark","warrior",NULL},NULL,3},
        {{"trader",NULL},NULL,3},
	{{"mercenary",NULL},NULL,3},
	{{"clan",NULL},NULL,3},
	{{"herbalist",NULL},NULL,3}
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
				else switch(qa[q].answer_code) {
					     case 1:	say(cn,"I'm %s.",ch[cn].name); break;
					     default:	return qa[q].answer_code;
				}
				break;
			}
		}
	}
	
        return 0;
}

struct professor_driver_data
{
        int last_talk;
	int dir;

	int nr;
	int quest;
	int quest_option;
	int improve_cost;
};

void professor_driver_parse(int cn,struct professor_driver_data *dat)
{
	char *ptr,name[64],value[64];

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {

                if (!strcmp(name,"dir")) dat->dir=atoi(value);
		else if (!strcmp(name,"nr")) dat->nr=atoi(value);
		else if (!strcmp(name,"quest")) dat->quest=atoi(value);
		else if (!strcmp(name,"option")) dat->quest_option=atoi(value);
		else if (!strcmp(name,"cost")) dat->improve_cost=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

int count_prof(int cn)
{
	int n,cnt;

	for (n=cnt=0; n<P_MAX; n++) {
		if (ch[cn].prof[n]) cnt++;		
	}
	return cnt;
}

int learn_prof(int cn,int co,int nr)
{
	int cnt;

	if (ch[co].prof[nr]) {
		say(cn,"But thou knowest the ways of the %s already!",prof[nr].name);
		return 0;
	}
	if (!(ch[co].flags&CF_PAID) && count_prof(co)>0) {
		say(cn,"Only paying players may learn more than one profession.");
		return 0;
	}
	if (nr==P_LIGHT && ch[co].prof[P_DARK]) {
		say(cn,"Thou hast learned Master of Dark already, thou mayest not learn Master of Light.");
		return 0;
	}
	if (nr==P_DARK && ch[co].prof[P_LIGHT]) {
		say(cn,"Thou hast learned Master of Light already, thou mayest not learn Master of Dark.");
		return 0;
	}
	cnt=free_prof_points(co);

	if (cnt<prof[nr].base) {
		say(cn,"Thou have not the required profession points. Thou needst %d, but thou hast only %d.",prof[nr].base,cnt);
		return 0;
	}
	ch[co].prof[nr]=prof[nr].base;
	ch[co].flags|=CF_PROF;
	update_char(co);

	say(cn,"Thou hast learnt the art of %s.",prof[nr].name);
	return 1;
}

int improve_prof(int cn,int co,int nr)
{
	int cnt,step;

	if (!ch[co].prof[nr]) {
		say(cn,"But thou knowest not the ways of the %s. Thou must learn them first.",prof[nr].name);
		return 0;
	}
	if (ch[co].prof[nr]>=prof[nr].max) {
		say(cn,"Thou hast reached mastery in the art of the %s already.",prof[nr].name);
		return 0;
	}
        cnt=free_prof_points(co);

	step=min(prof[nr].step,prof[nr].max-ch[co].prof[nr]);

	if (!(ch[co].flags&CF_PAID) && ch[co].prof[nr]+step>=20) {
		say(cn,"Only paying players may raise this profession higher than %d.",ch[co].prof[nr]);
		return 0;
	}

	if (cnt<step) {
		say(cn,"Thou needst have at least %d unused profession points.",prof[nr].step);
		return 0;
	}
        ch[co].prof[nr]+=step;
	ch[co].flags|=CF_PROF;
	update_char(co);
	
	say(cn,"Thy profession %s was improved to %d.",prof[nr].name,ch[co].prof[nr]);
	return 1;
}

void professor_driver(int cn,int ret,int lastact)
{
	struct professor_driver_data *dat;
        int co,in;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_PROFDRIVER,sizeof(struct professor_driver_data));
	if (!dat) return;	// oops...

        if (ch[cn].arg) {
                professor_driver_parse(cn,dat);
		ch[cn].arg=NULL;
	}

	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont offer to teach anyone not having the profession skill
			if (!ch[co].value[1][V_PROFESSION]) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			say(cn,"Hello %s! I am a professor at Aston University, and I °c4teach°c0 °c4%s°c0.",ch[co].name,prof[dat->nr].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;
                        ret=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co);
			switch(ret) {
				case 2:		switch(dat->quest) {
							case 0:		say(cn,"If thou wishest to learn the art of the %s, thou must pay %d gold coins and %d profession points. Say °c4learn°c0 if this is thy wish. Thou canst also °c4improve°c0 thy knowledge of this art for the fee of %d gold coins and %d profession points.",prof[dat->nr].name,dat->quest_option,prof[dat->nr].base,dat->improve_cost*prof[dat->nr].step,prof[dat->nr].step); break;
							default:	say(cn,"You've found bug #418a"); break;
						}
						break;
				case 3:		switch(dat->nr) {
							case P_ATHLETE:		say(cn,"The art of the athlete are fast, precise movements. Skilled athletes make better use of their endurance and move faster than untrained humans."); break;
							case P_ALCHEMIST:	say(cn,"The alchemist can create better potions, calling on the powers of the moons and the seasons at any time."); break;
							case P_MINER:		say(cn,"A skilled miner will make better use of every vein of precious metal he finds. He will also not exhaust as fast as an unskilled miner."); break;
							case P_ASSASSIN:	say(cn,"The assassin is especially skilled at attacking an enemy from the side or behind, and he can backstab an unware opponent from behind."); break;
							case P_THIEF:		say(cn,"A skilled thief can remain unseen even when next to another person. But when he uses this skill of stealth he cannot do anything but wait or walk, and the effort of remaining unseen will drain his endurance."); break;
							case P_LIGHT:		say(cn,"A master of light will receive a bonus to his basic abilities during the day. If he masters this skill he will also be able to see all undead creatures in the dark."); break;
							case P_DARK:		say(cn,"A master of dark will receive a bonus to his basic abilities during the night. If he masters this skill he will also be able to see all living creatures in the dark."); break;
							case P_TRADER:		say(cn,"A skilled trader will get better prices when dealing with merchants."); break;
							case P_MERCENARY:	say(cn,"Those skilled in the art of the mercenary will advance in military rank faster. They will also collect pay for their missions."); break;
							case P_CLAN:		say(cn,"A clan master has received special training in the art of clan warfare. He will be at an advantage in any fight in the clan catacombs."); break;
							case P_HERBALIST:	say(cn,"A herbalist knows the art of making plants ripe faster. Any flower, berry or mushroom he picks will grow back in less time."); break;

							default: 		say(cn,"You've found bug #418b"); break;
						}
						break;
				case 4:		switch(dat->quest) {
							case 0:		if (ch[co].gold<dat->quest_option*100) { say(cn,"But thou cannot afford my fee of %dG.",dat->quest_option); break; }
									if (!learn_prof(cn,co,dat->nr)) break;
									ch[co].gold-=dat->quest_option*100;
									ch[co].flags|=CF_ITEMS;
									break;
							default:	say(cn,"You've found bug #418a"); break;
						}
						break;
				case 5:		if (ch[co].gold<dat->improve_cost*prof[dat->nr].step*100) { say(cn,"But thou cannot afford my fee of %dG.",dat->improve_cost*prof[dat->nr].step); break; }
						if (!improve_prof(cn,co,dat->nr)) break;
						ch[co].gold-=dat->improve_cost*prof[dat->nr].step*100;
						ch[co].flags|=CF_ITEMS;
                                                break;
			}

		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				// let it vanish
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,dat->dir,ret,lastact)) return;		

        do_idle(cn,TICKS);
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_PROFESSOR:	professor_driver(cn,ret,lastact); return 1;
		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_PROFESSOR:	immortal_dead(cn,co); return 1;
		default:		return 0;
	}
}
