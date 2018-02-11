/*

$Id: warped.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: warped.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2003/10/15 17:06:44  sam
added hell stone for four keys to warpmaster driver

Revision 1.2  2003/10/13 14:13:02  sam
Added RCS tags


*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "player_driver.h"
#include "task.h"
#include "poison.h"
#include "sector.h"
#include "lab.h"
#include "player.h"
#include "area.h"
#include "misc_ppd.h"
#include "consistency.h"

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
	{{"warped","world",NULL},"This world has been created by Rodney, the Mighty Mage. Well, actually, Ishtar created it while he was trying out designs for his Labyrinth, but Rodney added the final touches. He tried to create a Labyrinth, just like Ishtar. Anyway. Do you want to buy some °c4keys°c0 and °c4explore°c0 it?",0},
	{{"keys",NULL},"You need keys to open the various doors here. Each key will only work once, so you'll need plenty of them. I'll trade one key for an earth °c4stone°c0, two keys for a fire stone, three keys for an ice stone and four keys for a hell stone. Just hand me the stones if you want to trade.",0},
	{{"stone",NULL},"I need the stones to power the °c4Warped World°c0. Rodney created a device that will draw power from them.",0},
	{{"explore",NULL},"It might be worth the trouble. Adventurers report that °c4dangers°c0 and rewards are to be found inside. Oh, and one word of warning: Don't venture into the blue area before you're level 70.",0},
	{{"dangers",NULL},"It is said that enemies hide behind red doors. I've also heard that people can get stuck, with no way to progress. Should that happen to you, ask me to °c4reset°c0 your current points. You will not lose the level reached.",0},
	{{"reset",NULL},NULL,2}

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

struct warpfighter_data
{
	int co,cser;
	int tx,ty;
	int xs,xe,ys,ye;
	int creation_time;
	int pot_done;
};

#define MAXWARPBONUS	50

struct warped_ppd
{
	int base;
	int points;

	int bonusID[MAXWARPBONUS];
	int bonuslast_used[MAXWARPBONUS];

	int nostepexp;
};

struct target_list
{
	int x,y;
};

struct target_list tl[25]={
	// green teleport
	{247,243},	// green key
	{226,91},	// orange key
	{215,41},	// red key
	{197,41},	// blue key
	{191,41},	// white key

	// orange teleport
	{247,243},	// green key
	{179,41},	// orange key
	{251,41},	// red key
	{173,41},	// blue key
	{161,41},	// white key

	// red teleport
	{247,243},	// green key
	{111,48},	// orange key
	{161,49},	// red key
	{207,7},	// blue key
	{206,250},	// white key

	// blue teleport
	{247,243},	// green key
	{207,227},	// orange key
	{201,149},	// red key
	{176,250},	// blue key
	{167,192},	// white key

	// white teleport
	{247,243},	// green key
	{169,251},	// orange key
	{145,251},	// red key
	{127,251},	// blue key
	{151,251}	// white key
};

void warpteleport_driver(int in,int cn)
{
	int in2,target,n,flag=0;

	if (!cn) return;

	if (!it[in].drdata[0]) {
		switch(it[in].drdata[1]) {
			case 1:		flag=teleport_char_driver(cn,242,252); break;
			case 2:		flag=teleport_char_driver(cn,247,66); break;
			case 3:		flag=teleport_char_driver(cn,251,16); break;
			case 4:		flag=teleport_char_driver(cn,152,7); break;
			case 5:		flag=teleport_char_driver(cn,183,250); break;

			default:	log_char(cn,LOG_SYSTEM,0,"You found BUG #31as5.");
		}
		if (!flag) log_char(cn,LOG_SYSTEM,0,"Target is busy, please try again soon.");
		return;
	}

	if (!(in2=ch[cn].citem) || it[in2].ID!=IID_AREA25_TELEKEY) {
                log_char(cn,LOG_SYSTEM,0,"Nothing happened.");
                return;
	}

        target=(it[in].drdata[0]-1)*5+(it[in2].drdata[0]-1);
	
	//log_char(cn,LOG_SYSTEM,0,"target=%d",target);
	if (teleport_char_driver(cn,tl[target].x,tl[target].y)) {
		ch[cn].citem=0; ch[cn].flags|=CF_ITEMS;
		destroy_item(in2);
		flag++;

		for (n=0; n<INVENTORYSIZE; n++) {
			if ((in2=ch[cn].item[n]) && it[in2].ID==IID_AREA25_TELEKEY) {
				remove_item_char(in2);
				destroy_item(in2);
				flag++;
			}
		}
		if (flag) log_char(cn,LOG_SYSTEM,0,"Your sphere%s vanished.",flag>0 ? "s" : "");
	}
}

void warpbonus_driver(int in,int cn)
{
	int ID,n,old_n=0,old_val=0,level,in2=0,in3;
	struct warped_ppd *ppd;

	if (!cn) return;

        ppd=set_data(cn,DRD_WARP_PPD,sizeof(struct warped_ppd));
	if (!ppd) return;	// oops...
	if (!ppd->base) ppd->base=40;

	if (ppd->base>139) {
		log_char(cn,LOG_SYSTEM,0,"You're done. Finished. It's over. You're there. You've solved the final level.");
		return;
	}

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXWARPBONUS; n++) {
		if (ppd->bonusID[n]==ID) break;
                if (realtime-ppd->bonuslast_used[n]>old_val) {
			old_val=realtime-ppd->bonuslast_used[n];
			old_n=n;
		}
	}

        if (n==MAXWARPBONUS) n=old_n;
	else if (ppd->bonuslast_used[n]>=ppd->base) {
		log_char(cn,LOG_SYSTEM,0,"Nothing happened.");
		return;
	}

	if (ppd->points+1>=ppd->base/4 && (!(in2=ch[cn].citem) || it[in2].ID!=IID_AREA25_TELEKEY)) {
		log_char(cn,LOG_SYSTEM,0,"Nothing happened. You sense that you'll need one of the spheres this time.");
		return;
	}

        ppd->bonusID[n]=ID;
	ppd->bonuslast_used[n]=ppd->base;

	level=min(ch[cn].level,(int)(ppd->base*0.80));

	ppd->points++;
	if (ppd->points>=ppd->base/4) {
		ppd->points=0;
		ppd->base+=5;
		ppd->nostepexp=0;

		if (ppd->base>139) log_char(cn,LOG_SYSTEM,0,"You've finished the final level.");
		else if (ppd->base>134) log_char(cn,LOG_SYSTEM,0,"You've reached the final level.");
		else log_char(cn,LOG_SYSTEM,0,"You advanced a level! Take care!");

		if (in2 && it[in2].ID==IID_AREA25_TELEKEY) {
			switch(it[in2].drdata[0]) {
				case 1:		give_exp_bonus(cn,level_value(level)/7); log_char(cn,LOG_SYSTEM,0,"You received experience."); break;
				case 2:		if (ch[cn].saves<10 && !(ch[cn].flags&CF_HARDCORE)) { ch[cn].saves++; log_char(cn,LOG_SYSTEM,0,"You received a save."); } break;
				case 3:		log_char(cn,LOG_SYSTEM,0,"You received military rank."); give_military_pts_no_npc(cn,level,0); break;
				case 4:		ch[cn].gold+=level*level*10; ch[cn].flags|=CF_ITEMS; log_char(cn,LOG_SYSTEM,0,"You received %d gold.",level*level/10); break;
				case 5:		in3=create_item("lollipop"); if (give_char_item(cn,in3)) log_char(cn,LOG_SYSTEM,0,"You received a lollipop."); else destroy_item(in3); break;
			}
		}
		if (ppd->base>139) return;
	} else if (!ppd->nostepexp) give_exp_bonus(cn,level_value(level)/70);

	log_char(cn,LOG_SYSTEM,0,"You are at level %d, and you have %d of %d points.",(ppd->base-35)/5,ppd->points,ppd->base/4);	
}

void warpkeyspawn_driver(int in,int cn)
{
	int in2;
        char buf[80];
	
	if (!cn) return;
	
	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

        sprintf(buf,"warped_teleport_key%d",it[in].drdata[0]);
	in2=create_item(buf);

	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"It won't come off.");
		return;
	}

	ch[cn].citem=in2;
	it[in2].carried=cn;
	ch[cn].flags|=CF_ITEMS;
	log_char(cn,LOG_SYSTEM,0,"You got a glowing half sphere.");	
}

void warped_raise(int cn,int base)
{
	int n,in,val;

        for (n=0; n<V_MAX; n++) {
		if (!skill[n].cost) continue;
		if (!ch[cn].value[1][n]) continue;

		switch(n) {
			case V_HP:		val=max(10,base-(base/4)); break;
			case V_ENDURANCE:	val=max(10,base-(base/4)); break;
			
			case V_WIS:		val=max(10,base-(base/5)); break;
			case V_INT:		val=max(10,base-(base/10)); break;
			case V_AGI:		val=max(10,base-(base/10)); break;
			case V_STR:		val=max(10,base-(base/10)); break;

			case V_HAND:		val=max(1,base); break;
			case V_ARMORSKILL:	val=max(1,(base/10)*10); break;
			case V_ATTACK:		val=max(1,base); break;
			case V_PARRY:		val=max(1,base); break;
			case V_IMMUNITY:	val=max(1,base); break;
			
			case V_TACTICS:		val=max(1,base-5); break;
			case V_WARCRY:		val=max(1,base); break;
			case V_SURROUND:	val=max(1,base-20); break;
			case V_BODYCONTROL:	val=max(1,base-20); break;
			case V_SPEEDSKILL:	val=max(1,base-10); break;
			case V_PERCEPT:		val=max(1,base-10); break;
			case V_RAGE:		val=max(1,base-5); break;
			case V_PROFESSION:	val=min(60,max(1,base-5)); break;

			default:		val=max(1,base-40); break;
		}

		val=min(val,120);
		ch[cn].value[1][n]=val;
	}
        ch[cn].exp=ch[cn].exp_used=calc_exp(cn);
	ch[cn].level=exp2level(ch[cn].exp);

	ch[cn].prof[P_LIGHT]=min(30,ch[cn].value[1][V_PROFESSION]);
	ch[cn].prof[P_DARK]=min(30,ch[cn].value[1][V_PROFESSION]);
	if (ch[cn].value[1][V_PROFESSION]>30) {
		ch[cn].prof[P_ATHLETE]=min(30,ch[cn].value[1][V_PROFESSION]-30);
	}


	// create special equipment bonus to equal that of the average player
	in=create_item("equip1");
        for (n=0; n<5; n++) it[in].mod_value[n]=1+base/2.75;
	ch[cn].item[12]=in; it[in].carried=cn;

	in=create_item("equip2");
        for (n=0; n<4; n++) it[in].mod_value[n]=1+base/2.75;
	ch[cn].item[13]=in; it[in].carried=cn;

	in=create_item("equip3");
        for (n=0; n<5; n++) it[in].mod_value[n]=1+base/2.75;
	ch[cn].item[14]=in; it[in].carried=cn;

        in=create_item("armor_spell");
	ch[cn].item[15]=in; it[in].carried=cn;
	it[in].mod_value[0]=max(13,min(123,ch[cn].value[1][V_ARMORSKILL]+10))*20;

	in=create_item("weapon_spell");
	ch[cn].item[16]=in; it[in].carried=cn;
	it[in].mod_value[0]=max(13,min(123,ch[cn].value[1][V_HAND]+10));
}

int warptrialdoor_driver(int in,int cn)
{
	int xs,ys,xe,ye,x,y,m,in2,dx,dy,dir,co;
	struct warpfighter_data *dat;
	struct warped_ppd *ppd;

        ppd=set_data(cn,DRD_WARP_PPD,sizeof(struct warped_ppd));
	if (!ppd) return 2;	// oops...
	if (!ppd->base) ppd->base=40;

        if (!it[in].drdata[2]) {
		xs=xe=ys=ye=in2=0;
	
                for (x=it[in].x+1,y=it[in].y; x<it[in].x+15; x++) {
                        if (map[x+y*MAXMAP].it && it[map[x+y*MAXMAP].it].driver==IDR_WARPTRIALDOOR) {
				in2=map[x+y*MAXMAP].it;
				xs=it[in].x;
				xe=it[in2].x;

				for (x=it[in].x+1,y=it[in].y; y<it[in].y+15; y++) {
					if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { ye=y; break; }
				}
				for (x=it[in].x+1,y=it[in].y; y>it[in].y-15; y--) {
					if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { ys=y; break; }
				}
                                break;
			}
			if (map[x+y*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) break;
		}

		if (!in2) {
			for (x=it[in].x-1,y=it[in].y; x>it[in].x-15; x--) {
                                if (map[x+y*MAXMAP].it && it[map[x+y*MAXMAP].it].driver==IDR_WARPTRIALDOOR) {
					in2=map[x+y*MAXMAP].it;
					xe=it[in].x;
					xs=it[in2].x;
	
					for (x=it[in].x-1,y=it[in].y; y<it[in].y+15; y++) {
						if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { ye=y; break; }
					}
					for (x=it[in].x-1,y=it[in].y; y>it[in].y-15; y--) {
						if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { ys=y; break; }
					}
					break;
				}
				if (map[x+y*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) break;
			}
		}

		if (!in2) {
			for (x=it[in].x,y=it[in].y+1; y<it[in].y+15; y++) {
                                if (map[x+y*MAXMAP].it && it[map[x+y*MAXMAP].it].driver==IDR_WARPTRIALDOOR) {
					in2=map[x+y*MAXMAP].it;
					ys=it[in].y;
					ye=it[in2].y;
	
					for (x=it[in].x,y=it[in].y+1; x<it[in].x+15; x++) {
						if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { xe=x; break; }
					}
					for (x=it[in].x,y=it[in].y+1; x>it[in].x-15; x--) {
						if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { xs=x; break; }
					}
					break;
				}
				if (map[x+y*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) break;
			}
		}

		if (!in2) {
			for (x=it[in].x,y=it[in].y-1; y>it[in].y-15; y--) {
                                if (map[x+y*MAXMAP].it && it[map[x+y*MAXMAP].it].driver==IDR_WARPTRIALDOOR) {
					in2=map[x+y*MAXMAP].it;
					ye=it[in].y;
					ys=it[in2].y;
	
					for (x=it[in].x,y=it[in].y-1; x<it[in].x+15; x++) {
                                                if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { xe=x; break; }
					}
					for (x=it[in].x,y=it[in].y-1; x>it[in].x-15; x--) {
						if (map[x+y*MAXMAP].flags&MF_MOVEBLOCK) { xs=x; break; }
					}
					break;
				}
				if (map[x+y*MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) break;
			}
		}

		//xlog("xs=%d, ys=%d, xe=%d, ye=%d, in=%d, in2=%d",xs,ys,xe,ye,in,in2);
		
		it[in].drdata[2]=xs; it[in].drdata[3]=ys;
		it[in].drdata[4]=xe; it[in].drdata[5]=ye;
		*(unsigned short*)(it[in].drdata+6)=in2;
	}

	if (!cn) return 2;

	xs=it[in].drdata[2]; ys=it[in].drdata[3];
	xe=it[in].drdata[4]; ye=it[in].drdata[5];
	in2=*(unsigned short*)(it[in].drdata+6);

	if (ch[cn].x>=xs && ch[cn].x<=xe && ch[cn].y>=ys && ch[cn].y<=ye) {
		log_char(cn,LOG_SYSTEM,0,"You cannot open the door from this side.");
		return 2;
	}

	for (y=ys+1; y<ye; y++) {		
		for (x=xs+1,m=x+y*MAXMAP; x<xe; x++,m++) {
			if ((co=map[m].ch) && ch[co].driver!=CDR_SIMPLEBADDY) {
                                log_char(cn,LOG_SYSTEM,0,"You hear fighting noises and the door won't open.");
				return 2;
			}
		}
	}
	co=create_char("warped_fighter",0);
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Bug #319i, sorry.");
		return 2;
	}
        if (!drop_char(co,(xs+xe)/2,(ys+ye)/2,0)) {
		log_char(cn,LOG_SYSTEM,0,"Bug #319j, sorry.");
		destroy_char(co);
		return 2;
	}
	ch[co].tmpx=ch[co].x;
	ch[co].tmpy=ch[co].y;

	warped_raise(co,ppd->base);

	update_char(co);

        ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
	ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
	ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
	ch[co].lifeshield=ch[co].value[0][V_MAGICSHIELD]*POWERSCALE;
	
	ch[co].dir=DX_RIGHTDOWN;

	dat=set_data(co,DRD_WARPFIGHTER,sizeof(struct warpfighter_data));
	if (!dat) {
		log_char(cn,LOG_SYSTEM,0,"Bug #319k, sorry.");
		remove_char(co);
		destroy_char(co);
		return 2;
	}
	dir=offset2dx(it[in].x,it[in].y,it[in2].x,it[in2].y);
	if (!dir) {
		log_char(cn,LOG_SYSTEM,0,"Bug #319l, sorry.");
		remove_char(co);
		destroy_char(co);
		return 2;
	}
	dx2offset(dir,&dx,&dy,NULL);

	dat->co=cn; dat->cser=ch[cn].serial;
	dat->tx=it[in2].x+dx;
	dat->ty=it[in2].y+dy;
	dat->xs=xs; dat->xe=xe;
	dat->ys=ys; dat->ye=ye;

        teleport_char_driver(cn,it[in].x+dx,it[in].y+dy);

	return 1;
}

int warpkeydoor_driver(int in,int cn)
{
	int in2,dx,dy;

	if (!cn) return 1;

        dx=it[in].x-ch[cn].x;
	dy=it[in].y-ch[cn].y;
	if (!dx && !dy) {
		log_char(cn,LOG_SYSTEM,0,"Bug #329i, sorry.");
		return 2;
	}

	in2=has_item(cn,IID_AREA25_DOORKEY);
	
	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"The door is locked and you do not have the right key.");
		return 2;
	}

	if (teleport_char_driver(cn,it[in].x+dx,it[in].y+dy)) {
		log_char(cn,LOG_SYSTEM,0,"A %s vanished.",it[in2].name);
		remove_item_char(in2);
		destroy_item(in2);
                switch(ch[cn].dir) {
			case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
			case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
			case DX_UP:	ch[cn].dir=DX_DOWN; break;
			case DX_DOWN:	ch[cn].dir=DX_UP; break;
		}
		return 1;
	} else {
		log_char(cn,LOG_SYSTEM,0,"Oops. Please try again.");
		return 2;
	}
}

void warpfighter(int cn,int ret,int lastact)
{
        struct warpfighter_data *dat;
	struct msg *msg,*next;
        int co,in,fre;

	dat=set_data(cn,DRD_WARPFIGHTER,sizeof(struct warpfighter_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch(msg->type) {
			case NT_CREATE:
                                fight_driver_set_dist(cn,40,0,40);
				dat->creation_time=ticker;
                                break;			

			case NT_TEXT:
				co=msg->dat3;
				tabunga(cn,co,(char*)msg->dat2);
				break;			
		}
		

                standard_message_driver(cn,msg,1,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	co=dat->co;
	if (!ch[co].flags || ch[co].serial!=dat->cser || ch[co].x<dat->xs || ch[co].y<dat->ys || ch[co].x>dat->xe || ch[co].y>dat->ye) {
		remove_char(cn);
		destroy_char(cn);
		//xlog("self-destruct %d %d %d %d %d %d (%d)",!ch[co].flags,ch[co].serial!=dat->cser,ch[co].x<dat->xs,ch[co].y<dat->ys,ch[co].x>dat->xe,ch[co].y>dat->ye,dat->co);
		return;
	}

	if (dat->pot_done<1 && ticker>dat->creation_time+TICKS*2) {
		dat->pot_done++;
		if (ch[cn].level>60 && !RANDOM(6)) {
			if (RANDOM(2)) {
				emote(cn,"drinks a potion of freeze");
				ch[cn].value[1][V_FREEZE]=ch[cn].value[1][V_ATTACK]+ch[cn].value[1][V_ATTACK]/4;
				ch[cn].value[1][V_MANA]=10;
				update_char(cn);
				ch[cn].mana=POWERSCALE*10;
			} else {
                                if ((fre=may_add_spell(cn,IDR_FREEZE)) && (in=create_item("freeze_spell"))) {
					emote(cn,"drinks a spoiled potion of freeze");
					it[in].mod_value[0]=-ch[cn].value[0][V_SPEED]-100;
					it[in].driver=IDR_FREEZE;
					it[in].carried=cn;
                                        ch[cn].item[fre]=in;
					*(signed long*)(it[in].drdata)=ticker+TICKS*60;
					*(signed long*)(it[in].drdata+4)=ticker;
					create_spell_timer(cn,in,fre);
					update_char(cn);
				}
			}
		}
	}

	if (ch[cn].lifeshield<POWERSCALE*5 && ch[cn].endurance<ch[cn].value[0][V_WARCRY]*POWERSCALE/3 && dat->pot_done<3) {
		dat->pot_done++;
		if (ch[cn].level>50 && !RANDOM(4)) {
			emote(cn,"drinks an endurance potion");
			ch[cn].endurance=min(ch[cn].value[0][V_ENDURANCE]*POWERSCALE,ch[cn].endurance+32*POWERSCALE);
		}
	}
	
	if (ch[cn].hp<ch[cn].value[0][V_ENDURANCE]*POWERSCALE/2 && dat->pot_done<5) {
		dat->pot_done++;
		if (ch[cn].level>40 && !RANDOM(4)) {
			emote(cn,"drinks a healing potion");
			ch[cn].hp=min(ch[cn].value[0][V_HP]*POWERSCALE,ch[cn].hp+32*POWERSCALE);
		}
	}

        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

        do_idle(cn,TICKS);
}

void warpfighter_died(int cn,int co)
{
	struct warpfighter_data *dat;

	dat=set_data(cn,DRD_WARPFIGHTER,sizeof(struct warpfighter_data));
	if (!dat) return;	// oops...

	if (dat->co!=co) { xlog("1"); return; }
        if (!ch[co].flags || ch[co].serial!=dat->cser || ch[co].x<dat->xs || ch[co].y<dat->ys || ch[co].x>dat->xe || ch[co].y>dat->ye) { xlog("2"); return; }
	
	teleport_char_driver(co,dat->tx,dat->ty);
	//xlog("3 %d %d",dat->tx,dat->ty);
}

void warpmaster(int cn,int ret,int lastact)
{
        int co,in,type,in2,flag=0,code,n;
	struct msg *msg,*next;
	struct warped_ppd *ppd;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
                        co=msg->dat1;

			// dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // dont talk to the same person twice
			if (mem_check_driver(cn,co,7)) { remove_message(cn,msg); continue; }

			if (ch[co].level<30) say(cn,"Hello %s! You'd better leave this area - it is too dangerous for you.",ch[co].name);
                        else say(cn,"Hello %s! Welcome to Rodney's °c4Warped World°c0! Would you like to buy some °c4keys°c0?",ch[co].name);
			mem_add_driver(cn,co,7);
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			code=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co);
			if (code==2 && (ppd=set_data(co,DRD_WARP_PPD,sizeof(struct warped_ppd)))) {	// reset
				ppd->points=0;
				for (n=0; n<MAXWARPBONUS; n++) ppd->bonuslast_used[n]=0;
				ppd->nostepexp=1;
				say(cn,"Done.");
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

			if ((in=ch[cn].citem)) {
				
				type=it[in].drdata[0];
                                if (it[in].ID==IID_ALCHEMY_INGREDIENT) {
					if (type==23) {
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						say(cn,"Here you go, one key.");
					}
					if (type==21) {
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						say(cn,"Here you go, two keys.");

					}
					if (type==22) {
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						say(cn,"Here you go, three keys.");
					}
					if (type==24) {
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						in2=create_item("warped_door_key"); if (give_char_item(co,in2)) flag=1; else destroy_item(in2);
						say(cn,"Here you go, four keys.");
					}
				}
				if (flag || !give_char_item(co,in)) {
					destroy_item(ch[cn].citem);
				}
				ch[cn].citem=0;
			}
		}

                remove_message(cn,msg);
	}
        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (spell_self_driver(cn)) return;

	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

        if (ticker%345600==0) {
		mem_erase_driver(cn,7);
	}

        do_idle(cn,TICKS);
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_WARPFIGHTER:	warpfighter(cn,ret,lastact); return 1;
		case CDR_WARPMASTER:	warpmaster(cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_WARPTELEPORT:	warpteleport_driver(in,cn); return 1;
		case IDR_WARPTRIALDOOR:	return warptrialdoor_driver(in,cn);
		case IDR_WARPBONUS:	warpbonus_driver(in,cn); return 1;
		case IDR_WARPKEYSPAWN:	warpkeyspawn_driver(in,cn); return 1;
		case IDR_WARPKEYDOOR:	return warpkeydoor_driver(in,cn);

		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_WARPFIGHTER:   warpfighter_died(cn,co); return 1;
		case CDR_WARPMASTER:	return 1;

                default:		return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_WARPFIGHTER:	return 1;
		case CDR_WARPMASTER:	return 1;

		default:		return 0;
	}
}
