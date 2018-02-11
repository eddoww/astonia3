/*
 *
 * $Id: area.c,v 1.4 2007/06/22 12:51:15 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: area.c,v $
 * Revision 1.4  2007/06/22 12:51:15  devel
 * added caligar
 *
 * Revision 1.3  2005/11/23 19:30:46  ssim
 * added hell pents and logic for demon armor clothes in fire/ice pents
 *
 * Revision 1.2  2005/11/22 19:59:55  ssim
 * added area desc for teufelheim, long tunnel
 *
 */

#include <stdlib.h>

#include "server.h"
#include "talk.h"
#include "tool.h"
#include "drdata.h"
#include "death.h"
#include "date.h"
#include "player.h"
#include "area.h"

// should use area file instead of hardcoded list

struct section
{
	char *name;		// section name
	int level;		// recommendet character level
};

struct sector
{
	int fx,fy;
	int tx,ty;
	int section;
};

static struct section section[]=
{
	{"none",0},				//0
	{"Skellie I",1},			//1
	{"Thieves Guild II",8},			//2
	{"Skellie III Upstairs",5},		//3
	{"Zombie I",10},			//4
	{"Village",0},				//5
	{"Fortress",0},				//6
	{"Robber Outpost",8},			//7
	{"Skellie II",8},			//8
	{"Skellie III Downstairs",6},		//9
	{"Skellie Showdown",7},			//10
	{"Thieves Guild I",0},			//11
	{"Mad Mages",7},			//12
	{"Mad Knights",8},			//13
	{"Cameron",0},				//14
	{"Creeper Death Run",14},		//15
	{"Creepers",13},			//16
	{"Zombie Showdown",13},			//17
	{"Zombie II",13},			//18
	{"Zombie III",13},			//19
	{"Palace",11},				//20
	{"Underground Park I",15},		//21
	{"Underground Park II",16},		//22
	{"Underground Park III",17},		//23
	{"Moonish Caverns",18},			//24
	{"Lower Crypt",20},			//25
	{"Crypt",21},				//26
	{"Inner Crypt",22},			//27
	{"Skellie III Downbelow",7},		//28

	{"Sewers I",8},				//29
	{"Sewers II",10},			//30
	{"Sewers III",12},			//31
	{"Sewers IV",14},			//32
	{"Sewers V",16},			//33
	{"Sewers VI",18},			//34
	{"Sewers VII",20},			//35
	{"Sewers VIII",22},			//36
	{"Sewers IX",24},			//37
	{"Sewers X",26},			//38
	{"Sewers XI",28},			//39
	{"Sewers XII",30},			//40
	{"Sewers XIII",32},			//41
	{"Sewers XIV",34},			//42
	{"Sewers XV",36},			//43
	{"Sewers XVI",38},			//44

	{"Thieves House",1},			//45
	{"Skellie I",2},			//46
	{"Skellie II",3},			//47
	{"Skellie III",4},			//48
	{"Robbers Outpost",5},			//49
	{"Skellie IV",5},			//50
	{"Skellie V",6},			//51
	{"Mad Mages",7},			//52
	{"Mad Knights",8},			//53
	{"Thieves Guild",9},			//54
	{"Gwendylon's Tower",0},		//55
	{"Fortress",0},				//56
	{"Cameron",0},				//57
	{"The Woods",0},			//58

	{"Aston",0},				//59
	{"Garden",16},				//60
	{"Park",0},				//61
	{"Graveyard",0},			//62

	{"Pentagram Quest",0},			//63
	{"Earth Underground",0},		//64
	{"Fire Underground",0},			//65
	{"Ice Underground",0},			//66
	{"Ice Palace",0},			//67
	{"Mine",0},				//68
	{"Catacombs",0},       			//69
	{"Random Dungeons",0},			//70
	{"Swamp",0},				//71
	{"Forest",0},				//72
	{"Robber's Lair",36},			//73
	{"Dungeon of Bones",36},		//74
	{"Skeleton Ruin",36},			//75

	{"Robber Sector I",36},			//76
	{"Robber Sector II",39},       		//77
	{"Robber Sector III",42},		//78
	{"Robber Sector IV",45},		//79
	{"Exkordon Sewers",38},			//80
	{"Below the Library",44},		//81
	{"Exkordon",0},				//82
	{"Governor's Palace",0},		//83
	{"Skeleton's Lair",47},			//84
	{"Spider's Lair",45},			//85
	{"Zombie's Lair",48},			//86

