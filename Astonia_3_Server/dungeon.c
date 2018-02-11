/*
 * $Id: dungeon.c,v 1.5 2008/03/24 11:22:07 devel Exp $
 *
 * $Log: dungeon.c,v $
 * Revision 1.5  2008/03/24 11:22:07  devel
 * min jewel count for raid
 *
 * Revision 1.4  2007/12/10 10:09:18  devel
 * clan changes
 *
 * Revision 1.3  2007/08/13 18:50:38  devel
 * fixed some warnings
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
#include "player_driver.h"
#include "consistency.h"

#include "dungeon_tab.c"

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
	{{"help",NULL},NULL,2},
	{{"list",NULL},NULL,3}
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

static int xsize=20,ysize=20,maze_base=0,maze_level=0,maze_clan=0;
static int xoff=2,yoff=2;

int build_warrior(int x,int y,int level)
{
	int cn,n,val,base,in;

	cn=create_char("warrior",0);
	if (level>33) {
		ch[cn].flags|=CF_ARCH;
		ch[cn].value[1][V_RAGE]=1;
	}
	
	if (level<1) level=1;
        base=warrior_tab[min(level,118)];

        for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost) continue;
		if (!ch[cn].value[1][n]) continue;

		switch(n) {
			case V_HP:		val=max(10,base-20); break;
			case V_ENDURANCE:	val=max(10,base-30); break;
			
			case V_PROFESSION:	if (level>19) val=max(1,base-7);
						else val=0;
						break;
			
			case V_WIS:		val=max(10,base-15); break;
			case V_INT:		val=max(10,base); break;
			case V_AGI:		val=max(10,base-5); break;
			case V_STR:		val=max(10,base); break;

                        case V_HAND:		val=max(1,base); break;
			case V_ARMORSKILL:	val=max(1,(base/10)*10); break;
			case V_ATTACK:		val=max(1,base); break;
			case V_PARRY:		val=max(1,base); break;
			case V_IMMUNITY:	val=max(1,base); break;
			
			case V_TACTICS:		val=max(1,base-5); break;
			case V_SURROUND:	val=max(1,base-50); break;
			case V_BODYCONTROL:	val=max(1,base-20); break;
			case V_SPEEDSKILL:	val=max(1,base-20); break;
			case V_PERCEPT:		val=max(1,base-10); break;
			
			case V_RAGE:		if (ch[cn].flags&CF_ARCH) val=max(1,base-20);
						else val=0;
						break;

			default:		val=max(1,base-50); break;
		}

		val=min(val,125);
		ch[cn].value[1][n]=val;
	}
	if (maze_clan<17) ch[cn].sprite=266+maze_clan;
	else ch[cn].sprite=516+maze_clan-16;

	ch[cn].x=ch[cn].tmpx=x;
	ch[cn].y=ch[cn].tmpy=y;
	ch[cn].dir=DX_RIGHTDOWN;
	
	ch[cn].restx=maze_clan;
	ch[cn].resty=(level-maze_level)/2;

	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);

        if (ch[cn].value[1][V_PROFESSION]>0) ch[cn].prof[P_CLAN]=min(30,ch[cn].value[1][V_PROFESSION]);
	if (ch[cn].value[1][V_PROFESSION]>30) ch[cn].prof[RANDOM(2) ? P_LIGHT : P_DARK]=min(30,ch[cn].value[1][V_PROFESSION]-30);

        // create special equipment bonus to equal that of the average player
	in=create_item("equip1");
	for (n=0; n<5; n++) it[in].mod_value[n]=level2maxitem(level)*1.1+max(0,level-63)/2;;
	ch[cn].item[12]=in; it[in].carried=cn;

	in=create_item("equip2");
	for (n=0; n<4; n++) it[in].mod_value[n]=level2maxitem(level)*1.1+max(0,level-63)/2;;
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

	sprintf(ch[cn].name,"Warrior%d",level);

	/*xlog("%s (%d):",ch[cn].name,ch[cn].level);
	xlog("HP:        %3d/%3d",ch[cn].value[1][V_HP],ch[cn].value[0][V_HP]);
	xlog("Wisdom:    %3d/%3d",ch[cn].value[1][V_WIS],ch[cn].value[0][V_WIS]);
	xlog("Intuition: %3d/%3d",ch[cn].value[1][V_INT],ch[cn].value[0][V_INT]);
	xlog("Agility:   %3d/%3d",ch[cn].value[1][V_AGI],ch[cn].value[0][V_AGI]);
	xlog("Strength:  %3d/%3d",ch[cn].value[1][V_STR],ch[cn].value[0][V_STR]);
	xlog("Twohanded: %3d/%3d",ch[cn].value[1][V_TWOHAND],ch[cn].value[0][V_TWOHAND]);
	xlog("Attack:    %3d/%3d",ch[cn].value[1][V_ATTACK],ch[cn].value[0][V_ATTACK]);
	xlog("Parry:     %3d/%3d",ch[cn].value[1][V_PARRY],ch[cn].value[0][V_PARRY]);
	xlog("Tactics:   %3d/%3d",ch[cn].value[1][V_TACTICS],ch[cn].value[0][V_TACTICS]);
	xlog("Immunity:  %3d/%3d",ch[cn].value[1][V_IMMUNITY],ch[cn].value[0][V_IMMUNITY]);*/

	return ch[cn].level;
}

