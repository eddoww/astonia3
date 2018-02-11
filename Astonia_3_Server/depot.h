/*

$Id: depot.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: depot.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:09  sam
Added RCS tags


*/

#define MAXDEPOT	80

struct depot_ppd
{
	struct item itm[MAXDEPOT];
};
int swap_depot(int cn,int nr);
void player_depot(int cn,int nr,int flag,int fast);
void depot_sort(int cn);
