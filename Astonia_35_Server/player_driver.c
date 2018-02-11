/*
 * $Id: player_driver.c,v 1.5 2008/04/14 14:39:11 devel Exp $
 *
 * $Log: player_driver.c,v $
 * Revision 1.5  2008/04/14 14:39:11  devel
 * added questlog hint
 *
 * Revision 1.4  2007/09/11 17:08:17  devel
 * dont use LF before FR
 *
 * Revision 1.3  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include "server.h"
#include "client.h"
#define NEED_PLAYER_STRUCT
#include "player.h"
#undef NEED_PLAYER_STRUCT
#include "notify.h"
#include "do.h"
#include "act.h"
#include "drvlib.h"
#include "error.h"
#include "tool.h"
#include "log.h"
#include "drdata.h"
#include "player_driver.h"
#include "date.h"
#include "talk.h"
#include "item_id.h"
#include "area.h"
#include "create.h"
#include "skill.h"
#include "area1.h"
#include "consistency.h"
#include "database.h"
#include "lostcon.h"
#include "path.h"
#include "questlog.h"

static void insert_queue(int nr,int ac,int ac1,int ac2)
{
	int n;

        for (n=0; n<15; n++) {
		if (!player[nr]->queue[n].action) break;				// we're done		
	}
	//if (n==16) return;
	// new orders get inserted automatically into last slot
	
	player[nr]->queue[n].action=ac;
	player[nr]->queue[n].act1=ac1;
	player[nr]->queue[n].act2=ac2;
}

void player_driver_stop(int nr,int nofight)
{
        if (!player[nr]) return;
        bzero(player[nr]->queue,sizeof(player[nr]->queue));
	player[nr]->action=PAC_IDLE;
	player[nr]->next_fightback_cn=0;
	player[nr]->next_fightback_ticker=0;
	if (nofight) player[nr]->nofight_timer=ticker;
}

void player_driver_halt(int nr)
{
	if (!player[nr]) return;
        player[nr]->action=PAC_IDLE;
	player[nr]->next_fightback_cn=0;
	player[nr]->next_fightback_ticker=0;
}

void player_driver_move(int nr,int x,int y)
{
	player[nr]->action=PAC_MOVE;
	player[nr]->act1=x;
	player[nr]->act2=y;
}

void player_driver_take(int nr,int in)
{
	player[nr]->action=PAC_TAKE;
	player[nr]->act1=in;
	player[nr]->act2=it[in].serial;
}
void player_driver_drop(int nr,int x,int y)
{
        player[nr]->action=PAC_DROP;
	player[nr]->act1=x;
	player[nr]->act2=y;
}
void player_driver_use(int nr,int in)
{
	player[nr]->action=PAC_USE;
	player[nr]->act1=in;
	player[nr]->act2=it[in].serial;
}
void player_driver_teleport(int nr,int tel)
{
	player[nr]->action=PAC_TELEPORT;
	player[nr]->act1=tel;
}
void player_driver_kill(int nr,int co)
{
	player[nr]->action=PAC_KILL;
	player[nr]->act1=co;
	player[nr]->act2=ch[co].serial;
	player[nr]->isautoaction=0;
}
void player_driver_give(int nr,int co)
{
	player[nr]->action=PAC_GIVE;
	player[nr]->act1=co;
	player[nr]->act2=ch[co].serial;
}
void player_driver_charspell(int nr,int spell,int co)
{
	insert_queue(nr,spell,co,ch[co].serial);
}
void player_driver_mapspell(int nr,int spell,int x,int y)
{
	insert_queue(nr,spell,x,y);
}
void player_driver_selfspell(int nr,int spell)
{
	insert_queue(nr,spell,0,0);
}

static int error_state(void)
{
	switch(error) {
		case ERR_ALREADY_WORKING:	return 0;
		case ERR_UNCONCIOUS:		return 0;
		default:			return 2;
	}
}

static int error_state_mana(void)
{
	switch(error) {
		case ERR_ALREADY_WORKING:	return 0;
		case ERR_UNCONCIOUS:		return 0;
		case ERR_MANA_LOW:		return 0;
		default:			return 2;
	}
}

static int check_high_prio_task(int cn,int ac,int ac1,int ac2)
{
	switch(ac) {	
		case PAC_BLESS:		if (do_bless(cn,ac1)) return 1; else return error_state_mana();
		case PAC_HEAL:		if (do_heal(cn,ac1)) return 1; else return error_state();
		case PAC_MAGICSHIELD:	if (do_magicshield(cn)) return 1; else return error_state();		
	}
	return 0;
}

static int check_med_prio_task(int cn,int ac,int ac1,int ac2)
{
	switch(ac) {	
                case PAC_FREEZE:	if (do_freeze(cn)) return 1; else return error_state();
                case PAC_FLASH:		if (do_flash(cn)) return 1; else return error_state();
		case PAC_WARCRY:	if (do_warcry(cn)) return 1; else return error_state();
	}
	return 0;
}

static int check_low_prio_task(int cn,int ac,int ac1,int ac2)
{
	switch(ac) {	
                case PAC_FIREBALL:	if (do_fireball(cn,ac1,ac2)) return 1; else return error_state();
		case PAC_FIREBALL2:	if (fireball_driver(cn,ac1,ac2)) return 1; else return error_state();
                case PAC_BALL:		if (do_ball(cn,ac1,ac2)) return 1; else return error_state();
		case PAC_BALL2:		if (ball_driver(cn,ac1,ac2)) return 1; else return error_state();
	}
	return 0;
}

static int run_queue(int nr,int cn)
{
	int n,ret,noflash=0;

	for (n=0; n<16; n++) {
		if (!player[nr]->queue[n].action) break;	// done
		//say(cn,"nr=%d, ac=%d",n,player[nr]->queue[n].action);
		if ((ret=check_high_prio_task(cn,player[nr]->queue[n].action,player[nr]->queue[n].act1,player[nr]->queue[n].act2))) {
			if (n<15) memmove(player[nr]->queue+n,player[nr]->queue+n+1,sizeof(player[nr]->queue[0])*(16-n-1));
			player[nr]->queue[15].action=PAC_IDLE;
			if (ret==1) return 1;
		}
	}
	for (n=0; n<16; n++) {
		if (!player[nr]->queue[n].action) break;	// done
		//say(cn,"nr=%d, ac=%d",n,player[nr]->queue[n].action);
		
		if (player[nr]->queue[n].action==PAC_FIREBALL2 && player[nr]->queue[n].act1==cn) noflash=1;
		if (noflash==1 && player[nr]->queue[n].action==PAC_FLASH) continue;

		if ((ret=check_med_prio_task(cn,player[nr]->queue[n].action,player[nr]->queue[n].act1,player[nr]->queue[n].act2))) {
			if (n<15) memmove(player[nr]->queue+n,player[nr]->queue+n+1,sizeof(player[nr]->queue[0])*(16-n-1));
			player[nr]->queue[15].action=PAC_IDLE;
			if (ret==1) return 1;
		}
	}
	for (n=0; n<16; n++) {
		if (!player[nr]->queue[n].action) break;	// done
		//say(cn,"nr=%d, ac=%d",n,player[nr]->queue[n].action);
		if ((ret=check_low_prio_task(cn,player[nr]->queue[n].action,player[nr]->queue[n].act1,player[nr]->queue[n].act2))) {
			if (n<15) memmove(player[nr]->queue+n,player[nr]->queue+n+1,sizeof(player[nr]->queue[0])*(16-n-1));
			player[nr]->queue[15].action=PAC_IDLE;
			if (ret==1) return 1;
		}
	}
	return 0;
}

static int player_did_use(int nr,int cn,int last_action)
{
	int x,y,m;

	if (last_action!=AC_USE) return 0;
	if (player[nr]->act2==-1) return 0;

	dx2offset(ch[cn].dir,&x,&y,NULL);

	x+=ch[cn].x;
	y+=ch[cn].y;
	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return 1;		// ugh
	
	m=x+y*MAXMAP;
	if (map[m].it==player[nr]->act1) return 1;
	
	return 0;
}

void player_driver_dig_on(int cn)
{
	int nr;

	if ((nr=ch[cn].player) && player[nr]) player[nr]->act2=-1;
}

void player_driver_dig_off(int cn)
{
	int nr;

	if ((nr=ch[cn].player) && player[nr]) player[nr]->act2=0;
}

struct plr_attack_driver_data
{
	int co,serial,x,y;
};

int plr_attack_driver(int cn,int co,int isautoaction)
{
	struct plr_attack_driver_data *dat;
	int ret;
	
        if ((ch[cn].flags&CF_NOMOVE) && isautoaction) {
		return attack_driver_nomove(cn,co);
	}

	dat=set_data(cn,DRD_PLRATTACK,sizeof(struct plr_attack_driver_data));
	if (!dat) { return 0; }	// oops...

        ret=attack_driver(cn,co);
        if (ret) {
		dat->co=co;
		dat->serial=ch[co].serial;
		dat->x=ch[co].x;
		dat->y=ch[co].y;
	} else {
		if (error==ERR_NOT_VISIBLE) {
			if (co==dat->co && (ch[co].flags) && ch[co].serial==dat->serial) {
				if (move_driver(cn,dat->x,dat->y,0)) return 1;				
			}
		}
	}
	return ret;
}

#define TF_TIMEOUT	(60*60)
struct tutorial_ppd
{
	int state;
	int timer;

	int torch_last,torch_cnt;
	int grave_last,grave_cnt;
	int battle_last,battle_cnt;
	int battle2_last,battle2_cnt;
	int shop_last,shop_cnt;
	int chest_last,chest_cnt;
	int citem_last,citem_cnt,citem_start;
	int give_last,give_cnt;
	int raise_last,raise_cnt;

	int shift_last,shift_cnt;
	int ctrl_last,ctrl_cnt;
	int left_last,left_cnt;
	int raise2_last,raise2_cnt;

	int welcome_cnt,welcome_last;
	int lydia_cnt,lydia_last;
	int thief_cnt,thief_last;

	int potion_cnt,potion_last;
	int chat_cnt,chat_last;
	int chat2_cnt,chat2_last;

	int ownership_cnt,trader_cnt;
	int enchant_cnt,stones_cnt;
	int metal_cnt,metal2_cnt;
	int sharing_cnt,hacker_cnt;

	int ownership_last,trader_last;
	int enchant_last,stones_last;
	int metal_last,metal2_last;
	int sharing_last,hacker_last;
	
	int questlog_cnt,questlog_last;
};

void tutorial(int cn,int nr,struct tutorial_ppd *ppd)
{
	struct area1_ppd *ppd2;
	int in,n,tmp,f;
	char *sk=NULL;

	// newbie greeting
	if (realtime-ch[cn].login_time<20 && ppd->welcome_cnt<3 && realtime-ppd->welcome_last>TF_TIMEOUT) {
		log_char(cn,LOG_SYSTEM,0,"#Welcome to Astonia 3, %s. This is the help window. To remove it, press ESCAPE.$$You can access the client help facility by pressing F11.$$Should you ever require human help, type '/info i need help with ...' and press RETURN.",ch[cn].name);
		ppd->welcome_cnt++;
		ppd->welcome_last=realtime;
		ppd->timer=realtime;
		return;
	}

	// how to find first quest giver
	if (ch[cn].level<3 && (ppd2=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd))) && !ppd2->lydia_state && ppd->lydia_cnt<10 && realtime-ppd->lydia_last>60) {
		char *dirtext1,*dirtext2;
		int dir;
		static char *dir1[9]={"unknown","south-east","south","south-west","west","north-west","north","north-east","east"};
		static char *dir2[9]={"unknown","down and right","down","down and left","left","up and left","up","up and right","right"};

		dir=offset2dx(ch[cn].x,ch[cn].y,100,129);
		dirtext1=dir1[dir];
		dirtext2=dir2[dir];

		log_char(cn,LOG_SYSTEM,0,"#Some time ago, James asked you to help Lydia. You can find her %s (%s) of you.$To walk, left-click on the ground where you wish to go.$$Should you need human help, type '/info  i need help with ...' and press RETURN. To remove this window, press ESCAPE.",dirtext1,dirtext2);
		ppd->lydia_cnt++;
		ppd->lydia_last=realtime;
		ppd->timer=realtime;
		return;

	}

	// how to find quest
	if (ch[cn].level<4 && (ppd2=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd))) && ppd2->lydia_state==4 && realtime-ppd2->lydia_seen_timer>60 && get_section(ch[cn].x,ch[cn].y)!=45 && !has_item(cn,IID_AREA1_WOODPOTION) && ppd->thief_cnt<10 && realtime-ppd->thief_last>60) {
		char *dirtext1,*dirtext2;
		int dir;
		static char *dir1[9]={"unknown","south-east","south","south-west","west","north-west","north","north-east","east"};
		static char *dir2[9]={"unknown","down and right","down","down and left","left","up and left","up","up and right","right"};

		dir=offset2dx(ch[cn].x,ch[cn].y,91,156);
		dirtext1=dir1[dir];
		dirtext2=dir2[dir];

		log_char(cn,LOG_SYSTEM,0,"#Lydia asked you to find the thieves who stole her potion. She hinted that you can find them %s (%s) of you.$To walk, left-click on the ground where you wish to go.$$Should you need human help, type '/info i need help with ...' and press RETURN. To remove this window, press ESCAPE.",dirtext1,dirtext2);
		ppd->thief_cnt++;
		ppd->thief_last=realtime;
		ppd->timer=realtime;
		return;

	}

	// its dark
	if (map[ch[cn].x+ch[cn].y*MAXMAP].light<8 && (dlight*map[ch[cn].x+ch[cn].y*MAXMAP].dlight)/256<8 && ppd->torch_cnt<5) {
		// player has torch equipped?
                if ((in=ch[cn].item[WN_LHAND]) && it[in].driver==IDR_TORCH) {
			if (realtime-ppd->torch_last>TF_TIMEOUT) {
				log_char(cn,LOG_SYSTEM,0,"#It's pretty dark, isn't it? Why don't you light the torch you're holding by left-clicking on it?$You can use any item by left-clicking on it.");
				player_special(cn,0,5,0);
				ppd->torch_cnt++;
				ppd->torch_last=realtime;
				ppd->timer=realtime;
				return;
			}			
		}  else {
			// player has torch in inventory?
			for (n=30; n<INVENTORYSIZE; n++) {
				if ((in=ch[cn].item[n]) && it[in].driver==IDR_TORCH) break;						
			}
			if (n<INVENTORYSIZE) {
				if (realtime-ppd->torch_last>TF_TIMEOUT) {
					log_char(cn,LOG_SYSTEM,0,"#It's pretty dark, isn't it? Why don't you equip that torch you have in your inventory and light it?$To equip the torch, hold down SHIFT, left-click on it, then left-click on the torch slot.$To light the torch, left-click on it without holding SHIFT.$If you have trouble finding a torch in your inventory, right-click on the items there to read their descriptions.");
					player_special(cn,0,17,0);
					ppd->torch_cnt++;
					ppd->torch_last=realtime;
					ppd->timer=realtime;
					return;
				}
			} else {
				// player has no torch
				if (!ch[cn].item[WN_LHAND] && (!(in=ch[cn].item[WN_RHAND]) || !(it[in].flags&IF_WNTWOHANDED))) {
					in=create_item("torch");
					ch[cn].item[WN_LHAND]=in;
					it[in].carried=cn;
					ch[cn].flags|=CF_ITEMS;
		
					log_char(cn,LOG_SYSTEM,0,"#There you are, standing in the darkness, and no torch around. Well, I've just created one for you. It's there. Left-click on it to light it. But don't expect me to do this all the time.");
					player_special(cn,0,5,0);
					ppd->torch_cnt++;
					ppd->torch_last=realtime;
					ppd->timer=realtime;
					return;
				} else if (!ch[cn].citem) {
					in=create_item("torch");
					dlog(cn,in,"took torch from tutorial");
					ch[cn].citem=in;
					it[in].carried=cn;
					ch[cn].flags|=CF_ITEMS;
		
					log_char(cn,LOG_SYSTEM,0,"#There you are, standing in the darkness, and no torch around. Well, I've just created one for you. It there, on your mouse cursor. Hold down SHIFT and left-click on the torch slot. Then left-click on it without holding SHIFT to light it. But don't expect me to do this all the time.");
					player_special(cn,0,5,0);
					ppd->torch_cnt++;
					ppd->torch_last=realtime;
					ppd->timer=realtime;
					return;
				}
			}
		}
	}

	// hints for how to fight, triggered by leaving the village for the first time
	tmp=get_section(ch[cn].x,ch[cn].y);
	if (ch[cn].flags&CF_WARRIOR) {
		if (areaID==1 && (tmp<55 || tmp>57) && ppd->battle_cnt<3 && realtime-ppd->battle_last>TF_TIMEOUT) {
			if (ch[cn].flags&CF_WARRIOR) log_char(cn,LOG_SYSTEM,0,"#You've left the village, and things might get dangerous. If you get into a fight, it might be wise to use the skill 'Warcry'. Hold down ALT and press 8 to do that.");
			ppd->battle_cnt++;
			ppd->battle_last=realtime;
			ppd->timer=realtime;
			return;
		}
	} else {
		if (areaID==1 && (tmp<55 || tmp>57) && ppd->battle_cnt<3 && realtime-ppd->battle_last>TF_TIMEOUT && (may_add_spell(cn,IDR_BLESS) || ch[cn].lifeshield<POWERSCALE*5)) {
			log_char(cn,LOG_SYSTEM,0,"#You've left the village, and things might get dangerous. You'd better prepare yourself by casting the spells 'Bless' and 'Magic Shield'. Hold down ALT and press first 6 and then 5.");
			ppd->battle_cnt++;
			ppd->battle_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (areaID==1 && (tmp<55 || tmp>57) && ppd->battle2_cnt<3 && ticker-ppd->battle2_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#When you get into a fight, remember that mages rely on spells. A good spell in close ranged combat is 'Lightning Flash' - use ALT-3 to cast it.");
			ppd->battle2_cnt++;
			ppd->battle2_last=realtime;
			ppd->timer=realtime;
			return;
		}
	}

	// questlog, triggered by completing last skelly quest
	if (questlog_count(cn,4)>0 && ppd->questlog_cnt<3 && realtime-ppd->questlog_last>TF_TIMEOUT) {
		log_char(cn,LOG_SYSTEM,0,"#If you ever feel as if you are not strong enough to complete a quest, open your quest log in the top right corner of the screen and re-do some of your previous quests.");
		ppd->questlog_cnt++;
		ppd->questlog_last=realtime;
		ppd->timer=realtime;
		return;
	}



	// shopping tips, triggered by merchant window
	if (ch[cn].merchant && ppd->shop_cnt<3 && realtime-ppd->shop_last>TF_TIMEOUT) {
		log_char(cn,LOG_SYSTEM,0,"#You've opened a shop window. The items you can buy are shown in the bottom left window. To buy anything, left-click on it. To find out what these items are, right-click on them.$Note that you'll sell any of your items if you left-click on them now.");
		ppd->shop_cnt++;
		ppd->shop_last=realtime;
		ppd->timer=realtime;
		return;
	}

	// how to use chests, triggered by thief and skelly chest
	if (areaID==1 &&
	    ((ch[cn].x>=74 && ch[cn].x<=78 && ch[cn].y>=148 && ch[cn].y<=152) ||
	     (ch[cn].x>=196 && ch[cn].x<=201 && ch[cn].y>=160 && ch[cn].y<=166)) &&
	    ppd->chest_cnt<3 && realtime-ppd->chest_last>TF_TIMEOUT) {
		log_char(cn,LOG_SYSTEM,0,"#Do you see that chest? To search it, hold down SHIFT and left-click on it. If you do not have the right key, go through the building again and be sure to search all bodies.");
		ppd->chest_cnt++;
		ppd->chest_last=realtime;
		ppd->timer=realtime;
		return;
	}

	// player has citem for a long time?
	if (ch[cn].citem) {
		if (!ppd->citem_start) ppd->citem_start=realtime;
                if (realtime-ppd->citem_start>30 && ppd->citem_cnt<3 && realtime-ppd->citem_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#You've been carrying that item on your mouse cursor for quite a while now. Hold down SHIFT and click on the ground to drop it, or hold down SHIFT and click in your inventory to keep it.");
			ppd->citem_cnt++;
			ppd->citem_last=realtime;
			ppd->timer=realtime;
			return;
		}
	} else ppd->citem_start=0;

	// player got enough experience to raise a skill
	if (ppd->raise_cnt<3 && realtime-ppd->raise_last>TF_TIMEOUT) {
		
		if ((ch[cn].flags&CF_WARRIOR) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_SWORD,ch[cn].value[1][V_SWORD]) &&
		    (in=ch[cn].item[WN_RHAND]) && (it[in].flags&IF_SWORD) &&
		    ch[cn].value[1][V_SWORD]<=ch[cn].value[1][V_ATTACK] &&
		    ch[cn].value[1][V_SWORD]<=ch[cn].value[1][V_PARRY]) sk="Sword";

		if (!sk && ((ch[cn].flags&CF_WARRIOR) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_TWOHAND,ch[cn].value[1][V_TWOHAND])) &&
		    (in=ch[cn].item[WN_RHAND]) && (it[in].flags&IF_TWOHAND) &&
		    ch[cn].value[1][V_TWOHAND]<=ch[cn].value[1][V_ATTACK] &&
		    ch[cn].value[1][V_TWOHAND]<=ch[cn].value[1][V_PARRY]) sk="Two-Handed";

		if (!sk && ((ch[cn].flags&CF_WARRIOR) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_ATTACK,ch[cn].value[1][V_ATTACK])) &&
                    ch[cn].value[1][V_ATTACK]<=ch[cn].value[1][V_SWORD] &&
		    ch[cn].value[1][V_ATTACK]<=ch[cn].value[1][V_PARRY]) sk="Attack";

		if (!sk && ((ch[cn].flags&CF_WARRIOR) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_PARRY,ch[cn].value[1][V_PARRY])) &&
                    ch[cn].value[1][V_PARRY]<=ch[cn].value[1][V_ATTACK] &&
		    ch[cn].value[1][V_PARRY]<=ch[cn].value[1][V_SWORD]) sk="Parry";

		if (!sk && ((ch[cn].flags&CF_MAGE) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_BLESS,ch[cn].value[1][V_BLESS])) &&
                    ch[cn].value[1][V_BLESS]==1) sk="Bless";

		if (!sk && ((ch[cn].flags&CF_MAGE) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_FLASH,ch[cn].value[1][V_FLASH])) &&
                    ch[cn].value[1][V_FLASH]<=ch[cn].value[1][V_MAGICSHIELD]) sk="Lightning Flash";

		if (!sk && ((ch[cn].flags&CF_MAGE) && ch[cn].exp-ch[cn].exp_used>=raise_cost(V_MAGICSHIELD,ch[cn].value[1][V_MAGICSHIELD])) &&
                    ch[cn].value[1][V_MAGICSHIELD]<=ch[cn].value[1][V_FLASH]) sk="Magic Shield";
	
		if (sk) {	
			log_char(cn,LOG_SYSTEM,0,"#You've accumulated enough experience to raise a skill. You'll find some blue orbs in the bottom left window. Left-click on the one next to '%s' to raise that skill.",sk);
			ppd->raise_cnt++;
			ppd->raise_last=realtime;
			ppd->timer=realtime;
			return;
		}
	}

	// generic hints if nothing was displayed for 3 minutes
	if (realtime-ppd->timer>180) {
		if (ppd->potion_cnt<3 && realtime-ppd->potion_last>TF_TIMEOUT) {
			int f_dead[4]={0,0,0,0};

			for (n=30; n<INVENTORYSIZE; n++) {
				if (!f_dead[(n-30)%4] && (in=ch[cn].item[n]) && it[in].driver==IDR_POTION && it[in].drdata[1]) break;
				if ((in=ch[cn].item[n]) && (it[in].flags&IF_USE)) f_dead[(n-30)%4]=1;

			}
			if (n<INVENTORYSIZE) {
				f=(n-30)%4+1;
				log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$You should always watch your Hitpoints by looking at the small red line below your character's name. If they get too low, use a healing potion, either by left-clicking on it, or by pressing F%d.$Note that the F-key is assigned to first usable item in that column in your inventory, not to the item itself.",f);
				ppd->potion_cnt++;
				ppd->potion_last=realtime;
				ppd->timer=realtime;
				return;
			}
		}
		if (ppd->shift_cnt<3 && realtime-ppd->shift_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$As a general rule, anything that deals with items requires you to hold down SHIFT. But there is one exception: To use an item you have in your inventory or equipment field, you left-click on it without holding SHIFT.");
			ppd->shift_cnt++;
			ppd->shift_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->ctrl_cnt<3 && realtime-ppd->ctrl_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Anything that deals with characters requires you to hold down CTRL. To look at another character, hold down CTRL and right-click on that character. To attack him instead, hold down CTRL and left-click.");
			ppd->ctrl_cnt++;
			ppd->ctrl_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->left_cnt<3 && realtime-ppd->left_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Clicking the left mouse button always initiates an action, while right-clicking merely looks at the item or character.");
			ppd->left_cnt++;
			ppd->left_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->chat_cnt<3 && realtime-ppd->chat_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$To talk with those you see on your screen, just type what you want to say and press RETURN. Some things you hear contain words in blue letters. You can click on those to use these texts as an answer.");
			ppd->chat_cnt++;
			ppd->chat_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->chat2_cnt<3 && realtime-ppd->chat2_last>TF_TIMEOUT) {
			log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$If you wish to talk to all players in the game, you have to use the chat system. It is divided into different channels, numbered 1 to 30. Type /channels to get a list of these channels. Use /join 2, to join, for example, chat channel 2. To talk in that channel, use /c2 hello.");
			ppd->chat2_cnt++;
			ppd->chat2_last=realtime;
			ppd->timer=realtime;
			return;
		}
		
		
		if (ppd->raise2_cnt<3 && realtime-ppd->raise2_last>TF_TIMEOUT) {
			if (ch[cn].flags&CF_WARRIOR) log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$A good strategy to raise a warrior is to keep your main weapon skill (Sword or Two-Handed), Attack and Parry close together. Also, do not neglect Tactics and Immunity.");
			else log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$A good strategy to raise a mage is to concentrate on Lightning Flash, Magic Shield, Dagger (or Staff) and Immunity at first.");
			ppd->raise2_cnt++;
			ppd->raise2_last=realtime;
			ppd->timer=realtime;
			return;
		}

                if (ppd->ownership_cnt<3 && realtime-ppd->ownership_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Your account belongs to you alone. You cannot sell it, trade it, or give it away.$$Neither can another player give you his account. Any account you might receive is stolen property.");
			ppd->ownership_cnt++;
			ppd->ownership_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->trader_cnt<3 && realtime-ppd->trader_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Conduct all trade via the Trader. He's an automated middleman who'll make sure you don't get cheated. You can find a trader in one of the small houses on the eastern side of Cameron.");
			ppd->trader_cnt++;
			ppd->trader_last=realtime;
			ppd->timer=realtime;
			return;
		}

                if (ppd->enchant_cnt<3 && realtime-ppd->enchant_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$You can increase the strength of magic items (those with, for example, +1 Immunity) by using them on an Orb of Enhancement. Jeremy in Aston sells them.");
			ppd->enchant_cnt++;
			ppd->enchant_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->stones_cnt<3 && realtime-ppd->stones_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$If your Orb of Enhancement is not strong enough, find some magic stones (Earth Stone, Fire Stone, Ice Stone) in the Pentagram quest to enhance the Orb's strength.");
			ppd->stones_cnt++;
			ppd->stones_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->metal_cnt<3 && realtime-ppd->metal_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Take good care of your gear, and it will take good care of you. All magic items lose part of their strength through use. You'll need raw metal (Silver, Gold) to repair them. Just use the item on the metal. You can dig for metal in the mines north of Aston.");
			ppd->metal_cnt++;
			ppd->metal_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->metal2_cnt<3 && realtime-ppd->metal2_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Normal item repairs can be done with silver. If you want to make them even better, use gold. It will increase their strength beyond the original values.");
			ppd->metal2_cnt++;
			ppd->metal2_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->sharing_cnt<3 && realtime-ppd->sharing_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Your account is yours, and yours alone. You are not allowed to share it with anyone (children playing on their parent's account being the only exception). Sharing might get your account banned permanently.");
			ppd->sharing_cnt++;
			ppd->sharing_last=realtime;
			ppd->timer=realtime;
			return;
		}
		if (ppd->hacker_cnt<3 && realtime-ppd->hacker_last>TF_TIMEOUT) {
                        log_char(cn,LOG_SYSTEM,0,"#Todays Hint:$$Do not accept any files from other players. There are a few malicious persons out there who will send you keyloggers to steal your account.$Also, be wary of any emails you receive. We will never ask you for your password, or give you a link to click on. If in doubt, email game@astonia.com directly and ask before you react to any mails you receive.");
			ppd->hacker_cnt++;
			ppd->hacker_last=realtime;
			ppd->timer=realtime;
			return;
		}
	}
}

static int ignorechar_check_target(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
	if (map[m].flags&MF_DOOR) return 1;
	//if (map[m].ch && (ch[map[m].ch].flags&(CF_PLAYER|CF_PLAYERLIKE)) && ch[map[m].ch].action==AC_IDLE) return 1;
        if ((map[m].flags&MF_TMOVEBLOCK) && map[m].it) return 0;

	return 1;
}


int move_driver_ignorechar(int cn,int tx,int ty,int mindist)
{
	int dir;

        dir=pathfinder(ch[cn].x,ch[cn].y,tx,ty,mindist,ignorechar_check_target,0);
	
	if (dir==-1) return 0;

        return walk_or_use_driver(cn,dir);
}


static int player_move_driver(int cn,int x,int y)
{
	if (ch[cn].x==x && ch[cn].y==y) return 0;		// we're there. done.
	if (move_driver(cn,x,y,0)) return 1;			// we can go to the exact tile, go there.
	if (abs(ch[cn].x-x)+abs(ch[cn].y-y)<2) return 0;	// exact tile is blocked, but we're just one step away. done.
	//return move_driver(cn,x,y,1);				// try to go close to target
	if (move_driver(cn,x,y,1)) return 1;			// try to go close to target

	return move_driver_ignorechar(cn,x,y,1);
}

static int player_use_driver(int cn,int in)
{
	if (!(it[in].flags&IF_USE)) { error=ERR_NOT_USEABLE; return 0; }
	if (use_driver(cn,in,0)) return 1;

	if (it[in].flags&IF_FRONTWALL) {
                if (!(map[it[in].x+it[in].y*MAXMAP+1].flags&(MF_MOVEBLOCK)) && move_driver(cn,it[in].x+1,it[in].y,1)) return 1;
		if (!(map[it[in].x+it[in].y*MAXMAP+MAXMAP].flags&(MF_MOVEBLOCK)) && move_driver(cn,it[in].x,it[in].y+1,1)) return 1;
	} else if (move_driver(cn,it[in].x,it[in].y,2)) return 1;

	return 0;
}

int player_driver_optimize_surround(int cn,int co)
{
	int back,left,right,front;
	int x,y,diag;

	dx2offset(ch[cn].dir,&x,&y,&diag);
	if (diag) return 0;	// facing diagonally
	
	front=map[(ch[cn].x+x)+(ch[cn].y+y)*MAXMAP].ch;
	back= map[(ch[cn].x-x)+(ch[cn].y-y)*MAXMAP].ch;
	if (!back) return 0;	// no one in back

	left =map[(ch[cn].x+y)+(ch[cn].y+x)*MAXMAP].ch;
	right=map[(ch[cn].x-y)+(ch[cn].y-x)*MAXMAP].ch;

        if (front && !can_attack(cn,front)) front=0;
	if (back && !can_attack(cn,back)) back=0;
	if (left && !can_attack(cn,left)) left=0;
	if (right && !can_attack(cn,right)) right=0;
	if (!back) return 0;	// no one in back

	// choose weakest when surrounded
	if (front && back && left && right) {		// completely surrounded
		if (ch[co].hp<=ch[front].hp &&
		    ch[co].hp<=ch[back].hp &&
		    ch[co].hp<=ch[left].hp &&
		    ch[co].hp<=ch[right].hp) return 1;		// choose weakest one		
		return 0;
	}

	// choose weaker one if one in front, one behind
	if (front && back && !left && !right) {		// one in front, one behind
		if (ch[co].hp<=ch[front].hp &&
		    ch[co].hp<=ch[back].hp) return 1;		// choose weakest one
		return 0;
	}

	// choose middle one if three enemies
        if (left!=co && right!=co) return 0;	// not the attacker on a side
	if (front && ch[front].hp<ch[co].value[0][V_HP]*POWERSCALE/4) // finish off current enemy if he's about to die
		return 0;

	return 1;
}

void player_driver(int cn,int ret,int last_action)
{
	int nr,co;
	struct msg *msg,*next;
	struct tutorial_ppd *ppd;
	struct lostcon_ppd *lppd;

        if (!(ch[cn].flags&CF_PLAYER)) { charlog(cn,"not player"); return; }

        nr=ch[cn].player;

	if (!player[nr]) { do_idle(cn,TICKS/4); return; }

	ppd=set_data(cn,DRD_TUTORIAL_PPD,sizeof(struct tutorial_ppd));
	if (!ppd) return;

	if (realtime-ch[cn].login_time<5) ppd->timer=ch[cn].login_time-10;

        if (realtime-ppd->timer>20) {
                tutorial(cn,nr,ppd);
	}

	area_sound(cn);

	lppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd));

	// loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		// did we get hit? - auto fightback
		if (msg->type==NT_GOTHIT) {
			
			co=msg->dat1;
			
			if (co && char_dist(cn,co)<3) {
				if (player[nr]->action==PAC_KILL && ticker-player[nr]->nofight_timer>TICKS*3) {
					if (lppd && lppd->autoturn && player_driver_optimize_surround(cn,co)) {
						player[nr]->action=PAC_KILL;
						player[nr]->act1=co;
						player[nr]->act2=ch[co].serial;
						player[nr]->isautoaction=1;
					}
                                } else if (player[nr]->action==PAC_IDLE && ticker-player[nr]->nofight_timer>TICKS*3) {
					player[nr]->action=PAC_KILL;
					player[nr]->act1=co;
					player[nr]->act2=ch[co].serial;
					player[nr]->isautoaction=1;
                                } else if ((player[nr]->action!=PAC_KILL || player[nr]->act1!=co || player[nr]->act2!=ch[co].serial) && ticker-player[nr]->nofight_timer>TICKS*3) {
					player[nr]->next_fightback_cn=co;
					player[nr]->next_fightback_serial=ch[co].serial;
					player[nr]->next_fightback_ticker=ticker;
				}
			}
		}

		if (msg->type==NT_NPC && msg->dat1==NTID_TUTORIAL) {
			//say(cn,"got dat2=%d, dat=%s, diff1=%d, cnt=%d, diff2=%d",msg->dat2,ch[msg->dat3].name,realtime-ppd->timer,ppd->give_cnt,realtime-ppd->give_last);
			if (msg->dat2==0 && msg->dat3==cn && realtime-ppd->timer>10 && ppd->give_cnt<3 && realtime-ppd->give_last>TF_TIMEOUT && has_item(cn,IID_AREA1_WOODPOTION)) {
				log_char(cn,LOG_SYSTEM,0,"#Now would be a good time to hand Lydia the potion you found. Hold down SHIFT and left-click on the potion. Then hold down CTRL (and release shift) and left-click on Lydia.");
				ppd->give_cnt++;
				ppd->give_last=realtime;
				ppd->timer=realtime;

			}
			if (msg->dat2==1 && msg->dat3==cn && realtime-ppd->timer>10 && ppd->give_cnt<3 && realtime-ppd->give_last>TF_TIMEOUT && has_item(cn,IID_AREA1_SKELSKULL)) {
				log_char(cn,LOG_SYSTEM,0,"#Now would be a good time to hand Gwendylon the skull you found. Hold down SHIFT and left-click on the skull. Then hold down CTRL (and release shift) and left-click on Gwendylon.");
				ppd->give_cnt++;
				ppd->give_last=realtime;
				ppd->timer=realtime;

			}
			//if (msg->dat2==2525 && msg->dat3==cn) { bzero(ppd,sizeof(*ppd)); ppd->state=4; } // !!!!!!!!!!!!
		}

		if (msg->type==NT_DEAD && msg->dat2==cn && realtime-ppd->timer>10 && ppd->grave_cnt<3 && realtime-ppd->grave_last>TF_TIMEOUT) {
			co=msg->dat1;
			log_char(cn,LOG_SYSTEM,0,"#Now that you've killed the %s, it would be wise to search %s body. Hold down SHIFT and left click on the body. You'll see all the things you find in the bottom left window.$To take any of the items, left-click on them.",ch[co].name,hisname(co));
                        ppd->grave_cnt++;
			ppd->grave_last=realtime;
			ppd->timer=realtime;
		}

                remove_message(cn,msg);
	}

	if (player[nr]->action==PAC_IDLE && ticker-player[nr]->next_fightback_ticker<TICKS && ticker-player[nr]->nofight_timer>TICKS*3) {
		player[nr]->action=PAC_KILL;
		player[nr]->act1=player[nr]->next_fightback_cn;
		player[nr]->act2=player[nr]->next_fightback_serial;
		player[nr]->isautoaction=1;
	}

        switch(player[nr]->action) {
		case PAC_USE:	if (player_did_use(nr,cn,last_action)) player[nr]->action=PAC_IDLE;
				if (last_action==AC_USE && ret==2) player[nr]->action=PAC_IDLE;
				break;
		case PAC_KILL:	if (ch[player[nr]->act1].serial!=player[nr]->act2) player[nr]->action=PAC_IDLE; break;
		case PAC_MOVE:	if (last_action==AC_USE && ret==2) player[nr]->action=PAC_IDLE; break;
		
	}

        if (ch[cn].value[0][V_BLESS] && lppd && lppd->autobless && may_add_spell(cn,IDR_BLESS) && do_bless(cn,cn)) return;

	if (run_queue(nr,cn)) return;

	switch(player[nr]->action) {
		case PAC_IDLE:		do_idle(cn,4); return;
		case PAC_MOVE:		if (!player_move_driver(cn,player[nr]->act1,player[nr]->act2)) { player[nr]->action=0; do_idle(cn,4); } return;
		case PAC_TAKE:		if (!take_driver(cn,player[nr]->act1)) { player[nr]->action=0; do_idle(cn,4); } return;
		case PAC_DROP:          if (!drop_driver(cn,player[nr]->act1,player[nr]->act2)) { player[nr]->action=0; do_idle(cn,4); } return;
		case PAC_KILL:		if (!plr_attack_driver(cn,player[nr]->act1,player[nr]->isautoaction)) { player[nr]->action=0; do_idle(cn,4); } return;
		case PAC_USE:		if (!player_use_driver(cn,player[nr]->act1)) { player[nr]->action=0; do_idle(cn,4); } return;
		
		case PAC_LOOK_MAP:	look_map(cn,player[nr]->act1,player[nr]->act2); player[nr]->action=0; do_idle(cn,4); return;
		case PAC_GIVE:		if (!give_driver(cn,player[nr]->act1)) { player[nr]->action=0; do_idle(cn,4); } return;
		
		case PAC_TELEPORT:	if (!do_use(cn,ch[cn].dir,player[nr]->act1+1)) do_idle(cn,4); player[nr]->action=0; return;

		default:		do_idle(cn,4); return;
	}
}

int player_driver_get_move(int cn,int *px,int *py)
{
	int nr;

	if (!(nr=ch[cn].player)) return 0;
	if (!player[nr]) return 0;
	if (player[nr]->action!=PAC_MOVE) return 0;

	if (px) *px=player[nr]->act1;
	if (py) *py=player[nr]->act2;

	return 1;
}

int player_driver_fake_move(int cn,int x,int y)
{
	int nr;

	if (!(nr=ch[cn].player)) return 0;
	if (!player[nr]) return 0;
	
	player[nr]->action=PAC_MOVE;
        player[nr]->act1=x;
	player[nr]->act2=y;

	return 1;
}