int build_mage(int x,int y,int level)
{
	int cn,n,val,base,in;

	cn=create_char("mage",0);
	if (level>33) {
		ch[cn].flags|=CF_ARCH;
		ch[cn].value[1][V_DURATION]=1;
	}
	
	if (level<1) level=1;
        base=mage_tab[min(level,118)];

        for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost) continue;
		if (!ch[cn].value[1][n]) continue;

		switch(n) {
			case V_HP:		val=max(10,base-40); break;
			case V_MANA:		val=max(10,base-10); break;
			case V_ENDURANCE:	val=max(10,base-30); break;
			
			case V_PROFESSION:	if (level>19) val=max(1,base-7);
						else val=0;
						break;
			
			case V_WIS:		val=max(10,base); break;
			case V_INT:		val=max(10,base); break;
			case V_AGI:		val=max(10,base); break;
			case V_STR:		val=max(10,base); break;

			case V_HAND:		val=max(1,base); break;
			case V_MAGICSHIELD:	val=max(1,base); break;
			case V_FLASH:		val=max(1,base); break;
			case V_BLESS:		val=max(1,base); break;
			case V_IMMUNITY:	val=max(1,base); break;
			
			case V_FREEZE:		val=max(1,base-10); break;
			case V_HEAL:		val=max(1,base-10); break;
			case V_FIREBALL:	val=max(1,base-10); break;
                        case V_PERCEPT:		val=max(1,base-10); break;
			case V_DURATION:	val=max(1,base-10); break;

			default:		val=max(1,base-50); break;
		}

		val=min(val,125);
		ch[cn].value[1][n]=val;
	}
	if (maze_clan<17) ch[cn].sprite=282+maze_clan;
	else ch[cn].sprite=532+maze_clan-16;

	ch[cn].x=ch[cn].tmpx=x;
	ch[cn].y=ch[cn].tmpy=y;
	ch[cn].dir=DX_RIGHTDOWN;

	ch[cn].restx=maze_clan;
	ch[cn].resty=(level-maze_level)/2;
	
	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);

	if (ch[cn].value[1][V_PROFESSION]>0) ch[cn].prof[P_CLAN]=min(30,ch[cn].value[1][V_PROFESSION]);
	if (ch[cn].value[1][V_PROFESSION]>30) ch[cn].prof[RANDOM(2) ? P_LIGHT : P_DARK]=min(30,ch[cn].value[1][V_PROFESSION]-30);
	
        // create special equipment bonus to equal that of the average player
	in=create_item("equip1b");
	for (n=0; n<5; n++) it[in].mod_value[n]=level2maxitem(level)*1.1+max(0,level-63)/2;;
        ch[cn].item[12]=in; it[in].carried=cn;

	in=create_item("equip2b");
	for (n=0; n<4; n++) it[in].mod_value[n]=level2maxitem(level)*1.1+max(0,level-63)/2;;
        ch[cn].item[13]=in; it[in].carried=cn;

        in=create_item("weapon_spell");
	ch[cn].item[15]=in; it[in].carried=cn;
	it[in].mod_value[0]=max(13,min(113,ch[cn].value[1][V_HAND]));

        update_char(cn);
	
	ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
	ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

	drop_char(cn,x,y,0);

	sprintf(ch[cn].name,"Mage%d",level);

	/*xlog("%s (%d):",ch[cn].name,ch[cn].level);
	xlog("HP:        %3d/%3d",ch[cn].value[1][V_HP],ch[cn].value[0][V_HP]);
	xlog("Wisdom:    %3d/%3d",ch[cn].value[1][V_WIS],ch[cn].value[0][V_WIS]);
	xlog("Intuition: %3d/%3d",ch[cn].value[1][V_INT],ch[cn].value[0][V_INT]);
	xlog("Agility:   %3d/%3d",ch[cn].value[1][V_AGI],ch[cn].value[0][V_AGI]);
	xlog("Strength:  %3d/%3d",ch[cn].value[1][V_STR],ch[cn].value[0][V_STR]);
	xlog("Twohanded: %3d/%3d",ch[cn].value[1][V_TWOHAND],ch[cn].value[0][V_TWOHAND]);
	xlog("Attack:    %3d/%3d",ch[cn].value[1][V_ATTACK],ch[cn].value[0][V_ATTACK]);
	xlog("Parry:     %3d/%3d",ch[cn].value[1][V_PARRY],ch[cn].value[0][V_PARRY]);
	xlog("Tactics:   %3d/%3d",ch[cn].value[1][V_TACTICS],ch[cn].value[0][V_TACTICS]);
	xlog("Immunity:  %3d/%3d",ch[cn].value[1][V_IMMUNITY],ch[cn].value[0][V_IMMUNITY]);*/

	return ch[cn].level;
}

