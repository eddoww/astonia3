/*

$Id: area1.h,v 1.2 2006/09/14 09:55:22 devel Exp $

$Log: area1.h,v $
Revision 1.2  2006/09/14 09:55:22  devel
added questlog

Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:44  sam
Added RCS tags


*/

#define AF1_STORAGE_HINT	(1u<<1)
#define AF1_BUY_HINT		(1u<<2)

struct area1_ppd
{
	int yoakin_state;
	int yoakin_seen_timer;

	int gwendy_state;
	int gwendy_seen_timer;

	int terion_state;

	int james_state;
	int flags;

	int darkin_state;	

	int gerewin_state;
	int gerewin_seen_timer;

	int nook_state;

	int lydia_state;
	int lydia_seen_timer;

	int asturin_state;
	int asturin_seen_timer;

	int guiwynn_state;
	int guiwynn_seen_timer;

	int logain_state;
	int logain_seen_timer;

	int reskin_state;
	int reskin_seen_timer;
	int reskin_got_bits;

	int shrike_state;
	int shrike_fails;
};

