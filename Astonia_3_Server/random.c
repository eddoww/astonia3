/*
 * $Id: random.c,v 1.5 2007/08/13 18:50:38 devel Exp $
 *
 * $Log: random.c,v $
 * Revision 1.5  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.4  2006/12/19 15:19:56  devel
 * made welding shrine not overwrite description of christmas items
 *
 * Revision 1.3  2006/09/28 11:58:38  devel
 * added questlog
 *
 * Revision 1.2  2006/07/27 15:32:10  ssim
 * moved player bodies now use set_expire_body() instead of the generic function
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

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
#include "light.h"
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "database.h"
#include "mem.h"
#include "player.h"
#include "skill.h"
#include "expire.h"
#include "clan.h"
#include "chat.h"
#include "los.h"
#include "shrine.h"
#include "player_driver.h"
#include "sector.h"
#include "consistency.h"

int warrior_tab[]={
0,
1,      //level 1 (1)
2,      //level 2 (3)
3,      //level 3 (3)
4,      //level 4 (4)
5,      //level 5 (5)
7,      //level 6 (6)
9,      //level 7 (7)
10,     //level 8 (8)
11,     //level 9 (9)
13,     //level 10 (10)
15,     //level 11 (11)
17,     //level 12 (12)
18,     //level 13 (13)
20,     //level 14 (15)
21,     //level 15 (15)
22,     //level 16 (16)
24,     //level 17 (17)
25,     //level 18 (18)
27,     //level 19 (19)
28,     //level 20 (20)
30,     //level 21 (22)
31,     //level 22 (22)
32,     //level 23 (23)
33,     //level 24 (24)
35,     //level 25 (25)
36,     //level 26 (26)
37,     //level 27 (27)
39,     //level 28 (28)
40,     //level 29 (29)
41,     //level 30 (30)
42,     //level 31 (31)
43,     //level 32 (32)
45,     //level 33 (33)
46,     //level 34 (34)
47,     //level 35 (35)
48,     //level 36 (36)
50,     //level 37 (38)
51,     //level 38 (39)
52,     //level 39 (39)
53,     //level 40 (40)
54,     //level 41 (41)
55,     //level 42 (42)
56,     //level 43 (43)
58,     //level 44 (44)
59,     //level 45 (45)
60,     //level 46 (46)
61,     //level 47 (47)
62,     //level 48 (48)
63,     //level 49 (49)
64,     //level 50 (50)
65,     //level 51 (51)
67,     //level 52 (52)
68,     //level 53 (53)
69,     //level 54 (54)
70,     //level 55 (55)
71,     //level 56 (56)
72,     //level 57 (57)
73,     //level 58 (58)
74,     //level 59 (59)
75,     //level 60 (60)
76,     //level 61 (61)
78,     //level 62 (62)
79,     //level 63 (63)
80,     //level 64 (64)
81,     //level 65 (65)
82,     //level 66 (66)
83,     //level 67 (67)
84,     //level 68 (68)
85,     //level 69 (69)
86,     //level 70 (70)
87,     //level 71 (71)
89,     //level 72 (72)
90,     //level 73 (74)
91,     //level 74 (74)
92,     //level 75 (75)
93,     //level 76 (76)
94,     //level 77 (77)
95,     //level 78 (78)
96,     //level 79 (79)
97,     //level 80 (80)
98,     //level 81 (81)
99,     //level 82 (82)
100,    //level 83 (83)
101,    //level 84 (84)
102,    //level 85 (85)
104,    //level 86 (86)
105,    //level 87 (87)
106,    //level 88 (88)
107,    //level 89 (89)
108,    //level 90 (90)
109,    //level 91 (91)
110,    //level 92 (92)
111,    //level 93 (93)
112,    //level 94 (94)
113,    //level 95 (95)
114,    //level 96 (96)
115,    //level 97 (97)
117,    //level 98 (98)
118,    //level 99 (99)
120,    //level 100 (100)
121,    //level 101 (101)
123,    //level 102 (102)
125,    //level 103 (103)
127,    //level 104 (104)
129,    //level 105 (105)
131,    //level 106 (106)
134,    //level 107 (107)
138,    //level 108 (108)
143     //level 109 (109)
};

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
	{{"wizard",NULL},"The Wizard of Yendor was fascinated by the Labyrinth created by Ishtar and spent his life building an imitation of it. Before his death, he transfered all his magical energies into various °c4shrines°c0 he hid in his dungeons. With the Wizard no longer around, I take care of these °c4dungeons°c0.",0},
	{{"shrines",NULL},"Unfortunately, the Wizard had a peculiar sense of humor, and some of his shrines do strange things to those who use them. Do look at them before thou usest them, and decide carefully.",0},
	{{"yendor",NULL},"No one really knows where the name 'Yendor' stems from. But rumor has it that the Wizard's name used to be 'Rodney', before he changed it to 'The Wizard of Yendor'.",0},
	{{"dungeons",NULL},"The Wizard of Yendor created 99 different dungeons, but the first nine have collapsed. The remaining dungeons are of the levels 10 to 99. Thou canst ask me for °c4help°c0 if thou wishest to enter one of them.",0},
	{{"help",NULL},NULL,2},
	{{"list",NULL},NULL,3},
	{{"reset",NULL},NULL,4}
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

	if (!(ch[co].flags&(CF_PLAYER|CF_PLAYERLIKE))) return 0;

	//if (char_dist(cn,co)>16) return 0;

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
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}

struct cell
{
        int t,l;
	int v;
	
	int special;
};

static int xsize=15,ysize=15,maze_base=0,maze_level=0;
static int xoff=2,yoff=2;

int build_enemy(int x,int y,int level)
{
	int cn,n,val,base,in;
	//char buf[80];

	cn=create_char("enemy",0);
	if (level>35) ch[cn].value[1][V_RAGE]=1;

	if (level<1) level=1;
	if (level>109) level=109;
        base=warrior_tab[level];

        for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost) continue;
		if (!ch[cn].value[1][n]) continue;

		switch(n) {
			case V_HP:		val=max(10,base-15); break;
			case V_ENDURANCE:	val=max(10,base-15); break;
			
			case V_WIS:		val=max(10,base-25); break;
			case V_INT:		val=max(10,base-5); break;
			case V_AGI:		val=max(10,base-5); break;
			case V_STR:		val=max(10,base-5); break;

			case V_HAND:		val=max(1,base); break;
			case V_ARMORSKILL:	val=max(1,(base/10)*10); break;
			case V_ATTACK:		val=max(1,base); break;
			case V_PARRY:		val=max(1,base); break;
			case V_IMMUNITY:	val=max(1,base); break;
			
			case V_TACTICS:		val=max(1,base-5); break;
			case V_WARCRY:		val=max(1,base-15); break;
			case V_SURROUND:	val=max(1,base-20); break;
			case V_BODYCONTROL:	val=max(1,base-20); break;
			case V_SPEEDSKILL:	val=max(1,base-20); break;
			case V_PERCEPT:		val=max(1,base-10); break;
			case V_RAGE:		val=max(1,base-20); break;

			default:		val=max(1,base-30); break;
		}

		val=min(val,115);
		ch[cn].value[1][n]=val;
	}
        ch[cn].x=ch[cn].tmpx=x;
	ch[cn].y=ch[cn].tmpy=y;
	ch[cn].dir=DX_RIGHTDOWN;
	
	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);

	// give shrine key to NPC
	in=create_item("shrinekey");
	sprintf(it[in].name,"Jewel of Yendor (%d)",level);
	it[in].carried=cn;
	it[in].drdata[0]=level;
	ch[cn].item[30]=in;
	
        // create special equipment bonus to equal that of the average player
	in=create_item("equip1");
	
        for (n=0; n<5; n++) it[in].mod_value[n]=1+level/3;
//	for (n=0; n<5; n++) it[in].mod_value[n]=(level*level/125+level)/2-4;
	ch[cn].item[12]=in; it[in].carried=cn;

	in=create_item("equip2");
        for (n=0; n<4; n++) it[in].mod_value[n]=1+level/3;
	//for (n=0; n<4; n++) it[in].mod_value[n]=(level*level/125+level)/2-4;
	ch[cn].item[13]=in; it[in].carried=cn;

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

int build_junk(int x,int y,int level)
{
	int in;

	in=create_item("junk");
	it[in].sprite=15281+RANDOM(10);
	it[in].drdata[0]=level;

	if (!set_item_map(in,x+RANDOM(3),y+RANDOM(3))) { destroy_item(in); return 0; }

	return 1;
}

int build_shrine_continuity(int x,int y,int level)
{
	int in;

	in=create_item("shrine");
	strcpy(it[in].name,"Shrine of Continuity");
	it[in].sprite=59391;
	it[in].drdata[0]=255;
	it[in].drdata[1]=level;

	if (!set_item_map(in,x,y)) { destroy_item(in); return 0; }

	//xlog("Shrine of Continuity-shrine at %d,%d",x,y);

	return 1;
}

int build_shrine_sub(int x,int y,int level,int type,char *name,int sprite)
{
	int in;

	in=create_item("shrine");
	strcpy(it[in].name,name);
	it[in].sprite=sprite;
	it[in].drdata[0]=type;
	it[in].drdata[1]=level;

        if (!set_item_map(in,x,y)) { destroy_item(in); return 0; }

	//xlog("%s-shrine at %d,%d",name,x,y);

	return 1;
}

int build_shrine_death(int x,int y,int level)
{
	int in;

	in=create_item("shrine");
	strcpy(it[in].name,"Shrine of Death");
	it[in].sprite=59403;
	it[in].drdata[0]=51;	//type;
	it[in].drdata[1]=level;

	if (!set_item_map(in,x,y)) { destroy_item(in); return 0; }

	map[x-1+(y+1)*MAXMAP].fsprite=15279;
	map[x+1+(y-1)*MAXMAP].fsprite=15280;

	//xlog("death-shrine at %d,%d",x,y);

	return 1;
}

int build_gastrap1(int x,int y,int level)
{
	int in;

	in=create_item("gastrap");
	it[in].drdata[0]=level;
	if (!set_item_map(in,x+1,y)) {
		destroy_item(in);
	}
	
	in=create_item("gastrap");
	it[in].drdata[0]=level;
	if (!set_item_map(in,x+1,y+1)) {
		destroy_item(in);
	}
	
	map[x+1+y*MAXMAP].fsprite=15291;

	//xlog("trap1 at %d,%d",x,y);

	return 1;
}

int build_gastrap2(int x,int y,int level)
{
	int in;

	in=create_item("gastrap");
	it[in].drdata[0]=level;
	if (!set_item_map(in,x,y+1)) {
		destroy_item(in);
	}
	
	in=create_item("gastrap");
	it[in].drdata[0]=level;
	if (!set_item_map(in,x+1,y+1)) {
		destroy_item(in);
	}
	
	map[x+(y+1)*MAXMAP].fsprite=15300;

	//xlog("trap2 at %d,%d",x,y);

	return 1;
}

int build_trapdoor1(int x,int y,int level)
{
	int in;

	in=create_item("trapdoor");
        if (!set_item_map(in,x+1,y+1)) { destroy_item(in); return 0; }
	
        map[x+1+(y+0)*MAXMAP].flags|=MF_MOVEBLOCK;
	map[x+1+(y+2)*MAXMAP].flags|=MF_MOVEBLOCK;

	//xlog("trapdoor1 at %d,%d",x,y);

	return 1;
}

int build_trapdoor2(int x,int y,int level)
{
	int in;

	in=create_item("trapdoor2");
        if (!set_item_map(in,x+1,y+1)) { destroy_item(in); return 0; }
	
        map[x+0+(y+1)*MAXMAP].flags|=MF_MOVEBLOCK;
	map[x+2+(y+1)*MAXMAP].flags|=MF_MOVEBLOCK;

	//xlog("trapdoor2 at %d,%d",x,y);

	return 1;
}

void build_wall(int x,int y)
{
	int m;

	m=x+y*MAXMAP;

        map[m].flags=MF_INDOORS|MF_SIGHTBLOCK|MF_SOUNDBLOCK|MF_SHOUTBLOCK|MF_MOVEBLOCK;
	map[m].fsprite=15246+((x&3)+(y&3))%4;
	map[m].gsprite=0;
}

void build_empty(int x,int y)
{
	int m,in;

	m=x+y*MAXMAP;

	if ((in=map[m].it)) {
                //xlog("build_empty: destroying %s",it[in].name);
		remove_item_map(in);
		destroy_item(in);
	}

        map[m].flags=MF_INDOORS;
	map[m].fsprite=0;	
	map[m].gsprite=15237+(x%3)+(y%3)*3;
	map[m].dlight=0;
	map[m].light=0;
}

void build_remove(int x,int y)
{
	int m,cn,in,fn,n;

	m=x+y*MAXMAP;

	if ((cn=map[m].ch)) {
		if (ch[cn].flags&CF_PLAYER) {
			log_char(cn,LOG_SYSTEM,0,"The dungeon collapsed on you.");

			if (!teleport_char_driver(cn,245,250) &&
			    !teleport_char_driver(cn,240,250) &&
			    !teleport_char_driver(cn,235,250) &&
			    !teleport_char_driver(cn,230,250) &&
			    !change_area(cn,ch[cn].resta,ch[cn].restx,ch[cn].resty))
				exit_char(cn);
		} else {
			remove_destroy_char(cn);
		}
	}

	if ((in=map[m].it)) {
                if (it[in].flags&IF_PLAYERBODY) {
			remove_item_map(in);
			if (!drop_item(in,250,245) &&
			    !drop_item(in,250,240) &&
			    !drop_item(in,250,235) &&
			    !drop_item(in,250,230)) {
				dlog(0,in,"RD: Could not teleport body %s (%d). Destroyed.",it[in].name,in);
				destroy_item(in);				
			} else set_expire_body(in,PLAYER_BODY_DECAY_TIME);
		} else if (it[in].flags&IF_TAKE) {		
			//xlog("remove: destroying %s",it[in].name);
			remove_item_map(in);
			destroy_item(in);
		}
	}

	for (n=0; n<4; n++) {
		if ((fn=map[m].ef[n])) {
			remove_effect(fn);
			free_effect(fn);
		}
	}
}

void build_shrine(int x,int y,int level)
{
	//xlog("build shrine called with level %d",level);
	switch(level) {
		case 10:	build_shrine_sub(x,y,level,54,"Shrine of Security",59399); break;
		case 11:	build_shrine_sub(x,y,level,1,"Shrine of Indecisiveness",59392); break;
		case 12:        break;
		case 13:	build_shrine_sub(x,y,level,31,"Shrine of Living on the Edge",59395); break;
		case 14:	break;
		case 15:	build_shrine_sub(x,y,level,11,"Shrine of Bribes",59393); break;
		case 16:        break;
		case 17:        break;
		case 18:        build_shrine_sub(x,y,level,21,"Shrine of Welding",59394); break;
		case 19:        break;
		case 20:        build_shrine_sub(x,y,level,64,"Shrine of the Jobless",59400); break;
		case 21:        build_shrine_sub(x,y,level,41,"Shrine of Kindness",59396); break;
		case 22:        build_shrine_sub(x,y,level,12,"Shrine of Bribes",59393); break;
		case 23:        break;
		case 24:        build_shrine_sub(x,y,level,2,"Shrine of Indecisiveness",59392); break;
		case 25:        break;
		case 26:        build_shrine_sub(x,y,level,32,"Shrine of Living on the Edge",59395); break;
		case 27:        build_shrine_sub(x,y,level,22,"Shrine of Welding",59394); break;
		case 28:        break;
		case 29:        build_shrine_sub(x,y,level,55,"Shrine of Security",59399); break;
		case 30:        build_shrine_sub(x,y,level,50,"Shrine of Vitality",59397); break;
		case 31:        break;
		case 32:        build_shrine_sub(x,y,level,23,"Shrine of Welding",59394); break;
		case 33:        break;
		case 34:        build_shrine_sub(x,y,level,13,"Shrine of Bribes",59393); break;
		case 35:        break;
		case 36:        build_shrine_sub(x,y,level,33,"Shrine of Living on the Edge",59395); break;
		case 37:        build_shrine_death(x,y,level); break;
		case 38:        build_shrine_sub(x,y,level,3,"Shrine of Indecisiveness",59392); break;
		case 39:        break;
		case 40:        break;
		case 41:        build_shrine_sub(x,y,level,56,"Shrine of Security",59399); break;
		case 42:        build_shrine_sub(x,y,level,34,"Shrine of Living on the Edge",59395); break;
		case 43:        build_shrine_sub(x,y,level,4,"Shrine of Indecisiveness",59392); break;
		case 44:        break;
		case 45:        build_shrine_sub(x,y,level,65,"Shrine of the Jobless",59401); break;
		case 46:        build_shrine_sub(x,y,level,24,"Shrine of Welding",59394); break;
		case 47:        break;
		case 48:        build_shrine_sub(x,y,level,14,"Shrine of Bribes",59393); break;
		case 49:        break;
		case 50:        break;
		case 51:        build_shrine_sub(x,y,level,52,"Shrine of Braveness",59398); break;
		case 52:        build_shrine_sub(x,y,level,25,"Shrine of Welding",59394); break;
		case 53:        break;
		case 54:        build_shrine_sub(x,y,level,15,"Shrine of Bribes",59393); break;
		case 55:        build_shrine_sub(x,y,level,42,"Shrine of Kindness",59396); break;
		case 56:        break;
		case 57:        build_shrine_sub(x,y,level,5,"Shrine of Indecisiveness",59392); break;
		case 58:        build_shrine_sub(x,y,level,57,"Shrine of Security",59399); break;
		case 59:        build_shrine_sub(x,y,level,35,"Shrine of Living on the Edge",59395); break;
		case 60:        break;
		case 61:        build_shrine_sub(x,y,level,66,"Shrine of the Jobless",59401); break;
		case 62:        build_shrine_sub(x,y,level,26,"Shrine of Welding",59394); break;
		case 63:        break;
		case 64:        build_shrine_sub(x,y,level,6,"Shrine of Indecisiveness",59392); break;
		case 65:        break;
		case 66:        build_shrine_sub(x,y,level,36,"Shrine of Living on the Edge",59395); break;
		case 67:        build_shrine_sub(x,y,level,16,"Shrine of Bribes",59393); break;
		case 68:        break;
		case 69:        build_shrine_sub(x,y,level,58,"Shrine of Security",59399); break;
		case 70:        break;
		case 71:        break;
		case 72:        build_shrine_sub(x,y,level,27,"Shrine of Welding",59394); break;
		case 73:        break;
		case 74:        build_shrine_sub(x,y,level,37,"Shrine of Living on the Edge",59395); break;
		case 75:        build_shrine_sub(x,y,level,59,"Shrine of Security",59399); break;
		case 76:        build_shrine_sub(x,y,level,7,"Shrine of Indecisiveness",59392); break;
		case 77:        break;
		case 78:        build_shrine_sub(x,y,level,17,"Shrine of Bribes",59393); break;
		case 79:        break;
		case 80:        break;
		case 81:        build_shrine_sub(x,y,level,28,"Shrine of Welding",59394); break;
		case 82:        build_shrine_sub(x,y,level,67,"Shrine of the Jobless",59401); break;
		case 83:        build_shrine_sub(x,y,level,8,"Shrine of Indecisiveness",59392); break;
		case 84:        break;
		case 85:        build_shrine_sub(x,y,level,60,"Shrine of Security",59399); break;
		case 86:        build_shrine_sub(x,y,level,18,"Shrine of Bribes",59393); break;
		case 87:        break;
		case 88:        build_shrine_sub(x,y,level,38,"Shrine of Living on the Edge",59395); break;
		case 89:        break;
		case 90:        break;
		case 91:        build_shrine_sub(x,y,level,39,"Shrine of Living on the Edge",59395); break;
		case 92:        break;
		case 93:        build_shrine_sub(x,y,level,19,"Shrine of Bribes",59393); break;
		case 94:        build_shrine_sub(x,y,level,61,"Shrine of Security",59399); break;
		case 95:        break;
		case 96:        build_shrine_sub(x,y,level,9,"Shrine of Indecisiveness",59392); break;
		case 97:        build_shrine_sub(x,y,level,68,"Shrine of the Jobless",59401); break;
		case 98:        build_shrine_sub(x,y,level,29,"Shrine of Welding",59394); break;
		case 99:	break;
	}
}

void build_cell(int cx,int cy,struct cell *cell)
{
        build_wall(cx*4+xoff,cy*4+yoff);
	if (cell->t) {
		build_wall(cx*4+1+xoff,cy*4+yoff);
		build_wall(cx*4+2+xoff,cy*4+yoff);
		build_wall(cx*4+3+xoff,cy*4+yoff);
	}
	if (cell->l) {
		build_wall(cx*4+xoff,cy*4+1+yoff);
		build_wall(cx*4+xoff,cy*4+2+yoff);
		build_wall(cx*4+xoff,cy*4+3+yoff);
	}	

	switch(cell->special) {
		case 3:		build_enemy(cx*4+xoff+2,cy*4+yoff+2,maze_level); break;
		case 4:		build_junk(cx*4+xoff+1,cy*4+yoff+1,maze_level); break;
		case 5:		build_gastrap1(cx*4+xoff+1,cy*4+yoff+1,maze_level); break;
		case 6:		build_gastrap2(cx*4+xoff+1,cy*4+yoff+1,maze_level); break;
		case 7:		build_trapdoor1(cx*4+xoff+1,cy*4+yoff+1,maze_level); break;
		case 8:		build_trapdoor2(cx*4+xoff+1,cy*4+yoff+1,maze_level); break;
		case 9:		build_shrine_continuity(cx*4+xoff+2,cy*4+yoff+2,maze_level); break;
		case 10:	build_shrine(cx*4+xoff+2,cy*4+yoff+2,maze_level); break;

	}
}

int special_fill(struct cell *cell,int nr,int val)
{
	int tmp,best=999;

	if (cell[nr].special==999) return val;
	
	if (cell[nr].special && cell[nr].special<=val) return 0;

	cell[nr].special=val;
	if (!cell[nr].l && nr%xsize>0) if ((tmp=special_fill(cell,nr-1,val+1))) best=min(tmp,best);
	if (!cell[nr].t && nr/xsize>0) if ((tmp=special_fill(cell,nr-xsize,val+1))) best=min(tmp,best);

	if (nr%xsize<xsize-1 && !cell[nr+1].l) if ((tmp=special_fill(cell,nr+1,val+1))) best=min(tmp,best);
	if (nr/xsize<ysize-1 && !cell[nr+xsize].t) if ((tmp=special_fill(cell,nr+xsize,val+1))) best=min(tmp,best);

	return best;
}

void build_maze(struct cell *cell)
{
	int *stack,n,m,visited=0,ptr,opt;

        stack=xcalloc(sizeof(int)*xsize*ysize,IM_TEMP); ptr=0;

	for (m=0; m<xsize*ysize; m++) {
		cell[m].t=1;
		cell[m].l=1;
	}
	stack[ptr++]=RANDOM(m);
	
	while (visited<xsize*ysize) {
		if (ptr==0) { elog("ran out of stack (%d of %d)",visited,xsize*ysize); break; }
                n=stack[--ptr];

                repeat:
		//xlog("processing cell at %d,%d",n%xsize,n/xsize);

                opt=0;
		if (n%xsize>0 && !cell[n-1].v) opt++;
		if (n%xsize<xsize-1 && !cell[n+1].v) opt++;

		if (n/xsize>0 && !cell[n-xsize].v) opt++;
		if (n/xsize<ysize-1 && !cell[n+xsize].v) opt++;

                if (!opt) continue;

                if (opt>1) {
			stack[ptr++]=n;
		}

		opt=RANDOM(opt);

		if (n%xsize>0 && !cell[n-1].v) {
			if (opt) { opt--; }
			else {				
				cell[n].l=0;
                                n=n-1;
				cell[n].v=1; visited++;
                                goto repeat;
			}
		}
		if (n%xsize<xsize-1 && !cell[n+1].v) {
			if (opt) { opt--; }
			else {
                                n=n+1;
				cell[n].l=0;
				cell[n].v=1; visited++;
                                goto repeat;
			}
		}

		if (n/xsize>0 && !cell[n-xsize].v) {
			if (opt) { opt--; }
			else {
                                cell[n].t=0;
				n=n-xsize;
				cell[n].v=1; visited++;
                                goto repeat;
			}
		}
		if (n/xsize<ysize-1 && !cell[n+xsize].v) {
			if (opt) { opt--; }
			else {
                                n=n+xsize;
				cell[n].t=0;
				cell[n].v=1; visited++;
                                goto repeat;
			}
		}
		elog("not reached");
	}

	xfree(stack);
}

void show_maze(struct cell *cell)
{
        int x,y;

	for (y=0; y<xsize; y++) {
		for (x=0; x<xsize; x++) {
			if (cell[x+y*xsize].t && cell[x+y*xsize].l)        printf("!===");
			else if (cell[x+y*xsize].t && !cell[x+y*xsize].l)  printf("====");
			else if (!cell[x+y*xsize].t && cell[x+y*xsize].l)  printf("!   ");
			else if (!cell[x+y*xsize].t && !cell[x+y*xsize].l) printf("    ");
		}
		printf("!\n");
		for (x=0; x<xsize; x++) {
			if (cell[x+y*xsize].l)        printf("!%03d",cell[x+y*xsize].special);
			else if (!cell[x+y*xsize].l)  printf(" %03d",cell[x+y*xsize].special);
		}
		printf("!\n");
	}
	for (x=0; x<xsize; x++) printf("====");
	printf("!\n");	
}

int create_maze(int base,int level,int show)
{
	int m,maxi,panic=5000;
	struct cell *cell;

	maze_base=base; maze_level=level;
	srand(base);

	cell=xcalloc(sizeof(struct cell)*xsize*ysize,IM_TEMP);

	build_maze(cell);

	// shrine of continuity
	while (panic--) {
		m=RANDOM(xsize*ysize);
		if (m/xsize>ysize/2) continue;	// force to lower half
		if (m%xsize<xsize/2) continue;	// force to right half
		if (cell[m].special) continue;

		if (cell[m].t && cell[m].l && cell[m+xsize].t) { cell[m].special=9; break; }
	}
	if (panic<0) { xfree(cell); return 0; }

	// level-specific shrine
	while (panic--) {
		m=RANDOM(xsize*ysize);
		if (m/xsize>ysize/2) continue;	// force to lower half
		if (m%xsize<xsize/2) continue;	// force to right half
		if (cell[m].special) continue;

		if (cell[m].t && cell[m].l && cell[m+xsize].t) { cell[m].special=10; break; }
	}
	if (panic<0) { xfree(cell); return 0; }

	// junk piles
	maxi=15;
	while (maxi>0 && panic--) {
		m=RANDOM(xsize*ysize);
		if (m==xsize*ysize-ysize) continue;
                if (cell[m].special) continue;
		cell[m].special=4;
		maxi--;
	}
	if (panic<0) { xfree(cell); return 0; }

	// gas traps
	maxi=10;
	while (maxi>0 && panic--) {
		m=RANDOM(xsize*ysize);
		if (m==xsize*ysize-ysize) continue;
		if (cell[m].special) continue;
		
		if (cell[m].t) cell[m].special=5;
		else if (cell[m].l) cell[m].special=6;
		else continue;

		maxi--;
	}
	if (panic<0) { xfree(cell); return 0; }

	// trapdoors
	maxi=5;
	while (maxi>0 && panic--) {
		m=RANDOM(xsize*ysize);
		if (m==xsize*ysize-ysize) continue;
		if (cell[m].special) continue;
		if (m%xsize>xsize-2) continue;
		if (m%xsize<3) continue;
		if (m/xsize>ysize-2) continue;
		if (m/xsize<3) continue;

		if (cell[m].t && !cell[m].l && cell[m+xsize].t && !cell[m+xsize].l) cell[m].special=7;
		else if (!cell[m].t && cell[m].l && !cell[m+1].t && cell[m+1].l) cell[m].special=8;
		else continue;

		maxi--;
	}
	if (panic<0) { xfree(cell); return 0; }

	// enemies
        maxi=50;
        while (maxi>0 && panic--) {
		m=RANDOM(xsize*ysize);
		if (m==xsize*ysize-ysize) continue;
		if (cell[m].special) continue;
		cell[m].special=3;
		maxi--;
	}
	if (panic<0) { xfree(cell); return 0; }

        for (m=0; m<xsize*ysize; m++) build_cell(m%xsize,m/xsize,cell+m);		

	if (show) show_maze(cell);

        xfree(cell);

	return 1;
}

void destroy_dungeon(int nr)
{
	int xf,yf,xt,yt,x,y;

	//xlog("destroying dungeon %d",nr);

	xf=(nr%4)*61+2; xt=xf+60;
	yf=(nr/4)*61+2; yt=yf+60;

	for (x=xf; x<xt; x++) {
		for (y=yf; y<yt; y++) {
			build_remove(x,y);
		}
	}

	for (x=xf; x<xt; x++) {
		for (y=yf; y<yt; y++) {
			build_empty(x,y);
		}
	}
}

#define DUNGEONTIME	(TICKS*60*30)

struct master_data
{
        int level[16];
	int created[16];
	int warning[16];
	int owner[16];
	int ownerlevel[16];

	int memcleartimer;
};

void create_dungeon(int cn,int co,int level,struct master_data *dat)
{
        int n,best=0,bestn=0,tmp,free_d=0;

	if (level<10 || level>99) {
		say(cn,"Sorry, only level 10 to 99 are accessible.");
		return;
	}

	for (n=0; n<16; n++) {
                if (dat->owner[n]==ch[co].ID) {
			say(cn,"You have created a dungeon already, you may not create another one before the first one has collapsed. You can use °c4destroy %d°c0 to collapse it now.",n+1);
			return;
		}
	}

	for (n=0; n<16; n++) {
                if (dat->created[n]) tmp=ticker-dat->created[n]; else tmp=max(DUNGEONTIME,ticker-dat->created[n]);
		if (tmp>best) { best=tmp; bestn=n; }		
		if (!dat->created[n]) free_d++;		
	}
	if (!(ch[co].flags&CF_PAID) && free_d<3) {
		say(cn,"Sorry, the remaining dungeons are reserved for paying players.");
		return;
	}
	if (best<DUNGEONTIME) {
		say(cn,"Sorry, all dungeons are busy. Please try again in %.2f minutes",(DUNGEONTIME-best)/(TICKS*60.0));
		return;
	}

	xoff=(bestn%4)*61+2;
	yoff=(bestn/4)*61+2;

	dat->level[bestn]=level;
        dat->created[bestn]=ticker;
	dat->warning[bestn]=0;
	dat->owner[bestn]=ch[co].ID;
	dat->ownerlevel[bestn]=ch[co].level;

        do {
		destroy_dungeon(bestn);
	} while (!create_maze(RANDOM(12345678),level,0));

	say(cn,"This dungeon will collapse in %.2f minutes.",DUNGEONTIME/(TICKS*60.0));

	teleport_char_driver(co,xoff+2,yoff+58);
}

void enter_dungeon(int cn,int co,int target,struct master_data *dat)
{
	int tmp;

	if (target<1 || target>16) {
		say(cn,"Sorry, the target is out of bounds.");
		return;
	}
	target--;

	if (!dat->created[target]) {
		say(cn,"Sorry, this dungeon does not exist.");
		return;
	}
	
        tmp=DUNGEONTIME-ticker+dat->created[target];
	if (tmp<TICKS*60) {
		say(cn,"Sorry, this dungeon is about to collapse.");
		return;
	}

	if (abs(dat->ownerlevel[target]-ch[co].level)>3) {
		say(cn,"Sorry, this dungeon was created for levels %d to %d.",dat->ownerlevel[target]-3,dat->ownerlevel[target]+3);
		return;
	}
	say(cn,"This dungeon will collapse in %.2f minutes,",tmp/(TICKS*60.0));	

	teleport_char_driver(co,(target%4)*61+4,(target/4)*61+60);
}

void list_dungeon(int cn,struct master_data *dat)
{
	int n,flag=0;

	for (n=0; n<16; n++) {
		if (dat->level[n]) {
			say(cn,"Dungeon %d: Level %d (%d to %d), remaining time: %.2f minutes.",
			    n+1,
			    dat->level[n],
			    dat->ownerlevel[n]-3,dat->ownerlevel[n]+3,
			    (DUNGEONTIME-ticker+dat->created[n])/(TICKS*60.0));
			flag=1;
		}
	}
	if (!flag) {
		say(cn,"No dungeons.");
	}
}

void warn_dungeon(int nr,int left)
{
	int co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].flags&CF_PLAYER) {
			if (((ch[co].x-2)/61)+((ch[co].y-2)/61)*4==nr) {
				log_char(co,LOG_SYSTEM,0,"This dungeon will collapse in %.2f minutes.",left/(TICKS*60.0));
			}
		}		
	}
}

void randommaster(int cn,int ret,int lastact)
{
	struct master_data *dat;
        int co,target,n,tmp;
	struct msg *msg,*next;
	char *ptr,*haystack;

        dat=set_data(cn,DRD_RANDOMMASTER,sizeof(struct master_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		/*if (msg->type==NT_CREATE) {
			int wl,l;
			wl=1;
			for (n=1; n<200; n++) {
				l=build_enemy(n,10,n);
				if (l>=wl) {
					printf("%d,\t//level %d (%d)\n",n,wl,l);
					wl++;
				}
			}
		}*/

                // did we see someone?
		if (msg->type==NT_CHAR) {
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			say(cn,"Hello %s! Welcome to the dungeons of the °c4Wizard°c0 of °c4Yendor°c0!",ch[co].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			
			switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		say(cn,"Use: 'create <nr>' to create a dungeon of level <nr>, 'enter <nr>' to enter dungeon <nr>, 'list' to get a listing of all dungeons or 'destroy <nr>' to destroy the dungeon <nr> you have created.");
						break;
				case 3:		list_dungeon(cn,dat);
						break;
				case 4:		if (ch[co].flags&CF_GOD) del_data(co,DRD_RANDOMSHRINE_PPD);
						break;
			}

			for (haystack=(char*)msg->dat2; *haystack && *haystack!=':'; haystack++) ;
			if (*haystack==':') haystack++;

			//say(cn,"got: %s",haystack);

			if ((ptr=strcasestr(haystack,"create"))) {
				ptr+=6;
				target=atoi(ptr);
                                create_dungeon(cn,co,target,dat);
			}
			if ((ptr=strcasestr(haystack,"enter"))) {
                                ptr+=5;
				target=atoi(ptr);
				say(cn,"You want to enter dungeon %d",target);

				enter_dungeon(cn,co,target,dat);
			}

			if ((ptr=strcasestr(haystack,"destroy"))) {
                                ptr+=7;
				target=atoi(ptr);
				say(cn,"You want to destroy dungeon %d",target);

				if (target>0 && target<17) {
					target--;
					if (dat->owner[target]==ch[co].ID) {
						destroy_dungeon(target);
						dat->level[target]=0;
						dat->created[target]=0;
						dat->warning[target]=0;
						dat->owner[target]=0;
						say(cn,"Done.");
					} else say(cn,"Sorry, you did not create that dungeon, hence you may not destroy it.");
				} else say(cn,"Sorry, that number is out of bounds.");
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        // let it vanish, then
			destroy_item(ch[cn].citem);
			ch[cn].citem=0;			
		}

		if (msg->type==NT_NPC && msg->dat1==NTID_DUNGEON) {
                        destroy_dungeon(msg->dat2);
                        dat->level[msg->dat2]=0;
			dat->created[msg->dat2]=0;
			dat->warning[msg->dat2]=0;
			dat->owner[msg->dat2]=0;
		}

                remove_message(cn,msg);
	}
        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (spell_self_driver(cn)) return;

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

	for (n=0; n<16; n++) {
		if (!dat->created[n]) continue;
		
		tmp=ticker-dat->created[n];
		
		if (tmp>DUNGEONTIME) {
			destroy_dungeon(n);
                        dat->level[n]=0;
			dat->created[n]=0;
			dat->warning[n]=0;
			dat->owner[n]=0;
		}
		
		if (tmp>dat->warning[n]) {
			warn_dungeon(n,DUNGEONTIME-tmp);
			dat->warning[n]+=TICKS*60*5;			
		}
	}

	if (ticker>dat->memcleartimer) {
		mem_erase_driver(cn,7);
		dat->memcleartimer=ticker+TICKS*60*60*12;
	}


        do_idle(cn,TICKS);
}

