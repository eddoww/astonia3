/*
 * $Id: alchemy.c,v 1.7 2007/10/29 13:56:52 devel Exp $
 *
 * $Log: alchemy.c,v $
 * Revision 1.7  2007/10/29 13:56:52  devel
 * changed min levels for shrooms
 *
 * Revision 1.6  2007/10/07 11:18:05  devel
 * added min level to shrooms
 *
 * Revision 1.5  2007/07/11 12:37:14  devel
 * added some debug outputs
 *
 * Revision 1.4  2007/07/04 09:19:17  devel
 * added /support support
 *
 * Revision 1.3  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.2  2006/04/26 16:05:44  ssim
 * added hack for teufelheim arena: no potions allowed
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
#include "los.h"
#include "skill.h"
#include "item_id.h"
#include "libload.h"
#include "task.h"
#include "consistency.h"
#include "flower_ppd.h"
#include "prof.h"

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

// Ingredients:
// -----------
//
// Influences:
// 01: flower A
// 02: flower B
// 03: flower C
// 04: flower D
// 05: flower E
// 06: flower F
// 07: flower G

// Powers:
// 08: mushroom A
// 09: mushroom B
// 10: mushroom C
// 11: mushroom D
// 12: mushroom E
// 13: mushroom F
// 14: mushroom G
// 15: mushroom H
// 16: mushroom I

// Durations:
// 17: berry A
// 18: berry B
// 19: berry C
// 20: berry D

// Special:
// 21: fiery stone
// 22: icy stone

// Power
// -----

// PW1		6
// PW2		8
// PW3		12
// PW4		16
// PW5		20
// PW6		24
// PW7		28
// PW8		32
// PW9		36

// PW1 PW2	10...16
// PW2 PW3	
// PW3 PW4	
// PW4 PW5	
// PW5 PW6	
// PW6 PW7	
// PW7 PW8	
// PW8 PW9	40...72

// +4 per fiery stone
// +8 per icy stone

// all other combinations take power from highest single ingredient

// Duration

// D10		10 min		div 1
// D20		20 min		div 1.25
// D30		30 min		div 1.5
// D60		60 min		div 2

// Influences:

// (power) (duration) ...

// Wisdom			WIS	div 4
// Intuition			INT	div 4
// Agility			AGI	div 4
// Strength			STR	div 4

// Hitpoints			LFE	div 2
// Mana				SPR	div 2
// Endurance			END	div 2

// Pulse			STR STR END	div 2
// Dagger			AGI AGI END	div 2
// Hand to Hand			AGI STR LFE	div 2
// Staff			AGI STR END	div 2
// Sword			AGI AGI STR	div 2
// Two-Handed			AGI STR STR	div 2
// Attack			INT AGI STR	div 2
// Parry			WIS AGI STR	div 2
// Warcry			INT STR END	div 2
// Tactics			WIS INT AGI	div 2
// Surround Hit			INT AGI AGI	div 2
// Body Control			AGI AGI LFE	div 2
// Speed			AGI AGI END	div 2
// Bartering			WIS INT INT	div 2
// Perception			INT INT END	div 2
// Stealth			INT INT AGI	div 2
// Bless			WIS INT SPR	div 2
// Heal				WIS INT LFE 	div 2
// Freeze			AGI END SPR	div 2
// Magic Shield			LFE LFE SPR	div 2
// Flash			INT LFE SPR	div 2
// Fire				STR LFE SPR	div 2
// Ball				WIS LFE SPR	div 2
// Immunity			STR STR SPR	div 2
// Duration			LFE SPR SPR 	div 2
// Rage				STR STR LFE	div 2

// Attack Parry			WIS INT AGI STR		div 3
// Flash Immunity		INT STR LFE SPR		div 3
// Fire Immunity		STR STR LFE SPR		div 3
// Magic-Shield Immunity	STR LFE LFE SPR		div 3
// Dagger Flash			AGI END LFE SPR		div 3
// Dagger Fire			AGI STR END SPR		div 3
// Staff Flash 			AGI STR LFE SPR		div 3
// Staff Fire 			AGI STR STR SPR		div 3

// Sword Attack Parry		WIS INT AGI AGI STR	div 4
// Two-Handed Attack Parry	WIS INT AGI STR STR	div 4
// Attack Parry Immunity	AGI STR STR LFE SPR	div 4
// Flash Magic-Shield Immunity	INT STR LFE LFE SPR	div 4
// Fire Magic-Shield Immunity	STR STR LFE LFE SPR	div 4
// Flash Magic-Shield Pulse	INT LFE LFE SPR END	div 4
// Fire Magic-Shield Pulse	STR LFE LFE SPR END	div 4
// Flash Immunity Pulse		INT STR STR SPR END	div 4
// Fire Immunity Pulse		STR STR STR SPR END	div 4

// all non-fitting ones influence basic attributes relative to amounts in potion
// ie INT AGI AGI = INT+1, AGI+2

// max for normal potions:

// power: 72
// base attrib: 18
// HP/Mana: 36
// skill: 36 (reduced to 25)
// 2 skills: 24
// 3 skills: 18

static char *ing_name[]={
	"Adygalah",
	"Bhalkissa",
	"Chrysado",
	"Domari",
	"Elithah",
	"Firuba",
	"Ghethiye",
        "Akond",
	"Barun",
	"Chylmoth",
	"Dizul",
	"Edyak",
	"Forud",
	"Ghestroz",
	"Hangot",
	"Ivnan",
	"Azmey",
	"Beelough",
	"Ciuba",
	"Dyelshi",
	"Fiery Stone",
	"Icy Stone",
	"Earth Stone",
	"Hell Stone"};

int mixer_power(int in,int cn)
{
	int cnt,pw1,pw2,pw3,pw4,pw5,pw6,pw7,pw8,pw9,stone;
	int good=0,bad=0;

	pw1=it[in].drdata[18];
	pw2=it[in].drdata[19];
	pw3=it[in].drdata[20];
	pw4=it[in].drdata[21];
	pw5=it[in].drdata[22];
	pw6=it[in].drdata[23];
	pw7=it[in].drdata[24];
	pw8=it[in].drdata[25];
	pw9=it[in].drdata[26];

	cnt=pw1+pw2+pw3+pw4+pw5+pw6+pw7+pw8+pw9;

	stone=it[in].drdata[31]+it[in].drdata[32]+it[in].drdata[33]+it[in].drdata[34];

	if (cnt==2 && pw1==1 && pw2==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 16;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 12;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 10;
	}
	if (cnt==2 && pw2==1 && pw3==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 24;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 20;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 16;		
	}
	if (cnt==2 && pw3==1 && pw4==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 32;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 26;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 20;
	}
	if (cnt==2 && pw4==1 && pw5==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 40;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 32;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 24;
	}
	if (cnt==2 && pw5==1 && pw6==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 48;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 38;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 28;
	}
	if (cnt==2 && pw6==1 && pw7==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 56;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 44;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 32;
	}
	if (cnt==2 && pw7==1 && pw8==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 64;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 50;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 36;
	}
	if (cnt==2 && pw8==1 && pw9==1) {
		if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) return 72;
		if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) return 56;
		if (fullmoon || stone || get_support_prof(cn,P_ALCHEMIST)>=10) return 40;
	}

	if (solstice || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=25)) good=8;
	else if (equinox || (fullmoon && get_support_prof(cn,P_ALCHEMIST)>=15)) good=4;
	else if (fullmoon || get_support_prof(cn,P_ALCHEMIST)>=10) good=2;
	else if (hour==12) good=1;

	if (hour==0) bad=1;
        if (newmoon) bad=2;

	if (pw9) return 36+good-bad;
	if (pw8) return 32+good-bad;
	if (pw7) return 28+good-bad;
	if (pw6) return 24+good-bad;
	if (pw5) return 20+good-bad;
	if (pw4) return 16+good-bad;
	if (pw3) return 12+good-bad;
	if (pw2) return max(2,8+good-bad);
	if (pw1) return max(2,6+good-bad);
	
	return -1;
}

int mixer_duration(int in,double *divi)
{
	int cnt,d60,d10,d20,d30;

	d60=it[in].drdata[27];
	d10=it[in].drdata[28];
	d20=it[in].drdata[29];
	d30=it[in].drdata[30];

	cnt=d60+d10+d20+d30;

	if (d60) { *divi=1.75; return 60; }
        if (d30) { *divi=1.5; return 30; }
	if (d20) { *divi=1.25; return 20; }
	if (d10) { *divi=1.0; return 10; }

	return -1;
}

int mixer_mix(int cn,int in,int power,int duration,double divi)
{
	int cnt,wis,inu,agi,str,lfe,spr,end,fire,ice,hell;
	int skl[3],val[3];
	int n,value,flag=0;

	if (!duration || duration==-1 || !power || power==-1 || !divi) {
		return 0;
	}

	wis=it[in].drdata[11];
	inu=it[in].drdata[12];
	agi=it[in].drdata[13];
	str=it[in].drdata[14];
	lfe=it[in].drdata[15];
	spr=it[in].drdata[16];
	end=it[in].drdata[17];

        cnt=wis+inu+agi+str+lfe+spr+end;

	fire=it[in].drdata[31];
	ice=it[in].drdata[32];
	hell=it[in].drdata[34];

	power+=fire*4;
	power+=ice*8;
	power+=hell*12;
	
	if (get_support_prof(cn,P_ALCHEMIST)>=5) power+=2;
	if (get_support_prof(cn,P_ALCHEMIST)>=10) power+=2;
	if (get_support_prof(cn,P_ALCHEMIST)>=15) power+=4;
	if (get_support_prof(cn,P_ALCHEMIST)>=25) power+=4;

	log_char(cn,LOG_SYSTEM,0,"Final Power: %d",power);

        if (cnt==5 && wis==1 && inu==1 && agi==2 && str==1) {		// sword attack parry
		skl[0]=V_SWORD; if ((val[0]=power/divi/4)) flag=1;
		skl[1]=V_ATTACK; val[1]=power/divi/4;
		skl[2]=V_PARRY; val[2]=power/divi/4;
		value=10;
	} else if (cnt==5 && wis==1 && inu==1 && agi==1 && str==2) {	// two-handed attack parry
		skl[0]=V_TWOHAND; if ((val[0]=power/divi/4)) flag=1;
		skl[1]=V_ATTACK; val[1]=power/divi/4;
		skl[2]=V_PARRY; val[2]=power/divi/4;
		value=10;
	} else if (cnt==5 && agi==1 && str==2 && lfe==1 && spr==1) {	// attack parry immunity
		skl[0]=V_ATTACK; if ((val[0]=power/divi/4)) flag=1;
		skl[1]=V_PARRY; val[1]=power/divi/4;
		skl[2]=V_IMMUNITY; val[2]=power/divi/4;
		value=10;
	} else if (cnt==5 && inu==1 && str==1 && lfe==2 && spr==1) {	// flash magic-shield immunity
		skl[0]=V_FLASH; if ((val[0]=power/divi/4)) flag=1;
		skl[1]=V_MAGICSHIELD; val[1]=power/divi/4;
		skl[2]=V_IMMUNITY; val[2]=power/divi/4;
		value=10;
	} else if (cnt==5 && str==2 && lfe==2 && spr==1) {		// fire magic-shield immunity
		skl[0]=V_FIRE; if ((val[0]=power/divi/4)) flag=1;
		skl[1]=V_MAGICSHIELD; val[1]=power/divi/4;
		skl[2]=V_IMMUNITY; val[2]=power/divi/4;
		value=10;
	} else if (cnt==4 && wis==1 && inu==1 && agi==1 && str==1) {	// attack parry
		skl[0]=V_ATTACK; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_PARRY; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==4 && inu==1 && str==1 && lfe==1 && spr==1) {	// flash immunity
		skl[0]=V_FLASH; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_IMMUNITY; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==4 && str==2 && lfe==1 && spr==1) {	// fire immunity
		skl[0]=V_FIRE; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_IMMUNITY; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==4 && str==1 && lfe==2 && spr==1) {		// magic shield immunity
		skl[0]=V_MAGICSHIELD; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_IMMUNITY; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=10;
	} else if (cnt==4 && agi==1 && end==1 && lfe==1 && spr==1) {	// dagger flash
		skl[0]=V_DAGGER; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_FLASH; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==4 && agi==1 && str==1 && end==1 && spr==1) {	// dagger fire
		skl[0]=V_DAGGER; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_FIRE; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==4 && agi==1 && str==1 && lfe==1 && spr==1) {	// staff flash
		skl[0]=V_STAFF; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_FLASH; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==4 && agi==1 && str==2 && spr==1) {		// staff fire
		skl[0]=V_STAFF; if ((val[0]=power/divi/3)) flag=1;
		skl[1]=V_FIRE; val[1]=power/divi/3;
		skl[2]=-1; val[2]=0;
		value=8;
	} else if (cnt==3 && agi==2 && end==1) {			// dagger
		skl[0]=V_DAGGER; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && agi==1 && str==1 && end==1) {		// staff
		skl[0]=V_STAFF; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && agi==2 && str==1) {			// sword
		skl[0]=V_SWORD; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && agi==1 && str==2) {			// two-handed
		skl[0]=V_TWOHAND; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && inu==1 && agi==1 && str==1) {		// attack
		skl[0]=V_ATTACK; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && wis==1 && agi==1 && str==1) {		// parry
		skl[0]=V_PARRY; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && inu==2 && end==1) {			// perception
		skl[0]=V_PERCEPT; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && inu==2 && agi==1) {			// stealth
		skl[0]=V_STEALTH; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && agi==2 && end==1) {			// speed
		skl[0]=V_SPEED; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && agi==1 && end==1 && spr==1) {		// freeze
		skl[0]=V_FREEZE; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && lfe==2 && spr==1) {			// magic shield
		skl[0]=V_MAGICSHIELD; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && inu==1 && lfe==1 && spr==1) {		// flash
		skl[0]=V_FLASH; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && str==1 && lfe==1 && spr==1) {		// fireball
		skl[0]=V_FIRE; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} /*else if (cnt==3 && wis==1 && lfe==1 && spr==1) {		// ball
		skl[0]=V_BALL; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} */else if (cnt==3 && str==2 && spr==1) {		// immunity
		skl[0]=V_IMMUNITY; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && agi==1 && str==1 && lfe==1) {	// hand to hand
		skl[0]=V_HAND; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && inu==1 && str==1 && end==1) {	// warcry
		skl[0]=V_WARCRY; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && wis==1 && inu==1 && agi==1) {	// tactics
		skl[0]=V_TACTICS; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && inu==1 && agi==2) {	// surround hit
		skl[0]=V_SURROUND; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && wis==1 && inu==2) {	// bartering
		skl[0]=V_BARTER; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && wis==1 && inu==1 && spr==1) {	// bless
		skl[0]=V_BLESS; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && wis==1 && inu==1 && lfe==1) {	// heal
		skl[0]=V_HEAL; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && lfe==1 && spr==2) {	// duration
		skl[0]=V_DURATION; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt==3 && str==2 && lfe==1) {	// rage
		skl[0]=V_RAGE; if ((val[0]=power/divi/2)) flag=1;
		skl[1]=-1; val[1]=0;
		skl[2]=-1; val[2]=0;
		value=3;
	} else if (cnt) {
		for (n=0; n<3; n++) {
			if (wis) { skl[n]=V_WIS; if ((val[n]=power*wis/divi/cnt/4)) flag=1; wis=0; continue; }
			if (inu) { skl[n]=V_INT; if ((val[n]=power*inu/divi/cnt/4)) flag=1; inu=0; continue; }
			if (agi) { skl[n]=V_AGI; if ((val[n]=power*agi/divi/cnt/4)) flag=1; agi=0; continue; }
			if (str) { skl[n]=V_STR; if ((val[n]=power*str/divi/cnt/4)) flag=1; str=0; continue; }
			if (lfe) { skl[n]=V_HP; if ((val[n]=power*lfe/divi/cnt/2)) flag=1; lfe=0; continue; }
			if (spr) { skl[n]=V_MANA; if ((val[n]=power*spr/divi/cnt/2)) flag=1; spr=0; continue; }
			if (end) { skl[n]=V_ENDURANCE; if ((val[n]=power*end/divi/cnt/2)) flag=1; end=0; continue; }
			skl[n]=-1; val[n]=0;
		}
		value=1;
	} else {
		// nothing useful
		return 0;
	}

        if (!flag) return 0;
	
        it[in].mod_index[0]=skl[0];
        it[in].mod_value[0]=val[0];
	it[in].mod_index[1]=skl[1];
        it[in].mod_value[1]=val[1];
	it[in].mod_index[2]=skl[2];
        it[in].mod_value[2]=val[2];

	it[in].drdata[3]=duration;

	it[in].value=value*power*13+50;

	if (fire || ice || hell) it[in].needs_class=8;
	else it[in].needs_class=0;

        return 1;
}

