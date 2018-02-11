/*
 * $Id: lab2.c,v 1.3 2007/09/03 08:58:04 devel Exp $
 *
 * $Log: lab2.c,v $
 * Revision 1.3  2007/09/03 08:58:04  devel
 * fixed exp abuse
 *
 * Revision 1.2  2007/05/02 12:33:27  devel
 * fixed bug with repeat in character names
 *
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
#include "misc_ppd.h"
#include "act.h"
#include "sector.h"
#include "consistency.h"
#include "lab.h"

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

struct lab2_herald_driver_data
{
        int last_talk;
        int next_talk;
};

void lab2_herald_driver(int cn, int ret, int lastact)
{
	struct lab_ppd *ppd;
        struct lab2_herald_driver_data *dat;
        struct msg *msg,*next;
        int co;
        int talkdir=0,didsay=0;
        char *str;

        // get data
        dat=set_data(cn,DRD_LAB2_HERALD,sizeof(*dat));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                }

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                if (msg->type==NT_GIVE) {
                        if (!ch[cn].citem) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source

                        co=msg->dat1;
									// DB: besser: sizeof(struct lab_ppd)
                        if ((ch[co].flags&CF_PLAYER) && (ppd=set_data(co,DRD_LAB_PPD,sizeof(*ppd)))) {
                                if (it[ch[cn].citem].ID==IID_LAB2_ARATHASRING) ppd->herald_talkstep=60;
                        }

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_CHAR) {

                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			
                        // dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
                        // only talk when the old sentence is read
			if (ticker<dat->next_talk) { remove_message(cn,msg); continue; }
			
                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get lab ppd
                        ppd=set_data(co,DRD_LAB_PPD,sizeof(*ppd));
                        if (!ppd) { remove_message(cn,msg); continue; }

			switch(ppd->herald_talkstep) {
                                case 0: // INTRO
                                        say(cn,"Hello %s. I am Herald, the Keeper of this graveyard. I can't say how glad I am to see thee here. I assume thou wantst to pass this test. I need thine help urgently. Horrible things happen here, as thou hast probably noticed. I don't dare to leave the chapel since the dead are rising from their graves. °c4Arathas°c0 has caused this abomination, may his soul rest in peace.",ch[co].name);
                                        didsay=1;
                                        ppd->herald_talkstep++;
                                        dat->next_talk=ticker+10*TICKS;
                                        break;

                                case 1:
                                        say(cn,"If thou wishest to help me, kill °c4Arathas°c0. Bring me his ring as proof, and I will open thee a gate leading out of this part of the Labyrinth.");
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+10*TICKS;
                                        break;

                                case 10: // ARATHAS
                                        say(cn,"I don't know much about him. I just started to read what °c4Elias°c0, his brother, wrote in his °c4diary°c0 when the skeletons attacked me in my study. I'm lucky I got out alive. Now I'm hiding here in the chapel. The undeads dare not enter it.");
                                        didsay=1;
                                        ppd->herald_talkstep++;
                                        dat->next_talk=ticker+10*TICKS;
                                        break;

                                case 11:
                                        say(cn,"But thou wished to hear about Arathas. He and his brother stem from a family of well renowed mages. They have their own °c4family vault°c0 on this graveyard. Arathas died during some kind of magical experiment, and he was buried in the family vault a long time ago.");
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+10*TICKS;
                                        break;

                                case 20: // ELIAS
                                        say(cn,"Elias is Arathas' brother. The last times I saw him, he seemed very frightened. He spoke about strange happenings in the crypt. Now we know what he was talking about.");
                                        didsay=1;
                                        ppd->herald_talkstep++;
                                        dat->next_talk=ticker+10*TICKS;
                                        break;

                                case 21:
                                        say(cn,"One day he entered the family vault. But he did not return, and after a while, his relatives divided his belongings among them. By now, they are all dead, too, and rest in this graveyard. If things were different, I'd show thee their graves, but with the undeads about I do not dare. Thou couldst check the books yourself, for the locations of their tombs. They are in the °c4administrative building°c0. But beware, lots of skeletons and undeads are there, too.");
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+10*TICKS;
                                        break;

                                case 30: // FAMILY VAULT
                                        say(cn,"The family vault is located northeast of the chapel.");
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+5*TICKS;
                                        break;

                                case 40: // ADMINISTRATIVE BUILDING
                                        say(cn,"The administrative building is located northwest of the chapel. All administrative records about the graveyard are stored there.");
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+5*TICKS;
                                        break;

                                case 50: // DIARY
                                        say(cn,"I left the diary in my rooms, in the north-eastern part of the °c4administrative building°c0. I left it there, in my study, when I fled from the undeads.");
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+5*TICKS;
                                        break;

                                case 60: // AFTER RING
                                        say(cn,"I thank thee, %s. This ring proves that thou hast killed Arathas. I hope peace will return now.",ch[co].name);
                                        didsay=1;
                                        ppd->herald_talkstep++;
                                        dat->next_talk=ticker+5*TICKS;
                                        break;

                                case 61: // AFTER RING
                                        say(cn,"And here, my friend, is the Gate, as I have promised thee. Thou hast been most resourceful, %s.",ch[co].name);
                                        didsay=1;
                                        ppd->herald_talkstep++;
                                        dat->next_talk=ticker+5*TICKS;
                                        break;

                                case 62: // AFTER RING
                                        say(cn,"Mayest thou pass the last gate, %s",ch[co].name);
                                        didsay=1;
                                        ppd->herald_talkstep=255;
                                        dat->next_talk=ticker+5*TICKS;
                                        create_lab_exit(co,30);
                                        break;

			}

                        if (didsay) {
                                dat->last_talk=ticker;
                                talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
                        }

		}

                if (msg->type==NT_TEXT) {

                        co=msg->dat3;
                        str=(char *)msg->dat2;

                        if (co==cn) { remove_message(cn,msg); continue; }
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }
			if (!char_see_char(cn,co)) { remove_message(cn,msg); continue; }

                        // get lab ppd
                        ppd=set_data(co,DRD_LAB_PPD,sizeof(*ppd));
                        if (!ppd) { remove_message(cn,msg); continue; }

                        if (strcasestr(str,"ARATHAS")) { ppd->herald_talkstep=10;  dat->next_talk=ticker+TICKS/2; }
                        else if (strcasestr(str,"ELIAS")) { ppd->herald_talkstep=20; dat->next_talk=ticker+TICKS/2; }
                        else if (strcasestr(str,"FAMILY") && strcasestr(str,"VAULT")) { ppd->herald_talkstep=30; dat->next_talk=ticker+TICKS/2; }
                        else if (strcasestr(str,"ADMINISTRATIVE") && strcasestr(str,"BUILDING")) { ppd->herald_talkstep=40; dat->next_talk=ticker+TICKS/2; }
                        else if (strcasestr(str,"DIARY")) { ppd->herald_talkstep=50; dat->next_talk=ticker+TICKS/2; }
			else if (strcasestr(str,"REPEAT")) { say(cn,"I will repeat, %s",ch[co].name); ppd->herald_talkstep=0; dat->next_talk=ticker+TICKS; }
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        if (talkdir) turn(cn,talkdir);

        if (dat->last_talk+TICKS*30<ticker) {
                if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHTDOWN,ret,lastact)) return;		
        }

        do_idle(cn,TICKS);
}

// ------------------------------

struct lab2_deamon_driver_data
{
        int co;                 // enemy
        int serial;             // serial number of enemy
        int talkstep;
        int talkticker;
        char attacking;         // set if the demon is in seek and destory mode
        char observing;         // set if the demon was created an the player had elias stuff
};

struct lab2_player_data
{
        int deamonchecked;
        int numyard;
        int numcrypt;
        unsigned char bitarray[0];
};

void lab2_deamon_create(int x, int y, int co)
{
        struct lab2_deamon_driver_data *dat;
        int cn;

        if (!co) return;

        // check if we already have one
        if ((cn=getfirst_char())) do {
                if (ch[cn].flags && ch[cn].driver==CDR_LAB2DEAMON) {
                        if (!(dat=set_data(cn,DRD_LAB2_DEAMON,sizeof(*dat)))) return;           // oops
                        if (dat->co==co && dat->serial==ch[dat->co].serial) return;             // we already have him
                }
        } while((cn=getnext_char(cn)));

        // create the deamon
        cn=create_char("lab2_daemon",0);
        if (!cn) return;

        ch[cn].dir=DX_DOWN;
        ch[cn].flags&=~CF_RESPAWN;
        update_char(cn);

        ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
        ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
        ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;

        // drop him to map
        if (!drop_char(cn,x,y,0) && !drop_char(cn,x,y+3,0)) {
		destroy_char(cn);
                xlog("drop_char failed (%s,%d)",__FILE__,__LINE__);
                return;
        }

        // set values of the secure move driver
        ch[cn].tmpx=ch[cn].x;
        ch[cn].tmpy=ch[cn].y;

        // get data
        dat=set_data(cn,DRD_LAB2_DEAMON,sizeof(*dat));
	if (!dat) return;	// oops...

        dat->co=co;
        dat->serial=ch[co].serial;

        // create an effect
        create_mist(ch[cn].x,ch[cn].y);
}

int lab2_deamon_is_elias(int co)        // 0=no 1=yes -1=partially
{
        int part=0;

        if (it[ch[co].item[WN_HEAD]].ID==IID_LAB2_ELIASHAT) part++;
        if (it[ch[co].item[WN_CLOAK]].ID==IID_LAB2_ELIASCAPE) part++;
        if (it[ch[co].item[WN_BELT]].ID==IID_LAB2_ELIASBELT) part++;
        if (it[ch[co].item[WN_FEET]].ID==IID_LAB2_ELIASBOOTS) part++;

        if (part==4) return 1;
        if (part==0) return 0;
        return -1;
}

void lab2_deamon_driver(int cn, int ret, int lastact)
{
        struct lab2_deamon_driver_data *dat;
	struct msg *msg,*next;
        int co;
        struct lab2_player_data *player_dat;

        // get data
        dat=set_data(cn,DRD_LAB2_DEAMON,sizeof(*dat));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
                        fight_driver_set_dist(cn,2*MAXMAP,2*MAXMAP,2*MAXMAP);

                        switch(lab2_deamon_is_elias(dat->co)) {
                                case -1:        dat->talkstep=50;
                                                break;

                                case  1:        dat->observing=1;
                                                dat->talkstep=10;
                                                if (dat->co) player_dat=set_data(dat->co,DRD_LAB2_PLAYER,sizeof(*player_dat)); else player_dat=NULL;
                                                if (!player_dat) break;
                                                if (player_dat->deamonchecked) dat->talkstep=20; else player_dat->deamonchecked=1;
                                                break;
                        }
                }

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                if (msg->type==NT_GIVE) {
                        if (!ch[cn].citem) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source

                        // destroy everything we get
                        destroy_item(ch[cn].citem);
                        ch[cn].citem=0;
                }

                if (msg->type==NT_NPC && msg->dat1==NTID_LAB2_DEAMONCHECK) {
                        co=msg->dat2;

                        if (co==dat->co && ch[co].serial==dat->serial && lab2_deamon_is_elias(co)!=1 ) {

                                if (fight_driver_add_enemy(cn,co,1,1)) {
                                        dat->attacking=1;
                                        dat->talkstep=255;
                                        if (dat->observing) shout(cn,"Hey! Thou are not Elias. Now thou shalt die, %s!",ch[co].name);
                                        else shout(cn,"I warned thee. Now thou shalt die, %s!",ch[co].name);
                                }
                        }
                }

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // talking
        switch(dat->talkstep) {
                case  0:        // warn
                                dat->talkticker=ticker+TICKS/8;
                                dat->talkstep++;
                                break;

                case  1:        if (ticker<dat->talkticker) break;
                                say(cn,"STOP!");
                                dat->talkstep++;
                                dat->talkticker=ticker+5*TICKS;
                                if (ch[dat->co].player) player_driver_halt(ch[dat->co].player);
                                break;

                case  2:        if (ticker<dat->talkticker) break;
                                say(cn,"On behalf of Elias, mine Master, I shall not allow anyone but himself to enter this family vault. So try to reach that door and I will kill thee!");
                                dat->talkstep++;
                                dat->talkticker=ticker+8*TICKS;
                                break;

                case  3:        if (ticker<dat->talkticker) break;
                                say(cn,"Ohh. Excuse me, Master. I am ashamed not to have recognized thee immediately. So, Elias, if you might want to enter... WAIT!");
                                dat->talkstep++;
                                dat->talkticker=ticker+4*TICKS;
                                if (ch[dat->co].player) player_driver_halt(ch[dat->co].player);
                                break;

                case  4:        if (ticker<dat->talkticker) break;
                                say(cn,"My eyes tricked me again. Thou art not Elias. I remember excatly his black hat and cape, and also his belt and boots. So, again, go away or I will kill thee!");
                                if (ch[dat->co].player) player_driver_halt(ch[dat->co].player);
                                dat->talkstep=255;
                                break;

                case 10:        // elias
                                dat->talkticker=ticker+TICKS/8;
                                dat->talkstep++;
                                break;

                case 11:        if (ticker<dat->talkticker) break;
                                say(cn,"Ahhh, Master Elias.");
                                dat->talkstep++;
                                dat->talkticker=ticker+2*TICKS;
                                break;

                case 12:        if (ticker<dat->talkticker) break;
                                say(cn,"Excuse me for coming out of my Dimension. I hadn't recognized thee.");
                                dat->talkstep++;
                                dat->talkticker=ticker+6*TICKS;
                                break;

                case 13:        if (ticker<dat->talkticker) break;
                                say(cn,"But 'tis good to see thee around again. Last time we met thou wert in a bad condition. Very bad, I must say, very bad indeed.");
                                dat->talkstep++;
                                dat->talkticker=ticker+7*TICKS;
                                break;

                case 14:        if (ticker<dat->talkticker) break;
                                say(cn,"It must have been a couple of years since we last met. Maybe some couples more. Hahaha.");
                                dat->talkstep++;
                                dat->talkticker=ticker+7*TICKS;
                                break;

                case 15:        if (ticker<dat->talkticker) break;
                                say(cn,"But here I am, talking and talking. Thou sure hast more important things to do than listening to an old demon like me.");
                                dat->talkstep++;
                                dat->talkticker=ticker+9*TICKS;
                                break;

                case 16:        if (ticker<dat->talkticker) break;
                                say(cn,"So, farewell, Elias.");
                                dat->talkstep++;
                                dat->talkticker=ticker+3*TICKS;
                                break;

                case 17:        dat->co=0;
                                dat->talkstep=255;
                                break;

                case 20:        // quick elias
                                dat->talkticker=ticker+TICKS/8;
                                dat->talkstep++;
                                break;

                case 21:        if (ticker<dat->talkticker) break;
                                say(cn,"Ahh, it's you again, Master Elias. See You.");
                                dat->talkstep++;
                                dat->talkticker=ticker+3*TICKS;
                                break;

                case 22:        dat->co=0;
                                dat->talkstep=255;
                                break;


                case 50:        // masquerade
                                dat->talkticker=ticker+TICKS/8;
                                dat->talkstep++;
                                break;

                case 51:        if (ticker<dat->talkticker) break;
                                say(cn,"STOP!");
                                dat->talkstep++;
                                dat->talkticker=ticker+5*TICKS;
                                if (ch[dat->co].player) player_driver_halt(ch[dat->co].player);
                                break;

                case 52:        if (ticker<dat->talkticker) break;
                                say(cn,"What kind of masquerade is that. Thou are wearing some parts of Elias stuff, but thou are not Elias.");
                                dat->talkstep++;
                                dat->talkticker=ticker+4*TICKS;
                                break;

                case 53:        if (ticker<dat->talkticker) break;
                                say(cn,"Do not try to get into the family vault, I won't let you in, and thou will have to die!");
                                dat->talkstep=255;
                                break;

        }

        // stop attacking when player tries to run away
        if (dat->attacking) {
                if (!(map[ch[dat->co].x+ch[dat->co].y*MAXMAP].flags&MF_NOMAGIC)) {
                        if (fight_driver_remove_enemy(cn,dat->co)) {
                                dat->attacking=0;
                                say(cn,"Master Elias told me to let them run away, so my work is done.");
                        }
                }
        }

        // do we need a teleport ? (validate enemy first, cause otherwise we'll teleport into nirwana)
        if (dat->co && ch[dat->co].flags && ch[dat->co].serial==dat->serial) {
                if (dat->attacking && pathfinder(ch[cn].x,ch[cn].y,ch[dat->co].x,ch[dat->co].y,2,NULL,0)==-1 && pathfinder(ch[cn].x,ch[cn].y,ch[dat->co].x,ch[dat->co].y,1,NULL,0)==-1) {
                        int oldx,oldy;

                        oldx=ch[cn].x;
                        oldy=ch[cn].y;
                        teleport_char_driver(cn,ch[dat->co].x,ch[dat->co].y);
                        if (oldx!=ch[cn].x || oldy!=ch[cn].y) {
                                create_mist(oldx,oldy);
                                create_mist(ch[cn].x,ch[cn].y);
                                return;
                        }
                }
        }

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        // rest of standard action
        if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        // remove deamon
        if (!dat->co || (!dat->attacking && char_see_char(cn,dat->co)==0) || ch[dat->co].serial!=dat->serial || !ch[dat->co].flags) {
                create_mist(ch[cn].x,ch[cn].y);
                remove_destroy_char(cn);
                return;
        }

        // go home (???)
        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

        // turn to char
        if (!dat->attacking) turn(cn,offset2dx(ch[cn].x,ch[cn].y,ch[dat->co].x,ch[dat->co].y));

        do_idle(cn,TICKS/2);
}

// ------------------------------

int create_lab2_regenerate_spell(int cn, /*const*/ char *name);
void set_lab2_regenerate_spell_startat(int in, int value);

