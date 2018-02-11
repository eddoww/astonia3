/*

$Id: sidestory.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: sidestory.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:46  sam
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
#include "map.h"
#include "create.h"
#include "drvlib.h"
#include "libload.h"
#include "item_id.h"
#include "skill.h"
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

struct answer
{
	char *text;
	int nextstate;
};

struct story
{
	int state;
	char *text;
        int special;
	int opt;

	struct answer a[5];
};

struct story_npc
{
	int state;
        int last_say;
};

struct ruby_aston_npc
{
	int state;
        int last_say;
	int skl;
	int level;
	int mod1;
	int mod2;
	int add;
	int last_see;
};

#define SPEC_ITEM	1
#define SPEC_ALCITEM	2
#define SPEC_ITEM32	3
#define SPEC_ITEMDRV	4
#define SPEC_SETBIT	5
#define SPEC_ISSET	6
#define SPEC_ISNOTSET	7

#define BIT_ZOMBIE	(1u<<0)
#define BIT_SHROOM1	(1u<<1)
#define BIT_SHROOM2	(1u<<2)
#define BIT_SHROOM3	(1u<<3)
#define BIT_OCRYSTAL	(1u<<4)
#define BIT_RCRYSTAL	(1u<<5)
#define BIT_FSCROLL	(1u<<6)
#define BIT_ISLENA	(1u<<7)
#define BIT_TORCH	(1u<<8)
#define BIT_SWAMP	(1u<<9)
#define BIT_FOREST	(1u<<10)
#define BIT_GOVERNOR	(1u<<11)
#define BIT_WRONG1	(1u<<12)

#define BIT_ALLSHROOMS	(BIT_SHROOM1|BIT_SHROOM2|BIT_SHROOM3)

struct story ruby2_story[]={
	{0,	"My greetings, %s!",					0,		0,			{{"Hello, %s!",1},			       	{"My greetings, noble Lady!",1},		{"Yes?",1},			{NULL}}},
	{1,	"How art thou, %s?",		   		     	0,		0,			{{"Fine.",2},				       	{"Fine, just fine. And how art thou?",2},	{"Get to the point.",2},	{NULL}}},
	{2,	"I'm in quite a bit of trouble, %s.",			0,		0,			{{"Thou art? Maybe I could help thee?",3},	{"What kind of trouble?",3},			{"That's thy problem.",20},	{NULL}}},
        {3,	"Well, I was sent here to get a golden skull, but...",	0,		0,			{{"A golden skull?",4},			       	{"Oh. Well, good luck.",20},			{NULL}}},
	{4,	"Yes, but I can't get one. All I get is torches.",	0,		0,			{{"I'll get one for thee!",5},		       	{"Oh. Well, good luck.",20},			{NULL}}},
        {5,	"(Waiting for golden skull)",				SPEC_ITEM,	IID_AREA2_ZOMBIESKULL3,	{{NULL,6},			       	{NULL}}},
	{6,	"My eternal thanks, %s.",				SPEC_SETBIT,	BIT_ZOMBIE,		{{NULL}}},
	
	{20,	"Get lost, %s.",					0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby5_story[]={
	{0,	"My greetings, %s!",					SPEC_ISNOTSET,	BIT_ZOMBIE,		{{"Hello, %s!",1},				{"My greetings, noble Lady!",1},		{"Yes?",1},					{NULL}}},
	{0,	"Oh, hello again, %s!",					SPEC_ISSET,	BIT_ZOMBIE,		{{"Hello, %s!",2},				{"'Tis good to see thee again, Ruby!",2},	{"Yes?",2},					{NULL}}},
	{1,	"May I ask thee for a favor, %s?",   		     	0,		0,			{{"Sure.",3},					{"Well... Yes.",3},				{"I guess I can't stop thee anyway.",3},	{NULL}}},
	{2,	"May I ask thee for another favor, %s?",	     	0,		0,			{{"Sure.",3},					{"Yes?",3},					{"What is it this time?",3},			{NULL}}},
	{3,	"I need some mushrooms, but I can't seem to find any.",	0,		0,			{{"What mushrooms dost thou need?",4},		{"Shrooms? Which ones?",4},			{"So? Well, good luck finding them.",20},	{NULL}}},
	{4,	"I need a Ghestroz.",					SPEC_ISNOTSET,	BIT_SHROOM1,		{{"I'll get one for thee!",5},			{"So? Good luck finding one.",20},		{NULL}}},
	{4,	"I need a Hangot.",					SPEC_ISNOTSET,	BIT_SHROOM2,		{{"I'll get one for thee!",6},			{"So? Good luck finding one.",20},		{NULL}}},
	{4,	"I need an Ivnan.",					SPEC_ISNOTSET,	BIT_SHROOM3,		{{"I'll get one for thee!",7},			{"So? Good luck finding one.",20},		{NULL}}},
	{5,	"(Waiting for Ghestroz)",				SPEC_ALCITEM,	14,			{{NULL,8},				       	{NULL}}},
	{6,	"(Waiting for Hangot)",					SPEC_ALCITEM,	15,			{{NULL,9},				       	{NULL}}},
	{7,	"(Waiting for Ivnan)",					SPEC_ALCITEM,	16,			{{NULL,10},				       	{NULL}}},
	{8,	"My eternal thanks, %s.",				SPEC_SETBIT,	BIT_SHROOM1,		{{NULL,11}}},
	{9,	"My eternal thanks, %s.",				SPEC_SETBIT,	BIT_SHROOM2,		{{NULL,11}}},
	{10,	"My eternal thanks, %s.",				SPEC_SETBIT,	BIT_SHROOM3,		{{NULL,11}}},
	{11,	"I still need another mushroom, %s.",		     	SPEC_ISNOTSET,	BIT_ALLSHROOMS,		{{"Which one?",4},				{"Too bad. I'm not getting you another one.",20},	{NULL}}},
	{11,	"I thank thee, %s. That's all I needed.",	     	SPEC_ISSET,	BIT_ALLSHROOMS,		{{NULL}}},

	{20,	"Oh, just go away, %s.",				0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby6_story[]={
	{0,	"My greetings, %s!",					0,		0,			{{"Hello, %s!",1},			       	{"My greetings, noble Lady!",1},		{"Yes?",1},			{NULL}}},
        {1,	"Thou don't happen to have a spare orange crystal, %s?",0,		0,			{{"Yes, as a matter of fact I do.",2},		{"No, but I'll get one for thee.",2},		{"No.",20},			{NULL}}},
        {2,	"(Waiting for orange crystal)",				SPEC_ITEM,	IID_AREA6_YELLOWCRYSTAL,{{NULL,3},			       	{NULL}}},
	{3,	"Thank thee, %s.",					SPEC_SETBIT,	BIT_OCRYSTAL,		{{NULL}}},
	
	{20,	"Nevermind then, %s.",					0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby8_story[]={
	{0,	"My greetings, %s!",					0,		0,			{{"Hello, %s!",1},			       	{"My greetings, noble Lady!",1},		{"Yes?",1},			{NULL}}},
        {1,	"I'm looking for a big red crystal, %s.",		0,		0,			{{"Here, take this one.",2},			{"I'll get one for thee.",2},			{"I don't care.",20},		{NULL}}},
        {2,	"(Waiting for big red crystal)",			SPEC_ITEM32,	IID_AREA8_REDCRYSTAL,	{{NULL,3},			       	{NULL}}},
	{3,	"Thank thee, %s.",					SPEC_SETBIT,	BIT_RCRYSTAL,		{{NULL}}},
	
	{20,	"So? Leave me alone then, %s.",				0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby10_story[]={
	{0,	"Oh, hullo, %s!",					0,		0,			{{"Hello, %s!",1},			       		{"My greetings, noble Lady!",1},			{"Yes?",1},			{NULL}}},
        {1,	"I'm lost and cold, can you help me, %s?",		0,		0,			{{"Sure, this scroll will take you to a fire.",2},	{"I'll get a scroll to take you back to a fire.",2},	{"I don't care.",20},		{NULL}}},
        {2,	"(Waiting for fire scroll)",				SPEC_ITEMDRV,	55,			{{NULL,3},			       			{NULL}}},
	{3,	"I thank thee, %s!",					SPEC_SETBIT,	BIT_FSCROLL,		{{NULL}}},
	
	{20,	"So? Leave me to freeze alone then, %s.",		0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby11_story[]={
	{0,	"Welcome, %s, to the Ice Palace!",					0,		0,			{{"Hello, %s!",1},		{"My greetings, noble Lady!",1},			{"Yes?",1},{NULL}}},
        {1,	"This time I have to ask thee for a big favor, %s.",			0,		0,			{{"Yes?",2},			{"Let's hear it.",2},					{"I'm not doing thee any more favors.",20},{NULL}}},
	{2,	"Well, I need one of Islena's rings for my collection.",		0,		0,			{{"Islena's Rings?",3},		{"No, sorry. I cannot help thee with that.",21},	{NULL}}},
	{3,	"Yes. Islena is wearing them. Wouldst thou bring me one of them?",	0,		0,			{{"Yes, I will.",4},		{"No, sorry. I cannot help thee with that.",21},	{NULL}}},
        {4,	"(Waiting for Islena's ring)",						SPEC_ITEM,	IID_ISLENARING,		{{NULL,5},			{NULL}}},
	{5,	"Oh, it's wonderful. Thank thee, %s, thank thee very much! ",		SPEC_SETBIT,	BIT_ISLENA,		{{NULL}}},
	
	{20,	"So? Well, it's thy choice, %s, but I think thou wilt regret this.",	0,		0,	{{NULL}}},
	{21,	"Oh. Alright. I understand.",						0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby12_story[]={
	{0,	"Hello? %s?",						0,		0,			{{"Hello, %s!",1},			       		{"My greetings, noble Lady!",1},			{"Yes?",1},			{NULL}}},
        {1,	"My torch is running out, couldst thou spare one?",	0,		0,			{{"Yes, certainly.",2},					{"Not right now, but I'll go get one for thee.",2},	{"I don't care.",20},		{NULL}}},
        {2,	"(Waiting for torch)",					SPEC_ITEMDRV,	12,			{{NULL,3},			       			{NULL}}},
	{3,	"Oh, jolly good. Thanks, %s!",				SPEC_SETBIT,	BIT_TORCH,		{{NULL}}},
	
	{20,	"So? Well, I'll go in the dark then, %s.",		0,		0,	{{NULL}}},
	{0,	NULL}
};

struct story ruby15_story[]={
	{0,	"May Ishtar be with thee, %s.",				0,		0,			{{"Hello, %s!",1},			       		{"And with thee, %s!",1},				{"Yes?",1},			{NULL}}},
        {1,	"I am looking for Swamp Beast heads, thou dost not happen to have any, %s?",0,0,		{{"Here, I can spare one.",2},				{"Not right now, but I'll go get one for thee.",2},	{"No, and I don't care what thou wantst.",20},{NULL}}},
        {2,	"(Waiting for Swamp Beast head)",			SPEC_ITEM,	IID_AREA15_HEAD,	{{NULL,3},			       			{NULL}}},
	{3,	"Ah, yes, that's it. Thanks, %s!",			SPEC_SETBIT,	BIT_SWAMP,		{{NULL}}},
	
	{20,	"So? I don't like thee either, %s.",			0,		0, 		     	{{NULL}}},
	{0,	NULL}
};

struct story ruby16_story[]={
	{0,	"Hello, %s, happy hunting.",				0,		0,			{{"Hello, %s!",1},     		{"Yes?",1},				{NULL}}},
        {1,	"I've heard rumors that there's a treasure buried somewhere in this forest.",0,0,		{{"Rumors?",2},			{"Yes, I've heard the same.",2},	{"Shut up, %s.",20},{NULL}}},
	{2,	"Well, I think it's right here, but my spade broke....",0,		0,			{{"Here, take my spade.",3},	{"I'll get a spade for thee, %s.",3},	{"That's too bad. Well, I must be going.",20},{NULL}}},
        {3,	"(Waiting for Spade)",					SPEC_ITEMDRV,	77,			{{NULL,4},			{NULL}}},
	{4,	"Thanks, %s! *Starts digging*",				SPEC_SETBIT,	BIT_FOREST,		{{NULL}}},
	
	{20,	"Oh? *Sighs*",						0,		0,			{{NULL}}},
	{0,	NULL}
};

struct story ruby17_story[]={
	{0,	"Welcome to Exkordon, %s.",				0,		0,			{{"Hello, %s!",1},     		{"Yes?",1},				{NULL}}},
        {1,	"I'm trying to get on the good side of the governor, and I thought I'd bring him some nice food as a gift. Dost thou, perchance, know what he likes to eat?",0,0,	{{"Apple pie.",3},{"Plum pie.",3},{"Strawberry pie.",2},{"Pear pie.",3},{NULL}}},
        {2,	"Ah. Good. Thanks, %s!",				SPEC_SETBIT,	BIT_GOVERNOR,		{{NULL}}},
	{3,	"Ah. Good. Thanks, %s!",				SPEC_SETBIT,	BIT_WRONG1,		{{NULL}}},
	
        {0,	NULL}
};


struct sidestory_ppd
{
	unsigned int bits;
	struct story_npc ruby2;
	struct ruby_aston_npc ruby3;
	struct story_npc ruby5;
	struct story_npc ruby6;
	struct story_npc ruby8;
	struct story_npc ruby10;	
	struct story_npc ruby11;
	struct story_npc ruby12;
	struct story_npc ruby15;
	struct story_npc ruby16;
	struct story_npc ruby17;
	struct story_npc ruby18;	
};

struct ruby_driver_data
{
	int last_char,last_time;
};

int check_story_special(int cn,int co,struct sidestory_ppd *ppd,struct story_npc *me,struct story *story,int nr,int opt)
{
	switch(story[nr].special) {
		case SPEC_ITEM:		return 1;
		case SPEC_ALCITEM:	return 1;
		case SPEC_ITEM32:	return 1;
		case SPEC_ITEMDRV:	return 1;

                case SPEC_SETBIT:	return ((ppd->bits&opt)!=opt);
		case SPEC_ISSET:	return ((ppd->bits&opt)==opt);
		case SPEC_ISNOTSET:	return ((ppd->bits&opt)!=opt);
		default:		return 1;
	}
}

void do_story_special(int cn,int co,struct sidestory_ppd *ppd,struct story_npc *me,struct story *story,int nr,int opt)
{
	switch(story[nr].special) {
                case SPEC_SETBIT:	ppd->bits|=opt; break;
                default:		break;
	}
}

int do_story(int cn,int co,struct sidestory_ppd *ppd,struct story_npc *me,struct story *story)
{
	int n,m,pos=0;
	char buf[512];

        if (realtime-me->last_say<60) return 0;
        //say(cn,"bits=%X, state=%d",ppd->bits,me->state);

        for (n=0; story[n].text; n++) {
                if (story[n].state==me->state && check_story_special(cn,co,ppd,me,story,n,story[n].opt)) {
			pos+=sprintf(buf+pos,story[n].text,ch[co].name);
			if (story[n].a[0].text) {
				pos+=sprintf(buf+pos," [ ");
				for (m=0; story[n].a[m].text; m++) {
					if (m>0) pos+=sprintf(buf+pos," / ");
					pos+=sprintf(buf+pos,"°c4");
					pos+=sprintf(buf+pos,story[n].a[m].text,ch[cn].name);
					pos+=sprintf(buf+pos,"°c0");
				}
				pos+=sprintf(buf+pos," ]"); // (state=%d, like=%d)",me->state,me->like);
				me->last_say=realtime;
				say(cn,buf);
				do_story_special(cn,co,ppd,me,story,n,story[n].opt);
				return 1;
			} else if (story[n].a[0].nextstate) {
                                say(cn,buf);
				do_story_special(cn,co,ppd,me,story,n,story[n].opt);

				if (story[n].special!=SPEC_ITEM && story[n].special!=SPEC_ALCITEM && story[n].special!=SPEC_ITEM32 && story[n].special!=SPEC_ITEMDRV) {
					me->state=story[n].a[0].nextstate;
					me->last_say=0;
					return 1;
				} else {
					me->last_say=0x7fffffff;
					return 2;
				}
			} else {
				me->last_say=0x7fffffff;
				say(cn,buf);
				do_story_special(cn,co,ppd,me,story,n,story[n].opt);
				return 2;
			}			
		}
	}
	me->last_say=0x7fffffff;
	return 0;
}

int hear_story(int cn,int co,struct sidestory_ppd *ppd,struct story_npc *me,struct story *story,char *text)
{
	int n,m;
	char buf[256];

	if (strcasestr(text,"repeat")) {
		me->last_say=-60;
		return 1;
	}

	if (strcasestr(text,"reset") && (ch[co].flags&CF_GOD)) {
                bzero(me,sizeof(struct story_npc));
		me->last_say=-60;
		return 1;
	}

	for (n=0; story[n].text; n++) {
		if (story[n].state==me->state && check_story_special(cn,co,ppd,me,story,n,story[n].opt)) {
			for (m=0; story[n].a[m].text; m++) {
				sprintf(buf,story[n].a[m].text,ch[cn].name);
				if (strcasestr(text,buf)) {
					me->state=story[n].a[m].nextstate;
					me->last_say=0;
					return 1;
				}
			}
		}
	}
	return 0;
}

int give_story(int cn,int co,struct sidestory_ppd *ppd,struct story_npc *me,struct story *story,int in)
{
	int n;

        for (n=0; story[n].text; n++) {
		if (story[n].state==me->state && story[n].special==SPEC_ITEM && story[n].opt==it[in].ID) {
			me->state=story[n].a[0].nextstate;
			me->last_say=0;
			return 1;
		}
		if (story[n].state==me->state && story[n].special==SPEC_ALCITEM && it[in].ID==IID_ALCHEMY_INGREDIENT && story[n].opt==it[in].drdata[0]) {
			me->state=story[n].a[0].nextstate;
			me->last_say=0;
			return 1;
		}
		if (story[n].state==me->state && story[n].special==SPEC_ITEM32 && story[n].opt==it[in].ID && it[in].drdata[0]==32) {
			me->state=story[n].a[0].nextstate;
			me->last_say=0;
			return 1;
		}
		if (story[n].state==me->state && story[n].special==SPEC_ITEMDRV && story[n].opt==it[in].driver) {
			me->state=story[n].a[0].nextstate;
			me->last_say=0;
			return 1;
		}
		
	}
	return 0;
}

static int find_weapon(int cn,int *pskill,int *plevel)
{
	int n,best,skl;

	for (best=skl=0,n=V_DAGGER; n<=V_TWOHAND; n++) {
		if (ch[cn].value[1][n]>best) {
			skl=n;
			best=ch[cn].value[1][n];
		}
	}
	if (!skl) return 0;
	
	if (best<10) best=1;
	else best=(best/10)*10;

	best=min(110,best);

	if (pskill) *pskill=skl;
	if (plevel) *plevel=best;

	return 1;
}

int ruby_create_item(struct ruby_aston_npc *me)
{
	int sprite,dam,flags,in;
	char *name,*desc;

	//xlog("skl=%d, level=%d, mod1=%d, mod2=%d, add=%d",me->skl,me->level,me->mod1,me->mod2,me->add);

	switch(me->skl) {
		case V_TWOHAND:		sprite=10373; dam=3; flags=IF_TWOHAND|IF_WNTWOHANDED; name="Two-Handed Sword"; desc="two-handed sword"; break;
		case V_SWORD:		sprite=10384; dam=0; flags=IF_SWORD; name="Sword"; desc="sword"; break;
		case V_DAGGER:		sprite=10395; dam=-3; flags=IF_DAGGER; name="Dagger"; desc="dagger"; break;
		case V_STAFF:		sprite=10406; dam=-2; flags=IF_STAFF|IF_WNTWOHANDED; name="Staff"; desc="staff"; break;
		case V_HAND:		sprite=51616; dam=0; flags=0; name="Gloves"; desc="pair of gloves"; break;
		default:		return 0;
	}

	in=create_item("ruby_weapon");
	if (!in) return 0;

	it[in].sprite=sprite;
	
	if (me->skl!=V_HAND) {
		it[in].mod_index[0]=V_WEAPON;
		it[in].mod_value[0]=dam+10+(me->level/10)*10+4;

		it[in].mod_index[1]=-me->skl;
		it[in].mod_value[1]=me->level;
	}
	
	it[in].flags|=flags|IF_WNRHAND;

	it[in].mod_index[2]=me->mod1;
	it[in].mod_index[3]=me->mod2;
	it[in].mod_value[2]=it[in].mod_value[3]=me->add;

	set_item_requirements(in);

	strcpy(it[in].name,name);
	sprintf(it[in].description,"A %s, bearing the inscription 'Ruby'.",desc);

	return in;
}

int ruby_aston_char(int cn,int co,struct sidestory_ppd *ppd)
{
	int bitcount=0,bitvalue=0,n,didsay=0,in,val;
	unsigned int bit;

        if (realtime-ppd->ruby3.last_say<7) return 0;
	
	if (realtime-ppd->ruby3.last_see>60*5) ppd->ruby3.state=0;
	ppd->ruby3.last_see=realtime;

	for (n=0; n<32; n++) {
		bit=1u<<n;
		if (ppd->bits&bit) {
			bitcount++;
			switch(bit) {
				case BIT_ZOMBIE:	bitvalue+=40; break;
				case BIT_SHROOM1:       bitvalue+=10; break;
				case BIT_SHROOM2:	bitvalue+=10; break;
				case BIT_SHROOM3:	bitvalue+=10; break;
				case BIT_OCRYSTAL:	bitvalue+=20; break;
				case BIT_RCRYSTAL:	bitvalue+=20; break;
				case BIT_FSCROLL:	bitvalue+=20; break;
				case BIT_ISLENA:	bitvalue+=50; break;
				case BIT_TORCH:		bitvalue+=20; break;
				case BIT_SWAMP:		bitvalue+=20; break;
				case BIT_FOREST:	bitvalue+=20; break;
				case BIT_GOVERNOR:	bitvalue+=20; break;


			} // sum=260
		}
	}
	//say(cn,"IN: bitvalue=%d, bitcount=%d, state=%d",bitvalue,bitcount,ppd->ruby3.state);

	switch(ppd->ruby3.state) {
		case 0:		if (bitvalue>=30) ppd->ruby3.state=3;
				else ppd->ruby3.state++;
				break;
		case 1:		if (!bitvalue) quiet_say(cn,"Hello, %s. I'm %s, and this is my house. The door is over there...",ch[co].name,ch[cn].name);
				else quiet_say(cn,"Hello, %s. I'm %s. Nice to see thee again. I'm kinda busy now, so if thou wouldst come by again later?",ch[co].name,ch[cn].name);
				ppd->ruby3.state++; didsay=1;
				break;
		case 2:		if (bitvalue>=30) ppd->ruby3.state=0;
				break;
		case 3:		quiet_say(cn,"Oh, hello again, %s. I guess I owe thee something for thine help. I collect weapons of all kinds. Maybe thou'd take one of them for all thy trouble?",ch[co].name);
				ppd->ruby3.state++; didsay=1;
				break;
		case 4:		if (find_weapon(co,&ppd->ruby3.skl,&ppd->ruby3.level)) ppd->ruby3.state++;
				else {
					quiet_say(cn,"Uh, I'm confused. Please come back later.");
                                        ppd->ruby3.state=0;
					didsay=1;
				}
				break;
		case 5:         if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.skl==V_HAND) quiet_say(cn,"I guess I have some Gloves somewhere.");
				else if (ppd->ruby3.skl==V_TWOHAND) quiet_say(cn,"I guess I have some two-handed weapons with a requirement of %d somewhere.",ppd->ruby3.level);
				else quiet_say(cn,"I guess I have some %ss with a requirement of %d somewhere.",skill[ppd->ruby3.skl].name,ppd->ruby3.level);
                                ppd->ruby3.state++;
				didsay=1;
				break;
		case 6:		if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				quiet_say(cn,"What modifier wouldst thou like? [ °c4Tactics°c0 / °c4%s°c0 / °c4Pulse°c0 ]",skill[ppd->ruby3.skl].name);
				ppd->ruby3.state++;
				didsay=1;
				break;
		case 7:		if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				break;
		case 8:		if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.mod1<0 || ppd->ruby3.mod1>=V_MAX) { ppd->ruby3.state=0; break; }
				quiet_say(cn,"Ok, %s it is. What other modifier wouldst thou like? [ °c4Wisdom°c0 / °c4Intuition°c0 / °c4Agility°c0 / °c4Strength°c0 ]",skill[ppd->ruby3.mod1].name);
				ppd->ruby3.state++;
				didsay=1;
				break;
		case 9:		if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.mod1<0 || ppd->ruby3.mod1>=V_MAX) { ppd->ruby3.state=0; break; }
				break;
		case 10:	if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.mod1<0 || ppd->ruby3.mod1>=V_MAX) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.mod2<0 || ppd->ruby3.mod2>=V_MAX) { ppd->ruby3.state=0; break; }
				quiet_say(cn,"Ok, %s it is.",skill[ppd->ruby3.mod2].name);
				ppd->ruby3.state++;
				//didsay=1;
				break;
		case 11:	if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.mod1<0 || ppd->ruby3.mod1>=V_MAX) { ppd->ruby3.state=0; break; }
				if (ppd->ruby3.mod2<0 || ppd->ruby3.mod2>=V_MAX) { ppd->ruby3.state=0; break; }
				
                                if (ch[co].level<15) val=4;
				else if (ch[co].level<17) val=5;
				else if (ch[co].level<20) val=6;
				else if (ch[co].level<23) val=7;
				else if (ch[co].level<26) val=8;
				else if (ch[co].level<30) val=9;
				else if (ch[co].level<33) val=10;
				else if (ch[co].level<36) val=11;
				else if (ch[co].level<40) val=12;
				else if (ch[co].level<43) val=13;
				else if (ch[co].level<46) val=14;
				else if (ch[co].level<50) val=15;
				else if (ch[co].level<53) val=16;
				else if (ch[co].level<56) val=17;
				else if (ch[co].level<60) val=18;
				else if (ch[co].level<63) val=19;
                                else val=20;
				if (!(ch[co].flags&CF_ARCH)) val=min(val,9);

				ppd->ruby3.add=min(val,bitvalue*20/260);
				
				if (ppd->ruby3.skl==V_HAND) quiet_say(cn,"I have a pair of gloves, with %s+%d and %s+%d. [ °c4That'd be great!°c0 / °c4Uh, no, but thanks anyway.°c0 ]",skill[ppd->ruby3.mod1].name,ppd->ruby3.add,skill[ppd->ruby3.mod2].name,ppd->ruby3.add);
				else if (ppd->ruby3.skl==V_TWOHAND) quiet_say(cn,"I have a two-handed sword with %s+%d and %s+%d and with a requirement of %d. [ °c4That'd be great!°c0 / °c4Uh, no, but thanks anyway.°c0 ]",skill[ppd->ruby3.mod1].name,ppd->ruby3.add,skill[ppd->ruby3.mod2].name,ppd->ruby3.add,ppd->ruby3.level);
				else quiet_say(cn,"I have a %s with with %s+%d and %s+%d and a requirement of %d. [ °c4That'd be great!°c0 / °c4Uh, no, but thanks anyway.°c0 ]",skill[ppd->ruby3.skl].name,skill[ppd->ruby3.mod1].name,ppd->ruby3.add,skill[ppd->ruby3.mod2].name,ppd->ruby3.add,ppd->ruby3.level);
				ppd->ruby3.state++;
				didsay=1;
				break;
		case 12:	break;
		case 13:	quiet_say(cn,"Oh. Alright. Feel free to visit me again, I might have something different for thee then. Just say 'repeat'.");
				ppd->ruby3.state++;
				didsay=1;
				break;
		case 14:	break;
		case 15:	quiet_say(cn,"Here you go...");
				in=ruby_create_item(&ppd->ruby3);
				if (!in) {
					quiet_say(cn,"Uh, I'm confused. Please come back later.");
                                        ppd->ruby3.state=0;
					didsay=1;
				}
				if (!give_char_item(co,in)) {
					quiet_say(cn,"Uh, try again when your inventory isn't full.");
                                        ppd->ruby3.state=0;
					didsay=1;
				}
                                bzero(&ppd->ruby2,sizeof(struct story_npc));
				bzero(&ppd->ruby5,sizeof(struct story_npc));
				bzero(&ppd->ruby6,sizeof(struct story_npc));
				bzero(&ppd->ruby8,sizeof(struct story_npc));
				bzero(&ppd->ruby10,sizeof(struct story_npc));
				bzero(&ppd->ruby11,sizeof(struct story_npc));
				bzero(&ppd->ruby12,sizeof(struct story_npc));
				bzero(&ppd->ruby15,sizeof(struct story_npc));
				bzero(&ppd->ruby16,sizeof(struct story_npc));
				bzero(&ppd->ruby17,sizeof(struct story_npc));
				bzero(&ppd->ruby18,sizeof(struct story_npc));
				ppd->bits=0;
				
				log_char(co,LOG_SYSTEM,0,"You sense that %s has forgotten all about you...",ch[cn].name);
				ppd->ruby3.state=0;
				didsay=1;
                                break;
	}
	//say(cn,"OUT: bitvalue=%d, state=%d",bitvalue,ppd->ruby3.state);

	if (didsay) ppd->ruby3.last_say=realtime;
        return didsay;
}

void ruby_aston_text(int cn,int co,struct sidestory_ppd *ppd,char *ptr)
{
        while (*ptr && *ptr!=':') ptr++;
	if (!*ptr) return;
        if (strcasestr(ptr,"repeat")) { ppd->ruby3.state=0; ppd->ruby3.last_say=0; return; }
	if (strcasestr(ptr,"allbits") && (ch[co].flags&CF_GOD)) {
		ppd->bits=BIT_ZOMBIE|BIT_SHROOM1|BIT_SHROOM2|BIT_SHROOM3|BIT_OCRYSTAL|BIT_RCRYSTAL|BIT_FSCROLL|BIT_TORCH|BIT_FOREST|BIT_GOVERNOR;
		ppd->ruby3.state=0; ppd->ruby3.last_say=0;
		return;
	}
	
	if (ppd->ruby3.skl<V_DAGGER || ppd->ruby3.skl>V_TWOHAND) return;

        if (ppd->ruby3.state==7 && strcasestr(ptr,"tactics")) { ppd->ruby3.mod1=V_TACTICS; ppd->ruby3.state=8; ppd->ruby3.last_say=0; return; }
	if (ppd->ruby3.state==7 && strcasestr(ptr,"pulse")) { ppd->ruby3.mod1=V_PULSE; ppd->ruby3.state=8; ppd->ruby3.last_say=0; return; }
	if (ppd->ruby3.state==7 && strcasestr(ptr,skill[ppd->ruby3.skl].name)) { ppd->ruby3.mod1=ppd->ruby3.skl; ppd->ruby3.state=8; ppd->ruby3.last_say=0; return; }
	
	if (ppd->ruby3.state==9 && strcasestr(ptr,"wisdom")) { ppd->ruby3.mod2=V_WIS; ppd->ruby3.state=10; ppd->ruby3.last_say=0; return; }	
	if (ppd->ruby3.state==9 && strcasestr(ptr,"intuition")) { ppd->ruby3.mod2=V_INT; ppd->ruby3.state=10; ppd->ruby3.last_say=0; return; }
	if (ppd->ruby3.state==9 && strcasestr(ptr,"agility")) { ppd->ruby3.mod2=V_AGI; ppd->ruby3.state=10; ppd->ruby3.last_say=0; return; }
	if (ppd->ruby3.state==9 && strcasestr(ptr,"strength")) { ppd->ruby3.mod2=V_STR; ppd->ruby3.state=10; ppd->ruby3.last_say=0; return; }

	if (ppd->ruby3.state==12 && strcasestr(ptr,"be great")) { ppd->ruby3.state=15; ppd->ruby3.last_say=0; return; }
	if (ppd->ruby3.state==12 && strcasestr(ptr,"thanks anyway")) { ppd->ruby3.state=13; ppd->ruby3.last_say=0; return; }
}

void ruby_driver(int cn,int retval,int lastact)
{
        struct ruby_driver_data *dat;
	struct sidestory_ppd *ppd;
	struct msg *msg,*next;
	int co,ret,in;
	char *ptr;

        dat=set_data(cn,DRD_RUBYDRIVER,sizeof(struct ruby_driver_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        ;
                }

		if (msg->type==NT_CHAR) {
                        co=msg->dat1;

			if ((ch[co].flags&CF_PLAYER) &&
			    ch[co].driver!=CDR_LOSTCON &&
			    char_dist(cn,co)<16 &&
			    char_see_char(cn,co) &&
			    (ticker-dat->last_time>TICKS*30 || dat->last_char==co) &&
			    (ppd=set_data(co,DRD_SIDESTORY_PPD,sizeof(struct sidestory_ppd)))) {
				switch(areaID) {
					case 2:		ret=do_story(cn,co,ppd,&ppd->ruby2,ruby2_story); break;
					case 3:		ret=ruby_aston_char(cn,co,ppd); break;
					case 5:		ret=do_story(cn,co,ppd,&ppd->ruby5,ruby5_story); break;
					case 6:		ret=do_story(cn,co,ppd,&ppd->ruby6,ruby6_story); break;
					case 8:		ret=do_story(cn,co,ppd,&ppd->ruby8,ruby8_story); break;
					case 10:	ret=do_story(cn,co,ppd,&ppd->ruby10,ruby10_story); break;
					case 11:	ret=do_story(cn,co,ppd,&ppd->ruby11,ruby11_story); break;
					case 12:	ret=do_story(cn,co,ppd,&ppd->ruby12,ruby12_story); break;
					case 15:	ret=do_story(cn,co,ppd,&ppd->ruby15,ruby15_story); break;
					case 16:	ret=do_story(cn,co,ppd,&ppd->ruby16,ruby16_story); break;
					case 17:	ret=do_story(cn,co,ppd,&ppd->ruby17,ruby17_story); break;

					default:	ret=0; break;
				}
				if (ret==1) {
					dat->last_char=co;
					dat->last_time=ticker;
				} else if (ret==2) {
					dat->last_char=dat->last_time=0;
				}
                        }
		}

		if (msg->type==NT_TEXT) {
			ptr=(char*)msg->dat2;
			co=msg->dat3;

			if ((ch[co].flags&CF_PLAYER) &&
			    ch[co].driver!=CDR_LOSTCON &&
			    char_dist(cn,co)<16 &&
			    char_see_char(cn,co) &&
			    (ppd=set_data(co,DRD_SIDESTORY_PPD,sizeof(struct sidestory_ppd)))) {
				switch(areaID) {
					case 2:		hear_story(cn,co,ppd,&ppd->ruby2,ruby2_story,ptr); break;
					case 3:		ruby_aston_text(cn,co,ppd,ptr); break;
					case 5:		hear_story(cn,co,ppd,&ppd->ruby5,ruby5_story,ptr); break;
					case 6:		hear_story(cn,co,ppd,&ppd->ruby6,ruby6_story,ptr); break;
					case 8:		hear_story(cn,co,ppd,&ppd->ruby8,ruby8_story,ptr); break;
					case 10:	hear_story(cn,co,ppd,&ppd->ruby10,ruby10_story,ptr); break;
					case 11:	hear_story(cn,co,ppd,&ppd->ruby11,ruby11_story,ptr); break;
					case 12:	hear_story(cn,co,ppd,&ppd->ruby12,ruby12_story,ptr); break;
					case 15:	hear_story(cn,co,ppd,&ppd->ruby15,ruby15_story,ptr); break;
					case 16:	hear_story(cn,co,ppd,&ppd->ruby16,ruby16_story,ptr); break;
					case 17:	hear_story(cn,co,ppd,&ppd->ruby17,ruby17_story,ptr); break;

				}

                        }
			
		}
		
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {
				if ((ch[co].flags&CF_PLAYER) && (ppd=set_data(co,DRD_SIDESTORY_PPD,sizeof(struct sidestory_ppd)))) {
					switch(areaID) {
						case 2:		ret=give_story(cn,co,ppd,&ppd->ruby2,ruby2_story,in); break;
						case 5:		ret=give_story(cn,co,ppd,&ppd->ruby5,ruby5_story,in); break;
						case 6:		ret=give_story(cn,co,ppd,&ppd->ruby6,ruby6_story,in); break;
						case 8:		ret=give_story(cn,co,ppd,&ppd->ruby8,ruby8_story,in); break;
						case 10:	ret=give_story(cn,co,ppd,&ppd->ruby10,ruby10_story,in); break;
						case 11:	ret=give_story(cn,co,ppd,&ppd->ruby11,ruby11_story,in); break;
						case 12:	ret=give_story(cn,co,ppd,&ppd->ruby12,ruby12_story,in); break;
						case 15:	ret=give_story(cn,co,ppd,&ppd->ruby15,ruby15_story,in); break;
						case 16:	ret=give_story(cn,co,ppd,&ppd->ruby16,ruby16_story,in); break;
						case 17:	ret=give_story(cn,co,ppd,&ppd->ruby17,ruby17_story,in); break;


						default:	ret=0; break;
					}
				} else ret=0;
				
				ch[cn].citem=0;
				if (ret || !give_char_item(co,in)) {
					destroy_item(in);
				}
			}
		}

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
	if (fight_driver_follow_invisible(cn)) return;

	if (spell_self_driver(cn)) return;
	if (regenerate_driver(cn)) return;

	if (dat->last_time && map_dist(ch[cn].x,ch[cn].y,ch[cn].tmpx,ch[cn].tmpy)>16) dat->last_time=0;

	if (dat->last_char && !char_see_char(cn,dat->last_char)) dat->last_char=dat->last_time=0;

	if (ticker-dat->last_time<TICKS*60) {
		int dir;

		co=dat->last_char;

                dir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
	
		if (tile_char_dist(cn,co)>2 && move_driver(cn,ch[co].x,ch[co].y,2)) return;
		turn(cn,dir);		
	} else if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,retval,lastact)) return;

        do_idle(cn,TICKS/2);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_RUBY:		ruby_driver(cn,ret,lastact); return 1;

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
		case CDR_RUBY:		return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                case CDR_RUBY:		return 1;

		default:		return 0;
	}
}