int mixer(int cn,int in)
{
        int power=0,duration=0;
	double divi;
	int n;

        for (n=11; n<40; n++) {
		if (it[in].drdata[n]) {
			log_char(cn,LOG_SYSTEM,0,"Contains %d parts %s.",it[in].drdata[n],ing_name[n-11]);
		}
	}

	power=mixer_power(in,cn);
	duration=mixer_duration(in,&divi);

	log_char(cn,LOG_SYSTEM,0,"Power=%d, Duration=%d, Divisor=%f",power,duration,divi);
	
	return mixer_mix(cn,in,power,duration,divi);
}

int mixer_use(int cn,int in)
{
	int fre,in2,endtime,duration;

	if (!check_requirements(cn,in)) {
		log_char(cn,LOG_SYSTEM,0,"You do not meet the requirements needed to use this potion.");
		return 0;
	}

        if (!(fre=may_add_spell(cn,IDR_POTION_SP))) {
		log_char(cn,LOG_SYSTEM,0,"Another potion is still active.");
		return 0;
	}

	in2=create_item("potion_spell");
	if (!in2) return 0;

        it[in2].mod_index[0]=it[in].mod_index[0];
	it[in2].mod_value[0]=it[in].mod_value[0];
	it[in2].mod_index[1]=it[in].mod_index[1];
	it[in2].mod_value[1]=it[in].mod_value[1];
	it[in2].mod_index[2]=it[in].mod_index[2];
	it[in2].mod_value[2]=it[in].mod_value[2];	
	it[in2].driver=IDR_POTION_SP;

	duration=it[in].drdata[3]*60*TICKS;

	endtime=ticker+duration;

	*(signed long*)(it[in2].drdata)=endtime;
	*(signed long*)(it[in2].drdata+4)=ticker;

	it[in2].carried=cn;

        ch[cn].item[fre]=in2;

	create_spell_timer(cn,in2,fre);

	update_char(cn);

	return 1;
}

