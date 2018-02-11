/*

$Id: poison.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: poison.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:36  sam
Added RCS tags


*/

void poison_someone(int cn,int pwr,int type);
void poison_callback(int cn,int in,int pos,int cserial,int iserial);
int remove_all_poison(int cn);
int remove_poison(int cn,int type);

