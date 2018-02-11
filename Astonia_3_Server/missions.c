/*
 * $Id: missions.c,v 1.4 2006/08/03 15:48:34 ssim Exp $
 *
 * $Log: missions.c,v $
 * Revision 1.4  2006/08/03 15:48:34  ssim
 * fixed chest bug in jobbington
 *
 * Revision 1.3  2006/07/16 18:47:40  ssim
 * further increase of points per mission
 *
 * Revision 1.2  2005/09/25 15:18:35  ssim
 * reduced most rewards prices by a factor of 2; increased rewards for higher difficulties by about 50%
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
#include "player_driver.h"

#define MAXDIFF	1000


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
	{{"job","alpha",NULL},NULL,4},
	{{"job","beta",NULL},NULL,5},
	{{"job","gamma",NULL},NULL,6},
	{{"accept","job","alpha",NULL},NULL,7},
	{{"accept","job","beta",NULL},NULL,8},
	{{"accept","job","gamma",NULL},NULL,9},
	{{"fail",NULL},NULL,10},
	{{"offer",NULL},NULL,11},
	{{"increase",NULL},NULL,12},
	{{"decrease",NULL},NULL,13},
	{{"gimme100",NULL},NULL,14},
	{{"gimme1000",NULL},NULL,15},
	{{"gimme10000",NULL},NULL,16},
	{{"gimme100000",NULL},NULL,17},
	{{"special","offer",NULL},NULL,18},
	{{"buy","the","special","offer",NULL},NULL,19},	
        {{"new","job",NULL},NULL,2},
	{{"reset","me",NULL},NULL,3},
	
	{{"one","skill",NULL},NULL,20},
	{{"two","skills",NULL},NULL,21},
	{{"three","skills",NULL},NULL,22}
};

struct reward
{
	char *code;
	char *desc;
	int value;
	char *itmtmp;
};

static int init_done=0;
struct reward mis_rew[]={
        /*{"LNPOD","Leonid's Potion of Defense",940,"mis_pot1"},
	{"AZPOD","Arcazor's Potion of Defense",2820,"mis_pot2"},
	{"GCPOD","Geracas' Potion of Defense",8460,"mis_pot3"},

	{"LNPOO","Leonid's Potion of Offense",940,"mis_pot4"},
	{"AZPOO","Arcazor's Potion of Offense",2820,"mis_pot5"},
	{"GCPOO","Geracas' Potion of Offense",8460,"mis_pot6"},

	{"CRPOD","Carisah's Potion of Defense",940,"mis_pot7"},
	{"WCPOD","Wicala's Potion of Defense",2820,"mis_pot8"},
	{"SLPOD","Sulina's Potion of Defense",8460,"mis_pot9"},

	{"CRPOO","Carisah's Potion of Offense",940,"mis_pot10"},
	{"WCPOO","Wicala's Potion of Offense",2820,"mis_pot11"},
	{"SLPOO","Sulina's Potion of Offense",8460,"mis_pot12"},*/
	
	{"LNROS","Leonid's Ring of Skirmish",1875/4,"mis_ring1"},
	{"LNROB","Leonid's Ring of Battle",5625/4,"mis_ring2"},
	{"LNROW","Leonid's Ring of War",26250/4,"mis_ring3"},

	{"CRROS","Carisah's Ring of Skirmish",1875/4,"mis_ring4"},
	{"CRROB","Carisah's Ring of Battle",5625/4,"mis_ring5"},
	{"CRROW","Carisah's Ring of War",26250/4,"mis_ring6"},

	{"AZROS","Arcazor's Ring of Skirmish",3750/4,"mis_ring7"},
	{"AZROB","Arcazor's Ring of Battle",11250/4,"mis_ring8"},
	{"AZROW","Arcazor's Ring of War",52500/4,"mis_ring9"},

	{"WCROS","Wicala's Ring of Skirmish",3750/4,"mis_ring10"},
	{"WCROB","Wicala's Ring of Battle",11250/4,"mis_ring11"},
	{"WCROW","Wicala's Ring of War",52500/4,"mis_ring12"},

	{"MEXP1","Military Experience 1",100,"MEXP"},
	{"MEXP2","Military Experience 2",1000,"MEXP"},
	{"MEXP3","Military Experience 3",10000,"MEXP"},

	{"GOLD1","Money 1",10,"GOLD"},
	{"GOLD2","Money 2",100,"GOLD"},
	{"GOLD3","Money 3",1000,"GOLD"},
	{"GOLD4","Money 4",10000,"GOLD"},

	{"CBPOT","Huge Combo Potion",125,"mis_combopot"},
	{"RNORB","Random Orb",5250,"RNORB"},
	{"FGPOT","Forgetfulness Potion",3750/2,"forgetfulness_potion"},
	{"CTPOT","Custom Stat Potion",500,"CTPOT"},

	{"SCPOT","Potion of Security",2500/2,"security_potion"}
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
		for (q=0; q<sizeof(mis_rew)/sizeof(mis_rew[0]); q++) {
			//xlog("%s - %s",mis_rew[q].code,wordlist[0]);
			if (!strcmp("show",wordlist[0]) && !strcasecmp(mis_rew[q].code,wordlist[1])) return q+1000;
			if (!strcmp("ibuy",wordlist[0]) && !strcasecmp(mis_rew[q].code,wordlist[1])) return q+2000;
		}
	}

        return 0;
}


//-----------------------

