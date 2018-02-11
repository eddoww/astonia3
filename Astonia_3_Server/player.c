/*
 *
 * $Id: player.c,v 1.12 2007/08/21 22:08:41 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: player.c,v $
 * Revision 1.12  2007/08/21 22:08:41  devel
 * more frequent depot-access checks
 *
 * Revision 1.11  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.10  2007/01/05 08:54:25  devel
 * added too many tries and must lock owner data error messages
 *
 * Revision 1.9  2006/10/06 17:26:58  devel
 * changed some charlogs to dlogs
 *
 * Revision 1.8  2006/09/28 11:58:38  devel
 * added random dungeon to questlog
 *
 * Revision 1.7  2006/09/22 12:12:06  devel
 * added iplog and ipban logic to logins
 *
 * Revision 1.6  2006/09/22 09:55:46  devel
 * added ip to parameters of login
 *
 * Revision 1.5  2006/09/14 09:55:22  devel
 * added questlog
 *
 * Revision 1.4  2005/12/07 13:35:00  ssim
 * added military EXP transmission
 *
 * Revision 1.3  2005/12/01 16:31:09  ssim
 * added clan message
 *
 * Revision 1.2  2005/11/21 12:07:42  ssim
 * added logic for player-used demon sprite
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>
#include <netinet/in.h>

#include "server.h"
#include "do.h"
#include "log.h"
#include "io.h"
#include "client.h"
#include "map.h"
#include "database.h"
#include "create.h"
#include "see.h"
#include "notify.h"
#define NEED_PLAYER_STRUCT
#define NEED_PLAYER_DRIVER
#include "player.h"
#undef NEED_PLAYER_STRUCT
#undef NEED_PLAYER_DRIVER

#define NEED_FAST_LOS
#include "los.h"
#undef NEED_FAST_LOS

#include "effect.h"
#include "talk.h"
#include "drvlib.h"
#include "direction.h"
#include "do.h"
#include "drdata.h"
#include "act.h"
#include "command.h"
#include "container.h"
#include "date.h"
#include "skill.h"
#include "store.h"
#include "libload.h"
#include "death.h"
#include "tool.h"
#include "statistics.h"
#include "sector.h"
#include "player_driver.h"
#include "depot.h"
#include "clan.h"
#include "mail.h"
#include "motd.h"
#include "tell.h"
#include "consistency.h"
#include "drdata.h"
#include "lostcon.h"
#include "military.h"
#include "questlog.h"
#include "shrine.h"

#define MAX_IDLE	(TICKS*30)

// remove char from game without changing target area server or coordinates
void kick_char(int cn)
{
        /*if (ch[cn].player) { ????????????
		elog("kick_char has player: %s (%d), player %d",ch[cn].name,cn,ch[cn].player);

		exit_player(ch[cn].player);
		ch[cn].player=0;
	}*/

	if (ch[cn].x) remove_char(cn);

	if (!save_char(cn,ch[cn].tmpa)) {
		elog("could not save char %s %d!!",ch[cn].name,ch[cn].ID);
	}

	dlog(cn,0,"Character left server");
	//charlog(cn,"Character left server");
	
	destroy_char(cn);
}

// remove char from game and make him re-enter on current rest-area
void exit_char(int cn)
{
	ch[cn].tmpa=ch[cn].resta;
	ch[cn].tmpx=ch[cn].restx;
	ch[cn].tmpy=ch[cn].resty;

        kick_char(cn);
}

void exit_char_player(int cn)
{
	int nr;

	exit_char(cn);
	if ((nr=ch[cn].player) && player[nr]) player_client_exit(nr,"internal server error #32");
}

void kick_player(int nr,char *reason)
{
	int cn;

	if (player[nr]->state==ST_NORMAL) {		
		cn=player[nr]->cn;
		//elog("kick_player: %d",nr);
                if (cn) {
			if (ch[cn].player!=nr) {
				elog("character-player link wrong in kick_player(): %s %d %d",ch[cn].name,nr,ch[cn].player);
			}
			ch[cn].player=0;
			ch[cn].driver=CDR_LOSTCON;
			
			// reset lostcon driver values (bad style?)
			char_driver(ch[cn].driver,CDT_DEAD,cn,0,0);

			dlog(cn,0,"Player left server");
			//charlog(cn,"Player lost connection");
		}
	}

        if (reason) player_client_exit(nr,reason);
	else exit_player(nr);
}

static void check_idle(int nr)
{
	if (ticker>player[nr]->lastcmd+MAX_IDLE) {
		xlog("check_idle(): going to kick player %d for being idle too long.",nr);
                exit_player(nr);
	}
}

static void check_ingame_idle(int nr)
{
	int cn;

	cn=player[nr]->cn;

	if (ch[cn].player!=nr) {
		elog("character-player link wrong for player %d, character %s (%d) (%d)",nr,ch[cn].name,cn,ch[cn].player);
		player_client_exit(nr,"idle too long");
		player[nr]->lastcmd=ticker;
	} else if (ticker>player[nr]->lastcmd+TICKS*60*5) {
		xlog("check_ingame_idle(): going to kick player %d for being idle too long.",nr);
                exit_char(cn);
                player_client_exit(nr,"idle too long");
		player[nr]->lastcmd=ticker;
	}
}

static void remove_input(int nr,int len)
{
	if (len>player[nr]->in_len) {
		elog("remove_input(): trying to remove too much (%d from %d).",len,player[nr]->in_len);
		player[nr]->in_len=0;
		return;
	}
        player[nr]->in_len-=len;
	if (player[nr]->in_len) memmove(player[nr]->inbuf,player[nr]->inbuf+len,player[nr]->in_len);
}

void player_to_server(int nr,unsigned int server,int port)
{
	char buf[16];

	buf[0]=SV_SERVER;
	*(unsigned int*)(buf+1)=server;
	*(short*)(buf+5)=port;
	psend(nr,buf,7);

	if (!player[nr]) return;

	player[nr]->state=ST_EXIT;
	player[nr]->lastcmd=ticker;
}

void player_client_exit(int nr,char *reason)
{
	int len;
	char buf[256];

	len=min(200,strlen(reason));

	buf[0]=SV_EXIT;
	buf[1]=len;
	memcpy(buf+2,reason,len);
	psend(nr,buf,len+2);

	if (!player[nr]) return;

	player[nr]->state=ST_EXIT;
}

void decrypt(char *name,char *password)
{
	int i;
	static char secret[4][MAXPASSWORD]={
		"\000cgf\000de8etzdf\000dx",
		"jrfa\000v7d\000drt\000edm",
		"t6zh\000dlr\000fu4dms\000",
		"jkdm\000u7z5g\000j77\000g"
		};

	for (i=0; i<MAXPASSWORD; i++) {
		password[i]=password[i]^secret[name[1]%4][i]^name[i%3];
	}
}

static void read_login(int nr)
{
	int cn,ret,vendor,mirror,area,ID;
	char password[MAXPASSWORD],name[sizeof(ch[0].name)],buf[16];
	unsigned long long prof;
	unsigned int hisip,ourip,unique,olduni;

	if (player[nr]->in_len<sizeof(ch[0].name)+MAXPASSWORD+4+12) return;

	vendor=*(unsigned int*)(player[nr]->inbuf+sizeof(ch[0].name)+MAXPASSWORD);
	//if (version<CVERSION) { player_client_exit(nr,"client needs update"); return; }

	memcpy(name,player[nr]->inbuf,sizeof(ch[0].name)); name[sizeof(ch[0].name)-1]=0;
        memcpy(password,player[nr]->inbuf+sizeof(ch[0].name),MAXPASSWORD);
        decrypt(name,password); password[MAXPASSWORD-1]=0;
	hisip=*(unsigned int*)(player[nr]->inbuf+sizeof(ch[0].name)+MAXPASSWORD+4);
	ourip=*(unsigned int*)(player[nr]->inbuf+sizeof(ch[0].name)+MAXPASSWORD+8);
	olduni=unique=*(unsigned int*)(player[nr]->inbuf+sizeof(ch[0].name)+MAXPASSWORD+12);

        if (MAXCHARS-used_chars<16) {
                player_client_exit(nr,"Too many players in this area. Please try again later.");
		elog("server too full (chars: %d / %d)",used_chars,MAXCHARS);
		return;
	}

	if (MAXITEM-used_items<512) {
		player_client_exit(nr,"Too many players in this area. Please try again later.");
		elog("server too full (items: %d / %d)",used_chars,MAXCHARS);
		return;
	}

        prof=prof_start(29); ret=find_login(name,password,&area,&cn,&mirror,&ID,vendor,&unique,htonl(player[nr]->addr));  prof_stop(29,prof);

        if (ret==0) return;	// no answer yet

	if (ret>0 && cn>0 && !ch[cn].flags) { 	// the character we got was kicked in the meantime, try again...
		xlog("avoided race in read_login() - shouldnt happen anymore!");
		return;
	}

	//xlog("find_login(%s,%s)=%d (cn=%d,ID=%d)",name,password,ret,cn,ID);
	
	remove_input(nr,20);

	if (ret==-1) { player_client_exit(nr,"Internal error. Please try again. If several retries fail email game@astonia.com."); return; }
	if (ret==-2) { player_client_exit(nr,"You have been banned. Please email game@astonia.com for details."); return; }
        if (ret==-3) { player_client_exit(nr,"Username or password wrong."); return; }
	if (ret==-4) { player_client_exit(nr,"Duplicate login. Please make sure no other character from your account is active."); return; }
	if (ret==-5) { player_client_exit(nr,"Your account has not been paid."); return; }
	if (ret==-6) { player_client_exit(nr,"The server is being shut down. Please try again in a few minutes."); return; }
	if (ret==-7) { player_client_exit(nr,"Your IP address is banned. Please email game@astonia.com with your account ID and ask for an exception to be made."); return; }
	if (ret==-8) { player_client_exit(nr,"Please log onto your account management on www.astonia.com and update the account ownership information. Scroll down to 'Address Information' and choose 'Edit'."); return; }
	if (ret==-9) { player_client_exit(nr,"Too many tries with bad passwords. Please come back later."); return; }

	//xlog("nr=%d, cn=%d",nr,cn);

	if (area) {
                int server,port;

		if (area!=-1 && get_area(area,mirror,&server,&port)) {
			//xlog("player to server %d",area);
			player_to_server(nr,server,port);
			player[nr]->state=ST_EXIT;
			return;
		} else {
			rescue_char(ID);
                        //xlog("rescued char");
                        player_client_exit(nr,"Target area server is down. Your character is being transfered to a different area. Please try again.");
			return;
		}
	}

	player[nr]->state=ST_NORMAL;

        // destroy old connection if any
	//if (ch[cn].player && ch[cn].player!=nr) exit_player(ch[cn].player);

	if (ch[cn].player) {
		elog("load_char: player is set for %s??",ch[cn].name);
		if (ch[cn].player!=nr) exit_player(ch[cn].player);
	}
		
	ch[cn].player=nr;
	player[nr]->cn=cn;
	player[nr]->login_time=realtime;
	player[nr]->ticker=ticker;

	//xlog("login: character %s (%d,%llu) got player %d",ch[cn].name,cn,ch[cn].flags,nr);

	ch[cn].flags|=CF_UPDATE|CF_ITEMS|CF_PROF;
	ch[cn].driver=0;

        dlog(cn,0,"Character entered server: unique=%08u, his ip=%d.%d.%d.%d, our ip=%d.%d.%d.%d",
	     unique,
	     (hisip>>0)&255,
	     (hisip>>8)&255,
	     (hisip>>16)&255,
	     (hisip>>24)&255,
	     (ourip>>0)&255,
	     (ourip>>8)&255,
	     (ourip>>16)&255,
	     (ourip>>24)&255);
        //charlog(cn,"Character entered server: x=%d, y=%d, tmpx=%d, tmpy=%d, tmpa=%d, restx=%d, resty=%d, resta=%d, player=%d",ch[cn].x,ch[cn].y,ch[cn].tmpx,ch[cn].tmpy,ch[cn].tmpa,ch[cn].restx,ch[cn].resty,ch[cn].resta,ch[cn].player);
	
        log_items(cn);
	ch[cn].login_time=realtime;
	
	buf[0]=SV_LOGINDONE;
	psend(nr,buf,1);

	buf[0]=SV_TICKER;
	*(unsigned int*)(buf+1)=ticker-1;
	psend(nr,buf,5);

	buf[0]=SV_MIRROR;
	*(unsigned int*)(buf+1)=ch[cn].mirror;
	psend(nr,buf,5);

        if (olduni!=unique) {
		buf[0]=SV_UNIQUE;
		*(unsigned int*)(buf+1)=unique;
		psend(nr,buf,5);
	}

	if (!(ch[cn].flags&CF_AREACHANGE)) {
		show_motd(nr);
		show_clan_message(cn);
        } else ch[cn].flags&=~CF_AREACHANGE;
	
	if (areaID==21) log_char(cn,LOG_SYSTEM,0,"°c3You have entered the test area. You cannot die here, but you can't earn experience either.");

	buggy_items(cn);
	questlog_init(cn);
}

