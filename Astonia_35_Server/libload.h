/*

$Id: libload.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: libload.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:18  sam
Added RCS tags


*/

int init_lib(void);
void exit_lib(void);

#define CDT_DRIVER	0
#define CDT_ITEM	1
#define CDT_DEAD	2
#define CDT_RESPAWN	3
#define CDT_SPECIAL	4

int char_driver(int nr,int type,int cn,int ret,int last_action);
int item_driver(int nr,int in,int cn);