int find_skill_text(int cn,int type,char *text,int co)
{
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

	if (!strcasestr(text," skill")) return 0;

	if (!strncasecmp(text,"pulse",5)) return V_PULSE;
	if (!strncasecmp(text,"dagger",6)) return V_DAGGER;
	if (!strncasecmp(text,"hand",4)) return V_HAND;
	if (!strncasecmp(text,"staff",5)) return V_STAFF;
	if (!strncasecmp(text,"sword",5)) return V_SWORD;
	if (!strncasecmp(text,"twohand",7)) return V_TWOHAND;
	if (!strncasecmp(text,"two hand",8)) return V_TWOHAND;
	if (!strncasecmp(text,"two-hand",8)) return V_TWOHAND;
	if (!strncasecmp(text,"attack",6)) return V_ATTACK;
	if (!strncasecmp(text,"parry",5)) return V_PARRY;
	if (!strncasecmp(text,"warcry",6)) return V_WARCRY;
	if (!strncasecmp(text,"tactics",7)) return V_TACTICS;
	if (!strncasecmp(text,"surround",8)) return V_SURROUND;
	if (!strncasecmp(text,"body control",12)) return V_BODYCONTROL;
	if (!strncasecmp(text,"body-control",12)) return V_BODYCONTROL;
	if (!strncasecmp(text,"bodycontrol",11)) return V_BODYCONTROL;
	if (!strncasecmp(text,"speed",5)) return V_SPEED;
	if (!strncasecmp(text,"barter",6)) return V_BARTER;
	if (!strncasecmp(text,"percept",7)) return V_PERCEPT;
	if (!strncasecmp(text,"stealth",7)) return V_STEALTH;
	if (!strncasecmp(text,"bless",5)) return V_BLESS;
	if (!strncasecmp(text,"heal",4)) return V_HEAL;
	if (!strncasecmp(text,"freeze",6)) return V_FREEZE;
	if (!strncasecmp(text,"magic shield",12)) return V_MAGICSHIELD;
	if (!strncasecmp(text,"magic-shield",12)) return V_MAGICSHIELD;
	if (!strncasecmp(text,"magicshield",11)) return V_MAGICSHIELD;
	if (!strncasecmp(text,"flash",5)) return V_FLASH;
	if (!strncasecmp(text,"lightning",8)) return V_FLASH;
	if (!strncasecmp(text,"fire",4)) return V_FIRE;
	if (!strncasecmp(text,"regen",5)) return V_REGENERATE;
	if (!strncasecmp(text,"meditate",8)) return V_MEDITATE;
	if (!strncasecmp(text,"immunity",8)) return V_IMMUNITY;
	if (!strncasecmp(text,"duration",8)) return V_DURATION;
	if (!strncasecmp(text,"rage",4)) return V_RAGE;
	
	return 0;
}

#define MISS_TYPE_KILL	1

static char *misstypetext[]={
	"NULL",
	"Kill"
};

struct mission_data
{
	char *title;
	char *description;
	int type;
	char *basename;
	char *basedesc;
	char *bossname;
	char *bossdesc;
	char *bigbossname;
	char *bigbossdesc;
	int sprite;
	int bosssprite;
	char *strengthname[3];
	char *temp;
	char *bosstemp;
	char *itemtemp;
	char *itemname;
	char *itemdesc;
	float diff_mult;
	int area;
	long long char_flags;
};

struct mission_data thief_data=
{
	"Stolen Documents",
	"A bold thief named 'Sacewan' from the famous gang 'The Pickers' has stolen some vitally important documents from a friend of mine. Please retrieve the documents, and kill Sacewan and his whole gang.",
	MISS_TYPE_KILL,
	"Thief",
	"A thief belonging to the famous gang 'The Pickers'.",
	"Sacewan",
	"The boss of the gang 'The Pickers'.",
	"Thief Lord",
	"Sacewan's boss.",
	312,
	312,
	{" Apprentice", "", " Master"},
	"mis_warrior",
	"mis_seyan",
	"mis_documents",
	"Documents",
	"The stolen documents.",
	0.8,
	0,
	CF_ALIVE
};

struct mission_data spy_data=
{
	"Silence the Spy",
	"A spy has stolen a important secret from my boss. He's fled into the wilderness while his messengers are trying to find a buyer for the information. See to it the deal is never made.",
	MISS_TYPE_KILL,
	"Bodyguard",
	"One of the spy's hirelings.",
	"Spy",
	"The spy who stole the book.",
	"Spy Lord",
	"Uh-oh. This is the spies's boss. Looks tough.",
	312,
	312,
	{" Trainee", "", " Master"},
	"mis_warrior",
	"mis_seyan",
	"mis_book",
	"Book",
	"The stolen book.",
	0.75,
	1,
	CF_ALIVE
};

struct mission_data beast_data=
{
	"Swamp Beast Invasion",
	"A group of swamp beasts has invaded one of our outposts. Please free the outpost of their presence.",
	MISS_TYPE_KILL,
	"Swamp Beast",
	"A vicious looking beast.",
	"Beasty Boss",
	"This looks like the swamp beasts leader.",
	"Beast Lord",
	"A very vicious looking beast. Must be the leader's leader.",
	300,
	300,
	{" Youngling", "", " Elder"},
	"mis_beast",
	"mis_beast",
	NULL,
	NULL,
	NULL,
	0.75,
	2,
	CF_ALIVE
};

struct mission_data ruffian_data=
{
	"Dispatch Ruffians",
	"A group of ruffians has taken camp in my superior's summer house. Please see to it that they leave. Be careful, their boss, one 'Gorinion', is said to be a powerful mage.",
	MISS_TYPE_KILL,
	"Ruffian",
	"A ruffian under the command of Gorinion.",
	"Gorinion",
	"The ruffian's leader.",
	"Ruffian Lord",
	"My, oh, my, what a hunk.",
	15,
	13,
	{" Newbie", "", " Bottomkicker"},
	"mis_warrior",
	"mis_mage",
	NULL,
	NULL,
	NULL,
	0.75,
	3,
	CF_ALIVE
};

struct mission_data vampire_data=
{
	"Vampire Infection",
	"There have been rumors about a group of vampires in a town nearby. Please investigate the town's graveyard for any signs of vampires. Make all vampires you find 'see the light'.",
	MISS_TYPE_KILL,
	"Vampire",
	"White skin, fangs, blood on its lips. This is a vampire, alright.",
	"Vampire Master",
	"White skin, fangs, blood on its lips. This is a vampire, alright.",
	"Vampire Lord",
	"White skin, fangs, blood on its lips. This is a vampire, alright.",
	23,
	23,
	{" Youngling", "", " Elder"},
	"mis_seyan",
	"mis_seyan",
	NULL,
	NULL,
	NULL,
	1.2,
	4,
	CF_UNDEAD
};

struct mission_data graverobber_data=
{
	"Graverobber",
	"This time I have a delicate mission for you. I have discovered the location of an ancient tomb in old writings, which is said to hold the 'Magical Spoon of Doom'. Please enter the tomb and retrieve the spoon.",
	MISS_TYPE_KILL,
	"Zombie",
	"Rotten flesh, decaying bones, weird grin. Looks like a Zombie.",
	"Zombie Boss",
	"Rotten flesh, decaying bones, weird grin, lots of muscle. Looks like a Zombie Boss.",
	"Zombie Lord",
	"Rotten flesh, decaying bones, weird grin, lots of muscle. Looks like the Zombie Bosses boss.",
	9,
	9,
	{" Slave", "", " Master"},
	"mis_warrior",
	"mis_warrior",
	"mis_spoon",
	"Spoon of Doom",
	"The magical spoon of doom. Even Gwendylon has heard about it.",
	1.25,
	4,
	CF_UNDEAD
};

