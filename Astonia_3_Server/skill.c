/*

$Id: skill.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: skill.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2003/10/27 11:38:46  sam
added armorskill to supermax

Revision 1.2  2003/10/13 14:12:47  sam
Added RCS tags


*/

#include "server.h"
#include "create.h"
#include "database.h"
#include "log.h"
#include "tool.h"
#include "skill.h"

struct skill skill[V_MAX]={
				//  Bases          Cost W M (0=not raisable, 1=skill, 2=attribute, 3=power)
	// Powers
	{"Hitpoints",		   -1,   -1,   -1, 	3,	10},	// 0		done
	{"Endurance",		   -1,   -1,   -1, 	3,	10},	// 1		done
	{"Mana",		   -1,   -1,   -1, 	3,	10},	// 2		done

	// Attributes
	{"Wisdom",		   -1,   -1,   -1, 	2,	10},	// 3		done
	{"Intuition",		   -1,   -1,   -1, 	2,	10},	// 4		done
	{"Agility",		   -1,   -1,   -1, 	2,	10},	// 5		done
	{"Strength",		   -1,   -1,   -1, 	2,	10},	// 6		done

	// Values
	{"Armor",		   -1,   -1,   -1, 	0,	 0},	// 7		done
	{"Weapon",		   -1,   -1,   -1, 	0,	 0},	// 8		done
	{"Light",		   -1,   -1,   -1, 	0,	 0},	// 9		done
	{"Speed",     		V_AGI,V_AGI,V_STR, 	0,	 0},	// 10		done

	{"Pulse",	  	V_INT,V_INT,V_WIS, 	1,	 1},	// 11		done

        // Primary Fighting Skills	
	{"Dagger",		V_INT,V_AGI,V_STR, 	1,	 1},	// 12		done
	{"Hand to Hand",	V_AGI,V_STR,V_STR, 	1,	 1},	// 13		done
        {"Staff",		V_INT,V_AGI,V_STR, 	1,	 1},	// 14		done
	{"Sword",		V_INT,V_AGI,V_STR, 	1,	 1},	// 15		done
	{"Two-Handed",		V_AGI,V_STR,V_STR, 	1,	 1},	// 16		done

	// Secondary Fighting Skills
	{"Armor Skill",		V_AGI,V_AGI,V_STR, 	1,	 1},	// 17		done
	{"Attack",		V_INT,V_AGI,V_STR, 	1,	 1},	// 18		done
	{"Parry",		V_INT,V_AGI,V_STR, 	1,	 1},	// 19		done
	{"Warcry",		V_INT,V_AGI,V_STR, 	1,	 1},	// 20		done
	{"Tactics",		V_INT,V_AGI,V_STR, 	1,	 1},	// 21		done
	{"Surround Hit",	V_INT,V_AGI,V_STR, 	1,	 1},	// 22		done
	{"Body Control",	V_INT,V_AGI,V_STR, 	1,	 1},	// 23		done
	{"Speed Skill",		V_INT,V_AGI,V_STR, 	1,	 1},	// 24		done

	// Misc. Skills
        {"Bartering",		V_INT,V_INT,V_WIS, 	1,	 1},	// 25		done
        {"Perception",		V_INT,V_INT,V_WIS, 	1,	 1},	// 26		done
	{"Stealth",		V_INT,V_AGI,V_AGI, 	1,	 1},	// 27		done

	// Spells
	{"Bless",		V_INT,V_INT,V_WIS, 	1,	 1},	// 28		done
	{"Heal",		V_INT,V_INT,V_WIS, 	1,	 1},	// 29		done
	{"Freeze",		V_INT,V_INT,V_WIS, 	1,	 1},	// 30		done
	{"Magic Shield",	V_INT,V_INT,V_WIS, 	1,	 1},	// 31		done
	{"Lightning",		V_INT,V_INT,V_WIS, 	1,	 1},	// 32		done
	{"Fire",		V_INT,V_INT,V_WIS, 	1,	 1},	// 33		done
	{"empty",		V_INT,V_INT,V_WIS, 	1,	 1},	// 34		done

	{"Regenerate",		V_STR,V_STR,V_STR, 	1,	 1},	// 35		done
	{"Meditate",		V_WIS,V_WIS,V_WIS, 	1,	 1},	// 36		done
	{"Immunity",		V_INT,V_WIS,V_STR, 	1,	 1},	// 37		done

	{"Ancient Knowledge",	V_INT,V_WIS,V_STR, 	0,	 1},	// 38		done
	{"Duration",		V_INT,V_WIS,V_STR, 	1,	 1},	// 39		done
	{"Rage",		V_INT,V_STR,V_STR, 	1,	 1},	// 40		done
	{"Resist Cold",		   -1,   -1,   -1, 	0,	 1},	// 41		done
	{"Profession",		   -1,   -1,   -1, 	3,	 1}	// 42		
};

int raise_cost(int v,int n,int seyan)
{
	int nr;

	nr=n-skill[v].start+1+5;

	if (seyan) return max(1,nr*nr*nr*skill[v].cost*4/30);
        else return max(1,nr*nr*nr*skill[v].cost/10);
}