struct lab2_undead_driver_data
{
        char aggressive;
        char helper;
        char undead;
        char patrol;
        unsigned char pat;              // current patrol coordinate
        unsigned char patstep;          // numer of steps
        unsigned char dummy;            // 4 byte alignment
        unsigned char patx[8],paty[8];  // patrol coords
        int graveit;                    // the grave item (where he will return to) // short2int
        int regenit;                    // the regenrate item    // short2int

        int co;                         // set by the grave. the enemy, just for the died driver and grave bit logic
        int serial;                     // set by the grave. the enemy, just for the died driver and grave bit logic // short2int

        int nextwaitticker;             // used by patrol2 undead not to wait more than once behind the door
};

struct lab2_grave_data
{
        char item;              // grave leads to an item (0=no, 1=hat, 2=cape, 3=boots, 4=belt, 5=amulet (elias), 6=ring (arathas)
        char dummy[3];
        int co;                 // 0 = grave closed, otherwise grave open and undead still walking around // short2int
        int serial;             // short2int
        int nr;                 // grave number used as index into bit array
};

int graves_enumerated=0;
int max_crypt=0,max_yard=0,max_grave=0;
int sizeof_player_data=0;

#define LAB2_GRAVE_VERSION 2
#define MAX_DESCRIBED_GRAVE 40
// #define GRAVE_TWO_BITS

