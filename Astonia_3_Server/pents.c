/*
 *
 * $Id: pents.c,v 1.9 2007/12/10 10:12:04 devel Exp $
 *
 * $Log: pents.c,v $
 * Revision 1.9  2007/12/10 10:12:04  devel
 * clan changes
 *
 * Revision 1.8  2006/10/08 17:40:52  devel
 * removed on xlog() call
 *
 * Revision 1.7  2006/07/16 22:55:13  ssim
 * small bugfix
 *
 * Revision 1.6  2006/07/01 10:30:50  ssim
 * added online count correction for teufelheim hpent area
 *
 * Revision 1.5  2005/12/18 14:13:50  ssim
 * added missing demon gear (boots,cape)
 *
 * Revision 1.4  2005/12/13 16:44:20  ssim
 * fixed fpents level bug in creation of new demon items (90 -> 70)
 *
 * Revision 1.3  2005/11/23 19:30:46  ssim
 * added hell pents and logic for demon armor clothes in fire/ice pents
 *
 * Revision 1.2  2005/11/22 12:16:46  ssim
 * added stackable (demon) chips
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "item_id.h"
#include "los.h"
#include "libload.h"
#include "clan.h"
#include "act.h"
#include "consistency.h"
#include "skill.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
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

#define MAXLEVEL	56

static int init_done=0;
static int solve_ser=255;
static int total=0,active=0,solve=12;
static int lastsolve=0;
static int power[MAXLEVEL]; /*={
		2000,2000,2000,2000,2000,2000,2000,2000,2000,2000,
		2000,2000,2000,2000,2000,2000,2000,2000,2000,2000,
		2000,2000,2000,2000,2000,2000,2000,2000,2000,2000,
		2000,2000,2000,2000,2000,2000,2000,2000,2000,2000,
		2000,2000,2000,2000,2000,2000,2000,2000};*/
static int areacnt[MAXLEVEL+1];
static int areadone[MAXLEVEL+1];
static int areaser[MAXLEVEL+1]={
	255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255};

static int tpower=0,minlevel=0,maxlevel=0;
static int pent_record=0,pent_record_ID=0;
static int demoncnt=0;
char pent_record_name[40]={"Nobody"};

struct pent_nppd
{
	int status;	// 0 = normal, 1 = got 5 of same color
	int pent_it[6];
        int pent_color[6];
	int pent_value[6];
	int pent_worth[6];
	int bonus;
	
	int pent_cnt;
};

void set_demon_prof(int cn)
{
	int val;

	if (!ch[cn].deaths) ch[cn].deaths=ch[cn].prof[P_DEMON];

	if (ch[cn].class<258 || ch[cn].class>305) {	// normal demon	
		val=ch[cn].deaths*(max(0,tpower)+20000)/64000;
	} else {					// demon lord or hell demon
		val=ch[cn].deaths*(max(0,tpower)+52000)/64000;
	}
		
        if (val!=ch[cn].prof[P_DEMON]) {
		ch[cn].prof[P_DEMON]=val;
		update_char(cn);
	}

	//if (!demon) ch[cn].group=RANDOM(1000)+1;
}

