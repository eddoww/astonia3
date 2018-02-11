/*
 * $Id: death.c,v 1.18 2007/12/03 10:07:43 devel Exp $ (c) D.Brockhaus
 *
 * $Log: death.c,v $
 * Revision 1.18  2007/12/03 10:07:43  devel
 * death extinguishes flames
 *
 * Revision 1.17  2007/07/28 18:51:43  devel
 * feuding chars now cannot use saves when attacking by a player
 *
 * Revision 1.16  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.15  2007/07/01 13:26:46  devel
 * removed /allow and helpers
 *
 * Revision 1.14  2007/06/08 12:08:16  devel
 * added first kill message for random dungeon skellies
 *
 * Revision 1.13  2007/06/07 15:28:54  devel
 * removed PK
 *
 * Revision 1.12  2007/05/25 12:25:02  devel
 * back to rev 1.10 since 1.11 didn't work out. how do you to this right?
 *
 * Revision 1.10  2007/05/04 14:15:24  devel
 * lag logic will no longer use potions and recalls in arenas
 *
 * Revision 1.9  2007/02/28 09:51:44  devel
 * made respawn use the surrounding tiles too
 *
 * Revision 1.8  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.7  2006/10/06 17:26:47  devel
 * removed some charlogs
 *
 * Revision 1.6  2006/09/21 11:23:48  devel
 * added hook for new LQ area 35
 *
 * Revision 1.5  2006/04/26 16:05:44  ssim
 * made teufelheim hooks work only on arena tiles
 *
 * Revision 1.4  2006/04/24 17:22:42  ssim
 * added hooks and basics for teufelheim PK
 *
 * Revision 1.3  2005/11/27 18:17:04  ssim
 * added no-body zone in islena's room
 *
 * Revision 1.2  2005/11/06 14:09:57  ssim
 * added check for lifeshield<0 in respawn_callback()
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "server.h"
#include "log.h"
#include "timer.h"
#include "map.h"
#include "notify.h"
#include "create.h"
#include "drdata.h"
#include "libload.h"
#include "direction.h"
#include "error.h"
#include "act.h"
#include "talk.h"
#include "expire.h"
#include "effect.h"
#include "database.h"
#include "tool.h"
#include "container.h"
#include "player.h"
#include "sector.h"
#include "area.h"
#include "death.h"
#include "lookup.h"
#include "chat.h"
#include "drvlib.h"
#include "military.h"
#include "item_id.h"
#include "respawn.h"
#include "poison.h"
#include "misc_ppd.h"
#include "date.h"
#include "consistency.h"
#include "lostcon.h"
#include "clan.h"

// respawn character tmp at tmpx,tmpy
// if that place is occupied, try again in one second
static void respawn_callback(int tmp,int tmpx,int tmpy,int tmpa,int dum)
{
	int cn;

        cn=create_char_nr(tmp,tmpa);
	
	ch[cn].tmpx=tmpx;
	ch[cn].tmpy=tmpy;
	
        if (char_driver(ch[cn].driver,CDT_RESPAWN,cn,0,0)==1 && drop_char(cn,tmpx,tmpy,1)) {
		
                update_char(cn);
		
		ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
		ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
		ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

		if (ch[cn].lifeshield<0) {
			elog("respawn_callback(): lifeshield=%d (%X) for char %d (%s). fixed.",ch[cn].lifeshield,ch[cn].lifeshield,cn,ch[cn].name);
			ch[cn].lifeshield=0;
		}
		
		ch[cn].dir=DX_RIGHTDOWN;

		register_respawn_respawn(cn);

		//charlog(cn,"created by respawn");
		
		return;
	}

	destroy_char(cn);

	set_timer(ticker+TICKS*10,respawn_callback,tmp,tmpx,tmpy,tmpa,0);
}

void log_player_death(int cn,char *reason,int itemflag)
{
	struct lostcon_ppd *ppd;
	int n,in;

        if (!(ch[cn].flags&CF_PLAYER)) return;

        ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd));
	if (!ppd) return;	// oops...

	dlog(cn,0,"Death: Lag Control=%s, Settings: Maxlag=%d, Health Pot=%s, Mana Pot=%s, Combo Pot=%s, Recall=%s",
	     ch[cn].driver==CDR_LOSTCON ? "Yes" : "No",
             ppd->maxlag,
             ppd->nolife ? "off" : "on",
	     ppd->nomana ? "off" : "on",
	     ppd->nocombo ? "off" : "on",
	     ppd->norecall ? "off" : "on");

	if (itemflag) {
		for (n=0; n<INVENTORYSIZE; n++) {
			if (n>12 && n<30) continue;
			if (!(in=ch[cn].item[n])) continue;
			if (n<30) dlog(cn,in,"kept worn item");
			else dlog(cn,in,"item in grave (inventory)");
		}
		if ((in=ch[cn].citem)) dlog(cn,in,"item in grave (citem)");		
	}
}

struct firstkill_ppd	// persistent player data
{
	unsigned int kill[32];	// bitfield, containing ones for all kills already done
};

static void give_first_kill(int cn,int co)
{
	struct firstkill_ppd *ppd;
	int index,offset,mask;

	if (!(ch[cn].flags&CF_PLAYER)) return;
	
        if (ch[co].class<1 || ch[co].class>1023) return;	// victim has no class set, so no first kill...

	if (!(ppd=set_data(cn,DRD_FIRSTKILL_PPD,sizeof(struct firstkill_ppd)))) return;	// OOPS

	index=ch[co].class/32;
	offset=ch[co].class&31;
	mask=1<<offset;

        if (ppd->kill[index]&mask) return;
	
	ppd->kill[index]|=mask;

	give_exp_bonus(cn,kill_score(co,cn)*5);

	if (ch[co].flags&CF_HASNAME) {
		log_char(cn,LOG_SYSTEM,0,"You just killed %s for the first time. Congratulations!",ch[co].name);
	} else if ((ch[co].class>=52 && ch[co].class<=84) ||	// pents
		   (ch[co].class>=85 && ch[co].class<=100) ||	// sewers
		   (ch[co].class>=101 && ch[co].class<=106) ||	// edemon
		   (ch[co].class>=107 && ch[co].class<=138) ||	// fpents
		   (ch[co].class>=139 && ch[co].class<=170) ||	// ipents
		   (ch[co].class>=172 && ch[co].class<=181) ||	// fdemon
		   (ch[co].class>=183 && ch[co].class<=202) ||	// idemon
		   (ch[co].class>=204 && ch[co].class<=214) ||	// palace demon
		   (ch[co].class>=215 && ch[co].class<=220) ||	// palace key bearer
		   (ch[co].class>=221 && ch[co].class<=228) ||	// mine, silver golem
		   (ch[co].class>=229 && ch[co].class<=236) ||	// mine, gold golem
		   (ch[co].class>=237 && ch[co].class<=244) ||  // mine, zombie
		   (ch[co].class>=336 && ch[co].class<=365) ||  // bones, skelly, zombie, wizard
		   (ch[co].class>=500 && ch[co].class<=600) ||  // random dungeon dark skellies
		   (ch[co].class>=388 && ch[co].class<=403)) {	// hell pents demons		
		log_char(cn,LOG_SYSTEM,0,"You just killed your first level %d %s. Congratulations!",ch[co].level,ch[co].name);
	} else if ((ch[co].class>=258 && ch[co].class<=305) || (ch[co].class>=404 && ch[co].class<=411)) {
		if (get_army_rank_int(cn)) {
			log_char(cn,LOG_SYSTEM,0,"You just killed your first level %d %s! The Governor will be proud of you.",ch[co].level,ch[co].name);
			give_military_pts_no_npc(cn,min(ch[co].level/3,10),kill_score(co,cn)*15);
		} else log_char(cn,LOG_SYSTEM,0,"You just killed your first level %d %s!",ch[co].level,ch[co].name);
	} else {	
		log_char(cn,LOG_SYSTEM,0,"You just killed your first %s. Congratulations!",ch[co].name);
	}
}

int check_first_kill(int cn,int nr)
{
	struct firstkill_ppd *ppd;
	int index,offset,mask;

	if (!(ch[cn].flags&CF_PLAYER)) return 0;
	
        if (!(ppd=set_data(cn,DRD_FIRSTKILL_PPD,sizeof(struct firstkill_ppd)))) return 0;	// OOPS

	index=nr/32;
	offset=nr&31;
	mask=1<<offset;

        if (ppd->kill[index]&mask) return 1;
	return 0;
}

void check_military_solve(int cn,int co)
{
	struct military_ppd *ppd;
	int nr;

	if (!(ch[cn].flags&CF_PLAYER)) return;

	if (!(ppd=set_data(cn,DRD_MILITARY_PPD,sizeof(struct military_ppd)))) return;

	if (ppd->took_mission && !ppd->solved_mission) {
		nr=ppd->took_mission-1;

		switch(ppd->mis[nr].type) {
			case 1:		if ((ch[co].class>=52 && ch[co].class<=84) || (ch[co].class>=107 && ch[co].class<=170) || (ch[co].class>=388 && ch[co].class<=403)) { // its a pent demon
						if (ch[co].level==ppd->mis[nr].opt2) {
							ppd->mis[nr].opt1--;
							if (ppd->mis[nr].opt1) {
								if (ppd->mis[nr].opt1<10 ||
								    (ppd->mis[nr].opt1<100 && ppd->mis[nr].opt1%5==0) ||
								    (ppd->mis[nr].opt1%10==0)) {
									log_char(cn,LOG_SYSTEM,0,"°c1Mission kill, %d to go.",ppd->mis[nr].opt1);
								}
							} else {
								log_char(cn,LOG_SYSTEM,0,"You solved your mission. Talk to the governor to claim your reward.");
								ppd->solved_mission=1;
							}
							
						}
					}
					break;
			case 2:		if (ch[co].class>=85 && ch[co].class<=100) { // its a sewer ratling
						if (ch[co].level==ppd->mis[nr].opt2) {
							ppd->mis[nr].opt1--;
							if (ppd->mis[nr].opt1) {
								if (ppd->mis[nr].opt1<10 ||
								    (ppd->mis[nr].opt1<100 && ppd->mis[nr].opt1%5==0) ||
								    (ppd->mis[nr].opt1%10==0)) {
									log_char(cn,LOG_SYSTEM,0,"°c1Mission kill, %d to go.",ppd->mis[nr].opt1);
								}
							} else {
								log_char(cn,LOG_SYSTEM,0,"You solved your mission. Talk to the governor to claim your reward.");
								ppd->solved_mission=1;
							}
							
						}
					}
					break;
		}
	}
}

// kill char cn. if co is set, he gets the fame (blame?) for the kill.
int kill_char(int cn,int co)
{
	int val;

	log_player_death(cn,"kill",1);
	
	char_driver(ch[cn].driver,CDT_DEAD,cn,co,0);
	
	if (ch[cn].flags&CF_RESPAWN) {
		register_respawn_death(cn);
		set_timer(ticker+ch[cn].respawn,respawn_callback,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy,ch[cn].tmpa,0);
	}

	ch[cn].flags|=CF_DEAD;

	if (co && ch[cn].flags) {
		val=kill_score(cn,co);
                give_exp_bonus(co,val);
		give_first_kill(co,cn);
		check_military_solve(co,cn);
		register_kill_in_section(co,cn);
	}

	if (ch[cn].flags&CF_PLAYER) {
		dlog(cn,0,"killed by %s (lvl %d)",ch[co].name,ch[co].level);
	}

        ch[cn].action=AC_DIE;
	ch[cn].act1=co;		// remember killer
	
	if (ch[co].flags&CF_PLAYER)  ch[cn].act2=1;	// remember if PK death (to avoid races due to log-outs)
	else ch[cn].act2=0;
	
	ch[cn].duration=12;
	ch[cn].step=0;	

	return 1;
}

// returns 1 if the character did not return to this server
// ie rest area is on different server OR player was logged out
// OR an error happened.
static int transfer_to_restarea(int cn)
{
	if (ch[cn].flags&CF_PLAYERLIKE) {
		teleport_char_driver(cn,ch[cn].restx,ch[cn].resty);
		return 0;
	}
	
        if (ch[cn].resta!=areaID) {	// respawn point is on different area server
		// if the target server is there, send player there. if it isn't, let him remain here
		if (change_area(cn,ch[cn].resta,ch[cn].restx,ch[cn].resty)) return 1;
	}

	if (!ch[cn].player) {		// character without player - kick now
		exit_char(cn);
		return 1;
	}

	if (ch[cn].x) remove_char(cn);

        if (ch[cn].resta!=areaID || !drop_char(cn,ch[cn].restx,ch[cn].resty,0)) {// yuck! no space to drop char!
		if (ch[cn].player) kick_player(ch[cn].player,"no space to drop char");
		exit_char(cn);
		return 1;
	}
	
	ch[cn].action=ch[cn].duration=ch[cn].step=0;
        purge_messages(cn);
        player_idle(ch[cn].player);

	return 0;
}

// calculate exp lost due to death
int death_loss(int total_exp)
{
	return total_exp/25;
}

int drop_grave(int in,int x,int y,int isplayer)
{
	if (drop_item_extended(in,x,y,5)) return 1;

	switch(areaID) {
		case 8:		return drop_item_extended(in,230,233,5);
		default:	return 0;
	}
}

// death animation is done, now its time to do the real dying
// NPCs are just killed off, players will be relieved of items and EXP
// and transported to the last rest area they've been on.
/* dead body drdata usage:
[0]	tmp (used for... ??)
[2..3]	c1 for player bodies
[4..5]	c2 for player bodies
[6..7]	c3 for player bodies
[8..11]	expire timer call count */
int die_char(int cn,int co,int ispk)
{
	int in,n,x,y,sprite,dir,tmp,ct,offset;
	
        x=ch[cn].x;
	y=ch[cn].y;
	sprite=ch[cn].sprite;
	dir=bigdir(ch[cn].dir);
	tmp=ch[cn].tmp;

	// remove character from map
	remove_char(cn);

	// remove all effects from char
	destroy_chareffects(cn);

	// create item to pose as dead body
	if (ch[cn].flags&CF_NOBODY) {
		in=0;
	} else if (ch[cn].flags&CF_ITEMDEATH) {
                in=ch[cn].item[30];
		ch[cn].item[30]=0;
	} else {
		in=create_item("dead_body");
		if (in) {
			it[in].drdata[0]=tmp;

			// set the right sprite
			switch(sprite) {
				default:	it[in].sprite=100000+sprite*1000+(dir-1)/2*8+335; break;
			}
			sprintf(it[in].description,"%s's body.",ch[cn].name);
			if (ch[cn].flags&CF_PLAYER) {
				it[in].flags|=IF_PLAYERBODY;
				*(unsigned short*)(it[in].drdata+2)=ch[cn].c1;
				*(unsigned short*)(it[in].drdata+4)=ch[cn].c2;
				*(unsigned short*)(it[in].drdata+6)=ch[cn].c3;
			}
		}
	}

	// drop the item
	if (in) {
                if (!drop_grave(in,x,y,(ch[cn].flags&CF_PLAYER)!=0)) {
			if (ch[cn].flags&CF_PLAYER) {
				dlog(cn,0,"could not drop body");
			}
                        free_item(in);
			in=0;
		} else {
			if (ch[cn].flags&CF_PLAYER) {
				dlog(cn,0,"drop body succeeded at %d,%d for item %d",it[in].x,it[in].y,in);
			}
			if (!(it[in].flags&IF_TAKE)) {
				if (ch[cn].flags&CF_ITEMDEATH) set_expire(in,NPC_BODY_DECAY_TIME);
                                else if (ch[cn].flags&CF_PLAYER) set_expire_body(in,PLAYER_BODY_DECAY_TIME);
				else {
					if (areaID!=32) set_expire_body(in,NPC_BODY_DECAY_TIME);
					else set_expire_body(in,NPC_BODY_DECAY_TIME_AREA32);
				}
			} else notify_area(it[in].x,it[in].y,NT_ITEM,in,0,0);
		}
	}

        // dispose of the items
	if (!(ch[cn].flags&(CF_ITEMDEATH|CF_NOBODY)) && in && (ct=create_item_container(in))) {

		con[ct].owner=charID(cn);
		con[ct].owner_not_seyan=0;
		if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))!=(CF_WARRIOR|CF_MAGE)) con[ct].owner_not_seyan=1;

                con[ct].killer=charID(co);

		if (ch[cn].flags&CF_PLAYER) offset=12; else offset=0;
		
		for (n=offset; n<INVENTORYSIZE; n++) {
			if ((in=ch[cn].item[n])) {
				if ((n>11 && n<30) || !add_item_container(ct,in,-1)) {
					free_item(in);
				}
				ch[cn].item[n]=0;
			}
		}
		if ((in=ch[cn].citem)) {
			if (!add_item_container(ct,in,-1)) {
				free_item(in);
			}
			ch[cn].citem=0;
		}
		if (ch[cn].gold) {
			in=create_money_item(ch[cn].gold);
			if (in) {
				if (!add_item_container(ct,in,-1)) {
					free_item(in);
				} else ch[cn].gold=0;
			}			
		}
	} else {
		for (n=0; n<INVENTORYSIZE; n++) {
			if ((in=ch[cn].item[n])) {
				free_item(in);
				ch[cn].item[n]=0;
			}
		}
		
		if ((in=ch[cn].citem)) {
			free_item(in);
			ch[cn].citem=0;
		}
		ch[cn].gold=0;
	}

	// re-create player character at rest point
	if (ch[cn].flags&CF_PLAYER) {
		if (ispk) {
			log_char(cn,LOG_SYSTEM,0,"Thou died by the hands of a player. Thou did not lose any experience points.");

			dlog(cn,0,"died through PK");
		} else {
			int loss,minus,cnt;
	
			loss=death_loss(ch[cn].exp);

			if (loss<25) loss=0;
			else {
				minus=ch[cn].exp_used-ch[cn].exp;
				if (minus>0) {
					cnt=minus/loss+4;
					loss=loss*3/cnt;
				}
			}
			
			if (loss) log_char(cn,LOG_SYSTEM,0,"Thou died and lost some experience points.");
			else log_char(cn,LOG_SYSTEM,0,"Thou died, but since thou art still a Newbie, thou did not lose any experience points.");
	
			ch[cn].exp-=loss;
	
			dlog(cn,0,"died and lost %d exp",loss);
		}

		ch[cn].flags&=~(CF_DEAD);
		ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
		ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
		ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
		ch[cn].deaths++;

		if (transfer_to_restarea(cn)) return 1;
		
		// he's back at the respawn point, everything worked
		ch[cn].serial=sercn++;
		
		update_char(cn);
		
                ch[cn].flags|=CF_ITEMS|CF_UPDATE;

                return 0;
	} else {
		destroy_char(cn);

		return 1;
	}
}

