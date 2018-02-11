/*
 * $Id: prof.h,v 1.2 2007/07/04 09:24:23 devel Exp $
 *
 * $Log: prof.h,v $
 * Revision 1.2  2007/07/04 09:24:23  devel
 * profession cahnges
 *
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
int get_support_prof(int cn,int p);