static void solve_pents(int cc)
{
	int cn,exp,n,oldcolor,in,sum;
	struct pent_nppd *nppd;

	for (n=minlevel,sum=0; n<=maxlevel; n++) {
		power[n]+=1000/50;
		if (power[n]>2000) power[n]=2000;
		sum+=power[n];
	}
	if (areaID!=21) tpower=sum;

	for (cn=getfirst_char(); cn; cn=getnext_char(cn)) {
		if (!(ch[cn].flags&CF_PLAYER)) continue;

		if (areaID==25 && ch[cn].x>107) continue;	// no messages for players in RWW

		nppd=set_data(cn,DRD_PENT_NPPD,sizeof(struct pent_nppd));
		if (!nppd) continue;

		for (n=exp=0; n<6; n++) {
			exp+=nppd->pent_worth[n]; nppd->pent_value[n]=nppd->pent_worth[n]=0;

                        if (nppd->status && n<5) {	// color bonus? reset pent colors;
				in=nppd->pent_it[n];

				oldcolor=it[in].drdata[2];
				it[in].drdata[2]=RANDOM(3)+1;
				if (it[in].drdata[1]) it[in].sprite-=oldcolor-it[in].drdata[2];
			}

			nppd->pent_it[n]=nppd->pent_color[n]=0;
		}
		
		exp+=nppd->bonus;
		nppd->status=nppd->bonus=0;

		give_exp_bonus(cn,min(level_value(ch[cn].level)/3,(int)(exp*0.66)));
		log_char(cn,LOG_SYSTEM,0,"%s solved the pentagram quest (tm). You got %d experience points!",ch[cc].name,exp);
		if (tpower>=0) log_char(cn,LOG_SYSTEM,0,"Training area power setting now at %.2f%%.",100.0/32000*tpower);
		else log_char(cn,LOG_SYSTEM,0,"Training area power setting down to 0.00%%, %.2f%% underpowered.",-100.0/32000*tpower);
		
		log_char(cn,LOG_SYSTEM,0,"#30  - Solved -");
		if (get_char_clan(cc)) {
			log_char(cn,LOG_SYSTEM,0,"This solve goes to the %s clan!",get_char_clan_name(cc));
			if (get_char_clan(cc)==get_char_clan(cn)) {
				int cnt;

				cnt=get_clan_bonus(get_char_clan(cc),0);
				if (cnt>0) {
					exp=exp*min(20,cnt)/100;
					if (exp) {
						log_char(cn,LOG_SYSTEM,0,"Your clan's jewels reflected %d exp of the solve to you.",exp);
						give_exp(cn,min(level_value(ch[cn].level)/6,(int)(exp*0.70)));						
					}
				}
			}
		}

		log_char(cn,LOG_SYSTEM,0,"The current record is %d pentagrammas in one run, held by %s. You have %d pentagrammas so far.",pent_record,pent_record_name,nppd->pent_cnt);
	}
	lastsolve=ticker;
}

