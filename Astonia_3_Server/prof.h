/*

$Id: prof.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: prof.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:37  sam
Added RCS tags


*/

struct prof
{
	char *name;
	
	char base;	// start value
	char max;	// maximum value
	char step;	// raised in steps
};
extern struct prof prof[P_MAX];
int free_prof_points(int co);
int show_prof_info(int cn,int co,char *buf);
void cmd_steal(int cn);