int build_seyan(int x,int y,int level)
{
	int cn,n,val,base,in;

	cn=create_char("seyan",0);
	if (level>33) ch[cn].flags|=CF_ARCH;

	if (level<1) level=1;
        base=seyan_tab[min(level,118)];

        for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost) continue;
		if (!ch[cn].value[1][n]) continue;

		switch(n) {
			case V_HP:		val=max(10,base-40); break;
			case V_MANA:		val=max(10,base-30); break;
			case V_ENDURANCE:	val=max(10,base-50); break;

			case V_PROFESSION:	if (level>19) val=max(1,base-7);
						else val=0;
						break;
			
			case V_WIS:		val=max(10,base-15); break;
			case V_INT:		val=max(10,base); break;
			case V_AGI:		val=max(10,base-5); break;
			case V_STR:		val=max(10,base); break;

			case V_HAND:		val=max(1,base); break;
			case V_ARMORSKILL:	val=max(1,(base/10)*10); break;
			case V_ATTACK:		val=max(1,base); break;
			case V_PARRY:		val=max(1,base); break;
			case V_IMMUNITY:	val=max(1,base); break;

                        case V_BLESS:		val=max(1,base); break;
                        case V_FREEZE:		val=max(1,base); break;

			case V_TACTICS:		val=max(1,base-5); break;
			case V_PERCEPT:		val=max(1,base-10); break;

			default:		val=max(1,base-50); break;
		}

		val=min(val,107);
		ch[cn].value[1][n]=val;
	}
	if (maze_clan<17) ch[cn].sprite=266+maze_clan;
	else ch[cn].sprite=516+maze_clan-16;

	ch[cn].x=ch[cn].tmpx=x;
	ch[cn].y=ch[cn].tmpy=y;
	ch[cn].dir=DX_RIGHTDOWN;

	ch[cn].restx=maze_clan;
	ch[cn].resty=(level-maze_level)/2;
	
	ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);

	if (ch[cn].value[1][V_PROFESSION]>0) ch[cn].prof[P_CLAN]=min(30,ch[cn].value[1][V_PROFESSION]);
	if (ch[cn].value[1][V_PROFESSION]>30) ch[cn].prof[RANDOM(2) ? P_LIGHT : P_DARK]=min(30,ch[cn].value[1][V_PROFESSION]-30);
	
        // create special equipment bonus to equal that of the average player
	in=create_item("equip1c");
	for (n=0; n<5; n++) it[in].mod_value[n]=level2maxitem(level)*1.1+max(0,level-63)/2;
	ch[cn].item[12]=in; it[in].carried=cn;

	in=create_item("equip2c");
	for (n=0; n<5; n++) it[in].mod_value[n]=level2maxitem(level)*1.1+max(0,level-63)/2;
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

	sprintf(ch[cn].name,"Seyan%d",level);

	/*xlog("%s (%d):",ch[cn].name,ch[cn].level);
	xlog("HP:        %3d/%3d",ch[cn].value[1][V_HP],ch[cn].value[0][V_HP]);
	xlog("Wisdom:    %3d/%3d",ch[cn].value[1][V_WIS],ch[cn].value[0][V_WIS]);
	xlog("Intuition: %3d/%3d",ch[cn].value[1][V_INT],ch[cn].value[0][V_INT]);
	xlog("Agility:   %3d/%3d",ch[cn].value[1][V_AGI],ch[cn].value[0][V_AGI]);
	xlog("Strength:  %3d/%3d",ch[cn].value[1][V_STR],ch[cn].value[0][V_STR]);
	xlog("Twohanded: %3d/%3d",ch[cn].value[1][V_TWOHAND],ch[cn].value[0][V_TWOHAND]);
	xlog("Attack:    %3d/%3d",ch[cn].value[1][V_ATTACK],ch[cn].value[0][V_ATTACK]);
	xlog("Parry:     %3d/%3d",ch[cn].value[1][V_PARRY],ch[cn].value[0][V_PARRY]);
	xlog("Tactics:   %3d/%3d",ch[cn].value[1][V_TACTICS],ch[cn].value[0][V_TACTICS]);
	xlog("Immunity:  %3d/%3d",ch[cn].value[1][V_IMMUNITY],ch[cn].value[0][V_IMMUNITY]);*/

	return ch[cn].level;
}

void build_wall(int x,int y)
{
	int m;

	m=x+y*MAXMAP;

        map[m].flags=MF_INDOORS|MF_SIGHTBLOCK|MF_SOUNDBLOCK|MF_SHOUTBLOCK|MF_MOVEBLOCK;
	map[m].fsprite=59171+((x&3)+(y&3))%4;
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
	map[m].gsprite=59130+(x%3)+(y%3)*3;
	map[m].dlight=0;
	map[m].light=0;
}