static void add_pent(int cn,int in,int didsolve)
{
	int level,status,color,nr,n,same,lastcolor,value,worth;
	struct pent_nppd *nppd;
	static char *colortext[4]={"none","red","green","blue"};

	level=*(unsigned char*)(it[in].drdata+0);
	status=*(unsigned char*)(it[in].drdata+1);
	color=*(unsigned char*)(it[in].drdata+2);
	nr=*(unsigned char*)(it[in].drdata+3);
	value=level*50+nr;

        nppd=set_data(cn,DRD_PENT_NPPD,sizeof(struct pent_nppd));
	if (!nppd) return;

	if (didsolve) nppd->bonus+=value*3;

	log_char(cn,LOG_SYSTEM,0,"You got a %s Pentagram, value %d. %d of %d Pentagrammas are active.",
		 colortext[color],value,active,total);

	// add pent to data structure
	for (n=0; n<5; n++) {
		if (nppd->pent_value[n]==value) break;
	}

	if (n==5) {
		if (nppd->status==0) {
			for (n=0; n<5; n++) {
				if (nppd->pent_value[n]<value) break;
			}
			if (n<4) {
				memmove(nppd->pent_it+n+1,nppd->pent_it+n,(4-n)*sizeof(int));
				memmove(nppd->pent_color+n+1,nppd->pent_color+n,(4-n)*sizeof(int));
				memmove(nppd->pent_value+n+1,nppd->pent_value+n,(4-n)*sizeof(int));
				memmove(nppd->pent_worth+n+1,nppd->pent_worth+n,(4-n)*sizeof(int));
			}
			if (n<5) {
				nppd->pent_it[n]=in;
				nppd->pent_color[n]=color;
				nppd->pent_value[n]=value;
				nppd->pent_worth[n]=value/6;
			}
			for (n=same=lastcolor=0; n<5; n++) {
				if (!nppd->pent_value[n]) break;
		
				if (lastcolor==nppd->pent_color[n]) {
					same++;
				} else {
					same=1;
					lastcolor=nppd->pent_color[n];
				}
			}
			if (same==5) {
				nppd->status=1;
				log_char(cn,LOG_SYSTEM,0,"You got five Pentagrammas of the same color!");
			}
		} else {
			if (nppd->pent_value[5]<value) {
				nppd->pent_it[5]=in;
				nppd->pent_color[5]=color;
				nppd->pent_value[5]=value;
				nppd->pent_worth[5]=value;
			}
		}
	}

	// lucky pent
	if (!RANDOM(50)) {
		log_char(cn,LOG_SYSTEM,0,"You got the lucky Pentagram!");
		nppd->bonus+=value+RANDOM(value*3);
	}

	nppd->bonus+=level;

	// list pents
	worth=0;
	if (nppd->pent_value[5]) {
		log_char(cn,LOG_SYSTEM,0,"#3%dCombo: Pentagram value of %d, color of %s, %d exp.",
 		 	 nppd->pent_color[5],
                         nppd->pent_value[5],
			 colortext[nppd->pent_color[5]],
			 nppd->pent_worth[5]);
		worth+=nppd->pent_worth[5];
	} else log_char(cn,LOG_SYSTEM,0,"#3");

	for (n=0; n<5; n++) {
		if (!nppd->pent_value[n]) break;

		log_char(cn,LOG_SYSTEM,0,"#%d%dPentagram value of %d, color of %s, %d exp.",
			 n+4,
			 nppd->pent_color[n],
			 nppd->pent_value[n],
			 colortext[nppd->pent_color[n]],
			 nppd->pent_worth[n]);
		worth+=nppd->pent_worth[n];
			
	}
	log_char(cn,LOG_SYSTEM,0,"#90Bonus: %d, total: %d",nppd->bonus,worth+nppd->bonus);

	nppd->pent_cnt++;
	if (nppd->pent_cnt>pent_record) {
		if (pent_record_ID!=ch[cn].ID) {
			log_char(cn,LOG_SYSTEM,0,"You broke %s's record. New record is now %d pents activated in one run.",pent_record_name,nppd->pent_cnt);
		} else {
			if (nppd->pent_cnt%25==0) {
				log_char(cn,LOG_SYSTEM,0,"You increased your own record to %d pents in one run.",nppd->pent_cnt);
			}
		}
		pent_record=nppd->pent_cnt;
		pent_record_ID=ch[cn].ID;
		strcpy(pent_record_name,ch[cn].name);
	}
}

static void set_pent_solve_cnt(void)
{
	int cnt,cn;

	if (areaID==25) {
		for (cnt=0,cn=getfirst_char(); cn; cn=getnext_char(cn)) {
			if ((ch[cn].flags&CF_PLAYER) && ch[cn].x<108) cnt++;
		}
	} else if (areaID==34) {
		for (cnt=0,cn=getfirst_char(); cn; cn=getnext_char(cn)) {
			if ((ch[cn].flags&CF_PLAYER) && ch[cn].x>=129 && ch[cn].x<=238 && ch[cn].y<188) cnt++;
		}
	} else cnt=online;

	solve=12+(cnt+1)*4+RANDOM((cnt+1)*7);
	
	if (solve>total-total/4) solve=total-total/4;

	if (areaID==21) solve=12;
}


