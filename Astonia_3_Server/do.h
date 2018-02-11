/*

$Id: do.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: do.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:10  sam
Added RCS tags


*/

int do_idle(int cn,int dur);
int do_walk(int cn,int dir);
int do_take(int cn,int dir);
int do_drop(int cn,int dir);
int do_attack(int cn,int dir,int co);
int do_give(int cn,int dir);
int do_use(int cn,int dir,int spec);

int do_bless(int cn,int co);
int do_heal(int cn,int co);

int do_fireball(int cn,int x,int y);
int do_ball(int cn,int x,int y);

int do_magicshield(int cn);
int do_flash(int cn);
int do_warcry(int cn);
int do_freeze(int cn);

int swap(int cn,int pos);
int turn(int cn,int dir);
int container(int cn,int pos,int flag,int fast);
int use_item(int cn,int in);
int look_map(int cn,int x,int y);
int look_inv(int cn,int pos);
int char_swap(int cn);
int equip_item(int cn,int in,int pos);
int do_earthmud(int cn,int x,int y,int strength);
int do_earthrain(int cn,int x,int y,int strength);
int do_pulse(int cn);
