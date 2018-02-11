/*

$Id: timer.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: timer.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:59  sam
Added RCS tags


*/

struct timer
{
	int due;

	void (*func)(int,int,int,int,int);
	int dat1,dat2,dat3,dat4,dat5;

	struct timer *next;
};

extern int used_timers;

int init_timer(void);
void tick_timer(void);
int set_timer(int due,void (*func)(int,int,int,int,int),int dat1,int dat2,int dat3,int dat4,int dat5);

