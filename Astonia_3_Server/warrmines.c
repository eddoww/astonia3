/*
 * $Id: warrmines.c,v 1.2 2006/09/28 11:58:38 devel Exp $
 *
 * $Log: warrmines.c,v $
 * Revision 1.2  2006/09/28 11:58:38  devel
 * added questlog
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "skill.h"
#include "area1.h"
#include "consistency.h"
#include "database.h"
#include "sector.h"
#include "sleep.h"
#include "staffer_ppd.h"
#include "flower_ppd.h"
#include "questlog.h"

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
	{{"repeat",NULL},NULL,2},
	{{"restart",NULL},NULL,2},
	{{"please","repeat",NULL},NULL,2},
	{{"please","restart",NULL},NULL,2},
	{{"reset","me",NULL},NULL,3}
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

	if (!(ch[co].flags&CF_PLAYER)) return 0;

	if (char_dist(cn,co)>12) return 0;

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
				if (qa[q].answer) quiet_say(cn,qa[q].answer,ch[co].name,ch[cn].name);
				else return qa[q].answer_code;

				return 1;
			}
		}
	}
	

        return 0;
}


//-----------------------


struct dwarfchief_data
{
        int last_talk;
        int current_victim;
};	

void dwarfchief_driver(int cn,int ret,int lastact)
{
	struct dwarfchief_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_DWARFCHIEFDRIVER,sizeof(struct dwarfchief_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->dwarfchief_state) {
					case 0:         quiet_say(cn,"Welcome, stranger, to Grimroot, home of the dwarves. I would introduce you to our town further, but I have urgent matters to attend to.");
							questlog_open(co,47);
							ppd->dwarfchief_state++; didsay=1;
                                                        break;
					case 1:		quiet_say(cn,"Four of our miners have gone missing, one each in one of the 4 mine areas, and we can only think it's because of those bothersome golems...");
                                                        ppd->dwarfchief_state++; didsay=1;
							break;
                                        case 2:         quiet_say(cn,"If you wish, here's a scroll so you can help one of them. Give this one to the miner in the first section, then come back for another scroll for the next miner.");
                                                        ppd->dwarfchief_state++; didsay=1;
							if (!has_item(co,IID_DWARFRECALL1)) {
								in=create_item("dwarf_recall90");
								if (in) give_char_item(co,in);
							}
                                                        break;
					case 3:		break; // waiting for player to save first miner
					
					case 4:		questlog_done(co,47);
                                                        ppd->dwarfchief_state++;
							// fall-through intended
					case 5:		if (questlog_isdone(co,48)) { ppd->dwarfchief_state=8; break; }
							quiet_say(cn,"Not too bad for a human, you people are sturdier than I thought... Don't cheer up though, the miner in the next section is surrounded by stronger golems. Don't let them hurt your precious nails!");
							questlog_open(co,48);
                                                        ppd->dwarfchief_state++; didsay=1;
                                                        if (!has_item(co,IID_DWARFRECALL2)) {
								in=create_item("dwarf_recall100");
								if (in) give_char_item(co,in);
							}
                                                        break;
					case 6:		break; // waiting for player to save second miner
					
					case 7:		questlog_done(co,48);
                                                        ppd->dwarfchief_state++;
							// fall-through intended
					case 8:		if (questlog_isdone(co,49)) { ppd->dwarfchief_state=11; break; }
							quiet_say(cn,"A job well done! It's that we have enough hands already, otherwise I'd ask you to go out there and mine for us. Anyway, back to business. Go and find the next miner, and you will be rewarded again.");
							questlog_open(co,49);
                                                        ppd->dwarfchief_state++; didsay=1;
                                                        if (!has_item(co,IID_DWARFRECALL2)) {
								in=create_item("dwarf_recall110");
								if (in) give_char_item(co,in);
							}
                                                        break;
					case 9:		break; // waiting for player to save third miner
					
					case 10:	questlog_done(co,49);
                                                        ppd->dwarfchief_state++;
							// fall-through intended
					case 11:	if (questlog_isdone(co,50)) { ppd->dwarfchief_state=14; break; }
							quiet_say(cn,"Just in time! If you had been any later, he wouldn't have had his dinner, and trust me, you don't want to see a dwarf hungry, it's not a pretty sight. The fourth miner should be ok, he always packs more than the others, but do hurry and find him.");
							questlog_open(co,50);
                                                        ppd->dwarfchief_state++; didsay=1;
                                                        if (!has_item(co,IID_DWARFRECALL2)) {
								in=create_item("dwarf_recall120");
								if (in) give_char_item(co,in);
							}
                                                        break;
					case 12:	break; // waiting for player to save fourth miner

					case 13:	questlog_done(co,50);
                                                        ppd->dwarfchief_state++;
							// fall-through intended
					case 14:	quiet_say(cn,"Thank you for saving the last one! You have been of great help to us. Now let's hope they can stay out of the hands of those golems once and for all. Those recall scrolls aren't cheap you know!");
                                                        ppd->dwarfchief_state++; didsay=1;
                                                        break;
					case 15:	break; // all done

				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->dwarfchief_state<=3) { dat->last_talk=0; ppd->dwarfchief_state=0; }
                                                        if (ppd && ppd->dwarfchief_state>=5 && ppd->dwarfchief_state<=6) { dat->last_talk=0; ppd->dwarfchief_state=5; }
							if (ppd && ppd->dwarfchief_state>=8 && ppd->dwarfchief_state<=9) { dat->last_talk=0; ppd->dwarfchief_state=8; }
							if (ppd && ppd->dwarfchief_state>=11 && ppd->dwarfchief_state<=12) { dat->last_talk=0; ppd->dwarfchief_state=11; }
							if (ppd && ppd->dwarfchief_state>=14 && ppd->dwarfchief_state<=15) { dat->last_talk=0; ppd->dwarfchief_state=14; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->dwarfchief_state=0; }
							break;
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;

                        if ((in=ch[cn].citem)) {	// we still have it
				quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
				if (!give_char_item(co,ch[cn].citem)) destroy_item(ch[cn].citem);
				ch[cn].citem=0;				
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct dwarfshaman_data
{
        int last_talk;
        int current_victim;
};	

void dwarfshaman_driver(int cn,int ret,int lastact)
{
	struct dwarfshaman_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_DWARFSHAMANDRIVER,sizeof(struct dwarfshaman_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->dwarfshaman_state) {
					case 0:         quiet_say(cn,"Welcome to Grimroot stranger. To make it here you must have battled some strong foes, though they're nothing compared to what you're about to face, should you accept the quest I am about to give you.");
							questlog_open(co,51);
							ppd->dwarfshaman_state++; didsay=1;
                                                        break;
					case 1:		quiet_say(cn,"But before I give you the quest, I want to see if you can fight the lizards you will be facing. Bring me back 9 lizard's teeth, and I will see that as proof of your strength.");
                                                        ppd->dwarfshaman_state++; didsay=1;
							break;
                                        case 2:         break;	// waiting for teeth
					
					case 3:		if (questlog_isdone(co,52)) { ppd->dwarfshaman_state=6; break; }
							quiet_say(cn,"Ah! I see you've come back with all your teeth, and those of the lizards. I guess you are strong enough after all to do the quest I am about to give you. You see, I've seen the lizards come out with brown berries out of the water.");
							questlog_open(co,52);
                                                        ppd->dwarfshaman_state++; didsay=1;
                                                        break;
                                        case 4:		quiet_say(cn,"Since I hate water, I need others like you to grab them for me. If you want to breath underwater, you will have to combine 3 flowers. I'll leave it up to you to figure out which ones. Now go get me 9 brown berries!");
                                                        ppd->dwarfshaman_state++; didsay=1;
                                                        break;
					case 5:		break; // waiting for berries
					
					case 6:		if (questlog_isdone(co,53)) { ppd->dwarfshaman_state=10; break; }
							quiet_say(cn,"It's good that you can swim, you have no idea how much I hate water. Thanks for the berries. As I suspected, they seem to have magic properties, which I may be able to use.");
							questlog_open(co,53);
							ppd->dwarfshaman_state++; didsay=1;
                                                        break;
					case 7:		quiet_say(cn,"Also, I managed to learn some of the lizard's tongue, and overheard them talking in fear of an 'elite lizard'... If you can find it and bring it's head to me, I can learn more about these lizards, and why they're so varied.");
                                                        ppd->dwarfshaman_state++; didsay=1;
                                                        break;
					case 8:		break; // waiting for elite head
					
					case 9:		quiet_say(cn,"This is quite amazing! The reason these lizards are so varied is due to them being able to somehow absorb magical energy. To much of it seems to affect their mind however, as was the case with this elite lizard.");
							ppd->dwarfshaman_state++; didsay=1;
                                                        break;
					case 10:	quiet_say(cn,"Thank you for helping out! I guess you are sturdier than you look, even though your kind looks skinnier than a dwarven skeleton!");
                                                        ppd->dwarfshaman_state++; didsay=1;
                                                        break;
					case 11:	break; // all done

				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					case 2:         if (ppd && ppd->dwarfshaman_state<=2) { dat->last_talk=0; ppd->dwarfshaman_state=0; }
                                                        if (ppd && ppd->dwarfshaman_state>=3 && ppd->dwarfshaman_state<=5) { dat->last_talk=0; ppd->dwarfshaman_state=3; }
							if (ppd && ppd->dwarfshaman_state>=6 && ppd->dwarfshaman_state<=8) { dat->last_talk=0; ppd->dwarfshaman_state=6; }
							if (ppd && ppd->dwarfshaman_state>=9 && ppd->dwarfshaman_state<=11) { dat->last_talk=0; ppd->dwarfshaman_state=9; }
                                                        break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->dwarfshaman_state=0; ppd->dwarfshaman_count=0; }
							break;
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
			
			ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_LIZARDTOOTH && ppd && ppd->dwarfshaman_state<3) {
					ppd->dwarfshaman_count++;
					if (ppd->dwarfshaman_count>=9) {
						ppd->dwarfshaman_state=3;
						ppd->dwarfshaman_count=0;
						dat->last_talk=0;
						questlog_done(co,51);						
					} else {
						quiet_say(cn,"%d done, %d to go.",ppd->dwarfshaman_count,9-ppd->dwarfshaman_count);
					}
				} else if (it[in].ID==IID_BROWNBERRY && ppd && ppd->dwarfshaman_state>=3 && ppd->dwarfshaman_state<=5) {
					ppd->dwarfshaman_count++;
					if (ppd->dwarfshaman_count>=9) {
						ppd->dwarfshaman_state=6;
						ppd->dwarfshaman_count=0;
						dat->last_talk=0;
						questlog_done(co,52);
					} else {
						quiet_say(cn,"%d done, %d to go.",ppd->dwarfshaman_count,9-ppd->dwarfshaman_count);
					}
				} else if (it[in].ID==IID_LIZARDHEAD && ppd && ppd->dwarfshaman_state>=6 && ppd->dwarfshaman_state<=8) {
                                        ppd->dwarfshaman_state=9;
					dat->last_talk=0;
					questlog_done(co,53);
					destroy_item_byID(co,IID_LIZARDHEAD);
				} else if (give_char_item(co,in)) {
					quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					ch[cn].citem=0;
				}
			
				// let it vanish, then
				if (ch[cn].citem) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct dwarfsmith_data
{
        int last_talk;
        int current_victim;
};	

void dwarfsmith_driver(int cn,int ret,int lastact)
{
	struct dwarfsmith_data *dat;
	struct staffer_ppd *ppd;
        int co,in,didsay=0,talkdir=0,in2;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_DWARFSMITHDRIVER,sizeof(struct dwarfsmith_data));
	if (!dat) return;	// oops...

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*4) { remove_message(cn,msg); continue; }

			if (ticker<dat->last_talk+TICKS*10 && dat->current_victim!=co) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

                        // get current status with player
                        ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if (ppd) {
                                switch(ppd->dwarfsmith_state) {
					case 0:         quiet_say(cn,"Welcome to my smithy! If you are in need of my services, come to me and I will see what I can do for you. For now though, I'm afraid I can't do a whole lot.");
							ppd->dwarfsmith_state++; didsay=1;
                                                        break;
					case 1:         break;	// waiting for mold
					case 2:		break;	// waiting for silver
				}
				if (didsay) {
					dat->last_talk=ticker;
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

                // talk back
		if (msg->type==NT_TEXT) {
			co=msg->dat3;

			if (ch[co].flags&CF_PLAYER) {
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
                                switch((didsay=analyse_text_driver(cn,msg->dat1,(char*)msg->dat2,co))) {
					//case 2:         if (ppd && ppd->dwarfsmith_state<=3) { dat->last_talk=0; ppd->dwarfsmith_state=0; }
                                                        //break;
					case 3:		if (ch[co].flags&CF_GOD) { say(cn,"reset done"); ppd->dwarfsmith_state=0; }
							break;
				}
                                if (didsay) {
					talkdir=offset2dx(ch[cn].x,ch[cn].y,ch[co].x,ch[co].y);
					dat->current_victim=co;
				}
			}
		}

		// got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
			
			ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

                        if ((in=ch[cn].citem)) {	// we still have it
				if (it[in].ID==IID_LIZARDMOLD && ppd && ppd->dwarfsmith_state<=1) {
					ppd->dwarfsmith_state=2;
					ppd->dwarfsmith_type=it[in].drdata[0];
					quiet_say(cn,"What's this? A mold from the lizards? I guess I can make a key out of this, but I will need 5,000 silver to make it. You can't expect me to sacrifice my own ore for your adventuring!");
				} else if (it[in].driver==IDR_ENHANCE && it[in].drdata[0]==1 && *(unsigned int*)(it[in].drdata+1)==5000 && ppd && ppd->dwarfsmith_state==2) {
                                        quiet_say(cn,"There you go, one key for the adventurer.");
					switch(ppd->dwarfsmith_type) {
						case 1:		in2=create_item("lizard_elite_key1"); break;
						case 2:		in2=create_item("lizard_elite_key2"); break;
						case 3:		in2=create_item("lizard_elite_key3"); break;
						default:	in2=0; quiet_say(cn,"oops. bug # 3266/%d",ppd->dwarfsmith_type); break;
					}
					if (in2) {
						if (!give_char_item(co,in2)) destroy_item(in2);						
                                        }
					ppd->dwarfsmith_state=1;
					ppd->dwarfsmith_type=0;
					
				} else if (give_char_item(co,in)) {
					if (it[in].driver==IDR_ENHANCE) {
						if (it[in].drdata[0]!=1) quiet_say(cn,"I'll need silver, not any other material.");
						else if (*(unsigned int*)(it[in].drdata+1)!=5000) quiet_say(cn,"I'll need exactly 5000 units of silver.");
						else quiet_say(cn,"I'll need a mold first.");
					} else quiet_say(cn,"Thou hast better use for this than I do. Well, if there is use for it at all.");
					ch[cn].citem=0;
				}
			
				// let it vanish, then
				if (ch[cn].citem) destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if (talkdir) turn(cn,talkdir);

	if (dat->last_talk+TICKS*30<ticker) {
		if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;
	}

        do_idle(cn,TICKS);
}

struct lostdwarf_data
{
        int nr;
        int invis_tick;
	int last_talk;
};	

void lostdwarf_driver(int cn,int ret,int lastact)
{
	struct lostdwarf_data *dat;
	struct staffer_ppd *ppd;
        int co,in;
	struct msg *msg,*next;

        dat=set_data(cn,DRD_LOSTDWARFDRIVER,sizeof(struct lostdwarf_data));
	if (!dat) return;	// oops...

	if ((ch[cn].flags&CF_INVISIBLE) && dat->invis_tick<ticker) {
		ch[cn].flags&=~CF_INVISIBLE;
		set_sector(ch[cn].x,ch[cn].y);
	}

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                // did we see someone?
		if (msg->type==NT_CREATE) {
                        if (ch[cn].arg) {
				dat->nr=atoi(ch[cn].arg);
			}
		}

		// did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			// dont talk to other NPCs
			if (!(ch[co].flags&CF_PLAYER)) { remove_message(cn,msg); continue; }

			// dont talk to players without connection
			if (ch[co].driver==CDR_LOSTCON) { remove_message(cn,msg); continue; }
			
			// only talk every ten seconds
			if (ticker<dat->last_talk+TICKS*60) { remove_message(cn,msg); continue; }

                        // dont talk to someone we cant see, and dont talk to ourself
			if (!char_see_char(cn,co) || cn==co) { remove_message(cn,msg); continue; }

			// dont talk to someone far away
			if (char_dist(cn,co)>10) { remove_message(cn,msg); continue; }

			quiet_say(cn,"I hope you have a dwarven recall scroll for me! If not, be off with you!");
			dat->last_talk=ticker;

		}

                // got an item?
		if (msg->type==NT_GIVE) {
			co=msg->dat1;
                        if ((in=ch[cn].citem)) {	// we still have it
				ppd=set_data(co,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
				
				if (ppd && ppd->dwarfchief_state<=3 && dat->nr==1 && it[in].ID==IID_DWARFRECALL1) {
					ppd->dwarfchief_state=4;
					say(cn,"Thank you for saving me, %s. I got so hungry I almost ate my beard.",ch[co].name);
					log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s uses a scroll of recall and vanishes.",ch[cn].name);
					ch[cn].flags|=CF_INVISIBLE;
					dat->invis_tick=ticker+TICKS*30;
					set_sector(ch[cn].x,ch[cn].y);
				}

				if (ppd && ppd->dwarfchief_state>=5 && ppd->dwarfchief_state<=6 && dat->nr==2 && it[in].ID==IID_DWARFRECALL2) {
					ppd->dwarfchief_state=7;
					say(cn,"Thank you for saving me, %s. I got so hungry I almost ate my boots.",ch[co].name);
					log_char(co,LOG_SYSTEM,0,"You notice that the dwarf's beard looks somewhat thin.");
					log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s uses a scroll of recall and vanishes.",ch[cn].name);
					ch[cn].flags|=CF_INVISIBLE;
					dat->invis_tick=ticker+TICKS*30;
					set_sector(ch[cn].x,ch[cn].y);
				}

				if (ppd && ppd->dwarfchief_state>=8 && ppd->dwarfchief_state<=9 && dat->nr==3 && it[in].ID==IID_DWARFRECALL3) {
					ppd->dwarfchief_state=10;
					say(cn,"Thank you for saving me, %s. I got so hungry I almost ate my pick-axe.",ch[co].name);
					log_char(co,LOG_SYSTEM,0,"You notice that the dwarf is barefoot.");
					log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s uses a scroll of recall and vanishes.",ch[cn].name);
					ch[cn].flags|=CF_INVISIBLE;
					dat->invis_tick=ticker+TICKS*30;
					set_sector(ch[cn].x,ch[cn].y);
				}

				if (ppd && ppd->dwarfchief_state>=11 && ppd->dwarfchief_state<=12 && dat->nr==4 && it[in].ID==IID_DWARFRECALL4) {
					ppd->dwarfchief_state=13;
					say(cn,"Thank you for saving me, %s. I got so hungry I did eat my pick-axe.",ch[co].name);
                                        log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s uses a scroll of recall and vanishes.",ch[cn].name);
					ch[cn].flags|=CF_INVISIBLE;
					dat->invis_tick=ticker+TICKS*30;
					set_sector(ch[cn].x,ch[cn].y);
				}
			
				// let it vanish, then
				destroy_item(ch[cn].citem);
				ch[cn].citem=0;
			}
		}

		remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

        if (secure_move_driver(cn,ch[cn].tmpx,ch[cn].tmpy,DX_RIGHT,ret,lastact)) return;

        do_idle(cn,TICKS);

}

void oxy_potion(int in,int cn)
{
	if (!cn) return;
	if (!it[in].carried) return;
	
	add_spell(cn,IDR_OXYGEN,TICKS*60,"nonomagic_spell");
	remove_item(in);
	destroy_item(in);
	ch[cn].flags|=CF_ITEMS;
}

void pick_berry(int in,int cn)
{
	int ID,n,old_n=0,old_val=0,in2,ripetime;
	struct flower_ppd *ppd;

	if (!cn) return;
	
	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your hand (mouse cursor) first.");
		return;
	}

	ppd=set_data(cn,DRD_FLOWER_PPD,sizeof(struct flower_ppd));
	if (!ppd) return;	// oops...

	ID=(int)it[in].x+((int)(it[in].y)<<8)+(areaID<<16);

        for (n=0; n<MAXFLOWER; n++) {
		if (ppd->ID[n]==ID) break;
                if (realtime-ppd->last_used[n]>old_val) {
			old_val=realtime-ppd->last_used[n];
			old_n=n;
		}
	}

	if (ch[cn].prof[P_HERBALIST]>=30) ripetime=60*60*4;
        else if (ch[cn].prof[P_HERBALIST]>=20) ripetime=60*60*8;
	else if (ch[cn].prof[P_HERBALIST]>=10) ripetime=60*60*12;
	else ripetime=60*60*24;

	if (n==MAXFLOWER) n=old_n;
	else if (realtime-ppd->last_used[n]<ripetime) {
		log_char(cn,LOG_SYSTEM,0,"It's not ripe yet.");
		return;
	}

	ppd->ID[n]=ID;
	ppd->last_used[n]=realtime;

        switch(it[in].drdata[0]) {
		case 1:		in2=create_item("lizard_brown_berry"); break;
		case 2:		in2=create_item("picked_flower_h"); break;
		case 3:		in2=create_item("picked_flower_i"); break;
		case 4:		in2=create_item("picked_flower_j"); break;

		default:	log_char(cn,LOG_SYSTEM,0,"Bug # 4111c"); return;
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"berry/flower: picked");

	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;
}

void flower_mixer(int in,int cn)
{
	int bit1,bit2,in2;

	if (!cn) return;

	if (!(in2=ch[cn].citem)) {
		log_char(cn,LOG_SYSTEM,0,"No, eating this berry isn't a good idea.");
		return;
	}

	if (it[in2].driver!=IDR_LIZARDFLOWER) {
		log_char(cn,LOG_SYSTEM,0,"This cannot be used together. Try something else.");
		return;
	}

	bit1=it[in].drdata[0];
	bit2=it[in2].drdata[0];

	if (it[in].sprite!=11189 && it[in2].sprite!=11189) {
		log_char(cn,LOG_SYSTEM,0,"A bottle pops out of thin air as you try to combine the flowers. You're stunned for a moment, but then you mix the flowers in the bottle.");
	}
	
        it[in].drdata[0]|=bit2;
	ch[cn].flags|=CF_ITEMS;

	if (it[in].drdata[0]==7) {
		it[in].sprite=11188;
		it[in].driver=128;
		strcpy(it[in].name,"Scuba Potion");
		strcpy(it[in].description,"A bubbly fluid in a nice bottle.");
		log_char(cn,LOG_SYSTEM,0,"The potion seems finished.");
	} else {
		strcpy(it[in].description,"A partially finished scuba potion.");
		it[in].sprite=11189;
	}

	ch[cn].citem=0;
	destroy_item(in2);	
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_DWARFCHIEF:	dwarfchief_driver(cn,ret,lastact); return 1;
		case CDR_LOSTDWARF:	lostdwarf_driver(cn,ret,lastact); return 1;
		case CDR_DWARFSHAMAN:	dwarfshaman_driver(cn,ret,lastact); return 1;
		case CDR_DWARFSMITH:	dwarfsmith_driver(cn,ret,lastact); return 1;

		default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_OXYPOTION:	oxy_potion(in,cn); return 1;
		case IDR_PICKBERRY:	pick_berry(in,cn); return 1;
		case IDR_LIZARDFLOWER:	flower_mixer(in,cn); return 1;
                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                default:		return 0;
	}
}

int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
                default:		return 0;
	}
}