int supermax_canraise(int skl)
{
	switch(skl) {
		case V_WIS:
		case V_INT:
		case V_AGI:
		case V_STR:		return 2;

		case V_PULSE:
		case V_ARMORSKILL:
		case V_DAGGER:
		case V_HAND:
		case V_STAFF:
		case V_SWORD:
		case V_TWOHAND:
		case V_ATTACK:
		case V_PARRY:
		case V_WARCRY:
		case V_TACTICS:
		case V_SURROUND:
		case V_BODYCONTROL:
		case V_SPEEDSKILL:
		case V_BARTER:
		case V_PERCEPT:
		case V_STEALTH:
		case V_BLESS:
		case V_HEAL:
		case V_FREEZE:
		case V_MAGICSHIELD:
		case V_FLASH:
                case V_FIRE:
		case V_REGENERATE:
		case V_MEDITATE:
		case V_IMMUNITY:
		case V_DURATION:
		case V_RAGE:		return 1;

		default:		return 0;
	}
}

int supermax_cost(int cn,int skl,int val)
{
	int seyan;

	if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1; else seyan=0;

	return supermax_canraise(skl)*3000000+raise_cost(skl,val,seyan);
}

int skillmax(int cn)
{
	int seyan;

	if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1; else seyan=0;
	
	return (seyan ? 100 : 115) + ((ch[cn].flags&CF_HARDCORE) ? 7 : 0);
}

int calc_exp(int cn)
{
	int v,n,exp=0,seyan;

	if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1;
	else seyan=0;

	for (v=0; v<V_MAX; v++) {
		if (!ch[cn].value[1][v]) continue;
		if (!skill[v].cost) continue;
		
		for (n=skill[v].start+1; n<=ch[cn].value[1][v]; n++) {
			if ((ch[cn].flags&CF_PLAYER) && n-1>=skillmax(cn)) {
				exp+=supermax_cost(cn,v,n-1);
				//xlog("supermax %d: %d",n,supermax_cost(cn,v,n-1));
                        } else exp+=raise_cost(v,n-1,seyan);			
		}		
	}
       	
	return exp;
}

// raise value v of cn by 1, does error checking
int raise_value(int cn,int v)
{
	int cost,seyan;
	int hardcore=0;

        if (v<0 || v>V_MAX) return 0;

	if (!skill[v].cost) return 0;

	if (!ch[cn].value[1][v]) return 0;
	
	if (!(ch[cn].flags&CF_ARCH) && ch[cn].value[1][v]>49) return 0;

        if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1;
	else seyan=0;

	if (ch[cn].flags&CF_HARDCORE) hardcore=7;
	/*{
		if (seyan) hardcore=5;
		else hardcore=7;		
	}*/

	if (seyan && ch[cn].value[1][v]>99+hardcore) return 0;
	if (ch[cn].value[1][v]>114+hardcore) return 0;

	if (v==V_PROFESSION && ch[cn].value[1][v]>99) return 0;

        cost=raise_cost(v,ch[cn].value[1][v],seyan);
	if (ch[cn].exp_used+cost>ch[cn].exp) return 0;

	ch[cn].exp_used+=cost;

	ch[cn].value[1][v]++;
	
	update_char(cn);
	dlog(cn,0,"raised %s to %d",skill[v].name,ch[cn].value[1][v]);

	return 1;
}

// lower value v of cn by 1, does error checking
int lower_value(int cn,int v)
{
	int cost,seyan;

        if (v<0 || v>V_MAX) return 0;

	if (!skill[v].cost) return 0;

	if (ch[cn].value[1][v]<=skill[v].start) return 0;
	
        if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1;
	else seyan=0;

	ch[cn].value[1][v]--;

        cost=raise_cost(v,ch[cn].value[1][v],seyan);
        ch[cn].exp_used-=cost;
	
	update_char(cn);
	dlog(cn,0,"lowered %s to %d",skill[v].name,ch[cn].value[1][v]);

	return 1;
}

// raise value v of cn by 1 and give exp for it, does error checking
int raise_value_exp(int cn,int v)
{
	int cost,seyan;
	int hardcore=0;

        if (v<0 || v>V_MAX) return 0;

	if (!skill[v].cost) return 0;

	if (!ch[cn].value[1][v]) return 0;
	
	if (!(ch[cn].flags&CF_ARCH) && ch[cn].value[1][v]>49) return 0;

        if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==(CF_WARRIOR|CF_MAGE)) seyan=1;
	else seyan=0;

	if (ch[cn].flags&CF_HARDCORE) hardcore=7;
	/*{
		if (seyan) hardcore=5;
		else hardcore=7;
	}*/

	if (seyan && ch[cn].value[1][v]>99+hardcore) return 0;
	if (ch[cn].value[1][v]>114+hardcore) return 0;

	if (v==V_PROFESSION && ch[cn].value[1][v]>99) return 0;

        cost=raise_cost(v,ch[cn].value[1][v],seyan);

	ch[cn].exp_used+=cost;
	ch[cn].exp+=cost;
	check_levelup(cn);

	ch[cn].value[1][v]++;
	
	update_char(cn);
	dlog(cn,0,"raised %s to %d",skill[v].name,ch[cn].value[1][v]);

	return 1;
}

