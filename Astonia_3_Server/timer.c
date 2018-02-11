/*
timer.c (C) 2001 D.Brockhaus

Timers are the means to make things happen some time in the future.

The timers are kept in two lists: The free list (free_t) and the used
list (next_t). The latter one contains all used timers sorted by time.
This makes for very fast execution of tick_timer and wastes very little
time for long-running timers.
Inserts of new timers are fairly expensive, since they need to look at
used_timers/2 timers on average to find their position in the list.
Possible optimization: Hash table for inserts.

$Id: timer.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: timer.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:58  sam
Added RCS tags


*/

#include <stdlib.h>

#include "server.h"
#include "log.h"
#include "mem.h"
#include "timer.h"

static struct timer *free_t=NULL,*next_t=NULL;

#define TIMER_CNT	4096

int used_timers;

static inline void free_timer(struct timer *t)
{
	t->next=free_t;
	free_t=t;
	//xfree(t);

	used_timers--;
}

// make server call func at tick due. func gets dat1...dat5 as arguments.
// DLL functions MUST NOT use timers directly as function addresses
// might change over reloads!
// never add a new timer with ticker==due from a timer-called function!
int set_timer(int due,void (*func)(int,int,int,int,int),int dat1,int dat2,int dat3,int dat4,int dat5)
{
	struct timer *t,*last=NULL,*f;

	// get free timer
	f=free_t;
	if (!f) {
		int n;

		f=xcalloc(sizeof(struct timer)*TIMER_CNT,IM_TIMER);
		for (n=1; n<TIMER_CNT-1; n++) {
			f[n].next=f+n+1;
		}
		free_t=f+1;
	} else free_t=f->next;

        // find position in list
	for (t=next_t; t && t->due<due; t=t->next) last=t;

	// insert new timer into list
        if (last) {
		f->next=last->next;
		last->next=f;
	} else {
		f->next=next_t;
		next_t=f;
	}

	// set timer parameters
	f->due=due;
	f->func=func;
	f->dat1=dat1;
	f->dat2=dat2;
	f->dat3=dat3;
	f->dat4=dat4;
	f->dat5=dat5;

	used_timers++;

	return 1;
}

// left from debugging. funny thing is: this one doesnt create a "unused" warning, even though it is unused.
// it seems GNU-C doesnt notice this because it references itself. ts
static void display_queue(int step,int dum2,int dum3,int dum4,int dum5)
{
	struct timer *t;
	int n;

	for (t=next_t,n=0; t; t=t->next,n++)
		xlog("%3d: %p %d (%d)",n,t->func,t->due,ticker);

	set_timer(ticker+step,display_queue,step,0,0,0,0);
}

// called once per tick. fires due timers.
void tick_timer(void)
{
	struct timer *t,*last=NULL;

	for (t=next_t; t && t->due<=ticker; t=t->next) {
                last=t;
                t->func(t->dat1,t->dat2,t->dat3,t->dat4,t->dat5);
		used_timers--;
	}
	
	// add executed timers to free list
	if (last) {
		last->next=free_t;
		free_t=next_t;
	}

	// make first not-executed timer start of used timer list
	next_t=t;
}

// initialise timer lists
int init_timer(void)
{
	used_timers=0;

	//set_timer(ticker+TICKS*10,display_queue,TICKS*10,0,0,0,0);

	return 1;
}
