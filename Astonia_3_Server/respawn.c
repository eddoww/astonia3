/*

$Id: respawn.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: respawn.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:40  sam
Added RCS tags


*/

#include "server.h"
#include "mem.h"
#include "tool.h"
#include "date.h"
#include "log.h"
#include "respawn.h"

struct respawn
{
	int cn;
	int serial;
	int tmp;
	int tmpx;
	int tmpy;
	int last_death;
	int last_respawn;
};

struct respawn *respawn=NULL;
int cnt=0;

void register_respawn_char(int cn)
{
	int n;

	for (n=0; n<cnt; n++) {
		if (respawn[n].tmp==ch[cn].tmp &&
		    respawn[n].tmpx==ch[cn].tmpx &&
		    respawn[n].tmpy==ch[cn].tmpy) {
                        elog("respawn checker register: double entry for %s (%d %d %d)",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy);
			return;
		}
	}

	n=cnt++;
	respawn=xrealloc(respawn,sizeof(struct respawn)*cnt,IM_BASE);

	respawn[n].cn=cn;
	respawn[n].serial=ch[cn].serial;
	respawn[n].tmp=ch[cn].tmp;
	respawn[n].tmpx=ch[cn].tmpx;
	respawn[n].tmpy=ch[cn].tmpy;
	respawn[n].last_death=0;
	respawn[n].last_respawn=realtime;
}

void register_respawn_death(int cn)
{
	int n,flag=0;

	for (n=0; n<cnt; n++) {
		if (respawn[n].tmp==ch[cn].tmp &&
		    respawn[n].tmpx==ch[cn].tmpx &&
		    respawn[n].tmpy==ch[cn].tmpy) {
			if (flag) {
				elog("respawn checker death: double entry for %s (%d %d %d)",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy);
			} else {
				flag=1;
				//xlog("%s (%d %d %d) died after %.2fm",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy,(realtime-respawn[n].last_respawn)/60.0);
				respawn[n].last_death=realtime;
				respawn[n].cn=0;
			}
		}
	}
	if (!flag) {
		elog("respawn checker death: no entry for %s (%d %d %d)",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy);
	}
}

void register_respawn_respawn(int cn)
{
	int n,flag=0;

	for (n=0; n<cnt; n++) {
		if (respawn[n].tmp==ch[cn].tmp &&
		    respawn[n].tmpx==ch[cn].tmpx &&
		    respawn[n].tmpy==ch[cn].tmpy) {
			if (flag) {
				elog("respawn checker respawn: double entry for %s (%d %d %d)",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy);
			} else {
				flag=1;
				//xlog("%s (%d %d %d) respawned after %.2fm",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy,(realtime-respawn[n].last_death)/60.0);
				respawn[n].last_respawn=realtime;
				respawn[n].cn=cn;
				respawn[n].serial=ch[cn].serial;
			}
		}
	}
	if (!flag) {
		elog("respawn checker respawn: no entry for %s (%d %d %d)",ch[cn].name,ch[cn].tmp,ch[cn].tmpx,ch[cn].tmpy);
	}
}

void respawn_check(void)
{
	int n,diff,cn,ok=1;

	for (n=0; n<cnt; n++) {
		if ((cn=respawn[n].cn) && ch[cn].serial!=respawn[n].serial) {
			elog("respawn checker: tmp=%d tmpx=%d tmpy=%d has wrong serial (char flags=%llu, x=%d,y=%d)",
			     respawn[n].tmp,respawn[n].tmpx,respawn[n].tmpy,
			     ch[cn].flags,
			     ch[cn].x,
			     ch[cn].y);
			ok=0;
		}
		if (respawn[n].last_death>respawn[n].last_respawn) diff=realtime-respawn[n].last_death;
		else diff=0;

                if (diff>60*10) {
			elog("respawn checker: %d %d %d is not respawning",respawn[n].tmp,respawn[n].tmpx,respawn[n].tmpy);
			ok=0;
		}
	}
	xlog("respawn check done: %s",ok ? "OK" : "Errors");
}
