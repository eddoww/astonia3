#include <stdlib.h>

#define ISCLIENT
#include "main.h"
#include "client.h"
#include "skill.h"

struct skill skill[V_MAX]={
				//  Bases          Cost W M (0=not raisable, 1=skill, 2=attribute, 3=power)
	// Powers
	{"Hitpoints",		   -1,   -1,   -1, 	3,	10},	// 0		done
	{"Endurance",		   -1,   -1,   -1, 	3,	10},	// 1		
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

	{"Pulse",	  	V_INT,V_WIS,V_STR, 	1,	 1},	// 11		done

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
	{"Warcry",		V_INT,V_AGI,V_STR, 	1,	 1},	// 20		
	{"Tactics",     	V_INT,V_AGI,V_STR,  	1,   	1},     // 21
	{"Surround Hit",	V_INT,V_AGI,V_STR, 	1,	 1},	// 22		
	{"Body Control",	V_INT,V_AGI,V_STR, 	1,	 1},	// 23		done
	{"Speed Skill",		V_INT,V_AGI,V_STR, 	1,	 1},	// 24		done

	// Misc. Skills
        {"Bartering",		V_INT,V_INT,V_WIS, 	1,	 1},	// 25
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
	{"Immunity",		V_INT,V_WIS,V_STR, 	1,	 1},	// 37		

	{"Ancient Knowledge",	V_WIS,V_INT,V_STR, 	1,	 1},	// 38

	{"Duration",		V_WIS,V_INT,V_STR, 	1,	 1},	// 39
	{"Rage",		V_WIS,V_INT,V_STR, 	1,	 1},	// 40
	{"Resist Cold",		   -1,   -1,   -1, 	0,	 1},	// 41
	{"Profession",		   -1,   -1,   -1, 	3,	 1},	// 42



	{"Athlete",       	   -1,   -1,   -1, 	0,	 1},	// 0
	{"Alchemist",    	   -1,   -1,   -1, 	0,	 1},	// 1
	{"Miner",        	   -1,   -1,   -1, 	0,	 1},	// 2
	{"Assassin",     	   -1,   -1,   -1, 	0,	 1},	// 3
	{"Thief",        	   -1,   -1,   -1, 	0,	 1},	// 4
	{"Light",		   -1,   -1,   -1, 	0,	 1},	// 5
	{"Dark",		   -1,   -1,   -1, 	0,	 1},	// 6
        {"Trader",     		   -1,   -1,   -1, 	0,	 1},	// 7
	{"Mercenary",  		   -1,   -1,   -1, 	0,	 1},	// 8
	{"Clan",		   -1,   -1,   -1, 	0,	 1},	// 9
	{"Herbalist",		   -1,   -1,   -1, 	0,	 1},	// 10
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 11
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 12
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 13
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 14
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 15
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 16
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 17
	{"empty",        	   -1,   -1,   -1, 	0,	 1},	// 18
	{"empty",        	   -1,   -1,   -1, 	0,	 1}	// 19
};