struct described_grave
{
        int x,y;           // coord of the grave
        char *description; // take care, a pointer
};

struct described_grave described_grave[MAX_DESCRIBED_GRAVE]=
{
        { 194,183, "%s is buried in the third grave behind the chapel." },{ 192,183, "%s is buried at the left side of her husband John."},
        { 186,183, "%s is buried in the seventh grave behind the chapel." },{ 184,183, "%s is buried at the left side of her husband John."},
        { 176,194, "For his generosity %s is buried in the third grave of the first row next to the northwestern chapel aisle." },{ 176,196, "%s is buried at the left side of her husband John."},
        { 173,191, "For his generosity %s is buried in the first grave of the second row next to the northwestern chapel aisle." },{ 173,193, "%s is buried at the left side of her husband John."},
        { 199,195, "For his generosity %s is buried in the first grave of the second row next to the southeastern chapel aisle." },{ 199,193, "%s is buried at the left side of her husband John."},
        { 196,196, "For his generosity %s is buried in the first grave of the first row next to the southeastern chapel aisle." },{ 196,194, "%s is buried at the left side of her husband John."},
        { 160,233, "%s is buried in the fifth grave of the second row in the southwest section of the graveyard." },{ 158,233, "%s is buried at the left side of her husband John."},
        { 162,230, "%s is buried in the fourth grave of the third row in the southwest section of the graveyard." },{ 160,230, "%s is buried at the left side of her husband John."},
        { 210,244, "%s is buried in the fourth grave of the second row in the southeast section of the graveyard." },{ 208,244, "%s is buried at the left side of her husband John."},
        { 206,232, "%s is buried in the sixth grave of the sixth row in the southeast section of the graveyard." },{ 204,232, "%s is buried at the left side of her husband John."},
        { 172,228, "%s is buried in the fifth grave of the first row in the northwest entrance section of the graveyard." },{ 172,226, "%s is buried at the left side of her husband John."},
        { 181,224, "%s is buried in the seventh grave of the last row in the northwest entrance section of the graveyard." },{ 181,222, "%s is buried at the left side of her husband John."},
        { 191,222, "%s is buried in the first grave of the last row in the southeast entrance section of the graveyard." },{ 191,224, "%s is buried at the left side of her husband John."},
        { 197,232, "%s is buried in the sixth grave of the second row in the southeast entrance section of the graveyard." },{ 197,234, "%s is buried at the left side of her husband John."},
        { 155,211, "%s is buried in the second grave of the first row in the section with the cross in front of the administrative building." },{ 155,209, "%s is buried at the left side of her husband John."},
        { 164,201, "%s is buried in the seventh grave of the last row in the section with the cross in front of the administrative building." },{ 164,199, "%s is buried at the left side of her husband John."},
        { 158,189, "%s is buried in the second grave of the second row in the section without the cross in front of the administravive building." },{ 158,187, "%s is buried at the left side of her husband John."},
        { 161,189, "%s is buried in the second grave of the third row in the section without the cross in front of the administravive building." },{ 161,187, "%s is buried at the left side of her husband John."},
        { 208,182, "%s is buried in the first grave in the northeastern part of the northeast section of the graveyard." },{ 210,182, "%s is buried at the left side of her husband John."},
        { 214,191, "%s is buried in the fourth grave of the last row in the northeastern part of the northeast section of the graveyard." },{ 212,191, "%s is buried at the left side of her husband John."}
};


