/*
 * $Id: skill.c,v 1.3 2007/07/04 09:23:52 devel Exp $
 *
 * $Log: skill.c,v $
 * Revision 1.3  2007/07/04 09:23:52  devel
 * added special limit for profession points
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
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
	{"Hitpoints",		   -1,   -1,   -1, 	3,	25},	// 0		done
	{"Endurance",		   -1,   -1,   -1, 	3,	25},	// 1		done
	{"Mana",		   -1,   -1,   -1, 	3,	25},	// 2		done

	// Attributes
	{"Wisdom",		   -1,   -1,   -1, 	2,	10},	// 3		done
	{"Intuition",		   -1,   -1,   -1, 	2,	10},	// 4		done
	{"Agility",		   -1,   -1,   -1, 	2,	10},	// 5		done
	{"Strength",		   -1,   -1,   -1, 	2,	10},	// 6		done

	// Values
	{"Armor",		   -1,   -1,   -1, 	0,	 0},	// 7		done
	{"Weapon",		   -1,   -1,   -1, 	0,	 0},	// 8		done
	{"Offense",		   -1,   -1,   -1, 	0,	 0},	// 9		done
	{"Defense",		   -1,   -1,   -1, 	0,	 0},	// 10		done
	{"Light",		   -1,   -1,   -1, 	0,	 0},	// 11		done
	{"Speed",     		V_AGI,V_AGI,V_STR, 	0,	 0},	// 12		done

        // Primary Fighting Skills	
	{"Dagger",		V_INT,V_AGI,V_STR, 	1,	 1},	// 13		done
	{"Hand to Hand",	V_AGI,V_STR,V_STR, 	1,	 1},	// 14		done
        {"Staff",		V_INT,V_AGI,V_STR, 	1,	 1},	// 15		done
	{"Sword",		V_INT,V_AGI,V_STR, 	1,	 1},	// 16		done
	{"Two-Handed",		V_AGI,V_STR,V_STR, 	1,	 1},	// 17		done

	// Secondary Fighting Skills
        {"Attack",		V_INT,V_AGI,V_STR, 	1,	 1},	// 18		done
	{"Parry",		V_INT,V_AGI,V_STR, 	1,	 1},	// 19		done
	{"Warcry",		V_INT,V_AGI,V_STR, 	1,	 1},	// 20		done
	{"Tactics",		V_INT,V_AGI,V_STR, 	1,	 1},	// 21		done
	{"Surround Hit",	V_INT,V_AGI,V_STR, 	1,	 1},	// 22		done
	{"Speed Skill",		V_INT,V_AGI,V_STR, 	1,	 1},	// 23		done

	// Misc. Skills
        {"Bartering",		V_INT,V_INT,V_WIS, 	1,	 1},	// 24		done
        {"Perception",		V_INT,V_INT,V_WIS, 	1,	 1},	// 25		done
	{"Stealth",		V_INT,V_AGI,V_AGI, 	1,	 1},	// 26		done

	// Spells
	{"Bless",		V_INT,V_INT,V_WIS, 	1,	 1},	// 27		done
	{"Heal",		V_INT,V_INT,V_WIS, 	1,	 1},	// 28		done
	{"Freeze",		V_INT,V_INT,V_WIS, 	1,	 1},	// 29		done
	{"Magic Shield",	V_INT,V_INT,V_WIS, 	1,	 1},	// 30		done
	{"Lightning",		V_INT,V_INT,V_WIS, 	1,	 1},	// 31		done
	{"Fire",		V_INT,V_INT,V_WIS, 	1,	 1},	// 32		done

	{"Regenerate",		V_STR,V_STR,V_STR, 	1,	 1},	// 33		done
	{"Meditate",		V_WIS,V_WIS,V_WIS, 	1,	 1},	// 34		done
	{"Immunity",		V_INT,V_WIS,V_STR, 	1,	 1},	// 35		done

	{"Ancient Knowledge",	V_INT,V_WIS,V_STR, 	0,	 1},	// 36		done
	{"Duration",		V_INT,V_WIS,V_STR, 	1,	 1},	// 37		done
	{"Rage",		V_INT,V_STR,V_STR, 	1,	 1},	// 38		done
	{"Resist Cold",		   -1,   -1,   -1, 	0,	 1},	// 39		done
	{"Profession",		   -1,   -1,   -1, 	3,	 1}	// 40		
};

int raise_cost(int v,int n)
{
	int nr;

	nr=n-skill[v].start+1+5;

        return max(1,nr*nr*nr*skill[v].cost/10);
}

int calc_exp(int cn)
{
	int v,n,exp=0;

        for (v=0; v<V_MAX; v++) {
		if (!ch[cn].value[1][v]) continue;
		if (!skill[v].cost) continue;
		
		for (n=skill[v].start+1; n<=ch[cn].value[1][v]; n++) {
			exp+=raise_cost(v,n-1);
		}		
	}
       	
	return exp;
}

// raise value v of cn by 1, does error checking
int raise_value(int cn,int v)
{
	int cost;

        if (v<0 || v>V_MAX) return 0;

	if (!skill[v].cost) return 0;

	if (!ch[cn].value[1][v]) return 0;

	if (v==V_PROFESSION) {
		if (!(ch[cn].flags&CF_ARCH) && ch[cn].value[1][v]>24) return 0;
                if (ch[cn].value[1][v]>49) return 0;
	}
	
	if (!(ch[cn].flags&CF_ARCH) && ch[cn].value[1][v]>49) return 0;

        if (ch[cn].value[1][v]>99) return 0;

        cost=raise_cost(v,ch[cn].value[1][v]);
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
	int cost;

        if (v<0 || v>V_MAX) return 0;

	if (!skill[v].cost) return 0;

	if (ch[cn].value[1][v]<=skill[v].start) return 0;
	
        ch[cn].value[1][v]--;

        cost=raise_cost(v,ch[cn].value[1][v]);
        ch[cn].exp_used-=cost;
	
	update_char(cn);
	dlog(cn,0,"lowered %s to %d",skill[v].name,ch[cn].value[1][v]);

	return 1;
}

// raise value v of cn by 1 and give exp for it, does error checking
int raise_value_exp(int cn,int v)
{
	int cost;

        if (v<0 || v>V_MAX) return 0;

	if (!skill[v].cost) return 0;

	if (!ch[cn].value[1][v]) return 0;
	
	if (!(ch[cn].flags&CF_ARCH) && ch[cn].value[1][v]>49) return 0;

        if (ch[cn].value[1][v]>99) return 0;

        cost=raise_cost(v,ch[cn].value[1][v]);

	ch[cn].exp_used+=cost;
	ch[cn].exp+=cost;
	check_levelup(cn);

	ch[cn].value[1][v]++;
	
	update_char(cn);
	dlog(cn,0,"raised %s to %d",skill[v].name,ch[cn].value[1][v]);

	return 1;
}

