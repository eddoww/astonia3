/*

$Id: respawn.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: respawn.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:40  sam
Added RCS tags


*/

void register_respawn_char(int cn);
void respawn_check(void);
void register_respawn_respawn(int cn);
void register_respawn_death(int cn);