void pent_init(void)
{
	int n,l;
	int level_nr[MAXLEVEL+1];

	bzero(level_nr,sizeof(level_nr));

	for (n=MAXITEM-1; n>0; n--) {
		if (!it[n].flags) continue;
                if (it[n].driver!=IDR_PENT) continue;
		
		// set number
		l=it[n].drdata[0];	// level
		level_nr[l]++;
		it[n].drdata[3]=level_nr[l];

                // set color if not already set
		if (!it[n].drdata[2]) it[n].drdata[2]=RANDOM(3)+1;

		// adjust total
		total++;

		// adjust number of active pents if pent is active
		if (it[n].drdata[1] && it[n].drdata[1]==solve_ser) active++;

		// increase level cnt
		areacnt[l]++;
		if (it[n].drdata[1] && it[n].drdata[4]==areaser[l]) areadone[l]++;
	}
	xlog("initialised %d pents, %d are active.",total,active);
	for (n=1; n<=MAXLEVEL; n++) {
                xlog(" level %d, %d pents",n,level_nr[n]);
		if (level_nr[n] && !minlevel) minlevel=n;
		if (level_nr[n]) maxlevel=n;		
	}
	minlevel--;
	maxlevel--;
	xlog("minlevel=%d, maxlevel=%d",minlevel,maxlevel);
	set_pent_solve_cnt();
}

void pentenhance_char(int cn)
{
	int v,n;

	for (n=0; n<3; n++) {
		v=RANDOM(V_MAX);
		switch(v) {
			case V_HAND:
			case V_ATTACK:
			case V_PARRY:
			case V_TACTICS:
			case V_FREEZE:
			case V_FLASH:
			case V_FIREBALL:
			case V_IMMUNITY:	if (ch[cn].value[1][v]) {
							ch[cn].value[1][v]+=RANDOM(ch[cn].value[1][v]);
							sprintf(ch[cn].name,"%s %d",skill[v].name,ch[cn].value[1][v]);
						}
						break;
		}
	}
}

void pent_driver(int in,int cn)
{
	int co,ser,level,n,status,color,cnt=0,nr,maxspawn,areastatus;
	char name[80];

	if (!init_done) {
		pent_init();
		init_done=1;
	}

        level=*(unsigned char*)(it[in].drdata+0);
	status=*(unsigned char*)(it[in].drdata+1);
	color=*(unsigned char*)(it[in].drdata+2);
	nr=*(unsigned char*)(it[in].drdata+3);
	areastatus=*(unsigned char*)(it[in].drdata+4);

	if (cn) {	// player activated pent
		if (!status) {	// player use only allowed when pent is not active
			status=*(unsigned char*)(it[in].drdata+1)=solve_ser;	// activate pent
			it[in].sprite+=color;
			remove_item_light(in); it[in].mod_value[0]=100; add_item_light(in);
			
                        active++;
			sound_area(it[in].x,it[in].y,42);

			areadone[level]++;
			areastatus=*(unsigned char*)(it[in].drdata+4)=areaser[level];
			//xlog("area %d done = %d of %d",level,areadone[level],areacnt[level]);
			if (areadone[level]>=areacnt[level]) {
				areaser[level]++;
				if (areaser[level]>255) areaser[level]=1;
				//xlog("area %d will reset: %d",level,areaser[level]);
			}
			
			if (active>=solve) {
				add_pent(cn,in,1);
				solve_ser++; if (solve_ser>255) solve_ser=1;
				active=0;
				
				set_pent_solve_cnt();
				
				solve_pents(cn);
				return;	// no spawn on solve
			} else add_pent(cn,in,0);

			cnt=3;	// spawn three on activate
		} else return;
	} else {	// if (!cn) ...
		call_item(it[in].driver,in,0,ticker+RANDOM(TICKS*5)+TICKS*2);
		if (status && (status!=solve_ser || areastatus!=areaser[level])) {			// pent is active, but quest has been solved
			status=*(unsigned char*)(it[in].drdata+1)=*(unsigned char*)(it[in].drdata+4)=0;	// mark pent non-active
			it[in].sprite-=color;
			remove_item_light(in); it[in].mod_value[0]=10; add_item_light(in);
			cnt=3;	// spawn three on reset

			if (areadone[level]>0) areadone[level]--;
		} else if (!status) { 			// pent is not active, but quest has not been solved		
                        if (!RANDOM(15)) cnt=1;		// spawn one on normal timer call
			else return;			
		} else return;
	}

	if (!cnt) return;	// sanity check

	if (level<16) maxspawn=3;
	else maxspawn=2;

        for (n=0; n<maxspawn; n++) {
		co=*(unsigned short*)(it[in].drdata+6+n*4);
		ser=*(unsigned short*)(it[in].drdata+8+n*4);
		
		if (!co || !ch[co].flags || (unsigned short)ch[co].serial!=(unsigned short)ser) {
			sprintf(name,"penter%d",level*2+RANDOM(2));
			co=create_char(name,0);
			if (!co) break;			

                        if (item_drop_char(in,co)) {
				
				ch[co].tmpx=ch[co].x;
				ch[co].tmpy=ch[co].y;

				//pentenhance_char(co);

                                update_char(co);

				if (ch[co].value[0][V_BLESS]) bless_someone(co,ch[co].value[0][V_BLESS],BLESSDURATION);
				
				ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
				ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
				ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
				ch[co].lifeshield=ch[co].value[0][V_MAGICSHIELD]*POWERSCALE;
				
				ch[co].dir=DX_RIGHTDOWN;
				ch[co].flags|=CF_NONOTIFY;

				//if (!demon) ch[co].group=RANDOM(1000)+1;

				*(unsigned short*)(it[in].drdata+6+n*4)=co;
				*(unsigned short*)(it[in].drdata+8+n*4)=ch[co].serial;
				cnt--;
				if (cnt<1) break;	// only cnt spawns per call				
			} else {
				destroy_char(co);
                                break;
			}
		}
	}	
}

