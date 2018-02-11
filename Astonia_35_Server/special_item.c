/*
 * $Id: special_item.c,v 1.3 2008/04/14 14:39:42 devel Exp $
 *
 * $Log: special_item.c,v $
 * Revision 1.3  2008/04/14 14:39:42  devel
 * added about 30 new items
 *
 */

struct special_item
{
	char *name;
	int mod_index[3];
	int chance;
	unsigned long long needflag;
	int the;
};

#define SP_MANY		1000
#define SP_SOME		250
#define SP_FEW		50
#define SP_RARE		10
#define SP_ULTRA	1

struct special_item special_item[]={
        {"Wisdom",			{V_WIS,-1,-1},				SP_MANY,	0,		0},
	{"Intuition",			{V_INT,-1,-1},				SP_MANY,	0,		0},
	{"Agility",			{V_AGI,-1,-1},				SP_MANY,	0,		0},
	{"Strength",			{V_STR,-1,-1},				SP_MANY,	0,		0},
	
	{"Hitpoints",			{V_HP,-1,-1},				SP_MANY,	0,		0},
	{"Mana",			{V_MANA,-1,-1},				SP_MANY,	0,		0},
	{"Endurance",			{V_ENDURANCE,-1,-1},			SP_MANY,	0,		0},

	{"Dagger",			{V_DAGGER,-1,-1},			SP_SOME,	IF_DAGGER,	0},
	{"Hand to Hand",		{V_HAND,-1,-1},				SP_SOME,	0,		0},
	{"Staff",			{V_STAFF,-1,-1},			SP_SOME,	IF_STAFF,	0},
	{"Sword",			{V_SWORD,-1,-1},			SP_SOME,	IF_SWORD,	0},
	{"Two-Handed",			{V_TWOHAND,-1,-1},			SP_SOME,	IF_TWOHAND,	0},

	{"Attack",			{V_ATTACK,-1,-1},			SP_SOME,	0,		0},
	{"Parry",			{V_PARRY,-1,-1},			SP_SOME,	0,		0},
	{"Warcry",			{V_WARCRY,-1,-1},			SP_MANY,	0,		0},
	{"Tactics",			{V_TACTICS,-1,-1},			SP_SOME,	0,		0},
	{"Surround Hit",		{V_SURROUND,-1,-1},			SP_MANY,	0,		0},
        {"Speed",			{V_SPEEDSKILL,-1,-1},			SP_MANY,	0,		0},

	{"Bartering",			{V_BARTER,-1,-1},			SP_MANY,	0,		0},
	{"Perception",			{V_PERCEPT,-1,-1},			SP_MANY,	0,		0},
	{"Stealth",			{V_STEALTH,-1,-1},			SP_MANY,	0,		0},

	{"Bless",			{V_BLESS,-1,-1},			SP_MANY,	0,		0},
	{"Heal",			{V_HEAL,-1,-1},				SP_MANY,	0,		0},
	{"Freeze",			{V_FREEZE,-1,-1},			SP_SOME,	0,		0},
	{"Magic Shield",		{V_MAGICSHIELD,-1,-1},			SP_SOME,	0,		0},
	{"Lightning",			{V_FLASH,-1,-1},			SP_SOME,	0,		0},
	{"Fireball",			{V_FIRE,-1,-1},				SP_MANY,	0,		0},

	{"Regenerate",			{V_REGENERATE,-1,-1},			SP_MANY,	0,		0},
	{"Meditate",			{V_MEDITATE,-1,-1},			SP_MANY,	0,		0},
	{"Immunity",			{V_IMMUNITY,-1,-1},			SP_SOME,	0,		0},
	{"Duration",			{V_DURATION,-1,-1},			SP_SOME,	0,		0},
	{"Rage",			{V_RAGE,-1,-1},				SP_SOME,	0,		0},

	{"Thief",			{V_STEALTH,V_ENDURANCE,-1},		SP_FEW,		0,		1},
	{"Vision",			{V_PERCEPT,V_INT,-1},			SP_FEW,		0,		0},
	{"Wounded",			{V_HEAL,V_REGENERATE,-1},		SP_FEW,		0,		1},
        {"Sorcery",			{V_MANA,V_DURATION,-1},			SP_FEW,		0,		0},

	{"Fighting",			{V_ATTACK,V_PARRY,-1},			SP_RARE,	0,		0},
	{"Magic",			{V_FLASH,V_MAGICSHIELD,-1},		SP_RARE,	0,		0},
	{"Berserk",			{V_ATTACK,V_RAGE,-1},			SP_RARE,	0,		0},
	