static void cl_nop(int nr,char *buf)
{
	;	// nop...
}

static void cl_stop(int nr)
{
	int cn;

	cn=player[nr]->cn;

	ch[cn].con_in=0;
	ch[cn].merchant=0;

	player_driver_stop(nr,0);
	//player[nr]->action=PAC_IDLE;
}

static void cl_move(int nr,char *buf)
{
        player_driver_move(nr,*(unsigned short*)(buf),*(unsigned short*)(buf+2));
	
	/*player[nr]->action=PAC_MOVE;
	player[nr]->act1=*(unsigned short*)(buf);
	player[nr]->act2=*(unsigned short*)(buf+2);*/
}

static void cl_swap(int nr,char *buf)
{
	int cn,pos;

	pos=*(unsigned char*)(buf+0);

	cn=player[nr]->cn;

	swap(cn,pos);
}

static void cl_take(int nr,char *buf)
{
	int x,y,m,in;

        x=*(unsigned short*)(buf);
	y=*(unsigned short*)(buf+2);

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return;

	m=x+y*MAXMAP;

	in=map[m].it;
	
        if (!in) return;

        player_driver_take(nr,in);

	/*player[nr]->action=PAC_TAKE;
	player[nr]->act1=in;*/
}

static void cl_look_map(int nr,char *buf)
{
	int x,y;

        x=*(unsigned short*)(buf);
	y=*(unsigned short*)(buf+2);

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return;

        player[nr]->action=PAC_LOOK_MAP;
	player[nr]->act1=*(unsigned short*)(buf);
	player[nr]->act2=*(unsigned short*)(buf+2);
}

static void cl_look_item(int nr,char *buf)
{
	int x,y,m,in,cn;

        x=*(unsigned short*)(buf);
	y=*(unsigned short*)(buf+2);

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return;

	m=x+y*MAXMAP;

	if (!(in=map[m].it)) return;

	cn=player[nr]->cn;

	if (!char_see_item(cn,in)) return;

	look_item(cn,it+in);	
}

static void cl_look_inv(int nr,char *buf)
{
	int cn,pos;

        pos=*(unsigned char*)(buf);

	if (pos<0 || pos>=INVENTORYSIZE) return;

	cn=player[nr]->cn;

	look_inv(cn,pos);
}

static void cl_look_char(int nr,char *buf)
{
	int cn,co;

        co=*(unsigned short*)(buf);

        if (co<1 || co>=MAXCHARS) return;

	cn=player[nr]->cn;

	if (!char_see_char(cn,co)) return;

	look_char(cn,co);
}

static void cl_use(int nr,char *buf)
{
	int x,y,m,in;

        x=*(unsigned short*)(buf);
	y=*(unsigned short*)(buf+2);

	if (x<1 || x>=MAXMAP-1 || y<1 || y>=MAXMAP-1) return;

	m=x+y*MAXMAP;

	in=map[m].it;
	
	if (!in) return;	

	player_driver_use(nr,in);

	/*player[nr]->action=PAC_USE;
	player[nr]->act1=in;*/
}

static void cl_teleport(int nr,char *buf)
{
	int tel,mir;
	
	tel=buf[0];
	mir=buf[1];
	
	player_driver_teleport(nr,tel+mir*256);
}

static void cl_use_inv(int nr,char *buf)
{
	int pos,cn,in;

        pos=*(unsigned char*)(buf+0);
	if (pos<0 || pos>=INVENTORYSIZE || (pos>=12 && pos<=29)) return;

	cn=player[nr]->cn;

	in=ch[cn].item[pos];

	if (!in) return;	

	if (it[in].flags&IF_USE) use_item(cn,in);
	else equip_item(cn,in,pos);
}

static void cl_fastsell(int nr,char *buf)
{
	int pos,cn,in;

        pos=*(unsigned char*)(buf+0);
	if (pos<0 || pos>=INVENTORYSIZE || (pos>=12 && pos<=29)) return;

	cn=player[nr]->cn;

	if (!swap(cn,pos)) return;
	if (!(in=ch[cn].citem)) return;

	if (ch[cn].merchant) check_merchant(cn);
	if (ch[cn].con_in) check_container_item(cn);

	if (ch[cn].merchant) {
		if (it[in].flags&IF_QUEST) {
			log_char(cn,LOG_SYSTEM,0,"You cannot quick-sell quest items (hold down SHIFT and LEFT-CLICK on the merchant's windows to go ahead).");
			return;
		}
		player_store(cn,0,1,0);
	} else if ((in=ch[cn].con_in)) {
		if (it[in].flags&IF_DEPOT) player_depot(cn,0,1,1);
		else container(cn,0,1,0);
	}
}

static void cl_drop(int nr,char *buf)
{
	player_driver_drop(nr,*(unsigned short*)(buf),*(unsigned short*)(buf+2));
	
	/*player[nr]->action=PAC_DROP;
        player[nr]->act1=*(unsigned short*)(buf);
	player[nr]->act2=*(unsigned short*)(buf+2);*/
}

static void cl_kill(int nr,char *buf)
{
	int cn,co;

	cn=player[nr]->cn;
	
        co=*(unsigned short*)(buf);

	if (co<1 || co>=MAXCHARS) return;
	if (!char_see_char(cn,co)) return;

	player_driver_kill(nr,co);
		
	/* player[nr]->action=PAC_KILL;
	player[nr]->act1=co;
	player[nr]->act2=ch[co].serial; */
}

static void cl_give(int nr,char *buf)
{
	int cn,co;

	cn=player[nr]->cn;
	
        co=*(unsigned short*)(buf);

	if (co<1 || co>=MAXCHARS) return;
	if (!char_see_char(cn,co)) return;
	
	player_driver_give(nr,co);
	
	/*player[nr]->action=PAC_GIVE;
	player[nr]->act1=co;
	player[nr]->act2=ch[co].serial;*/
}

static void cl_speed(int nr,char *buf)
{
	int cn,mode;

	cn=player[nr]->cn;
	mode=*(unsigned char*)(buf+0);
	
	if (mode!=SM_NORMAL && mode!=SM_FAST && mode!=SM_STEALTH) return;

	if (mode==SM_FAST && ch[cn].endurance<POWERSCALE) return;	

	ch[cn].speed_mode=mode;
}

static void cl_fightmode(int nr,char *buf)
{
	return;
}

static void cl_mapspell(int nr,char *buf,int driver)
{
        int cn,co,x,y;

        x=*(unsigned short*)(buf);
	y=*(unsigned short*)(buf+2);

	if (!x) {
		cn=player[nr]->cn;
		co=y;

		if (co<1 || co>=MAXCHARS || !char_see_char(cn,co)) return;

		switch(driver) {
			case PAC_FIREBALL:	player_driver_charspell(nr,PAC_FIREBALL2,co); return;
			case PAC_BALL:		player_driver_charspell(nr,PAC_BALL2,co); return;
		}

		/* x=ch[co].x;
		y=ch[co].y; */
	}

	player_driver_mapspell(nr,driver,x,y);
	
	/*player[nr]->action=driver;
	player[nr]->act1=x;
	player[nr]->act2=y;*/
}

/* static void cl_fireball(int nr,char *buf)
{
        int cn,co,x,y;

        x=*(unsigned short*)(buf);
	y=*(unsigned short*)(buf+2);

	if (!x) {
		cn=player[nr]->cn;
		co=y;

		if (co<1 || co>=MAXCHARS || !char_see_char(cn,co)) return;

		player[nr]->action=PAC_FIREBALL2;
		player[nr]->act1=co;
		
	} else cl_mapspell(nr,buf,PAC_FIREBALL);
} */

static void cl_charspell(int nr,char *buf,int driver)
{
        int cn,co;

	cn=player[nr]->cn;
        co=*(unsigned short*)(buf);
	if (co<1 || co>=MAXCHARS || !char_see_char(cn,co)) return;

	player_driver_charspell(nr,driver,co);
	
	/* player[nr]->action=driver;
	player[nr]->act1=co;
	player[nr]->act2=0; */
}

static void cl_selfspell(int nr,char *buf,int driver)
{
	player_driver_selfspell(nr,driver);
        //player[nr]->action=driver;
}

static void cl_container(int nr,char *buf)
{
	int n,cn,in;

	n=*(unsigned char*)(buf+0);

	if (n<0 || n>=STORESIZE) return;

	cn=player[nr]->cn;

	if (ch[cn].merchant) check_merchant(cn);
	if (ch[cn].con_in) check_container_item(cn);

	if (ch[cn].merchant) player_store(cn,n,1,0);
	else if ((in=ch[cn].con_in)) {
		if (it[in].flags&IF_DEPOT) player_depot(cn,n,1,0);
		else container(cn,n,1,0);
	}
}

static void cl_container_fast(int nr,char *buf)
{
	int n,cn,in;

	n=*(unsigned char*)(buf+0);

	if (n<0 || n>=STORESIZE) return;

	cn=player[nr]->cn;

	if (ch[cn].merchant) check_merchant(cn);
	if (ch[cn].con_in) check_container_item(cn);

	if (ch[cn].merchant) player_store(cn,n,1,1);
	else if ((in=ch[cn].con_in)) {
		if (it[in].flags&IF_DEPOT) player_depot(cn,n,1,1);
		else container(cn,n,1,1);
	}
}