struct mission_data hideseek_data=
{
	"Hide and Seek",
	"A wizard-friend of my boss has hidden a ring in a dungeon which my boss is to retrieve. Some kind of bet I heard. Go there, get the ring, and kill anything that comes into your way.",
	MISS_TYPE_KILL,
	"Skeleton",
	"My, what a bony guy. Uh, on second look: It's just bones.",
	"Skeleton Master",
	"My, what a bony guy. Uh, on second look: It's just bones.",
	"Skeleton Lord",
	"My, what a bony guy. Uh, on second look: It's just bones.",
	8,
	8,
	{" Weakling", "", " Strongling"},
	"mis_warrior",
	"mis_warrior",
	"mis_ring",
	"Ring",
	"A simple, silver ring. The object of 'The Bet'.",
	1.50,
	5,
	CF_UNDEAD
};

struct mission_data *mdtab[]={
	&thief_data,
	&spy_data,
	&beast_data,
	&ruffian_data,
	&vampire_data,
	&graverobber_data,
	&hideseek_data
};


void offer_mission_sub(int cn,int co,int idx,struct mission_ppd *ppd)
{
	static char *missname[3]={"Alpha","Beta","Gamma"};

	log_char(co,LOG_SYSTEM,0," ");
        log_char(co,LOG_SYSTEM,0,"°c2Job %s \006%s \020%s \024Difficulty %d°c0",missname[idx],mdtab[ppd->sm[idx].mdidx]->title,misstypetext[ppd->sm[idx].type],ppd->sm[idx].difficulty);
        log_char(co,LOG_SYSTEM,0," ");
	log_char(co,LOG_SYSTEM,0,"%s",mdtab[ppd->sm[idx].mdidx]->description);
	log_char(co,LOG_SYSTEM,0," ");
	log_char(co,LOG_SYSTEM,0,"Do you °c4accept Job %s°c0? Accepting the job will teleport you to the job area immediately. Or do you want to look at °c4Job %s°c0 or °c4Job %s°c0 instead?",
		missname[idx],idx==0 ? missname[1] : missname[0],idx==2 ? missname[1] : missname[2]);
}

void offer_mission(int cn,int co,struct mission_ppd *ppd)
{
	int n;
	static char *missname[3]={"Alpha","Beta","Gamma"};

	quiet_say(cn,"Please choose one of the following jobs (this will display job details, you get a chance to accept or refuse the job later):");

	for (n=0; n<3; n++) {
		if (!ppd->sm[n].type) {
			do {
				ppd->sm[n].mdidx=RANDOM(sizeof(mdtab)/sizeof(mdtab[0]));
				ppd->sm[n].type=mdtab[ppd->sm[n].mdidx]->type;
			} while (ppd->sm[n].mdidx==ppd->sm[n==0 ? 1 : 0].mdidx || ppd->sm[n].mdidx==ppd->sm[n==1 ? 2 : 1].mdidx);
			
			switch(ppd->sm[n].type) {
				case MISS_TYPE_KILL:	ppd->sm[n].difficulty=ppd->dif_kill+RANDOM(10); break;
			}
		}
		log_char(co,LOG_SYSTEM,0,"°c4Job %s°c0 \006%s \020%s \024Difficulty %d",missname[n],mdtab[ppd->sm[n].mdidx]->title,misstypetext[ppd->sm[n].type],ppd->sm[n].difficulty);
	}	
}

int build_fighter(int x,int y,int diff,int keyID,char *keyname,char *name,char *temp,char *desc,int fID,int sprite,int hasspecialitem,long long extra_flags)
{
	int cn,n,val,in;
	//char buf[80];

	cn=create_char(temp,0);

        for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost) continue;
		if (!ch[cn].value[1][n]) continue;

		switch(n) {
			case V_HP:		val=max(10,diff-15); break;
			case V_ENDURANCE:	val=max(10,diff-15); break;
			case V_MANA:		val=max(10,diff-15); break;
			
			case V_WIS:		val=max(10,diff-25); break;
			case V_INT:		val=max(10,diff-5); break;
			case V_AGI:		val=max(10,diff-5); break;
			case V_STR:		val=max(10,diff-5); break;

			case V_HAND:		val=max(1,diff); break;
			case V_ARMORSKILL:	val=max(1,(diff/10)*10); break;
			case V_ATTACK:		val=max(1,diff); break;
			case V_PARRY:		val=max(1,diff); break;
			case V_IMMUNITY:	val=max(1,diff); break;
			
			case V_TACTICS:		val=max(1,diff-5); break;
			case V_WARCRY:		val=max(1,diff-15); break;
			case V_SURROUND:	val=max(1,diff-20); break;
			case V_BODYCONTROL:	val=max(1,diff-20); break;
			case V_SPEEDSKILL:	val=max(1,diff-20); break;
			case V_PERCEPT:		val=max(1,diff-10); break;
			
			case V_BLESS:		val=max(1,diff-5); break;
			case V_FIREBALL:	val=max(1,diff-5); break;
			case V_FREEZE:		val=max(1,diff-5); break;
			case V_MAGICSHIELD:	val=max(1,diff-5); break;

			default:		val=max(1,diff-30); break;
		}

		val=min(val,250);
		ch[cn].value[1][n]=val;
	}
        ch[cn].x=ch[cn].tmpx=x;
	ch[cn].y=ch[cn].tmpy=y;
	ch[cn].dir=DX_RIGHTDOWN;
	ch[cn].deaths=fID;
	
	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);
	if ((diff>100 && ch[cn].level<10) || ch[cn].level>200) ch[cn].level=200;

	ch[cn].sprite=sprite;
	ch[cn].flags|=extra_flags;
	strncpy(ch[cn].name,name,sizeof(ch[cn].name)); ch[cn].name[sizeof(ch[cn].name)-1]=0;
	strncpy(ch[cn].description,desc,sizeof(ch[cn].description)); ch[cn].description[sizeof(ch[cn].description)-1]=0;

	// give shrine key to NPC
	if (keyID) {
                in=create_item("mis_key");
		it[in].ID=keyID;
		strcpy(it[in].name,keyname);
                it[in].carried=cn;
                ch[cn].item[30]=in;
	}

	// give special item to big bosses
	if (hasspecialitem) {
		int str,base;
		
		if (ch[cn].level<10) { str=3; base=1; }
		else if (ch[cn].level<17) { str=4; base=10; }
		else if (ch[cn].level<24) { str=6; base=20; }
		else if (ch[cn].level<31) { str=7; base=30; }
		else if (ch[cn].level<38) { str=8; base=40; }
		else if (ch[cn].level<45) { str=10; base=50; }
		else if (ch[cn].level<52) { str=12; base=60; }
		else if (ch[cn].level<60) { str=14; base=70; }
		else if (ch[cn].level<66) { str=16; base=80; }
		else if (ch[cn].level<74) { str=18; base=90; }
		else { str=20; base=90; }
		
		in=create_special_item(str,base,1,10000);
		
		it[in].carried=cn;
                ch[cn].item[31]=in;
	}
	
        // create armor
        in=create_item("armor_spell");
	ch[cn].item[14]=in; it[in].carried=cn;
	it[in].mod_value[0]=max(13,min(113,ch[cn].value[1][V_ARMORSKILL]))*20;

	in=create_item("weapon_spell");
	ch[cn].item[15]=in; it[in].carried=cn;
	it[in].mod_value[0]=max(13,min(113,ch[cn].value[1][V_HAND]));

        update_char(cn);
	
	ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
	ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

	drop_char(cn,x,y,0);

        return ch[cn].level;
}


