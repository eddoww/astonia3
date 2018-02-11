/*
 *
 * $Id: create.c,v 1.5 2007/08/13 18:50:38 devel Exp $
 *
 * $Log: create.c,v $
 * Revision 1.5  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.4  2005/12/04 17:41:10  ssim
 * added second test for negative lifeshield to create_char_nr()
 *
 * Revision 1.3  2005/11/22 23:42:29  ssim
 * added support for fire and ice demon player sprites
 *
 * Revision 1.2  2005/11/21 12:07:42  ssim
 * added logic for player-used demon sprite
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <sys/mman.h>
#include <errno.h>

#include "server.h"
#include "tool.h"
#include "log.h"
#include "skill.h"
#include "light.h"
#include "player.h"
#include "direction.h"
#include "timer.h"
#include "mem.h"
#include "notify.h"
#include "libload.h"
#include "sector.h"
#include "drvlib.h"
#include "area.h"
#include "date.h"
#include "create.h"
#include "respawn.h"
#include "balance.h"
#include "map.h"
#include "consistency.h"
#include "btrace.h"
#include "item_id.h"

#define safecpy(a,b) { strncpy(a,b,sizeof(a)-1); a[sizeof(a)-1]=0; }

#ifndef PAGESIZE
#define PAGESIZE	4096
#endif

#define MAXTCHARS	256
#define MAXTITEM	1500

int used_chars,used_tchars;
int used_items,used_titems;
static int tmp_a=1;

void protect(void *mem,unsigned int size)
{
	void *ptr;
	unsigned int len,off;

	ptr=(void*)((unsigned int)(((unsigned char*)(mem)+PAGESIZE-1))&(~(PAGESIZE-1)));
	off=(char*)(ptr)-(char*)(mem);
	len=(size-off)&(~(PAGESIZE-1));

	xlog("protect: ptr=%p/%p, len=%X/%X, off=%d",ptr,mem,len,size,off);

	if (mprotect(ptr,len,PROT_READ)) {
		elog("mprotect failed: %d",errno);
		exit(1);
	}
}

// zone load from definition file
static char *load_zone(char *name)
{
	int handle,len;
	char *text;

        handle=open(name,O_RDONLY);
	if (handle==-1) { elog("ERROR: Could not open zone \"%s\".",name); return NULL; }

	len=lseek(handle,0,SEEK_END);
	if (len==-1) { close(handle); elog("ERROR: Could not seek to end of zone \"%s\".",name); return NULL; }
	
	if (lseek(handle,0,SEEK_SET)==-1) { close(handle); elog("ERROR: Could not seek to start of zone \"%s\".",name); return NULL; }

	text=xmalloc(len+1,IM_TEMP);
	if (!text) { close(handle); elog("ERROR: Could not malloc memory for zone \"%s\".",name); return NULL; }
	
	if (read(handle,text,len)!=len) { xfree(text); close(handle); elog("ERROR: Could not read zone \"%s\".",name); return NULL; }
	text[len]=0;

	close(handle);

        return text;
}

// load all zones ending in mask and call process on their content
static int load_zones(char *dirname,char *mask,int (*process)(char*))
{
	DIR *dir;
	struct dirent *de;
	char name[NAME_MAX],*ptr;

        dir=opendir(dirname);
	if (!dir) return 1;

	while ((de=readdir(dir))) {
		if (!endcmp(de->d_name,mask)) continue;

		sprintf(name,"./%s/%s",dirname,de->d_name);
		
		ptr=load_zone(name);
		if (ptr) {
			xlog("Processing zone \"%s\".",name);
			if (!process(ptr)) {
				xfree(ptr);
				closedir(dir);
				return 0;
			}
			xfree(ptr);
		} else {
			closedir(dir);
			return 0;
		}

	}	
	closedir(dir);

	return 1;
}

static char *get_text(char *name,char *value,char *ptr)
{
	int len;

	while (isspace(*ptr)) ptr++;

	if (*ptr==';') { name[0]=';'; name[1]=0; value[0]=';'; value[1]=0; return ptr+1; }

	if (*ptr=='#') {
		name[0]='#'; name[1]=0; value[0]='#'; value[1]=0;
		while (*ptr && *ptr!='\n' && *ptr!='\r') ptr++;		
		return ptr;
	}

	// read name
	for (len=0; len<255 && (isalnum(*ptr) || *ptr=='_' || *ptr=='-'); len++) *name++=*ptr++;
	*name=0;

	while (isspace(*ptr)) ptr++;

	// read command
	if (*ptr==':') { value[0]=':'; value[1]=0; return ptr+1; }

	if (*ptr!='=') { value[0]=0; return NULL; }
	else ptr++;

	while (isspace(*ptr)) ptr++;

	if (*ptr=='"') { // read value with quotation marks
		ptr++;
		for (len=0; len<255 && *ptr && *ptr!='"'; len++) *value++=*ptr++;
		*value=0;

		if (*ptr!='"') return NULL;
		else return ptr+1;		
	}
	
	// read value without quotation marks
        for (len=0; len<255 && (isalnum(*ptr) || *ptr=='_' || *ptr=='-'); len++) *value++=*ptr++;
	*value=0;

	return ptr;
}

static char *V_tab[V_MAX]={
"V_HP",
"V_ENDURANCE",
"V_MANA",

"V_WIS",
"V_INT",
"V_AGI",
"V_STR",

"V_ARMOR",
"V_WEAPON",
"V_LIGHT",
"V_SPEED",

"V_PULSE",
"V_DAGGER",
"V_HAND",
"V_STAFF",
"V_SWORD",
"V_TWOHAND",

"V_ARMORSKILL",
"V_ATTACK",
"V_PARRY",
"V_WARCRY",
"V_TACTICS",
"V_SURROUND",
"V_BODYCONTROL",
"V_SPEEDSKILL",

"V_BARTER",
"V_PERCEPT",
"V_STEALTH",

"V_BLESS",
"V_HEAL",
"V_FREEZE",
"V_MAGICSHIELD",
"V_FLASH",
"V_FIREBALL",
"V_ARCANE",

"V_REGENERATE",
"V_MEDITATE",
"V_IMMUNITY",
"V_DEMON",
"V_DURATION",
"V_RAGE",
"V_COLD",
"V_PROFESSION"
};

static int lookup_V(char *name)
{
	int n;

	for (n=0; n<V_MAX; n++) {
		if (!strcasecmp(V_tab[n],name)) return n;
	}

	return -1;
}

static char *P_tab[P_MAX]={
	"P_ATHLETE",		//0
	"P_ALCHEMIST",		//1
	"P_MINER",		//2
	"P_ASSASSIN",		//3
	"P_THIEF",		//4
	"P_LIGHT",		//5
	"P_DARK",		//6
	"P_TRADER",		//7
	"P_MERCENARY",		//8
	"P_CLAN",		//9
	"P_HERBALIST",		//10
	"P_DEMON",		//11
	"P_NULL",		//12
	"P_NULL",		//13
	"P_NULL",		//14
	"P_NULL",		//15
	"P_NULL",		//16
	"P_NULL",		//17
	"P_NULL",		//18
	"P_NULL"		//19
};

static int lookup_P(char *name)
{
	int n;

	for (n=0; n<P_MAX; n++) {
		if (!strcasecmp(P_tab[n],name)) return n;
	}

	return -1;
}

static char *WN_tab[]={
"WN_NECK",
"WN_HEAD",
"WN_CLOAK",
"WN_ARMS",
"WN_BODY",
"WN_BELT",
"WN_RHAND",
"WN_LEGS",
"WN_LHAND",
"WN_RRING",
"WN_FEET",
"WN_LRING"
};

static int lookup_WN(char *name)
{
	int n;

	for (n=0; n<12; n++) {
		if (!strcasecmp(WN_tab[n],name)) return n;
	}

	return -1;
}

static char *IF_tab[]=
{
"IF_USED",
"IF_MOVEBLOCK",
"IF_SIGHTBLOCK",
"IF_TAKE",
"IF_USE",
"IF_WNHEAD",
"IF_WNNECK",
"IF_WNBODY",
"IF_WNARMS",
"IF_WNBELT",
"IF_WNLEGS",
"IF_WNFEET",
"IF_WNLHAND",
"IF_WNRHAND",
"IF_WNCLOAK",
"IF_WNLRING",
"IF_WNRRING",
"IF_WNTWOHANDED",
"IF_AXE",
"IF_DAGGER",
"IF_HAND",
"IF_SHANON",
"IF_STAFF",
"IF_SWORD",
"IF_TWOHAND",
"IF_DOOR",
"IF_QUEST",
"IF_SOUNDBLOCK",
"IF_STEPACTION",
"IF_MONEY",
"IF_NODECAY",
"IF_FRONTWALL",
"IF_DEPOT",
"IF_NODEPOT",
"IF_NODROP",
"IF_NOJUNK",
"IF_PLAYERBODY",
"IF_BONDTAKE",
"IF_BONDWEAR",
"IF_LABITEM",
"IF_VOID",
"IF_NOENHANCE",
"IF_BEYONDBOUNDS",
"IF_BEYONDMAXMOD"
};

static int lookup_IF(char *name)
{
	int n;

	for (n=0; n<sizeof(IF_tab)/sizeof(char*); n++) {
		if (!strcasecmp(IF_tab[n],name)) return n;
	}

	return -1;
}

static char *CF_tab[]={
"CF_USED",			//0
"CF_IMMORTAL",			//1
"CF_GOD",			//2
"CF_PLAYER",			//3
"CF_STAFF",			//4
"CF_INVISIBLE",			//5
"CF_SHUTUP",			//6
"CF_KICKED",			//7
"CF_UPDATE",			//8
"CF_STUNNED",			//9
"CF_UNCONSCIOUS",		//10
"CF_DEAD",			//11
"CF_ITEMS",			//12
"CF_RESPAWN",			//13
"CF_MALE",			//14
"CF_FEMALE",			//15
"CF_WARRIOR",			//16
"CF_MAGE",			//17
"CF_ARCH",			//18
"CF_BERSERK",			//19
"CF_NOATTACK",			//20
"CF_HASNAME",			//21
"CF_QUESTITEM",			//22
"CF_INFRARED",			//23
"CF_PK",			//24
"CF_ITEMDEATH",			//25
"CF_NODEATH",			//26
"CF_NOBODY",			//27
"CF_EDEMON",			//28
"CF_FDEMON",			//29
"CF_IDEMON",			//30
"CF_NOGIVE",			//31
"CF_PLAYERLIKE",		//32
"CF_RESERVED0",			//33
"CF_PAID",			//34
"CF_PROF",			//35
"CF_ALIVE",			//36
"CF_DEMON",			//37
"CF_UNDEAD",			//38
"CF_HARDKILL",			//39
"CF_NOBLESS",			//40
"CF_AREACHANGE",		//41
"CF_LAG",			//42
"CF_WON",			//43
"CF_THIEFMODE",			//44
"CF_NOTELL",			//45
"CF_INFRAVISION",		//46
"CF_NOMAGIC",			//47
"CF_NONOMAGIC",			//48
"CF_OXYGEN",			//49
"CF_NOPLRATT",			//50
"CF_ALLOWSWAP",			//51
"CF_LQMASTER",			//52
"CF_HARDCORE",			//53
"CF_NONOTIFY",			//54
"CF_SMALLUPDATE"		//55
};

static int lookup_CF(char *name)
{
	int n;

	for (n=0; n<sizeof(CF_tab)/sizeof(char*); n++) {
		if (!strcasecmp(CF_tab[n],name)) return n;
	}

	return -1;
}

static char *MF_tab[]={
"MF_MOVEBLOCK",			//0
"MF_SIGHTBLOCK",		//1
"MF_TMOVEBLOCK",		//2
"MF_TSIGHTBLOCK",		//3
"MF_INDOORS",			//4
"MF_RESTAREA",			//5
"MF_DOOR",			//6
"MF_SOUNDBLOCK",		//7
"MF_TSOUNDBLOCK",		//8
"MF_SHOUTBLOCK",		//9
"MF_CLAN",			//10
"MF_ARENA",			//11
"MF_PEACE",			//12
"MF_NEUTRAL",			//13
"MF_FIRETHRU",			//14
"MF_SLOWDEATH",			//15
"MF_NOLIGHT",			//16
"MF_NOMAGIC",			//17
"MF_UNDERWATER",		//18
"MF_NOREGEN"			//19
};

static int lookup_MF(char *name)
{
	int n;

	for (n=0; n<sizeof(MF_tab)/sizeof(char*); n++) {
		if (!strcasecmp(MF_tab[n],name)) return n;
	}

	return -1;
}

struct it_temp
{
	char name[80];
	struct item item;
} *it_temp;

#define MAXRANDITEM	10

struct ch_temp
{
	char name[80];
	struct character ch;
	int rand_item[MAXRANDITEM];
	int rand_prob[MAXRANDITEM];
	int special_prob;
        int special_str;
	int special_base;
	int gold_prob;
	int gold_base;
	int gold_random;
} *ch_temp;

static int add_item(struct it_temp *itt)
{
	int n;
	
	for (n=1; n<MAXTITEM; n++)
		if (!it_temp[n].name[0]) break;
	if (n==MAXTITEM) return 0;

	it_temp[n]=*itt;
	used_titems++;

        return 1;
}

int lookup_item(char *name)
{
	int n;
	
	for (n=1; n<MAXTITEM; n++)
		if (!strcmp(it_temp[n].name,name)) return n;
	return 0;
}

unsigned char trans1hex(char src)
{
	if (isdigit(src)) return src-'0';
	if (src>='A' && src<='F') return src-'A'+10;
	if (src>='a' && src<='f') return src-'a'+10;
	
	return 0;	
}

unsigned char trans2hex(char *src)
{
	return trans1hex(*src)*16+trans1hex(*(src+1));
}

static void trans_arg(unsigned char *dst,char *src)
{
	int n;

	for (n=0; n<IT_DR_SIZE && *src && *(src+1); n++) {
		dst[n]=trans2hex(src); src+=2;
	}
	
}

static int trans_ID(char *src)
{
	return strtol(src,NULL,16);
}

static int process_item(char *ptr)
{
	char name[256],value[256];
	struct it_temp itt;
	int gotname=0,line=1;
	int cmod=0,tmp;

	while ((ptr=get_text(name,value,ptr))) {
		line++;

		// ignore comments
		if (name[0]=='#') continue;

                // error checking...
		if (!gotname && value[0]!=':') {
			elog("Error in line %d: Need name before values (%100.100s).",line,ptr);
			return 0;
		}
		if (gotname && value[0]==':') {
			elog("Error in line %d: Need semicolon first (%100.100s).",line,ptr);
			return 0;
		}
		if (!gotname && value[0]==';') {
			elog("Error in line %d: Need name before semicolon (%100.100s).",line,ptr);
			return 0;
		}

		// parse control keywords
		if (value[0]==':') {
			bzero(&itt,sizeof(itt));
			strcpy(itt.name,name);
			itt.item.flags=IF_USED;
			gotname=1;
			cmod=0;
			continue;
		}
		if (value[0]==';') {
			if (!add_item(&itt)) {
				elog("Error in line %d: add_item() failed (%100.100s).",line,ptr);
				return 0;
			}
			gotname=0;
			continue;
		}

		// parse values
		if (!strcasecmp(name,"name")) safecpy(itt.item.name,value)
		else if (!strcasecmp(name,"description")) safecpy(itt.item.description,value)
		else if (!strcasecmp(name,"value")) itt.item.value=atoi(value);
                else if (!strcasecmp(name,"sprite")) itt.item.sprite=atoi(value);
		else if (!strcasecmp(name,"driver")) itt.item.driver=atoi(value);
		else if (!strcasecmp(name,"min_level")) itt.item.min_level=atoi(value);
		else if (!strcasecmp(name,"max_level")) itt.item.max_level=atoi(value);
		else if (!strcasecmp(name,"needs_class")) itt.item.needs_class=atoi(value);
		else if (!strcasecmp(name,"ID")) itt.item.ID=trans_ID(value);
		else if (!strcasecmp(name,"arg")) trans_arg(itt.item.drdata,value);
		else if (!strcasecmp(name,"mod_index")) {
			tmp=lookup_V(value);
			if (tmp==-1) {
				elog("Error in line %d: Unknown V_name \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
			itt.item.mod_index[cmod]=tmp;
		} else if (!strcasecmp(name,"req_index")) {
			tmp=lookup_V(value);
			if (tmp==-1) {
				elog("Error in line %d: Unknown V_name \"%s\". (%100.100s)",line,value,ptr);
				return 0;
			}
			itt.item.mod_index[cmod]=-tmp;
		} else if (!strcasecmp(name,"mod_value") || !strcasecmp(name,"req_value")) {
			if (cmod>=MAXMOD) {
				elog("Error in line %d: MAXMOD (%d) exceeded (%100.100s).",line,MAXMOD,ptr);
				return 0;
			}
			itt.item.mod_value[cmod]=atoi(value);
			cmod++;
		} else if (!strcasecmp(name,"flag")) {
			tmp=lookup_IF(value);
			if (tmp==-1) {
				elog("Error in line %d: Unknown IF \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
			itt.item.flags|=(1ull<<tmp);			
		} else {
			elog("Item: Error in line %d: unknown name \"%s\" (%100.100s).",line,name,ptr);
			return 0;
		}

	}
	if (gotname) {
		elog("Error in line %d: Premature end of input (or syntax error) (%100.100s).",line,ptr);
		return 0;
	}
	return 1;
}

static int add_char(struct ch_temp *cht)
{
	int n;
	
	for (n=1; n<MAXTCHARS; n++)
		if (!ch_temp[n].name[0]) break;
	if (n==MAXTCHARS) return 0;

	//if ((cht->ch.flags&CF_QUESTITEM) && !(cht->ch.flags&(CF_IMMORTAL|CF_NOATTACK))) elog("******\nchar %s has questitem but no immortal/noattack flag\n******",cht->ch.name);

	ch_temp[n]=*cht;
	used_tchars++;

        return 1;
}

int lookup_char(char *name)
{
	int n;
	
	for (n=1; n<MAXTCHARS; n++)
		if (!strcmp(ch_temp[n].name,name)) return n;
	return 0;
}

static int process_char(char *ptr)
{
	char name[256],value[256];
	struct ch_temp cht;
	int gotname=0,line=1;
	int citem=30,tmp,wn,ritem=0;
	int cspell=12;

	while ((ptr=get_text(name,value,ptr))) {
		line++;

		// ignore comments
		if (name[0]=='#') continue;

                // error checking...
		if (!gotname && value[0]!=':') {
			elog("Error in line %d: Need name before values (%100.100s).",line,ptr);
			return 0;
		}
		if (gotname && value[0]==':') {
			elog("Error in line %d: Need semicolon first (%100.100s).",line,ptr);
			return 0;
		}
		if (!gotname && value[0]==';') {
			elog("Error in line %d: Need name before semicolon (%100.100s).",line,ptr);
			return 0;
		}

		// parse control keywords
		if (value[0]==':') {
			bzero(&cht,sizeof(cht));
			strcpy(cht.name,name);
			cht.ch.flags=CF_USED;
			cht.ch.respawn=RESPAWN_TIME;
			gotname=1;
			continue;
		}
		if (value[0]==';') {
			if (!add_char(&cht)) {
				elog("Error in line %d: add_char() failed (%100.100s).",line,ptr);
				return 0;
			}
			gotname=0;
			citem=30;
			cspell=12;
			ritem=0;
			continue;
		}

                // parse values
		if (!strcasecmp(name,"name")) safecpy(cht.ch.name,value)
		else if (!strcasecmp(name,"description")) safecpy(cht.ch.description,value)
		else if (!strcasecmp(name,"sprite")) cht.ch.sprite=atoi(value);
		else if (!strcasecmp(name,"sound")) cht.ch.sound=atoi(value);
		else if (!strcasecmp(name,"gold")) cht.ch.gold=atoi(value);
		else if (!strcasecmp(name,"driver")) cht.ch.driver=atoi(value);
		else if (!strcasecmp(name,"group")) cht.ch.group=atoi(value);
		else if (!strcasecmp(name,"class")) cht.ch.class=atoi(value);
		else if (!strcasecmp(name,"respawn")) cht.ch.respawn=atoi(value)*TICKS;
		else if (!strcasecmp(name,"special_prob")) cht.special_prob=atoi(value);
		else if (!strcasecmp(name,"special_strength")) cht.special_str=atoi(value);
                else if (!strcasecmp(name,"special_base")) cht.special_base=atoi(value);
		else if (!strcasecmp(name,"gold_prob")) cht.gold_prob=atoi(value);
		else if (!strcasecmp(name,"gold_base")) cht.gold_base=atoi(value);
		else if (!strcasecmp(name,"gold_random")) cht.gold_random=atoi(value);
		else if (!strcasecmp(name,"arg")) {
			if (cht.ch.arg) {
				cht.ch.arg=xrealloc(cht.ch.arg,strlen(cht.ch.arg)+strlen(value)+1,IM_CHARARGS);
				if (!cht.ch.arg) {
					elog("Error in line %d: Memory low! (%100.100s)",line,ptr);
					return 0;
				}
				strcat(cht.ch.arg,value);
			} else {
				cht.ch.arg=xstrdup(value,IM_CHARARGS);
			}
		} else if (!strcasecmp(name,"item")) {
			tmp=lookup_item(value);
			if (tmp==0) {
				elog("Error in line %d: Unknown item name \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
			if (citem>=INVENTORYSIZE) {
				elog("Error in line %d: Carrying capacity (%d) exceeded (%100.100s).\n",line,INVENTORYSIZE-30,ptr);
				return 0;
			}
			cht.ch.item[citem++]=tmp;			
		} else if (!strcasecmp(name,"spell")) {
			tmp=lookup_item(value);
			if (tmp==0) {
				elog("Error in line %d: Unknown spell name \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
			if (cspell>=30) {
				elog("Error in line %d: Carrying capacity (18) exceeded (%100.100s).\n",line,ptr);
				return 0;
			}
			cht.ch.item[cspell++]=tmp;
		} else if (!strcasecmp(name,"ritem")) {
			tmp=lookup_item(value);
			if (tmp==0) {
				elog("Error in line %d: Unknown r-item name \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
			if (ritem>=MAXRANDITEM) {
				elog("Error in line %d: Random carrying capacity (%d) exceeded (%100.100s).\n",line,MAXRANDITEM,ptr);
				return 0;
			}
			if (!cht.rand_prob[ritem]) {
				elog("Error in line %d: No random value set (%100.100s).\n",line,ptr);
				return 0;
			}
			cht.rand_item[ritem++]=tmp;
		} else if (!strcasecmp(name,"rprob")) {
			tmp=atoi(value);
			if (tmp<1 || tmp>9999) {
				elog("Error in line %d: Illegal probability \"%s\". (%100.100s)",line,value,ptr);
				return 0;
			}
                        cht.rand_prob[ritem]=tmp;
		} else if (!strcasecmp(name,"flag")) {
			tmp=lookup_CF(value);
			if (tmp==-1) {
				elog("Error in line %d: Unknown CF \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
			cht.ch.flags|=(1ull<<tmp);
		} else if ((tmp=lookup_V(name))!=-1) {
                        cht.ch.value[1][tmp]=atoi(value);
		} else if ((wn=lookup_WN(name))!=-1) {
                        tmp=lookup_item(value);
			if (tmp==0) {
				elog("Error in line %d: Unknown item name \"%s\" (%100.100s).",line,value,ptr);
				return 0;
			}
                        cht.ch.item[wn]=tmp;			
		} else if ((tmp=lookup_P(name))!=-1) {
                        cht.ch.prof[tmp]=atoi(value);
		} else {
			elog("Char: Error in line %d: unknown name \"%s\" (%100.100s).",line,name,ptr);
			return 0;
		}

	}
	if (gotname) {
		elog("Error in line %d: Premature end of input (or syntax error) (%100.100s).",line,ptr);
		return 0;
	}
	return 1;
}

static int comma(char *one,char *two,char *ptr)
{
	while (isspace(*ptr)) ptr++;
	while (isdigit(*ptr)) *one++=*ptr++;
	while (isspace(*ptr)) ptr++;
	if (*ptr!=',') return 0;
	ptr++;

	while (isdigit(*ptr)) *two++=*ptr++;
	while (isspace(*ptr)) ptr++;
	if (*ptr) return 0;

	*one=*two=0;

	return 1;
}

int create_item_nr(int tmp)
{
	int n;

	n=alloc_item();
	if (!n) {
		elog("create_item(): MAXITEM reached!");
		return 0;
	}

        it[n]=it_temp[tmp].item;
	it[n].flags|=IF_USED;
	it[n].serial=serin++;

        /*if (it[n].driver==IDR_DEMONSHRINE) {
		static int cnt=0;
		xlog("created shrine %d.",++cnt);
	}
	if (it[n].driver==IDR_ITEMSPAWN) {
                xlog("created spawner %s.",it_temp[tmp].name);
	}

	if (it[n].driver==IDR_CLANSPAWN) {
                xlog("created clan-spawner %s.",it_temp[tmp].name);
	}

	if (it[n].driver==IDR_ORBSPAWN) {
                xlog("created orb-spawner level %d.",it_temp[tmp].item.min_level);
	}

	if (it[n].driver==IDR_RANDCHEST) {
		static int cnt=0;
                xlog("created random chest %d, level %d (avg %.2fG, max %.2fG).",
		     ++cnt,
		     it_temp[tmp].item.drdata[0],
		     it_temp[tmp].item.drdata[0]*it_temp[tmp].item.drdata[0]/400.0,
		     it_temp[tmp].item.drdata[0]*it_temp[tmp].item.drdata[0]/100.0);
	}

	if (it[n].driver) {
                xlog("created item with driver %d.",it[n].driver);
	}

	if (it[n].driver==IDR_WARPBONUS) {
		static int cnt=0;
                xlog("created warped bonus %d",++cnt);
	}*/



        if (it[n].driver) call_item(it[n].driver,n,0,ticker+1);		

	return n;
}