void pentboss_door_driver(int in,int cn)
{
	int x,y,oldx,oldy,dx,dy;

        if (!cn) return;	// always make sure its not an automatic call if you don't handle it

        dx=(ch[cn].x-it[in].x);
	dy=(ch[cn].y-it[in].y);

	if (ticker-lastsolve>TICKS*30 && (dx>0 || dy>0)) {
		log_char(cn,LOG_SYSTEM,0,"The door won't open. It seems it is only accessible directly after a solve.");
		return;
	}

	if (dx && dy) return;

        x=it[in].x-dx;
	y=it[in].y-dy;

	if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s touches a teleport object but nothing happens - BUG (%d,%d).",ch[cn].name,x,y);
		return;
	}

        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);
	
	if (!drop_char(cn,x,y,0)) {
		log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,10,"%s says: \"Please try again soon. Target is busy.\"",it[in].name);
		drop_char(cn,oldx,oldy,0);
	}

	switch(ch[cn].dir) {
		case DX_RIGHT:	ch[cn].dir=DX_LEFT; break;
		case DX_LEFT:	ch[cn].dir=DX_RIGHT; break;
		case DX_UP:	ch[cn].dir=DX_DOWN; break;
		case DX_DOWN:	ch[cn].dir=DX_UP; break;
	}	
}