static void cl_look_container(int nr,char *buf)
{
	int n,cn,in;

	n=*(unsigned char*)(buf+0);

	if (n<0 || n>=STORESIZE) return;

	cn=player[nr]->cn;

	if (ch[cn].merchant) check_merchant(cn);
	if (ch[cn].con_in) check_container_item(cn);

        if (ch[cn].merchant) player_store(cn,n,0,0);
	else if ((in=ch[cn].con_in)) {
		if (it[in].flags&IF_DEPOT) player_depot(cn,n,0,0);
		else container(cn,n,0,0);
	}
}

static void cl_raise(int nr,char *buf)
{
	int n,cn,ret;

	n=*(unsigned short*)(buf);

	if (n<0 || n>V_MAX) return;

	cn=player[nr]->cn;

	ret=raise_value(cn,n);
}

static void cl_text(int nr,char *buf)
{
	int len;

	if (player[nr]->command[0]) return;

        len=*(unsigned char*)(buf+0);
	if (len<1) return;

	buf[len]=0;
	strcpy(player[nr]->command,buf+1);
}

static void check_command(int nr)
{
	if (!player[nr]) return;
	
	if ((ticker&7)==0) check_tells(player[nr]->cn);

	if (!player[nr]) return;

	if (!player[nr]->command[0]) return;
	
	if (command(player[nr]->cn,player[nr]->command)) {
		if (player[nr]) player[nr]->command[0]=0;
	}
}

static void cl_log(int nr,char *buf)
{
	int len,cn;

	cn=player[nr]->cn;
        len=*(unsigned char*)(buf+0);

	buf[len]=0;
	charlog(cn,buf+1);
}

static void cl_take_gold(int nr,char *buf)
{
	int val,cn,in;

	cn=player[nr]->cn;
        val=*(unsigned int*)(buf);

        if ((in=ch[cn].citem)) {	// already holding something?
		if ((it[in].flags&IF_MONEY)) {	// money?
			dlog(cn,in,"dropped into goldbag");
			ch[cn].gold+=destroy_money_item(in);
			ch[cn].citem=0;
			ch[cn].flags|=CF_ITEMS;
		} else return;	// not money, leave
	}
	if (val<1 || val>ch[cn].gold) return;

	in=create_money_item(val);
	if (!in) return;
	
	ch[cn].gold-=val;

	dlog(cn,in,"took from goldbag");
	ch[cn].citem=in;
	it[in].carried=cn;
	
	ch[cn].flags|=CF_ITEMS;
}

static void cl_drop_gold(int nr)
{
	int cn,in;

	cn=player[nr]->cn;

	if ((in=ch[cn].citem) && (it[in].flags&IF_MONEY)) {
		dlog(cn,in,"dropped into goldbag");
		ch[cn].gold+=destroy_money_item(in);
		ch[cn].citem=0;
		
		ch[cn].flags|=CF_ITEMS;
	}
}

static void cl_ticker(int nr,char *buf)
{
	int cn,diff,val;
	struct lostcon_ppd *ppd;

	cn=player[nr]->cn;
        player[nr]->ticker=*(unsigned int*)(buf);
	diff=ticker-player[nr]->ticker;

	if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd))) && ppd->maxlag) {
		val=ppd->maxlag*TICKS*0.75;
	} else val=TICKS*7;

        if (diff<val && ch[cn].driver!=0 && !(ch[cn].flags&CF_LAG)) {
		log_char(cn,LOG_SYSTEM,0,"Lag within reasonable bounds again. Auto-Control disabled.");
                ch[cn].driver=0;
		dlog(cn,0,"lostcon disabled");
	}
}

static void check_lag(int nr)
{
	int cn,diff,val;
	struct lostcon_ppd *ppd;

	cn=player[nr]->cn;
	diff=ticker-player[nr]->ticker;

        if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd))) && ppd->maxlag) {
		val=ppd->maxlag*TICKS;
	} else val=TICKS*10;

	if ((diff>val || (ch[cn].flags&CF_LAG)) && ch[cn].driver!=CDR_LOSTCON) {
		log_char(cn,LOG_SYSTEM,0,"Lag exceeds system limits. Auto-Control taking over.%s",
			 (ch[cn].flags&CF_LAG) ? "" : " Choosing a different connection in the lower left corner of the startup screen might help. You might also want to try to change the setting for /MAXLAG.");
		ch[cn].driver=CDR_LOSTCON;
		
		// reset lostcon driver values (bad style?)
		char_driver(ch[cn].driver,CDT_DEAD,cn,0,0);
		
		dlog(cn,0,"lostcon enabled");
	}
}

static void cl_junk_item(int nr)
{
	int cn,in;

	cn=player[nr]->cn;

	if ((in=ch[cn].citem) && !(it[in].flags&IF_NOJUNK)) {
		dlog(cn,in,"junked %s (%s)",it[in].name,it[in].description);
		ch[cn].citem=0;
                destroy_item(in);

		ch[cn].flags|=CF_ITEMS;
	}
}

static void cl_clientinfo(int nr,struct client_info *ci)
{
	/*int n;

	xlog("player %d: skip=%d, idle=%d",nr,ci->skip,ci->idle);
	xlog("player %d: sysmem=%dK/%dK",nr,ci->systemfree>>10,ci->systemtotal>>10);
	xlog("player %d: vidmem=%dK/%dK",nr,ci->vidmemfree>>10,ci->vidmemtotal>>10);

	for (n=0; n<CL_MAX_SURFACE; n++) {
		if (!ci->surface[n].xres) break;
		xlog("player %d: surface %d: type=%d, %dx%d",nr,n,ci->surface[n].type,ci->surface[n].xres,ci->surface[n].yres);		
	}*/
}

static void cl_ping(int nr,char *ibuf)
{
	char buf[16];

	buf[0]=SV_PING;
	*(unsigned int*)(buf+1)=*(unsigned int*)(ibuf);
	psend(nr,buf,5);
}

void sendquestlog(int cn,int nr)
{
	char buf[512];
	struct quest *quest;
	struct shrine_ppd *shrine;
	int size;

	if (!cn) return;
	if (!nr) return;
	if (!player[nr]) return;

	size=sizeof(struct quest)*MAXQUEST;

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,size))) return;
	if (!(shrine=set_data(cn,DRD_RANDOMSHRINE_PPD,size))) return;

        buf[0]=SV_QUESTLOG;
        memcpy(buf+1,quest,size);
	memcpy(buf+1+size,shrine,sizeof(struct shrine_ppd));
	psend(nr,buf,size+1+sizeof(struct shrine_ppd));
}

static void cl_getquestlog(int nr)
{
	int cn;

	if (!(cn=player[nr]->cn)) return;
	sendquestlog(cn,nr);	
}

static void cl_reopen_quest(int nr,char *buf)
{
	int n,cn;

	n=*(unsigned char*)(buf);
	if (!(cn=player[nr]->cn)) return;
	questlog_reopen(cn,n);
}

static void read_input(int nr)
{
	int need;

	if (player[nr]->in_len<1) return;	// minimum command size is one byte. obvious, isn't it?

	switch(player[nr]->inbuf[0]) {
		case CL_NOP:		need=1; break;
		case CL_MOVE:		need=5; break;
		case CL_SWAP:		need=2; break;
		case CL_TAKE:		need=5; break;
		case CL_DROP:		need=5; break;
		case CL_KILL:		need=3; break;
		
		case CL_TEXT:		if (player[nr]->in_len<2) need=2;
					else need=player[nr]->inbuf[1]+2;
					break;
		case CL_LOG:		if (player[nr]->in_len<2) need=2;
					else need=player[nr]->inbuf[1]+2;
					break;

		case CL_USE:		need=5; break;

		case CL_BLESS:		need=3; break;
		case CL_HEAL:		need=3; break;

		case CL_FIREBALL:	need=5; break;
		case CL_BALL:		need=5; break;

		case CL_MAGICSHIELD:	need=1; break;
		case CL_FLASH:		need=1; break;
		case CL_WARCRY: 	need=1; break;
		case CL_FREEZE:		need=1; break;
		case CL_PULSE: 		need=1; break;

		case CL_CONTAINER:	need=2; break;
		case CL_CONTAINER_FAST:	need=2; break;
		case CL_LOOK_CONTAINER:	need=2; break;
		case CL_RAISE:		need=3; break;
		case CL_USE_INV:	need=2; break;
		case CL_FASTSELL:	need=2; break;
		case CL_LOOK_MAP:	need=5; break;
		case CL_LOOK_INV:	need=2; break;
		case CL_LOOK_ITEM:	need=5; break;
		case CL_LOOK_CHAR:	need=3; break;
		case CL_GIVE:		need=3; break;
		case CL_SPEED:		need=2; break;
		case CL_FIGHTMODE:	need=2; break;
		case CL_STOP:		need=1; break;
		case CL_TAKE_GOLD:	need=5; break;
		case CL_DROP_GOLD:	need=1; break;
		case CL_JUNK_ITEM:	need=1; break;
		case CL_CLIENTINFO:	need=1+sizeof(struct client_info); break;
		case CL_TICKER:		need=5; break;
		case CL_TELEPORT:	need=3; break;
		case CL_PING:		need=5; break;
		case CL_GETQUESTLOG:	need=1; break;
		case CL_REOPENQUEST:	need=2; break;

		default:		player[nr]->in_len=0;	// got illegal command. trash all input and bail out
					return;
	}

	if (player[nr]->in_len<need) return;	// we don't have the whole command yet

	// update idle timer
	if (player[nr]->inbuf[0]!=CL_TICKER) {
		player[nr]->lastcmd=ticker;		
	}

        switch(player[nr]->inbuf[0]) {
		case CL_NOP:		cl_nop(nr,player[nr]->inbuf+1); break;
		case CL_MOVE:		cl_move(nr,player[nr]->inbuf+1); break;
		case CL_SWAP:		cl_swap(nr,player[nr]->inbuf+1); break;
		case CL_TAKE:		cl_take(nr,player[nr]->inbuf+1); break;
		case CL_DROP:		cl_drop(nr,player[nr]->inbuf+1); break;
		case CL_KILL:		cl_kill(nr,player[nr]->inbuf+1); break;
		case CL_TEXT:		cl_text(nr,player[nr]->inbuf+1); break;
		case CL_LOG:		cl_log(nr,player[nr]->inbuf+1); break;
		case CL_USE:		cl_use(nr,player[nr]->inbuf+1); break;
		
		case CL_BLESS:		cl_charspell(nr,player[nr]->inbuf+1,PAC_BLESS); break;
		case CL_HEAL:		cl_charspell(nr,player[nr]->inbuf+1,PAC_HEAL); break;

		case CL_FIREBALL:  	cl_mapspell(nr,player[nr]->inbuf+1,PAC_FIREBALL); break;
		case CL_BALL:		cl_mapspell(nr,player[nr]->inbuf+1,PAC_BALL); break;

		case CL_MAGICSHIELD:	cl_selfspell(nr,player[nr]->inbuf+1,PAC_MAGICSHIELD); break;
		case CL_FLASH:		cl_selfspell(nr,player[nr]->inbuf+1,PAC_FLASH); break;
		case CL_WARCRY:		cl_selfspell(nr,player[nr]->inbuf+1,PAC_WARCRY); break;
		case CL_PULSE:		cl_selfspell(nr,player[nr]->inbuf+1,PAC_PULSE); break;
		case CL_FREEZE:		cl_selfspell(nr,player[nr]->inbuf+1,PAC_FREEZE); break;

		case CL_CONTAINER:	cl_container(nr,player[nr]->inbuf+1); break;
		case CL_CONTAINER_FAST:	cl_container_fast(nr,player[nr]->inbuf+1); break;
		case CL_LOOK_CONTAINER: cl_look_container(nr,player[nr]->inbuf+1); break;
		case CL_RAISE:		cl_raise(nr,player[nr]->inbuf+1); break;
		case CL_USE_INV:	cl_use_inv(nr,player[nr]->inbuf+1); break;
		case CL_FASTSELL:	cl_fastsell(nr,player[nr]->inbuf+1); break;
		case CL_LOOK_MAP:	cl_look_map(nr,player[nr]->inbuf+1); break;
		case CL_LOOK_INV:	cl_look_inv(nr,player[nr]->inbuf+1); break;
		case CL_LOOK_ITEM:	cl_look_item(nr,player[nr]->inbuf+1); break;
		case CL_LOOK_CHAR:	cl_look_char(nr,player[nr]->inbuf+1); break;
		case CL_GIVE:		cl_give(nr,player[nr]->inbuf+1); break;
		case CL_SPEED:		cl_speed(nr,player[nr]->inbuf+1); break;
		case CL_FIGHTMODE:	cl_fightmode(nr,player[nr]->inbuf+1); break;
		case CL_STOP:		cl_stop(nr); break;
		case CL_TAKE_GOLD:	cl_take_gold(nr,player[nr]->inbuf+1); break;
		case CL_DROP_GOLD:	cl_drop_gold(nr); break;
		case CL_JUNK_ITEM:	cl_junk_item(nr); break;
		case CL_CLIENTINFO:	cl_clientinfo(nr,(void*)(player[nr]->inbuf+1)); break;
		case CL_TICKER:		cl_ticker(nr,player[nr]->inbuf+1); break;
		case CL_TELEPORT:	cl_teleport(nr,player[nr]->inbuf+1); break;
		case CL_PING:		cl_ping(nr,player[nr]->inbuf+1); break;
		case CL_GETQUESTLOG:	cl_getquestlog(nr); break;
		case CL_REOPENQUEST:	cl_reopen_quest(nr,player[nr]->inbuf+1); break;

		default:	elog("read_input: you need to add a new command to BOTH tables, dude!"); break;

	}
	
        remove_input(nr,need);	
}