int get_grave_value(char *array, int nr)
{
        int byte,bit,cur;

        if (nr<0 || nr>=max_grave) { xlog("fatal get_grave_bit parameter nr=%d in %s %d",nr,__FILE__,__LINE__); return -1; }

#ifdef GRAVE_TWO_BITS
        byte=(2*nr)/8;
        bit=(2*nr)%8;
        cur=array[byte]&(3<<bit);
#else
        byte=nr/8;
        bit=nr%8;
        cur=array[byte]&(1<<bit);
#endif

        return cur;
}

int inc_grave_value(unsigned char *array, int nr)
{
        int byte,bit,old;

        if (nr<0 || nr>=max_grave) { xlog("fatal set_grave_bit parameter nr=%d in %s %d",nr,__FILE__,__LINE__); return -1; }

#ifdef GRAVE_TWO_BITS
        byte=(2*nr)/8;
        bit=(2*nr)%8;
        old=array[byte]&(3<<bit);

        if (old==0) array[byte]=(array[byte]&(~(3<<bit)))|(1<<bit);
        else if (old==1) array[byte]=(array[byte]&(~(3<<bit)))|(2<<bit);
        else if (old==2) array[byte]=(array[byte]&(~(3<<bit)))|(3<<bit);
#else
        byte=nr/8;
        bit=nr%8;
        old=array[byte]&(1<<bit);
        array[byte]|=(1<<bit);
#endif

        return old;
}

void enumerate_graves(void)
{
        int x,y,in,mn;
        struct lab2_grave_data *dat;

        for (y=0; y<MAXMAP; y++) {
                for (x=0; x<MAXMAP; x++) {
                        if (!(in=map[mn=x+y*MAXMAP].it)) continue;
                        if (it[in].driver!=IDR_LAB2_GRAVE) continue;

                        dat=(struct lab2_grave_data *)it[in].drdata;

                        dat->nr=max_grave++;

                        if (map[mn].flags&MF_NOMAGIC) max_crypt++; else max_yard++;

                        // remove the marker from the map ;)
                        if (map[mn].fsprite==(5<<16)) {
                                map[mn].fsprite=0;
                                set_sector(x,y);
                        }

                }
        }

#ifdef GRAVE_TWO_BITS
        sizeof_player_data=sizeof(struct lab2_player_data)+(max_grave*2+7)/8;
#else
        sizeof_player_data=sizeof(struct lab2_player_data)+(max_grave+7)/8;
#endif

        xlog("found %d graves (crypt=%d yard=%d) in lab2 (%d bytes)",max_grave,max_crypt,max_yard,sizeof_player_data);
        graves_enumerated=1;
}

int get_player_grave_value(int in, int cn, struct lab2_grave_data *grave_dat)
{
        struct lab2_player_data *player_dat;

        if (!(ch[cn].flags&CF_PLAYER)) return 0;

        player_dat=set_data(cn,DRD_LAB2_PLAYER,sizeof_player_data);
        if (!player_dat) return 0;

        return get_grave_value(player_dat->bitarray,grave_dat->nr);
}