void flask_driver(int in,int cn)
{
	int in2;
	int size;
	int used;
	int type;
	int shaken;

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it
	if (!it[in].carried) return;

        size=it[in].drdata[0];
	used=it[in].drdata[1];
	shaken=it[in].drdata[2];
	// duration = drdata 3

        if (shaken && ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"This potion is finished. You cannot add more ingredients.");
		return;
	}

	if (!(in2=ch[cn].citem)) {
		if (shaken) {	// potion is ready
			if (!mixer_use(cn,in)) return;
			strcpy(it[in].name,"Empty Potion");
			switch(size) {
				case 1:	it[in].sprite=10290; strcpy(it[in].description,"A small flask made of glass."); break;
				case 2:	it[in].sprite=10294; strcpy(it[in].description,"A flask made of glass.");break;
				case 3:	it[in].sprite=10302; strcpy(it[in].description,"A big flask made of glass."); break;
				default:	elog("umpsi dumpsi (%s)",it[in].name); break;
			}
			bzero(it[in].drdata,sizeof(it[in].drdata));
			bzero(it[in].mod_index,sizeof(it[in].mod_index));
			bzero(it[in].mod_value,sizeof(it[in].mod_value));
			it[in].value=10;
			it[in].drdata[0]=size;
			it[in].needs_class=0;
			ch[cn].flags|=CF_ITEMS;
			return;
		}
		if (it[in].drdata[1]==0) {	// shaking empty potion
			log_char(cn,LOG_SYSTEM,0,"You shake the empty bottle, but nothing happens.");
			return;
		}

		if (mixer(cn,in)) {
			log_char(cn,LOG_SYSTEM,0,"You shake the bottle and create a magical potion.");

			switch(it[in].drdata[0]) {
				case 1:		it[in].sprite=50213; strcpy(it[in].description,"A small flask containing a magical liquid."); break;
				case 2:		it[in].sprite=50214; strcpy(it[in].description,"A flask containing a magical liquid."); break;
				case 3:		it[in].sprite=50253; strcpy(it[in].description,"A big flask containing a magical liquid."); break;
	
				default:	elog("humba dumba (%s)",it[in].name); break;
			}
			strcpy(it[in].name,"Magical Potion");

			it[in].drdata[2]=1;	// shaken=yes
		} else {
			log_char(cn,LOG_SYSTEM,0,"You shake the bottle and create a stinking liquid which you throw away.");
			strcpy(it[in].name,"Empty Potion");
			switch(size) {
				case 1:	it[in].sprite=10290; strcpy(it[in].description,"A small flask made of glass."); break;
				case 2:	it[in].sprite=10294; strcpy(it[in].description,"A flask made of glass.");break;
				case 3:	it[in].sprite=10302; strcpy(it[in].description,"A big flask made of glass."); break;
				default:	elog("umpsi dumpsi (%s)",it[in].name); break;
			}
			bzero(it[in].drdata,sizeof(it[in].drdata));
			bzero(it[in].mod_index,sizeof(it[in].mod_index));
			bzero(it[in].mod_value,sizeof(it[in].mod_value));
			it[in].drdata[0]=size;
			it[in].needs_class=0;
			it[in].value=10;
		}

		ch[cn].flags|=CF_ITEMS;

		return;
	}

	type=it[in2].drdata[0];

	if (it[in2].ID!=IID_ALCHEMY_INGREDIENT) {
		log_char(cn,LOG_SYSTEM,0,"That's not an ingredient you can use in a flask.");
		return;
	}

        if (used>=(size*3)) {
		log_char(cn,LOG_SYSTEM,0,"The Flask is full.");
		return;
	}

	strcpy(it[in].name,"Unfinished Potion");
        switch(size) {
		case 1:		it[in].sprite=50204+used; strcpy(it[in].description,"A small flask containing some strange liquid."); break;
		case 2:		it[in].sprite=50207+used; strcpy(it[in].description,"A flask containing some strange liquid."); break;
		case 3:		it[in].sprite=50243+used; strcpy(it[in].description,"A big flask containing some strange liquid."); break;
		default:	elog("uhga ugha (%s)",it[in].name); break;
	}

	it[in].drdata[1]++;

	if (type<1 || type>29) {
		elog("duda duda (%s)",it[in2].name);
		log_char(cn,LOG_SYSTEM,0,"BUG # 231...");
		return;
	}
	it[in].drdata[type+10]++;

	log_char(cn,LOG_SYSTEM,0,"You put %s into the flask.",it[in2].name);

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"alchemy: put into flask");
	
	ch[cn].citem=0;
	ch[cn].flags|=CF_ITEMS;

	destroy_item(in2);
}

