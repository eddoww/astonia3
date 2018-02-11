/*

$Id: act.h,v 1.2 2007/10/24 11:39:27 devel Exp $

$Log: act.h,v $
Revision 1.2  2007/10/24 11:39:27  devel
*** empty log message ***

Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:42  sam
Added RCS tags


*/

#define AC_IDLE		0
#define AC_WALK		1
#define AC_TAKE		2
#define AC_DROP		3

#define AC_ATTACK1	4
#define AC_ATTACK2	5
#define AC_ATTACK3	6

#define AC_USE		7

#define AC_FIREBALL1	10
#define AC_FIREBALL2	11

#define AC_BALL1	12
#define AC_BALL2	13

#define AC_MAGICSHIELD	14
#define AC_FLASH	15	

#define AC_BLESS_SELF	16
#define AC_BLESS1	17
#define AC_BLESS2	18

#define AC_HEAL_SELF	19
#define AC_HEAL1	20
#define AC_HEAL2	21

#define AC_FREEZE	22
#define AC_WARCRY	23
#define AC_GIVE		24

#define AC_EARTHRAIN	25
#define AC_EARTHMUD	26

#define AC_PULSE	27

#define AC_FIRERING	28

#define AC_DIE		50

void tick_char(void);
int bless_someone(int co,int strength,int duration);
void check_container_item(int cn);
void check_merchant(int cn);
