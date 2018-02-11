/*

$Id: swear.c,v 1.4 2007/10/24 11:39:15 devel Exp $

$Log: swear.c,v $
Revision 1.4  2007/10/24 11:39:15  devel
removed wtf

Revision 1.3  2006/03/22 14:30:13  ssim
added /shutup

Revision 1.2  2005/12/11 10:36:51  ssim
removed spam filter exceptions

Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.3  2003/11/29 12:14:24  sam
made gods exempt from swearing rules

Revision 1.2  2003/10/13 14:12:54  sam
Added RCS tags


*/

#include <string.h>
#include <ctype.h>

#include "server.h"
#include "drdata.h"
#include "talk.h"
#include "date.h"
#include "tool.h"
#include "create.h"
#include "chat.h"
#include "swear.h"

struct swear_ppd
{
	int lasttalk[10];
	int bad;
	
	char last_sentence[10][80];
	int last_time[10];
	int last_cnt[10];
	int last_pos;

	int banned_till;
};

int all_upper(char *ptr)
{
	int cnt=0;

        while (*ptr) {
		if (toupper(*ptr)!=*ptr) return 0;
		if (isalpha(*ptr)) cnt++;		
		ptr++;
	}
	if (cnt>3) return 1;
	
	return 0;
}

int swearing(int cn,char *text)
{
	struct swear_ppd *ppd;
	int n,flag;

	if (!(ch[cn].flags&CF_PLAYER)) return 0;

	ppd=set_data(cn,DRD_SWEAR_PPD,sizeof(struct swear_ppd));
	if (!ppd) return 0;	// oops...

	if (ppd->banned_till>realtime) {
		log_char(cn,LOG_SYSTEM,0,"°c3Chat is blocked for %.2f minutes.",(ppd->banned_till-realtime)/60.0);
		return 1;
	}

	if (ch[cn].flags&CF_GOD) return 0;

	if (realtime-ppd->bad<30) {
		log_char(cn,LOG_SYSTEM,0,"°c3Chat is blocked.");
		return 1;
	}

        if (realtime-ppd->lasttalk[1]<1) {	// 0.3s per line
		log_char(cn,LOG_SYSTEM,0,"°c3Chat has been blocked for 30 seconds for excessive usage (1).");
		ppd->bad=realtime;
		return 1;
	}
	if (realtime-ppd->lasttalk[4]<10) {	// 2s per line
		log_char(cn,LOG_SYSTEM,0,"°c3Chat has been blocked for 30 seconds for excessive usage (2).");
		ppd->bad=realtime;
		return 1;
	}
	if (realtime-ppd->lasttalk[9]<30) {	// 3s per line
		log_char(cn,LOG_SYSTEM,0,"°c3Chat has been blocked for 30 seconds for excessive usage (3).");
		ppd->bad=realtime;
		return 1;
	}

	if (strcasestr(text,"fuck") || strcasestr(text,"cunt") || strcasestr(text,"faggot")) {
		log_char(cn,LOG_SYSTEM,0,"°c3Swearing is illegal in this game. While only a few words are blocked by the system, you will get punished and eventually banned if you swear using non-blocked words.");
		log_char(cn,LOG_SYSTEM,0,"°c3Chat has been blocked for 30 seconds.");
		ppd->bad=realtime;
		return 1;
	}

	if (strlen(text)>3 && all_upper(text)) {
		log_char(cn,LOG_SYSTEM,0,"°c3Using capitalized letters only is impolite. Trying to get around the block by using mostly caps will get you punished and eventually banned.");
		log_char(cn,LOG_SYSTEM,0,"°c3Chat has been blocked for 30 seconds.");
		ppd->bad=realtime;
		return 1;
	}

	// test for repeating long sentences
	if (strlen(text)>20) {
		for (n=flag=0; n<10; n++) {
			if (!strncmp(ppd->last_sentence[n],text,78) && realtime-ppd->last_time[n]<30) {
				if (ppd->last_cnt[n]>2 || realtime-ppd->last_time[n]<4) {
					log_char(cn,LOG_SYSTEM,0,"°c3Repeating the same sentence is impolite. Repeating variants of the same sentence will get you punished and eventually banned.");
					log_char(cn,LOG_SYSTEM,0,"°c3Chat has been blocked for 30 seconds.");
					ppd->bad=realtime;
					return 1;
				}
				ppd->last_cnt[n]++;
				ppd->last_time[ppd->last_pos]=realtime;
				flag=1;
				break;
			}
		}
		if (!flag) {
			if (ppd->last_pos<0 || ppd->last_pos>9) ppd->last_pos=0;
			strncpy(ppd->last_sentence[ppd->last_pos],text,78); ppd->last_sentence[ppd->last_pos][78]=0;
			ppd->last_time[ppd->last_pos]=realtime;
			ppd->last_cnt[ppd->last_pos]=1;
			ppd->last_pos++;
		}
	}

	for (n=9; n>0; n--)
		ppd->lasttalk[n]=ppd->lasttalk[n-1];
	
	ppd->lasttalk[0]=realtime;

	return 0;
}

void shutup_bg(int cnID,int coID,int minutes)
{
	struct swear_ppd *ppd;
	int co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].ID==coID) break;		
	}
	if (!co) return;

	if (!(ch[co].flags&CF_PLAYER)) return;

	ppd=set_data(co,DRD_SWEAR_PPD,sizeof(struct swear_ppd));
	if (!ppd) return;	// oops...

	ppd->banned_till=realtime+(minutes*60);

        if (minutes>0) log_char(co,LOG_SYSTEM,0,"°c3Your ability to talk has been disabled.");
	else log_char(co,LOG_SYSTEM,0,"°c3Your ability to talk has been enabled.");

	tell_chat(0,cnID,1,"%s cannot talk for %d minutes.",ch[co].name,minutes);
}