void build_remove(int x,int y)
{
	int m,cn,in,fn,n;

	m=x+y*MAXMAP;

	if ((cn=map[m].ch)) {
		if (ch[cn].flags&CF_PLAYER) {
			log_char(cn,LOG_SYSTEM,0,"The catacomb collapsed on you.");

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
			    !drop_item(in,250,230))
				{ destroy_item(in); }
			else set_expire_body(in,PLAYER_BODY_DECAY_TIME);
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

void build_teleport(int x,int y)
{
	int in;

        in=create_item("teleport_trap");
	
	*(unsigned short*)(it[in].drdata+0)=xoff+2;
        *(unsigned short*)(it[in].drdata+2)=yoff+78;
	*(unsigned short*)(it[in].drdata+4)=maze_clan;

	if (!set_item_map(in,x,y)) {
		destroy_item(in);
	}
}

void build_fake(int x,int y)
{
	int in;

        in=create_item("fake_wall");
	
        *(unsigned short*)(it[in].drdata+0)=maze_clan;

	if (!set_item_map(in,x,y)) {
		destroy_item(in);
	}

	//xlog("fake wall at %d,%d",x,y);
}

void build_door(int x,int y,int keyid,int keys)
{
	int in;

        in=create_item("dungeon_door");
	
        if (keys>0) *(unsigned long*)(it[in].drdata+0)=MAKE_ITEMID(DEV_ID_MAZE1,keyid); else *(unsigned long*)(it[in].drdata+0)=0;
	if (keys>1) *(unsigned long*)(it[in].drdata+4)=MAKE_ITEMID(DEV_ID_MAZE2,keyid); else *(unsigned long*)(it[in].drdata+4)=0;
	*(unsigned short*)(it[in].drdata+8)=maze_clan;

	if (!set_item_map(in,x,y)) {
		destroy_item(in);
	}
}

void build_key(int x,int y,int nr,int keyid)
{
	int in;

        in=create_item("maze_key_spawn");

	it[in].drdata[0]=nr;
	it[in].drdata[1]=maze_clan;
	it[in].drdata[2]=0;
	*(unsigned long*)(it[in].drdata+4)=keyid;

	if (!set_item_map(in,x,y)) {
		destroy_item(in);
	}
}

void build_cell(int cx,int cy,struct cell *cell)
{
        build_wall(cx*4+xoff,cy*4+yoff);
	if (cell->t) {
		build_wall(cx*4+1+xoff,cy*4+yoff);
		if (cell->special==2) build_fake(cx*4+2+xoff,cy*4+yoff);
		else build_wall(cx*4+2+xoff,cy*4+yoff);
		build_wall(cx*4+3+xoff,cy*4+yoff);
	}
	if (cell->l) {
		build_wall(cx*4+xoff,cy*4+1+yoff);
		if (cell->special==1) build_fake(cx*4+xoff,cy*4+2+yoff);
		else build_wall(cx*4+xoff,cy*4+2+yoff);
		build_wall(cx*4+xoff,cy*4+3+yoff);
	}	

	switch(cell->special) {
		case 3:		build_key(cx*4+xoff+2,cy*4+yoff+2,1,maze_base); break;
		case 4:		build_key(cx*4+xoff+2,cy*4+yoff+2,2,maze_base); break;

		case 5:		build_warrior(cx*4+xoff+2,cy*4+yoff+2,maze_level); break;
		case 6:		build_mage(cx*4+xoff+2,cy*4+yoff+2,maze_level); break;
		case 7: 	build_seyan(cx*4+xoff+2,cy*4+yoff+2,maze_level); break;

		case 8:		build_warrior(cx*4+xoff+2,cy*4+yoff+2,maze_level+2); break;
		case 9:		build_mage(cx*4+xoff+2,cy*4+yoff+2,maze_level+2); break;
		case 10: 	build_seyan(cx*4+xoff+2,cy*4+yoff+2,maze_level+2); break;

		case 11:	build_warrior(cx*4+xoff+2,cy*4+yoff+2,maze_level+4); break;
		case 12:	build_mage(cx*4+xoff+2,cy*4+yoff+2,maze_level+4); break;
		case 13: 	build_seyan(cx*4+xoff+2,cy*4+yoff+2,maze_level+4); break;

		case 14:	build_warrior(cx*4+xoff+2,cy*4+yoff+2,maze_level+6); break;
		case 15:	build_mage(cx*4+xoff+2,cy*4+yoff+2,maze_level+6); break;
		case 16:	build_seyan(cx*4+xoff+2,cy*4+yoff+2,maze_level+6); break;

		case 17:	build_warrior(cx*4+xoff+2,cy*4+yoff+2,maze_level+8); break;
		case 18:	build_mage(cx*4+xoff+2,cy*4+yoff+2,maze_level+8); break;
		case 19:	build_seyan(cx*4+xoff+2,cy*4+yoff+2,maze_level+8); break;

		case 20:	build_warrior(cx*4+xoff+2,cy*4+yoff+2,maze_level+10); break;
		case 21:	build_mage(cx*4+xoff+2,cy*4+yoff+2,maze_level+10); break;
		case 22:	build_seyan(cx*4+xoff+2,cy*4+yoff+2,maze_level+10); break;

		case 23:	build_teleport(cx*4+xoff+2,cy*4+yoff+2); break;
		case 24:	build_teleport(cx*4+xoff+1,cy*4+yoff+2); break;
		case 25:	build_teleport(cx*4+xoff+3,cy*4+yoff+2); break;
                case 26:	build_teleport(cx*4+xoff+2,cy*4+yoff+1); break;
		case 27:	build_teleport(cx*4+xoff+2,cy*4+yoff+3); break;

		case 28:	build_door(cx*4+xoff+2,cy*4+yoff+2,maze_base,0); break;
		case 29:	build_door(cx*4+xoff+2,cy*4+yoff+2,maze_base,1); break;
		case 30:	build_door(cx*4+xoff+2,cy*4+yoff+2,maze_base,2); break;

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

int create_maze(int base,int level,int do_fake,int keys,const int *warrior,const int *mage,const int *seyan,int teleport,int cnr,int show)
{
	int n,m,path1,path2,path3,fake=0,fakedir=0,tmp,maxi,panic;
	struct cell *cell;
        int score;

	maze_base=base; maze_level=level; maze_clan=cnr;
	srand(base);

	cell=xcalloc(sizeof(struct cell)*xsize*ysize,IM_TEMP);

	build_maze(cell);

	// find path start->target
        special_fill(cell,xsize-1,1);
	//xlog("cost start->target: %d",cell[xsize*ysize-1].special);
	path1=cell[xsize*ysize-ysize].special;
	
	n=xsize*ysize-ysize;
	m=cell[n].special;
	tmp=m/2;

	while (m>0) {
		cell[n].special=999;

		if (!cell[n].l && n%xsize>0 && cell[n-1].special==m-1) { if (m==tmp) { fake=n; fakedir=1; } n=n-1; }
		else if (!cell[n].t && n/xsize>0 && cell[n-xsize].special==m-1) { if (m==tmp) { fake=n; fakedir=2; } n=n-xsize; }
		else if (n%xsize<xsize-1 && !cell[n+1].l && cell[n+1].special==m-1) { n=n+1; if (m==tmp) { fake=n; fakedir=1; } }
		else if (n/xsize<ysize-1 && !cell[n+xsize].t && cell[n+xsize].special==m-1) { n=n+xsize; if (m==tmp) { fake=n; fakedir=2; } }
		else break;
		m--;
	}
	
	// delete all numbers but path
	for (m=0; m<xsize*ysize; m++) if (cell[m].special!=999) cell[m].special=0;

	// find distance from path to key
	path2=special_fill(cell,0,1); for (m=0; m<xsize*ysize; m++) if (cell[m].special!=999) cell[m].special=0;

        // find distance from path to key
	path3=special_fill(cell,xsize*ysize-1,1); for (m=0; m<xsize*ysize; m++) cell[m].special=0;

	score=(path1+path2+path3)+cbrt(path1*path2*path3)*3;
	//xlog("total difficulty rating (%d %d %d): %d",path1,path2,path3,score);

	if (do_fake) {
		// build a wall where the fake will be:
		if (fakedir==1) cell[fake].l=1;
		if (fakedir==2) cell[fake].t=1;
		
		// find path
		special_fill(cell,0,1);
	
		// there is a path? then this is junk
		if (cell[xsize*ysize-1].special) score=0;

		for (m=0; m<xsize*ysize; m++) cell[m].special=0;

		cell[fake].special=fakedir;
		//xlog("fake at %d,%d",(fake%xsize)*4+xoff,(fake/xsize)*4+yoff);
	}
	if (keys>0) cell[0].special=3;
	if (keys>1) cell[xsize*ysize-1].special=4;

	if (keys==0) cell[xsize-1].special=28;
	if (keys==1) cell[xsize-1].special=29;
	if (keys==2) cell[xsize-1].special=30;

	maxi=50; panic=200;
	for (n=5; n>=0; n--) {
		int x,y;

		tmp=warrior[n];
		while (tmp>0 && maxi>0 && panic-->0) {
			m=RANDOM(xsize*ysize);
			if (cell[m].special) continue;
			
			x=m%xsize;
			y=m/xsize;
                        if ((x<5 && y>(ysize-6))) continue;
			
			cell[m].special=5+n*3;
			tmp--; maxi--;
		}
		tmp=mage[n];
		while (tmp>0 && maxi>0 && panic-->0) {
			m=RANDOM(xsize*ysize);
			if (cell[m].special) continue;
			
			x=m%xsize;
			y=m/xsize;
                        if ((x<5 && y>(ysize-6))) continue;

			cell[m].special=6+n*3;
			tmp--; maxi--;
		}
		tmp=seyan[n];
		while (tmp>0 && maxi>0 && panic-->0) {
			m=RANDOM(xsize*ysize);
			if (cell[m].special) continue;
			
			x=m%xsize;
			y=m/xsize;
                        if ((x<5 && y>(ysize-6))) continue;

			cell[m].special=7+n*3;
			tmp--; maxi--;
		}
	}

	//xlog("%d NPCs total",50-maxi);

	// teleport traps
	tmp=teleport;
        while (tmp>0) {
		m=RANDOM(xsize*ysize);
                if (m%xsize==0 && m/xsize==ysize-1) continue;	// no trap at start point
		if (cell[m].special) continue;
		cell[m].special=RANDOM(5)+23;
		tmp--;
	}

        for (m=0; m<xsize*ysize; m++) build_cell(m%xsize,m/xsize,cell+m);		

	if (show) show_maze(cell);

        xfree(cell);

	return score;
}

void destroy_dungeon(int nr)
{
	int xf,yf,xt,yt,x,y;

	xlog("destroying dungeon %d",nr);

	xf=(nr%3)*81+2; xt=xf+80;
	yf=(nr/3)*81+2; yt=yf+80;

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

#define DUNGEONTIME	(TICKS*60*60)

struct master_data
{
	int target[9];
	int level[9];
	int created[9];
	int warning[9];
	int owner[9];
	int created_by_clan[9];

	int memcleartimer;
};

void create_dungeon(int cn,int co,int target,struct master_data *dat)
{
        int warrior[6]={0,0,0,0,0,0};
	int mage[6]={0,0,0,0,0,0};
	int seyan[6]={0,0,0,0,0,0};
	int teleport=0,fake=0,key=0;
	int n,best=0,bestn=0,tmp,fee;

	if (target<1 || target>=32) {
		say(cn,"No clan by that number.");
		return;
	}

	if (ch[co].level>56) {
		say(cn,"You cannot create a clan catacomb, your level is too high (max 56).");
		return;
	}

	if (!clan_can_attack_inside(get_char_clan(co),target) && !(ch[co].flags&CF_GOD)) {
		say(cn,"You are not at war with that clan.");
		return;
	}

	if (cnt_jewels(target)<11) {
		say(cn,"That clan does not have any jewels you could steal.");
		return;
	}

	if (cnt_jewels(get_char_clan(co))<12) {
		say(cn,"Your clan does not have enough jewels to mount a raid (your clan needs to have at least 11 of them).");
		return;
	}

	for (n=0; n<9; n++) {
		if (dat->target[n]==target) {
			say(cn,"This catacomb already exists, please use 'enter %d' instead.",n+1);
			return;
		}
                if (dat->created_by_clan[n]==get_char_clan(co)) {
			say(cn,"Your clan has created a catacomb already, you may not create another one before the first one has collapsed.");
			return;
		}
		if (dat->owner[n]==ch[co].ID) {
			say(cn,"You have created a catacomb already, you may not create another one before the first one has collapsed.");
			return;
		}
	}

	for (n=0; n<9; n++) {
                if (dat->created[n]) tmp=ticker-dat->created[n]; else tmp=max(DUNGEONTIME,ticker-dat->created[n]);
		if (tmp>best) { best=tmp; bestn=n; }		
	}
	if (best<DUNGEONTIME) {
		say(cn,"Sorry, all catacombs are busy. Please try again in %.2f minutes",(DUNGEONTIME-best)/(TICKS*60.0));
		return;
	}

	fee=3500;

	if (!take_money(co,fee*100)) {
		say(cn,"Sorry, you cannot afford the fee of %dG.",fee);
		return;
	}
	say(cn,"Very well, I have created the catacomb for you. Thank you for paying %d gold.",fee);

	xoff=(bestn%3)*81+2;
	yoff=(bestn/3)*81+2;

	dat->level[bestn]=56+score_to_level(clan_get_training_score(target)); //ch[co].level;
	dat->target[bestn]=target;
	dat->created[bestn]=ticker;
	dat->warning[bestn]=0;
	dat->owner[bestn]=ch[co].ID;
	dat->created_by_clan[bestn]=get_char_clan(co);

	for (n=1; n<22; n++) {
		switch(n) {
			case 1:	
			case 2:	
			case 3:	
			case 4:	
			case 5:	
			case 6:		warrior[n-1]=get_clan_dungeon(target,n); break;

			case 7:	
			case 8:	
			case 9:	
			case 10:	
			case 11:	
			case 12:	mage[n-7]=get_clan_dungeon(target,n); break;

			case 13:	
			case 14:	
			case 15:	
			case 16:	
			case 17:	
			case 18:	seyan[n-13]=get_clan_dungeon(target,n); break;

			case 19:	teleport=get_clan_dungeon(target,n); break;
			case 20:	fake=get_clan_dungeon(target,n); break;
			case 21:	key=get_clan_dungeon(target,n); break;
		}
	}

	do {
		destroy_dungeon(bestn);
	} while (create_maze(RANDOM(12345678),dat->level[bestn],fake,key,warrior,mage,seyan,teleport,target,0)<350);

	say(cn,"This catacomb will collapse in %.2f minutes.",DUNGEONTIME/(TICKS*60.0));

	teleport_char_driver(co,xoff+2,yoff+78);	

	add_clanlog(target,clan_serial(target),ch[co].ID,20,"Clan was attacked by %s of %s (%d)",ch[co].name,get_clan_name(get_char_clan(co)),get_char_clan(co));
	add_clanlog(get_char_clan(co),clan_serial(get_char_clan(co)),ch[co].ID,20,"%s attacked clan %s (%d)",ch[co].name,get_clan_name(target),target);
}

void enter_dungeon(int cn,int co,int target,struct master_data *dat)
{
	int tmp;

	if (target<1 || target>9) {
		say(cn,"Sorry, the target is out of bounds.");
		return;
	}
	target--;
	if (ch[co].level>56) {
		say(cn,"Sorry, you may not enter this catacomb, it was created for level %d and below.",dat->level[target]);
		return;
	}
	if (!clan_can_attack_inside(get_char_clan(co),dat->target[target])) {
		say(cn,"You are not at war with that clan.");
		return;
	}

	tmp=DUNGEONTIME-ticker+dat->created[target];
	if (tmp<TICKS*60) {
		say(cn,"Sorry, this catacomb is about to collapse.");
		return;
	}
	say(cn,"This catacomb will collapse in %.2f minutes,",tmp/(TICKS*60.0));	

	teleport_char_driver(co,(target%3)*81+4,(target/3)*81+80);
}

void list_dungeon(int cn,struct master_data *dat)
{
	int n,flag=0;

	for (n=0; n<9; n++) {
		if (dat->target[n]) {
			say(cn,"Catacomb %d: Clan %d, level %d, remaining time: %.2f minutes.",n+1,dat->target[n],dat->level[n],(DUNGEONTIME-ticker+dat->created[n])/(TICKS*60.0));
			flag=1;
		}
	}
	if (!flag) {
		say(cn,"No catacombs.");
	}
}

void warn_dungeon(int nr,int left)
{
	int co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].flags&CF_PLAYER) {
			if (((ch[co].x-2)/81)+((ch[co].y-2)/81)*3==nr) {
				log_char(co,LOG_SYSTEM,0,"This catacomb will collapse in %.2f minutes.",left/(TICKS*60.0));
			}
		}		
	}
}

void dungeonmaster(int cn,int ret,int lastact)
{
	struct master_data *dat;
        int co,target,n,tmp;
	struct msg *msg,*next;
	char *ptr;

        dat=set_data(cn,DRD_DUNGEONMASTER,sizeof(struct master_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		if (msg->type==NT_CREATE) {
			/*int l,wl;
			wl=1;
			for (n=1; n<200; n++) {
				l=build_seyan(n,10,n);
				if (l>=wl) {
					printf("%d,\t//level %d (%d)\n",n,wl,l);
					wl++;
				}
			}*/
		}

                // did we see someone?
		if (msg->type==NT_CHAR) {
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			// dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			say(cn,"Hello %s! Welcome to the clan catacombs. Be warned, there is a fee of 3500 gold for attacking now. Say °c4help°c0 for details.",ch[co].name);
			mem_add_driver(cn,co,7);


		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			
			switch(analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co)) {
				case 2:		say(cn,"Use: 'attack <nr>' to attack clan <nr>, 'enter <nr>' to enter catacomb <nr> or 'list' to get a listing of all catacombs.");
						break;
				case 3:		list_dungeon(cn,dat);
						break;
			}
			if ((ptr=strcasestr((char*)msg->dat2,"attack"))) {
                                ptr+=6;
				target=atoi(ptr);
                                create_dungeon(cn,co,target,dat);
			}
			if ((ptr=strcasestr((char*)msg->dat2,"enter"))) {
                                ptr+=5;
				target=atoi(ptr);
				//say(cn,"You want to enter catacomb %d",target);

				enter_dungeon(cn,co,target,dat);
			}

			if ((ptr=strcasestr((char*)msg->dat2,"destroy")) && (ch[co].flags&CF_GOD)) {
                                ptr+=7;
				target=atoi(ptr);
				//say(cn,"You want to destroy catacomb %d",target);

				if (target>0 && target<10) {
					target--;
					destroy_dungeon(target);
					dat->target[target]=0;
					dat->level[target]=0;
					dat->created[target]=0;
					dat->warning[target]=0;
					dat->owner[target]=0;
					dat->created_by_clan[target]=0;
				}
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
			dat->target[msg->dat2]=0;
			dat->level[msg->dat2]=0;
			dat->created[msg->dat2]=0;
			dat->warning[msg->dat2]=0;
			dat->owner[msg->dat2]=0;
			dat->created_by_clan[msg->dat2]=0;
		}

                remove_message(cn,msg);
	}
        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

	for (n=0; n<9; n++) {
		if (!dat->created[n]) continue;
		
		tmp=ticker-dat->created[n];
		
		if (tmp>DUNGEONTIME) {
			destroy_dungeon(n);
			dat->target[n]=0;
			dat->level[n]=0;
			dat->created[n]=0;
			dat->warning[n]=0;
			dat->owner[n]=0;
			dat->created_by_clan[n]=0;
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

void dungeonteleport(int in,int cn)
{
	int x,y,oldx,oldy,cnr;
	char buf[80];

        if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	if (!(ch[cn].flags&CF_PLAYER)) return;

	x=*(unsigned short*)(it[in].drdata+0);
	y=*(unsigned short*)(it[in].drdata+2);
	cnr=*(unsigned short*)(it[in].drdata+4);

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);
	
	if (!drop_char(cn,x,y,0) &&
	    !drop_char(cn,240,250,0) &&
	    !drop_char(cn,235,250,0) &&
	    !drop_char(cn,230,250,0)) {
		elog("drop char failed in dungeonteleport for %s",ch[cn].name);
		kick_char(cn);
	} else {
		if (ch[cn].player) player_driver_stop(ch[cn].player,0);
		remove_item(in);
		destroy_item(in);

		sprintf(buf,"%02d:T",cnr);
		server_chat(1028,buf);
	}
}

void dungeonfake(int in,int cn)
{
	int cnr,x,y;
	char buf[80];

	if (!cn) return;
	
	cnr=*(unsigned short*)(it[in].drdata);

	x=it[in].x; y=it[in].y;
	remove_lights(x,y);

	remove_item(in);
	destroy_item(in);
	
	reset_los(x,y);
        add_lights(x,y);

	sprintf(buf,"%02d:F",cnr);
	server_chat(1028,buf);
}

void dungeondoor(int in,int cn)
{
	int ID1,ID2,missing=0,co,nr,cnr,onr,cnt;
	char buf[80];
	int xf,yf,xt,yt,x,y,alive=0;

	if (!cn) return;

	ID1=*(unsigned long*)(it[in].drdata+0);
	ID2=*(unsigned long*)(it[in].drdata+4);
	cnr=*(unsigned long*)(it[in].drdata+8);

        if (ID1 && !has_item(cn,ID1)) missing++;
	if (ID2 && !has_item(cn,ID2)) missing++;

	if (ID1 && ID2 && missing) {
		log_char(cn,LOG_SYSTEM,0,"You need %d more key%s.",missing,missing>1 ? "s" : "");
	} else if (missing) {
		log_char(cn,LOG_SYSTEM,0,"You need a key.");
	}
	if (missing) return;

	nr=((it[in].x-2)/81)+((it[in].y-2)/81)*3;

        xf=(nr%3)*81+2; xt=xf+80;
	yf=(nr/3)*81+2; yt=yf+80;

	for (x=xf; x<xt; x++) {
		for (y=yf; y<yt; y++) {
			if ((co=map[x+y*MAXMAP].ch) && !(ch[co].flags&CF_PLAYER)) alive++;			
		}
	}
	if (alive>20) {
		log_char(cn,LOG_SYSTEM,0,"Too many Defenders are still alive (%d vs %d).",alive,20);
		return;
	}

        if (!it[in].drdata[12]) {

                onr=get_char_clan(cn);
		if (!onr) {
			log_char(cn,LOG_SYSTEM,0,"You're not supposed to be here.");
			return;
		}
		if (cnt_jewels(onr)<12) {
			log_char(cn,LOG_SYSTEM,0,"You can't steal jewels while your own clan has less than 12 of them.");
			return;
		}
		
		cnt=min(cnt_jewels(cnr)-11,3); //clan_stolen_jewel_nr(cnr,onr,ch[cn].level);

		if (cnt>0) {
			log_char(cn,LOG_SYSTEM,0,"You won. You stole %d jewels for your clan's storage.",cnt);
			
			sprintf(buf,"%02d:J:%02d:%03d:%010u:%s",cnr,onr,ch[cn].level,ch[cn].ID,ch[cn].name);
			server_chat(1028,buf);
		} else log_char(cn,LOG_SYSTEM,0,"You won. Unfortunately there's nothing left to steal.");
		
		*(unsigned long*)(it[in].drdata+0)=0;
		*(unsigned long*)(it[in].drdata+4)=0;
		it[in].drdata[12]=1;
	
                for (co=getfirst_char(); co; co=getnext_char(co)) {
			if (ch[co].flags&CF_PLAYER) {
				if (((ch[co].x-2)/81)+((ch[co].y-2)/81)*3==nr) {
					log_char(co,LOG_SYSTEM,0,"This catacomb has been solved and will collapse.");
				}
			}
			if (ch[co].driver==CDR_DUNGEONMASTER) {
				notify_char(co,NT_NPC,NTID_DUNGEON,nr,ticker);
			}
		}
	}

	if (!teleport_char_driver(cn,245,250) &&
	    !teleport_char_driver(cn,240,250) &&
	    !teleport_char_driver(cn,235,250)) teleport_char_driver(cn,230,250);
}

void fighter_dead(int cn,int co)
{
	int type;
	char buf[80];

        if ((ch[cn].flags&(CF_MAGE|CF_WARRIOR))==(CF_MAGE|CF_WARRIOR)) type='S';
	else if (ch[cn].flags&CF_MAGE) type='M';
	else type='W';
	
	sprintf(buf,"%02d:%c:%d",ch[cn].restx,type,ch[cn].resty);
	server_chat(1028,buf);	
}

void dungeonkey(int in,int cn)
{
        int in2;
	char buf[80];

	if (!cn) return;

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your 'hand' (mouse cursor) first.");
		return;
	}

	if (it[in].drdata[0]==1) {
		in2=create_item("maze_key1");
		it[in2].ID=MAKE_ITEMID(DEV_ID_MAZE1,*(unsigned long*)(it[in].drdata+4));
	} else {
		in2=create_item("maze_key2");	
		it[in2].ID=MAKE_ITEMID(DEV_ID_MAZE2,*(unsigned long*)(it[in].drdata+4));
	}

	if (!in2) {
		elog("dungeonkey item driver: could not create key");		
		return;			
	}
	
	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from dungeonkey");
	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;

	if (!it[in].drdata[2]) {
		it[in].drdata[2]=1;
		
		sprintf(buf,"%02d:K",it[in].drdata[1]);
		server_chat(1028,buf);	
	}
}

int dungeon_potion(int cn)
{
	int fre,in2,endtime,duration,nr,type,str,cnr;
	char buf[256];
	extern struct clan clan[];

	cnr=ch[cn].restx;

        if (!(fre=may_add_spell(cn,IDR_POTION_SP))) return 0;

	if (ch[cn].flags&CF_WARRIOR) type=0;
	else type=1;

	for (nr=5; nr>=0; nr--) {
		if (nr*10>ch[cn].value[1][V_INT]) continue;
		if (clan[cnr].dungeon.alc_pot[type][nr]<1) continue;
		break;
	}
	if (nr<0) return 0;

	str=nr*4+4;

	in2=create_item("potion_spell");
	if (!in2) return 0;

	if (type==0) {
		it[in2].mod_index[0]=V_ATTACK;
		it[in2].mod_value[0]=str;
		it[in2].mod_index[1]=V_PARRY;
		it[in2].mod_value[1]=str;
		it[in2].mod_index[2]=V_IMMUNITY;
		it[in2].mod_value[2]=str;
	} else {
		it[in2].mod_index[0]=V_FLASH;
		it[in2].mod_value[0]=str;
		it[in2].mod_index[1]=V_MAGICSHIELD;
		it[in2].mod_value[1]=str;
		it[in2].mod_index[2]=V_IMMUNITY;
		it[in2].mod_value[2]=str;
	}
	it[in2].driver=IDR_POTION_SP;

	duration=10*60*TICKS;

	endtime=ticker+duration;

	*(signed long*)(it[in2].drdata)=endtime;
	*(signed long*)(it[in2].drdata+4)=ticker;

	it[in2].carried=cn;

        ch[cn].item[fre]=in2;

	create_spell_timer(cn,in2,fre);

	update_char(cn);

	sprintf(buf,"%02d:a:%01d:%01d",cnr,type,nr);
	server_chat(1028,buf);

	emote(cn,"drinks a stat potion (%d,%d-%d)",type,nr,str);

	return 1;
}


struct dungeonfighter_data
{
	int damage_done;
	int damage_taken;
	int simple_pots_taken;
	int alc_pots_taken;
};

void dungeonfighter(int cn,int ret,int lastact)
{
	int dam,didhit=0,cnr,need,add,nr,flag=0;
	struct dungeonfighter_data *dat;
	struct msg *msg,*next;
	extern struct clan clan[];
	char buf[80];

	dat=set_data(cn,DRD_DUNGEONFIGHTER,sizeof(struct dungeonfighter_data));
	if (!dat) return;	// oops...

	cnr=ch[cn].restx;

	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;
		
                switch(msg->type) {
			case NT_DIDHIT:
				dam=msg->dat2;
				dat->damage_done+=dam;
				didhit=1;
                                break;

			case NT_GOTHIT:
				dam=msg->dat2;
				dat->damage_taken+=dam;
                                break;
		}
	}

	if (ch[cn].value[0][V_MANA] && ch[cn].mana<ch[cn].value[0][V_MANA]*POWERSCALE/2 && dat->simple_pots_taken<5) {
		if (dat->damage_done>10 && dat->damage_done>dat->damage_taken/16) {
			need=ch[cn].value[0][V_MANA]-ch[cn].mana/POWERSCALE; add=nr=0;
			if (need>24 && clan[cnr].dungeon.simple_pot[1][2]) {
				add=24; nr=2;
			} else if (need>12 && clan[cnr].dungeon.simple_pot[1][1]) {
				add=16; nr=1;
			} else if (clan[cnr].dungeon.simple_pot[1][0]) {
				add=8; nr=0;
			}
			if (add) {
				ch[cn].mana=min(ch[cn].value[0][V_MANA]*POWERSCALE,ch[cn].mana+add*POWERSCALE);
				emote(cn,"drinks a mana potion (%d)",add);
				dat->simple_pots_taken++;
				sprintf(buf,"%02d:s:%01d:%01d",cnr,1,nr);
				server_chat(1028,buf);
				flag=1;
			}
		}
		//say(cn,"mana: taken=%d, done=%d, pots=%d",dat->damage_taken,dat->damage_done,dat->simple_pots_taken);
	}

	if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 && dat->simple_pots_taken<5) {
		if (dat->damage_done>10 && dat->damage_done>dat->damage_taken/16) {
			need=ch[cn].value[0][V_HP]-ch[cn].hp/POWERSCALE; add=nr=0;
			if (need>24 && clan[cnr].dungeon.simple_pot[0][2]) {
				add=24; nr=2;
			} else if (need>12 && clan[cnr].dungeon.simple_pot[0][1]) {
				add=16; nr=1;
			} else if (clan[cnr].dungeon.simple_pot[0][0]) {
				add=8; nr=0;
			}
			if (add) {
				ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+add*POWERSCALE);
				emote(cn,"drinks a healing potion (%d)",add);
				dat->simple_pots_taken++;
				sprintf(buf,"%02d:s:%01d:%01d",cnr,0,nr);
				server_chat(1028,buf);
				flag=1;
			}
		}
		//say(cn,"hp: taken=%d, done=%d, pots=%d",dat->damage_taken,dat->damage_done,dat->simple_pots_taken);
	}
	if ((ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2 || ch[cn].mana<ch[cn].value[0][V_MANA]*POWERSCALE/2) && dat->simple_pots_taken<5 && !flag) {
		if (dat->damage_done>10 && dat->damage_done>dat->damage_taken/16) {
			need=max(ch[cn].value[0][V_HP]-ch[cn].hp/POWERSCALE,ch[cn].value[0][V_MANA]-ch[cn].mana/POWERSCALE); add=nr=0;
			if (need>24 && clan[cnr].dungeon.simple_pot[2][2]) {
				add=24; nr=2;
			} else if (need>12 && clan[cnr].dungeon.simple_pot[2][1]) {
				add=16; nr=1;
			} else if (clan[cnr].dungeon.simple_pot[2][0]) {
				add=8; nr=0;
			}
			if (add) {
				ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+add*POWERSCALE);
				ch[cn].mana=min(ch[cn].value[0][V_MANA]*POWERSCALE,ch[cn].mana+add*POWERSCALE);
				ch[cn].endurance=min(ch[cn].value[0][V_ENDURANCE]*POWERSCALE,ch[cn].endurance+add*POWERSCALE);
				emote(cn,"drinks a combo potion (%d)",add);
				dat->simple_pots_taken++;
				sprintf(buf,"%02d:s:%01d:%01d",cnr,2,nr);
				server_chat(1028,buf);
			}
		}
		//say(cn,"hp: taken=%d, done=%d, pots=%d",dat->damage_taken,dat->damage_done,dat->simple_pots_taken);
	}

	if (didhit && dat->alc_pots_taken<3 && dat->damage_done>0 && ch[cn].hp>ch[cn].value[0][V_HP]*POWERSCALE/2) {
                if (dungeon_potion(cn)) { dat->alc_pots_taken++; }
	}

	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_DUNGEONMASTER:		dungeonmaster(cn,ret,lastact); return 1;
		case CDR_DUNGEONFIGHTER:	dungeonfighter(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_DUNGEONTELE:	dungeonteleport(in,cn); return 1;
		case IDR_DUNGEONFAKE:	dungeonfake(in,cn); return 1;
		case IDR_DUNGEONDOOR:	dungeondoor(in,cn); return 1;
		case IDR_DUNGEONKEY:	dungeonkey(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_DUNGEONMASTER:		immortal_dead(cn,co); return 1;
		case CDR_DUNGEONFIGHTER:	fighter_dead(cn,co); return 1;

		default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:		return 0;
	}
}