void set_player_described_graves(int cn, struct lab_ppd *ppd)
{
        if (ppd->graveversion==LAB2_GRAVE_VERSION) return;

        ppd->graveversion=LAB2_GRAVE_VERSION;
        do {
                ppd->graveindex[0]=RANDOM(10)*4;
                ppd->graveindex[1]=RANDOM(10)*4;
                ppd->graveindex[2]=RANDOM(10)*4;

        } while ( ppd->graveindex[0]==ppd->graveindex[1] || ppd->graveindex[1]==ppd->graveindex[2] || ppd->graveindex[2]==ppd->graveindex[0] );

        ppd->graveindex[0]+=RANDOM(2)*2;
        ppd->graveindex[1]+=RANDOM(2)*2;
        ppd->graveindex[2]+=RANDOM(2)*2;
        ppd->graveindex[3]=ppd->graveindex[2]+1;
}

void lab2_undead_driver_parse(int cn, struct lab2_undead_driver_data *dat)
{
	char *ptr,name[64],value[64];

        dat->aggressive=0;
        dat->helper=0;

	for (ptr=nextnv(ch[cn].arg,name,value); ptr; ptr=nextnv(ptr,name,value)) {
                if (!strcmp(name,"aggressive")) dat->aggressive=atoi(value);
                else if (!strcmp(name,"helper")) dat->helper=atoi(value);
                else if (!strcmp(name,"patrol")) dat->patrol=atoi(value);
                else if (!strcmp(name,"undead")) dat->undead=atoi(value);
                else elog("unknown arg for %s (%d): %s",ch[cn].name,cn,name);
	}
}

void lab2_undead_driver(int cn, int ret, int lastact)
{
        struct lab2_undead_driver_data *dat;
	struct msg *msg,*next;
        int co,in,mn,p;
        static int drx=-1,dry; // door x/y
        static int scx,scy,ecx,ecy; // start cooridor x/y, end cooridor x/y

        dat=set_data(cn,DRD_LAB2_UNDEAD,sizeof(*dat));
	if (!dat) return;	// oops...

        // set corridor coords relative to the char position
        if (drx==-1 && dat->patrol==2) {
                drx=ch[cn].tmpx-3;
                dry=ch[cn].tmpy-8;
                scx=ch[cn].tmpx-2;
                scy=ch[cn].tmpy-10;
                ecx=ch[cn].tmpx+17;
                ecy=ch[cn].tmpy-6;
        }

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {

                        lab2_undead_driver_parse(cn,dat);

                        if (dat->graveit) {     // i'm to lazy to create tons of templates
                                dat->aggressive=0;
                                dat->helper=0;
                                fight_driver_set_dist(cn,10,0,100);
                        }
                        else {
                                fight_driver_set_dist(cn,14,0,18);
                        }

                        // only undead get the regenerate feature
                        if (dat->undead) {
                                dat->regenit=create_lab2_regenerate_spell(cn,"lab2_regenerate_spell");
                        }

                        // is it a patrol undead
                        if (dat->patrol==1) {
                                // graveyard (first coord ist at the edge of the top left weld)
                                dat->patx[0]=168;             dat->paty[0]=178;
                                dat->patx[1]=dat->patx[0]+ 0; dat->paty[1]=dat->paty[0]+40;
                                dat->patx[2]=dat->patx[0]+36; dat->paty[2]=dat->paty[0]+40;
                                dat->patx[3]=dat->patx[0]+36; dat->paty[3]=dat->paty[0]+ 0;
                                dat->patstep=4;
                                fight_driver_set_dist(cn,0,10,0);
                                dat->helper=0;
                        }

                        if (dat->patrol==2) {
                                // crypta (first coord ist chr position x and y+1)
                                dat->patx[0]=171;       dat->paty[0]=164;
                                dat->patx[1]=dat->patx[0]-33;   dat->paty[1]=dat->paty[0]+0;
                                dat->patx[2]=dat->patx[0]-33;   dat->paty[2]=dat->paty[0]-18;
                                dat->patx[3]=dat->patx[0]-6;    dat->paty[3]=dat->paty[0]-18;
                                dat->patx[4]=dat->patx[0]-4;    dat->paty[4]=dat->paty[0]-18; // -- say hm.
                                dat->patx[5]=dat->patx[0]-33;   dat->paty[5]=dat->paty[0]-18;
                                dat->patx[6]=dat->patx[0]-33;   dat->paty[6]=dat->paty[0]+0;
                                dat->patx[7]=dat->patx[0]+0;    dat->paty[7]=dat->paty[0]+0;
                                dat->patstep=8;
                                fight_driver_set_dist(cn,0,10,0);
                                dat->helper=0;
                        }
                }

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

                if (msg->type==NT_GIVE) {
                        if (!ch[cn].citem) { remove_message(cn,msg); continue; } // ??? i saw something like this at DBs source

			co=msg->dat1;
                        in=ch[cn].citem;

                        if (it[in].driver==IDR_LAB2_WATER && it[in].drdata[0]==5) {

                                // destroy item
                                destroy_item(in);
                                ch[cn].citem=0;

                                if (ch[co].flags) log_char(co,LOG_SYSTEM,0,"You spill the holy water all over the %s.",ch[cn].name);

                                // check if we are in the secure area
                                mn=ch[cn].x+ch[cn].y*MAXMAP;
                                if ( ((map[mn].flags&MF_NOMAGIC) && !(ch[co].flags&CF_NONOMAGIC)) || dat->undead==0) {
                                        say(cn,"Mwahahahaha...");
                                }
                                else {
                                        say(cn,"Arrgh!");
					ch[cn].flags&=~CF_NODEATH;
                                        create_mist(ch[cn].x,ch[cn].y);
                                        set_lab2_regenerate_spell_startat(dat->regenit,ticker+20*TICKS);
                                        hurt(cn,20*POWERSCALE,co,1,0,0); // he might die,...
                                        remove_message(cn,msg); // we use return, so we remove the message here
                                        return; // ... so we return here!
                                }
                        }
                        else {
                                // destroy everything we get
                                destroy_item(in);
                                ch[cn].citem=0;
                        }

                }

                standard_message_driver(cn,msg,dat->aggressive,dat->helper);

                // the crypt patrol will remove everyone in the second corridor from his enemy list
                if (dat->patrol==2 && msg->type==NT_CHAR) {
                        co=msg->dat1;
                        if (cn!=co && ch[co].x>=scx && ch[co].y>=scy && ch[co].x<=ecx && ch[co].y<=ecy && char_see_char(cn,co)) fight_driver_remove_enemy(cn,co);
                }

                remove_message(cn,msg);
	}

        // a nice thing, kill them when entering the cathedral
        if (map[ch[cn].x+ch[cn].y*MAXMAP].gsprite==20456 || map[ch[cn].x+ch[cn].y*MAXMAP].gsprite==17062) {
                say(cn,"Arrgh!");
                create_mist(ch[cn].x,ch[cn].y);
                kill_char(cn,0);
                return;
        }

        // fighting
        fight_driver_update(cn);
        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        // the crypt patrol will close the door, if he is near to it
        if (dat->patrol==2 && (in=map[drx+dry*MAXMAP].it) && it[in].driver==2 && it[in].drdata[0]==1 && abs(ch[cn].x-drx)<3 && abs(ch[cn].y-dry)<3 && ch[cn].x<drx) { // door is present and open
                if (use_item(cn,in)) return;
        }

        // regeneration
        if (regenerate_driver(cn)) return;
	if (spell_self_driver(cn)) return;

        // patroling
        if (dat->patrol) {
                p=dat->pat;
                if (move_driver(cn,dat->patx[p],dat->paty[p],0)) return;
                if (tmove_driver(cn,dat->patx[p],dat->paty[p],0)) return;
                if (abs(ch[cn].x-dat->patx[p])<3 && abs(ch[cn].y-dat->paty[p])<3) {
                        dat->pat=(dat->pat+1)%dat->patstep;
                        if (dat->patrol==2 && p==0) { do_idle(cn,TICKS*2); }
                        if (dat->patrol==2 && p==3) { say(cn,"A gust of wind?"); do_idle(cn,TICKS*2); }
                        if (dat->patrol==2 && p==4) { say(cn,"Strange."); do_idle(cn,TICKS*2); }
                        return;
                }


        }

        // rest of standard action
        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_DOWN,ret,lastact)) return;

        // use the grave
        if (dat->graveit) {
                if (use_driver(cn,dat->graveit,0)) return;
        }

        do_idle(cn,TICKS/2);
}