#define DO_ALL		1
#define DO_UP		2
#define DO_DOWN		4
#define DO_LEFT		8
#define DO_RIGHT	16

void player_reset_map_cache(int nr)
{
        if (nr && player[nr]) player[nr]->x=player[nr]->y=0;
}

static int player_check_scroll(int nr,int x,int y)
{
	char buf[16];

        if (player[nr]->x!=x || player[nr]->y!=y) {
                if (player[nr]->x==x-1 && player[nr]->y==y) {
                        buf[0]=SV_SCROLL_RIGHT;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap,player[nr]->cmap+1,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-1));			
                } else if (player[nr]->x==x+1 && player[nr]->y==y) {
                        buf[0]=SV_SCROLL_LEFT;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap+1,player[nr]->cmap,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-1));
                } else if (player[nr]->x==x && player[nr]->y==y-1) {
                        buf[0]=SV_SCROLL_DOWN;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap,player[nr]->cmap+(DIST*2+1),sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)));
                } else if (player[nr]->x==x && player[nr]->y==y+1) {
                        buf[0]=SV_SCROLL_UP;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap+(DIST*2+1),player[nr]->cmap,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)));
                } else if (player[nr]->x==x+1 && player[nr]->y==y+1) {
                        buf[0]=SV_SCROLL_LEFTUP;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap+(DIST*2+1)+1,player[nr]->cmap,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)-1));
                } else if (player[nr]->x==x+1 && player[nr]->y==y-1) {
                        buf[0]=SV_SCROLL_LEFTDOWN;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap,player[nr]->cmap+(DIST*2+1)-1,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)+1));
                } else if (player[nr]->x==x-1 && player[nr]->y==y+1) {
                        buf[0]=SV_SCROLL_RIGHTUP;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap+(DIST*2+1)-1,player[nr]->cmap,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)+1));
                } else if (player[nr]->x==x-1 && player[nr]->y==y-1) {
                        buf[0]=SV_SCROLL_RIGHTDOWN;
                        psend(nr,buf,1);

                        memmove(player[nr]->cmap,player[nr]->cmap+(DIST*2+1)+1,sizeof(struct cmap)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)-1));
                }

		if (!player[nr]) return 0;

		player[nr]->x=x;
                player[nr]->y=y;

		buf[0]=SV_ORIGIN;
		*(unsigned short*)(buf+1)=x;
		*(unsigned short*)(buf+3)=y;
		psend(nr,buf,5);

		return 1;
	} else return 0;
}

static inline int check_dlightm(int m)
{
        return (dlight*map[m].dlight)/256;
}

static inline int health_per(int cn)
{
	int val,from;
	
	from=max(1,ch[cn].value[0][V_HP]);
	val=ch[cn].hp/(POWERSCALE/100);

	return max(0,val/from);
}

static inline int mana_per(int cn)
{
	int val,from;
	
	from=max(1,ch[cn].value[0][V_MANA]);
	val=ch[cn].mana/(POWERSCALE/100);

	return max(0,val/from);
}

static inline int shield_per(int cn)
{
	int val,from;
	
	//if (ch[cn].value[0][V_MAGICSHIELD]) from=max(1,ch[cn].value[0][V_MAGICSHIELD]);
	//else from=max(1,ch[cn].value[0][V_RAGE]);
	from=max(1,get_lifeshield_max(cn));
	
	val=ch[cn].lifeshield/(POWERSCALE/100);

	return max(0,val/from);
}

static inline int player_knows_name(int nr,int cn)
{
	int pos,val;

	pos=cn>>3;
	val=1<<(cn&7);

        return player[nr]->nameflag[pos]&val;
}

void set_player_knows_name(int nr,int cn,int flag)
{
	int pos,val;

	if (!player) return;
	if (!player[nr]) return;

	pos=cn>>3;
	val=1<<(cn&7);

        if (flag) player[nr]->nameflag[pos]|=val;
	else player[nr]->nameflag[pos]&=~val;	
}

void reset_name(int cn)
{
	int n;

	for (n=1; n<MAXPLAYER; n++) {
		set_player_knows_name(n,cn,0);
	}
}

// do we need to transmit a pointer on the map to the effect?
// needed for effects which do not have own coordinates.
static int trans_ceffect(int fn)
{
        switch(ef[fn].type) {
		case EF_MAGICSHIELD:	return 0;
		case EF_BALL:		return 0;
		case EF_STRIKE:		return 0;
		case EF_FIREBALL:	return 0;
		case EF_FLASH:		return 0;
		case EF_EXPLODE:	return 1;
		case EF_WARCRY:		return 0;
		case EF_BLESS:		return 0;
		case EF_HEAL:		return 0;
		case EF_FREEZE:		return 0;
		case EF_BURN:		return 0;
		case EF_MIST:		return 1;
		case EF_POTION:		return 0;
		case EF_EARTHRAIN:	return 1;
		case EF_EARTHMUD:	return 1;
		case EF_EDEMONBALL: 	return 0;
		case EF_CURSE:		return 0;
		case EF_CAP:		return 0;
		case EF_LAG:		return 0;
		case EF_PULSE:		return 1;
		case EF_PULSEBACK:	return 0;
		case EF_FIRERING:	return 0;
		case EF_BUBBLE:		return 1;
	}
	return 0;
}

static int is_char_ceffect(int type)
{
        switch(type) {
		case 1:		return 1;
		case 2:		return 0;
		case 3:		return 1;
		case 4:		return 0;
		case 5:		return 1;
		case 6:		return 0;
		case 7:		return 0;
		case 8:		return 1;
		case 9:		return 1;
		case 10:	return 1;
		case 11:	return 1;
		case 12:	return 1;
		case 13:	return 0;
		case 14:	return 1;
		case 15:	return 0;
		case 16:	return 0;
		case 17:	return 0;
		case 18:	return 1;
		case 19:	return 1;
		case 20:	return 1;
		case 21:	return 0;
		case 22:	return 1;
		case 23:	return 1;
		case 24:	return 0;

	}
	return 0;
}