void mission_status(int cn,struct mission_ppd *ppd)
{
	struct mission_data *md=mdtab[ppd->md_idx];
	int line=3;

	log_char(cn,LOG_SYSTEM,0,"#%d0%s",line++,md->title);
	if (ppd->kill_easy[1]>ppd->kill_easy[0]) log_char(cn,LOG_SYSTEM,0,"#%d0- Kill %d %s%s%s",line++,ppd->kill_easy[1]-ppd->kill_easy[0],md->basename,md->strengthname[0],ppd->kill_easy[1]-ppd->kill_easy[0]!=1?"s":"");
	if (ppd->kill_normal[1]>ppd->kill_normal[0]) log_char(cn,LOG_SYSTEM,0,"#%d0- Kill %d %s%s%s",line++,ppd->kill_normal[1]-ppd->kill_normal[0],md->basename,md->strengthname[1],ppd->kill_normal[1]-ppd->kill_normal[0]!=1?"s":"");
	if (ppd->kill_hard[1]>ppd->kill_hard[0]) log_char(cn,LOG_SYSTEM,0,"#%d0- Kill %d %s%s%s",line++,ppd->kill_hard[1]-ppd->kill_hard[0],md->basename,md->strengthname[2],ppd->kill_hard[1]-ppd->kill_hard[0]!=1?"s":"");
	if (ppd->kill_boss[1]>ppd->kill_boss[0]) log_char(cn,LOG_SYSTEM,0,"#%d0- Kill %d %s%s",line++,ppd->kill_boss[1]-ppd->kill_boss[0],md->bossname,ppd->kill_boss[1]-ppd->kill_boss[0]!=1?"s":"");
	if (ppd->find_item[1]>ppd->find_item[0]) log_char(cn,LOG_SYSTEM,0,"#%d0- Find %s",line++,md->itemname);

	while (line<9) {
		log_char(cn,LOG_SYSTEM,0,"#%d0",line++);
	}
}

int teleporter(int cn,int x,int y)
{
	int oldx,oldy;

	if (abs(ch[cn].x-x)+abs(ch[cn].y-y)<2) return 0;

	oldx=ch[cn].x;
	oldy=ch[cn].y;

	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;
	if (ch[cn].player) player_driver_stop(ch[cn].player,0);

	if (drop_char_extended(cn,x,y,4)) return 1;

	drop_char(cn,oldx,oldy,0);

	return 0;
}

void mission_done(int cn,struct mission_ppd *ppd)
{
	if (ppd->kill_easy[1]>ppd->kill_easy[0]) return;
	if (ppd->kill_normal[1]>ppd->kill_normal[0]) return;
	if (ppd->kill_hard[1]>ppd->kill_hard[0]) return;
	if (ppd->kill_boss[1]>ppd->kill_boss[0]) return;
	if (ppd->find_item[1]>ppd->find_item[0]) return;

	//teleporter(cn,228,228);

	if (ppd->active) {
		ppd->solved=ppd->active; ppd->active=0;

		log_char(cn,LOG_SYSTEM,0,"You've finished the job. Good work, %s. Now talk to Mr. Jones for your reward.",ch[cn].name);
	}
}

