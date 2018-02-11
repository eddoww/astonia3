/*

$Id: tell.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: tell.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:58  sam
Added RCS tags


*/

void register_sent_tell(int cn,int coID);
void register_rec_tell(int cn,int coID);
void check_tells(int cn);
