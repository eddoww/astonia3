/*
 * $Id: chat.c,v 1.7 2007/08/21 22:07:19 devel Exp $
 *
 * $Log: chat.c,v $
 * Revision 1.7  2007/08/21 22:07:19  devel
 * removed global channel again
 *
 * Revision 1.6  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.5  2007/06/22 12:54:50  devel
 * changed V2 chat channel to NWO channel
 *
 * Revision 1.4  2007/03/06 10:37:12  devel
 * added questlog item destruction
 *
 * Revision 1.3  2006/12/14 13:58:05  devel
 * added xmass special
 *
 * Revision 1.2  2006/03/22 14:30:13  ssim
 * added /shutup
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

#include "server.h"
#include "mem.h"
#include "log.h"
#include "talk.h"
#include "database.h"
#include "chat.h"
#include "death.h"
#include "tool.h"
#include "clan.h"
#include "ignore.h"
#include "tell.h"
#include "swear.h"
#include "chat.h"
#include "club.h"

int remove_item_from_body_bg(int cnID,int IID);

unsigned long long chat_raw=0;

struct cname
{
	char *name;
	char *desc;
};

struct cname cname[]={
	{"Announce","Announcements from management - NOLEAVE"},			//0
	{"Info","Requesting staff help, technical and gameplay questions"},	//1
	{"Gossip","Talk about Life, the Universe and Everything"},		//2
	{"Auction","Buy and sell stuff"},					//3
	{"Astonia","Other Astonia versions (2.0, 3.5)"},			//4
	{"Clan","Public channel for clan related matters"},			//5
	{"Grats","Grats on leveling!"},						//6
	{"Clan2","Channel only visible to members of your clan"},		//7
	{"Area","Channel only visible to those in your area"},			//8
	{"Mirror","Only visible to those in your area and mirror"},		//9
	{"Games","Discussions of computer games"},				//10
	{"Kill","Playerkiller related topics"},					//11
	{"ClanA","Channel only visible to clan members and allies"},		//12
	{"Club","Channel only visible to your club members"},			//13
	{NULL,NULL},								//14
	{NULL,NULL},								//15
	{NULL,NULL},								//16
	{NULL,NULL},								//17
	{NULL,NULL},								//18
	{NULL,NULL},								//19
	{NULL,NULL},								//20
	{NULL,NULL},								//21
	{NULL,NULL},								//22
	{NULL,NULL},								//23
	{NULL,NULL},								//24
	{NULL,NULL},								//25
	{NULL,NULL},								//26
	{NULL,NULL},								//27
	{NULL,NULL},								//28
	{NULL,NULL},								//29
	{NULL,NULL},								//30
	{"Staff","Staff member's private channel"},				//31
	{"God","Ye God's private channel"}					//32
};

#define CHATSERVER	"localhost"
#define INBUFSIZE	1024

static int 	connected=0,
                sock=-1,
		ilen=0,
		state=0;

static char *inbuf=NULL;

static int connect_chat(void)
{
        int one=1,ret;
	static struct sockaddr_in addr;
	struct hostent *he;

	he=gethostbyname(CHATSERVER);
	if (!he) return 0;

        sock=socket(PF_INET,SOCK_STREAM,0);
	if (sock==-1) return 0;

        addr.sin_family=AF_INET;
	addr.sin_port=htons(5554);
	addr.sin_addr.s_addr=*(unsigned long*)(*he->h_addr_list);	//htonl((195<<24)+(50<<16)+(130<<8)+3);

	ioctl(sock,FIONBIO,(u_long*)&one);      // non-blocking mode

	ret=connect(sock,(struct sockaddr *)&addr,sizeof(addr));

        return 1;
}

int init_chat(void)
{
        inbuf=xmalloc(INBUFSIZE,IM_BASE);

	return 1;
}

static void setglobal(int idx,int value) {
	switch(idx) {
		case 0:	isxmas=value; break;
	}
}

static void rec_chat(unsigned short channel,char *text)
{
	int n;
	unsigned int bit;

        if (channel==0) {
		for (n=1; n<MAXCHARS; n++) {
			if (!ch[n].flags) continue;
                        log_char(n,LOG_SYSTEM,0,"%s",text);
		}
	} else if (channel<1024) {
		int uID,cnr=0,step,anr=0,mnr=0;

		bit=1<<(channel-1);
		uID=atoi(text);
		
		if (channel==7 || channel==12 || channel==13) { cnr=atoi(text+11); step=14; }
		else if (channel==8) { anr=atoi(text+11); step=14; }
		else if (channel==9) { anr=atoi(text+11); mnr=atoi(text+14); step=17; }
		else step=10;

                for (n=1; n<MAXCHARS; n++) {
			if (!ch[n].flags) continue;
			if (!(ch[n].channel&bit)) continue;
                        if (channel==31 && !(ch[n].flags&(CF_STAFF|CF_GOD))) { ch[n].channel&=~bit; continue; }
			if (channel==32 && !(ch[n].flags&CF_GOD)) { ch[n].channel&=~bit; continue; }
			if (ignoring(n,uID)) continue;
			if (channel==7 && cnr!=get_char_clan(n)) continue;
			if (channel==12 && cnr!=get_char_clan(n) && !clan_alliance(cnr,get_char_clan(n))) continue;
                        if (anr && anr!=areaID) continue;
			if (mnr && mnr!=areaM) continue;
			if (channel==13 && get_char_club(n)!=cnr) continue;
			
			log_char(n,LOG_SYSTEM,0,"%s",text+step);
		}
	} else if (channel==1024 || channel==1030) {	// tell
		int cnID,coID,ret;
		char buf[80];

		cnID=atoi(text);
		coID=atoi(text+11);
		for (n=1; n<MAXCHARS; n++) {
			if (!ch[n].flags) continue;
			if (ch[n].ID!=cnID) continue;
			if (channel!=1030 && (ch[n].flags&CF_NOTELL)) continue;
			if (channel!=1030 && ignoring(n,coID)) continue;
			ret=log_char(n,LOG_SYSTEM,0,"°c6%s",text+22);
			if (ret && coID) {	// server sends messages with ID 0 and does not need a received note
				sprintf(buf,"%010u:%010u",coID,cnID);
				server_chat(1029,buf);
			}
		}
	} else if (channel==1026) {	// allow
		int cnID,coID;

		cnID=atoi(text);
		coID=atoi(text+11);
		allow_body_db(cnID,coID);
	} else if (channel==1027) {	// look
		int cnID,coID;

		cnID=atoi(text);
		coID=atoi(text+11);
		look_values_bg(cnID,coID);
	} else if (channel==1028) {	// dungeon-clan
		clan_dungeon_chat(text);
	} else if (channel==1029) {	// chat receive ack
		int cnID,coID;

		cnID=atoi(text);
		coID=atoi(text+11);
		for (n=1; n<MAXCHARS; n++) {
			if (!ch[n].flags) continue;
			if (ch[n].ID!=cnID) continue;
			register_rec_tell(n,coID);
		}
	} else if (channel==1031) {	// whostaff request
		int cnID;

		cnID=atoi(text);
                do_whostaff(cnID);
	} else if (channel==1032) {	// look
		int cnID,coID;

		cnID=atoi(text);
		coID=atoi(text+11);
		lollipop_bg(cnID,coID);
	} else if (channel==1033) {	// shutdown
		int t,m;

                t=atoi(text);
		m=atoi(text+11);
		shutdown_bg(t,m);
	} else if (channel==1034) {	// shutup
		int cnID,coID,minutes;
		void shutup_bg(int,int,int);

		cnID=atoi(text);
		coID=atoi(text+11);
		minutes=atoi(text+22);
		shutup_bg(cnID,coID,minutes);
	} else if (channel==1035) {	// setglobal
		int idx,value;

		idx=atoi(text);
		value=atoi(text+11);
                setglobal(idx,value);
	} else if (channel==1036) {	// destroy items in body
		int cnID,IID;

		cnID=atoi(text);
		IID=atoi(text+11);
		remove_item_from_body_bg(cnID,IID);
	}
}

static int send_chat(unsigned short channel,char *text)
{
	char buf[1024];
	int len,ret;

	len=strlen(text)+3;
	if (len>1000) return -1;

	if (connected) {
		*(unsigned short*)(buf)=len;
		*(unsigned short*)(buf+2)=channel;
		strcpy(buf+4,text);
	
		ret=send(sock,buf,len+2,0);
                if (ret==-1) {
			connected=0;
			close(sock);
			state=0;
			elog("lost connection to chat server (send - %s)",text);
		}
		chat_raw+=ret+60;
	}

        if (!connected) {
		rec_chat(channel,text);
	}

        return 1;
}

void tick_chat(void)
{
	int len,ret;
	fd_set fd_out,fd_err,fd_in;
	struct timeval tv;
	static int delay=0;

	while (connected) {
                ret=recv(sock,inbuf+ilen,INBUFSIZE-ilen,0);

		if (ret>0) {
			ilen+=ret;
			chat_raw+=ret+60;
		}

                if (ilen>1) {
			len=*(unsigned short *)(inbuf)+2;
			if (ilen>=len) {
				rec_chat(*(unsigned short*)(inbuf+2),inbuf+4);
				ilen-=len;
				if (ilen) memmove(inbuf,inbuf+len,ilen);
			} else break;
		} else break;
	}
	
	if (!connected) {
		if (delay>0) { delay--; return; }
		
		if (state==0) {
			//xlog("trying chat connect");
			connect_chat();
			state=1;
			delay=10;
			return;
		}
		
		if (state==1) {
			bzero(&tv,sizeof(tv));
			FD_ZERO(&fd_out); FD_ZERO(&fd_err); FD_ZERO(&fd_in);
                        FD_SET(sock,&fd_out); FD_SET(sock,&fd_err);

			ret=select(sock+1,&fd_in,&fd_out,&fd_err,&tv);
			
			if (ret==-1) {
				close(sock);
				state=0;
				delay=100;
				return;
			}
			if (ret==0) return;
	
                        if (FD_ISSET(sock,&fd_err)) {
				close(sock);
				delay=100;
				state=0;				
			} else if (FD_ISSET(sock,&fd_out)) {
				unsigned int sol,sollen=4;
				if (getsockopt(sock,SOL_SOCKET,SO_ERROR,&sol,&sollen)) { close(sock); delay=100; state=0; }
				else if (sol) { close(sock); delay=100; state=0; }
				else {
					connected=1;
					//xlog("chat connect success");
				}
                                //xlog("sol=%d, sollen=%d",sol,sollen);
			}
		}
	}
}

void join_chat(int cn,int nr)
{
	unsigned int bit;

	if (nr<1 || nr>32) {
		log_char(cn,LOG_SYSTEM,0,"Channel number must be between 1 and 32.");
		return;
	}

	bit=1<<(nr-1);

	if (ch[cn].channel&bit) {
		log_char(cn,LOG_SYSTEM,0,"You have already joined channel %d (%s).",nr,cname[nr].name);
		return;
	}

	if (nr==31 && !(ch[cn].flags&(CF_STAFF|CF_GOD))) {
		log_char(cn,LOG_SYSTEM,0,"Permission denied to join channel %d (%s).",nr,cname[nr].name);
		return;
	}

	if (nr==32 && !(ch[cn].flags&CF_GOD)) {
		log_char(cn,LOG_SYSTEM,0,"Permission denied to join channel %d (%s).",nr,cname[nr].name);
		return;
	}

        ch[cn].channel|=bit;

	log_char(cn,LOG_SYSTEM,0,"You have joined channel %d (%s).",nr,cname[nr].name);
}

void leave_chat(int cn,int nr)
{
	unsigned int bit;

	if (nr<1 || nr>32) {
		log_char(cn,LOG_SYSTEM,0,"Channel number must be between 1 and 32.");
		return;
	}

	bit=1<<(nr-1);

	if (!(ch[cn].channel&bit)) {
		log_char(cn,LOG_SYSTEM,0,"You have already left channel %d (%s).",nr,cname[nr].name);
		return;
	}

	ch[cn].channel&=~bit;

	log_char(cn,LOG_SYSTEM,0,"You have left channel %d (%s).",nr,cname[nr].name);
}

void list_chat(int cn)
{
	int n;

	for (n=0; n<33; n++) {
		if (cname[n].name) log_char(cn,LOG_SYSTEM,0,"%2d: %-10.10s - %s",n,cname[n].name,cname[n].desc);
	}
}

static int cmdcmp(char *ptr,char *cmd)
{
	int len=0;

	while (toupper(*ptr)==toupper(*cmd)) {
		ptr++; cmd++; len++;
		if (*ptr==0 || *ptr==' ') return len;
	}

	return 0;
}

static int write_chat(int cn,int channel,char *text)
{
	int bit,n,xID,col;
	char buf[256],name[80];

	while (isspace(*text)) text++;
	
	if (!*text) {
		log_char(cn,LOG_SYSTEM,0,"You cannot send empty chat messages.");
		return 1;
	}
	if (strlen(text)>200) {
		log_char(cn,LOG_SYSTEM,0,"This chat message is too long.");
		return 1;
	}

	if ((ch[cn].flags&CF_PLAYER) && (channel==0 || channel==32) && !(ch[cn].flags&CF_GOD)) {
		log_char(cn,LOG_SYSTEM,0,"Access denied.");
		return 1;
	}

	if ((ch[cn].flags&CF_PLAYER) && channel==31 && !(ch[cn].flags&(CF_STAFF|CF_GOD))) {
		log_char(cn,LOG_SYSTEM,0,"Access denied.");
		return 1;
	}

	if ((ch[cn].flags&CF_PLAYER) && channel) {
		bit=1<<(channel-1);
		if (!(ch[cn].channel&bit)) {
			log_char(cn,LOG_SYSTEM,0,"You must join a channel before you can use it.");
			return 1;
		}
	}
	if ((ch[cn].flags&CF_PLAYER) && (channel==7 || channel==12) && get_char_clan(cn)==0) {
		log_char(cn,LOG_SYSTEM,0,"Access denied - clan members only.");
		return 1;
	}

	if ((ch[cn].flags&CF_PLAYER) && (channel==13) && get_char_club(cn)==0) {
		log_char(cn,LOG_SYSTEM,0,"Access denied - club members only.");
		return 1;
	}

	if (swearing(cn,text)) return 1;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"chat(%s): \"%s\"",cname[channel].name,text);

	if (ch[cn].flags&CF_STAFF) {
		for (n=0; n<75 && ch[cn].name[n]; n++) name[n]=toupper(ch[cn].name[n]);
		name[n]=0;
	} else strcpy(name,ch[cn].name);

	if (ch[cn].flags&(CF_STAFF|CF_GOD)) xID=0;
	else xID=ch[cn].ID;

	switch(channel) {
		case 0:		col=3; break;	// announce
		case 1:		col=12; break;	// info
		case 2:		col=2; break;	// gossipe
		case 3:		col=9; break;	// auction
		case 4:		col=14; break; // v2
		case 5:		col=15; break; // public clan
		case 6:		col=10; break; // grats
		case 7:		col=16; break; // internal clan
		case 8:		col=13; break; // area
		case 9:		col=11; break; // mirror
		case 10:	col=14; break; // games
		case 11:	col=14; break; // kill
		case 12:	col=16; break; // allied clan
		case 13:	col=16; break; // club channel
		case 31:	col=7; break;	// staff
		case 32:	col=8; break;	// god
		default:	col=2; break;
	}

        if (channel==0) sprintf(buf,"°c%d%s",col,text);	// announce
        else if (channel==7 || channel==12) sprintf(buf,"%010u:%02u:°c%d%s: °c17%s°c18 (%d) says: \"%s\"",xID,get_char_clan(cn),col,cname[channel].name,name,ch[cn].mirror,text);		// clan internal
	else if (channel==13) sprintf(buf,"%010u:%02u:°c%d%s: °c17%s°c18 (%d) says: \"%s\"",xID,get_char_club(cn),col,cname[channel].name,name,ch[cn].mirror,text);			// club internal
	else if (channel==8) {
		sprintf(buf,"%010u:%02u:°c%d%s: °c17%s°c18%s%s%s (%d) says: \"%s\"",
			xID,
			areaID,
			col,
			cname[channel].name,
			name,
			(ch[cn].flags&CF_STAFF) ? " [" : "",
			(ch[cn].flags&CF_STAFF) ? ch[cn].staff_code : "",
			(ch[cn].flags&CF_STAFF) ? "]" : "",
			ch[cn].mirror,
			text);			// area internal
	} else if (channel==9) {
		sprintf(buf,"%010u:%02u:%02u:°c%d%s: °c17%s°c18%s%s%s (%d) says: \"%s\"",
			xID,
			areaID,
			areaM,
			col,
			cname[channel].name,
			name,
			(ch[cn].flags&CF_STAFF) ? " [" : "",
			(ch[cn].flags&CF_STAFF) ? ch[cn].staff_code : "",
			(ch[cn].flags&CF_STAFF) ? "]" : "",
			ch[cn].mirror,
			text);	// mirror internal
        } else if (channel==4) {
		sprintf(buf,"%010u°c%d%s: °c17%s°c18%s%s%s (%s) says: \"%s\"",
			xID,
			col,
			cname[channel].name,
			name,
			(ch[cn].flags&CF_STAFF) ? " [" : "",
			(ch[cn].flags&CF_STAFF) ? ch[cn].staff_code : "",
			(ch[cn].flags&CF_STAFF) ? "]" : "",
			"OW",
			text);	// normal
	} else {
		sprintf(buf,"%010u°c%d%s: °c17%s°c18%s%s%s (%d) says: \"%s\"",
			xID,
			col,
			cname[channel].name,
			name,
			(ch[cn].flags&CF_STAFF) ? " [" : "",
			(ch[cn].flags&CF_STAFF) ? ch[cn].staff_code : "",
			(ch[cn].flags&CF_STAFF) ? "]" : "",
			ch[cn].mirror,			
			text);	// normal
	}
	

	send_chat(channel,buf);

	return 1;
}

int server_chat(int channel,char *text)
{
        if (strlen(text)>200) {
                return 0;
	}

        send_chat(channel,text);

	return 1;
}

// non-error-checking version
int tell_chat(int cnID,int coID,int staffmode,char *format,...)
{
	int ret;
	char buf[512];
        va_list args;

        sprintf(buf,"%010u:%010u:",coID,cnID);

	va_start(args,format);
	vsnprintf(buf+22,300,format,args);
	va_end(args);

        if (staffmode) ret=send_chat(1030,buf);
	else ret=send_chat(1024,buf);

        return ret;
}

int cmd_chat(int cn,char *text)
{
	int n,len;
	char buf[80];

	for (n=0; n<33; n++) {
		if (!cname[n].name) continue;
		
		sprintf(buf,"c%d",n);
		if ((len=cmdcmp(text,buf))) {
			return write_chat(cn,n,text+len);
		}
		if ((len=cmdcmp(text,cname[n].name))) {
			return write_chat(cn,n,text+len);
		}
	}
	return 0;
}

void npc_chat(int cn,int channel,char *format,...)
{
        va_list args;
        char buf[1024];

        va_start(args,format);
        vsnprintf(buf,1024,format,args);
        va_end(args);

	write_chat(cn,channel,buf);
}