	{"Depths of Bone, Level I",50},		//87
	{"Depths of Bone, Level II",51},	//88
	{"Depths of Bone, Level III",52},	//89
	{"Depths of Bone, Level IV",54},	//90
	{"Depths of Bone, Level V",55},		//91
	{"Depths of Bone, Level VI",57},	//92
	{"Depths of Bone, Level VII",58},	//93
	{"Depths of Bone, Level VIII",59},	//94
	{"Depths of Bone, Level IX",61},	//95
	{"Depths of Bone, Hidden Level",63},	//96
        {"Depths of Bone, Bottom",63},		//97
	{"Depths of Bone, Towers",0},		//98

	{"Tower of Ansuz",50},			//99
	{"Tower of Berkano",51},		//100
	{"Tower of Dagaz",52},			//101
	{"Tower of Ehwaz",54},			//102
	{"Tower of Fehu",55},			//103
	{"Tower of Hagalaz",57},		//104
	{"Tower of Isa",58},			//105
	{"Tower of Ingwaz",59},			//106
	{"Halls of Raidho",61},			//107

	{"Nomad Plains",0},			//108

	{"Live Quest Area",0},			//109

	{"Test Pents",0},			//110

	{"Labyrinth, Light&Dark",20},		//111

	{"Live Quest Area, Entrance",0},	//112

	{"Labyrinth, Undeads",30},		//113
	{"Labyrinth, Underwater",25},		//114
	{"Labyrinth, First Steps",10},		//115

	{"Labyrinth, Hard Life",15},		//116

	{"Ice Army Caves",0},			//117

	{"More Ice Army Caves",0},		//118

	{"Rodneys Warped World, Green",0},	//119
	{"Rodneys Warped World, Orange",0},	//120
	{"Rodneys Warped World, Red",0},	//121
	{"Rodneys Warped World, Blue",0},	//122
	{"Rodneys Warped World, White",0},	//123

	{"Dragon's Breath",30},			//124

	{"Tower Top",0},			//125
	{"Tower I",60},				//126
	{"Tower II",60},			//127
	{"Tower III",60},			//128
	{"Tower IV",60},			//129
	{"Tover V",60},				//130
	{"Tower VI",60},			//131
	{"Tower VII",60},			//132
	{"Tower VIII",60},			//133
	
	{"Brannington, Bar",0},			//134
	{"Brannington, Castle",0},		//135
	{"Brannington",0},			//136

	{"Grimroot",0},				//137

	{"Jobbington",0},			//138

	{"Long Tunnel",0},			//139

	{"Teufelheim, Slums",38},		//140
	{"Teufelheim, Worker District",70},	//141
	{"Teufelheim, Noble District",102},	//142
	{"Hell Pents",0},			//143	

	{"Caligar Forest",60},			//144
	{"Caligar City",0},			//145

	{"Dungeon of Blood",60},		//146
	{"Dungeon of Bone",60},			//147
	{"Dungeon of Flesh",60},		//148
	{"Palace Level 1",60},			//149
	{"Palace Level 2",60},			//150
	{"Palace Level 3",60},			//151
	{"Palace Level 4",60},			//152
	{"Amazon Den",60},			//153
	{"Underground Passage",60},		//154
};

static struct sector sector1[]={
        { 73,147, 91,165, 45},		// thieves house
	{146,115,202,169, 46},		// skellie 1
	{ 35,229, 72,252, 47},		// skellie 2
	{182,224,196,254, 48},		// skellie 3
	{  3,229, 33,251, 49},		// robbers outpost
	{ 72,209,103,254, 50},		// skellie 4
	{  3,186, 38,223, 51},		// skellie 5
	{169, 84,205,114, 52},		// mad mages
	{149,103,169,114, 52},		// mad mages
	{147, 58,169, 88, 53},		// mad knights
	{147, 88,153, 94, 53},		// mad knights
	{151,210,181,254, 54},		// mad knights
	{ 96,109,116,135, 55},		// gwendy's tower
	{110,105,114,110, 55},		// gwendy's tower
	{104,135,115,146, 55},		// gwendy's tower
	{114,168,134,176, 56},		// fortress
	{114,176,145,191, 56},		// fortress
	{108, 76,169,168, 57},		// cameron
	{  1,  1,254,254, 58},		// cameron
	{0,0,0,0,0}
};

