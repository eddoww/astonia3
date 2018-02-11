#include <stdlib.h>

#define ISCLIENT
#include "main.h"
#include "client.h"
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
	{"Profession",		   -1,   -1,   -1, 	3,	 1},	// 40
	
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 41
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 42
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 43
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 44
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 45
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 46
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 47
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 48
	{"empty",		   -1,   -1,   -1, 	3,	 1},	// 49
	

	{"Smith",       	   -1,   -1,   -1, 	0,	 1},	// 0
	{"Alchemist",    	   -1,   -1,   -1, 	0,	 1},	// 1
	{"Miner",        	   -1,   -1,   -1, 	0,	 1},	// 2
        {"Enhancer",		   -1,   -1,   -1, 	0,	 1},	// 3
	{"Mercenary",		   -1,   -1,   -1, 	0,	 1},	// 4
        {"Trader",     		   -1,   -1,   -1, 	0,	 1},	// 5
	{"empty",  		   -1,   -1,   -1, 	0,	 1},	// 6
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 7
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 8
	{"empty",        	   -1,   -1,   -1, 	0,	 1}	// 9
};

int raise_cost(int v,int n)
{
	int nr;

        nr=n-skill[v].start+1+5;

        return max(1,nr*nr*nr*skill[v].cost/10);
}

char *skilldesc[]={
	"Hitpoints ('life force') are reduced as you battle and sustain injury. The top red line above your head shows your Hitpoints level. If your Hitpoints level drops to zero, then you will die!",
	"Endurance enables you to run and fight with greater speed. When your Endurance is gone, you will slow down and feel exhausted.",
	"Magic users need Mana to cast spells. If your Mana runs out, then you can no longer cast spells or perform magic. If you stand completely still for a few moments, your Mana will replenish itself.",
	"Wisdom, a base attribute, deepens your comprehension of the secrets of spells and magic; enhances all spells and some skills.",
	"Intuition helps you remain calm and focused when fighting. This base attribute is needed for all spells and most other skills.",
	"Agility will improve your control over your body movements. This base attribute enhances all fighting skills; it will help you move and fight faster.",
	"Strength increases your physical power, allowing you to hit harder. This base attribute is needed for all fighting skills and some other skills.",

        "The amount of protection given by your armor.",
	"The amount of damage your weapon is doing.",
	"Your overall offensive strength. How high your chances are to hit an enemy with your current weapon.",
	"Your overall defensive strength. How high your chances are to evade an enemy's blow.",
	"The amount of light you are radiating.",
	"How fast your movements are.",

        "The Dagger skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with daggers.",
	"This skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with no weapons.",
	"The Staff skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with a mage's staff.",
	"The Sword skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with swords or other similar weapons.",
	"The Two-Handed skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with Two-Handed weapons.",

        "Increases your chances to hit an enemy.",
	"Parry allows you to raise your defenses by fending off hits from the enemy.",
	"In the heat of battle, cry out loud, and your enemy will be stunned in terror.",
	"Tactical usage of Attack and Parry; transfers part of Parry to Attack when you hit, and vice versa when you defend.",
        "Inflicts injury to the enemies on your left side and on your right side.",
        "Helps make you a better, faster fighter - the faster you fight, the faster you kill, and the less injury you sustain. It may save thy life.",

	"Bartering increases your negotiating skills, particularly with merchants, enabling you to purchase shop items at a lower price, and sell your own items to shops for a higher price.",
	"Perception increases the depth of your sight, thus allowing you to see enemies better at night. It also allows you to more easily see those who are using Stealth.",
	"Stealth allows you to approach an enemy without being seen.",

	"Bless increases all attributes. It will make you more powerful, but only for a limited time.",
	"This spell will heal injuries and recover Hitpoints - the greater the damage suffered, the more Mana that must be used for healing.",
	"Turns the enemy to ice.",

	"This spell creates a Magic Shield to increase the strength of your armor, but becomes weaker with each enemy hit received.",
	"This spell will either send a Lightning ball, or create lightning flashes to harm your enemies.",

	"Fire unleashes its energy in an enormous explosion, or creates a firy ring around you.",

	"Regenerate helps recover Hitpoints when you are standing motionless; the higher you train this skill, the faster your Hitpoints will replenish.",
	"Meditate helps a magic user recover Mana; the higher you train this skill, the faster your Mana will replenish when standing still.",
	"Reduces the power of spells cast on you by the enemy.",

	"Your knowledge of the ancients, their language and magic.",
	
	"Makes spells last longer.",
	"As you fight continuously, your Rage builds up to increase Attack and Parry.",
	
	"Resisting demonic cold.",
	"Raise this skill to be able to learn/improve your professions.",

	"empty",
	"empty",
	"empty",
	"empty",
	"empty",
	"empty",
	"empty",
	"empty",
	"empty",

	"Repair weapons more efficiently.",
	"Create better potions.",
	"Find more silver/gold in mines.",
        "Build up orbs faster.",
	"Get money from governor for missions; raise faster in rank.",
	"Better prices from merchants.",
	"empty",
        "empty",
	"prof9: write me!",
	"prof10: write me!"};