void start_mission(int cn,int co,int idx,struct mission_ppd *ppd)
{
	int x,y,in,fx,fy,tx,ty,n,cc;
	struct mission_data *md;
	char buf[256];
	int easy=0,normal=0,hard=0,boss=0,chest=0,keyID,ex=0,ey=0,busy;
	int key1=0,key2=0,key3=0;

	md=mdtab[ppd->sm[idx].mdidx];

	if (shutdown_at && realtime-shutdown_at<10) {
		quiet_say(cn,"I'm sorry, %s, but a shutdown is in progress. Please come back after the shutdown.",ch[co].name);
		return;
	}

	for (n=0; n<6; n++) {
		if (n==5 && md->area==5) { busy=1; break; }
		
		fx=1+md->area*41;
		fy=1+n*41;
		tx=1+md->area*41+40;
		ty=1+n*41+40;
		
		busy=0;
		for (x=fx; x<=tx && !busy; x++) {
			for (y=fy; y<=ty; y++) {
				if ((cc=map[x+y*MAXMAP].ch) && ch[cc].flags&CF_PLAYER) { busy=1; break; }
			}
		}
		if (!busy) break;		
	}
	if (busy) {
		quiet_say(cn,"I'm sorry, %s, but it appears that this job is unavailable right now. Please choose a different one.",ch[co].name);
		return;
	}

	ppd->active=idx+1;
	ppd->solved=0;
	ppd->md_idx=ppd->sm[idx].mdidx;
	ppd->mcnt++;
        keyID=MAKE_ITEMID(DEV_ID_MISSION,ppd->mcnt*3);

	// count key-NPCs
	for (x=fx; x<=tx; x++) {
		for (y=fy; y<=ty; y++) {
                        if ((in=map[x+y*MAXMAP].it)) {
                                if (it[in].ID==IID_MISSIONFIGHTER) {
					switch(it[in].drdata[0]) {
						case 5:		key1++; break;
						case 6:		key2++; break;
						case 7:		key3++; break;
					}
				}
			}
		}
	}

	//say(cn,"key1=%d, key2=%d, key3=%d",key1,key2,key3);
	key1=RANDOM(key1)+1;
	key2=RANDOM(key2)+1;
	key3=RANDOM(key3)+1;
	//say(cn,"key1=%d, key2=%d, key3=%d",key1,key2,key3);

	// create NPCs
        for (x=fx; x<=tx; x++) {
		for (y=fy; y<=ty; y++) {
			if ((cc=map[x+y*MAXMAP].ch) && !(ch[cc].flags&CF_PLAYER)) {
				remove_destroy_char(cc);
				
			}
                        if ((in=map[x+y*MAXMAP].it)) {
				if ((it[in].flags&IF_TAKE) || it[in].content!=0) {
					remove_item_map(in);
					destroy_item(in);
					continue;
				}
				if (it[in].ID==IID_MISSIONFIGHTER) {
					it[in].sprite=0;
					switch(it[in].drdata[0]) {
						case 1:		sprintf(buf,"%s%s",md->basename,md->strengthname[0]);
								build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3),0,"",buf,md->temp,md->basedesc,1,md->sprite,0,md->char_flags);
								easy++;
								break;
						case 2:		sprintf(buf,"%s%s",md->basename,md->strengthname[1]);
								build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+1,0,"",buf,md->temp,md->basedesc,2,md->sprite,0,md->char_flags);
								normal++;
								break;
						case 3:		sprintf(buf,"%s%s",md->basename,md->strengthname[2]);
								build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+2,0,"",buf,md->temp,md->basedesc,3,md->sprite,0,md->char_flags);
								hard++;
								break;
						case 4:		if (RANDOM(10)) {
									sprintf(buf,"%s",md->bossname);
									build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+3,0,"",buf,md->bosstemp,md->bossdesc,4,md->bosssprite,0,md->char_flags);
								} else {
									sprintf(buf,"%s",md->bigbossname);
									build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+5,0,"",buf,md->bosstemp,md->bigbossdesc,4,md->bosssprite,1,md->char_flags);
								}
								boss++;
								break;
						case 5:		sprintf(buf,"%s%s",md->basename,md->strengthname[1]);
								key1--;
								build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+1,key1==0 ? keyID : 0,"Door Key I",buf,md->temp,md->basedesc,2,md->sprite,0,md->char_flags);
								normal++;
								break;
						case 6:		sprintf(buf,"%s%s",md->basename,md->strengthname[1]);
								key2--;
								build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+1,key2==0 ? keyID+1 : 0,"Door Key II",buf,md->temp,md->basedesc,2,md->sprite,0,md->char_flags);
								normal++;
								break;
						case 7:		sprintf(buf,"%s%s",md->basename,md->strengthname[1]);
								key3--;
								build_fighter(it[in].x,it[in].y,(ppd->sm[idx].difficulty/3)+1,md->itemname && key3==0 ? keyID+2 : 0,"Chest Key",buf,md->temp,md->basedesc,2,md->sprite,0,md->char_flags);
								normal++;
								break;
					}
				}
				if (it[in].ID==IID_MISSIONCHEST && md->itemname) {
					*(unsigned int*)(it[in].drdata+1)=keyID+2;
					chest++;
				}
				if (it[in].ID==IID_MISSIONDOOR1) {
					*(unsigned int*)(it[in].drdata+1)=keyID;
				}
				if (it[in].ID==IID_MISSIONDOOR2) {
					*(unsigned int*)(it[in].drdata+1)=keyID+1;
				}
				if (it[in].ID==IID_MISSIONENTRY) {
					ex=it[in].x;
					ey=it[in].y;
					it[in].sprite=0;
				}				
			}			
		}
	}

	ppd->kill_easy[0]=0; ppd->kill_easy[1]=easy;
	ppd->kill_normal[0]=0; ppd->kill_normal[1]=normal;
	ppd->kill_hard[0]=0; ppd->kill_hard[1]=hard;
	ppd->kill_boss[0]=0; ppd->kill_boss[1]=boss;
	ppd->find_item[0]=0; ppd->find_item[1]=chest;

	mission_status(co,ppd);
	teleport_char_driver(co,ex,ey);
}

int mr_cmp(const void *a,const void *b)
{
	return (((struct reward *)a)->value)-(((struct reward *)b)->value);
}

void mission_reward_list(int cn,int co,struct mission_ppd *ppd)
{
	int n,first,last;

	if (!init_done) {
		qsort(mis_rew,sizeof(mis_rew)/sizeof(mis_rew[0]),sizeof(mis_rew[0]),mr_cmp);
		init_done=1;
	}

	for (n=0; n<sizeof(mis_rew)/sizeof(mis_rew[0])-1; n++) {
		if (mis_rew[n+1].value>=ppd->points) break;		
	}
	last=min(n+3,sizeof(mis_rew)/sizeof(mis_rew[0]));
	first=max(0,last-5);

	log_char(co,LOG_SYSTEM,0,"°c2Code \007Cost \012Description");
	for (n=first; n<last; n++) {
		log_char(co,LOG_SYSTEM,0,"°c4show %s°c0 \010%d \012%s",mis_rew[n].code,mis_rew[n].value,mis_rew[n].desc);
	}
	log_char(co,LOG_SYSTEM,0,"You have: \010%d \012points.",ppd->points);
	log_char(co,LOG_SYSTEM,0,"You'll get a more detailed description of the offer when you choose it.");
}

void remove_mission_items(int cn)
{
	int n,in;

	if ((in=ch[cn].citem) && (it[in].flags&IF_LABITEM)) {
		remove_item_char(in);
		destroy_item(in);
	}

	for (n=30; n<INVENTORYSIZE; n++) {
		if ((in=ch[cn].item[n]) && (it[in].flags&IF_LABITEM)) {
			remove_item_char(in);
			destroy_item(in);
		}
	}
}