static struct sector sector2[]={
	{  1,  1, 85, 70,  4},
	{123,  2,142, 57,  15},
	{127, 58,142,102,  15},
	{118,102,142,124,  15},
	{118,124,128,183,  16},
	{ 95,125,118,183,  16},
	{ 84,137, 84,183,  16},
	{ 59,148, 84,183,  16},
	{ 39,141, 59,183,  16},
	{ 18,124, 38,183,  16},
	{  2,112, 18,183,  16},
	{ 85,  1,122, 62,  17},
	{122, 58,126, 62,  17},
	{  1, 70, 14, 84,  18},
	{  1, 84,  7,100,  18},
	{ 15, 69, 47, 88,  18},
	{ 48, 74, 64, 74,  18},
	{ 58, 70, 90, 97,  18},
	{ 64, 98, 85,101,  18},
	{ 90, 63,121, 82,  18},
	{ 96, 83,101, 90,  18},
	{  3,101,  7,110,  19},
	{  8, 86, 12, 90,  19},
	{  8, 90, 19,111,  19},
	{ 20, 90, 39,123,  19},
	{ 40, 92, 52,140,  19},
	{ 53, 97, 60,140,  19},
	{ 60, 97, 60,148,  19},
	{ 64,102, 83,147,  19},
	{ 84,124, 94,136,  19},
	{ 84,113,117,113,  19},
	{ 87,101,117,112,  19},
	{ 91, 83, 96,100,  19},
	{ 97, 90,102,100,  19},
	{103, 95,127,100,  19},
	{103, 85,115, 94,  19},
	{116, 83,126, 89,  19},
	{122, 83,126, 63,  19},
	{143,  1,192, 85,  21},
	{143, 85,192,170,  22},
	{143,170,192,254,  23},
	{  1,184,142,254,  24},
	{194,208,254,254,  25},
	{214,197,254,207,  25},
	{194,197,212,207,  26},
	{194,153,254,197,  26},
	{214,116,254,153,  27},
	{0,0,0,0,0}
};

static struct sector sector3[]={	
	{110,100,169,162,  20},	// palace
	{218,209,254,254,  58},	// wood
	{197,163,219,189,  60},	// astronomer's garden
	{179,101,227,160,  61},	// park
	{ 62,173,104,213,  62},	// graveyard
	{ 91, 93,254,254,  59},	// aston
	{0,0,0,0,0}
};

static struct sector sector4[]={	
	{1,1,254,254,63},
	{0,0,0,0,0}
};

static struct sector sector5[]={
	{ 55, 70,134,119,  29},		//sewer1
	{ 70,  1,134, 70,  30},		//sewer2
	{  1, 55, 55,119,  31},		//sewer3
	{  1,  1, 70, 55,  32},		//sewer4
	{134, 55,183,134,  33},		//sewer5
	{134,  1,198, 55,  34},		//sewer6
	{183, 70,254,134,  35},		//sewer7
	{198,  1,254, 70,  36},		//sewer8
	{121,136,200,185,  37},		//sewer9
	{121,185,185,254,  38},		//sewer10
	{200,136,254,200,  39},		//sewer11
	{185,200,254,254,  40},		//sewer12
	{ 72,121,121,200,  41},		//sewer13
	{ 57,200,121,254,  42},		//sewer14
	{  1,121, 72,185,  43},		//sewer15
	{  1,185, 57,254,  44},		//sewer16
	{0,0,0,0,0}
};

static struct sector sector6[]={
	{1,1,254,254,64},
	{0,0,0,0}
};

static struct sector sector7[]={
	{1,1,254,254,63},
	{0,0,0,0}
};

static struct sector sector8[]={
	{1,1,254,254,65},
	{0,0,0,0}
};

static struct sector sector9[]={
	{1,1,254,254,63},
	{0,0,0,0}
};

static struct sector sector10[]={
	{1,1,254,254,66},
	{0,0,0,0}
};

static struct sector sector11[]={
	{1,1,254,254,67},
	{0,0,0,0}
};

static struct sector sector12[]={
	{1,1,254,254,68},
	{0,0,0,0}
};