int create_item(char *name)
{
	int tmp;

	tmp=lookup_item(name);
	if (!tmp) {
		elog("create_item(): could not find item \"%s\".",name);
		btrace("create_item");
		return 0;
	}
	
	return create_item_nr(tmp);
}

int create_char_nr(int ctmp,int tmpa)
{
	int n,m,in,f,itmp;

	n=alloc_char();
	if (!n) {
		elog("create_char(): MAXCHARS reached!");
		return 0;
	}

	copy_char(ch+n,&ch_temp[ctmp].ch);
	
	ch[n].flags|=CF_USED;
	ch[n].serial=sercn++;
	ch[n].tmp=ctmp;
	ch[n].tmpa=tmpa;
	ch[n].exp=ch[n].exp_used=calc_exp(n);
	ch[n].level=exp2level(ch[n].exp);

        if (ch[n].flags&CF_PLAYER) ;
        else if (ch[n].level>1) ch[n].level+=2;
	else ch[n].level+=1;

	if (ch[n].lifeshield<0) {
		elog("lifeshield<0 while creating char %d (%s). fixing. (1)",ctmp,ch[n].name);
		ch[n].lifeshield=0;
		ch_temp[ctmp].ch.lifeshield=0;
	}

	//if (ch[n].flags&CF_DEMON) xlog("created %s (%s - %d), %d exp, level %d (%.2f%%, %d exp to next level)",ch[n].name,ch_temp[ctmp].name,ch[n].tmpa,ch[n].exp,ch[n].level,100.0/80977100*ch[n].exp,level2exp(ch[n].level-1)-ch[n].exp);

        for (m=0; m<INVENTORYSIZE; m++) {
		if ((itmp=ch[n].item[m])) {
			in=create_item_nr(itmp);
			if (!in) {
                                elog("create_char(): create_item_nr() failed.");
				// FIXME: all items created so far are lost!
				free_char(n);
				return 0;
			}
                        ch[n].item[m]=in;
			it[in].carried=n;
		}
	}
	for (m=0,f=30; m<MAXRANDITEM; m++) {
		if ((itmp=ch_temp[ctmp].rand_item[m]) && RANDOM(10000)<ch_temp[ctmp].rand_prob[m]) {
			in=create_item_nr(itmp);
			if (!in) {
				elog("create_char(): create_item_nr() failed.");
				// FIXME: all items created so far are lost!
				free_char(n);
				return 0;
			}
			for ( ; f<INVENTORYSIZE && ch[n].item[f]; f++) ;
			if (f<INVENTORYSIZE) {
				ch[n].item[f]=in;
				it[in].carried=n;
				//xlog("created %s for %s.",it[in].name,ch[n].name);
			} else {
				elog("carrying capacity exceeded");
				// FIXME: all items created so far are lost!
				free_char(n);
				return 0;
			}
		}
	}

	if (ch_temp[ctmp].special_prob && RANDOM(ch_temp[ctmp].special_prob)==0) {
		in=create_special_item(ch_temp[ctmp].special_str,ch_temp[ctmp].special_base,20,10000);
		if (in) {
			for (f=30; f<INVENTORYSIZE && ch[n].item[f]; f++) ;
			if (f<INVENTORYSIZE) {
				ch[n].item[f]=in;
				it[in].carried=n;
				//xlog("created special item %s (%s) for %s.",it[in].name,it[in].description,ch[n].name);
			} else {
				free_item(in);
			}
		}
	}

	if (ch_temp[ctmp].gold_prob && RANDOM(ch_temp[ctmp].gold_prob)==0) {
		ch[n].gold+=ch_temp[ctmp].gold_base+RANDOM(ch_temp[ctmp].gold_random+1);
	}

        notify_char(n,NT_CREATE,ticker,0,0);

	if (ch[n].lifeshield<0) {
		elog("lifeshield<0 while creating char %d (%s). fixing. (2)",ctmp,ch[n].name);
		ch[n].lifeshield=0;
	}

        return n;
}

