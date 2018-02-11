/*

$Id: player_driver.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: player_driver.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:35  sam
Added RCS tags


*/

void player_driver_stop(int nr,int nofight);
void player_driver_move(int nr,int x,int y);
void player_driver_take(int nr,int in);
void player_driver_drop(int nr,int x,int y);
void player_driver_use(int nr,int in);
void player_driver_kill(int nr,int co);
void player_driver_give(int nr,int co);
void player_driver_charspell(int nr,int spell,int co);
void player_driver_mapspell(int nr,int spell,int x,int y);
void player_driver_selfspell(int nr,int spell);
void player_driver_teleport(int nr,int tel);
void player_driver_halt(int nr);
int player_driver_get_move(int cn,int *px,int *py);
int player_driver_fake_move(int cn,int x,int y);
void player_driver_dig_off(int cn);
void player_driver_dig_on(int cn);