static struct sector sector13[]={
	{1,1,254,254,69},
	{0,0,0,0}
};

static struct sector sector14[]={
	{1,1,254,254,70},
	{0,0,0,0}
};

static struct sector sector15[]={
	{1,1,254,254,71},
	{0,0,0,0}
};

static struct sector sector16[]={
	{92,76,116,96,73},
	{42,229,86,255,74},
	{114,193,134,226,75},
	{1,1,254,254,72},
	{0,0,0,0}
};

static struct sector sector17[]={
	// robber, part I
	{110,  1,	129, 51,	76},
	{130, 11,	133, 51,	76},
	{134, 31,	137, 51,	76},
	{138, 47,	148, 51,	76},

	// robber, part II
	{113, 52,	133, 78,	77},
	{101, 62,	123, 81,	77},

	// robber, part III
	{134, 52,	170, 88,	78},
	{149, 47,	153, 51,	78},

	// robber, part IV (order is important)
	{132,  1,	175, 52,	79},

	// sewers
	{195,  1,	254, 99,	80},

	// below library
	{  1,191,	 37,254,	81},

	// governors palace
	{  1,  1,	 59, 37,	83},

	// skeleton place
	{ 38,191,	 82,254,	84},

	// spider place
	{ 83,191,	 160,254,	85},

	// zombie place
	{161,191,	 208,254,	86},

	// zombie place
	{209,222,	 225,254,	99},

	// exkordon (order!)
	{  1,  1,	194,100,	82},


	{0,0,0,0}
};

static struct sector sector18[]={
	{  1,  1, 85, 64,	87},
	{ 85,  1,170, 64,	88},
	{170,  1,255, 64,	89},

	{  1, 64, 85,128,	90},
	{ 85, 64,170,128,	91},
	{170, 64,255,128,	92},

	{  1,128, 85,192,	93},
	{ 85,128,170,192,	94},
	{170,128,255,192,	95},

	{  1,192, 85,255,	96},
	{ 85,192,170,255,	97},

	{172,222,188,254,	100},	// tower 2
	{188,222,204,254,	101},	// tower 3
	{204,222,220,254,	102},	// tower 4
	{220,222,236,254,	103},	// tower 5
	{236,222,252,254,	104},	// tower 6
	{172,206,204,222,	105},	// tower 7
	{204,206,236,222,	106},	// tower 8
	{172,194,236,205,	107},	// underground tower 9
	{237,194,254,221,	107},	// underground tower 9

	{170,192,255,255,	98},

	{0,0,0,0}
};

static struct sector sector19[]={
	{  1,  1,255,255,	108},
        {0,0,0,0}
};

static struct sector sector20[]={
	{230,233,255,255,	112},
	{  1,  1,255,255,	109},
        {0,0,0,0}
};

static struct sector sector21[]={
	{  1,  1,255,255,	110},
        {0,0,0,0}
};

static struct sector sector22[]={
	{224,199,255,255,	111},
	{134,141,223,254,	113},
	{134,  1,254,140,	114},
	{224,140,254,198,	114},
	{ 21,171,133,254,	115},
	{ 66,  1,133,114,	116},
        {0,0,0,0}
};

static struct sector sector23[]={
        {  1,  1,255,255,	117},
        {0,0,0,0}
};

static struct sector sector24[]={
        {  1,  1,255,255,	118},
        {0,0,0,0}
};

static struct sector sector25[]={
        {  211,132,254,254,	119},	// green
	{  179, 45,254,131,	120},	// orange
	{  158, 14,254, 44,	121},	// red
	{  212,  1,254, 44,	121},	// red
	{  157,  1,211, 13,	122},	// blue
	{  108,  1,157, 44,	122},	// blue
	{  108, 45,178,131,	122},	// blue
	{  108,132,211,254,	123},	// white
        {0,0,0,0}
};

static struct sector sector26[]={
        {    1,  1,165, 55,	124},	// dragons breath (rho)
        {0,0,0,0}
};

static struct sector sector27[]={
        {0,0,0,0}
};

static struct sector sector28[]={
        {0,0,0,0}
};