static int fill_ceffect(int fn,union ceffect *cef)
{
	bzero(cef,sizeof(union ceffect));
	
	switch(ef[fn].type) {
		case EF_MAGICSHIELD:
			cef->shield.nr=fn;
			cef->shield.type=1;
			cef->shield.cn=ef[fn].efcn;
			cef->shield.start=ef[fn].start;
                        return sizeof(struct cef_shield);
		case EF_BALL:
			cef->ball.nr=fn;
			cef->ball.type=2;
			cef->ball.start=ef[fn].start;
                        cef->ball.frx=ef[fn].frx;
			cef->ball.fry=ef[fn].fry;
			cef->ball.tox=ef[fn].tox;
			cef->ball.toy=ef[fn].toy;
                        return sizeof(struct cef_ball);
		case EF_STRIKE:
			cef->strike.nr=fn;
			cef->strike.type=3;
			cef->strike.cn=ef[fn].efcn;
			cef->strike.x=ef[fn].x;
			cef->strike.y=ef[fn].y;
                        return sizeof(struct cef_strike);
		case EF_FIREBALL:
			cef->fireball.nr=fn;
			cef->fireball.type=4;
			cef->fireball.start=ef[fn].start;
			cef->fireball.frx=ef[fn].frx;
			cef->fireball.fry=ef[fn].fry;
			cef->fireball.tox=ef[fn].tox;
			cef->fireball.toy=ef[fn].toy;
                        return sizeof(struct cef_fireball);
		case EF_FLASH:
			cef->flash.nr=fn;
			cef->flash.type=5;			
			cef->flash.cn=ef[fn].cn;
                        return sizeof(struct cef_flash);
		case EF_EXPLODE:
			cef->explode.nr=fn;
			cef->explode.type=7;
			cef->explode.start=ef[fn].start;
			cef->explode.base=ef[fn].base_sprite;
			return sizeof(struct cef_explode);
		case EF_WARCRY:
			cef->warcry.nr=fn;
			cef->warcry.type=8;
			cef->warcry.cn=ef[fn].efcn;
			cef->warcry.stop=ef[fn].stop;
                        return sizeof(struct cef_warcry);
		case EF_BLESS:
			cef->bless.nr=fn;
			cef->bless.type=9;
			cef->bless.cn=ef[fn].efcn;
			cef->bless.start=ef[fn].start;
			cef->bless.stop=ef[fn].stop;
			cef->bless.strength=ef[fn].strength;
                        return sizeof(struct cef_bless);
		case EF_HEAL:
			cef->heal.nr=fn;
			cef->heal.type=10;
			cef->heal.cn=ef[fn].efcn;
			cef->heal.start=ef[fn].start;
                        return sizeof(struct cef_heal);
		case EF_FREEZE:
			cef->freeze.nr=fn;
			cef->freeze.type=11;
			cef->freeze.cn=ef[fn].efcn;
			cef->freeze.start=ef[fn].start;
			cef->freeze.stop=ef[fn].stop;
                        return sizeof(struct cef_freeze);
		case EF_BURN:
			cef->burn.nr=fn;
			cef->burn.type=12;
			cef->burn.cn=ef[fn].efcn;
			cef->burn.stop=ef[fn].stop;
                        return sizeof(struct cef_burn);
		case EF_MIST:
			cef->explode.nr=fn;
			cef->explode.type=13;
			cef->explode.start=ef[fn].start;
			return sizeof(struct cef_mist);
		case EF_POTION:
			cef->potion.nr=fn;
			cef->potion.type=14;
			cef->potion.cn=ef[fn].efcn;
			cef->potion.start=ef[fn].start;
			cef->potion.stop=ef[fn].stop;
			cef->potion.strength=ef[fn].strength;			
                        return sizeof(struct cef_potion);
		case EF_EARTHRAIN:
			cef->earthrain.nr=fn;
			cef->earthrain.type=15;
			cef->earthrain.strength=ef[fn].strength;
                        return sizeof(struct cef_earthrain);
		case EF_EARTHMUD:
			cef->earthmud.nr=fn;
			cef->earthmud.type=16;
                        return sizeof(struct cef_earthmud);
		case EF_EDEMONBALL:
			cef->edemonball.nr=fn;
			cef->edemonball.type=17;
			cef->edemonball.start=ef[fn].start;
			cef->edemonball.base=ef[fn].base_sprite;
			cef->edemonball.frx=ef[fn].frx;
			cef->edemonball.fry=ef[fn].fry;
			cef->edemonball.tox=ef[fn].tox;
			cef->edemonball.toy=ef[fn].toy;
                        return sizeof(struct cef_edemonball);
                case EF_CURSE:
			cef->curse.nr=fn;
			cef->curse.type=18;
			cef->curse.cn=ef[fn].efcn;
			cef->curse.start=ef[fn].start;
			cef->curse.stop=ef[fn].stop;
			cef->curse.strength=ef[fn].strength;
                        return sizeof(struct cef_curse);
		case EF_CAP:
			cef->cap.nr=fn;
			cef->cap.type=19;
			cef->cap.cn=ef[fn].efcn;
                        return sizeof(struct cef_cap);
		case EF_LAG:
			cef->lag.nr=fn;
			cef->lag.type=20;
			cef->lag.cn=ef[fn].efcn;
                        return sizeof(struct cef_lag);
		case EF_PULSE:
			cef->pulse.nr=fn;
			cef->pulse.type=21;
			cef->pulse.start=ef[fn].start;
			return sizeof(struct cef_pulse);
		case EF_PULSEBACK:
			cef->pulseback.nr=fn;
			cef->pulseback.type=22;
			cef->pulseback.cn=ef[fn].efcn;
			cef->pulseback.x=ef[fn].x;
			cef->pulseback.y=ef[fn].y;
                        return sizeof(struct cef_pulseback);
		case EF_FIRERING:
			cef->firering.nr=fn;
			cef->firering.type=23;
			cef->firering.start=ef[fn].start;
			cef->firering.cn=ef[fn].efcn;
                        return sizeof(struct cef_firering);
		case EF_BUBBLE:
			cef->bubble.nr=fn;
			cef->bubble.type=24;
			cef->bubble.yoff=ef[fn].strength;
                        return sizeof(struct cef_bubble);
	}
	return 0;
}

static int trans_light(int light)
{
	if (light>52) light=0;
	else if (light>40) light=1;
	else if (light>32) light=2;
	else if (light>28) light=3;
	else if (light>24) light=4;
	else if (light>20) light=5;
	else if (light>16) light=6;
	else if (light>12) light=7;
	else if (light>10) light=8;
	else if (light>8) light=9;
	else if (light>6) light=10;
	else if (light>4) light=11;
	else if (light>2) light=12;
	else if (light>1) light=13;
	else light=14;
			
	return light;
}