int create_char(char *name,int tmpa)
{
	int tmp;

	tmp=lookup_char(name);
	if (!tmp) {
		elog("create_char(): could not find char \"%s\".",name);
		return 0;
	}

	//xlog("creating char %s",name);	

	return create_char_nr(tmp,tmpa);
}

static void field2square(int src,int xf,int yf,int xt,int yt)
{
	int x,y;

	for (y=yf; y<=yt; y++) {
		for (x=xf; x<=xt; x++) {
			map[x+y*MAXMAP]=map[src];
		}
	}
}

static int process_map(char *ptr)
{
	char name[256],value[256],one[256],two[256];
        int line=1;
	int x=0,y=0,ox=0,oy=0,tmp;
	int xf=0,yf=0,xt=0,yt=0;

	while ((ptr=get_text(name,value,ptr))) {
		line++;

		// ignore comments
		if (name[0]=='#') continue;

                if (!strcasecmp(name,"origin")) {
			if (!comma(one,two,value)) {
				elog("Error in line %d: Expected two comma seperated values.",line);
				return 0;
			}
			ox=atoi(one);
			oy=atoi(two);
		} else if (!strcasecmp(name,"field")) {
			if (!comma(one,two,value)) {
				elog("Error in line %d: Expected two comma seperated values.",line);
				return 0;
			}
                        x=atoi(one)+ox;
			y=atoi(two)+oy;
                        bzero(map+x+y*MAXMAP,sizeof(struct map));			
		} else if (!strcasecmp(name,"from")) {
			if (!comma(one,two,value)) {
				elog("Error in line %d: Expected two comma seperated values.",line);
				return 0;
			}
			xf=atoi(one)+ox;
			yf=atoi(two)+oy;
		} else if (!strcasecmp(name,"to")) {
			if (!comma(one,two,value)) {
				elog("Error in line %d: Expected two comma seperated values.",line);
				return 0;
			}
			xt=atoi(one)+ox;
			yt=atoi(two)+oy;
			field2square(x+y*MAXMAP,xf,yf,xt,yt);
		} else if (!strcasecmp(name,"gsprite")) map[x+y*MAXMAP].gsprite=atoi(value);
		else if (!strcasecmp(name,"fsprite")) map[x+y*MAXMAP].fsprite=atoi(value);
		else if (!strcasecmp(name,"ch")) {
                        tmp=create_char(value,tmp_a++);
			if (tmp==0) {
				elog("Error in line %d: Unknown character name \"%s\".",line,value);
				return 0;
			}
			map[x+y*MAXMAP].ch=tmp;
			map[x+y*MAXMAP].flags|=MF_TMOVEBLOCK;
			ch[tmp].x=ch[tmp].tmpx=x;
			ch[tmp].y=ch[tmp].tmpy=y;
			ch[tmp].dir=DX_RIGHTDOWN;

			//ch[tmp].group=RANDOM(100);	//!!!!!!!!!!!!

			add_char_sector(tmp);			
			
			if (ch[tmp].flags&CF_RESPAWN) {
				if ((ch[tmp].flags&CF_RESPAWN) && (ch[tmp].flags&(CF_IMMORTAL|CF_NOATTACK))) elog("char %s has both, respawn and immortal/noattack",ch[tmp].name);
				register_npc_to_section(tmp);
				register_respawn_char(tmp);
			}
			if (!(ch[tmp].flags&(CF_RESPAWN|CF_IMMORTAL|CF_PLAYERLIKE))) {
				elog("char %s had neither respawn nor immortal nor playerlike",ch[tmp].name);
			}
			if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) {
				elog("placed char %s at %d,%d on MF_MOVEBLOCK",ch[tmp].name,x,y);
			}			
		} else if (!strcasecmp(name,"it")) {
                        tmp=create_item(value);
			if (tmp==0) {
				elog("Error in line %d: Unknown item name \"%s\" (%d,%d).",line,value,x,y);
				return 0;
			}
			map[x+y*MAXMAP].it=tmp;
			if (it[tmp].flags&IF_MOVEBLOCK) map[x+y*MAXMAP].flags|=MF_TMOVEBLOCK;
			if (it[tmp].flags&IF_SIGHTBLOCK) map[x+y*MAXMAP].flags|=MF_TSIGHTBLOCK;
			if (it[tmp].flags&IF_DOOR) map[x+y*MAXMAP].flags|=MF_DOOR;
			if (it[tmp].flags&IF_SOUNDBLOCK) map[x+y*MAXMAP].flags|=MF_TSOUNDBLOCK;
			it[tmp].x=x;
			it[tmp].y=y;
			if (it[tmp].flags&IF_TAKE) {
				elog("put IF_TAKE item %s on map at %d,%d during map load.",it[tmp].name,x,y);
			}
			// EXPIRE IF_TAKE-ITEMS CREATED HERE???
		} else if (!strcasecmp(name,"flag")) {
			tmp=lookup_MF(value);
			if (tmp==-1) {
				elog("Error in line %d: Unknown MF \"%s\".",line,value);
				return 0;
			}
			map[x+y*MAXMAP].flags|=(1ull<<tmp);
			if (((1ull<<tmp)&MF_MOVEBLOCK) && map[x+y*MAXMAP].ch) {
				elog("placed char %s at %d,%d on MF_MOVEBLOCK",ch[map[x+y*MAXMAP].ch].name,x,y);
			}
		} else {
			elog("Map: Error in line %d: unknown name \"%s\" (%100.100s).",line,name,ptr);
			return 0;
		}
	}

	return 1;
}

