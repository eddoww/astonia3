/*

$Id: misc_ppd.h,v 1.2 2006/12/14 13:58:05 devel Exp $

$Log: misc_ppd.h,v $
Revision 1.2  2006/12/14 13:58:05  devel
added xmass special

Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:27  sam
Added RCS tags


*/

struct misc_ppd
{
	int lfreduct_usage_count;
	
        int complaint_date;

	int last_lq_death;

	int supermax_state;
	int supermax_gold;

	int swapped;

	unsigned char treedone[8];
};