static struct sector sector29[]={
	{  37,112, 71,146,	125},	// tower top
	{  37,148, 71,182,	126},	// tower 1
	{  37,184, 71,218,	127},	// tower 2
	{  37,220, 71,254,	128},	// tower 3
	{   1,220, 35,254,	129},	// tower 4
	{   1,184, 35,218,	130},	// tower 5
	{   1,148, 35,182,	131},	// tower 6
	{   1,112, 35,146,	132},	// tower 7
	{   1, 76, 35,110,	133},	// tower 8

	{   1,  1, 24, 30,	134},	// brannington bar
	{   1, 32, 26, 43,	135},	// brannington castle
	{ 182,167,207,183,	135},	// brannington castle
	{ 172,152,243,185,	136},	// brannington
	{ 136,185,243,222,	136},	// brannington
	{ 181,222,220,232,	136},	// brannington

        {0,0,0,0}
};

static struct sector sector30[]={
        {0,0,0,0}
};

static struct sector sector31[]={
	{1,1,255,255,		137},	// grimroot
        {0,0,0,0}
};

static struct sector sector32[]={
	{1,1,255,255,		138},	// jobbington
        {0,0,0,0}
};

static struct sector sector33[]={
	{1,1,255,255,		139},	// long tunnel
        {0,0,0,0}
};

static struct sector sector34[]={
	{129,193,255,255,	140},	// teufelheim, slums
	{1,  193,128,255,	141},	// teufelheim, worker district
	{1,    1,128,192,	142},	// teufelheim, noble district
	{129,  1,255,192,	143},	// hell pents extension	
        {0,0,0,0}
};

static struct sector sector35[]={
	{0,0,0,0}
};

static struct sector sector36[]={
        {207,167,221,194,	146},
	{207,138,221,165,	147},
	{207,109,221,136,	148},
	{100,175,128,227,	149},
	{142,176,170,228,	150},
	{224,109,252,161,	151},
        {172,176,205,196,	152},
	{172,196,189,202,	152},
        {222,181,255,255,	153},
        {190,197,255,255,	153},
        {186,203,255,255,	153},
        {179,203,186,240,	153},
        {171,203,179,228,	153},

	{178,241,185,250,	154},
	{141,229,178,253,	154},
	{108,239,141,253,	154},

	{  1,  1,255,108,	144},
	{  1,109,255,255,	145},
	{0,0,0,0}
};

struct sector *area_sector[]={
	NULL,
	sector1,
	sector2,
	sector3,
	sector4,
	sector5,
	sector6,
	sector7,
	sector8,
	sector9,
	sector10,
	sector11,
	sector12,
	sector13,
	sector14,
	sector15,
	sector16,
	sector17,
	sector18,
	sector19,
	sector20,
	sector21,
	sector22,
	sector23,
	sector24,
	sector25,
	sector26,
	sector27,
	sector28,
	sector29,
	sector30,
	sector31,
	sector32,
	sector33,
	sector34,
	sector35,
	sector36
};

static short npc_in_section[TOTAL_MAXCHARS];

int get_section(int x,int y)
{
	struct sector *sec;
        int n;

	if (areaID>=ARRAYSIZE(area_sector)) return 0;

	if (!(sec=area_sector[areaID])) return 0;

	for (n=0; sec[n].fx; n++) {	
		if (x>=sec[n].fx && x<=sec[n].tx &&
		    y>=sec[n].fy && y<=sec[n].ty) break;
	}

	return sec[n].section;
}

char *get_section_name(int x,int y)
{
	struct sector *sec;
        int n;

	if (areaID>=ARRAYSIZE(area_sector)) return NULL;

	if (!(sec=area_sector[areaID])) return NULL;

	for (n=0; sec[n].fx; n++) {	
		if (x>=sec[n].fx && x<=sec[n].tx &&
		    y>=sec[n].fy && y<=sec[n].ty) break;
	}

	return section[sec[n].section].name;
}