        {"One Handed Offense",		{V_ATTACK,V_SWORD,-1},			SP_RARE,	IF_SWORD,	0},
	{"One Handed Defense",		{V_PARRY,V_SWORD,-1},			SP_RARE,	IF_SWORD,	0},
        {"Two Handed Offense",		{V_ATTACK,V_TWOHAND,-1},		SP_RARE,	IF_TWOHAND,	0},
	{"Two Handed Defense",		{V_PARRY,V_TWOHAND,-1},			SP_RARE,	IF_TWOHAND,	0},

	{"Tactical Offense",		{V_ATTACK,V_TACTICS,-1},		SP_RARE,	0,		0},
	{"Tactical Defense",		{V_PARRY,V_TACTICS,-1},			SP_RARE,	0,		0},

	{"Magical Offense",		{V_FLASH,V_FIRE,V_BLESS},		SP_RARE,	0,		0},
	{"Magical Defense",		{V_MAGICSHIELD,V_FREEZE,V_HEAL},	SP_RARE,	0,		0},

	{"Warrior",			{V_ATTACK,V_PARRY,V_IMMUNITY},		SP_ULTRA,	0,		1},
	{"Mage",			{V_FLASH,V_MAGICSHIELD,V_IMMUNITY},	SP_ULTRA,	0,		1},
	{"Tactician",			{V_ATTACK,V_PARRY,V_TACTICS},		SP_ULTRA,	0,		1},
	{"Arch-Warrior",		{V_ATTACK,V_PARRY,V_RAGE},		SP_ULTRA,	0,		1},
	{"Arch-Mage",			{V_FLASH,V_MAGICSHIELD,V_DURATION},	SP_ULTRA,	0,		1},

	{"Guardian",			{V_HP,V_ENDURANCE,V_MANA},		SP_RARE,	0,		1},
	{"Arcane",			{V_WIS,V_INT,-1},			SP_FEW,		0,		1},
	{"Soldier",			{V_STR,V_AGI,-1},			SP_FEW,		0,		1},
	
	{"Silent Running",		{V_STEALTH,V_SPEEDSKILL,-1},		SP_FEW,		0,		0},
	{"Focus",			{V_INT,V_FLASH,-1},			SP_FEW,		0,		0},
	{"Rampage",			{V_STR,V_ATTACK,-1},			SP_FEW,		0,		0},
	{"Dashing",			{V_AGI,V_SPEEDSKILL,-1},		SP_FEW,		0,		0},
	{"Shielding",			{V_MAGICSHIELD,V_HP,-1},		SP_FEW,		0,		0},
	{"Spellcaster",			{V_MANA,V_INT,V_WIS},			SP_RARE,	0,		1},
	{"Athlete",			{V_SPEEDSKILL,V_ENDURANCE,-1},		SP_FEW,		0,		0},

	{"Arctic",			{V_FREEZE,V_DURATION,-1},		SP_FEW,		0,		1},
	{"Power",			{V_ATTACK,V_PARRY,V_SURROUND},		SP_RARE,	0,		0},
	{"Gale",			{V_AGI,V_STR,V_SPEEDSKILL},		SP_RARE,	0,		0},
	{"Heaven",			{V_FLASH,V_MAGICSHIELD,V_BLESS},	SP_RARE,	0,		0},
	{"Hell",			{V_FIRE,V_MAGICSHIELD,V_BLESS},		SP_RARE,	0,		0},
	{"Onehanded Fighting",		{V_SWORD,V_SURROUND,V_SPEEDSKILL},	SP_RARE,	0,		0},
	{"Twohanded Fighting",		{V_TWOHAND,V_SURROUND,V_SPEEDSKILL},	SP_RARE,	0,		0},
	{"Icestorm",			{V_FREEZE,V_FLASH,V_DURATION},		SP_RARE,	0,		0},
	{"Scholar",			{V_WIS,V_INT,V_PERCEPT},		SP_RARE,	0,		1},
	{"Warlord",			{V_AGI,V_STR,V_ATTACK},			SP_RARE,	0,		1},

	{"Battle",			{V_WARCRY,V_ENDURANCE,V_SURROUND},	SP_RARE,	0,		0},

	{"Protection",			{V_HP,V_IMMUNITY,-1},			SP_FEW,		0,		0},
	{"Ultimate Protection",		{V_HP,V_IMMUNITY,V_MAGICSHIELD},	SP_RARE,	0,		0},
	{"Light",			{V_MANA,V_FLASH,-1},			SP_FEW,		0,		1},
	{"Holy Light",			{V_MANA,V_FLASH,V_IMMUNITY},		SP_RARE,	0,		1},

