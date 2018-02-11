/*

$Id: sector.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: sector.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:42  sam
Added RCS tags


*/

int init_sector(void);
void set_sector(int x,int y);
int skipx_sector(int x,int y);
void add_char_sector(int cn);
void del_char_sector(int cn);
int getfirst_char_sector(int x,int y);
void init_sound_sector(void);
int sector_hear(int xf,int yf,int xt,int yt);
int sector_hear_shout(int xf,int yf,int xt,int yt);