#define MAXEF	64
static void player_map(int nr)
{
	int xoff,yoff,x,y,m,c,in,co,xs,xe,light=0,skipx,n,dark=0;
	int isprite,flags,cn;
        char buf01[256];
	char buf10[256];
	char buf11[256];
	char buf[256];

	int p01,p10,p11;

        int needef[MAXEF],usedef[MAXEF];
	int maxef=0,fn,f,size,len;
	union ceffect cef;

	struct cmap *cmap;
	int last=-1;

        int redo=0;

	unsigned long long uf=0;

//#define CCYCLES
#ifdef CCYCLES
	unsigned long long start,end,dur;
	static unsigned long long avg=0;
	unsigned long long rdtsc(void);

	start=rdtsc();
#endif

	cn=player[nr]->cn;

	// make sure los is current
        redo=update_los(cn,ch[cn].x,ch[cn].y,DIST);

	// check for scrolling
	redo|=player_check_scroll(nr,ch[cn].x,ch[cn].y);
	if (!player[nr]) return;

	if (dlight!=player[nr]->dlight) {
		player[nr]->dlight=dlight;
		redo=1;
	}

        xoff=ch[cn].x-DIST;
	yoff=ch[cn].y-DIST;

	cmap=player[nr]->cmap;

        for (y=0; y<=DIST*2; y++) {

		if (y<DIST) { xs=DIST-y; xe=DIST+y; }
		else { xs=y-DIST; xe=DIST*3-y; }

		for (x=xs; x<=xe; x++) {
                        c=x+y*(DIST*2+1);
		
			if (!redo) {
				skipx=skipx_sector(x+xoff,y+yoff);
				if (skipx) {
					x+=skipx-1;
					continue;
				}
			}

			p01=4; p10=4; p11=4;

			buf01[3]=0; buf10[3]=0; buf11[3]=0;

                        if (x+xoff<1 || x+xoff>=MAXMAP || y+yoff<1 || y+yoff>=MAXMAP) { flags=0; goto skip; }

                        m=x+xoff+(y+yoff)*MAXMAP;

			light=max(map[m].light,check_dlightm(m)); dark=0;
			if ((ch[cn].flags&CF_INFRAVISION) && light<4) { light=max(4,light); flags=CMF_INFRA; }
			else if (ch[cn].flags&CF_INFRARED) { light=max(32,light); flags=0; }
			else flags=0;

			if (light<1) {
				//if (map[m].ch==cn) light=1;	// make player visible to himself all the time
				if (abs(x-DIST)<2 && abs(y-DIST)<2) light=1;
				else if (ch[cn].prof[P_DARK]>=30) { light=1; dark=1; }
				else if (ch[cn].prof[P_LIGHT]>=30) { light=1; dark=2; }
                                else { flags=0; goto skip; }
			}

			if (!fast_los(cn,x+xoff,y+yoff)) { flags=0; goto skip; }

			light=trans_light(light);
			flags|=CMF_VISIBLE|light;

			if (map[m].flags&MF_UNDERWATER) flags|=CMF_UNDERWATER;

                        // character
			if ((co=map[m].ch) && char_see_char(cn,co)) {
				if (cmap[c].cn!=co || cmap[c].csprite!=ch[co].sprite) {
					buf10[3]|=1;
					*(unsigned int*)(buf10+p10)=cmap[c].csprite=ch[co].sprite; p10+=4;
					*(unsigned short*)(buf10+p10)=cmap[c].cn=co; p10+=2;
				}
				if (cmap[c].action!=ch[co].action || cmap[c].duration!=ch[co].duration) {				
					buf10[3]|=2;
					*(unsigned char*)(buf10+p10)=cmap[c].action=ch[co].action; p10++;
					*(unsigned char*)(buf10+p10)=cmap[c].duration=ch[co].duration; p10++;
					*(unsigned char*)(buf10+p10)=ch[co].step; p10++;
				}
				if (cmap[c].dir!=ch[co].dir || cmap[c].health!=health_per(co) || cmap[c].mana!=mana_per(co) || cmap[c].shield!=shield_per(co)) {
				       	buf10[3]|=4;
					*(unsigned char*)(buf10+p10)=cmap[c].dir=ch[co].dir; p10++;
					*(unsigned char*)(buf10+p10)=cmap[c].health=health_per(co); p10++;
					*(unsigned char*)(buf10+p10)=cmap[c].mana=mana_per(co); p10++;
					*(unsigned char*)(buf10+p10)=cmap[c].shield=shield_per(co); p10++;
				}
				
				if (!player_knows_name(nr,co)) {
					char tmp[80];
					extern char *get_title(int);

					sprintf(tmp,"%s%s",get_title(co),ch[co].name);

					len=strlen(tmp);
					
					buf[0]=SV_NAME;
					*(unsigned short*)(buf+1)=co;
					*(unsigned char*)(buf+3)=ch[co].level;
					if (ch[co].sprite==27) {	// hack to make demon sprite look more natural
						*(unsigned short*)(buf+4)=0;
						*(unsigned short*)(buf+6)=0;
						*(unsigned short*)(buf+8)=0;
					} else {
						*(unsigned short*)(buf+4)=ch[co].c1;
						*(unsigned short*)(buf+6)=ch[co].c2;
						*(unsigned short*)(buf+8)=ch[co].c3;
					}
					*(unsigned char*)(buf+10)=get_char_clan(co);
					*(unsigned char*)(buf+11)=get_pk_relation(cn,co);
					buf[12]=len;
					
					memcpy(buf+13,tmp,len);

					psend(nr,buf,len+13);
					if (!player[nr]) return;

					set_player_knows_name(nr,co,1);					
				}
				
				// char effects
				for (n=0; n<4; n++) {
					fn=ch[co].ef[n];
	
					// add effect to list
					if (fn) {
                                                for (f=0; f<maxef; f++) {
							if (fn==needef[f]) break;
						}
						if (f==maxef && maxef<MAXEF) {
							needef[maxef++]=fn;
						}
					}
				}
			} else {
				if (dark) { flags=0; goto skip; }
				
				if (cmap[c].cn) {
					buf10[3]|=8;
					cmap[c].cn=0;
					cmap[c].duration=cmap[c].action=cmap[c].step=0;
					cmap[c].dir=cmap[c].health=0;
				}
			}

			// map effects
			if (!dark) {
				for (n=0; n<4; n++) {
					fn=map[m].ef[n];
	
					// add effect to list
					if (fn) {
						for (f=0; f<maxef; f++) {
							if (fn==needef[f]) break;
						}
						if (f==maxef && maxef<MAXEF) {
							needef[maxef++]=fn;
						}
					}
	
					// transmit pointer if needed
					if (fn && !trans_ceffect(fn)) fn=0;
					
					if (fn!=cmap[c].ef[n]) {
						buf01[3]|=(1<<n);
						*(unsigned int*)(buf01+p01)=cmap[c].ef[n]=fn; p01+=4;
					}
				}
			} else { // dark
				for (n=0; n<4; n++) {
					if (cmap[c].ef[n]!=0) {
						buf01[3]|=(1<<n);
						*(unsigned int*)(buf01+p01)=cmap[c].ef[n]=0; p01+=4;
					}
				}
			} // end dark

			if (!dark) {
				// background sprite
				if (cmap[c].gsprite!=map[m].gsprite) {
					buf11[3]|=1;
					*(unsigned int*)(buf11+p11)=cmap[c].gsprite=map[m].gsprite; p11+=4;				
				}
	
				// foreground sprite
				if (cmap[c].fsprite!=map[m].fsprite) {
					buf11[3]|=2;
					*(unsigned int*)(buf11+p11)=cmap[c].fsprite=map[m].fsprite; p11+=4;
				}
				
				// item
				if ((in=map[m].it) && char_see_item(cn,in)) {
					isprite=it[in].sprite;
					if (it[in].flags&IF_TAKE) flags|=CMF_TAKE;
					if (it[in].flags&IF_USE) flags|=CMF_USE;
					if (it[in].flags&IF_PLAYERBODY) isprite|=0x80000000;
				} else isprite=0;
	
				if (isprite!=cmap[c].isprite) {
					buf11[3]|=4;
					*(unsigned int*)(buf11+p11)=cmap[c].isprite=isprite; p11+=4;
					if (isprite&0x80000000) {
						*(unsigned short*)(buf11+p11)=*(unsigned short*)(it[in].drdata+2); p11+=2;
						*(unsigned short*)(buf11+p11)=*(unsigned short*)(it[in].drdata+4); p11+=2;
						*(unsigned short*)(buf11+p11)=*(unsigned short*)(it[in].drdata+6); p11+=2;
					}
				}
				
				if (map[m].flags&(MF_SIGHTBLOCK|MF_TSIGHTBLOCK)) flags|=CMF_LIGHT;
			} else { // dark
				// background sprite
				if (dark==1) {
					if (cmap[c].gsprite!=51066) {
						buf11[3]|=1;
						*(unsigned int*)(buf11+p11)=cmap[c].gsprite=51066; p11+=4;
					}
				} else {
					if (cmap[c].gsprite!=51067) {
						buf11[3]|=1;
						*(unsigned int*)(buf11+p11)=cmap[c].gsprite=51067; p11+=4;
					}
				}
	
				// foreground sprite
				if (cmap[c].fsprite!=0) {
					buf11[3]|=2;
					*(unsigned int*)(buf11+p11)=cmap[c].fsprite=0; p11+=4;
				}
				
				// item
				if (cmap[c].isprite!=0) {
					buf11[3]|=4;
					*(unsigned int*)(buf11+p11)=cmap[c].isprite=0; p11+=4;
				}				
			} // end dark
			
                        skip:
			// flags (collected above)
			if (cmap[c].flags!=flags) {
				buf11[3]|=8;
				if (flags&0xff) {
					*(unsigned short*)(buf11+p11)=cmap[c].flags=flags; p11+=2;
				} else {
					*(unsigned char*)(buf11+p11)=cmap[c].flags=flags; p11++;
				}
			}

                        // send buf01 if needed
			if (buf01[3]) {
			
				buf01[3]|=SV_MAP01;
				
                                if (c==last) {
					buf01[3]|=SV_MAPTHIS;
					psend(nr,buf01+3,p01-3);
					if (!player[nr]) return;
				} else if (c==last+1) {
					buf01[3]|=SV_MAPNEXT;
					psend(nr,buf01+3,p01-3);
					if (!player[nr]) return;
				} else if (c<last+255) {
					buf01[2]=SV_MAPOFF|buf01[3];
					buf01[3]=c-last;
					psend(nr,buf01+2,p01-2);
					if (!player[nr]) return;
				} else {
					buf01[1]=SV_MAPPOS|buf01[3];
					*(unsigned short*)(buf01+2)=c;
					psend(nr,buf01+1,p01-1);
					if (!player[nr]) return;
				}
				
				last=c;
			}

			// send buf10 if needed
			if (buf10[3]) {

				buf10[3]|=SV_MAP10;
				
				if (c==last) {
					buf10[3]|=SV_MAPTHIS;
					psend(nr,buf10+3,p10-3);
					if (!player[nr]) return;
				} else if (c==last+1) {
					buf10[3]|=SV_MAPNEXT;
					psend(nr,buf10+3,p10-3);
					if (!player[nr]) return;
				} else if (c<last+255) {
					buf10[2]=SV_MAPOFF|buf10[3];
					buf10[3]=c-last;
					psend(nr,buf10+2,p10-2);
					if (!player[nr]) return;
				} else {
					buf10[1]=SV_MAPPOS|buf10[3];
					*(unsigned short*)(buf10+2)=c;
					psend(nr,buf10+1,p10-1);
					if (!player[nr]) return;
				}
				
				last=c;
			}

			// send buf11 if needed
			if (buf11[3]) {

				buf11[3]|=SV_MAP11;
				
				if (c==last) {
					buf11[3]|=SV_MAPTHIS;
					psend(nr,buf11+3,p11-3);
					if (!player[nr]) return;
					/*xlog("sending 11: %d size=%d [ %d %d %d %d %d %d %d %d ]",
						(unsigned char)buf11[3],
						p11-3,
						(unsigned char)buf11[4],
						(unsigned char)buf11[5],
						(unsigned char)buf11[6],
						(unsigned char)buf11[7],
						(unsigned char)buf11[8],
						(unsigned char)buf11[9],
						(unsigned char)buf11[10],
						(unsigned char)buf11[11]);*/
				} else if (c==last+1) {
					buf11[3]|=SV_MAPNEXT;
					psend(nr,buf11+3,p11-3);
					if (!player[nr]) return;
					/*xlog("sending 11: %d size=%d [ %d %d %d %d %d %d %d %d ]",
						(unsigned char)buf11[3],
						p11-3,
						(unsigned char)buf11[4],
						(unsigned char)buf11[5],
						(unsigned char)buf11[6],
						(unsigned char)buf11[7],
						(unsigned char)buf11[8],
						(unsigned char)buf11[9],
						(unsigned char)buf11[10],
						(unsigned char)buf11[11]);*/
				} else if (c<last+255) {
					buf11[2]=SV_MAPOFF|buf11[3];
					buf11[3]=c-last;
					psend(nr,buf11+2,p11-2);
					if (!player[nr]) return;
					/*xlog("sending 11: %d size=%d [ %d %d %d %d %d %d %d %d ]",
						(unsigned char)buf11[2],
						p11-2,
						(unsigned char)buf11[3],
						(unsigned char)buf11[4],
						(unsigned char)buf11[5],
						(unsigned char)buf11[6],
						(unsigned char)buf11[7],
						(unsigned char)buf11[8],
						(unsigned char)buf11[9],
						(unsigned char)buf11[10]);*/
				} else {
					buf11[1]=SV_MAPPOS|buf11[3];
					*(unsigned short*)(buf11+2)=c;
					psend(nr,buf11+1,p11-1);
					if (!player[nr]) return;
					/*xlog("sending 11: %d size=%d [ %d %d %d %d %d %d %d %d ]",
						(unsigned char)buf11[1],
						p11-1,
						(unsigned char)buf11[2],
						(unsigned char)buf11[3],
						(unsigned char)buf11[4],
						(unsigned char)buf11[5],
						(unsigned char)buf11[6],
						(unsigned char)buf11[7],
						(unsigned char)buf11[8],
						(unsigned char)buf11[9]);*/
				}
				
				last=c;
			}			
		}
	}

	// transmit effects on client map
	bzero(usedef,sizeof(usedef));

	// try to use cached effects
	for (f=0; f<maxef; f++) {
		
		fn=needef[f];

                for (n=0; n<MAXEF; n++) {
			if (player[nr]->ceffect[n].generic.nr==fn) break;
		}
		if (n<MAXEF) {
			needef[f]=0;
			usedef[n]=1;

                        size=fill_ceffect(fn,&cef);

                        if (memcmp(&cef,player[nr]->ceffect+n,size)) {
				buf[0]=SV_CEFFECT;
				buf[1]=n;
				memcpy(buf+2,&cef,size);
				psend(nr,buf,size+2);
				if (!player[nr]) return;

                                memcpy(player[nr]->ceffect+n,&cef,size);
				//log_char(cn,LOG_SYSTEM,0,"transmit 1 slot %d type %d",n,ef[fn].type);
			}
			player[nr]->seffect[n]=ef[fn].serial;
		}
		
	}

	// mark used effects
	for (n=0; n<MAXEF; n++) {
		if (player[nr]->ceffect[n].generic.nr) {
			if (!ef[player[nr]->ceffect[n].generic.nr].type) continue;
                        if (ef[player[nr]->ceffect[n].generic.nr].serial!=player[nr]->seffect[n]) continue;
			
			if (is_char_ceffect(player[nr]->ceffect[n].generic.type)) {
				co=player[nr]->ceffect[n].strike.cn;
				//say(cn,"effe cn=%s (%d), see=%d",ch[co].name,co,char_see_char(cn,co));
				if (char_see_char(cn,co)) {	//fast_los(cn,ch[co].x,ch[co].y)
					usedef[n]=1;
				}
			} else usedef[n]=1;			
		}
	}

	// transmit those not found in cache
        for (f=0; f<maxef; f++) {		
		if (!(fn=needef[f])) continue;

		for (n=0; n<MAXEF; n++) {
			if (!usedef[n]) break;
		}
		if (n<MAXEF) {
			usedef[n]=1;

                        size=fill_ceffect(fn,&cef);

                        buf[0]=SV_CEFFECT;
			buf[1]=n;
			memcpy(buf+2,&cef,size);
			psend(nr,buf,size+2);
			if (!player[nr]) return;
			
                        memcpy(player[nr]->ceffect+n,&cef,size);
			
			player[nr]->seffect[n]=ef[fn].serial;

			//log_char(cn,LOG_SYSTEM,0,"transmit 2 slot %d type %d",n,ef[fn].type);
		}
		
	}
	
	// build used effects flag
	for (n=0; n<MAXEF; n++) {
		if (usedef[n]) {
			uf|=1ull<<n;
			//log_char(cn,LOG_SYSTEM,0,"used slot %d",n);
		}
	}	
	
	// send used effects flags
	if (uf!=player[nr]->uf) {
		buf[0]=SV_UEFFECT;
		*(unsigned long long *)(buf+1)=player[nr]->uf=uf;
                psend(nr,buf,9);
		if (!player[nr]) return;
	}

#ifdef CCYCLES
	end=rdtsc();

	dur=end-start;
	if (dur<1000000) {
		if (!avg) avg=dur;
                avg=avg*0.99+dur*0.01;
	}

	if ((ticker&15)==15) log_char(cn,LOG_SYSTEM,0,"%lldK cycles",avg>>10);	
#endif
}