	{"Trader",			{V_INT,V_WIS,V_BARTER},			SP_RARE,	0,		1},
	{"Sly",				{V_INT,V_AGI,V_PERCEPT},		SP_RARE,	0,		1},

	{"Hero",			{V_FLASH,V_ATTACK,-1},			SP_FEW,		0,		1},
	{"Frozen Sword",		{V_SWORD,V_FREEZE,-1},			SP_FEW,		IF_SWORD,	1},
	{"Holy Frozen Sword",		{V_SWORD,V_FREEZE,V_FLASH},		SP_RARE,	IF_SWORD,	1},
	{"Frozen Two-Handed",		{V_TWOHAND,V_FREEZE,-1},		SP_FEW,		IF_TWOHAND,	1},
	{"Holy Frozen Two-Handed",	{V_TWOHAND,V_FREEZE,V_FLASH},		SP_RARE,	IF_TWOHAND,	1},
	{"Recovery",			{V_REGENERATE,V_MEDITATE,-1},		SP_FEW,		0,		0},
	{"Demoralization",		{V_FREEZE,V_WARCRY,-1},			SP_FEW,		0,		0},
	{"Tactical Bartering",		{V_TACTICS,V_BARTER,-1},		SP_FEW,		0,		0},
	{"Magical Parry",		{V_DAGGER,V_MAGICSHIELD,V_IMMUNITY},	SP_ULTRA,	0,		0},
	{"Fiery Focus",			{V_INT,V_FIRE,-1},			SP_FEW,		0,		0},
	{"Mage Defense",		{V_DAGGER,V_IMMUNITY,-1},		SP_FEW,		0,		0},
	{"Tactical Warcry",		{V_TACTICS,V_WARCRY,-1},		SP_FEW,		0,		0},
	{"Relentless",			{V_ATTACK,V_STR,V_RAGE},		SP_ULTRA,	0,		1},
	{"Magical Parry",		{V_DAGGER,V_MAGICSHIELD,V_IMMUNITY},	SP_ULTRA,	0,		0},
	{"Holy Ice",			{V_FREEZE,V_BLESS,-1},			SP_FEW,		0,		0},
	{"Hidden Magic",		{V_STEALTH,V_FLASH,-1},			SP_FEW,		0,		0},
	{"Tactical Magic",		{V_TACTICS,V_MANA,-1},			SP_FEW,		0,		0},
	{"Pirate",			{V_INT,V_AGI,-1},			SP_FEW,		0,		1},
	{"Warlock",			{V_FLASH,V_MAGICSHIELD,V_INT},		SP_ULTRA,	0,		1},
	{"Firestorm",			{V_FIRE,V_FLASH,-1},			SP_FEW,		0,		0},
	{"Defensive Power",		{V_PARRY,V_STR,-1},			SP_FEW,		0,		0},
	{"Swordsman",			{V_SWORD,V_ATTACK,V_PARRY},		SP_ULTRA,	0,		1},
	{"Hell Mage",			{V_FIRE,V_MAGICSHIELD,V_IMMUNITY},	SP_ULTRA,	0,		1},
	{"Smart Survical",		{V_HP,V_IMMUNITY,V_INT},		SP_RARE,	0,		0},
	{"Durability",			{V_PARRY,V_HP,-1},			SP_FEW,		0,		0},
	{"Survival",			{V_HP,V_MANA,-1},			SP_FEW,		0,		0}
};

char *item_strength_text(int strength)
{
	char *str_desc;
	switch(strength) {
		case 1:		str_desc="Extremely Weak "; break;
		case 2:		str_desc="Very Weak "; break;
		case 3:		str_desc="Weak "; break;
		case 4:		str_desc="Fairly Weak "; break;
		case 5:		str_desc="Somewhat Weak "; break;
		case 6:		str_desc=""; break;
		case 7:		str_desc="Somewhat Strong "; break;
		case 8:		str_desc="Fairly Strong "; break;
		case 9:		str_desc="Strong "; break;
		case 10:	str_desc="Very Strong "; break;
		case 11:	str_desc="Extremely Strong "; break;
		case 12:	str_desc="Somewhat Powerful "; break;
		case 13:	str_desc="Fairly Powerful "; break;
		case 14:	str_desc="Powerful "; break;
		case 15:	str_desc="Very Powerful "; break;
		case 16:	str_desc="Extremely Powerful "; break;
		case 17:	str_desc="Superhuman "; break;
		case 18:	str_desc="Demi-Godly "; break;
		case 19:	str_desc="Godly "; break;
                case 20:	str_desc="Ultimate "; break;
		default:	str_desc="Buggy "; break;
	}
	return str_desc;
}