static int create_items(void)
{
	char dirname[80];

        if (!load_zones("zones/generic",".itm",process_item)) return 0;

	sprintf(dirname,"zones/%d",areaID);
	if (!load_zones(dirname,".itm",process_item)) return 0;

	return 1;
}

static int create_characters(void)
{
	char dirname[80];

	if (!load_zones("zones/generic",".chr",process_char)) return 0;

	sprintf(dirname,"zones/%d",areaID);
	if (!load_zones(dirname,".chr",process_char)) return 0;

	return 1;
}

static int create_map(void)
{
	char dirname[80];
	
	sprintf(dirname,"zones/%d",areaID);

        return load_zones(dirname,".map",process_map);
}

static void update_chars(void)
{
	int n;

	for (n=getfirst_char(); n; n=getnext_char(n)) {
		if (!(ch[n].flags)) continue;
		update_char(n);
		ch[n].hp=ch[n].value[0][V_HP]*POWERSCALE;
		ch[n].endurance=ch[n].value[0][V_ENDURANCE]*POWERSCALE;
		ch[n].mana=ch[n].value[0][V_MANA]*POWERSCALE;		
	}
}

static void update_items(void)
{
	int n;

	for (n=1; n<MAXITEM; n++) {
		if (!(it[n].flags)) continue;
		add_item_light(n);		
	}
}

