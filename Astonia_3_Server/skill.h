/*

$Id: skill.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: skill.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:47  sam
Added RCS tags


*/

struct skill
{
	char name[80];
	int base1,base2,base3;
        int cost;		// 0=not raisable, 1=skill, 2=attribute, 3=power
	int start;		// start value, pts up to this value are free
};

extern struct skill skill[];

int raise_cost(int v,int n,int seyan);
int raise_value(int cn,int v);
int skillmax(int cn);
int supermax_canraise(int skl);
int supermax_cost(int cn,int skl,int val);
int calc_exp(int cn);
int lower_value(int cn,int v);
int raise_value_exp(int cn,int v);
