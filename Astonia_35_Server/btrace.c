/*

$Id: btrace.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: btrace.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:51  sam
Added RCS tags


*/

#include <stdlib.h>
#include <execinfo.h>

#include "log.h"

void btrace(char *msg)
{
	int i,n;
	void *ba[128];
	char **names;

	if ((n=backtrace(ba,sizeof(ba)/sizeof(ba[0])))!=0) {
		names=backtrace_symbols(ba,n);
		for (i=n-3; i>0; i--) {
			elog("%s: %2d: %s",msg,i,names[i]);
		}
		free(names);
	}
}