static struct character *uchar,*fchar;

// get new character from free list
// and add it to used list
int alloc_char(void)
{
	int cn,n;
	struct character *tmp;

	// get free char
	tmp=fchar;
	if (!tmp) { elog("alloc_char: MAXCHARS reached!"); return 0; }
	fchar=tmp->next;
	cn=tmp-ch;

	// erase character data (pure safety measure
	bzero(ch+cn,sizeof(struct character));
	
	// flags character as used
        ch[cn].flags|=CF_USED;

	// add char to used list
	ch[cn].next=uchar;
	ch[cn].prev=NULL;
	if (uchar) uchar->prev=ch+cn;
	uchar=ch+cn;

	used_chars++;

	//elog("alloc: %d, next=%d, prev=%d uchar=%d",cn,ch[cn].next-ch,ch[cn].prev-ch,uchar-ch);

	// mark char as new in player cache
        for (n=0; n<MAXPLAYER; n++)
		set_player_knows_name(n,cn,0);

	return cn;
}

// release (delete) character to free list
// and remove it from used list
void free_char(int cn)
{
	struct character *next,*prev;

	if (!ch[cn].flags) {
		elog("trying to free already free char %d (%s)",cn,ch[cn].name);
		btrace("free_char");
		return;
	}
	
	// remove character from used list
        prev=ch[cn].prev;
	next=ch[cn].next;

        if (prev) prev->next=next;
	else uchar=next;

	if (next) next->prev=prev;

	// add character to free list
        ch[cn].next=fchar;
	fchar=ch+cn;

	// flag character as unused
	ch[cn].flags=0;

	used_chars--;

	//elog("freed char %d",cn);
}