void penter(int cn,int ret,int lastact)
{
	struct msg *msg,*next;
	int n,in=0,prob,p;

	if (ch[cn].level<=38) prob=2000;
	else if (ch[cn].level<=70) prob=1000;
	// max ice pents: 102
	// max hell pents: 118
	else prob=666;

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

		switch (msg->type) {
			case NT_CREATE:
				demoncnt++;
				if (demoncnt>prob && !ch[cn].item[30]) {
					p=RANDOM(1000);
					if (p<100) {
						n=RANDOM(12);
						if (ch[cn].level<=38) {
							switch (n) {
								case 0: in=create_item("demon_sleeves1"); break;
								case 1: in=create_item("demon_leggings1"); break;
								case 2: in=create_item("demon_helmet1"); break;
								case 3: in=create_item("demon_armor1"); break;
								case 4: in=create_item("demon_bracelet1"); break;
								case 5: in=create_item("demon_hat1"); break;
								case 6: in=create_item("demon_trousers1"); break;
								case 7: in=create_item("demon_vest1"); break;
								case 8: in=create_item("demon_cape1"); break;
								case 9: in=create_item("demon_cape1"); break;
								case 10: in=create_item("demon_boots1"); break;
								case 11: in=create_item("demon_boots1"); break;

							}
						} else if (ch[cn].level<=70) {
							switch (n) {
								case 0: in=create_item("demon_sleeves2"); break;
								case 1: in=create_item("demon_leggings2"); break;
								case 2: in=create_item("demon_helmet2"); break;
								case 3: in=create_item("demon_armor2"); break;
								case 4: in=create_item("demon_bracelet2"); break;
								case 5: in=create_item("demon_hat2"); break;
								case 6: in=create_item("demon_trousers2"); break;
								case 7: in=create_item("demon_vest2"); break;
								case 8: in=create_item("demon_cape2"); break;
								case 9: in=create_item("demon_cape2"); break;
								case 10: in=create_item("demon_boots2"); break;
								case 11: in=create_item("demon_boots2"); break;
							}
						} else {
							switch (n) {
								case 0: in=create_item("demon_sleeves3"); break;
								case 1: in=create_item("demon_leggings3"); break;
								case 2: in=create_item("demon_helmet3"); break;
								case 3: in=create_item("demon_armor3"); break;
								case 4: in=create_item("demon_bracelet3"); break;
								case 5: in=create_item("demon_hat3"); break;
								case 6: in=create_item("demon_trousers3"); break;
								case 7: in=create_item("demon_vest3"); break;
								case 8: in=create_item("demon_cape3"); break;
								case 9: in=create_item("demon_cape3"); break;
								case 10: in=create_item("demon_boots3"); break;
								case 11: in=create_item("demon_boots3"); break;
							}
						}						
					} else if (p<900) in=create_item("bronzechip");
					else if (p<990) in=create_item("silverchip");
					else in=create_item("goldchip");
					
					if (in) {
						it[in].carried=cn;
						ch[cn].item[30]=in;
						demoncnt=0;
						//xlog("created %s/%s (%X)",it[in].name,it[in].description,it[in].ID);
					}
				}
				break;
		}
	}
	set_demon_prof(cn);

	char_driver(CDR_SIMPLEBADDY,CDT_DRIVER,cn,ret,lastact);
}

void penter_dead(int cn,int co)
{
	int i,str,val;

        if ((ch[cn].class<258 || ch[cn].class>305) && (ch[cn].class<404 || ch[cn].class>411)) return;
	
	if (ch[cn].class>=258 && ch[cn].class<=305) i=ch[cn].class-258;
	else i=ch[cn].class-404+48;

        val=750;
	str=power[i]+val;
	power[i]=-val;

	tpower-=str;	

        if (co) {
		if (tpower>=0) log_char(co,LOG_SYSTEM,0,"Training area power setting down to %.2f%%, loss of %.2f%%.",100.0/32000*tpower,100.0/32000*str);
		else log_char(co,LOG_SYSTEM,0,"Training area power setting down to 0.00%%, %.2f%% underpowered, loss of %.2f%%.",-100.0/32000*tpower,100.0/32000*str);
	}
}


//..............
struct tester_data
{
	int useitem;
	int pickitem;
	int killchar;

	int x,y;
	int seenpent[10];
};

