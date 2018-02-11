/*

$Id: edemon.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: edemon.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:12  sam
Added RCS tags


*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "task.h"
#include "sector.h"
#include "act.h"
#include "effect.h"
#include "player_driver.h"
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

// part 1: control room disables defense stations
static int fire=1,pause_till=0;

// part 5: section powerup
static int sect[8]={0,0,0,0,0,0,0,0};	// power status of section 1...8

// sector offset table
static char offx[25]={ 0,-8, 8, 0, 0,-8,-8, 8, 8,-16, 16,  0,  0, -8, -8,  8,  8,-16,-16, 16, 16,-16,-16, 16, 16};
static char offy[25]={ 0, 0, 0,-8, 8,-8, 8,-8, 8,  0,  0,-16, 16,-16, 16,-16, 16, -8,  8, -8,  8,-16, 16,-16, 16};

static int can_hit(int in,int co,int frx,int fry,int tox,int toy)
{
	int dx,dy,x,y,n,cx,cy,m;

	x=frx*1024+512;
	y=fry*1024+512;

	dx=(tox-frx);
	dy=(toy-fry);

        if (abs(dx)<2 && abs(dy)<2) return 0;

	// line algorithm with a step of 0.5 tiles
	if (abs(dx)>abs(dy)) { dy=dy*256/abs(dx); dx=dx*256/abs(dx); }
	else { dx=dx*256/abs(dy); dy=dy*256/abs(dy); }
	
        for (n=0; n<48; n++) {	

		x+=dx;
		y+=dy;
		
		cx=x/1024;
		cy=y/1024;

		if (cx==tox && cy==toy) return 1;

		m=cx+cy*MAXMAP;

		if ((edemonball_map_block(m)) && map[m].it!=in) {
			if (map[m].ch!=co) return 0;
			else return 1;
		}
	}
	return 1;
}

int free_line(int x,int y,int dx,int dy)
{
	int n;

        for (n=0; n<5; n++) {
		x+=dx;
		y+=dy;
		if (edemonball_map_block(x+y*MAXMAP)) return 0;		
	}
	return 1;
}

void edemonball_driver(int in,int cn)
{
	int co,n,m,base;
	int dx,dy,ox=0,oy=0,tx=0,ty=0;
	int dir,dist,left,step,eta,str;

	if (cn) return;

	base=it[in].drdata[1];
	str=it[in].drdata[2];

	// disable/enable for part 1
	if (it[in].drdata[0]==0) {
		if (!fire) {
			if (it[in].sprite!=14160) {
				it[in].sprite=14160;
				set_sector(it[in].x,it[in].y);
			}		
			call_item(it[in].driver,in,0,ticker+10);
			return;
		} else {
			it[in].sprite=14159;
			set_sector(it[in].x,it[in].y);
		}
	}

	// disable/enable for part 5
	if (it[in].drdata[0]>=2 && it[in].drdata[0]<=9) {
		if (sect[it[in].drdata[0]-2]==0 || sect[it[in].drdata[0]-2]>248) {	// turned off
			if (it[in].sprite!=14160) {
				it[in].sprite=14160;
				set_sector(it[in].x,it[in].y);
			}		
			call_item(it[in].driver,in,0,ticker+10);
			return;
		} else {
			it[in].sprite=14161;
			set_sector(it[in].x,it[in].y);
		}
	}

	for (n=0; n<25;  n++) {
		for (co=getfirst_char_sector(it[in].x+offx[n],it[in].y+offy[n]); co; co=ch[co].sec_next) {

                        if (abs(ch[co].x-it[in].x)>10 || abs(ch[co].y-it[in].y)>10) continue;

			if (abs(ch[co].x-it[in].x)>abs(ch[co].y-it[in].y)) {
				if (ch[co].x>it[in].x) ox=1;
				if (ch[co].x<it[in].x) ox=-1;
				oy=0;
			} else {
				if (ch[co].y>it[in].y) oy=1;
				if (ch[co].y<it[in].y) oy=-1;
				ox=0;
			}
	
                        if (ch[co].action!=AC_WALK) { tx=ch[co].x; ty=ch[co].y; }
			else {
	
				dir=ch[co].dir;
				dx2offset(dir,&dx,&dy,NULL);
				dist=map_dist(it[in].x,it[in].y,ch[co].x,ch[co].y);
			
				eta=dist*1.5;
				
				left=ch[co].duration-ch[co].step;
				step=ch[co].duration;
			
				eta-=left;
				if (eta<=0) { tx=ch[co].tox; ty=ch[co].toy; }
				else {
					for (m=1; m<10; m++) {
						eta-=step;
						if (eta<=0) { tx=ch[co].x+dx*m; ty=ch[co].y+dy*m; break; }
					}
			
					// too far away, time-wise to make any prediction. give up.
					if (m==10) { tx=ch[co].x; ty=ch[co].y; }
				}
			}
		
                        if (can_hit(in,co,it[in].x+ox,it[in].y+oy,tx,ty)) {				
				break;
			}
		}

                if (co) {
                        create_edemonball(0,it[in].x+ox,it[in].y+oy,tx,ty,str,base);
	
			call_item(it[in].driver,in,0,ticker+8);
			
			return;
		}
	}

	if (it[in].drdata[3]==0 && !free_line(it[in].x,it[in].y+1,0,1)) it[in].drdata[3]=3;
	if (it[in].drdata[3]==3 && !free_line(it[in].x,it[in].y-1,0,-1)) it[in].drdata[3]=6;
	if (it[in].drdata[3]==6 && !free_line(it[in].x+1,it[in].y,1,0)) it[in].drdata[3]=9;
	if (it[in].drdata[3]==9 && !free_line(it[in].x-1,it[in].y,-1,0)) it[in].drdata[3]=12;

        switch(it[in].drdata[3]) {	
		case 0:         create_edemonball(0,it[in].x,it[in].y+1,it[in].x,it[in].y+10,str,base); it[in].drdata[3]++; break;
		case 1:		create_edemonball(0,it[in].x,it[in].y+1,it[in].x+1,it[in].y+10,str,base); it[in].drdata[3]++; break;
		case 2:		create_edemonball(0,it[in].x,it[in].y+1,it[in].x-1,it[in].y+10,str,base); it[in].drdata[3]++; break;

		case 3:         create_edemonball(0,it[in].x,it[in].y-1,it[in].x,it[in].y-10,str,base); it[in].drdata[3]++; break;
		case 4:		create_edemonball(0,it[in].x,it[in].y-1,it[in].x+1,it[in].y-10,str,base); it[in].drdata[3]++; break;
		case 5:		create_edemonball(0,it[in].x,it[in].y-1,it[in].x-1,it[in].y-10,str,base); it[in].drdata[3]++; break;

		case 6:         create_edemonball(0,it[in].x+1,it[in].y,it[in].x+10,it[in].y,str,base); it[in].drdata[3]++; break;
		case 7:		create_edemonball(0,it[in].x+1,it[in].y,it[in].x+10,it[in].y+1,str,base); it[in].drdata[3]++; break;
		case 8:		create_edemonball(0,it[in].x+1,it[in].y,it[in].x+10,it[in].y-1,str,base); it[in].drdata[3]++; break;

		case 9:         create_edemonball(0,it[in].x-1,it[in].y,it[in].x-10,it[in].y,str,base); it[in].drdata[3]++; break;
		case 10:	create_edemonball(0,it[in].x-1,it[in].y,it[in].x-10,it[in].y+1,str,base); it[in].drdata[3]++; break;
		case 11:	create_edemonball(0,it[in].x-1,it[in].y,it[in].x-10,it[in].y-1,str,base); it[in].drdata[3]++; break;
		default:	it[in].drdata[3]=0; break;
	}

	call_item(it[in].driver,in,0,ticker+16);
}

void edemonswitch_driver(int in,int cn)
{
	if (!cn) {
		if (fire) return;
		
		if (ticker>pause_till) {
			remove_item_light(in);
			fire=1;
			it[in].sprite--;
			it[in].mod_value[0]=64;
			add_item_light(in);
		}
		return;
	}
	if (!fire) {
		log_char(cn,LOG_SYSTEM,0,"The lever seems stuck.");
		return;
	}
	
	remove_item_light(in);
	fire=0;
	it[in].sprite++;
	it[in].mod_value[0]=0;
	pause_till=ticker+TICKS*60*5;
	add_item_light(in);

	call_item(it[in].driver,in,0,pause_till+1);
}

void edemongate_driver(int in,int cn)
{
	int n,co,ser,nr;
	char name[80];
        if (cn) return;

	nr=it[in].drdata[0];

        if (nr==0) {
		static int pos[14]={
			62,157,
			62,164,
			62,174,
			62,184,
			62,191,
			56,174,
			67,174};
	
		for (n=0; n<7; n++) {
			co=*(unsigned short*)(it[in].drdata+4+n*4);
			ser=*(unsigned short*)(it[in].drdata+6+n*4);
	
			if (!co || !ch[co].flags || (unsigned short)ch[co].serial!=(unsigned short)ser) {
				sprintf(name,"edemon2s");
				co=create_char(name,0);
				if (!co) break;
	
				if (item_drop_char(in,co)) {
					
					ch[co].tmpx=pos[n*2];
					ch[co].tmpy=pos[n*2+1];
					
					update_char(co);
					
					ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
					ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
					ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
					
					ch[co].dir=DX_RIGHTDOWN;
		
					*(unsigned short*)(it[in].drdata+4+n*4)=co;
					*(unsigned short*)(it[in].drdata+6+n*4)=ch[co].serial;
					break;	// only one spawn per call
				} else {
					destroy_char(co);
					break;
				}
			}
		}
		call_item(it[in].driver,in,0,ticker+TICKS*10);
	} else if (nr==1) {
		static int pos[100]={0},co_nr[100],serial[100],maxpos=0;

		if (!maxpos) {
			for (n=1; n<MAXITEM; n++) {
				if (!it[n].flags) continue;
				if (it[n].driver!=IDR_EDEMONLIGHT) continue;
				if (it[n].drdata[0]!=4) continue;
                                pos[maxpos++]=it[n].x+it[n].y*MAXMAP;
			}
		}
		for (n=0; n<maxpos; n++) {
			if (!(co=co_nr[n]) || !ch[co].flags || ch[co].serial!=serial[n]) {
				co=create_char("edemon6s",0);
				if (!co) break;

				if (item_drop_char(in,co)) {
					
					ch[co].tmpx=pos[n]%MAXMAP;
					ch[co].tmpy=pos[n]/MAXMAP;
					
					update_char(co);
					
					ch[co].hp=ch[co].value[0][V_HP]*POWERSCALE;
					ch[co].endurance=ch[co].value[0][V_ENDURANCE]*POWERSCALE;
					ch[co].mana=ch[co].value[0][V_MANA]*POWERSCALE;
					
					ch[co].dir=DX_RIGHTDOWN;
		
					co_nr[n]=co;
					serial[n]=ch[co].serial;
					break;	// only one spawn per call
				} else {
					destroy_char(co);
					break;
				}
			}
		}
		call_item(it[in].driver,in,0,ticker+TICKS*20);
	}		
}

void edemonloader_driver(int in,int cn)
{
	int nr,pwr,sprite,in2,ani,m,gsprite;

        nr=it[in].drdata[0];	// section number
	pwr=it[in].drdata[1];	// power status
	ani=it[in].drdata[2];

	if (cn) {	// player using item
		if (pwr) {
			if (ch[cn].citem) log_char(cn,LOG_SYSTEM,0,"There is already a crystal, you cannot add another item.");				
			else log_char(cn,LOG_SYSTEM,0,"The crystal is stuck.");
			return;
		}
		if (!(in2=ch[cn].citem)) {
			log_char(cn,LOG_SYSTEM,0,"Nothing happens.");
			return;
		}
		if (it[in2].ID!=IID_AREA6_YELLOWCRYSTAL) {
			log_char(cn,LOG_SYSTEM,0,"That doesn't fit.");
			return;
		}
		pwr=it[in2].drdata[0];
		ani=7;
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"dropped into edemonloader");
		ch[cn].citem=0;
		ch[cn].flags|=CF_ITEMS;
		destroy_item(in2);
		sound_area(it[in].x,it[in].y,41);
	} else {	// timer call
		if (pwr) pwr--;
		if (ani) ani--;

		call_item(it[in].driver,in,0,ticker+TICKS);
	}
	
	it[in].drdata[2]=ani;
	it[in].drdata[1]=pwr;

	m=it[in].x+it[in].y*MAXMAP;
	gsprite=map[m].gsprite&0x0000ffff;
	
	if (ani) gsprite|=(14247-ani)<<16;
	else if (pwr) gsprite|=14248<<16;
	else gsprite|=14240<<16;

	if (pwr) { sprite=14257+5-(pwr/43); sect[nr]=pwr; }	//50467
	else { sprite=14234; sect[nr]=0; } // 50466;

	if (it[in].sprite!=sprite || map[m].gsprite!=gsprite) {
		it[in].sprite=sprite;
		map[m].gsprite=gsprite;
		set_sector(it[in].x,it[in].y);
		if (sprite==14234) sound_area(it[in].x,it[in].y,43);	// power off
	}
}

void edemonlight_driver(int in,int cn)
{
	int nr,light,sprite;

	if (cn) return;

	nr=it[in].drdata[0];

	if (sect[nr] && sect[nr]<249) {
		light=200;
		sprite=14191;
	} else {
		light=0;
		sprite=14189;
	}

	if (it[in].sprite!=sprite) {
		it[in].sprite=sprite;
		set_sector(it[in].x,it[in].y);
	}

	if (it[in].mod_value[0]!=light) {
		remove_item_light(in);
		it[in].mod_value[0]=light;
		add_item_light(in);
	}
	call_item(it[in].driver,in,cn,ticker+TICKS);
}

int edemondoor_driver(int in,int cn)
{
	int m,in2,n,nr;

	if (!it[in].x) return 2;

	if (!cn) {	// called by timer
		if (it[in].drdata[39]) it[in].drdata[39]--;	// timer counter
                if (!it[in].drdata[0]) return 2;		// if the door is closed already, don't open it again
		if (it[in].drdata[39]) return 2;		// we have more outstanding timer calls, dont close now
	}

	if (!cn && it[in].drdata[5]) return 2;	// no auto-close for this door

        m=it[in].x+it[in].y*MAXMAP;

	if (it[in].drdata[0] && (map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) {	// doorway is blocked
		if (!cn) {	// timer callback - restart
			it[in].drdata[39]++;	// timer counter
			if (!it[in].drdata[5]) call_item(it[in].driver,in,0,ticker+TICKS*5);			
		}
		return 2;
	}

	// if the door needs a key, check if the character has it
	if (cn && (it[in].drdata[1] || it[in].drdata[2])) {
		for (n=30; n<INVENTORYSIZE; n++)
			if ((in2=ch[cn].item[n]))
				if (*(unsigned int*)(it[in].drdata+1)==it[in2].ID) break;
		if (n==INVENTORYSIZE) {
			if (!(in2=ch[cn].citem) || *(unsigned int*)(it[in].drdata+1)!=it[in2].ID) {
				log_char(cn,LOG_SYSTEM,0,"You need a key to use this door.");
				return 2;
			}
		}
		log_char(cn,LOG_SYSTEM,0,"You use %s to %slock the door.",it[in2].name,it[in].drdata[0] ? "" :  "un");
	}

	nr=it[in].drdata[6];
	if (!sect[nr] && cn) {
		log_char(cn,LOG_SYSTEM,0,"The door won't move. It seems somehow lifeless.");
		return 2;
	}

	remove_lights(it[in].x,it[in].y);

	if (cn) sound_area(it[in].x,it[in].y,3);
	else sound_area(it[in].x,it[in].y,2);

        if (it[in].drdata[0]) {	// it is open, close
		it[in].flags|=*(unsigned long long*)(it[in].drdata+30);
		if (it[in].flags&IF_MOVEBLOCK) map[m].flags|=MF_TMOVEBLOCK;
		if (it[in].flags&IF_SIGHTBLOCK) map[m].flags|=MF_TSIGHTBLOCK;
		if (it[in].flags&IF_SOUNDBLOCK) map[m].flags|=MF_TSOUNDBLOCK;
		if (it[in].flags&IF_DOOR) map[m].flags|=MF_DOOR;
                it[in].drdata[0]=0;
		it[in].sprite--;
	} else { // it is closed, open
		*(unsigned long long*)(it[in].drdata+30)=it[in].flags&(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		it[in].flags&=~(IF_MOVEBLOCK|IF_SIGHTBLOCK|IF_DOOR|IF_SOUNDBLOCK);
		map[m].flags&=~(MF_TMOVEBLOCK|MF_TSIGHTBLOCK|MF_DOOR|MF_TSOUNDBLOCK);
		it[in].drdata[0]=1;
		it[in].sprite++;

		it[in].drdata[39]++;	// timer counter
		if (!it[in].drdata[5]) call_item(it[in].driver,in,0,ticker+TICKS*10);		
	}

        reset_los(it[in].x,it[in].y);
        if (!it[in].drdata[38] && !reset_dlight(it[in].x,it[in].y)) it[in].drdata[38]=1;
	add_lights(it[in].x,it[in].y);

	return 1;
}

void edemonblock_driver(int in,int cn)
{
	int m,m2,dx,dy,dir,nr;

	if (cn) {	// player using item
		m=it[in].x+it[in].y*MAXMAP;
		dir=ch[cn].dir;
		dx2offset(dir,&dx,&dy,NULL);
		m2=(it[in].x+dx)+(it[in].y+dy)*MAXMAP;

		if ((map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) || map[m2].it || map[m2].gsprite<12150 || map[m2].gsprite>12158) {
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

		*(unsigned int*)(it[in].drdata)=ticker;

		return;
	} else {	// timer call
		if (!(*(unsigned int*)(it[in].drdata+4))) {	// no coords set, so its first call. remember coords
			*(unsigned short*)(it[in].drdata+4)=it[in].x;
			*(unsigned short*)(it[in].drdata+6)=it[in].y;
		}

		// if 15 minutes have passed without anyone touching the chest, beam it back.
		if (ticker-*(unsigned int*)(it[in].drdata)>TICKS*60*15 &&
		    (*(unsigned short*)(it[in].drdata+4)!=it[in].x ||
		     *(unsigned short*)(it[in].drdata+6)!=it[in].y)) {
			
			m=it[in].x+it[in].y*MAXMAP;
			m2=(*(unsigned short*)(it[in].drdata+4))+(*(unsigned short*)(it[in].drdata+6))*MAXMAP;

			if (!(map[m2].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)) && !map[m2].it) {
				map[m].flags&=~MF_TMOVEBLOCK;
				map[m].it=0;
				set_sector(it[in].x,it[in].y);
	
				map[m2].flags|=MF_TMOVEBLOCK;
				map[m2].it=in;
				it[in].x=*(unsigned short*)(it[in].drdata+4);
				it[in].y=*(unsigned short*)(it[in].drdata+6);
				set_sector(it[in].x,it[in].y);
			}
                }
		call_item(it[in].driver,in,0,ticker+TICKS*5);
	}
}

void edemontube_driver(int in,int cn)
{
	int nr,in2,x,y,n,co,next,light,sprite;

	nr=it[in].drdata[0];

	if (cn) {	// character using it
		teleport_char_driver(cn,*(unsigned short*)(it[in].drdata+2),*(unsigned short*)(it[in].drdata+4));
	} else {	// timer call
		if (sect[nr] && sect[nr]<249) {
			light=200;
			sprite=14138;
		} else {
			light=0;
			sprite=14137;
		}
	
		if (it[in].sprite!=sprite) {
			it[in].sprite=sprite;
			set_sector(it[in].x,it[in].y);
		}
	
		if (it[in].mod_value[0]!=light) {
			remove_item_light(in);
			it[in].mod_value[0]=light;
			add_item_light(in);
		}



		if (!(*(unsigned short*)(it[in].drdata+2))) {
			for (in2=1; in2<MAXITEM; in2++) {
				if (it[in2].driver==IDR_EDEMONLOADER && it[in2].drdata[0]==nr) {
					
					// find teleport target (up or down of item)
					x=it[in2].x; y=it[in2].y;					
					if (!(map[x+y*MAXMAP+MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) y++;
					else if (!(map[x+y*MAXMAP-MAXMAP].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK))) y--;

					// remember target
					*(unsigned short*)(it[in].drdata+2)=x;
					*(unsigned short*)(it[in].drdata+4)=y;

					//xlog("%s (%d) found %s (%d) at %d,%d",it[in].name,in,it[in2].name,in2,it[in2].x,it[in2].y);
				}
			}
		}
		if (sect[nr]>250) {
			for (n=0; n<25;  n++) {
				for (co=getfirst_char_sector(it[in].x+offx[n],it[in].y+offy[n]); co; co=next) {
					
					next=ch[co].sec_next;

					if (!(ch[co].flags&CF_PLAYER)) continue;

					if (abs(ch[co].x-it[in].x)>10 || abs(ch[co].y-it[in].y)>10) continue;

					if (char_see_item(co,in)) {
						teleport_char_driver(co,*(unsigned short*)(it[in].drdata+2),*(unsigned short*)(it[in].drdata+4));
						log_char(co,LOG_SYSTEM,0,"The strange tube teleported you.");
					}
				}
			}
		}
		call_item(it[in].driver,in,0,ticker+TICKS);
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_EDEMONBALL:	edemonball_driver(in,cn); return 1;
		case IDR_EDEMONSWITCH:	edemonswitch_driver(in,cn); return 1;
		case IDR_EDEMONGATE:	edemongate_driver(in,cn); return 1;
		case IDR_EDEMONLOADER:	edemonloader_driver(in,cn); return 1;
		case IDR_EDEMONLIGHT:	edemonlight_driver(in,cn); return 1;
		case IDR_EDEMONDOOR:	return edemondoor_driver(in,cn);
		case IDR_EDEMONBLOCK:	edemonblock_driver(in,cn); return 1;
		case IDR_EDEMONTUBE:	edemontube_driver(in,cn); return 1;

                default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                default:	return 0;
	}
}
int ch_respawn_driver(int nr,int cn)
{
	switch(nr) {
		default:		return 0;
	}
}