void mission_give_reward(int cn,int co,int nr,struct mission_ppd *ppd)
{
	int in;

	if (nr<0 || nr>=sizeof(mis_rew)/sizeof(mis_rew[0])) return;
	if (mis_rew[nr].value>ppd->points) {
		quiet_say(cn,"%s costs %d points, but you only have %d points.",mis_rew[nr].code,mis_rew[nr].value,ppd->points);
		return;
	}
	if (strcmp(mis_rew[nr].itmtmp,"MEXP")==0) {
		if (!get_army_rank_int(co)) {
			quiet_say(cn,"I'm sorry, I can't do that. You are not part of the army.");
			return;
		}
		give_military_pts(cn,co,mis_rew[nr].value/40,1);
		ppd->points-=mis_rew[nr].value;
		dlog(co,0,"took %s from mission offer for %d points (new total %d points)",mis_rew[nr].itmtmp,mis_rew[nr].value,ppd->points);
	} else if (strcmp(mis_rew[nr].itmtmp,"GOLD")==0) {
		give_money(co,(int)(mis_rew[nr].value*5*100),"job reward gold");
		ppd->points-=mis_rew[nr].value;
		dlog(co,0,"took %s from mission offer for %d points (new total %d points)",mis_rew[nr].itmtmp,mis_rew[nr].value,ppd->points);
	} else if (strcmp(mis_rew[nr].itmtmp,"CTPOT")==0) {
                ppd->points-=mis_rew[nr].value;
		ppd->statowed++;
		ppd->statcnt=0;
		ppd->stat[0]=ppd->stat[1]=ppd->stat[2]=0;
		quiet_say(cn,"One custom stat potion coming up. Do you want it to hold °c4one skill°c0 or °c4two skills°c0 or °c4three skills°c0?");
		dlog(co,0,"took %s from mission offer for %d points (new total %d points)",mis_rew[nr].itmtmp,mis_rew[nr].value,ppd->points);
		return;
	} else {
		if (strcmp(mis_rew[nr].itmtmp,"RNORB")==0) in=create_orb();
		else in=create_item(mis_rew[nr].itmtmp);
		if (!in) {
			quiet_say(cn,"Oops. I've ran out of stock. Please choose something else.");
			elog("mission_give_reward: item template %s (%d) not found.",mis_rew[nr].itmtmp,nr);
			return;
		}
                if (it[in].flags&IF_BONDTAKE) {
                        it[in].ownerID=ch[co].ID;
                }
		if (!give_char_item(co,in)) {
			destroy_item(in);
			quiet_say(cn,"Hey, sleepy head, there's no room in your hand or inventory to give you an item!");
			return;
		}
		ppd->points-=mis_rew[nr].value;
		dlog(co,in,"took from mission offer for %d points (new total %d points)",mis_rew[nr].value,ppd->points);
	}
	
	quiet_say(cn,"Here you go, %s, one %s (%s) for %d points. You now have %d points left.",ch[co].name,mis_rew[nr].code,mis_rew[nr].desc,mis_rew[nr].value,ppd->points);
	
}

void mission_show_reward(int cn,int co,int nr,struct mission_ppd *ppd)
{
	int in;

	if (nr<0 || nr>=sizeof(mis_rew)/sizeof(mis_rew[0])) return;

	if (strcmp(mis_rew[nr].itmtmp,"CTPOT")==0) {
		quiet_say(cn,"A custom potion which will enhance one of your stats by 50 or two of your stats by 30 or three of your stats by 20.");
	} else if (strcmp(mis_rew[nr].itmtmp,"RNORB")==0) {
		quiet_say(cn,"A randomly chosen orb, used to enhance one modifier on an item by one.");
	} else if (strcmp(mis_rew[nr].itmtmp,"MEXP")==0) {
		if (mis_rew[nr].value==100) quiet_say(cn,"The equivalent of a two Privates in Military Experience.");
		else if (mis_rew[nr].value==1000) quiet_say(cn,"The equivalent of a Lance Corporal in Military Experience.");
		else if (mis_rew[nr].value==10000) quiet_say(cn,"The equivalent of a Staff Sergeant in Military Experience.");
		//else quiet_say(cn,"The equivalent of a Second Lieutenant in Military Experience."); //100000
	} else if (strcmp(mis_rew[nr].itmtmp,"GOLD")==0) {
		if (mis_rew[nr].value==10) quiet_say(cn,"50 gold coins, fresh from the press.");
		else if (mis_rew[nr].value==100) quiet_say(cn,"500 gold coins, fresh from the press.");
		else if (mis_rew[nr].value==1000) quiet_say(cn,"5000 gold coins, fresh from the press.");
		else if (mis_rew[nr].value==10000) quiet_say(cn,"50000 gold coins, fresh from the press.");
		//else quiet_say(cn,"150000 gold coins, fresh from the press."); //100000
	} else {
		in=create_item(mis_rew[nr].itmtmp);
		if (!in) {
			quiet_say(cn,"Oops. I've run out of stock. Please choose something else.");
			elog("mission_give_reward: item template %s (%d) not found.",mis_rew[nr].itmtmp,nr);
			return;
		}
		look_item(co,it+in);
		destroy_item(in);
	}
	
	quiet_say(cn,"This could be yours for %d points (you have %d points). Say °c4ibuy %s°c0 to buy it.",mis_rew[nr].value,ppd->points,mis_rew[nr].code);	
}


struct mission_giver_data
{
        int last_talk;
        int current_victim;
	int amgivingback;
	int next_spec;
	int spec_cost;
};	