int raise_cost(int v,int n)
{
	int nr,seyan;

	// hack to determine if we are a seyan:
	if (value[0][V_ATTACK] && value[0][V_BLESS]) seyan=1;
	else seyan=0;

	nr=n-skill[v].start+1+5;

	if (seyan) return max(1,nr*nr*nr*skill[v].cost*4/30);
        else return max(1,nr*nr*nr*skill[v].cost/10);
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
	"The amount of light you are radiating.",
	"How fast your movements are.",

	"Destroys the weakened enemies that surround you by transferring their Hitpoints to you in form of Mana.",

	"The Dagger skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with daggers.",
	"This skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with no weapons.",
	"The Staff skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with a mage's staff.",
	"The Sword skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with swords or other similar weapons.",
	"The Two-Handed skill helps you to hit the enemy more often when attacking and increases your chances to parry an enemy's hits; this skill is mainly used when fighting with Two-Handed weapons.",

        "This skill allows you to wear stronger armor. It may also enhance the protection value of your armor.",
	"Raise this skill to exploit your enemy's weaknesses and hit him when his defenses are down.",
	"Parry allows you to raise your defenses by fending off hits from the enemy.",
	"In the heat of battle, cry out loud, and your enemy will be stunned in terror.",
	"Tactical usage of Attack and Parry; transfers part of Parry to Attack when you hit, and vice versa when you defend.",
        "Inflicts injury to the enemies on your left side and on your right side.",
        "As you train, your body will become hard and strong, providing you with additional protection from enemy hits; Body Control helps increase your weapon value too.",
	"Helps make you a better, faster fighter - the faster you fight, the faster you kill, and the less injury you sustain. It may save thy life.",

	"Bartering increases your negotiating skills, particularly with merchants, enabling you to purchase shop items at a lower price, and sell your own items to shops for a higher price.",
	"Perception increases the depth of your sight, thus allowing you to see enemies better at night. It also allows you to more easily see those who are using Stealth.",
	"Stealth allows you to approach an enemy without being seen.",

	"Bless increases all attributes. It will make you more powerful, but only for a limited time. Casting one Bless spell costs two Mana points.",
	"This spell will heal injuries and recover Hitpoints - the greater the damage suffered, the more Mana that must be used for healing.",
	"Turns the enemy to ice. Casting one Freeze spell costs two Mana points.",

	"This spell creates a Magic Shield to increase the strength of your armor, but becomes weaker with each enemy hit received. It costs one Mana point to raise Magic Shield by two.",
	"This spell will either send a Lightning ball, or create lightning flashes to harm your enemies. One Lightning spell costs three Mana points.",

	"Fire unleashes its energy in an enormous explosion. Cost: three Mana points.",
	"empty",

	"Regenerate helps recover Hitpoints when you are standing motionless; the higher you train this skill, the faster your Hitpoints will replenish.",
	"Meditate helps a magic user recover Mana; the higher you train this skill, the faster your Mana will replenish when standing still.",
	"Reduces the power of spells cast on you by the enemy.",

	"Your knowledge of the ancients, their language and magic.",
	
	"Makes spells last longer.",
	"As you fight continuously, your Rage builds up to increase Attack and Parry.",
	
	"Resisting demonic cold.",
	"Raise this skill to be able to learn/improve your professions.",

	"Increases speed, has three times the effect of Speed Skill; also reduces endurance cost of fast mode by up to 66%; all classes, base cost 6, maxes at 30.",
	"Capable of making full moon, equinox, and solstice potions at any time (depending on profession level attained); all classes, base cost 10, maxes at 50.",
	"Increases probability to find silver/gold by 10% per PPS; reduced endurance cost (by PPS*4%); all classes, base cost 4, maxes at 20.",
	"Gets +1 to attack per PPS when attacking an enemy from the side/behind; if attacking an idle enemy from behind, gets +3 to attack per PPS, +5 damage per PPS, and backstab; all classes, base cost 10, maxes at 50.",
	"Gets +2 to stealth for each PPS; will remain invisible even when next to another character when walking or idle; a PK thief can steal from another PK within level limits; all classes, base cost 6, maxes at 30.",
	"Gets +0.5 per PPS to all bases from 6:00 to 18:00; also gets holy vision (ie. can see undead creatures in the dark but not walls/items/living beings); all classes, base cost 6, maxes at 30.",
	"Gets +0.5 per PPS to all bases from 18:00 to 6:00; also gets infravision (ie. can see other living characters in the dark but not walls/items/undead); cannot be learned by someone knowing 'Master of Light'; all classes, base cost 6, maxes at 30.",
	"Bonus (better prices) when buying/selling in shops; all classes, base cost 4, maxes at 20.",
	"PPS*2% more military points from military missions, also gets paid for doing them; all classes, base cost 4, maxes at 20.",
	"+PPS on all base stats in clan-catacombs; all classes, base cost 6, maxes at 30.",
	"Capable of picking a flower/berry/mushroom every 4-12 hours (depending on profession level attained); all classes, base cost 10, maxes at 30.",
	"Demon, non-player prof.",
	"prof13: write me!",
	"prof14: write me!",
	"prof15: write me!",
	"prof16: write me!",
	"prof17: write me!",
	"prof18: write me!",
	"prof19: write me!",
	"prof20: write me!"
};