void lab2_undead_died_driver(int cn, int co)
{
        struct lab2_grave_data *grave_dat;
        struct lab2_undead_driver_data *undead_dat;
        struct lab2_player_data *player_dat;
        int val,in2;
        int slot,gold=0;
        struct lab_ppd *ppd;

        if (!co) return;
        if (!(ch[co].flags&CF_PLAYER)) return;

        undead_dat=set_data(cn,DRD_LAB2_UNDEAD,sizeof(*undead_dat));
        if (!undead_dat) return;
        if (!undead_dat->graveit) return;

        // only the ones that opened the grave count
        if (undead_dat->co!=co || undead_dat->serial!=ch[co].serial) return;

        grave_dat=(struct lab2_grave_data *)it[undead_dat->graveit].drdata;
        player_dat=set_data(co,DRD_LAB2_PLAYER,sizeof_player_data);
        if (!player_dat) return;

        // ok, here we go...
        val=get_grave_value(player_dat->bitarray,grave_dat->nr);

        if (val==0) {
                if (map[it[undead_dat->graveit].x+it[undead_dat->graveit].y*MAXMAP].flags&MF_NOMAGIC) {
                        // crypta grave
                        player_dat->numcrypt++;

                        // log_char(co,LOG_SYSTEM,0,"%d of %d crypt graves done.",player_dat->numcrypt,max_crypt);

                        if (player_dat->numcrypt==max_crypt-1) {
                                ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd));
                                if (ppd && ppd->timesgotcryptgold==0) { log_char(co,LOG_SYSTEM,0,"Thou notice a silent jingling as thine enemy hits the ground."); ppd->timesgotcryptgold++; gold=500; }
                                else if (ppd && ppd->timesgotcryptgold==1) { log_char(co,LOG_SYSTEM,0,"Again thou notice a silent jingling as thine enemy hits the ground."); ppd->timesgotcryptgold++; gold=500; }
                                else log_char(co,LOG_SYSTEM,0,"Thou notice nothing special as thine enemy hits the ground.");
                        }
                }
                else {
                        // yard grave
                        player_dat->numyard++;

                        // log_char(co,LOG_SYSTEM,0,"%d of %d yard graves done.",player_dat->numyard,max_yard);

                        if (player_dat->numyard==max_yard-1) {
                                ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd));
                                if (ppd && ppd->timesgotyardgold==0) { log_char(co,LOG_SYSTEM,0,"Thou notice a loud jingling as thine enemy hits the ground."); ppd->timesgotyardgold++; gold=7500; }
                                else if (ppd && ppd->timesgotyardgold==1) { log_char(co,LOG_SYSTEM,0,"Again thou notice a loud jingling as thine enemy hits the ground."); ppd->timesgotyardgold++; gold=7500; }
                                else log_char(co,LOG_SYSTEM,0,"Thou notice nothing special as thine enemy hits the ground.");
                        }
                }

                // add money
                for (slot=30; gold && slot<INVENTORYSIZE; slot++) {
			if (ch[cn].item[slot]==0) {
				in2=ch[cn].item[slot]=create_money_item(gold*100);
				it[in2].carried=cn;
				break;
			}
		}
        }

        inc_grave_value(player_dat->bitarray,grave_dat->nr);
};

void lab2_grave(int in, int cn);

void lab2_arathas_awakes_all(int in, int cn)
{
        struct lab2_player_data *player_dat;
        struct lab2_grave_data *grave_dat;
        static int rx[10]={-6,-3, 0,+3,+6, -6,-3, 0,+3,+6};
        static int ry[10]={+3,+3,+3,+3,+3, -3,-3,-3,-3,-3};
        int mn,i;

        if (!(ch[cn].flags&CF_PLAYER)) return;

        player_dat=set_data(cn,DRD_LAB2_PLAYER,sizeof_player_data);
        if (!player_dat) return;

        for (i=0; i<10; i++) {
                mn=(it[in].x+rx[i])+(it[in].y+ry[i])*MAXMAP;
                // we won't need to, but we check the flags here cause it looks nicer if the grave won't open
                grave_dat=(struct lab2_grave_data *)it[map[mn].it].drdata;
                if (get_grave_value(player_dat->bitarray,grave_dat->nr)==0) lab2_grave(map[mn].it,cn);
        }

};