void immortal_dead(int cn,int co)
{
	charlog(cn,"I JUST DIED! I'M SUPPOSED TO BE IMMORTAL!");
}

int shrine_check(int cn,int nr,struct shrine_ppd *ppd)
{
	int i,bit;

	if (nr<0 || nr>=MAXSHRINE) {
		log_char(cn,LOG_SYSTEM,0,"You have found bug #2116a.");
		return 0;
	}
	i=nr/32;
	bit=1u<<(nr&31);
	if (ppd->used[i]&bit) {
		log_char(cn,LOG_SYSTEM,0,"The magic of this place will only work once.");
		return 0;
	}
	return 1;
}

void shrine_set(int nr,struct shrine_ppd *ppd)
{
	int i,bit;
	
	if (nr<0 || nr>=MAXSHRINE) {
		elog("Bug #2116c.");
		return;
	}

	i=nr/32;
	bit=1u<<(nr&31);

	ppd->used[i]|=bit;
}

void shrine_indecisiveness(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int n;

        for (n=0; n<V_MAX; n++) {
		if (ch[cn].value[1][n]>skillmax(cn)) continue;
		
                lower_value(cn,n); lower_value(cn,n); lower_value(cn,n);
	}
	dlog(cn,0,"Used shrine of indecisiveness (lvl %d)",level);
	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_bribes(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int val,want,need;

	level=min(ch[cn].level+5,level);	// avoid abuse

        want=level_value(level)*4/3;
	need=level_value(level)*4/10;

	val=ch[cn].gold;
	if (val<need) {
		log_char(cn,LOG_SYSTEM,0,"You feel a hand reach into your pocket and touch your purse. A second later, it is removed with a sneer.");
		return;
	}
        log_char(cn,LOG_SYSTEM,0,"You feel a hand reach into your pocket and touch your purse. Shocked, you reach for your purse and find it %sempty.",want<val ? "almost " : "");
	
	val=min(val,want);

	ch[cn].gold-=val;
	ch[cn].flags|=CF_ITEMS;

	give_exp_bonus(cn,val/4);
	dlog(cn,0,"Used shrine of bribes (%d G, %d exp (%d,%d), lvl %d)",val/100,val/4,level,level_value(level),level);

	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

int can_receive_mod(int in,int *pslot,int v)
{
	int n,cnt=0;

	// no special items
        if (it[in].driver) return 0;
	if (it[in].ID!=IID_GENERIC_SPECIAL && it[in].ID!=IID_HARDKILL && it[in].ID!=0) return 0;
	if (it[in].flags&IF_NOENHANCE) return 0;

	for (n=0; n<MAXMOD; n++) {
		if (it[in].mod_value[n] && it[in].mod_index[n]==v) {
                        return 0;
		}
		switch(it[in].mod_index[n]) {
			case V_WEAPON:	break;
			case V_ARMOR:	break;
                        case V_DEMON:	break;
			case V_LIGHT:	break;
			default:        if (it[in].mod_value[n]>0 && it[in].mod_index[n]>=0) cnt++;
		}
	}
	if (cnt>2) return 0;	

	for (n=0; n<MAXMOD; n++) {
		if (!it[in].mod_value[n]) {
			if (pslot) *pslot=n;
			return in;
		}
	}
	return 0;
}

int can_give_mod(int in,int slot)
{
	// no special items
	if (it[in].driver) return 0;
	if (it[in].ID!=IID_GENERIC_SPECIAL && it[in].ID!=IID_HARDKILL && it[in].ID!=0) return 0;
	if (it[in].flags&IF_NOENHANCE) return 0;

        if (!it[in].mod_value[slot]) return 0;
	if (it[in].mod_index[slot]<0) return 0;
		
	switch(it[in].mod_index[slot]) {
		case V_WEAPON:	return 0;
		case V_ARMOR:	return 0;
		case V_SPEED:	return 0;
		case V_DEMON:	return 0;
		case V_LIGHT:	return 0;
		default:        return in;
	}
	return 0;
}

void shrine_welding(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int in1=0,in2=0,tmp,slot1,slot2=-1,cnt;
	int n,m;

	if (ch[cn].level+ch[cn].level/4+2<level) {
		log_char(cn,LOG_SYSTEM,0,"You are not powerful enough to use this shrine.");
		return;
	}

	if (!(ch[cn].flags&CF_PAID)) {
		log_char(cn,LOG_SYSTEM,0,"Only paying players can use this shrine.");
		return;
	}
	
	for (n=cnt=0; n<12; n++) {
		for (m=0; m<MAXMOD; m++) {
			if ((tmp=ch[cn].item[n]) && can_give_mod(tmp,m)) cnt++;
		}
	}
	if (!cnt) {
		log_char(cn,LOG_SYSTEM,0,"You feel a cold hand touch your equipment. After it has touched all your items, it leaves with a laugh of contempt.");
		return;
	}

        cnt=RANDOM(cnt);

	for (n=0; n<12; n++) {
		for (m=0; m<MAXMOD; m++) {
			if ((tmp=ch[cn].item[n]) && (in2=can_give_mod(tmp,m)) && !cnt--) { slot2=m; break; }
		}
		if (cnt<0) break;		
	}

	if (slot2==-1) {
		log_char(cn,LOG_SYSTEM,0,"You found bug #337 (%d %d %d)",n,m,cnt);
		return;
	}

        for (n=cnt=0; n<12; n++) {
		if ((tmp=ch[cn].item[n]) && tmp!=in2 && can_receive_mod(tmp,&slot1,it[in2].mod_index[slot2])) cnt++;
	}
	if (!cnt) {
		log_char(cn,LOG_SYSTEM,0,"You feel a cold hand touch your equipment. After it has touched all your items, it leaves with a laugh of regret.");
		return;
	}
	//xlog("cnt=%d",cnt);
        cnt=RANDOM(cnt);

	for (n=0; n<12; n++) {
		if ((tmp=ch[cn].item[n]) && tmp!=in2 && (in1=can_receive_mod(tmp,&slot1,it[in2].mod_index[slot2])) && !cnt--) break;
	}

        dlog(cn,in1,"Used shrine of welding (lvl %d): Welding %s (%d) with %s (%d) (%s) (1)",level,it[in1].name,in1,it[in2].name,in2,skill[it[in2].mod_index[slot2]].name);
	dlog(cn,in2,"Used shrine of welding (lvl %d): Welding %s (%d) with %s (%d) (%s) (2)",level,it[in1].name,in1,it[in2].name,in2,skill[it[in2].mod_index[slot2]].name);
	
	log_char(cn,LOG_SYSTEM,0,"You feel a burning hand touch your %s and your %s.",it[in1].name,it[in2].name);
	
	it[in1].mod_index[slot1]=it[in2].mod_index[slot2];
	it[in1].mod_value[slot1]=it[in2].mod_value[slot2];

	set_item_requirements(in1);
	if (!strstr(it[in1].description,"Christmas"))
		snprintf(it[in1].description,sizeof(it[in1].description)-1,"%s of Welding.",it[in1].name); it[in1].description[sizeof(it[in1].description)-1]=0;

	it[in2].mod_index[slot2]=0;
	it[in2].mod_value[slot2]=0;
	
	set_item_requirements(in2);
	if (!strstr(it[in2].description,"Christmas"))
		snprintf(it[in2].description,sizeof(it[in2].description)-1,"%s of Unwelding.",it[in2].name); it[in2].description[sizeof(it[in2].description)-1]=0;

	ch[cn].flags|=CF_ITEMS;

	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_edge(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int bonus;

	if (!ch[cn].saves) {
		log_char(cn,LOG_SYSTEM,0,"A booming voice declares: 'Thou art living on the edge already!'");
		return;
	}

	level=min(ch[cn].level+5,level);	// avoid abuse

	bonus=level_value(level)/3+ch[cn].saves*level_value(level)/30;

	dlog(cn,0,"Used shrine of living on the edge (%d saves, %d exp (%d,%d) (lvl %d))",ch[cn].saves,bonus,level,level_value(level),level);

        give_exp_bonus(cn,bonus);
	ch[cn].saves=0;

	log_char(cn,LOG_SYSTEM,0,"A booming voice declares: 'Living on the edge has its merits - and its dangers!'");
	log_char(cn,LOG_SYSTEM,0,"Thou hast no saves left.");

	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_kindness(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	if (!(ch[cn].flags&CF_PK)){
		log_char(cn,LOG_SYSTEM,0,"A tender voice whispers: 'But thou art a kind soul already...'");
		return;
	}
	dlog(cn,0,"Used shrine of kindness (lvl %d)",level);

	ch[cn].flags&=~CF_PK;
	del_data(cn,DRD_PK_PPD);	// remove unneeded data block
	reset_name(cn);

	log_char(cn,LOG_SYSTEM,0,"A tender voice whispers: 'Mayest thou find other ways to amuse thyself. Thou art not a killer henceforth.'");
	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_vitality(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int cost,n,v,s,cnt;

	if (ch[cn].flags&CF_WARRIOR) {
		v=V_HP;
		if (ch[cn].flags&CF_MAGE) s=1;
		else s=0;
	} else { v=V_MANA; s=0; }

	if (s) cnt=min(5,100-ch[cn].value[1][v]);
	else cnt=min(5,115-ch[cn].value[1][v]);

	if (cnt<1) {
		log_char(cn,LOG_SYSTEM,0,"A lively voice says: 'Thou canst improve thine vitality any more.'");
		return;
	}

	for (n=cost=0; n<cnt; n++)
		cost+=raise_cost(v,ch[cn].value[1][v]+n,s);

	dlog(cn,0,"Used shrine of vitality (%d %s, %d exp, lvl %d)",cnt,skill[v].name,cost,level);
	
        ch[cn].value[1][v]+=cnt;
	ch[cn].exp_used+=cost;
	give_exp(cn,cost);
	update_char(cn);

	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_continuity(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int cost;

	if (ppd->continuity<10) ppd->continuity=10;

	if (level<ppd->continuity) {
		if (level==99) {
			teleport_char_driver(cn,41,250);
			log_char(cn,LOG_SYSTEM,0,"Thy continuity has opened a gate...");
		} else log_char(cn,LOG_SYSTEM,0,"A steady voice says: 'Thou hast visited me already.'");
		return;
	}
	if (level>ppd->continuity) {
		log_char(cn,LOG_SYSTEM,0,"A steady voice says: 'Thou must visit mine younger brother first.'");
		return;
	}

	log_char(cn,LOG_SYSTEM,0,"A steady voice says: 'Continuity is power.'");
	ppd->continuity=level+1;

        cost=level_value(min(ch[cn].level+5,level))/6;
	dlog(cn,0,"Used shrine of continuity (lvl %d, %d exp (%d,%d))",level,cost,level,level_value(level));

	give_exp_bonus(cn,cost);

	if (level==99) {
		teleport_char_driver(cn,41,250);
		log_char(cn,LOG_SYSTEM,0,"Thy continuity has opened a gate...");
	}
	sendquestlog(cn,ch[cn].player);
}

void shrine_death(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	ch[cn].saves=0;
	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);

	dlog(cn,0,"Used shrine of death (lvl %d)",level);
	
        log_char(cn,LOG_SYSTEM,0,"You hear a manical laugh.");
	kill_char(cn,0);
}

void shrine_braveness(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int cost;

	if (!(ppd->used[DEATH_SHRINE_INDEX]&DEATH_SHRINE_BIT)) {
		log_char(cn,LOG_SYSTEM,0,"An insulting voice says: 'Thou art a coward, bother me not!");
		return;
	}

	level=min(ch[cn].level+5,level);	// avoid abuse

	cost=level_value(level);

	dlog(cn,0,"Used shrine of braveness (%d exp, %d gold (%d,%d), lvl %d)",cost,cost/10,level,level_value(level),level);

	log_char(cn,LOG_SYSTEM,0,"A triumphant voice says: 'Thou art brave indeed!'");

	give_exp_bonus(cn,cost);
	ch[cn].gold+=cost/10;
	ch[cn].flags|=CF_ITEMS;

	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_security(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	if (ch[cn].saves>5) {
		log_char(cn,LOG_SYSTEM,0,"A scared voice whispers: 'Thou art secure already.'");
		return;
	}
	if (ch[cn].flags&CF_HARDCORE) {
		log_char(cn,LOG_SYSTEM,0,"A scared voice whispers: 'Thou wilt never be secure.'");
		return;
	}
	log_char(cn,LOG_SYSTEM,0,"A scared voice whispers: 'Thou shalt be secure.'");

	ch[cn].saves++;
	log_char(cn,LOG_SYSTEM,0,"Thou hast %s save%s.",save_number(ch[cn].saves),ch[cn].saves==1 ? "" : "s");

	dlog(cn,0,"Used shrine of security (lvl %d)",level);
	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void shrine_jobless(int in,int cn,int nr,int level,struct shrine_ppd *ppd)
{
	int n;

	for (n=0; n<P_MAX; n++) {
		if (ch[cn].prof[n]) break;		
	}
	if (n==P_MAX) {
		log_char(cn,LOG_SYSTEM,0,"A bored voice says: 'Thou art jobless already.'");
		return;
	}
	log_char(cn,LOG_SYSTEM,0,"A bored voice says: 'Thou shalt be jobless.'");

	for (n=0; n<P_MAX; n++) ch[cn].prof[n]=0;
	ch[cn].flags|=CF_PROF;
	update_char(cn);

	dlog(cn,0,"Used shrine of the jobless (lvl %d)",level);
	shrine_set(nr,ppd);
	sendquestlog(cn,ch[cn].player);
}

void randomshrine(int in,int cn)
{
	struct shrine_ppd *ppd;
	int in2,n;

	if (!cn) return;

	ppd=set_data(cn,DRD_RANDOMSHRINE_PPD,sizeof(struct shrine_ppd));
	if (!ppd) return;

	for (n=0; n<INVENTORYSIZE; n++)
		if ((in2=ch[cn].item[n]) && it[in2].ID==IID_AREA14_SHRINEKEY && it[in2].drdata[0]==it[in].drdata[1]) break;
	if (n==INVENTORYSIZE) {
		if (!(in2=ch[cn].citem) || it[in2].ID!=IID_AREA14_SHRINEKEY || it[in2].drdata[0]!=it[in].drdata[1]) {
			log_char(cn,LOG_SYSTEM,0,"Nothing happens. You seem to need some kind of magical item to invoke the powers of the shrine.");
			return;
		}
	}

	switch(it[in].drdata[0]) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:		if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_indecisiveness(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
		case 19:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_bribes(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
		case 28:
		case 29:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_welding(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
		case 38:
		case 39:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_edge(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;

		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
		case 48:
		case 49:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_kindness(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
                case 50:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_vitality(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;

		case 51:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_death(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 52:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_braveness(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 53:
		case 54:
		case 55:
		case 56:
		case 57:
		case 58:
		case 59:
		case 60:
		case 61:
		case 62:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_security(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 63:
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
		case 70:
		case 71:
		case 72:	if (shrine_check(cn,it[in].drdata[0],ppd))
				    shrine_jobless(in,cn,it[in].drdata[0],it[in].drdata[1],ppd);
				return;
		case 73:
		case 74:
		case 75:
		case 76:
		case 77:
		case 78:
		case 79:
		case 80:
		case 81:
		case 82:
		case 83:
		case 84:
		case 85:
		case 86:
		case 87:
		case 88:
		case 89:
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
		case 98:
		case 99:
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
		case 108:
		case 109:
		case 110:
		case 111:
		case 112:
		case 113:
		case 114:
		case 115:
		case 116:
		case 117:
		case 118:
		case 119:
		case 120:
		case 121:
		case 122:
		case 123:
		case 124:
		case 125:
		case 126:
		case 127:
		case 128:
		case 129:
		case 130:
		case 131:
		case 132:
		case 133:
		case 134:
		case 135:
		case 136:
		case 137:
		case 138:
		case 139:
		case 140:
		case 141:
		case 142:
		case 143:
		case 144:
		case 145:
		case 146:
		case 147:
		case 148:
		case 149:
		case 150:
		case 151:
		case 152:
		case 153:
		case 154:
		case 155:
		case 156:
		case 157:
		case 158:
		case 159:
		case 160:
		case 161:
		case 162:
		case 163:
		case 164:
		case 165:
		case 166:
		case 167:
		case 168:
		case 169:
		case 170:
		case 171:
		case 172:
		case 173:
		case 174:
		case 175:
		case 176:
		case 177:
		case 178:
		case 179:
		case 180:
		case 181:
		case 182:
		case 183:
		case 184:
		case 185:
		case 186:
		case 187:
		case 188:
		case 189:
		case 190:
		case 191:
		case 192:
		case 193:
		case 194:
		case 195:
		case 196:
		case 197:
		case 198:
		case 199:
		case 200:
		case 201:
		case 202:
		case 203:
		case 204:
		case 205:
		case 206:
		case 207:
		case 208:
		case 209:
		case 210:
		case 211:
		case 212:
		case 213:
		case 214:
		case 215:
		case 216:
		case 217:
		case 218:
		case 219:
		case 220:
		case 221:
		case 222:
		case 223:
		case 224:
		case 225:
		case 226:
		case 227:	
		case 228:
		case 229:
		case 230:
		case 231:
		case 232:
		case 233:
		case 234:
		case 235:
		case 236:
		case 237:
		case 238:
		case 239:
		case 240:
		case 241:
		case 242:
		case 243:
		case 244:
		case 245:
		case 246:
		case 247:
		case 248:
		case 249:
		case 250:
		case 251:
		case 252:
		case 253:
		case 254:	break;
		case 255:	shrine_continuity(in,cn,it[in].drdata[0],it[in].drdata[1],ppd); break;

		default:	elog("item %s at %d,%d: unknown type %d",it[in].name,it[in].x,it[in].y,it[in].drdata[0]);
	}
}

int teleport(int cn,int x,int y)
{
	int oldx,oldy;

        oldx=ch[cn].x;
	oldy=ch[cn].y;

	remove_char(cn);
	ch[cn].action=ch[cn].step=ch[cn].duration=0;
	if (ch[cn].player) player_driver_stop(ch[cn].player,0);

	if (drop_char(cn,x,y,0)) return 1;

	drop_char(cn,oldx,oldy,0);

	return 0;
}

void trapdoor(int in,int cn)
{
	int dx,dy,in2;

	if (cn) {	// open or block
		if (ch[cn].x==it[in].x && ch[cn].y==it[in].y) {			// stepped on it
			if (it[in].drdata[0]) return;
			if (!(ch[cn].flags&CF_PLAYER)) return;

			dx2offset(ch[cn].dir,&dx,&dy,NULL);
			if (!teleport(cn,ch[cn].x-dx,ch[cn].y-dy)) return;

			it[in].drdata[0]=1;
			it[in].sprite++;
			map[it[in].x+it[in].y*MAXMAP].flags|=MF_TMOVEBLOCK;
			set_sector(it[in].x,it[in].y);

                        log_char(cn,LOG_SYSTEM,0,"A trapdoor opens under your feet, but you manage to jump back in time.");

			call_item(it[in].driver,in,0,ticker+TICKS*6);
			return;
		}
		if (it[in].drdata[0]) {
			log_char(cn,LOG_SYSTEM,0,"You cannot do anything with it now.");
			return;
		}

		if ((in2=ch[cn].citem) && it[in2].ID==IID_AREA14_STEELBAR) {
			it[in].drdata[0]=2;
			it[in].sprite+=2;
			set_sector(it[in].x,it[in].y);

			dlog(cn,in2,"dropped on trapdoor");
			destroy_item(in2);
			ch[cn].citem=0;
			ch[cn].flags|=CF_ITEMS;

			return;
		}

		log_char(cn,LOG_SYSTEM,0,"You'd need something like a hard stick to lock the door.");
		
		return;
	}
	if (it[in].drdata[0]!=1) return;	// closed or blocked

	it[in].drdata[0]=0;
	it[in].sprite--;
	map[it[in].x+it[in].y*MAXMAP].flags&=~MF_TMOVEBLOCK;
	set_sector(it[in].x,it[in].y);
}

void junkpile(int in,int cn)
{
	int in2=0,level;

	if (!cn) return;

	level=it[in].drdata[0];

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

        switch(RANDOM(10)) {
		case 1:		in2=create_item("steelbar"); break;
		case 2:		in2=create_item("steelbar"); break;
		case 3:		in2=create_money_item(RANDOM(100*level)+level); break;
		case 4:		in2=create_item("steelbar"); break;
		case 5:		in2=create_item("steelbar"); break;
		case 7:		in2=create_item("steelbar"); break;
		case 9:		in2=create_item("steelbar"); break;
	}
        if (in2) {
		log_char(cn,LOG_SYSTEM,0,"You found something between all that junk.");
		dlog(cn,in2,"took from junk pile");
		ch[cn].citem=in2;
		it[in2].carried=cn;
		ch[cn].flags|=CF_ITEMS;
	}
	remove_item_map(in);
	destroy_item(in);
}

void gastrap(int in,int cn)
{
	int n,m,sprite;

	if (cn) {
                if (!it[in].drdata[1]) { call_item(it[in].driver,in,0,ticker+1); }
		else return;
	} else if (!it[in].drdata[1]) return;

	n=it[in].x+it[in].y*MAXMAP;
	if (map[n].fsprite>=15291 && map[n].fsprite<=15326) m=n;
	else if (map[n+1].fsprite>=15291 && map[n+1].fsprite<=15326) m=n+1;
        else if (map[n-1].fsprite>=15291 && map[n-1].fsprite<=15326) m=n-1;
	else if (map[n+MAXMAP].fsprite>=15291 && map[n+MAXMAP].fsprite<=15326) m=n+MAXMAP;
	else if (map[n-MAXMAP].fsprite>=15291 && map[n-MAXMAP].fsprite<=15326) m=n-MAXMAP;
        else { elog("gastrap at %d,%d could not find fsprite",it[in].x,it[n].y); return; }

	sprite=map[m].fsprite;

	if (sprite>=15291 && sprite<=15299) sprite=15291;
	if (sprite>=15300 && sprite<=15308) sprite=15300;
	if (sprite>=15309 && sprite<=15317) sprite=15309;
	if (sprite>=15318 && sprite<=15326) sprite=15318;

        it[in].drdata[1]++;
	if (it[in].drdata[1]==9) it[in].drdata[1]=0;
	else call_item(it[in].driver,in,0,ticker+3);

	map[m].fsprite=sprite+it[in].drdata[1];
	set_sector(m%MAXMAP,m/MAXMAP);

	if (ch[cn].flags&CF_PLAYER) hurt(cn,POWERSCALE*it[in].drdata[0],0,1,50,33);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_RANDOMMASTER:		randommaster(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_RANDOMSHRINE:	randomshrine(in,cn); return 1;
		case IDR_TRAPDOOR:	trapdoor(in,cn); return 1;
		case IDR_JUNKPILE:	junkpile(in,cn); return 1;
		case IDR_GASTRAP:	gastrap(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_RANDOMMASTER:		immortal_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:		return 0;
	}
}