void mission_giver_driver(int cn,int ret,int lastact)
{
	struct mission_giver_data *dat;
	struct mission_ppd *ppd;
        int co,in,didsay=0,talkdir=0,nr,n;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_MISSIONGIVEDRIVER,sizeof(struct mission_giver_data));
	if (!dat) return;	// oops...

	if (ticker>dat->next_spec || !ch[cn].item[30]) {
		int str,base,lvl,cost;

		if ((in=ch[cn].item[30])) {
			remove_item_char(in);
			destroy_item(in);
		}

		lvl=RANDOM(80);
		
		if (lvl<10) { str=3; base=1; }
		else if (lvl<17) { str=4; base=10; }
		else if (lvl<24) { str=6; base=20; }
		else if (lvl<31) { str=7; base=30; }
		else if (lvl<38) { str=8; base=40; }
		else if (lvl<45) { str=10; base=50; }
		else if (lvl<52) { str=12; base=60; }
		else if (lvl<60) { str=14; base=70; }
		else if (lvl<66) { str=16; base=80; }
		else if (lvl<74) { str=18; base=90; }
		else { str=20; base=90; }
		
		in=create_special_item(str,base,1,50);

		lvl=it[in].min_level;
		if (lvl<10) { cost=500;}
		else if (lvl<17) { cost=2000/2;}
		else if (lvl<24) { cost=4000/2;}
		else if (lvl<31) { cost=6000/2;}
		else if (lvl<38) { cost=8000/2;}
		else if (lvl<45) { cost=10000/2;}
		else if (lvl<52) { cost=12000/2;}
		else if (lvl<60) { cost=14000/2;}
		else if (lvl<66) { cost=16000/2;}
		else if (lvl<74) { cost=18000/2; }
		else { cost=20000/2; }

		dat->spec_cost=cost;
		dat->next_spec=ticker+TICKS*60*60*12;
		ch[cn].item[30]=in;
		it[in].carried=cn;
	}

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
			if (ticker<dat->last_talk+TICKS*3) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*5 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_MISSION_PPD,sizeof(struct mission_ppd));

                        if (ppd) {
				if (realtime-ppd->lastseenmissiongiver>30) ppd->missiongive_state=0;
				ppd->lastseenmissiongiver=realtime;

				switch(ppd->missiongive_state) {
					case 0:         if (ppd->solved) {
								int pts;
								struct mission_data *md=mdtab[ppd->md_idx];

								pts=(int)((13+(ppd->sm[ppd->solved-1].difficulty/6))*md->diff_mult);
								ppd->points+=pts;
								ppd->solved=0;
								dlog(co,0,"solved mission, got %d points (new total %d points)",pts,ppd->points);
								ppd->dif_kill=min(MAXDIFF,ppd->dif_kill+2);
								
								ppd->sm[0].type=0; ppd->sm[1].type=0; ppd->sm[2].type=0;

								quiet_say(cn,"Congratulations on doing a good job, %s. You earned %d brownie points in my book, for a total of %d points. Feel free to ask me for an °c4offer°c0 anytime. You can also ask me to °c4increase°c0 or °c4decrease°c0 the difficulty of your jobs (this must be done before asking for a new job, otherwise you'll be changing the difficulty of the following job offer, not the current one; multiple commands will stack). Or do you want a °c4new job°c0?",ch[co].name,pts,ppd->points);
								log_char(co,LOG_SYSTEM,0,"#30");
								log_char(co,LOG_SYSTEM,0,"#40");
								log_char(co,LOG_SYSTEM,0,"#50");
								log_char(co,LOG_SYSTEM,0,"#60");
								log_char(co,LOG_SYSTEM,0,"#70");
								log_char(co,LOG_SYSTEM,0,"#80");
								log_char(co,LOG_SYSTEM,0,"#90");
								remove_mission_items(co);
								ppd->missiongive_state=2;
								didsay=1;
                                                        } else if (ppd->active) {
								quiet_say(cn,"You still have a job. Do you want to °c4fail°c0 it?");
								ppd->missiongive_state=2;
								didsay=1;
							} else {
								quiet_say(cn,"Hello %s. Looking for a job, I bet. Well, let me see...",ch[co].name);
								ppd->missiongive_state++; didsay=1;
							}
                                                        break;
					case 1:		offer_mission(cn,co,ppd);
							ppd->missiongive_state++; didsay=1;
							break;
					case 2:		break; // waiting for player

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
                                ppd=set_data(co,DRD_MISSION_PPD,sizeof(struct mission_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:		ppd->missiongive_state=0; dat->last_talk=0; break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); bzero(ppd,sizeof(struct mission_ppd)); dat->last_talk=0; }
							break;
					case 4:		if (!ppd->active && !ppd->solved) offer_mission_sub(cn,co,0,ppd); else quiet_say(cn,"You still have a job. Do you want to °c4fail°c0 it?"); break;
					case 5:		if (!ppd->active && !ppd->solved) offer_mission_sub(cn,co,1,ppd); else quiet_say(cn,"You still have a job. Do you want to °c4fail°c0 it?"); break;
					case 6:		if (!ppd->active && !ppd->solved) offer_mission_sub(cn,co,2,ppd); else quiet_say(cn,"You still have a job. Do you want to °c4fail°c0 it?"); break;
					case 7:		if (ppd->sm[0].type && !ppd->active && !ppd->solved) start_mission(cn,co,0,ppd); else quiet_say(cn,"I haven't offered you that job yet."); break;
					case 8:		if (ppd->sm[1].type && !ppd->active && !ppd->solved) start_mission(cn,co,1,ppd); else quiet_say(cn,"I haven't offered you that job yet."); break;
					case 9:		if (ppd->sm[2].type && !ppd->active && !ppd->solved) start_mission(cn,co,2,ppd); else quiet_say(cn,"I haven't offered you that job yet."); break;
					case 10:	if (ppd->active) {
								int pts;
								quiet_say(cn,"Don't take on things you cannot handle, kid.");
								pts=ppd->sm[ppd->active-1].difficulty/10;
								ppd->points=max(0,ppd->points-pts);
								ppd->active=0;
								ppd->dif_kill=max(ppd->dif_kill-20,0);
								dlog(co,0,"failed mission, lost %d points (new total %d points)",pts,ppd->points);
								if (pts) {
									quiet_say(cn,"You lost %d brownie points for a new total of %d points.",pts,ppd->points);
								}
								log_char(co,LOG_SYSTEM,0,"#30");
								log_char(co,LOG_SYSTEM,0,"#40");
								log_char(co,LOG_SYSTEM,0,"#50");
								log_char(co,LOG_SYSTEM,0,"#60");
								log_char(co,LOG_SYSTEM,0,"#70");
								log_char(co,LOG_SYSTEM,0,"#80");
								log_char(co,LOG_SYSTEM,0,"#90");
								remove_mission_items(co);
								ppd->sm[0].type=0; ppd->sm[1].type=0; ppd->sm[2].type=0;
							}
							break;
					case 11:	mission_reward_list(cn,co,ppd);
							quiet_say(cn,"I also have a °c4special offer°c0, for %d points.",dat->spec_cost);
                                                        break;
					case 12:	ppd->dif_kill=min(MAXDIFF,ppd->dif_kill+10);
							quiet_say(cn,"Alright, bigmouth. Let's see how you handle this.");
							break;
					case 13:	ppd->dif_kill=max(0,ppd->dif_kill-10);
							quiet_say(cn,"Alright, little girl. You'll get the easy ones now.");
							if (ch[co].flags&CF_FEMALE) {
								quiet_say(cn,"Oops. Sorry. Old habit from my military days, ma'am. Alright, lady, you'll get the easy ones now.");
							}
							break;
					case 14:	if (ch[co].flags&CF_GOD) { say(cn,"I hate freeloaders!"); ppd->points+=100; } break;
					case 15:	if (ch[co].flags&CF_GOD) { say(cn,"I hate freeloaders!"); ppd->points+=1000; } break;
					case 16:	if (ch[co].flags&CF_GOD) { say(cn,"I hate freeloaders!"); ppd->points+=10000; } break;
					case 17:	if (ch[co].flags&CF_GOD) { say(cn,"I hate freeloaders!"); ppd->points+=100000; } break;
					case 18:	look_item(co,it+ch[cn].item[30]);
							log_char(co,LOG_SYSTEM,0,"Price: %d points (you have %d points)",dat->spec_cost,ppd->points);
							log_char(co,LOG_SYSTEM,0,"Do you want to °c4buy the special offer°c0 (offer guaranteed for 5 minutes, unless someone else buys it; might change anytime after that)?");
							dat->next_spec=max(dat->next_spec,ticker+TICKS*60*5);
							break;
					case 19:	if (ppd->points<dat->spec_cost) { quiet_say(cn,"Sorry, you can't afford it."); break; }
							in=ch[cn].item[30];
							if (!give_char_item(co,in)) { quiet_say(cn,"You don't have any space in your inventory, dude."); break; }
							ch[cn].item[30]=0;
							ppd->points-=dat->spec_cost;
							dat->next_spec=0;
							dlog(co,in,"took from mission offer for %d points (new total %d points)",dat->spec_cost,ppd->points);
							break;
					case 20:	if (ppd->statowed<1) { quiet_say(cn,"You did not buy a stat potion."); break; }
							ppd->statcnt=1;
							quiet_say(cn,"Alright, a one-stat potion it will be. Please name the skill you'd like to have (ie. °c4attack skill°c0, °c4immunity skill°c0 etc.).");
							break;
					case 21:	if (ppd->statowed<1) { quiet_say(cn,"You did not buy a stat potion."); break; }
							ppd->statcnt=2;
							quiet_say(cn,"Alright, a two-stat potion it will be. Please name the skills you'd like to have (ie. °c4attack skill°c0, °c4immunity skill°c0 etc.), one skill per line.");
							break;
					case 22:	if (ppd->statowed<1) { quiet_say(cn,"You did not buy a stat potion."); break; }
							ppd->statcnt=3;
							quiet_say(cn,"Alright, a three-stat potion it will be. Please name the skills you'd like to have (ie. °c4attack skill°c0, °c4immunity skill°c0 etc.), one skill per line.");
							break;
					default:	if (didsay>=2000) mission_give_reward(cn,co,didsay-2000,ppd);
							else if (didsay>=1000) mission_show_reward(cn,co,didsay-1000,ppd);
							break;
				}
				if (!didsay && ppd->statowed && (nr=find_skill_text(cn,msg->dat1,(char*)msg->dat2,co)) && nr>0 && nr<V_MAX) {
					didsay=1;
					//quiet_say(cn,"nr=%d",nr);
					for (n=0; n<2; n++) {
						if (ppd->stat[n]) continue;
						break;
					}
					ppd->stat[n]=nr; n++;
					if (n>=ppd->statcnt) {
                                                in=create_item("mis_potionbase");
						if (in) {
							it[in].mod_index[0]=ppd->stat[0];
							it[in].mod_index[1]=ppd->stat[1];
							it[in].mod_index[2]=ppd->stat[2];
							if (ppd->statcnt==1) { it[in].mod_value[0]=50; it[in].mod_value[1]=0; it[in].mod_value[2]=0; }
							if (ppd->statcnt==2) { it[in].mod_value[0]=30; it[in].mod_value[1]=30; it[in].mod_value[2]=0; }
							if (ppd->statcnt==3) { it[in].mod_value[0]=20; it[in].mod_value[1]=20; it[in].mod_value[2]=20; }
							if (give_char_item(co,in)) {
								quiet_say(cn,"Very well, %s, here you go.",ch[co].name);
								ppd->statowed=0;
							} else {
								quiet_say(cn,"please try again");
								ppd->stat[0]=ppd->stat[1]=ppd->stat[2]=0;
								destroy_item(in);
							}
						} else {
							quiet_say(cn,"please try again");
							ppd->stat[0]=ppd->stat[1]=ppd->stat[2]=0;
						}
					} else quiet_say(cn,"Very well, the %s skill will be %s.",n==1 ? "first" : "second",skill[nr].name);
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
				if (!dat->amgivingback) {					
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
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
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

void missionchest_driver(int in,int cn)
{
	int n,in2;
	struct mission_ppd *ppd;
	struct mission_data *md;
	
        if (!cn) return;
	
	ppd=set_data(cn,DRD_MISSION_PPD,sizeof(struct mission_ppd));
	if (!ppd) return;
	
	md=mdtab[ppd->md_idx];

	if (!md->itemtemp) {
		log_char(cn,LOG_SYSTEM,0,"The chest is empty.");
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
		log_char(cn,LOG_SYSTEM,0,"You use %s to unlock the chest.",it[in2].name);
	}

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your 'hand' (mouse cursor) first.");
		return;
	}
	
	in2=create_item(md->itemtemp);
	strcpy(it[in2].name,md->itemname);
	strcpy(it[in2].description,md->itemdesc);

	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;

	ppd->find_item[0]=1;
	mission_status(cn,ppd);

	log_char(cn,LOG_SYSTEM,0,"You got a %s.",it[in2].name);

	mission_done(cn,ppd);
}

void mission_fighter_driver(int cn,int ret,int lastact)
{
        char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}


void mission_fighter_dead(int cn,int co)
{
	struct mission_ppd *ppd;
	int nr;

	nr=ch[cn].deaths;

	ppd=set_data(co,DRD_MISSION_PPD,sizeof(struct mission_ppd));
	if (!ppd) return;

	switch(nr) {
		case 1:		ppd->kill_easy[0]=min(ppd->kill_easy[0]+1,ppd->kill_easy[1]); break;
		case 2:		ppd->kill_normal[0]=min(ppd->kill_normal[0]+1,ppd->kill_normal[1]); break;
		case 3:		ppd->kill_hard[0]=min(ppd->kill_hard[0]+1,ppd->kill_hard[1]); break;
		case 4:		ppd->kill_boss[0]=min(ppd->kill_boss[0]+1,ppd->kill_boss[1]); break;
	}

	mission_status(co,ppd);
	mission_done(co,ppd);
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_MISSIONGIVE:	mission_giver_driver(cn,ret,lastact); return 1;
		case CDR_MISSIONFIGHT:	mission_fighter_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_MISSIONCHEST:	missionchest_driver(in,cn); return 1;
                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		default:		return 0;
		case CDR_MISSIONFIGHT:	mission_fighter_dead(cn,co); return 1;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:		return 0;
	}
}