// get first character in used list
int getfirst_char(void)
{
	//xlog("getfirst: %d",uchar-ch);
	if (!uchar) return 0;	
	return uchar-ch;
}

// get next character in used list
// (following a getfirst)
int getnext_char(int cn)
{
	struct character *tmp;

        if (!(tmp=ch[cn].next)) return 0;

	//xlog("getnext2: %d",tmp-ch);

	return tmp-ch;	
}

// copy character template src to character dst
// use this to preserve the used list
void copy_char(struct character *dst,struct character *src)
{
	struct character *next,*prev;

	next=dst->next;
	prev=dst->prev;

	*dst=*src;

	dst->next=next;
	dst->prev=prev;
}

static struct item *fitem;

// get new item from free list
int alloc_item(void)
{
	int in;
	struct item *tmp;
	
	tmp=fitem;
	if (!tmp) { elog("alloc_item: MAXITEM reached!"); return 0; }

	fitem=tmp->next;

	in=tmp-it;

	if (it[in].flags&IF_USED) {
		elog("alloc_item: got used item!");
		btrace("alloc_item");
	}

	it[in].flags|=IF_USED;	

	used_items++;

        return in;
}

// release (delete) item to free list
void free_item(int in)
{
	if (!it[in].flags) {
		elog("trying to free already free item %d (%s)",in,it[in].name);
		btrace("free_item");
                return;
	}
        if (it[in].content) {
		elog("trying to free item %s (%d) with content set (%d)",it[in].name,in,it[in].content);
		btrace("free_item");
                return;
	}
	if (it[in].flags&IF_VOID) {
		elog("trying to free item %s (%d) with void set",it[in].name,in);
		btrace("free_item");
                return;
	}

	it[in].flags=0;

	it[in].next=fitem;
	fitem=it+in;

	used_items--;	
}

static void calc_daylight(void)
{
	int x,y;

	for (y=0; y<MAXMAP; y++) {
		for (x=0; x<MAXMAP; x++) {
			if (map[x+y*MAXMAP].flags&MF_INDOORS) {
				compute_dlight(x,y);
			} else {
				compute_shadow(x,y);
			}
			compute_groundlight(x,y);
		}
	}
}

void add_map_border(void)
{
	int x,y;

	for (x=0; x<MAXMAP; x++) {
		map[x+0*MAXMAP].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[x+0*MAXMAP].flags&=~MF_FIRETHRU;

		map[x+1*MAXMAP].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[x+1*MAXMAP].flags&=~MF_FIRETHRU;

		map[x+(MAXMAP-1)*MAXMAP].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[x+(MAXMAP-1)*MAXMAP].flags&=~MF_FIRETHRU;

		map[x+(MAXMAP-2)*MAXMAP].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[x+(MAXMAP-2)*MAXMAP].flags&=~MF_FIRETHRU;
	}

	for (y=0; y<MAXMAP; y++) {
		map[y*MAXMAP+0].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[y*MAXMAP+0].flags&=~MF_FIRETHRU;

		map[y*MAXMAP+1].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[y*MAXMAP+1].flags&=~MF_FIRETHRU;

		map[y*MAXMAP+MAXMAP-1].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[y*MAXMAP+MAXMAP-1].flags&=~MF_FIRETHRU;

		map[y*MAXMAP+MAXMAP-2].flags|=MF_MOVEBLOCK|MF_SIGHTBLOCK|MF_SHOUTBLOCK|MF_SOUNDBLOCK;
		map[y*MAXMAP+MAXMAP-2].flags&=~MF_FIRETHRU;
	}
}

int init_create(void)
{
	int n;

	map=xcalloc(sizeof(struct map)*MAXMAP*MAXMAP,IM_BASE);
	if (!map) return 0;
	xlog("Allocated map: %.2fM (%d*%d)",sizeof(struct map)*MAXMAP*MAXMAP/1024.0/1024.0,sizeof(struct map),MAXMAP*MAXMAP);
	mem_usage+=sizeof(struct map)*MAXMAP*MAXMAP;

	it=xcalloc(sizeof(struct item)*MAXITEM,IM_BASE);
	if (!it) return 0;
	xlog("Allocated items: %.2fM (%d*%d)",sizeof(struct item)*MAXITEM/1024.0/1024.0,sizeof(struct item),MAXITEM);
	mem_usage+=sizeof(struct item)*MAXITEM;

	ch=xcalloc(sizeof(struct character)*MAXCHARS,IM_BASE);
	if (!ch) return 0;
	xlog("Allocated characters: %.2fM (%d*%d)",sizeof(struct item)*MAXCHARS/1024.0/1024.0,sizeof(struct character),MAXCHARS);
	mem_usage+=sizeof(struct item)*MAXCHARS;

	it_temp=xcalloc(sizeof(struct it_temp)*MAXTITEM,IM_BASE);
	if (!it_temp) return 0;
	xlog("Allocated item templates: %.2fM (%d*%d)",sizeof(struct it_temp)*MAXTITEM/1024.0/1024.0,sizeof(struct it_temp),MAXTITEM);
	mem_usage+=sizeof(struct it_temp)*MAXTITEM;

	ch_temp=xcalloc(sizeof(struct ch_temp)*MAXTCHARS,IM_BASE);
	if (!ch_temp) return 0;
	xlog("Allocated character templates: %.2fM (%d*%d)",sizeof(struct ch_temp)*MAXTCHARS/1024.0/1024.0,sizeof(struct ch_temp),MAXTCHARS);
	mem_usage+=sizeof(struct ch_temp)*MAXTCHARS;

	// add characters to free list
	for (n=1; n<MAXCHARS; n++) { ch[n].flags=CF_USED; free_char(n); }

	// add items to free list
	for (n=1; n<MAXITEM; n++) { it[n].flags=IF_USED; free_item(n); }

	used_chars=used_items=used_tchars=used_titems=0;

        if (!create_items()) return 0;
	if (!create_characters()) return 0;
	if (!create_map()) return 0;
	add_map_border();

	// we need to update chars and items AFTER we finished creating the map to
	// avoid errors in the light computation
	update_chars();
	update_items();

	xlog("Created %d character and %d item templates",used_tchars,used_titems);
	xlog("Created %d characters and %d items",used_chars,used_items);

	xlog("Calculating daylight");
	calc_daylight();

	if (areaID==32) {
		protect(ch_temp,sizeof(struct ch_temp)*MAXTCHARS);
		protect(it_temp,sizeof(struct it_temp)*MAXTITEM);
	}

        return 1;
}

static int armor_skill_req(int in)
{
	int n,req=0;

	if (!in) return 0;

	for (n=0; n<MAXMOD; n++) {
		if (it[in].mod_index[n]==-V_ARMORSKILL) {
			req=max(req,it[in].mod_value[n]);
		}
	}
	return req;
}

static int armor_skill_bonus(int cn)
{
	int req=0,bonus,tmp,used=0;

	tmp=armor_skill_req(ch[cn].item[WN_BODY])*50;
	if (tmp) { req+=tmp; used+=50; }
	tmp=armor_skill_req(ch[cn].item[WN_HEAD])*20;
	if (tmp) { req+=tmp; used+=20; }
	tmp=armor_skill_req(ch[cn].item[WN_LEGS])*15;
	if (tmp) { req+=tmp; used+=15; }
	tmp=armor_skill_req(ch[cn].item[WN_ARMS])*15;
	if (tmp) { req+=tmp; used+=15; }

	if (!used) return 0;

	req/=used;
	bonus=(ch[cn].value[1][V_ARMORSKILL]-req)*5*used/100;

	return bonus;
}

