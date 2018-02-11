/*

$Id: talk.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: talk.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2004/03/16 11:50:09  sam
added logging to say()

Revision 1.2  2003/10/13 14:12:55  sam
Added RCS tags


*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "server.h"
#include "notify.h"
#include "log.h"
#include "player.h"
#include "see.h"
#include "path.h"
#include "mem.h"
#include "sector.h"
#include "btrace.h"
#include "talk.h"
#include "database.h"

// this is the low level "send text to character" routine ALL other functions MUST use
// make sure you use log_char(cn,type,"%s",text) if text is NOT a format string.
int log_char(int cn,int type,int dat1,char *format,...)
{
	va_list args;
        unsigned char buf[1024];
	int len,nr,n;

	if (cn<1 || cn>=MAXCHARS) { elog("log_char(): got illegal character %d",cn); btrace("illegal cn"); return 0; }

	if (type==LOG_INFO && !char_see_char(cn,dat1)) return 0;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	// make sure the text is legal - we don't want any control characters in it!
	for (n=0; n<len; n++)
		if (!isprint(buf[n]) && buf[n]!=176 && buf[n]>31) buf[n]=' ';

	if (ch[cn].flags&CF_PLAYER) {
		nr=ch[cn].player;
		if (!log_player(nr,type,"%s",buf)) return 0;
	} else {
                notify_char(cn,NT_TEXT,type,(int)xstrdup(buf,IM_TALK),dat1);
	}

	return len;
}

int log_area(int xc,int yc,int type,int dat1,int maxdist,char *format,...)
{
	int x,y,xs,xe,ys,ye,cn,len;
        unsigned char buf[1024];
	va_list args;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

        xs=max(0,xc-maxdist);
	xe=min(MAXMAP-1,xc+maxdist);
	ys=max(0,yc-maxdist);
	ye=min(MAXMAP-1,yc+maxdist);

        for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (cn=getfirst_char_sector(x,y); cn; cn=ch[cn].sec_next) {
				if (ch[cn].x>=xs && ch[cn].x<=xe && ch[cn].y>=ys && ch[cn].y<=ye) {
					if (type==LOG_TALK && !sector_hear(xc,yc,ch[cn].x,ch[cn].y)) continue;
					if (type==LOG_SHOUT && !sector_hear_shout(xc,yc,ch[cn].x,ch[cn].y)) continue;

					log_char(cn,type,dat1,"%s",buf);
				}					
			}
		}
	}

	return len;
}

int sound_area(int xc,int yc,int type)
{
	int x,y,xs,xe,ys,ye,cn;
	int distx,disty,dist;

        xs=max(0,xc-16);
	xe=min(MAXMAP-1,xc+16);
	ys=max(0,yc-16);
	ye=min(MAXMAP-1,yc+16);

        for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (cn=getfirst_char_sector(x,y); cn; cn=ch[cn].sec_next) {
				if (ch[cn].x>=xs && ch[cn].x<=xe && ch[cn].y>=ys && ch[cn].y<=ye) {
					if (type==LOG_TALK && !sector_hear(xc,yc,ch[cn].x,ch[cn].y)) continue;

					distx=ch[cn].x-xc;
					disty=ch[cn].y-yc;
					dist=(distx*distx+disty*disty)*10;

					if (ch[cn].player) player_special(cn,type,-dist,distx*100);
				}					
			}
		}
	}

	return 1;
}

#define HOLLERDIST	(DIST*3)
#define SHOUTDIST 	(DIST*2)
#define SAYDIST 	(DIST)
#define EMOTEDIST 	(DIST/2)
#define QUIETSAYDIST 	(DIST/3)
#define WHISPERDIST 	(DIST/4)

#define HOLLERCOST	(12*POWERSCALE)
#define SHOUTCOST	(6*POWERSCALE)

int holler(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	if (ch[cn].flags&CF_PLAYER) {
		if (ch[cn].endurance<HOLLERCOST) {
			log_char(cn,LOG_SYSTEM,0,"You're too exhausted to holler.");
			return 0;
		}
		ch[cn].endurance-=HOLLERCOST;
	}
	ch[cn].regen_ticker=ticker;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	if (strchr(buf,'"')) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"hollers: \"%s\"",buf);

	return log_area(ch[cn].x,ch[cn].y,LOG_SHOUT,cn,HOLLERDIST,"%s hollers: \"%s\"",ch[cn].name,buf);
}

int shout(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	if (ch[cn].flags&CF_PLAYER) {
		if (ch[cn].endurance<SHOUTCOST) {
			log_char(cn,LOG_SYSTEM,0,"You're too exhausted to shout.");
			return 0;
		}
		ch[cn].endurance-=SHOUTCOST;
	}
	ch[cn].regen_ticker=ticker;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	if (strchr(buf,'"')) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"shouts: \"%s\"",buf);

	return log_area(ch[cn].x,ch[cn].y,LOG_SHOUT,cn,SHOUTDIST,"%s shouts: \"%s\"",ch[cn].name,buf);
}

int say(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	//if (strchr(buf,'"')) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"says: \"%s\"",buf);

	if (ch[cn].flags&CF_PLAYER) return log_area(ch[cn].x,ch[cn].y,LOG_TALK,cn,SAYDIST,"%s says: \"%s\"",ch[cn].name,buf);
	else return log_area(ch[cn].x,ch[cn].y,LOG_TALK,cn,SAYDIST,"%s says: \"%s\"",ch[cn].name,buf);
}

int emote(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	if (strchr(buf,'"')) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"emotes: \"%s\"",buf);

	return log_area(ch[cn].x,ch[cn].y,LOG_INFO,cn,EMOTEDIST,"%s %s.",ch[cn].name,buf);
}

int quiet_say(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	if (strchr(buf,'"')) return 0;

	return log_area(ch[cn].x,ch[cn].y,LOG_TALK,cn,QUIETSAYDIST,"%s says: \"%s\"",ch[cn].name,buf);
}

int whisper(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	if (strchr(buf,'"')) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"whispers: \"%s\"",buf);

	return log_area(ch[cn].x,ch[cn].y,LOG_TALK,cn,WHISPERDIST,"%s whispers: \"%s\"",ch[cn].name,buf);
}

int murmur(int cn,char *format,...)
{
	unsigned char buf[1024];
	va_list args;
	int len;

	va_start(args,format);
        len=vsnprintf(buf,1020,format,args);
        va_end(args);
	
	if (len==1020) return 0;

	if (strchr(buf,'"')) return 0;

	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"murmers: \"%s\"",buf);

	return log_area(ch[cn].x,ch[cn].y,LOG_TALK,cn,WHISPERDIST,"%s murmurs: \"%s\"",ch[cn].name,buf);
}
