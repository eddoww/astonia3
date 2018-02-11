/*

$Id: motd.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: motd.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:28  sam
Added RCS tags


*/

int read_motd(void);
void show_motd(int nr);
#ifdef STAFF
int check_staff_start(void);
void check_staff_stop(void);
#endif