void update_char(int cn)
{
	int mod[V_MAX],base[V_MAX],beyond[V_MAX],n,in,m,v1,v2,v3,oldlight,fn,bless[V_MAX],infrared=0,newlight,nonomagic=0,oxygen=0;
	unsigned int mmf;

	if (!(ch[cn].flags)) kill(getpid(),11);
	
	oldlight=ch[cn].value[0][V_LIGHT];

	bzero(mod,sizeof(mod));
	bzero(beyond,sizeof(beyond));
	bzero(base,sizeof(base));
	bzero(bless,sizeof(bless));

	if (ch[cn].x>0 && ch[cn].y>0 && ch[cn].x<MAXMAP && ch[cn].y<MAXMAP) mmf=map[ch[cn].x+ch[cn].y*MAXMAP].flags;
	else mmf=0;

	// Add modifiers from items
	for (n=0; n<30; n++) {
		if ((in=ch[cn].item[n])) {
			if (in<1 || in>=MAXITEM) {
				elog("character %s (%d) has illegal item (nr %d out of bounds). Removing item.",ch[cn].name,cn,in);
				ch[cn].item[n]=0;
				continue;
			}
			if (it[in].driver==IDR_INFRARED) infrared=1;
			if (it[in].driver==IDR_NONOMAGIC) nonomagic=1;
			if (it[in].driver==IDR_OXYGEN) oxygen=1;
			for (m=0; m<MAXMOD; m++) {
				if (!it[in].mod_value[m]) continue;	// ignore empty ones
				 v1=it[in].mod_index[m];
				 if (v1<=-V_MAX || v1>=V_MAX) {
					 elog("item %s (%d) has illegal modifier (index %d out of bounds). Removing modifier.",it[in].name,in,v1);
					 it[in].mod_value[m]=0;
					 it[in].mod_index[m]=0;
					 continue;
				 }
				 if (v1>=0) {	// negative indizes are requirements
					 if (it[in].flags&IF_BEYONDMAXMOD) beyond[v1]+=it[in].mod_value[m];
                                         else if (!(ch[cn].flags&CF_WARRIOR) && it[in].driver==IDR_BLESS) bless[v1]+=it[in].mod_value[m];
					 else mod[v1]+=it[in].mod_value[m];
				 }
			}			
		}
	}

	if (nonomagic) ch[cn].flags|=CF_NONOMAGIC;
	else ch[cn].flags&=~CF_NONOMAGIC;

	if (oxygen) ch[cn].flags|=CF_OXYGEN;
	else ch[cn].flags&=~CF_OXYGEN;

	// add light from effects
	for (n=0; n<4; n++) {
		if ((fn=ch[cn].ef[n])) {
			if (fn<1 || fn>=MAXEFFECT) {
				elog("character %s (%d) has illegal effect (nr %d out of bounds). Removing effect.",ch[cn].name,cn,fn);
				ch[cn].ef[n]=0;
				continue;
			}
			mod[V_LIGHT]+=ef[fn].light;
		}
	}

	// Add base values and raised values and build total
	for (n=0; n<V_MAX; n++) {
		if (n==V_DEMON) {
			ch[cn].value[0][n]=min(ch[cn].value[0][n],ch[cn].value[1][n]);
                        continue;
		}
		if (n==V_COLD) {
			ch[cn].value[0][n]=ch[cn].value[1][n]=mod[n];
                        continue;
		}

		// Add base values
		v1=skill[n].base1;
		v2=skill[n].base2;
		v3=skill[n].base3;
		if (v1!=-1 && v2!=-1 && v3!=-1)
			base[n]+=(ch[cn].value[0][v1]+ch[cn].value[0][v2]+ch[cn].value[0][v3])/5;
		
		// Set total value
		if (!ch[cn].value[1][n] && n>=V_PULSE) ch[cn].value[0][n]=0;
		else {
                        if (n<=V_STR || n>=V_PULSE) {
				
				if ((ch[cn].flags&(CF_MAGE|CF_WARRIOR))==(CF_MAGE|CF_WARRIOR))
					mod[n]=min((int)(ch[cn].value[1][n]*0.725),mod[n]);	// reduce item and spell mods to max 72.5% of base value for seyans
				else mod[n]=min((int)(ch[cn].value[1][n]*0.500),mod[n]);	// reduce item and spell mods to max 50% of base value for warrior/mage
				
				bless[n]=min((int)(ch[cn].value[1][n]*0.500),bless[n]);
			}

			if (n>=V_PULSE) {
				base[n]=min(base[n],max(15,ch[cn].value[1][n]*2));
			}

			if (!(ch[cn].flags&CF_NOMAGIC) || (ch[cn].flags&CF_NONOMAGIC) || n==V_WEAPON || n==V_ARMOR || n==V_LIGHT) {
                                ch[cn].value[0][n]=base[n]+ch[cn].value[1][n]+mod[n]+bless[n];	// boni from base attributes plus skill value plus mods plus bless for mages
			} else {
				ch[cn].value[0][n]=base[n]+ch[cn].value[1][n];
			}

			if (ch[cn].prof[P_LIGHT] && n>=V_WIS && n<=V_STR && hour>=6 && hour<18) {	// day bonus for master of light
				ch[cn].value[0][n]+=ch[cn].prof[P_LIGHT]/2;
			}
			if (ch[cn].prof[P_DARK] && n>=V_WIS && n<=V_STR && (hour<6 || hour>=18)) {	// night bonus for master of dark
				ch[cn].value[0][n]+=ch[cn].prof[P_DARK]/2;
			}
                        if (ch[cn].prof[P_CLAN] && n>=V_WIS && n<=V_STR && (areaID==13 || (mmf&MF_CLAN))) {	// bonus for clan master in catacombs and in clan areas
				ch[cn].value[0][n]+=ch[cn].prof[P_CLAN];
			}
		}

		ch[cn].value[0][n]+=beyond[n];
		
		// force hp to >=1 and other powers and base attribs to >=0
		if (n==V_HP && ch[cn].value[0][n]<0) ch[cn].value[0][n]=1;
                else if (n<=V_STR && ch[cn].value[0][n]<0) ch[cn].value[0][n]=0;
	}

	if (ch[cn].value[1][V_SPEEDSKILL]) ch[cn].value[0][V_SPEED]+=ch[cn].value[0][V_SPEEDSKILL]/2;
	if (ch[cn].prof[P_ATHLETE]) ch[cn].value[0][V_SPEED]+=ch[cn].prof[P_ATHLETE]*3;
	if (ch[cn].prof[P_THIEF]) {
                if (ch[cn].flags&CF_THIEFMODE) ch[cn].value[0][V_STEALTH]+=ch[cn].prof[P_THIEF]*2;
		else ch[cn].value[0][V_STEALTH]+=ch[cn].prof[P_THIEF];
		ch[cn].value[0][V_PERCEPT]+=ch[cn].prof[P_THIEF]/2;
	}

	if (ch[cn].prof[P_DEMON] && (ch[cn].flags&CF_DEMON)) {
		if (ch[cn].value[1][V_HAND]) ch[cn].value[0][V_HAND]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_DAGGER]) ch[cn].value[0][V_DAGGER]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_STAFF]) ch[cn].value[0][V_STAFF]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_SWORD]) ch[cn].value[0][V_SWORD]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_TWOHAND]) ch[cn].value[0][V_TWOHAND]+=ch[cn].prof[P_DEMON];
		
		if (ch[cn].value[1][V_ATTACK]) ch[cn].value[0][V_ATTACK]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_PARRY]) ch[cn].value[0][V_PARRY]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_TACTICS]) ch[cn].value[0][V_TACTICS]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_IMMUNITY]) ch[cn].value[0][V_IMMUNITY]+=ch[cn].prof[P_DEMON];
		
		if (ch[cn].value[1][V_FLASH]) ch[cn].value[0][V_FLASH]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_FIREBALL]) ch[cn].value[0][V_FIREBALL]+=ch[cn].prof[P_DEMON];
		if (ch[cn].value[1][V_FREEZE]) ch[cn].value[0][V_FREEZE]+=ch[cn].prof[P_DEMON];		
	}

	if (ch[cn].value[1][V_BODYCONTROL]) {
		ch[cn].value[0][V_ARMOR]+=ch[cn].value[0][V_BODYCONTROL]*5;
		ch[cn].value[0][V_WEAPON]+=ch[cn].value[0][V_BODYCONTROL]/4;

		if ((!(in=ch[cn].item[WN_RHAND]) || !(it[in].flags&IF_WEAPON) || (it[in].flags&IF_HAND)) && (ch[cn].flags&CF_PLAYER)) ch[cn].value[0][V_WEAPON]+=min(90,ch[cn].value[0][V_BODYCONTROL]/2);
	} else ch[cn].value[0][V_ARMOR]+=get_spell_average(cn)*17.5;

	if (ch[cn].value[1][V_ARMORSKILL]) ch[cn].value[0][V_ARMOR]+=armor_skill_bonus(cn);

	if (ch[cn].x && oldlight!=ch[cn].value[0][V_LIGHT]) {
                newlight=ch[cn].value[0][V_LIGHT];
		if (newlight!=oldlight) {
			ch[cn].value[0][V_LIGHT]=oldlight;
			if (oldlight) remove_char_light(cn);
			ch[cn].value[0][V_LIGHT]=newlight;
			if (newlight) add_char_light(cn);
		}
	}

        ch[cn].flags|=CF_UPDATE;	// we changed values, so update client side display	

        if (ch[cn].hp>ch[cn].value[0][V_HP]*POWERSCALE) ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
	if (ch[cn].endurance>ch[cn].value[0][V_ENDURANCE]*POWERSCALE) ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
	if (ch[cn].mana>ch[cn].value[0][V_MANA]*POWERSCALE) ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

	// sprite changes to players
        if ((ch[cn].flags&CF_PLAYER) && (!(ch[cn].flags&(CF_GOD)) || (ch[cn].sprite>=60 && ch[cn].sprite<120) || ch[cn].sprite==27 || ch[cn].sprite==157 || ch[cn].sprite==39)) {
		int inr,inl,off,sbase;
		int demon1_count=0;
		int demon2_count=0;
		int demon3_count=0;

		inr=ch[cn].item[WN_RHAND];
		inl=ch[cn].item[WN_LHAND];

		if (!inr || !(it[inr].flags&IF_WEAPON)) {	// no weapon
			if (inl && it[inl].drdata[0]) off=3;			// torch
			else off=0;						// nothing
		} else {	// weapon
			if (inl && it[inl].drdata[0]) off=4;			// torch and one-hand weapon
			else if ((it[inr].flags&IF_WNTWOHANDED)) off=2;		// two-handed weapon
			else off=1;
		}
		
		switch(ch[cn].flags&(CF_WARRIOR|CF_MAGE|CF_MALE|CF_FEMALE|CF_ARCH)) {
			case (CF_MAGE|CF_MALE): 			sbase=60; break;
			case (CF_MAGE|CF_MALE|CF_ARCH): 		sbase=65; break;
			case (CF_MAGE|CF_FEMALE): 			sbase=75; break;
			case (CF_MAGE|CF_FEMALE|CF_ARCH): 		sbase=70; break;

			case (CF_WARRIOR|CF_MALE): 			sbase=85; break;
			case (CF_WARRIOR|CF_MALE|CF_ARCH): 		sbase=80; break;
			case (CF_WARRIOR|CF_FEMALE): 			sbase=95; break;
			case (CF_WARRIOR|CF_FEMALE|CF_ARCH):	 	sbase=90; break;
			
			case (CF_MAGE|CF_WARRIOR|CF_MALE): 		sbase=105; break;
			case (CF_MAGE|CF_WARRIOR|CF_MALE|CF_ARCH): 	sbase=100; break;
			case (CF_MAGE|CF_WARRIOR|CF_FEMALE): 		sbase=115; break;
			case (CF_MAGE|CF_WARRIOR|CF_FEMALE|CF_ARCH): 	sbase=110; break;

			default:					sbase=0; break;
		}
		if ((in=ch[cn].item[WN_HEAD])  && it[in].ID==IID_DEMONSKIN1) demon1_count++;
		if ((in=ch[cn].item[WN_ARMS])  && it[in].ID==IID_DEMONSKIN1) demon1_count++;
		if ((in=ch[cn].item[WN_LEGS])  && it[in].ID==IID_DEMONSKIN1) demon1_count++;
		if ((in=ch[cn].item[WN_BODY])  && it[in].ID==IID_DEMONSKIN1) demon1_count++;
		if ((in=ch[cn].item[WN_CLOAK]) && it[in].ID==IID_DEMONSKIN1) demon1_count++;
		if ((in=ch[cn].item[WN_FEET])  && it[in].ID==IID_DEMONSKIN1) demon1_count++;

		if ((in=ch[cn].item[WN_HEAD])  && it[in].ID==IID_DEMONSKIN2) demon2_count++;
		if ((in=ch[cn].item[WN_ARMS])  && it[in].ID==IID_DEMONSKIN2) demon2_count++;
		if ((in=ch[cn].item[WN_LEGS])  && it[in].ID==IID_DEMONSKIN2) demon2_count++;
		if ((in=ch[cn].item[WN_BODY])  && it[in].ID==IID_DEMONSKIN2) demon2_count++;
		if ((in=ch[cn].item[WN_CLOAK]) && it[in].ID==IID_DEMONSKIN2) demon2_count++;
		if ((in=ch[cn].item[WN_FEET])  && it[in].ID==IID_DEMONSKIN2) demon2_count++;

		if ((in=ch[cn].item[WN_HEAD])  && it[in].ID==IID_DEMONSKIN3) demon3_count++;
		if ((in=ch[cn].item[WN_ARMS])  && it[in].ID==IID_DEMONSKIN3) demon3_count++;
		if ((in=ch[cn].item[WN_LEGS])  && it[in].ID==IID_DEMONSKIN3) demon3_count++;
		if ((in=ch[cn].item[WN_BODY])  && it[in].ID==IID_DEMONSKIN3) demon3_count++;
		if ((in=ch[cn].item[WN_CLOAK]) && it[in].ID==IID_DEMONSKIN3) demon3_count++;
		if ((in=ch[cn].item[WN_FEET])  && it[in].ID==IID_DEMONSKIN3) demon3_count++;

		if (demon1_count==6) { sbase=27; off=0; }
		if (demon2_count==6) { sbase=157; off=0; }
		if (demon3_count==6) { sbase=39; off=0; }

		if (sbase) {
			if (ch[cn].sprite!=sbase+off) {
				if (ch[cn].sprite==27 || sbase==27 ||
				    ch[cn].sprite==157 || sbase==257 ||
				    ch[cn].sprite==39 || sbase==39) reset_name(cn);	// must reset colors if changing to/from demon sprite
                                ch[cn].sprite=sbase+off;
				set_sector(ch[cn].x,ch[cn].y);
				
			}
		}
	}

	if ((infrared && !(ch[cn].flags&CF_INFRAVISION)) || (!infrared && (ch[cn].flags&CF_INFRAVISION)) ) {
                ch[cn].flags^=CF_INFRAVISION;
		player_reset_map_cache(ch[cn].player);		
	}
}

int destroy_items(int cn)
{
	int n,in;

	for (n=0; n<INVENTORYSIZE; n++)
		if ((in=ch[cn].item[n])) {
			remove_item_char(in);
			destroy_item(in);
		}

	if ((in=ch[cn].citem)) {
		remove_item_char(in);
		destroy_item(in);
	}

	return 1;
}