static void extinguish(int cn)
{
        int fn,n;

        // is he on fire?
	for (n=0; n<4; n++) {
		if (!(fn=ch[cn].ef[n])) continue;
		if (ef[fn].type==EF_BURN) {
			remove_effect(fn);
			free_effect(fn);
		}
	}
}


static int god_save_char(int cn)
{
	log_player_death(cn,"saved",0);

	ch[cn].saves--;

	if (ch[cn].saves>10) ch[cn].saves=10;
	ch[cn].got_saved++;
	
	log_char(cn,LOG_SYSTEM,0,"Ishtar's hand reaches down and saves thee from certain death.");
	log_char(cn,LOG_SYSTEM,0,"Thou hast %s saves left.",save_number(ch[cn].saves));

	dlog(cn,0,"was saved");

	ch[cn].hp=1*POWERSCALE;
	remove_all_poison(cn);
	extinguish(cn);

	transfer_to_restarea(cn);	

	return 1;
}

static int shutdown_save_char(int cn)
{
	int loss,minus,cnt;

	log_player_death(cn,"shutdown save",0);
	
	loss=death_loss(ch[cn].exp);
	if (loss<25) loss=0;
	else {
		minus=ch[cn].exp_used-ch[cn].exp;
		if (minus>0) {
			cnt=minus/loss+4;
			loss=loss*3/cnt;
		}
	}
	
	if (loss) log_char(cn,LOG_SYSTEM,0,"Thou died and lost some experience points. Because of the coming shutdown, you did not lose your items.");
	else log_char(cn,LOG_SYSTEM,0,"Thou died, but since thou art still a Newbie, thou did not lose any experience points. Because of the coming shutdown, you did not lose your items. Consider yourself lucky.");

	ch[cn].exp-=loss;

	dlog(cn,0,"died and lost %d exp",loss);
	dlog(cn,0,"was saved (shutdown)");

	ch[cn].hp=1*POWERSCALE;
	remove_all_poison(cn);

	transfer_to_restarea(cn);	

	return 1;
}