void tester(int cn,int ret,int lastact)
{
        struct tester_data *dat;
	struct msg *msg,*next;
        int co,n,in;

	dat=set_data(cn,DRD_TESTERDRIVER,sizeof(struct tester_data));
	if (!dat) return;	// oops...

	scan_item_driver(cn);

        // loop through our messages
	for (msg=ch[cn].msg; msg; msg=next) {
		next=msg->next;

                if (msg->type==NT_CREATE) {
			;
                }

                // did we see someone?
		if (msg->type==NT_CHAR) {
			
                        co=msg->dat1;

			if (!dat->killchar && ch[co].driver==CDR_PENTER && char_see_char(cn,co)) dat->killchar=co;			
		}

		if (msg->type==NT_TEXT) {
			co=msg->dat3;
			tabunga(cn,co,(char*)msg->dat2);
		}

		if (msg->type==NT_ITEM) {
			in=msg->dat1;
			if (!dat->pickitem && it[in].driver==IDR_POTION && char_see_item(cn,in)) dat->pickitem=in;
			if (it[in].driver==IDR_PENT && !it[in].drdata[1] && char_see_item(cn,in)) {
				if (!dat->useitem) dat->useitem=in;
				else {
					for (n=0; n<10; n++) {
						if (dat->seenpent[n]==in) break;						
					}
					if (n==10) dat->seenpent[RANDOM(10)]=in;
				}
			}
		}

                standard_message_driver(cn,msg,0,0);
                remove_message(cn,msg);
	}

        // do something. whenever possible, call do_idle with as high a tick count
	// as reasonable when doing nothing.

	if ((in=ch[cn].citem)) {
		say(cn,"got item");
		if (it[in].driver==IDR_POTION) {
			for (n=30; n<INVENTORYSIZE; n++) {
				if (!ch[cn].item[n]) break;				
			}
			if (n<INVENTORYSIZE) swap(cn,n);
		}
	}
	if ((in=ch[cn].citem)) { say(cn,"junked item"); destroy_item(in); ch[cn].citem=0; }

	if (ch[cn].hp<ch[cn].value[0][V_HP]*POWERSCALE/2) {
		say(cn,"wanna use potion");
		for (n=30; n<INVENTORYSIZE; n++) {
			if ((in=ch[cn].item[n]) && it[in].driver==IDR_POTION && it[in].drdata[1]) {
				if (it[in].drdata[2]) { use_item(cn,in); break; }
				if (!it[in].drdata[2]) { use_item(cn,in); break; }
			}
		}
	}

        fight_driver_update(cn);

        if (fight_driver_attack_visible(cn,0)) return;
        if (fight_driver_follow_invisible(cn)) return;

        if (regenerate_driver(cn)) return;
        if (spell_self_driver(cn)) return;

	if (dat->pickitem) {
		if (take_driver(cn,dat->pickitem)) return;
		dat->pickitem=0;
	}

	if (!dat->useitem) {
		for (n=0; n<10; n++) {
			if (dat->seenpent[n]) {
				dat->useitem=dat->seenpent[n];
				dat->seenpent[n]=0;
				break;
			}
		}
	}

        if (dat->useitem) {
		if (!it[dat->useitem].drdata[1] && use_driver(cn,dat->useitem,0)) return;
		dat->useitem=0;
	}

	if (move_driver(cn,dat->x,dat->y,2)) return;

	dat->x=ch[cn].x+RANDOM(10)-5;
	dat->y=ch[cn].y+RANDOM(10)-5;

        do_idle(cn,TICKS);
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
		case CDR_PENTER:	penter(cn,ret,lastact); return 1;
		case CDR_TESTER:	tester(cn,ret,lastact); return 1;

                default:		return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_PENT:		pent_driver(in,cn); return 1;
		case IDR_PENTBOSSDOOR:	pentboss_door_driver(in,cn); return 1;

                default:		return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
		case CDR_PENTER:	penter_dead(cn,co); return 1;
		case CDR_TERION:	return 1;
                default:		return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		case CDR_PENTER:	return 1;
		case CDR_TESTER:	return 1;
                default:		return 0;
	}
}



































