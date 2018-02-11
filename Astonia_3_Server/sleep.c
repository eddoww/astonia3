/*

$Id: sleep.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: sleep.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:47  sam
Added RCS tags


*/

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include "server.h"
#include "log.h"
#include "sleep.h"

int cidle_avg=0;

long long timel(void)
{
        struct timeval tv;

        gettimeofday(&tv,NULL);

        return (long long)tv.tv_sec*(long long)1000000+(long long)tv.tv_usec;
}

void tick_sleep(int show)
{
	struct timeval tv;
	long long now,tosleep,diff;
	static long long next=0;
	static int nextshow=0,cidle=0;

	//return;	// !!!!!!!!!!!!

	if (!next) next=timel()+TICK;

	now=timel();
	tosleep=next-now;

        // sleep while idle
	if (tosleep>0) {
		tv.tv_usec=tosleep%1000000;
		tv.tv_sec=tosleep/1000000;
		select(0,NULL,NULL,NULL,&tv);
		
		// how long did we REALLY sleep? (tends to be too long)
		now=timel();
		diff=next-now;		
	} else { tosleep=0; diff=0; }

        // update statistics
	cidle=100.0/TICK*(tosleep-diff);
	cidle_avg=(cidle_avg*0.0099+cidle*0.01)*100;

        // calculate time for next tick
	next+=TICK;

	if (show) {
		if (nextshow) nextshow--;
		else {
			xlog("idle=%6.2f%%.",cidle_avg/100.0);
			nextshow=TICKS*3;
		}
	}
}
