/*

$Id: notify.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: notify.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:29  sam
Added RCS tags


*/

#include <stdlib.h>

#include "server.h"
#include "log.h"
#include "see.h"
#include "mem.h"
#include "sector.h"
#include "create.h"
#include "btrace.h"
#include "notify.h"

#define NOTIFY_SIZE	20

#define NOTIFY_CNT	4096

static struct msg *empty=NULL;

int used_msgs=0;

int init_notify(void)
{
        return 1;
}

struct msg *alloc_msg(void)
{
	struct msg *msg;
	
	if (!empty) {
		int n;

		msg=xcalloc(sizeof(struct msg)*NOTIFY_CNT,IM_NOTIFY);
		for (n=1; n<NOTIFY_CNT-1; n++) {
			msg[n].next=msg+n+1;
		}
		empty=msg+1;
	} else {
		msg=empty;
		empty=msg->next;
	}
	if (msg->type) {
		elog("got non-free notify message in alloc_msg (%d)",msg->type);
		msg=xcalloc(sizeof(struct msg),IM_NOTIFY);
	}

        used_msgs++;

	return msg;
}

void free_msg(struct msg *msg)
{
	if (!msg->type) {
		elog("trying to free already free notify msg. message=%p (%d %d %d %d)",msg,msg->type,msg->dat1,msg->dat2,msg->dat3);
		btrace("free_msg");
		return;
	}

	if (msg->type==NT_TEXT) xfree((void*)msg->dat2);
	msg->type=0;

	msg->next=empty;
	empty=msg;

        used_msgs--;
}

void add_msg(int cn,struct msg *msg)
{
	if (cn<1 || cn>=MAXCHARS) {
		elog("illegal add_msg: cn=%d, type=%d, dat=%d,%d,%d",cn,msg->type,msg->dat1,msg->dat2,msg->dat3);
		btrace("add_msg");
		return;
	}
	if (!ch[cn].flags) {
		elog("illegal add_msg: ch[%d].flags=0, type=%d, dat=%d,%d,%d",cn,msg->type,msg->dat1,msg->dat2,msg->dat3);
		btrace("add_msg");
		return;
	}
        msg->next=NULL;
	msg->prev=ch[cn].msg_last;

	if (ch[cn].msg_last) ch[cn].msg_last->next=msg;
        if (!ch[cn].msg) ch[cn].msg=msg;

	ch[cn].msg_last=msg;
}

void del_msg(int cn,struct msg *msg)
{
	struct msg *prev,*next;

	if (!msg->type) {
		elog("trying to free already free notify msg. char=%s (%d), message=%p (%d %d %d %d)",ch[cn].name,cn,msg,msg->type,msg->dat1,msg->dat2,msg->dat3);
		btrace("del_msg");
		return;
	}

	prev=msg->prev;
	next=msg->next;

	if (!prev) ch[cn].msg=next;
	else prev->next=next;

	if (!next) ch[cn].msg_last=prev;
        else next->prev=prev;	
}

void notify_char(int cn,int type,int dat1,int dat2,int dat3)
{
	struct msg *msg;

	msg=alloc_msg();

	msg->type=type;
	msg->dat1=dat1;
	msg->dat2=dat2;
	msg->dat3=dat3;

	add_msg(cn,msg);
}

void notify_area(int xc,int yc,int type,int dat1,int dat2,int dat3)
{
	int x,y,xs,xe,ys,ye,cn;
	unsigned long long prof;

	prof=prof_start(18);

	xs=max(0,xc-NOTIFY_SIZE);
	xe=min(MAXMAP-1,xc+NOTIFY_SIZE);
	ys=max(0,yc-NOTIFY_SIZE);
	ye=min(MAXMAP-1,yc+NOTIFY_SIZE);

	for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (cn=getfirst_char_sector(x,y); cn; cn=ch[cn].sec_next) {
                                if (ch[cn].x>=xs && ch[cn].x<=xe && ch[cn].y>=ys && ch[cn].y<=ye) {
					notify_char(cn,type,dat1,dat2,dat3);
				}
			}
		}
	}

	prof_stop(18,prof);
}

void notify_area_shout(int xc,int yc,int type,int dat1,int dat2,int dat3)
{
	int x,y,xs,xe,ys,ye,cn;
	unsigned long long prof;

	prof=prof_start(18);

	xs=max(0,xc-NOTIFY_SIZE);
	xe=min(MAXMAP-1,xc+NOTIFY_SIZE);
	ys=max(0,yc-NOTIFY_SIZE);
	ye=min(MAXMAP-1,yc+NOTIFY_SIZE);

	for (y=ys; y<=ye; y+=8) {
		for (x=xs; x<=xe; x+=8) {
                        for (cn=getfirst_char_sector(x,y); cn; cn=ch[cn].sec_next) {
                                if (ch[cn].x>=xs && ch[cn].x<=xe && ch[cn].y>=ys && ch[cn].y<=ye) {
					if (!sector_hear_shout(xc,yc,ch[cn].x,ch[cn].y)) continue;
					
					notify_char(cn,type,dat1,dat2,dat3);					
				}
			}
		}
	}

	prof_stop(18,prof);
}

void purge_messages(int cn)
{
	struct msg *msg,*next;

	for (msg=ch[cn].msg; msg; msg=next) {
                next=msg->next;
                free_msg(msg);
	}
	ch[cn].msg=NULL;
	ch[cn].msg_last=NULL;
}

void remove_message(int cn,struct msg *msg)
{
	del_msg(cn,msg);
	free_msg(msg);
}

void notify_all(int type,int dat1,int dat2,int dat3)
{
	int cn;

	for (cn=getfirst_char(); cn; cn=getnext_char(cn))
		notify_char(cn,type,dat1,dat2,dat3);

}