static int arena_save_char(int cn)
{
        log_char(cn,LOG_SYSTEM,0,"Yuck. If you hadn't been in an arena... You'd be dead by now. Ave Caesar, morituri te salutant!");

	if (areaID!=13 || !teleport_char_driver_extended(cn,37,247,5)) {
		ch[cn].hp=1*POWERSCALE;
		transfer_to_restarea(cn);
	}

	return 1;
}

// hurt cn with dam pts of damage. cc is the cause
// (and can be zero if no character is the cause).
// damage is in internal HP, that is HP*POWERSCALE.
// hurt deals with armor, so do not substract armor
// protection before calling hurt.
// armordiv is used by spells dealing low damage over
// a longer period of time, so that the armor is only
// substracted once.
// armorper is the percentage of armor that will be used
// normal attacks get 100, good attacks get less. 0 means
// armor will be ignored.
// shieldper is the same for magic shield

int hurt(int cn,int dam,int cc,int armordiv,int armorper,int shieldper)
{
	int str,n,fn,olddam;
        extern int showhurt;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	if (ch[cn].flags&(CF_DEAD)) { error=ERR_UNCONCIOUS; return 0; }
	
	if (cc<0 || cc>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

        if (dam<0) { elog("got negative damage %d to %s (%d) from %s (%d). Set to 0.",dam,ch[cn].name,cn,ch[cc].name,cc); dam=0; }

	olddam=dam;

        str=dam*armorper/100;
	if (str>ch[cn].value[0][V_ARMOR]*POWERSCALE/20/armordiv) dam-=ch[cn].value[0][V_ARMOR]*POWERSCALE/20/armordiv;
	else dam-=str;
        if (dam<0) dam=0;

	if (dam && showhurt) say(cn,"hurt by %s, dam=%.2f/%.2f, armor=%.2f armorper=%d shieldper=%d",ch[cc].name,dam/1000.0,olddam/1000.0,ch[cn].value[0][V_ARMOR]/20.0/armordiv,armorper,shieldper);

        // immortal beings do not suffer any damage
	if (ch[cn].flags&CF_IMMORTAL) dam=0;

	// fdemons only get damage to their back
	if (ch[cn].flags&CF_FDEMON) {
		if (!is_back(cn,cc)) dam/=100;		
	}

        // hard-to-kill characters only get hurt by special weapons
	if (ch[cn].flags&CF_HARDKILL) {
		int in;

		if (!cc || !(in=ch[cc].item[WN_RHAND]) || it[in].ID!=IID_HARDKILL || it[in].drdata[37]<ch[cn].level) dam=0;
	}

        //say(cn,"hurt: %d damage, %d HP",dam,ch[cn].hp);

	// less damage to clanvault character
	if (ch[cn].driver==CDR_CLANVAULT) {
		if (cc && clan_can_attack_inside(ch[cn].clan,ch[cc].clan) && char_dist(cn,cc)<3) dam/=500;
		else return 1;
	}

        if (dam) {	// check for magic shield and death if there was any damage

		if (ch[cn].lifeshield && shieldper>0) {
			if (ch[cn].value[1][V_MAGICSHIELD]) {
				for (n=0; n<4; n++) {
					if (!(fn=ch[cn].ef[n])) continue;
					if (ef[fn].type==EF_MAGICSHIELD) break;				
				}
				if (n==4) create_show_effect(EF_MAGICSHIELD,cn,ticker,ticker+3,16,0);
			}

			str=dam*shieldper/100;
	
			if (ch[cn].lifeshield>=str) ch[cn].lifeshield-=str;
			else { str=ch[cn].lifeshield; ch[cn].lifeshield=0; }
	
			//say(cn,"hurt: damage=%d, str=%d, shield=%d",dam,str,ch[cn].lifeshield);
			
			dam-=str;
		}

                ch[cn].hp-=dam;

                if (dam>=POWERSCALE && (ch[cn].flags&CF_PLAYER)) {
			if (ch[cn].flags&CF_MALE) sound_area(ch[cn].x,ch[cn].y,9);	// ouch
			else  sound_area(ch[cn].x,ch[cn].y,32);
		}

                if ((ch[cn].flags&CF_PLAYER) && ch[cn].driver==CDR_LOSTCON) {
			if (ch[cn].x<1 || ch[cn].x>=MAXMAP || ch[cn].y<1 || ch[cn].y>=MAXMAP || !(map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) {
				player_use_potion(cn);
				player_use_recall(cn);
			}
		}
	
		if (ch[cn].hp<(POWERSCALE/2)) {
			if (ch[cn].flags&CF_PLAYER) {
				log_char(cn,LOG_SYSTEM,0,"Killed with %.2f damage by a lvl %d %s.",
					 dam/1000.0,
					 cc ? ch[cc].level : 0,
					 cc ? ch[cc].name : "unknown causes");
				if (ch[cn].flags&CF_MALE) sound_area(ch[cn].x,ch[cn].y,4);
				else sound_area(ch[cn].x,ch[cn].y,33);
			}
			if (ch[cn].flags&CF_NODEATH) {
				ch[cn].hp=1;
			} else {
				if (ch[cn].flags&(CF_PLAYER|CF_PLAYERLIKE) && (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)) {	// arena death, no loss
					arena_save_char(cn);
				} else {				// normal death, check for saves
					if (cc && (ch[cn].flags&CF_PLAYER) && (ch[cc].flags&CF_PLAYER)) {	// PK death, no saves
						kill_char(cn,cc);
					} else if ((ch[cn].flags&CF_PLAYER) && ch[cn].saves>0)	god_save_char(cn);
					else if ((ch[cn].flags&CF_PLAYER) && shutdown_at && realtime-shutdown_at<30) shutdown_save_char(cn);
                                        else {
						kill_char(cn,cc);
					}
				}
	
				notify_area(ch[cn].x,ch[cn].y,NT_DEAD,cn,cc,0);
			}
		}
	}

	if (ch[cn].flags) {
		ch[cn].regen_ticker=ticker;

		notify_char(cn,NT_GOTHIT,cc,dam,0);
		if (cc && ch[cc].flags) notify_char(cc,NT_DIDHIT,cn,dam,0);

		notify_area(ch[cn].x,ch[cn].y,NT_SEEHIT,cc,cn,0);
		set_sector(ch[cn].x,ch[cn].y);		
	}

	return 1;	
}

int kill_score_level(int cnlev,int cclev)
{
	if (cclev && cclev<=cnlev+10) return cnlev;
	else return 0;
}

int kill_score(int cn,int cc)
{
	// no experience for killing players
	if (ch[cn].flags&CF_PLAYER) return 0;

        return kill_score_level(ch[cn].level,cc ? ch[cc].level : 0);
}

void delayed_hurt_callback(int cn,int serial,int dam,int armorper,int shieldper)
{
	if (!(ch[cn].flags)) return;
	if (ch[cn].serial!=serial) return;
	
	hurt(cn,dam,0,1,armorper,shieldper);
}

int delayed_hurt(int delay,int cn,int dam,int armorper,int shieldper)
{
	return set_timer(ticker+delay,delayed_hurt_callback,cn,ch[cn].serial,dam,armorper,shieldper);
}