void show_section(int x,int y,int cn)
{
        struct section *sec;
	int n,diff;
	
	if (!(n=get_section(x,y))) {
		log_char(cn,LOG_SYSTEM,0,"(%d,%d)",x,y);
		return;
	}

        sec=&section[n];

	//log_char(cn,LOG_SYSTEM,0,"If the following message seems wrong, please report these numbers to Ishtar: %d,%d - %d",x,y,n);

	if (!sec->level) {
		log_char(cn,LOG_SYSTEM,0,"%s. (%d,%d)",sec->name,x,y);
		return;
	}
	
	diff=sec->level-ch[cn].level;

	if (diff<-5) {
		log_char(cn,LOG_SYSTEM,0,"%s. This area is way too easy for you. (%d,%d)",sec->name,x,y);
		return;
	} else if (diff>5) {
		log_char(cn,LOG_SYSTEM,0,"%s. This area is way too dangerous for you. (%d,%d)",sec->name,x,y);
		return;
	}
	switch(diff) {
		case -5:	log_char(cn,LOG_SYSTEM,0,"%s. This area is too easy for you. (%d,%d)",sec->name,x,y); return;
		case -4:	log_char(cn,LOG_SYSTEM,0,"%s. This area is easy for you. (%d,%d)",sec->name,x,y); return;
		case -3:	log_char(cn,LOG_SYSTEM,0,"%s. This area is fairly easy for you. (%d,%d)",sec->name,x,y); return;
		case -2:	log_char(cn,LOG_SYSTEM,0,"%s. This area is rather easy for you. (%d,%d)",sec->name,x,y); return;
		case -1:	log_char(cn,LOG_SYSTEM,0,"%s. This area is just about right for you. (%d,%d)",sec->name,x,y); return;
		case 0:		log_char(cn,LOG_SYSTEM,0,"%s. This area is just right for you. (%d,%d)",sec->name,x,y); return;
		case 1:		log_char(cn,LOG_SYSTEM,0,"%s. This area is slightly dangerous for you. (%d,%d)",sec->name,x,y); return;
		case 2:		log_char(cn,LOG_SYSTEM,0,"%s. This area is a bit dangerous for you. (%d,%d)",sec->name,x,y); return;
		case 3:		log_char(cn,LOG_SYSTEM,0,"%s. This area is somewhat dangerous for you. (%d,%d)",sec->name,x,y); return;
		case 4:		log_char(cn,LOG_SYSTEM,0,"%s. This area is dangerous for you. (%d,%d)",sec->name,x,y); return;
		case 5:		log_char(cn,LOG_SYSTEM,0,"%s. This area is very dangerous for you. (%d,%d)",sec->name,x,y); return;
	}
}

void register_npc_to_section(int cn)
{
	int s;

	if (!(s=get_section(ch[cn].x,ch[cn].y))) return;
	if (s<1 || s>=MAXCHARS) return;

	npc_in_section[ch[cn].tmpa]=s;
}

struct section_data
{
	unsigned char npc[TOTAL_MAXCHARS/8];
	int current_section;
};

int register_kill_in_section(int cn,int co)
{
	struct section_data *dat;
	int t,s,index,bit,n,have=0,need=0,exp,diff,new;

        if (!(ch[cn].flags&CF_PLAYER)) return 0;

	if (!(dat=set_data(cn,DRD_SECTION_DATA,sizeof(struct section_data)))) return 0;	// OOPS

	t=ch[co].tmpa;
        if (t<1 || t>=MAXCHARS) return 0;

	index=t/8;
	bit=1<<(t&7);
	
	new=!(dat->npc[index]&bit);

	dat->npc[index]|=bit;

	s=npc_in_section[t];

	if (!section[s].level) return 0;

	for (n=1; n<MAXCHARS; n++) {
		if (npc_in_section[n]==s) {
			need++;

			index=n/8;
			bit=1<<(n&7);

			if (dat->npc[index]&bit) have++;			
		}
	}

	diff=need-have;
	exp=kill_score_level(section[s].level,ch[cn].level);
	if (new && exp && diff && ((diff%10)==0 || diff<6 || have==1)) {
		log_char(cn,LOG_SYSTEM,0,"%d done, %d to go.",have,diff);
	}

	if (have>=need) {
		for (n=1; n<MAXCHARS; n++) {
			if (npc_in_section[n]==s) {
                                index=n/8;
				bit=1<<(n&7);
	
				dat->npc[index]&=~bit;
			}
		}
                if (exp) {
			log_char(cn,LOG_SYSTEM,0,"Strike! You killed all %d enemies in this section. Good work!",need);
			give_exp_bonus(cn,(exp*need)/2);
		}
                return 1;
	}
	return 0;
}