static void player_stats(int nr)
{
	int n,cn,in,sprite,flags,ct,co,s,price,len;
	unsigned char buf[256];
	struct depot_ppd *ppd;
	struct military_ppd *mppd;

	cn=player[nr]->cn;

	// values
	if (ch[cn].flags&CF_UPDATE) {	// only check for value updates if update_char() was called (speed optim.)
		for (n=0; n<V_MAX; n++) {
			if (ch[cn].value[0][n]!=player[nr]->value[0][n]) {
				buf[0]=SV_SETVAL0;
				buf[1]=n;
				*(short*)(buf+2)=player[nr]->value[0][n]=ch[cn].value[0][n];
				psend(nr,buf,4);
				if (!player[nr]) return;
			}
			if (ch[cn].value[1][n]!=player[nr]->value[1][n]) {
				buf[0]=SV_SETVAL1;
				buf[1]=n;
				*(short*)(buf+2)=player[nr]->value[1][n]=ch[cn].value[1][n];
				psend(nr,buf,4);
				if (!player[nr]) return;
			}
		}
		ch[cn].flags&=~CF_UPDATE;
	}

	// hp, endurance, mana, lifeshield, exp
	if (ch[cn].hp/POWERSCALE!=player[nr]->hp) {
		buf[0]=SV_SETHP;
		*(short*)(buf+1)=player[nr]->hp=ch[cn].hp/POWERSCALE;
		psend(nr,buf,3);
		if (!player[nr]) return;
	}
	if (ch[cn].endurance/POWERSCALE!=player[nr]->endurance) {
		buf[0]=SV_ENDURANCE;
		*(short*)(buf+1)=player[nr]->endurance=ch[cn].endurance/POWERSCALE;
		psend(nr,buf,3);
		if (!player[nr]) return;
	}
	if (ch[cn].mana/POWERSCALE!=player[nr]->mana) {
		buf[0]=SV_SETMANA;
		*(short*)(buf+1)=player[nr]->mana=ch[cn].mana/POWERSCALE;
		psend(nr,buf,3);
		if (!player[nr]) return;
	}
	if (ch[cn].lifeshield/POWERSCALE!=player[nr]->lifeshield) {
		buf[0]=SV_LIFESHIELD;
		*(short*)(buf+1)=player[nr]->lifeshield=ch[cn].lifeshield/POWERSCALE;
		psend(nr,buf,3);
		if (!player[nr]) return;
	}
	if (ch[cn].exp!=player[nr]->exp) {
		buf[0]=SV_EXP;
		*(unsigned long*)(buf+1)=player[nr]->exp=ch[cn].exp;
		psend(nr,buf,5);
		if (!player[nr]) return;
	}
	if (ch[cn].exp_used!=player[nr]->exp_used) {
		buf[0]=SV_EXP_USED;
		*(unsigned long*)(buf+1)=player[nr]->exp_used=ch[cn].exp_used;
		psend(nr,buf,5);
		if (!player[nr]) return;
	}
	if ((ticker&15)==15 && (mppd=set_data(cn,DRD_MILITARY_PPD,sizeof(struct military_ppd))) && mppd->military_pts!=player[nr]->mil_exp) {
		buf[0]=SV_MIL_EXP;
		*(unsigned int*)(buf+1)=player[nr]->mil_exp=mppd->military_pts;
		psend(nr,buf,5);
		if (!player[nr]) return;
	}
	if (ch[cn].speed_mode!=player[nr]->speed_mode) {
		buf[0]=SV_SPEEDMODE;
		buf[1]=player[nr]->speed_mode=ch[cn].speed_mode;
		psend(nr,buf,2);
		if (!player[nr]) return;
	}
	if (ch[cn].rage/POWERSCALE!=player[nr]->rage) {
		buf[0]=SV_SETRAGE;
		*(short*)(buf+1)=player[nr]->rage=ch[cn].rage/POWERSCALE;
		psend(nr,buf,3);
		if (!player[nr]) return;
	}
	
        /*if (ch[cn].flags&CF_GOD) {
		buf[0]=SV_CYCLES;
		*(unsigned long*)(buf+1)=cycles;
		psend(nr,buf,5);
		if (!player[nr]) return;
	}*/

	// items
	if (ch[cn].flags&CF_ITEMS) {
		for (n=0; n<INVENTORYSIZE; n++) {
			if ((in=ch[cn].item[n])) {
				
				sprite=it[in].sprite;
                                flags=it[in].flags&(IF_USE|IF_WNHEAD|IF_WNNECK|IF_WNBODY|IF_WNARMS|IF_WNBELT|IF_WNLEGS|IF_WNFEET|IF_WNLHAND|IF_WNRHAND|IF_WNCLOAK|IF_WNLRING|IF_WNRRING|IF_WNTWOHANDED);
				price=0;

			} else sprite=price=flags=0;

                        if (player[nr]->item[n]!=sprite || player[nr]->item_flags[n]!=flags) {
				buf[0]=SV_SETITEM;
				buf[1]=n;
				*(unsigned int*)(buf+2)=player[nr]->item[n]=sprite;
				*(unsigned int*)(buf+6)=player[nr]->item_flags[n]=flags;
                                psend(nr,buf,10);
				if (!player[nr]) return;
			}
		}		

		if ((in=ch[cn].citem)) {
			sprite=it[in].sprite;
			flags=it[in].flags&(IF_USE|IF_WNHEAD|IF_WNNECK|IF_WNBODY|IF_WNARMS|IF_WNBELT|IF_WNLEGS|IF_WNFEET|IF_WNLHAND|IF_WNRHAND|IF_WNCLOAK|IF_WNLRING|IF_WNRRING|IF_WNTWOHANDED);			
		} else flags=sprite=0;

		if (player[nr]->citem_sprite!=sprite || player[nr]->citem_flags!=flags) {
			buf[0]=SV_SETCITEM;
                        *(unsigned int*)(buf+1)=player[nr]->citem_sprite=sprite;
			*(unsigned int*)(buf+5)=player[nr]->citem_flags=flags;
			psend(nr,buf,9);
			if (!player[nr]) return;
		}

		if ((in=ch[cn].citem) && (it[in].flags&IF_MONEY)) {
                        if (player[nr]->cprice!=it[in].value) {
				buf[0]=SV_CPRICE;
                                *(unsigned int*)(buf+1)=player[nr]->cprice=it[in].value;
				psend(nr,buf,5);
				if (!player[nr]) return;
			}
		} else {
			if (player[nr]->cprice!=0) {
				buf[0]=SV_CPRICE;
                                *(unsigned int*)(buf+1)=player[nr]->cprice=0;
				psend(nr,buf,5);
				if (!player[nr]) return;
			}
		}

		if (player[nr]->gold!=ch[cn].gold) {
			buf[0]=SV_GOLD;
                        *(unsigned int*)(buf+1)=player[nr]->gold=ch[cn].gold;
			psend(nr,buf,5);
			if (!player[nr]) return;
		}

		ch[cn].flags&=~CF_ITEMS;
	}

	// container
	if ((in=ch[cn].con_in) && (ct=it[in].content)) {
		if (player[nr]->con_type!=1) {
			buf[0]=SV_CONTYPE;
			buf[1]=player[nr]->con_type=1;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
		if (strcmp(player[nr]->con_name,it[in].description)) {
			buf[0]=SV_CONNAME;
			buf[1]=len=strlen(it[in].description);
			strcpy(buf+2,it[in].description);
			strcpy(player[nr]->con_name,it[in].description);
			psend(nr,buf,len+2);
			if (!player[nr]) return;
		}
		if (player[nr]->con_cnt!=STORESIZE) {
			buf[0]=SV_CONCNT;
			buf[1]=player[nr]->con_cnt=STORESIZE;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
		for (n=0; n<STORESIZE; n++) {
			if ((in=con[ct].item[n])) sprite=it[in].sprite;
			else sprite=0;

			if (player[nr]->container[n]!=sprite) {
				buf[0]=SV_CONTAINER;
				buf[1]=n;
				*(unsigned int*)(buf+2)=player[nr]->container[n]=sprite;
				psend(nr,buf,6);
				if (!player[nr]) return;
			}
		}
	} else if ((co=ch[cn].merchant) && (s=ch[co].store)) {
		
		if (player[nr]->con_type!=2) {
			buf[0]=SV_CONTYPE;
			buf[1]=player[nr]->con_type=2;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
		if (strcmp(player[nr]->con_name,ch[co].name)) {
			buf[0]=SV_CONNAME;
			buf[1]=len=strlen(ch[co].name);
			strcpy(buf+2,ch[co].name);
			strcpy(player[nr]->con_name,ch[co].name);
			psend(nr,buf,len+2);
			if (!player[nr]) return;
		}
		
		if (player[nr]->con_cnt!=STORESIZE) {
			buf[0]=SV_CONCNT;
			buf[1]=player[nr]->con_cnt=STORESIZE;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
		for (n=0; n<STORESIZE; n++) {
			if ((in=store[s]->ware[n].cnt)) sprite=store[s]->ware[n].item.sprite;
			else sprite=0;

			if (player[nr]->container[n]!=sprite) {
				buf[0]=SV_CONTAINER;
				buf[1]=n;
				*(unsigned int*)(buf+2)=player[nr]->container[n]=sprite;
				psend(nr,buf,6);
				if (!player[nr]) return;
			}

			price=salesprice(co,cn,n);

			if (player[nr]->price[n]!=price) {
				buf[0]=SV_PRICE;
				buf[1]=n;
				*(unsigned int*)(buf+2)=player[nr]->price[n]=price;
				psend(nr,buf,6);
				if (!player[nr]) return;
			}
		}
		for (n=0; n<INVENTORYSIZE; n++) {
			if ((in=ch[cn].item[n])) price=buyprice(cn,in);
			else price=0;

                        if (player[nr]->item_price[n]!=price) {
				buf[0]=SV_ITEMPRICE;
				buf[1]=n;
				*(unsigned int*)(buf+2)=player[nr]->item_price[n]=price;
				psend(nr,buf,6);
				if (!player[nr]) return;
			}
		}
		if (ch[cn].citem) {
			price=buyprice(cn,ch[cn].citem);
			if (player[nr]->cprice!=price) {
				buf[0]=SV_CPRICE;
                                *(unsigned int*)(buf+1)=player[nr]->cprice=price;
				psend(nr,buf,5);
				if (!player[nr]) return;
			}
		} else if (player[nr]->cprice!=0) {
			buf[0]=SV_CPRICE;
			*(unsigned int*)(buf+1)=player[nr]->cprice=0;
			psend(nr,buf,5);
			if (!player[nr]) return;
		}

	} else if ((in=ch[cn].con_in) && (it[in].flags&IF_DEPOT) && (ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd)))) {
		if (player[nr]->con_type!=1) {
			buf[0]=SV_CONTYPE;
			buf[1]=player[nr]->con_type=1;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
		if (strcmp(player[nr]->con_name,"Your Depot")) {
			buf[0]=SV_CONNAME;
			buf[1]=len=strlen("Your Depot");
			strcpy(buf+2,"Your Depot");
			strcpy(player[nr]->con_name,"Your Depot");
			psend(nr,buf,len+2);
			if (!player[nr]) return;
		}
		if (player[nr]->con_cnt!=MAXDEPOT) {
			buf[0]=SV_CONCNT;
			buf[1]=player[nr]->con_cnt=MAXDEPOT;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
		for (n=0; n<MAXDEPOT; n++) {
			if (ppd->itm[n].flags) sprite=ppd->itm[n].sprite;
			else sprite=0;

			if (player[nr]->container[n]!=sprite) {
				buf[0]=SV_CONTAINER;
				buf[1]=n;
				*(unsigned int*)(buf+2)=player[nr]->container[n]=sprite;
				psend(nr,buf,6);
				if (!player[nr]) return;
			}
		}
	} else {
		if (player[nr]->con_cnt!=0) {
			buf[0]=SV_CONCNT;
			buf[1]=player[nr]->con_cnt=0;
			psend(nr,buf,2);
			if (!player[nr]) return;
		}
	}

	if (ch[cn].flags&CF_PROF) {
		ch[cn].flags&=~CF_PROF;
		buf[0]=SV_PROF;
		for (n=0; n<P_MAX; n++) buf[n+1]=ch[cn].prof[n];
		psend(nr,buf,21);
		if (!player[nr]) return;
	}	
}

static void player_act(int nr)
{
	unsigned char buf[16];
	int in,co;

        buf[0]=SV_ACT;
	*(unsigned short*)(buf+1)=player[nr]->action;

	switch(player[nr]->action) {
		case PAC_IDLE:
		case PAC_MAGICSHIELD:
		case PAC_FLASH:
		case PAC_WARCRY:
		case PAC_PULSE:
                case PAC_FREEZE:
			*(unsigned short*)(buf+3)=0;
			*(unsigned short*)(buf+5)=0;
			break;
		case PAC_MOVE:
		case PAC_FIREBALL:
		case PAC_BALL:
		case PAC_LOOK_MAP:
		case PAC_DROP:
			*(unsigned short*)(buf+3)=player[nr]->act1;
			*(unsigned short*)(buf+5)=player[nr]->act2;
			break;
		case PAC_TAKE:		
		case PAC_USE:
			in=player[nr]->act1;
			*(unsigned short*)(buf+3)=it[in].x;
			*(unsigned short*)(buf+5)=it[in].y;
			break;
		case PAC_KILL:
		case PAC_HEAL:
		case PAC_BLESS:		
		case PAC_GIVE:
			co=player[nr]->act1;
			*(unsigned short*)(buf+3)=ch[co].x;
			*(unsigned short*)(buf+5)=ch[co].y;
			break;
	}
	if (memcmp(player[nr]->svactbuf,buf,7)) {
		memcpy(player[nr]->svactbuf,buf,7);
		psend(nr,buf,7);
		if (!player[nr]) return;
	}	
}

static void player_update(int nr)
{
	int a,b,cn;

	if (!(cn=player[nr]->cn)) return;

	a=ticker%(TICKS*60);
	b=nr%(TICKS*60);

        if (a==b) stats_update(cn,1,0);
}


// note: log_player is inherently dangerous as it does not check the correctness
// of any %s's. please take care, in V2 this was one of the main causes of crashes!
int log_player(int nr,int color,char *format,...)
{
        va_list args;
        unsigned char buf[1024];
	int len,size;

	if (nr<1 || nr>MAXPLAYER) return 0;
	if (!player[nr]) return 0;

        va_start(args,format);
        len=vsnprintf(buf+3,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	buf[0]=SV_TEXT;
	*(unsigned short*)(buf+1)=len;
	psend(nr,buf,len+3);
        if (!player[nr]) return 0;

	if (buf[3]!='#') {
		size=min(len+1,MAXSCROLLBACK-player[nr]->scrollpos);
		
		if (size) memcpy(player[nr]->scrollback+player[nr]->scrollpos,buf+3,size);
		if (size<len+1) {
			memcpy(player[nr]->scrollback,buf+3+size,len+1-size);
			player[nr]->scrollpos=len+1-size;		
		} else player[nr]->scrollpos+=size;
	}

        return len;	
}

void write_scrollback(int nr,int cn,char *reason,char *namea,char *nameb)
{
	int n,start=0,p=0,lp=0;
	char buf[MAXSCROLLBACK*2+1024],sub[80],line[MAXSCROLLBACK+16];
	struct tm *tm;
        time_t t;

	if (!player[nr]) return;

	t=time_now;
        tm=localtime(&t);

	p=sprintf(buf,"Chat Screen complaint from %s, reason '%s', date: %02d:%02d:%02d on %d/%d/%02d:\n\n",
		  ch[cn].name,reason,
		  tm->tm_hour,tm->tm_min,tm->tm_sec,tm->tm_mday,tm->tm_mon+1,tm->tm_year-100);


	p+=sprintf(buf+p,"Summary:\n");
	for (n=player[nr]->scrollpos+1; n<MAXSCROLLBACK; n++) {
		if (player[nr]->scrollback[n]) {
                        line[lp++]=player[nr]->scrollback[n];
			start=1;
		} else if (start) {
			line[lp]=0; lp=0;
			if (strcasestr(line,namea) || strcasestr(line,nameb)) p+=sprintf(buf+p,"%s\n",line);
		}
	}
	for (n=0; n<player[nr]->scrollpos; n++) {
		if (player[nr]->scrollback[n]) line[lp++]=player[nr]->scrollback[n];
		else {
			line[lp]=0; lp=0;
			if (strcasestr(line,namea) || strcasestr(line,nameb)) p+=sprintf(buf+p,"%s\n",line);
		}
	}

	p+=sprintf(buf+p,"\nWhole Log:\n");
	start=0;
	for (n=player[nr]->scrollpos+1; n<MAXSCROLLBACK; n++) {
		if (player[nr]->scrollback[n]) {
			buf[p++]=player[nr]->scrollback[n];
			start=1;
		} else if (start) {
			buf[p++]=10;
			//xlog("mark: %s",buf);
			//p=0;
		}
	}
	for (n=0; n<player[nr]->scrollpos; n++) {
		if (player[nr]->scrollback[n]) buf[p++]=player[nr]->scrollback[n];
		else {
			buf[p++]=10;
			//xlog("mark: %s",buf);
			//p=0;
		}
	}
	buf[p]=0;
	sprintf(buf+p,"----------------\n\n");
        sprintf(sub,"Auto Complaint from %s",ch[cn].name);
        sendmail("complaint@astonia.com",sub,buf,"auto@astonia.com",0);
	//sendmail("joker@astonia.com",sub,buf,"auto@astonia.com",0);
}

void tick_player(void)
{
	int n;

	// read input and process commands
	for (n=1; n<MAXPLAYER; n++) {
		if (player[n]) {
			switch(player[n]->state) {
				case ST_CONNECT:	if (player[n]) read_login(n);
							if (player[n]) check_idle(n);
							break;
				
				case ST_NORMAL:		if (player[n]) player_update(n);
							if (player[n]) read_input(n);
							if (player[n] && player[n]->state!=ST_NORMAL) break;
                                                        if (player[n]) check_command(n);
							if (player[n] && player[n]->state!=ST_NORMAL) break;
							if (player[n]) check_lag(n);
							if (player[n]) check_ingame_idle(n);
							break;
				
				case ST_EXIT:		if (player[n]) check_idle(n);
							break;

				default:		elog("tick_player(): Unknown player state %d for player %d.",player[n]->state,n);
							kick_player(n,NULL);
							break;
			}
		}
	}
	// gather changes and send them out. this needs to be in two steps
	// so that all changes are made before we start looking for them to
	// send them out.
	for (n=1; n<MAXPLAYER; n++) {
		if (player[n]) {
			switch(player[n]->state) {
				case ST_CONNECT:	break;
				case ST_NORMAL:		if (player[n]) player_map(n);
							if (player[n]) player_stats(n);
							if (player[n]) player_act(n);
							break;
				case ST_EXIT:		break;
				default:		elog("tick_player(): Unknown player state %d for player %d.",player[n]->state,n);
							if (player[n]) kick_player(n,NULL);
							break;
			}
		}
	}
}

void player_idle(int nr)
{
	if (nr) player[nr]->action=PAC_IDLE;	
}

// save one player per call
void backup_players(void)
{
	static int n=1;
	int cn;

	while (n<MAXPLAYER) {
		if (player[n] && player[n]->state==ST_NORMAL && (cn=player[n]->cn)) {
			save_char(cn,0);
			n++;
			return;
		} else n++;
	}
	n=1;
}

void plr_send_inv(int cn,int co)
{
	int nr,n,in,sprite;
	char buf[80];

	nr=ch[cn].player;
	if (!nr) return;
	
	buf[0]=SV_LOOKINV;
	*(unsigned int*)(buf+1)=ch[co].sprite;
	*(unsigned int*)(buf+5)=ch[co].c1;
	*(unsigned int*)(buf+9)=ch[co].c2;
	*(unsigned int*)(buf+13)=ch[co].c3;

	for (n=0; n<12; n++) {
		if ((in=ch[co].item[n])) sprite=it[in].sprite;
		else sprite=0;
		
		*(unsigned int*)(buf+17+n*4)=sprite;
	}	
	psend(nr,buf,17+12*4);
}

void plr_ls(int cn,char *dir)
{
	char buf[256];
	int len,nr;

	len=strlen(dir);
	if (len>200) return;

	nr=ch[cn].player;
	if (!nr) return;

	buf[0]=SV_LS;
	buf[1]=len;
	strcpy(buf+2,dir);

	psend(nr,buf,len+2);
}

void plr_cat(int cn,char *dir)
{
	char buf[256];
	int len,nr;

	len=strlen(dir);
	if (len>200) return;

	nr=ch[cn].player;
	if (!nr) return;

	buf[0]=SV_CAT;
	buf[1]=len;
	strcpy(buf+2,dir);

	psend(nr,buf,len+2);
}

void player_special(int cn,unsigned int type,unsigned int opt1,unsigned int opt2)
{
	char buf[16];
	int nr;

        if (!(nr=ch[cn].player)) return;
	if (!player || !player[nr]) return;

        buf[0]=SV_SPECIAL;
	*(unsigned int*)(buf+1)=type;
	*(unsigned int*)(buf+5)=opt1;
	*(unsigned int*)(buf+9)=opt2;

	psend(nr,buf,13);
}

void sanity_check(int cn)
{
	int nr;

	nr=ch[cn].player;
	if (nr) {
		if (nr<0 || nr>=MAXPLAYER) {
			elog("character %s (%d) has illegal player %d",ch[cn].name,cn,nr);
			ch[cn].player=0;
		} else if (!player[nr]) {
			elog("character %s (%d) has player %d, but that one has been freed",ch[cn].name,cn,nr);
			ch[cn].player=0;
		} else if (player[nr]->cn!=cn) {
			elog("character %s (%d) has player %d, but that player points to character %d",ch[cn].name,cn,nr,player[nr]->cn);
			ch[cn].player=0;
		}
	}
}

unsigned int get_player_addr(int nr)
{
	if (!player) return 0;
	if (!player[nr]) return 0;
	
	return player[nr]->addr;
}