void lab2_grave(int in, int cn)
{
        int co,in2,slot;
        struct lab2_grave_data *dat;
        struct lab2_undead_driver_data *cdat;
        struct lab_ppd *ppd;
        int val,item;

        if (!graves_enumerated) enumerate_graves();

        dat=(struct lab2_grave_data *)it[in].drdata;

        if (cn) {

                if (ch[cn].driver==CDR_LAB2UNDEAD) {
                        remove_destroy_char(cn);
                        return;
                }

                // already open
                if (dat->co) return;

                // players only
                if (!(ch[cn].flags&CF_PLAYER)) return;

                // get ppd
                ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd));
                if (!ppd) { log_char(cn,LOG_SYSTEM,0,"Congratulations, you detected bug no. 12/HIHO/17. Please report this to the development team."); return; }

                // set the graves (will return immediately if this is already done)
                set_player_described_graves(cn,ppd);

                // it's a book (quick and dirty, but it fits ;)
                switch(dat->item) {
                        case 1: log_char(cn,LOG_SYSTEM,0,described_grave[ppd->graveindex[0]].description,"Henry"); return;
                        case 2: log_char(cn,LOG_SYSTEM,0,described_grave[ppd->graveindex[1]].description,"Eldrick"); return;
                        case 3: log_char(cn,LOG_SYSTEM,0,described_grave[ppd->graveindex[2]].description,"John"); return;
                        case 4: log_char(cn,LOG_SYSTEM,0,described_grave[ppd->graveindex[3]].description,"Mariah"); return;
                }

                // check if it's one of the players graves (or one of the fixed item graves)
                if (it[in].x==described_grave[ppd->graveindex[0]].x && it[in].y==described_grave[ppd->graveindex[0]].y) item=1;
                else if (it[in].x==described_grave[ppd->graveindex[1]].x && it[in].y==described_grave[ppd->graveindex[1]].y) item=2;
                else if (it[in].x==described_grave[ppd->graveindex[2]].x && it[in].y==described_grave[ppd->graveindex[2]].y) item=3;
                else if (it[in].x==described_grave[ppd->graveindex[3]].x && it[in].y==described_grave[ppd->graveindex[3]].y) item=4;
                else if (dat->item) item=dat->item;
                else item=0;

                // check if the player already had this grave (ignore if it's a special one)
                val=get_player_grave_value(in,cn,dat);

                if (item==0 && val>0) {
                        log_char(cn,LOG_SYSTEM,1,"This grave is empty");

                        // store the undead values and open the grave
                        dat->co=-1;
                        dat->serial=-1;
                        it[in].sprite++;

                        set_sector(it[in].x,it[in].y);

                        // set the timer for checking if the undead is still around (not true, just to have it a few seconds open ;)
                        call_item(it[in].driver,in,0,ticker+TICKS*5);

                        return;
                }

                // create enemy (special items only on undead, except elias (5), he's a skeleton )
                if (item!=5 && (item || RANDOM(100)<=66) ) co=create_char("lab2_undead",0); else co=create_char("lab2_skeleton",0);
                if (!co) { xlog("create_char failed (%s,%d)",__FILE__,__LINE__); return; }

                // if it is a special grave, create the item
                if (item) {

                        for (slot=30; slot<INVENTORYSIZE; slot++) if (ch[co].item[slot]==0) break;

                        if (slot<INVENTORYSIZE) {
                                switch (item) {
                                        case 1: in2=create_item("lab2_elias_hat"); break;
                                        case 2: in2=create_item("lab2_elias_cape"); break;
                                        case 3: in2=create_item("lab2_elias_boots"); break;
                                        case 4: in2=create_item("lab2_elias_belt"); break;
                                        case 5: in2=create_item("lab2_elias_amulet");
						strcpy(ch[co].name,"Elias Skeleton");
						ch[co].value[1][V_HP]=5;
						ch[co].value[1][V_ATTACK]=5;
						ch[co].value[1][V_PARRY]=5;
						ch[co].level=1;
						break;
                                        case 6: in2=create_item("lab2_arathas_ring"); strcpy(ch[co].name,"Undead Arathas"); break;
                                        default: xlog("unkown item number %d in %s %d",item,__FILE__,__LINE__); in2=0; break;
                                }

                                if (in2) {
                                        ch[co].item[slot]=in2;
                                        it[in2].carried=co;
                                }
                                else xlog("failed to create item %d in %s %d",item,__FILE__,__LINE__);
                        }
                        else xlog("no free slot found, sorry in %s %d",__FILE__,__LINE__);
                }

                // initialize his values
                ch[co].dir=DX_DOWN;
                ch[co].flags&=~CF_RESPAWN;
                update_char(co);

                ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
                ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
                ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;

                // drop him to map
                if (!drop_char(co,it[in].x,it[in].y,0)) {
			destroy_char(co);
                        xlog("drop_char failed (%s,%d)",__FILE__,__LINE__);
                        return;
                }

                // set values of the secure move driver
                ch[co].tmpx=ch[co].x;
                ch[co].tmpy=ch[co].y;

                // set grave item of the char where he can return to
                if ((cdat=set_data(co,DRD_LAB2_UNDEAD,sizeof(*cdat)))) {
                        cdat->graveit=in;
                        cdat->co=cn;                    // just to remember for the died driver
                        cdat->serial=ch[cn].serial;     // just to remember for the died driver
                }

                // set enemy
                if (fight_driver_add_enemy(co,cn,1,1)) say(co,"Thou woke me, %s. Now, thou die!",ch[cn].name);

                // store the undead values and open the grave
                dat->co=co;
                dat->serial=ch[co].serial;
                it[in].sprite++;

                set_sector(it[in].x,it[in].y);

                // set the timer for checking if the undead is still around
                call_item(it[in].driver,in,0,ticker+TICKS*5);

                // and now to something completely different. arathas awakes the other graves
                if (item==6) lab2_arathas_awakes_all(in,cn);
        }
        else {
                if (!dat->co) return;

                // he's gone
                if (dat->serial==-1 || !ch[dat->co].flags || ch[dat->co].serial!=dat->serial) {
			it[in].sprite--;
                        dat->co=0;
                        dat->serial=0;
			set_sector(it[in].x,it[in].y);
                        return;
                }

                // set the timer for checking if the undead is still around
                call_item(it[in].driver,in,0,ticker+TICKS*5);
        }
}

// ------------------------------