void flower_driver(int in,int cn)
{
	int ID,n,old_n=0,old_val=0,in2,ripetime,minlevel;
	struct flower_ppd *ppd;

        switch(it[in].drdata[0]) {
		case 8:		minlevel=1; break; //in2=create_item("alc_mushroom1"); break;
		case 9:		minlevel=1; break; //in2=create_item("alc_mushroom2"); break;
		case 10:	minlevel=3; break; //in2=create_item("alc_mushroom3"); break;
		case 11:	minlevel=9; break; //in2=create_item("alc_mushroom4"); break;
		case 12:	minlevel=11; break; //in2=create_item("alc_mushroom5"); break;
		case 13:	minlevel=14; break; //in2=create_item("alc_mushroom6"); break;
		case 14:	minlevel=17; break; //in2=create_item("alc_mushroom7"); break;
		case 15:	minlevel=21; break; //in2=create_item("alc_mushroom8"); break;
		case 16:	minlevel=24; break; //in2=create_item("alc_mushroom9"); break;

		default:	minlevel=0; break;
	}

	if (!cn) {
		it[in].min_level=minlevel;
		return;
	}

	if (ch[cn].level<minlevel) {
		log_char(cn,LOG_SYSTEM,0,"This mushroom has a minimum level of %d. Sorry.",minlevel);
		return;
	}
	
	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	ppd=set_data(cn,DRD_FLOWER_PPD,sizeof(struct flower_ppd));
	if (!ppd) return;	// oops...

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXFLOWER; n++) {
		if (ppd->ID[n]==ID) break;
                if (realtime-ppd->last_used[n]>old_val) {
			old_val=realtime-ppd->last_used[n];
			old_n=n;
		}
	}

	ripetime=60*60*24;

	if (n==MAXFLOWER) n=old_n;
	else if (realtime-ppd->last_used[n]<ripetime) {
		log_char(cn,LOG_SYSTEM,0,"It's not ripe yet.");
		return;
	}

	ppd->ID[n]=ID;
	ppd->last_used[n]=realtime;

	switch(it[in].drdata[0]) {
		case 1:		in2=create_item("alc_flower1"); break;
		case 2:		in2=create_item("alc_flower2"); break;
		case 3:		in2=create_item("alc_flower3"); break;
		case 4:		in2=create_item("alc_flower4"); break;
		case 5:		in2=create_item("alc_flower5"); break;
		case 6:		in2=create_item("alc_flower6"); break;
		case 7:		in2=create_item("alc_flower7"); break;
		case 8:		in2=create_item("alc_mushroom1"); break;
		case 9:		in2=create_item("alc_mushroom2"); break;
		case 10:	in2=create_item("alc_mushroom3"); break;
		case 11:	in2=create_item("alc_mushroom4"); break;
		case 12:	in2=create_item("alc_mushroom5"); break;
		case 13:	in2=create_item("alc_mushroom6"); break;
		case 14:	in2=create_item("alc_mushroom7"); break;
		case 15:	in2=create_item("alc_mushroom8"); break;
		case 16:	in2=create_item("alc_mushroom9"); break;
		case 17:	in2=create_item("alc_berry1"); break;
		case 18:	in2=create_item("alc_berry2"); break;
		case 19:	in2=create_item("alc_berry3"); break;
		case 20:	in2=create_item("alc_berry4"); break;

		default:	log_char(cn,LOG_SYSTEM,0,"Bug # 4111"); return;
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"alchemy: picked");

	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_FLASK:		flask_driver(in,cn); return 1;
		case IDR_FLOWER:	flower_driver(in,cn); return 1;

		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:	return 0;
	}
}