void walk_section_msg(int cn)
{
        int s;
	struct section_data *dat;
	
	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(dat=set_data(cn,DRD_SECTION_DATA,sizeof(struct section_data)))) return;	// OOPS

	s=get_section(ch[cn].x,ch[cn].y);

	if (s!=dat->current_section) {
		
                if (s) {
			log_char(cn,LOG_SYSTEM,0,"°c1Now entering %s.",section[s].name);
		} else {
			log_char(cn,LOG_SYSTEM,0,"°c1Now leaving %s.",section[dat->current_section].name);
		}
		dat->current_section=s;
	}
}

void area_sound_wet_dungeon(int cn)
{
	int r;

        r=RANDOM(100);

        if (r==10 || r==20) player_special(cn,14,-(RANDOM(1000)+100),5000-RANDOM(10000));
	if (r==30 || r==40) player_special(cn,15,-(RANDOM(1000)+100),5000-RANDOM(10000));
	if (r==50 || r==60) player_special(cn,16,-(RANDOM(1000)+100),5000-RANDOM(10000));	
}

void area_sound_dry_dungeon(int cn)
{
	int r;

        r=RANDOM(100);

	if (r==10) player_special(cn,36,-(RANDOM(1000)+100),5000-RANDOM(10000));
	if (r==20) player_special(cn,37,-(RANDOM(1000)+100),5000-RANDOM(10000));	
	if (r==30) player_special(cn,38,-(RANDOM(1000)+100),5000-RANDOM(10000));
	if (r==40) player_special(cn,39,-(RANDOM(1000)+100),5000-RANDOM(10000));
	if (r==50) player_special(cn,40,-(RANDOM(1000)+100),5000-RANDOM(10000));
}

void area_sound_woods(int cn)
{
	int r;

        r=RANDOM(100);

	if (hour>6 && hour<22) {
		if (r==10) player_special(cn,10,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==20) player_special(cn,11,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==30) player_special(cn,12,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==40) player_special(cn,19,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==50) player_special(cn,20,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==60) player_special(cn,21,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==70) player_special(cn,22,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==80) player_special(cn,24,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==90) player_special(cn,25,-(RANDOM(1000)+100),5000-RANDOM(10000));
	} else {
		if (r==10) player_special(cn,17,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==20) player_special(cn,18,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==30) player_special(cn,26,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==40) player_special(cn,27,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==50) player_special(cn,28,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==60) player_special(cn,23,-(RANDOM(1000)+100),5000-RANDOM(10000));
	}
}

void area_sound_park(int cn)
{
	int r;

        r=RANDOM(100);

	if (hour>6 && hour<22) {
		if (r==10) player_special(cn,10,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==20) player_special(cn,11,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==30) player_special(cn,12,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==40) player_special(cn,19,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==50) player_special(cn,20,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==60) player_special(cn,21,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==70) player_special(cn,22,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==80) player_special(cn,24,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==90) player_special(cn,25,-(RANDOM(1000)+100),5000-RANDOM(10000));
	} else {
		if (r==30) player_special(cn,26,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==40) player_special(cn,27,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==50) player_special(cn,28,-(RANDOM(1000)+100),5000-RANDOM(10000));
		if (r==60) player_special(cn,23,-(RANDOM(1000)+100),5000-RANDOM(10000));
	}
}

void area_sound_underwater(int cn)
{
	int r;

        r=RANDOM(100);

	if (r==10) player_special(cn,47,-(RANDOM(1000)+100),5000-RANDOM(10000));
	if (r==20) player_special(cn,48,-(RANDOM(1000)+100),5000-RANDOM(10000));	
	if (r==30) player_special(cn,49,-(RANDOM(1000)+100),5000-RANDOM(10000));
}


void area_sound(int cn)
{
        switch(get_section(ch[cn].x,ch[cn].y)) {
		case 4:
		case 17:
		case 18:
		case 19:

		case 29:
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
		case 38:
		case 39:
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:

		case 46:
		case 47:
		case 48:
		case 50:	area_sound_wet_dungeon(cn); break;

		case 58:	area_sound_woods(cn); break;

		case 60:
		case 61:
		case 62:	area_sound_park(cn); break;
		
		case 63:
		case 64:
		case 65:
		case 66:	area_sound_dry_dungeon(cn); break;

		case 68:	
		case 69:
		case 70:	area_sound_wet_dungeon(cn); break;
		
		case 114:	area_sound_underwater(cn); break;

	}
}
