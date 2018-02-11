/*

$Id: death.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: death.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:08  sam
Added RCS tags


*/

int kill_char(int cn,int co);
int hurt(int cn,int dam,int cc,int armordiv,int armorper,int shieldper);
int die_char(int cn,int co,int ispk);
int allow_body(int cn,char *name);
int kill_score(int cn,int cc);
int death_loss(int total_exp);
int delayed_hurt(int delay,int cn,int dam,int armorper,int shieldper);
int kill_score_level(int cnlev,int cclev);
int allow_body_db(int cnID,int coID);
int check_first_kill(int cn,int nr);