// drdata[0]:   0=not inited; 1=well; 2=altar; 3=bowl(unused); 4=waterbowl; 5=holy water bowl;
void lab2_water(int in, int cn)
{
        int in2,in3;

        if (cn) {
                if (it[in].drdata[0]==1) {
                        if (ch[cn].citem) { log_char(cn,LOG_SYSTEM,0,"You won't throw this into the water, will you?"); return; }

                        in2=create_item("lab2_waterbowl");
                        log_char(cn,LOG_SYSTEM,0,"You received a %s.",it[in2].name);

			if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from lab2_water");
                        ch[cn].citem=in2;
                        ch[cn].flags|=CF_ITEMS;
                        it[in2].carried=cn;
                }
                else if (it[in].drdata[0]==2) {
                        if (!ch[cn].citem || it[ch[cn].citem].driver!=IDR_LAB2_WATER || it[ch[cn].citem].drdata[0]!=4) { log_char(cn,LOG_SYSTEM,0,"You feel the holyness of the Altar. Water would be holy now, if you had some."); return; }

                        in2=ch[cn].citem;
                        in3=create_item("lab2_holywaterbowl");

                        replace_item_char(in2,in3);
                        free_item(in2);

                        log_char(cn,LOG_SYSTEM,0,"The water inside your bowl is holy now");
                }
                else if (it[in].drdata[0]==4 || it[in].drdata[0]==5) { // can't happen, item has no IF_USE flag ;-)
                        log_char(cn,LOG_SYSTEM,0,"Skoll!");
                }
        }
        else {
                if (it[in].drdata[0]==0) {
                        // init
                        switch(it[in].sprite) {
                                case 11008:
                                case 11009:
                                case 11010: it[in].drdata[0]=2; break;

                                case 20793:
                                case 20794:
                                case 20795:
                                case 20796: it[in].drdata[0]=1; break;

                                case 11011: it[in].drdata[0]=3; break;
                                case 11012: it[in].drdata[0]=4; break;
                                case 11013: it[in].drdata[0]=5; break;

                                default: xlog("unknown sprite %d/%s in %s %d",it[in].sprite,it[in].name,__FILE__,__LINE__); break;
                        }
                }
        }
}

// ------------------------------

// -- 500
// -- 7500

void lab2_stepaction(int in, int cn)
{
        int type;

        type=it[in].drdata[0];

        switch(type) {
                case 1: break;
                case 2: break;
                default: xlog("unknown lab2 stepaction type %d in %s %d",type,__FILE__,__LINE__); return;
        }

        if (!cn) {
                switch(type) {
                        case 1:
                        case 2: it[in].sprite=0; update_item(in); break;
                }
                return;
        }

        switch(type) {
                case 1: // demon warning
                        if (ch[cn].dir!=DX_UP) break;
                        if (!(ch[cn].flags&CF_PLAYER)) break;
                        lab2_deamon_create(it[in].x,it[in].y-5,cn);
                        break;

                case 2: // demon killing
                        if (!(ch[cn].flags&CF_PLAYER)) break;
                        notify_area(it[in].x,it[in].y,NT_NPC,NTID_LAB2_DEAMONCHECK,cn,0);
                        break;
        }
}

// ------------------------------

struct lab2_regenerate_data
{
        char speed;             // speed in TICKS/24 ;-)
        char regen;             // percentage (0..255) of missing hitpoints to be regained
        char arg3;              // dummy for arguments
        char arg4;              // dummy for arguments
        unsigned int cn;        // cn to regenerate (won't need the serial, cause it's a spell)
        int startat;            // tickervalue when to start (modify this value and regeneration will stop it startat>ticker)
};

// use this funtion to add the regenerate spell item to
// a character. returns 0 if not possible, otherwise it returns the
// item number (to be stored for more sophisticated stuff like the stop
// and go timers)
int create_lab2_regenerate_spell(int cn, /*const*/ char *name)
{
        int slot,in;
        struct lab2_regenerate_data *dat;

        slot=may_add_spell(cn,IDR_LAB2_REGENERATE);
        if (slot==0) return 0;

        in=create_item(name);
        if (in==0) return 0;

        ch[cn].item[slot]=in;
        it[in].carried=cn;

        dat=(struct lab2_regenerate_data *)it[in].drdata;
        dat->cn=cn;

	ch[cn].flags|=CF_NODEATH;

        return in;
}

void set_lab2_regenerate_spell_startat(int in, int value)
{
        struct lab2_regenerate_data *dat;

        if (in==0) { xlog("illegal call of regenerate spell in %s %d",__FILE__,__LINE__); return; }
        if (it[in].driver!=IDR_LAB2_REGENERATE) { xlog("illegal call of regenerate spell in %s %d",__FILE__,__LINE__); return; }

        dat=(struct lab2_regenerate_data *)it[in].drdata;
        dat->startat=value;
}

void lab2_regenerate(int in, int cn)
{
        struct lab2_regenerate_data *dat;
        int max,diff,add;

        if (cn) return;

        dat=(struct lab2_regenerate_data *)it[in].drdata;

	if (it[in].carried==dat->cn) {
		if (ticker>=dat->startat) {
			max=ch[dat->cn].value[0][V_HP]*POWERSCALE;
			diff=max-ch[dat->cn].hp;
			if (diff>0) add=dat->regen*diff/256; else add=1;
			ch[dat->cn].hp=min(max,ch[dat->cn].hp+add);
			ch[dat->cn].flags|=CF_NODEATH;
		} else ch[dat->cn].flags&=~CF_NODEATH;
	}

        call_item(it[in].driver,in,0,ticker+dat->speed*TICKS/24);
}

// ------------------------------

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                case CDR_LAB2HERALD:            lab2_herald_driver(cn,ret,lastact); return 1;
                case CDR_LAB2DEAMON:            lab2_deamon_driver(cn,ret,lastact); return 1;
                case CDR_LAB2UNDEAD:            lab2_undead_driver(cn,ret,lastact); return 1;
                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
                case IDR_LAB2_GRAVE:            lab2_grave(in,cn); return 1;
                case IDR_LAB2_WATER:            lab2_water(in,cn); return 1;
                case IDR_LAB2_STEPACTION:       lab2_stepaction(in,cn); return 1;
                case IDR_LAB2_REGENERATE:       lab2_regenerate(in,cn); return 1;
		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                case CDR_LAB2UNDEAD:            lab2_undead_died_driver(cn,co); return 1;
                case CDR_LAB2DEAMON:            return 1;
                case CDR_LAB2HERALD:            return 1;
                default:	                return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_LAB2UNDEAD:            return 1;
                case CDR_LAB2DEAMON:            return 1;
                case CDR_LAB2HERALD:            return 1;
		default:		        return 0;
	}
}
