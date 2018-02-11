/*
 * $Id: swear.c,v 1.5 2007/08/21 22:05:59 devel Exp $
 *
 * $Log: swear.c,v $
 * Revision 1.5  2007/08/21 22:05:59  devel
 * no more filtering fuck
 *
 * Revision 1.4  2007/07/09 11:24:52  devel
 * removed shutup command
 * reduced sensitivity of anti-social tests
 *
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
};

int swearing(int cn,char *text)
{
	struct swear_ppd *ppd;
	int n;

	if (!(ch[cn].flags&CF_PLAYER)) return 0;

	ppd=set_data(cn,DRD_SWEAR_PPD,sizeof(struct swear_ppd));
	if (!ppd) return 0;	// oops...

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

        for (n=9; n>0; n--)
		ppd->lasttalk[n]=ppd->lasttalk[n-1];
	
	ppd->lasttalk[0]=realtime;

	return 0;
}













