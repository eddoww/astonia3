/*

$Id: shrike.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: shrike.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:45  sam
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
#include "area1.h"

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

static int is_fullnight() {
	return moonlight && dlight<100;
}

void tree_driver(int in,int cn)
{
	int in2,sprite;
	char *desc;

	if (!cn) {
		if (is_fullnight()) { sprite=51631; desc="A silver chain is hanging from one twig of this three."; }
		else { sprite=16006; desc="A dead tree."; }

		if (sprite!=it[in].sprite) {
			it[in].sprite=sprite;
			strcpy(it[in].description,desc);
			set_sector(it[in].x,it[in].y);
                }
		
		call_item(it[in].driver,in,0,ticker+TICKS*60);
		return;
	}

	if (!is_fullnight()) return;

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}
	
	in2=create_item("shrike_amulet2");
	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"BUG #SR335");
		return;
	}
	
	ch[cn].citem=in2;
	it[in2].carried=cn;
	ch[cn].flags|=CF_ITEMS;
}

void pede_driver(int in,int cn)
{
	int in2,sprite;
	char *desc;

	if (!cn) {
		if (is_fullnight()) { sprite=51636; desc="A crystal is floating above a pedestal of stone."; }
		else { sprite=51637; desc="A pedestal made of rock."; }

		if (sprite!=it[in].sprite) {
			it[in].sprite=sprite;
			strcpy(it[in].description,desc);
			set_sector(it[in].x,it[in].y);
                }
		
		call_item(it[in].driver,in,0,ticker+TICKS*60);
		return;
	}

	if (!is_fullnight()) return;

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}
	
	in2=create_item("shrike_amulet1");
	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"BUG #SR335");
		return;
	}
	
	ch[cn].citem=in2;
	it[in2].carried=cn;
	ch[cn].flags|=CF_ITEMS;
}

void rock_driver(int in,int cn)
{
	int in2,sprite;
	char *desc;

	if (!cn) {
		if (is_fullnight()) { sprite=51632; desc="A piece of silver seems to be stuck below this rock."; }
		else { sprite=59763; desc="A big rock."; }

		if (sprite!=it[in].sprite) {
			it[in].sprite=sprite;
			strcpy(it[in].description,desc);
			set_sector(it[in].x,it[in].y);
                }
		
		call_item(it[in].driver,in,0,ticker+TICKS*60);
		return;
	}

	if (!is_fullnight()) return;

	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot take the piece of silver. The stone is too heavy to move.");
		return;
	}

	if (it[in2].driver!=IDR_FORESTSPADE) {
		log_char(cn,LOG_SYSTEM,0,"You cannot get enough leverage with this to move the stone. Maybe a long stick with something thin on its end would do the trick.");
		return;
	}

	destroy_item(in2);
	log_char(cn,LOG_SYSTEM,0,"You use the spade as a lever to move the stone. You take the piece of silver. Just as you try to remove the spade it snaps.");
	
	in2=create_item("shrike_amulet3");
	if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"BUG #SR337");
		return;
	}
	
	ch[cn].citem=in2;
	it[in2].carried=cn;
	ch[cn].flags|=CF_ITEMS;
}

void door_driver(int in,int cn)
{
	int sprite,in2;

	if (!cn) {
		if (is_fullnight()) sprite=51625;
		else sprite=20122;
		
		if (it[in].sprite!=sprite) {
			it[in].sprite=sprite;
			set_sector(it[in].x,it[in].y);
		}
		
		call_item(it[in].driver,in,cn,ticker+TICKS*60);
	} else {
		if (ch[cn].level<65) {
			log_char(cn,LOG_SYSTEM,0,"You feel a tingling in your fingers and decide not to touch the door again until you have grown stronger (requires level 65).");
			return;
		}
		if (!(in2=ch[cn].citem) || !strstr(it[in2].description," of the Moon.")) {
			log_char(cn,LOG_SYSTEM,0,"Your fingers tingle as you run them over the metal of the door and a whispered voice can be heard \"The Moon Blade\" is the key.");
			return;
		}
		teleport_char_driver(cn,8,92);
	}
}

void pool_driver(int in,int cn)
{
	int in2;
	if (!cn) return;
	
	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"The water is sweet and refreshing.");
		return;
	}

	if (it[in2].driver!=IDR_SHRIKEAMULET || it[in2].drdata[0]!=7 || !is_fullnight()) {
		log_char(cn,LOG_SYSTEM,0,"Your %s is wet now.",lower_case(it[in2].name));
		return;
	}

	it[in2].ID=IID_SHRIKE_TALISMAN;
	sprintf(it[in2].description,"The Talisman of the Moon.");
	log_char(cn,LOG_SYSTEM,0,"The talisman seems to glow faintly.");
}

void cube_driver(int in,int cn)
{
	int m,m2,dx,dy,dir,nr;

	if (cn) {	// player using item
		m=it[in].x+it[in].y*MAXMAP;
		dir=ch[cn].dir;
		dx2offset(dir,&dx,&dy,NULL);
		m2=(it[in].x+dx)+(it[in].y+dy)*MAXMAP;

		if ((map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m2].it || map[m2].gsprite<59753 || map[m2].gsprite>59761) {
			log_char(cn,LOG_SYSTEM,0,"It won't move.");
			return;
		}
		map[m].flags&=~MF_TMOVEBLOCK;
		map[m].it=0;
		set_sector(it[in].x,it[in].y);

		map[m2].flags|=MF_TMOVEBLOCK;
		map[m2].it=in;
		it[in].x+=dx; it[in].y+=dy;
		set_sector(it[in].x,it[in].y);

		// hack to avoid using the chest over and over
		if ((nr=ch[cn].player)) {
			player_driver_halt(nr);
		}

		*(unsigned int*)(it[in].drdata+4)=ticker;

		return;
	} else {	// timer call
		if (!(*(unsigned int*)(it[in].drdata+8))) {	// no coords set, so its first call. remember coords
			*(unsigned short*)(it[in].drdata+8)=it[in].x;
			*(unsigned short*)(it[in].drdata+10)=it[in].y;
		}

		// if 15 minutes have passed without anyone touching the chest, beam it back.
		if (ticker-*(unsigned int*)(it[in].drdata+4)>TICKS*60*15 &&
		    (*(unsigned short*)(it[in].drdata+8)!=it[in].x ||
		     *(unsigned short*)(it[in].drdata+10)!=it[in].y)) {
			
			m=it[in].x+it[in].y*MAXMAP;
			m2=(*(unsigned short*)(it[in].drdata+8))+(*(unsigned short*)(it[in].drdata+10))*MAXMAP;

			if (!(map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) && !map[m2].it) {
				map[m].flags&=~MF_TMOVEBLOCK;
				map[m].it=0;
				set_sector(it[in].x,it[in].y);
	
				map[m2].flags|=MF_TMOVEBLOCK;
				map[m2].it=in;
				it[in].x=*(unsigned short*)(it[in].drdata+8);
				it[in].y=*(unsigned short*)(it[in].drdata+10);
				set_sector(it[in].x,it[in].y);
			}
                }
		call_item(it[in].driver,in,0,ticker+TICKS*5);
	}
}

void shr_werewolf_dead(int cn,int co)
{
	struct area1_ppd *ppd;

	create_mist(ch[cn].x,ch[cn].y);
	ch[cn].sprite=6;

	if ((ch[co].flags&CF_PLAYER) && (ppd=set_data(co,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) {
		ppd->shrike_fails++;
		say(cn,"I have deserved death. But still... I was hoping for something better.");
	}
}

void shrike_driver(int in,int cn)
{
        switch(it[in].drdata[0]) {
		case 1:		tree_driver(in,cn); break;
		case 2:		rock_driver(in,cn); break;
		case 3:		door_driver(in,cn); break;
		case 4:		pool_driver(in,cn); break;
		case 5:		cube_driver(in,cn); break;
		case 6:		pede_driver(in,cn); break;
	}
}

void shr_werewolf_driver(int cn,int ret,int lastact)
{
        if (is_fullnight()) {
		ch[cn].flags&=~CF_INVISIBLE;
		char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
		return;
	}

	ch[cn].flags|=CF_INVISIBLE;
	if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;
        do_idle(cn,TICKS);
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_SHR_WEREWOLF:	shr_werewolf_driver(cn,ret,lastact); return 1;

                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_SHRIKE:	shrike_driver(in,cn); return 1;

		default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_SHR_WEREWOLF: 	shr_werewolf_dead(cn,co); return 1;

                default:		return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_SHR_WEREWOLF:	return 1;

		default:		return 0;
	}
}
